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
        return self.samples[idx]


def _collate_samples(samples: list[Sample]):
    batch_size = len(samples)
    states = torch.stack([
        torch.from_numpy(sample.state.astype(np.float32, copy=False))
        for sample in samples
    ])
    max_actions = max(1, max(sample.action_features.shape[0] for sample in samples))
    action_dim = next(
        (
            sample.action_features.shape[1]
            for sample in samples
            if sample.action_features.ndim == 2 and sample.action_features.shape[1] > 0
        ),
        0,
    )

    action_feats = torch.zeros(batch_size, max_actions, action_dim, dtype=torch.float32)
    mask = torch.zeros(batch_size, max_actions, dtype=torch.bool)
    target_pi = torch.zeros(batch_size, max_actions, dtype=torch.float32)
    target_z = torch.tensor([sample.value for sample in samples], dtype=torch.float32)

    for row, sample in enumerate(samples):
        if sample.action_features.ndim != 2 or sample.action_features.shape[0] == 0:
            continue
        if sample.action_features.shape[1] != action_dim:
            raise ValueError(
                f"inconsistent action feature width: {sample.action_features.shape[1]} != {action_dim}"
            )
        count = min(sample.action_features.shape[0], max_actions)
        action_feats[row, :count] = torch.from_numpy(
            sample.action_features[:count].astype(np.float32, copy=False)
        )
        mask[row, :count] = True
        policy = sample.policy[:count].astype(np.float32, copy=False)
        total = float(policy.sum())
        if total > 0.0:
            policy = policy / total
        target_pi[row, :count] = torch.from_numpy(policy)

    return states, action_feats, mask, target_pi, target_z


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
    samples = [
        sample for sample in samples
        if sample.action_features.ndim == 2 and sample.action_features.shape[0] > 0
    ]
    if not samples:
        return metrics

    dataset = _ReplayDataset(samples)
    loader = DataLoader(
        dataset,
        batch_size=config.trainer.batch_size,
        shuffle=True,
        num_workers=0,
        drop_last=False,
        collate_fn=_collate_samples,
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

        # Entropy bonus: encourages the policy to keep some spread over
        # the legal-action set instead of collapsing onto MCTS's argmax
        # too early. Mean entropy over masked-in actions only.
        entropy_weight = float(getattr(config.trainer, "entropy_bonus", 0.0) or 0.0)
        if entropy_weight > 0.0:
            # Mask logits so invalid slots don't poison softmax.
            masked_logits = logits.masked_fill(~mask.bool(), -1e9)
            log_probs = torch.nn.functional.log_softmax(masked_logits, dim=-1)
            probs = log_probs.exp()
            # Per-row entropy in nats, summing only legal slots.
            ent_per_row = -(probs * log_probs * mask.float()).sum(dim=-1)
            entropy_term = ent_per_row.mean()
            loss = loss - entropy_weight * entropy_term

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
