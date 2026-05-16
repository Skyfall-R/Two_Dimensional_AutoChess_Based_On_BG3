"""Rolling replay buffer storing (state, policy_target, value_target).

Samples keep their natural action count instead of padding every row to a
global maximum. The trainer pads only inside each minibatch, which avoids
large replay buffers wasting memory on absent MoveDeployed slots. Action
features are stored as float16 and cast back to float32 during batching.

Checkpoint storage: ``save_npz`` / ``load_npz`` pack the buffer into a
compressed binary .npz file. With 200k samples this lands around 400-500MB
on disk and avoids the multi-GB intermediate string that JSON would build
in memory. JSON serialize/deserialize is preserved for tiny smoke tests
and tooling that already speaks JSON; the cloud orchestrator must use the
.npz path.
"""

from __future__ import annotations

import random
from collections import deque
from dataclasses import dataclass
from pathlib import Path

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

    # ------------------------------------------------------------------
    # Binary npz checkpoint - the only sane path at cloud scale.
    # ------------------------------------------------------------------

    def save_npz(self, path: Path) -> None:
        """Pack the buffer into a single compressed .npz file."""
        path = Path(path)
        path.parent.mkdir(parents=True, exist_ok=True)
        # numpy.savez_compressed auto-appends .npz if the path lacks it.
        # We always pass an explicit .npz path so the on-disk filename is
        # exactly what the caller provided.
        if path.suffix.lower() != ".npz":
            path = path.with_suffix(path.suffix + ".npz")
        if not self.samples:
            np.savez_compressed(
                str(path),
                states=np.zeros((0, 0), dtype=np.float32),
                values=np.zeros((0,), dtype=np.float32),
                action_offsets=np.zeros((1,), dtype=np.int64),
                action_features=np.zeros((0, 0), dtype=np.float16),
                policies=np.zeros((0,), dtype=np.float32),
                capacity=np.int64(self.capacity),
            )
            return

        n = len(self.samples)
        state_dim = int(self.samples[0].state.shape[0])
        action_dim = 0
        for s in self.samples:
            if s.action_features.ndim == 2 and s.action_features.shape[1] > 0:
                action_dim = int(s.action_features.shape[1])
                break

        states = np.empty((n, state_dim), dtype=np.float32)
        values = np.empty((n,), dtype=np.float32)
        action_counts = np.empty((n,), dtype=np.int64)
        for i, s in enumerate(self.samples):
            states[i] = s.state
            values[i] = s.value
            action_counts[i] = (
                int(s.action_features.shape[0])
                if s.action_features.ndim >= 1
                else 0
            )
        total_actions = int(action_counts.sum())
        action_offsets = np.zeros((n + 1,), dtype=np.int64)
        np.cumsum(action_counts, out=action_offsets[1:])

        eff_action_dim = max(1, action_dim)
        action_features = np.zeros((total_actions, eff_action_dim), dtype=np.float16)
        policies = np.zeros((total_actions,), dtype=np.float32)
        for i, s in enumerate(self.samples):
            count = int(action_counts[i])
            if count == 0 or s.action_features.ndim != 2:
                continue
            start = int(action_offsets[i])
            width = min(int(s.action_features.shape[1]), eff_action_dim)
            action_features[start : start + count, :width] = s.action_features[:count, :width]
            policies[start : start + count] = s.policy[:count]

        np.savez_compressed(
            str(path),
            states=states,
            values=values,
            action_offsets=action_offsets,
            action_features=action_features,
            policies=policies,
            capacity=np.int64(self.capacity),
        )

    @classmethod
    def load_npz(cls, path: Path) -> "ReplayBuffer":
        path = Path(path)
        if path.suffix.lower() != ".npz" and path.with_suffix(path.suffix + ".npz").exists():
            path = path.with_suffix(path.suffix + ".npz")
        data = np.load(str(path))
        capacity = int(data["capacity"]) if "capacity" in data.files else 100_000
        buf = cls(capacity=capacity)
        states = data["states"]
        values = data["values"]
        action_offsets = data["action_offsets"]
        action_features = data["action_features"]
        policies = data["policies"]
        n = int(states.shape[0])
        for i in range(n):
            start = int(action_offsets[i])
            end = int(action_offsets[i + 1])
            af = action_features[start:end].astype(np.float16, copy=False)
            pol = policies[start:end].astype(np.float32, copy=False)
            buf.samples.append(
                Sample(
                    state=states[i].astype(np.float32, copy=False),
                    action_features=af,
                    action_mask=np.ones(end - start, dtype=bool),
                    policy=_normalized_policy(pol),
                    value=float(values[i]),
                )
            )
        return buf

    # ------------------------------------------------------------------
    # Legacy JSON (kept for smoke tests and migration; do NOT use at cloud
    # scale - serializes the whole buffer as a single Python string).
    # ------------------------------------------------------------------

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
            mask = (
                np.asarray(masks[i], dtype=bool)
                if i < len(masks)
                else np.ones(action_features.shape[0], dtype=bool)
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
