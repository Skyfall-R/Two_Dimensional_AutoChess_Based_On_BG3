"""Training loop for the policy + value network.

Each iteration:
  1. Generate self-play data via tools/training/alphazero/selfplay.py
  2. Add to replay buffer
  3. Sample minibatches and update network with cross-entropy + MSE
  4. Periodically evaluate and snapshot
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np
import torch
from torch.utils.data import DataLoader, Dataset

from .config import TrainingConfig
from .network import PolicyValueNet, policy_loss, value_loss
from .replay import ReplayBuffer, Sample


class _ReplayDataset(Dataset):
    def __init__(self, samples: list[Sample]) -> None:
        self.samples = samples

    def __len__(self) -> int:
        return len(self.samples)

    def __getitem__(self, idx: int):
        sample = self.samples[idx]
        return (
            torch.from_numpy(sample.state),
            torch.from_numpy(sample.action_features),
            torch.from_numpy(sample.action_mask),
            torch.from_numpy(sample.policy),
            torch.tensor(sample.value, dtype=torch.float32),
        )


@dataclass
class TrainingMetrics:
    iterations: int = 0
    samples_seen: int = 0
    avg_policy_loss: float = 0.0
    avg_value_loss: float = 0.0
    avg_total_loss: float = 0.0


def train_one_iteration(
    network: PolicyValueNet,
    optimizer: torch.optim.Optimizer,
    buffer: ReplayBuffer,
    config: TrainingConfig,
    device: torch.device,
) -> TrainingMetrics:
    network.train()
    metrics = TrainingMetrics()

    if len(buffer) < config.replay.min_size_to_train:
        return metrics

    samples = buffer.sample(config.trainer.batch_size * config.trainer.minibatches_per_iter)
    if not samples:
        return metrics

    dataset = _ReplayDataset(samples)
    loader = DataLoader(
        dataset,
        batch_size=config.trainer.batch_size,
        shuffle=True,
        num_workers=0,
        drop_last=False,
    )

    total_p, total_v, total_loss, batches = 0.0, 0.0, 0.0, 0
    for state, action_feats, mask, target_pi, target_z in loader:
        state = state.to(device)
        action_feats = action_feats.to(device)
        mask = mask.to(device)
        target_pi = target_pi.to(device)
        target_z = target_z.to(device)

        logits, value_pred = network(state, action_feats, mask)
        loss_pi = policy_loss(logits, target_pi, mask)
        loss_v = value_loss(value_pred, target_z)
        loss = loss_pi + config.trainer.value_loss_weight * loss_v

        optimizer.zero_grad()
        loss.backward()
        if config.trainer.max_grad_norm > 0:
            torch.nn.utils.clip_grad_norm_(network.parameters(), config.trainer.max_grad_norm)
        optimizer.step()

        total_p += float(loss_pi.detach())
        total_v += float(loss_v.detach())
        total_loss += float(loss.detach())
        batches += 1

    if batches:
        metrics.iterations = 1
        metrics.samples_seen = len(samples)
        metrics.avg_policy_loss = total_p / batches
        metrics.avg_value_loss = total_v / batches
        metrics.avg_total_loss = total_loss / batches
    return metrics
