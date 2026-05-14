"""Hyperparameter configuration. JSON-driven so users can tweak without code.

Usage:
    cfg = load_preset("smoke")
    cfg = load_preset("full")
    cfg = TrainingConfig.from_dict(json.load(open("custom.json")))

Presets ship in tools/training/presets/ and are bundled with the repo so the
training pipeline is fully reproducible: the same preset on the same code
revision produces the same workflow.
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import Any


PRESET_DIR = Path(__file__).resolve().parent.parent / "presets"


@dataclass
class NetworkConfig:
    hidden_sizes: list[int] = field(default_factory=lambda: [256, 256])
    action_hidden: list[int] = field(default_factory=lambda: [128])
    activation: str = "relu"
    dropout: float = 0.0


@dataclass
class MCTSConfig:
    simulations: int = 64
    c_puct: float = 1.5
    dirichlet_alpha: float = 0.3
    dirichlet_epsilon: float = 0.25
    temperature_moves: int = 12  # softmax-sample for the first N moves
    temperature: float = 1.0


@dataclass
class SelfPlayConfig:
    workers: int = 4
    games_per_worker_iter: int = 4
    max_actions_per_round: int = 32
    rounds_per_game: int = 6
    opponent_difficulty: str = "Hard"
    use_opponent_pool: bool = True
    opponent_snapshot_every_iter: int = 4


@dataclass
class TrainerConfig:
    batch_size: int = 256
    minibatches_per_iter: int = 64
    learning_rate: float = 1e-3
    weight_decay: float = 1e-4
    value_loss_weight: float = 1.0
    entropy_bonus: float = 1e-3
    max_grad_norm: float = 1.0


@dataclass
class ReplayConfig:
    capacity: int = 100_000
    min_size_to_train: int = 2_000


@dataclass
class ArenaConfig:
    games_per_eval: int = 20
    accept_threshold: float = 0.55
    eval_every_iter: int = 5


@dataclass
class TrainingConfig:
    preset: str = "smoke"
    iterations: int = 10
    hours: float | None = None  # wall-clock cap; None = use iterations only
    seed: int = 7
    device: str = "auto"  # "cpu", "cuda", "auto"
    checkpoint_dir: str = "ai_runs/latest"
    checkpoint_every_iter: int = 1
    export_dir: str = "assets/ai"

    network: NetworkConfig = field(default_factory=NetworkConfig)
    mcts: MCTSConfig = field(default_factory=MCTSConfig)
    selfplay: SelfPlayConfig = field(default_factory=SelfPlayConfig)
    trainer: TrainerConfig = field(default_factory=TrainerConfig)
    replay: ReplayConfig = field(default_factory=ReplayConfig)
    arena: ArenaConfig = field(default_factory=ArenaConfig)

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "TrainingConfig":
        nested_factories = {
            "network": NetworkConfig,
            "mcts": MCTSConfig,
            "selfplay": SelfPlayConfig,
            "trainer": TrainerConfig,
            "replay": ReplayConfig,
            "arena": ArenaConfig,
        }
        data = dict(data)
        for key, factory in nested_factories.items():
            if key in data and isinstance(data[key], dict):
                data[key] = factory(**data[key])
        return cls(**data)

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)


def load_preset(name: str) -> TrainingConfig:
    path = PRESET_DIR / f"{name}.json"
    if not path.exists():
        raise FileNotFoundError(f"preset not found: {path}")
    with path.open("r", encoding="utf-8") as fp:
        data = json.load(fp)
    return TrainingConfig.from_dict(data)
