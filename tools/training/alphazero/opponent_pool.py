"""Opponent pool with PFSP (prioritized fictitious self-play) sampling.

Snapshots of accepted networks are stored as torch state_dicts. When picking
an opponent for a self-play game, we sample from the pool with probability
proportional to (1 - win_rate)^p, biasing toward opponents the candidate
struggles against. Empty pool falls back to scripted Normal AI.
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np
import torch


@dataclass
class OpponentEntry:
    iteration: int
    state_dict_path: str
    win_rate_against_current: float = 0.5  # symmetric: starts neutral

    def to_dict(self) -> dict:
        return {
            "iteration": self.iteration,
            "state_dict_path": self.state_dict_path,
            "win_rate_against_current": self.win_rate_against_current,
        }

    @classmethod
    def from_dict(cls, data: dict) -> "OpponentEntry":
        return cls(
            iteration=int(data["iteration"]),
            state_dict_path=str(data["state_dict_path"]),
            win_rate_against_current=float(data.get("win_rate_against_current", 0.5)),
        )


@dataclass
class OpponentPool:
    directory: Path
    entries: list[OpponentEntry] = field(default_factory=list)
    p: float = 2.0
    max_size: int = 12

    def add(self, iteration: int, state_dict: dict) -> Path:
        self.directory.mkdir(parents=True, exist_ok=True)
        path = self.directory / f"opponent-{iteration:06d}.pt"
        torch.save(state_dict, path)
        self.entries.append(OpponentEntry(iteration=iteration, state_dict_path=str(path)))
        if len(self.entries) > self.max_size:
            # Drop oldest, but never drop the latest
            evicted = self.entries.pop(0)
            try:
                Path(evicted.state_dict_path).unlink(missing_ok=True)
            except OSError:
                pass
        self._save_index()
        return path

    def sample(self, rng: np.random.Generator) -> OpponentEntry | None:
        if not self.entries:
            return None
        weights = np.array(
            [max(1e-3, (1.0 - e.win_rate_against_current)) ** self.p for e in self.entries],
            dtype=np.float64,
        )
        weights /= weights.sum()
        idx = int(rng.choice(len(self.entries), p=weights))
        return self.entries[idx]

    def update_win_rate(self, iteration: int, win_rate: float) -> None:
        for entry in self.entries:
            if entry.iteration == iteration:
                entry.win_rate_against_current = float(win_rate)
                break
        self._save_index()

    def _save_index(self) -> None:
        index = self.directory / "index.json"
        index.write_text(
            json.dumps([e.to_dict() for e in self.entries], indent=2),
            encoding="utf-8",
        )

    @classmethod
    def load(cls, directory: Path) -> "OpponentPool":
        directory = Path(directory)
        pool = cls(directory=directory)
        index = directory / "index.json"
        if index.exists():
            try:
                data = json.loads(index.read_text(encoding="utf-8"))
                pool.entries = [OpponentEntry.from_dict(d) for d in data]
            except json.JSONDecodeError:
                pool.entries = []
        return pool
