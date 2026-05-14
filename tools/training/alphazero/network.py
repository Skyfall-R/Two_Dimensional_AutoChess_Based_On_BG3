"""Policy + value network.

Input:
    state[F_s], action_features[A, F_a]
Output:
    policy_logits[A]  - one logit per (state, action) pair
    value             - scalar in [-1, 1] estimating the round outcome
                        from the player's perspective

The policy is computed as a per-action MLP score, conditioned on a shared
state embedding (so the model is invariant to action-list order and handles
variable A). The value head uses only the state embedding.

This is intentionally small (~few hundred K params) so a single GPU can drive
many parallel rollouts and inference is cheap on CPU at game time too.
"""

from __future__ import annotations

from typing import Iterable

import torch
import torch.nn as nn
import torch.nn.functional as F


def _activation(name: str) -> nn.Module:
    if name == "relu":
        return nn.ReLU()
    if name == "gelu":
        return nn.GELU()
    if name == "tanh":
        return nn.Tanh()
    raise ValueError(f"unknown activation: {name}")


def _mlp(in_dim: int, sizes: Iterable[int], activation: str, dropout: float) -> nn.Sequential:
    layers: list[nn.Module] = []
    last = in_dim
    for hidden in sizes:
        layers.append(nn.Linear(last, hidden))
        layers.append(_activation(activation))
        if dropout > 0:
            layers.append(nn.Dropout(dropout))
        last = hidden
    return nn.Sequential(*layers), last  # type: ignore[return-value]


class PolicyValueNet(nn.Module):
    def __init__(
        self,
        state_dim: int,
        action_dim: int,
        hidden_sizes: list[int],
        action_hidden: list[int],
        activation: str = "relu",
        dropout: float = 0.0,
    ) -> None:
        super().__init__()
        self.state_dim = state_dim
        self.action_dim = action_dim
        # Shared state encoder.
        self.state_trunk, state_emb = _mlp(state_dim, hidden_sizes, activation, dropout)
        self.state_emb_dim = state_emb
        # Action scorer takes [state_emb, action_features].
        self.action_head, scorer_hidden = _mlp(
            state_emb + action_dim, action_hidden, activation, dropout
        )
        self.action_logit = nn.Linear(scorer_hidden, 1)
        # Value head from shared state embedding.
        self.value_head = nn.Sequential(
            nn.Linear(state_emb, max(64, state_emb // 2)),
            _activation(activation),
            nn.Linear(max(64, state_emb // 2), 1),
            nn.Tanh(),
        )

    def encode_state(self, state: torch.Tensor) -> torch.Tensor:
        return self.state_trunk(state)

    def score_actions(
        self,
        state_emb: torch.Tensor,
        action_features: torch.Tensor,
        action_mask: torch.Tensor,
    ) -> torch.Tensor:
        """state_emb [B, E], action_features [B, A, F_a] -> logits [B, A]."""
        if action_features.dim() == 2:
            # Single example: add batch axis.
            action_features = action_features.unsqueeze(0)
            action_mask = action_mask.unsqueeze(0)
            state_emb = state_emb.unsqueeze(0)
            squeeze = True
        else:
            squeeze = False

        batch, num_actions, _ = action_features.shape
        emb_expanded = state_emb.unsqueeze(1).expand(batch, num_actions, -1)
        combined = torch.cat([emb_expanded, action_features], dim=-1)
        flat = combined.view(batch * num_actions, -1)
        hidden = self.action_head(flat)
        logits = self.action_logit(hidden).view(batch, num_actions)
        # Mask invalid slots so they never get probability mass.
        logits = logits.masked_fill(~action_mask.bool(), -1e9)
        if squeeze:
            logits = logits.squeeze(0)
        return logits

    def forward(
        self,
        state: torch.Tensor,
        action_features: torch.Tensor,
        action_mask: torch.Tensor,
    ) -> tuple[torch.Tensor, torch.Tensor]:
        emb = self.encode_state(state)
        logits = self.score_actions(emb, action_features, action_mask)
        value = self.value_head(emb).squeeze(-1)
        return logits, value

    @torch.no_grad()
    def predict(
        self,
        state: torch.Tensor,
        action_features: torch.Tensor,
        action_mask: torch.Tensor,
    ) -> tuple[torch.Tensor, torch.Tensor]:
        self.eval()
        logits, value = self(state, action_features, action_mask)
        probs = F.softmax(logits, dim=-1)
        return probs, value


def policy_loss(
    logits: torch.Tensor, target_pi: torch.Tensor, action_mask: torch.Tensor
) -> torch.Tensor:
    """Cross-entropy between MCTS visit distribution and predicted policy."""
    # Only consider masked-in slots; invalid logits already at -1e9 so softmax
    # gives them ~0. Mask the target as well to avoid NaN gradients.
    target = target_pi * action_mask.float()
    log_probs = F.log_softmax(logits, dim=-1)
    # Sum over actions, mean over batch.
    loss = -(target * log_probs).sum(dim=-1).mean()
    return loss


def value_loss(value_pred: torch.Tensor, target_z: torch.Tensor) -> torch.Tensor:
    return F.mse_loss(value_pred, target_z)
