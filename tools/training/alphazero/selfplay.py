"""Self-play game generator.

A self-play "iteration" produces a batch of (s, pi, z) tuples by running MCTS
at every prep state of each game. After the player presses Ready and combat
resolves, the round outcome (±1) backfills as the value target z for every
recorded state in that round.

For now we run sequentially in the parent process (works on any platform and
plays well with checkpoint/resume). When AUTOCHESS_PARALLEL_SELFPLAY=1 the
generator can be wrapped in a multiprocessing pool by the orchestrator. The
orchestrator owns process spawning so this module stays simple.
"""

from __future__ import annotations

import time
from dataclasses import dataclass

import numpy as np
import torch

from .config import TrainingConfig
from .encoder import encode
from .mcts import MCTS
from .replay import Sample, make_sample


@dataclass
class GameStats:
    rounds_played: int
    moves_played: int
    final_value: float
    duration_sec: float
    max_legal_actions: int = 0


def play_game(
    env_module,
    network,
    device: torch.device,
    config: TrainingConfig,
    seed: int,
    rng: np.random.Generator,
    opponent_difficulty: str = "Hard",
) -> tuple[list[Sample], GameStats]:
    """Play a single game and return generated training samples.

    A "game" is a fixed number of rounds. We record (state, MCTS policy,
    final_z) for each prep decision. final_z is the round outcome ±1 from
    the player's view, propagated back to all states in that round.
    """
    handle = env_module.env_create(
        seed=seed,
        max_steps=config.selfplay.max_actions_per_round * config.selfplay.rounds_per_game,
        difficulty=opponent_difficulty,
        policy_dir="assets/ai",
    )
    mcts = MCTS(
        env_module=env_module,
        network=network,
        device=device,
        c_puct=config.mcts.c_puct,
        dirichlet_alpha=config.mcts.dirichlet_alpha,
        dirichlet_epsilon=config.mcts.dirichlet_epsilon,
        rng=rng,
    )

    samples: list[Sample] = []
    pending_rows: list[tuple[np.ndarray, np.ndarray, int, np.ndarray]] = []
    rounds_played = 0
    moves_played = 0
    started = time.time()
    final_value = 0.0
    max_legal_actions = 0

    try:
        for round_idx in range(config.selfplay.rounds_per_game):
            round_pending: list[tuple[np.ndarray, np.ndarray, int, np.ndarray]] = []
            for move_idx in range(config.selfplay.max_actions_per_round):
                obs = env_module.env_observation(handle)
                if obs.get("done"):
                    break
                if obs.get("phase") != "Preparation":
                    # Combat is auto-running; let it finish.
                    env_module.env_finalize_round(handle)
                    break

                state_features = env_module.env_state_features(handle)
                legal_actions = env_module.env_legal_actions(handle)
                if not legal_actions:
                    break
                max_legal_actions = max(max_legal_actions, len(legal_actions))

                encoded = encode(state_features, legal_actions)
                visit_dist, _root_value, legal_indices = mcts.run(
                    handle, config.mcts.simulations, add_noise=True
                )
                if visit_dist.size == 0:
                    break

                # Record sample (z filled later from round outcome).
                round_pending.append(
                    (encoded.state, encoded.action_features, encoded.legal_count, visit_dist.copy())
                )

                # Sample action: temperature for first N moves of the game,
                # then argmax (greedy).
                if move_idx < config.mcts.temperature_moves:
                    probs = np.power(visit_dist + 1e-8, 1.0 / max(0.01, config.mcts.temperature))
                    probs /= probs.sum()
                    action_slot = int(rng.choice(len(probs), p=probs))
                else:
                    action_slot = int(np.argmax(visit_dist))

                engine_idx = legal_indices[action_slot]
                result = env_module.env_step(handle, engine_idx)
                moves_played += 1
                if result.get("done"):
                    break
                # If we just played Ready, combat already ran inside env_step.
                if env_module.env_observation(handle).get("phase") != "Preparation":
                    env_module.env_finalize_round(handle)
                    break

            # End-of-round bookkeeping: exploration score, economy, and board
            # presence provide a dense value target for the dungeon-run rules.
            obs = env_module.env_observation(handle)
            if obs.get("done"):
                winner = obs.get("winner", "")
                z = 1.0 if winner == "Player1" else (-1.0 if winner == "Player2" else 0.0)
                final_value = z
                for state, action_feats, legal_count, pi in round_pending + pending_rows:
                    samples.append(_make_sample(state, action_feats, legal_count, pi, z))
                pending_rows.clear()
                rounds_played += 1
                break
            else:
                proxy = _soft_value(obs)
                for state, action_feats, legal_count, pi in round_pending:
                    pending_rows.append((state, action_feats, legal_count, pi))
                # Apply proxy z to anything older than 1 round.
                if len(pending_rows) > 0:
                    for state, action_feats, legal_count, pi in pending_rows:
                        samples.append(
                            _make_sample(state, action_feats, legal_count, pi, float(proxy))
                        )
                    pending_rows.clear()
                rounds_played += 1
    finally:
        env_module.env_close(handle)

    duration = time.time() - started
    return samples, GameStats(
        rounds_played=rounds_played,
        moves_played=moves_played,
        final_value=final_value,
        duration_sec=duration,
        max_legal_actions=max_legal_actions,
    )


def _make_sample(
    state: np.ndarray,
    action_features: np.ndarray,
    legal_count: int,
    visits: np.ndarray,
    z: float,
) -> Sample:
    return make_sample(state, action_features, legal_count, visits, z)


def _soft_value(obs: dict) -> float:
    """Dense non-terminal value for the exploration-run scoring model."""
    score_delta = float(obs.get("playerExplorationScore", 0) - obs.get("enemyExplorationScore", 0))
    money_delta = float(obs.get("playerMoney", 0) - obs.get("enemyMoney", 0))
    boss_delta = float(obs.get("playerBossesCleared", 0) - obs.get("enemyBossesCleared", 0))
    deploy_delta = float(obs.get("playerDeployed", 0) - obs.get("enemyDeployed", 0))
    bench_delta = float(obs.get("playerBench", 0) - obs.get("enemyBench", 0))
    progress = float(obs.get("explorationRound", 0)) / max(1.0, float(obs.get("explorationRoundLimit", 1)))
    raw = (
        score_delta / 120.0
        + money_delta / 160.0
        + boss_delta * 0.25
        + deploy_delta * 0.04
        + bench_delta * 0.02
    )
    return float(np.tanh(raw * (0.65 + 0.35 * progress)))
