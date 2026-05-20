"""PUCT MCTS for AutoChess preparation phase.

State: position in the prep tree (a Python EnvState wrapping a C++ env handle).
Edge expansion: try every legal action, get NN policy prior + value estimate.
Selection: PUCT a* = argmax(Q(a) + c_puct * P(a) * sqrt(sum N(b)) / (1 + N(a))).
Rollouts: when a Ready action is chosen the C++ env runs combat to terminal
and returns ±1; otherwise we use the NN value head as the bootstrap estimate.

Returns the visit-count distribution over the *root* legal actions, which is
used both as the actor's move and as the policy target for training.
"""

from __future__ import annotations

import math
from dataclasses import dataclass

import numpy as np
import torch

from .encoder import encode, EncodedState


@dataclass
class _Node:
    prior: float
    value_sum: float = 0.0
    visit_count: int = 0
    children: dict[int, "_Node"] = None  # legal_index -> child Node

    def expanded(self) -> bool:
        return self.children is not None

    def q(self) -> float:
        return self.value_sum / self.visit_count if self.visit_count > 0 else 0.0


class MCTS:
    def __init__(
        self,
        env_module,
        network,
        device: torch.device,
        c_puct: float,
        dirichlet_alpha: float,
        dirichlet_epsilon: float,
        rng: np.random.Generator,
    ) -> None:
        self.env_module = env_module
        self.network = network
        self.device = device
        self.c_puct = c_puct
        self.dirichlet_alpha = dirichlet_alpha
        self.dirichlet_epsilon = dirichlet_epsilon
        self.rng = rng

    # --- inference helpers --------------------------------------------------

    def _evaluate(self, encoded: EncodedState) -> tuple[np.ndarray, float]:
        if encoded.legal_count == 0:
            return np.zeros(0, dtype=np.float32), 0.0
        state = torch.from_numpy(encoded.state).to(self.device).unsqueeze(0)
        action_features = (
            torch.from_numpy(encoded.action_features).to(self.device).unsqueeze(0)
        )
        # Build mask for the actually-present slots only (not full pad width).
        mask = torch.zeros(1, encoded.legal_count, dtype=torch.bool, device=self.device)
        mask[0, :] = True
        probs, value = self.network.predict(state, action_features, mask)
        priors = probs[0, : encoded.legal_count].detach().cpu().numpy()
        # Numerical guard: priors might be very small; renormalize.
        priors = np.clip(priors, 1e-8, None)
        priors = priors / priors.sum()
        return priors.astype(np.float32), float(value.item())

    # --- env helpers --------------------------------------------------------

    def _encode_handle(self, handle: int) -> EncodedState:
        state_features = self.env_module.env_state_features(handle)
        legal = self.env_module.env_legal_actions(handle)
        return encode(state_features, legal)

    def _is_terminal(self, handle: int) -> bool:
        obs = self.env_module.env_observation(handle)
        return bool(obs.get("done"))

    def _terminal_value(self, handle: int) -> float:
        obs = self.env_module.env_observation(handle)
        winner = obs.get("winner") or ""
        if winner == "Player1":
            return 1.0
        if winner == "Player2":
            return -1.0
        # Draw / round-completed mid-game: small negative pressure to push
        # the agent to actually finish opponents instead of stalling.
        return -0.05

    # --- main loop ----------------------------------------------------------

    def run(self, root_handle: int, simulations: int, add_noise: bool = True) -> tuple[np.ndarray, float, list[int]]:
        """Run MCTS at the root_handle and return (visit_distribution, root_value, legal_indices).

        root_handle is NOT modified: it is cloned as needed.
        """
        encoded = self._encode_handle(root_handle)
        if encoded.legal_count == 0:
            return np.zeros(0, dtype=np.float32), 0.0, []

        priors, root_value = self._evaluate(encoded)
        if add_noise and self.dirichlet_epsilon > 0 and len(priors) > 1:
            noise = self.rng.dirichlet([self.dirichlet_alpha] * len(priors))
            priors = (1 - self.dirichlet_epsilon) * priors + self.dirichlet_epsilon * noise

        root = _Node(prior=1.0)
        root.children = {
            i: _Node(prior=float(priors[i])) for i in range(encoded.legal_count)
        }

        for _ in range(simulations):
            self._simulate(root_handle, root, encoded)

        visit = np.array(
            [root.children[i].visit_count for i in range(encoded.legal_count)],
            dtype=np.float32,
        )
        if visit.sum() > 0:
            visit /= visit.sum()
        return visit, root_value, encoded.legal_indices

    # --- single simulation --------------------------------------------------

    def _simulate(self, root_handle: int, root: _Node, root_encoded: EncodedState) -> None:
        # Clone the env so we can mutate it freely along this rollout.
        sim_handle = self.env_module.env_clone(root_handle)
        try:
            value = self._descend(sim_handle, root, root_encoded, depth=0)
        finally:
            self.env_module.env_close(sim_handle)
        # Backprop already happened during _descend's recursion.
        _ = value

    # Depth cap for a single MCTS simulation. Each "depth step" corresponds to
    # one env_step in the cloned env, and Ready actions trigger a full combat
    # simulation that can take seconds. Without this cap a simulation could
    # roll out 8 rounds * many prep actions, with multiple combats per sim;
    # 256 sims * that depth is tens of thousands of expensive env_steps per
    # root decision and the trainer appears to hang. Bootstrapping with the
    # value head at this depth is the standard AlphaZero recipe anyway.
    MAX_SIM_DEPTH = 8

    def _descend(self, handle: int, node: _Node, encoded: EncodedState, depth: int) -> float:
        """PUCT descent + recursive backup. Returns the leaf value seen by `node`."""
        if encoded.legal_count == 0:
            # Terminal-like with no legal actions: read result directly.
            value = self._terminal_value(handle) if self._is_terminal(handle) else 0.0
            self._backup(node, value)
            return value

        if depth >= self.MAX_SIM_DEPTH:
            # Depth cap: do not descend further, just bootstrap with NN value.
            _, value = self._evaluate(encoded)
            self._backup(node, value)
            return value

        if not node.expanded():
            # Expand: get prior + bootstrap value from the network.
            priors, value = self._evaluate(encoded)
            node.children = {
                i: _Node(prior=float(priors[i])) for i in range(encoded.legal_count)
            }
            self._backup(node, value)
            return value

        # PUCT selection.
        total_visits = sum(child.visit_count for child in node.children.values())
        sqrt_total = math.sqrt(total_visits + 1)
        best_idx = -1
        best_score = -float("inf")
        for idx, child in node.children.items():
            ucb = (
                child.q()
                + self.c_puct * child.prior * sqrt_total / (1 + child.visit_count)
            )
            if ucb > best_score:
                best_score = ucb
                best_idx = idx

        chosen_idx = best_idx
        engine_idx = encoded.legal_indices[chosen_idx]

        # Ready short-circuit. If the chosen action is Ready, stepping it
        # would trigger a full combat tick loop inside env_step, which on a
        # 33x19 board is the dominant cost of MCTS. AlphaZero-style: do NOT
        # step; bootstrap the value with the NN's prediction at the current
        # state, then back-prop. Combat still runs ONCE in the outer
        # selfplay loop when the agent really commits to Ready, so the value
        # head learns from real outcomes.
        chosen_kind = (
            encoded.action_kinds[chosen_idx]
            if chosen_idx < len(encoded.action_kinds)
            else ""
        )
        if chosen_kind == "Ready":
            _, value = self._evaluate(encoded)
            value = max(-1.0, min(1.0, value))
            self._backup(node.children[chosen_idx], value)
            self._backup(node, value)
            return value

        # Defensive round-advance detection in case env_step still triggers
        # combat (e.g. stale state from a non-Ready action - shouldn't
        # happen, but the bootstrap is cheap insurance).
        round_before = int(self.env_module.env_observation(handle).get("explorationRound", 0))

        result = self.env_module.env_step(handle, engine_idx)
        reward = float(result.get("reward", 0.0))
        done = bool(result.get("done", False))
        if done:
            value = max(-1.0, min(1.0, reward))
            self._backup(node.children[chosen_idx], value)
            self._backup(node, value)
            return value

        obs_after = self.env_module.env_observation(handle)
        round_after = int(obs_after.get("explorationRound", 0))
        next_encoded = encode(
            self.env_module.env_state_features(handle),
            self.env_module.env_legal_actions(handle),
        )

        if round_after > round_before:
            # A round just resolved (combat ran inside env_step). Bootstrap
            # the value of the new prep state via the network and stop the
            # simulation here. This caps each MCTS rollout to at most one
            # combat resolution, which is what makes the budget tractable.
            if next_encoded.legal_count == 0:
                future = max(-1.0, min(1.0, reward))
            else:
                _, future = self._evaluate(next_encoded)
            value = max(-1.0, min(1.0, reward + future))
            self._backup(node.children[chosen_idx], value)
            self._backup(node, value)
            return value

        future = self._descend(handle, node.children[chosen_idx], next_encoded, depth + 1)
        # Within a single round we don't discount (~12 prep steps total).
        # Clamp to keep numerical scale aligned with the value head.
        value = max(-1.0, min(1.0, reward + future))
        self._backup(node, value)
        return value

    @staticmethod
    def _backup(node: "_Node", value: float) -> None:
        node.visit_count += 1
        node.value_sum += value
