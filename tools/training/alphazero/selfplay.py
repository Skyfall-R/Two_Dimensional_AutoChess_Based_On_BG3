"""Self-play game generator with stall-guard forced Ready.

The AutoChess engine has a degenerate equilibrium: if Player 1 never
issues Ready, Player 2 never preps, no combat ever runs, and every
prep state in the round inherits a near-zero proxy z. The agent learns
"Ready = risky (negative z if combat lost), stall = safe (z=0)" and
collapses onto MoveDeployed forever, which loses every arena game.

To break this, the stall-guard inside this module forces Ready with a
probability that ramps up across the second half of the round. The
forced-Ready training sample carries a one-hot policy target on the
Ready slot, so the policy directly learns "you should ready around
this point in the round". The same forced-Ready logic is mirrored in
arena.py so evaluation matches selfplay.
"""

from __future__ import annotations

import time
from dataclasses import dataclass

import numpy as np
import torch

from .config import TrainingConfig
from .encoder import encode, MAX_ACTIONS_PER_STATE
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
    opponent_difficulty: str = "Normal",
) -> tuple[list[Sample], GameStats]:
    """Play a single game and return generated training samples."""
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
                    env_module.env_finalize_round(handle)
                    break

                state_features = env_module.env_state_features(handle)
                legal_actions = env_module.env_legal_actions(handle)
                if not legal_actions:
                    break
                max_legal_actions = max(max_legal_actions, len(legal_actions))

                encoded = encode(state_features, legal_actions)

                # Find Ready slot if any.
                ready_slot = None
                ready_engine_idx = None
                for slot, action in enumerate(legal_actions):
                    if action.get("kind") == "Ready":
                        ready_engine_idx = int(action.get("index", slot))
                        ready_slot = slot
                        break

                # Stall guard: progressively ramp up Ready-forcing
                # probability through the second half of the round.
                forced_ready = False
                if ready_engine_idx is not None:
                    progress = move_idx / max(1, config.selfplay.max_actions_per_round - 1)
                    force_prob = max(0.0, (progress - 0.5) * 2.0) ** 2
                    if move_idx >= config.selfplay.max_actions_per_round - 1:
                        force_prob = 1.0
                    if rng.random() < force_prob:
                        forced_ready = True

                if forced_ready:
                    synthetic = np.zeros(encoded.legal_count, dtype=np.float32)
                    if ready_slot is not None and ready_slot < encoded.legal_count:
                        synthetic[ready_slot] = 1.0
                    round_pending.append(
                        (encoded.state, encoded.action_features, encoded.legal_count, synthetic)
                    )
                    result = env_module.env_step(handle, ready_engine_idx)
                    moves_played += 1
                    if result.get("done"):
                        break
                    if env_module.env_observation(handle).get("phase") != "Preparation":
                        env_module.env_finalize_round(handle)
                        break
                    continue

                visit_dist, _root_value, legal_indices = mcts.run(
                    handle, config.mcts.simulations, add_noise=True
                )
                if visit_dist.size == 0:
                    break

                round_pending.append(
                    (encoded.state, encoded.action_features, encoded.legal_count, visit_dist.copy())
                )

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
                if env_module.env_observation(handle).get("phase") != "Preparation":
                    env_module.env_finalize_round(handle)
                    break

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
    feat_dim = action_features.shape[1] if action_features.size else 0
    padded_actions = np.zeros((MAX_ACTIONS_PER_STATE, feat_dim), dtype=np.float32)
    padded_visits = np.zeros(MAX_ACTIONS_PER_STATE, dtype=np.float32)
    mask = np.zeros(MAX_ACTIONS_PER_STATE, dtype=bool)
    if legal_count > 0:
        padded_actions[:legal_count] = action_features[:legal_count]
        padded_visits[:legal_count] = visits[:legal_count]
        mask[:legal_count] = True
    return Sample(
        state=state,
        action_features=padded_actions,
        action_mask=mask,
        policy=padded_visits,
        value=float(np.clip(z, -1.0, 1.0)),
    )


def _soft_value(obs: dict) -> float:
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
