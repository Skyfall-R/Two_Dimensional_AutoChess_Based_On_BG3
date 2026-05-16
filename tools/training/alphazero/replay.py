"""Rolling replay buffer storing (state, policy_target, value_target).

Samples keep their natural action count instead of padding every row to a
global maximum. The trainer pads only inside each minibatch, which avoids
large replay buffers wasting memory on absent MoveDeployed slots. Action
features are stored as float16 and cast back to float32 during batching.
"""

from __future__ import annotations

import random
from collections import deque
from dataclasses import dataclass

import numpy as np


@dataclass
class Sample:
    state: np.ndarray            # [F_s], float32
    action_features: np.ndarray  # [A, F_a], float16 in replay storage
    action_mask: np.ndarray      # [A] bool
    policy: np.ndarray           # [A], float32
    value: float                 # in [-1, 1]


def _normalized_policy(policy: np.ndarray) -> np.ndarray:
    policy = policy.astype(np.float32, copy=False)
    total = float(policy.sum())
    if total > 0.0:
        return policy / total
    return policy


def make_sample(
    state: np.ndarray,
    action_features: np.ndarray,
    legal_count: int,
    policy_visits: np.ndarray,
    value: float,
) -> Sample:
    count = max(
        0,
        min(
            int(legal_count),
            int(action_features.shape[0]) if action_features.ndim >= 1 else 0,
            int(policy_visits.shape[0]) if policy_visits.ndim >= 1 else 0,
        ),
    )
    features = action_features[:count].astype(np.float16, copy=False)
    policy = _normalized_policy(policy_visits[:count])
    return Sample(
        state=state.astype(np.float32, copy=False),
        action_features=features,
        action_mask=np.ones(count, dtype=bool),
        policy=policy,
        value=float(np.clip(value, -1.0, 1.0)),
    )


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
        _ = action_dim
        self.samples.append(
            make_sample(state, action_features, legal_count, policy_visits, value)
        )

    def sample(self, batch_size: int) -> list[Sample]:
        if not self.samples:
            return []
        return random.sample(list(self.samples), min(batch_size, len(self.samples)))

    def extend(self, samples: list[Sample]) -> None:
        for s in samples:
            self.samples.append(s)

    def approx_bytes(self) -> int:
        total = 0
        for sample in self.samples:
            total += sample.state.nbytes
            total += sample.action_features.nbytes
            total += sample.action_mask.nbytes
            total += sample.policy.nbytes
            total += 8
        return total

    def serialize(self) -> dict:
        if not self.samples:
            return {
                "capacity": self.capacity,
                "states": [],
                "actions": [],
                "masks": [],
                "policies": [],
                "values": [],
                "storage": "variable-actions-fp16",
            }
        return {
            "capacity": self.capacity,
            "states": [s.state.tolist() for s in self.samples],
            "actions": [s.action_features.tolist() for s in self.samples],
            "masks": [s.action_mask.tolist() for s in self.samples],
            "policies": [s.policy.tolist() for s in self.samples],
            "values": [s.value for s in self.samples],
            "storage": "variable-actions-fp16",
        }

    @classmethod
    def deserialize(cls, data: dict) -> "ReplayBuffer":
        buf = cls(capacity=int(data.get("capacity", 100_000)))
        states = data.get("states", [])
        actions = data.get("actions", [])
        masks = data.get("masks", [])
        policies = data.get("policies", [])
        values = list(data.get("values", []))

        for i in range(len(values)):
            state = np.asarray(states[i], dtype=np.float32)
            action_features = np.asarray(actions[i], dtype=np.float16)
            policy = np.asarray(policies[i], dtype=np.float32)
            mask = np.asarray(masks[i], dtype=bool) if i < len(masks) else np.ones(
                action_features.shape[0],
                dtype=bool,
            )

            if (
                action_features.ndim == 2
                and mask.ndim == 1
                and mask.shape[0] == action_features.shape[0]
                and policy.ndim == 1
                and policy.shape[0] == action_features.shape[0]
            ):
                action_features = action_features[mask]
                policy = policy[mask]

            count = min(action_features.shape[0], policy.shape[0])
            buf.samples.append(
                Sample(
                    state=state,
                    action_features=action_features[:count].astype(np.float16, copy=False),
                    action_mask=np.ones(count, dtype=bool),
                    policy=_normalized_policy(policy[:count]),
                    value=float(values[i]),
                )
            )
        return buf
