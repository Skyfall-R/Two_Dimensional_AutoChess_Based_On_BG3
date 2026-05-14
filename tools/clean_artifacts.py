#!/usr/bin/env python3
"""Clean generated AutoChess run artifacts.

Default mode is dry-run. Pass --apply to delete. The script only targets
generated folders/files that are ignored by git and can be recreated:
legacy ai_runs timestamp folders, test/review exports under .cache, and GUI
event logs. The training venv is preserved unless --venv is passed.
"""

from __future__ import annotations

import argparse
import re
import shutil
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
LEGACY_RUN_RE = re.compile(r"^(full-)?\d{8}-\d{6}$")


def remove_path(path: Path, apply: bool) -> None:
    action = "delete" if apply else "would delete"
    print(f"[clean] {action}: {path.relative_to(ROOT)}")
    if not apply:
        return
    if path.is_dir():
        shutil.rmtree(path)
    else:
        path.unlink(missing_ok=True)


def clean_ai_runs(apply: bool, keep: int) -> None:
    runs = ROOT / "ai_runs"
    if not runs.exists():
        return
    candidates = [
        path for path in runs.iterdir()
        if path.is_dir() and LEGACY_RUN_RE.match(path.name)
    ]
    candidates.sort(key=lambda path: path.stat().st_mtime, reverse=True)
    for path in candidates[max(0, keep):]:
        remove_path(path, apply)


def clean_cache(apply: bool, include_venv: bool) -> None:
    cache = ROOT / ".cache"
    if not cache.exists():
        return
    for path in cache.iterdir():
        if path.name in {"latest-gui-events.log"}:
            remove_path(path, apply)
        elif path.is_dir() and (
            path.name.startswith("review-")
            or path.name.startswith("test-ai-")
            or path.name == "test-ai-policies"
            or path.name == "test-ai-policies-current"
            or path.name == "test-ai-policies-roster"
        ):
            remove_path(path, apply)
        elif include_venv and path.name == "ai-venv":
            remove_path(path, apply)


def main() -> int:
    parser = argparse.ArgumentParser(description="Clean generated AutoChess artifacts.")
    parser.add_argument("--apply", action="store_true", help="Actually delete matched files.")
    parser.add_argument("--keep-runs", type=int, default=3,
                        help="Keep newest N legacy ai_runs timestamp folders.")
    parser.add_argument("--venv", action="store_true",
                        help="Also remove .cache/ai-venv; it will be recreated on demand.")
    args = parser.parse_args()

    clean_ai_runs(args.apply, args.keep_runs)
    clean_cache(args.apply, args.venv)
    if not args.apply:
        print("[clean] dry-run only; rerun with --apply to delete.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
