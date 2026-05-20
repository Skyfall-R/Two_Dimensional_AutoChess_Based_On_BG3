"""Evaluate a candidate network against fixed opponents.

Two evaluation modes:
  - vs scripted: opponent is the C++ ScriptedNormalAiPlanner (Normal difficulty).
                 This is a stable yardstick: the candidate must beat scripted
                 by some margin to be considered an improvement at all.
  - vs previous: opponent is the previously accepted candidate (loaded from
                 the opponent pool).

Returns win-rate from the candidate's perspective.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np
import torch

from .config import TrainingConfig
from .encoder import encode
from .network import PolicyValueNet


@dataclass
class ArenaResult:
    games: int
    wins: int
    losses: int
    draws: int

    @property
    def win_rate(self) -> float:
        return self.wins / max(1, self.games)


def _greedy_action(network: PolicyValueNet, device: torch.device, encoded) -> int:
    if encoded.legal_count == 0:
        return -1
    state = torch.from_numpy(encoded.state).to(device).unsqueeze(0)
    action_feats = torch.from_numpy(encoded.action_features).to(device).unsqueeze(0)
    mask = torch.zeros(1, encoded.legal_count, dtype=torch.bool, device=device)
    mask[0, :] = True
    with torch.no_grad():
        probs, _ = network.predict(state, action_feats, mask)
    slot = int(torch.argmax(probs[0, : encoded.legal_count]).item())
    return encoded.legal_indices[slot]


def evaluate_vs_difficulty(
    env_module,
    network: PolicyValueNet,
    device: torch.device,
    config: TrainingConfig,
    difficulty: str,
    seed_offset: int = 0,
) -> ArenaResult:
    wins = losses = draws = 0
    games = config.arena.games_per_eval
    for game_idx in range(games):
        # Seed varies per call so identical iterations don't repeat the
        # exact same 48 games (which can mask incremental policy gains).
        seed = 1000 + seed_offset + game_idx
        handle = env_module.env_create(
            seed=seed,
            max_steps=config.selfplay.max_actions_per_round * config.selfplay.rounds_per_game,
            difficulty=difficulty,
        )
        try:
            for _round in range(config.selfplay.rounds_per_game):
                for move_idx in range(config.selfplay.max_actions_per_round):
                    obs = env_module.env_observation(handle)
                    if obs.get("done") or obs.get("phase") != "Preparation":
                        break
                    legal = env_module.env_legal_actions(handle)
                    if not legal:
                        break

                    # Force Ready at the end of the round if it's legal.
                    # Mirrors the selfplay stall-guard so arena evaluates
                    # the policy under the same "must commit" pressure.
                    ready_engine_idx = None
                    for action in legal:
                        if action.get("kind") == "Ready":
                            ready_engine_idx = int(action.get("index", -1))
                            break
                    last_move = move_idx >= config.selfplay.max_actions_per_round - 1
                    if last_move and ready_engine_idx is not None:
                        env_module.env_step(handle, ready_engine_idx)
                        if env_module.env_observation(handle).get("done"):
                            break
                        continue

                    encoded = encode(env_module.env_state_features(handle), legal)
                    engine_idx = _greedy_action(network, device, encoded)
                    if engine_idx < 0:
                        break
                    env_module.env_step(handle, engine_idx)
                    if env_module.env_observation(handle).get("done"):
                        break
                env_module.env_finalize_round(handle)
                if env_module.env_observation(handle).get("done"):
                    break
            obs = env_module.env_observation(handle)
            winner = obs.get("winner", "")
            if winner == "Player1":
                wins += 1
            elif winner == "Player2":
                losses += 1
            else:
                draws += 1
        finally:
            env_module.env_close(handle)
    return ArenaResult(games=games, wins=wins, losses=losses, draws=draws)
