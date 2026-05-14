"""Rolling replay buffer storing (state, policy_target, value_target).

The buffer pads action dimension to MAX_ACTIONS_PER_STATE so minibatches are
rectangular tensors. We track the per-row legal_count via the mask alone.
"""

from __future__ import annotations

import random
from collections import deque
from dataclasses import dataclass

import numpy as np

from .encoder import MAX_ACTIONS_PER_STATE


@dataclass
class Sample:
    state: np.ndarray            # [F_s]
    action_features: np.ndarray  # [MAX_ACTIONS_PER_STATE, F_a]
    action_mask: np.ndarray      # [MAX_ACTIONS_PER_STATE] bool
    policy: np.ndarray           # [MAX_ACTIONS_PER_STATE]
    value: float                 # in [-1, 1]


class ReplayBuffer:
    def __init__(self, capacity: int) -> None:
        self.capacity = capacity
        self.samples: deque[Sample] = deque(maxlen=capacity)

    def __len__(self) -> int:
        return len(self.samples)

    def add(
        self,
        state: np.ndarray,
        action_features: np.ndarray,
        legal_count: int,
        policy_visits: np.ndarray,
        value: float,
        action_dim: int,
    ) -> None:
        padded_actions = np.zeros((MAX_ACTIONS_PER_STATE, action_dim), dtype=np.float32)
        padded_policy = np.zeros(MAX_ACTIONS_PER_STATE, dtype=np.float32)
        mask = np.zeros(MAX_ACTIONS_PER_STATE, dtype=bool)
        if legal_count > 0:
            padded_actions[:legal_count] = action_features[:legal_count]
            padded_policy[:legal_count] = policy_visits[:legal_count]
            mask[:legal_count] = True
        self.samples.append(
            Sample(
                state=state.astype(np.float32, copy=False),
                action_features=padded_actions,
                action_mask=mask,
                policy=padded_policy,
                value=float(value),
            )
        )

    def sample(self, batch_size: int) -> list[Sample]:
        if not self.samples:
            return []
        return random.sample(list(self.samples), min(batch_size, len(self.samples)))

    def extend(self, samples: list[Sample]) -> None:
        for s in samples:
            self.samples.append(s)

    def serialize(self) -> dict:
        # Compact dump for checkpointing the buffer alongside model weights.
        if not self.samples:
            return {"capacity": self.capacity, "states": [], "actions": [], "masks": [], "policies": [], "values": []}
        return {
            "capacity": self.capacity,
            "states": np.stack([s.state for s in self.samples]).tolist(),
            "actions": np.stack([s.action_features for s in self.samples]).tolist(),
            "masks": np.stack([s.action_mask for s in self.samples]).tolist(),
            "policies": np.stack([s.policy for s in self.samples]).tolist(),
            "values": [s.value for s in self.samples],
        }

    @classmethod
    def deserialize(cls, data: dict) -> "ReplayBuffer":
        buf = cls(capacity=int(data.get("capacity", 100_000)))
        states = np.asarray(data.get("states", []))
        actions = np.asarray(data.get("actions", []))
        masks = np.asarray(data.get("masks", []))
        policies = np.asarray(data.get("policies", []))
        values = list(data.get("values", []))
        for i in range(len(values)):
            buf.samples.append(
                Sample(
                    state=states[i].astype(np.float32),
                    action_features=actions[i].astype(np.float32),
                    action_mask=masks[i].astype(bool),
                    policy=policies[i].astype(np.float32),
                    value=float(values[i]),
                )
            )
        return buf
