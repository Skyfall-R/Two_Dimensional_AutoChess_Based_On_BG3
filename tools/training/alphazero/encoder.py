"""State / action tensor encoding.

The C++ env exposes:
    state_features  -> list[float] of length stateFeatureCount
    legal_actions   -> list[dict] each with `features` plus action metadata

We turn each (state, legal_actions) into:
    state_vec   : Tensor[F_s]
    action_vecs : Tensor[A, F_a]
    mask        : Tensor[A] (all ones for the present actions)

The policy network supports variable action counts. Replay storage keeps each
sample unpadded, and the trainer pads only within the current minibatch. The
MAX_ACTIONS_PER_STATE value is a defensive cap for pathological states, not
the normal replay tensor width.

EncodedState also carries action_kinds (parallel list to legal_indices) so
the MCTS / selfplay layers can detect Ready actions without re-fetching the
legal-actions list. This is essential for the Ready short-circuit and for
the stall-guard that forces Ready near the end of a round.
"""

from __future__ import annotations

from dataclasses import dataclass, field

import numpy as np


MAX_ACTIONS_PER_STATE = 4096


@dataclass
class EncodedState:
    state: np.ndarray              # shape [F_s]
    action_features: np.ndarray    # shape [A, F_a]
    action_mask: np.ndarray        # shape [A], bool
    legal_count: int               # number of legal actions present
    legal_indices: list[int]       # original engine indices for legal actions
    action_kinds: list[str] = field(default_factory=list)  # parallel; e.g. "Ready"


def _select_actions_with_cap(legal_actions: list[dict]) -> list[dict]:
    if len(legal_actions) <= MAX_ACTIONS_PER_STATE:
        return legal_actions

    selected = list(legal_actions[:MAX_ACTIONS_PER_STATE])
    if any(action.get("kind") == "Ready" for action in selected):
        return selected

    ready = next(
        (
            action for action in legal_actions[MAX_ACTIONS_PER_STATE:]
            if action.get("kind") == "Ready"
        ),
        None,
    )
    if ready is not None:
        selected[-1] = ready
    return selected


def encode(state_features: list[float], legal_actions: list[dict]) -> EncodedState:
    state = np.asarray(state_features, dtype=np.float32)
    if not legal_actions:
        return EncodedState(
            state=state,
            action_features=np.zeros((0, 0), dtype=np.float32),
            action_mask=np.zeros(0, dtype=bool),
            legal_count=0,
            legal_indices=[],
            action_kinds=[],
        )

    selected_actions = _select_actions_with_cap(legal_actions)
    feat_len = len(selected_actions[0]["features"])
    legal_count = len(selected_actions)
    action_features = np.zeros((legal_count, feat_len), dtype=np.float32)
    legal_indices: list[int] = []
    action_kinds: list[str] = []
    for slot, action in enumerate(selected_actions):
        action_features[slot] = action["features"]
        legal_indices.append(int(action["index"]))
        action_kinds.append(str(action.get("kind", "")))

    mask = np.ones(legal_count, dtype=bool)

    return EncodedState(
        state=state,
        action_features=action_features,
        action_mask=mask,
        legal_count=legal_count,
        legal_indices=legal_indices,
        action_kinds=action_kinds,
    )
