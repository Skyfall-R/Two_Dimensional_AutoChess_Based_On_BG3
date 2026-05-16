"""State / action tensor encoding.

The C++ env exposes:
    state_features  -> list[float] of length stateFeatureCount (478)
    legal_actions   -> list[dict] each with `features` (length 32) +
                       (kind, type, unitId, x, y, kindId, typeId)

We turn each (state, legal_actions) into:
    state_vec   : Tensor[F_s]
    action_vecs : Tensor[A, F_a]
    mask        : Tensor[A] (all ones; opaque to model, used to align padded batch)

For batching across games with different action counts, we pad to a fixed
MAX_ACTIONS_PER_STATE and supply an explicit mask. The default pad target
matches the worst-case enumeration in the engine (kMaxAiActionsPerPreparation
times a safety factor).
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np


MAX_ACTIONS_PER_STATE = 768  # Deploy(10×77) + Move(20×77) + others fits


@dataclass
class EncodedState:
    state: np.ndarray              # shape [F_s]
    action_features: np.ndarray    # shape [A, F_a]
    action_mask: np.ndarray        # shape [MAX_ACTIONS_PER_STATE], bool
    legal_count: int               # number of legal actions present
    legal_indices: list[int]       # original engine indices for legal actions


def encode(state_features: list[float], legal_actions: list[dict]) -> EncodedState:
    state = np.asarray(state_features, dtype=np.float32)
    if not legal_actions:
        return EncodedState(
            state=state,
            action_features=np.zeros((0, 0), dtype=np.float32),
            action_mask=np.zeros(MAX_ACTIONS_PER_STATE, dtype=bool),
            legal_count=0,
            legal_indices=[],
        )

    feat_len = len(legal_actions[0]["features"])
    legal_count = min(len(legal_actions), MAX_ACTIONS_PER_STATE)
    action_features = np.zeros((legal_count, feat_len), dtype=np.float32)
    legal_indices: list[int] = []
    for slot, action in enumerate(legal_actions[:legal_count]):
        action_features[slot] = action["features"]
        legal_indices.append(int(action["index"]))

    mask = np.zeros(MAX_ACTIONS_PER_STATE, dtype=bool)
    mask[:legal_count] = True

    return EncodedState(
        state=state,
        action_features=action_features,
        action_mask=mask,
        legal_count=legal_count,
        legal_indices=legal_indices,
    )
