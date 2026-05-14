"""Logging + checkpoint helpers."""

from __future__ import annotations

import json
import sys
import time
from pathlib import Path
from typing import Any


def info(msg: str) -> None:
    print(f"[az-train {time.strftime('%H:%M:%S')}] {msg}", flush=True)


def fatal(msg: str) -> None:
    print(f"[az-train ERROR] {msg}", file=sys.stderr, flush=True)


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2), encoding="utf-8")


def read_json(path: Path) -> dict[str, Any] | None:
    if not path.exists():
        return None
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return None


def resolve_device(spec: str):
    import torch

    if spec == "auto":
        return torch.device("cuda" if torch.cuda.is_available() else "cpu")
    if spec == "cuda":
        if not torch.cuda.is_available():
            raise RuntimeError("device=cuda requested but no GPU available")
        return torch.device("cuda")
    return torch.device(spec)
