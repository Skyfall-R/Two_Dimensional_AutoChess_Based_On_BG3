#!/usr/bin/env python3
"""One-command AutoChess AI training (AlphaZero-style with MCTS).

Workflow:
    1. Make sure C++ targets are built (autochess_tests + autochess_env).
    2. Import the autochess_env extension.
    3. Bootstrap or resume an AlphaZero checkpoint (network + replay buffer
       + opponent pool). Rules-fingerprint changes are detected and force
       a fresh start with a clear warning.
    4. Iterate self-play -> training -> arena -> snapshot until iterations
       OR --hours wall-clock cap is reached. Ctrl-C is honoured: the
       current state is checkpointed so you can resume later.
    5. Distill the trained network to a linear policy and write to
       assets/ai/{hard,superhard}.policy.json so the C++ game can use it.

Reproducibility:
    Whenever the engine's rules fingerprint changes (you edited unit
    specs, board, economy constants, etc.) the existing checkpoint is
    detected as stale and a fresh training run is started automatically.
    Re-running this script with the same arguments after a rule change is
    therefore a true one-click retrain.

Quick usage:
    python tools/train_ai.py --preset smoke      # ~1 min sanity check
    python tools/train_ai.py --preset short      # ~30 min run
    python tools/train_ai.py --preset full       # 8h+ run with checkpoints
    python tools/train_ai.py --preset full --resume ai_runs/full

Environment:
    Set AUTOCHESS_SKIP_TRAINING_VENV=1 if you already have torch in your
    active Python; otherwise this script bootstraps a venv at
    .cache/ai-venv on demand.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import os
import shutil
import signal
import subprocess
import sys
import sysconfig
import time
import venv
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

VENV_DIR = ROOT / ".cache" / "ai-venv"
TRAINING_PRESET = os.environ.get(
    "AUTOCHESS_TRAINING_PRESET",
    "training-x64-debug" if os.name == "nt" else "training-linux-release",
)
BUILD_DIR = ROOT / "build" / TRAINING_PRESET
VS_CMAKE = Path(
    r"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
)


def _module_importable(name: str) -> bool:
    try:
        __import__(name)
    except Exception:
        return False
    return True


def run(command: list[str | os.PathLike[str]], cwd: Path = ROOT) -> None:
    printable = " ".join(str(part) for part in command)
    print(f"[train-ai] {printable}", flush=True)
    subprocess.run([str(part) for part in command], cwd=cwd, check=True)


def ensure_venv(needs_torch: bool) -> None:
    if os.environ.get("AUTOCHESS_TRAINING_VENV_ACTIVE") == "1":
        return
    if os.environ.get("AUTOCHESS_SKIP_TRAINING_VENV") == "1":
        return
    if _module_importable("torch") and _module_importable("numpy"):
        print("[train-ai] using active Python environment with torch + numpy", flush=True)
        return

    python_exe = VENV_DIR / ("Scripts/python.exe" if os.name == "nt" else "bin/python")
    if not python_exe.exists():
        print(f"[train-ai] creating training venv: {VENV_DIR}", flush=True)
        VENV_DIR.parent.mkdir(parents=True, exist_ok=True)
        venv.EnvBuilder(with_pip=True).create(VENV_DIR)
    pip_exe = VENV_DIR / ("Scripts/pip.exe" if os.name == "nt" else "bin/pip")
    requirements = ROOT / "requirements-train.txt"
    run([pip_exe, "install", "-r", requirements])

    env = os.environ.copy()
    env["AUTOCHESS_TRAINING_VENV_ACTIVE"] = "1"
    result = subprocess.run([str(python_exe), *sys.argv], cwd=ROOT, env=env)
    raise SystemExit(result.returncode)


def find_cmake() -> Path:
    cmake = shutil.which("cmake")
    if cmake:
        return Path(cmake)
    if VS_CMAKE.exists():
        return VS_CMAKE
    raise RuntimeError(
        "cmake was not found in PATH and the Visual Studio bundled cmake path does not exist."
    )


def build_extension(cmake: Path) -> None:
    include_dir = Path(sysconfig.get_path("include"))
    lib_dir = Path(sysconfig.get_config_var("LIBDIR") or "")
    library_name = sysconfig.get_config_var("LDLIBRARY") or sysconfig.get_config_var("LIBRARY") or ""
    library_path = lib_dir / library_name
    if os.name == "nt" and library_path.suffix.lower() != ".lib":
        version = f"{sys.version_info.major}{sys.version_info.minor}"
        roots = [lib_dir, Path(sys.base_prefix) / "libs", Path(sys.prefix) / "libs"]
        for root in roots:
            for candidate in (root / f"python{version}.lib", root / "python3.lib"):
                if candidate.exists():
                    library_path = candidate
                    break

    configure = [cmake, "--preset", TRAINING_PRESET, "-DAUTOCHESS_BUILD_PYTHON=ON"]
    if include_dir.exists() and library_path.exists():
        configure.extend([
            f"-DAUTOCHESS_PYTHON_INCLUDE_DIR={include_dir}",
            f"-DAUTOCHESS_PYTHON_LIBRARY={library_path}",
        ])
    run(configure)
    run([cmake, "--build", "--preset", TRAINING_PRESET, "--target", "autochess_tests"])
    run([cmake, "--build", "--preset", TRAINING_PRESET, "--target", "autochess_env"])
    ctest = cmake.with_name("ctest.exe" if os.name == "nt" else "ctest")
    if ctest.exists():
        run([ctest, "--preset", TRAINING_PRESET])
    else:
        run([cmake, "--build", "--preset", TRAINING_PRESET, "--target", "RUN_TESTS"])


def import_env_module():
    candidates: list[Path] = []
    for pattern in ("autochess_env*.pyd", "autochess_env*.so", "autochess_env*.dll"):
        candidates.extend(BUILD_DIR.rglob(pattern))
    candidates = [p for p in candidates if p.is_file() and not p.name.endswith((".lib", ".exp"))]
    if not candidates:
        raise RuntimeError(f"autochess_env extension was not found under {BUILD_DIR}")
    module_path = sorted(candidates, key=lambda p: len(p.parts))[0]
    spec = importlib.util.spec_from_file_location("autochess_env", module_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot import extension: {module_path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    print(f"[train-ai] imported {module_path}", flush=True)
    return module


# ---------------------------------------------------------------------------
# AlphaZero training driver.
# ---------------------------------------------------------------------------


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Train and export AutoChess AI policies.")
    parser.add_argument("--preset", default="smoke",
                        help="Preset name in tools/training/presets/ (smoke, short, full) "
                             "or path to a JSON config.")
    parser.add_argument("--export", type=Path, default=None,
                        help="Override export directory for *.policy.json files.")
    parser.add_argument("--checkpoint-dir", type=Path, default=None,
                        help="Override checkpoint directory.")
    parser.add_argument("--iterations", type=int, default=None,
                        help="Override preset iteration count.")
    parser.add_argument("--hours", type=float, default=None,
                        help="Wall-clock cap; overrides preset.")
    parser.add_argument("--device", default=None,
                        help="cpu / cuda / auto. Overrides preset.")
    parser.add_argument("--resume", type=Path, default=None,
                        help="Explicit checkpoint dir to resume from.")
    parser.add_argument("--fresh", action="store_true",
                        help="Force fresh start even if a checkpoint exists.")
    parser.add_argument("--skip-build", action="store_true",
                        help="Skip CMake build step.")
    parser.add_argument("--no-venv", action="store_true",
                        help="Do not bootstrap a training venv.")
    return parser.parse_args()


def load_config(preset_arg: str):
    from training.alphazero.config import load_preset, TrainingConfig

    if preset_arg.endswith(".json") and Path(preset_arg).exists():
        with open(preset_arg, "r", encoding="utf-8") as fp:
            return TrainingConfig.from_dict(json.load(fp))
    return load_preset(preset_arg)


def main() -> int:
    args = parse_args()
    if not args.no_venv:
        ensure_venv(needs_torch=True)

    cmake = find_cmake()
    if not args.skip_build:
        build_extension(cmake)

    env_module = import_env_module()

    import numpy as np
    import torch

    from training.alphazero.config import TrainingConfig
    from training.alphazero import utils
    from training.alphazero.network import PolicyValueNet
    from training.alphazero.replay import ReplayBuffer
    from training.alphazero.opponent_pool import OpponentPool
    from training.alphazero.selfplay import play_game
    from training.alphazero.trainer import train_one_iteration
    from training.alphazero.arena import evaluate_vs_difficulty
    from training.alphazero.export import export_policies

    cfg: TrainingConfig = load_config(args.preset)
    if args.export is not None:
        cfg.export_dir = str(args.export)
    if args.checkpoint_dir is not None:
        cfg.checkpoint_dir = str(args.checkpoint_dir)
    if args.iterations is not None:
        cfg.iterations = args.iterations
    if args.hours is not None:
        cfg.hours = args.hours
    if args.device is not None:
        cfg.device = args.device

    device = utils.resolve_device(cfg.device)
    utils.info(f"device = {device}")

    probe_handle = env_module.env_create(seed=1, max_steps=128, difficulty="Hard")
    rules_fingerprint = env_module.env_rules_fingerprint(probe_handle)
    schema = env_module.env_feature_schema(probe_handle)
    state_dim = int(schema["stateFeatureCount"])
    action_dim = int(schema["actionFeatureCount"])
    env_module.env_close(probe_handle)
    utils.info(f"rules fingerprint = {rules_fingerprint}")
    utils.info(f"state/action features = {state_dim}/{action_dim}")

    checkpoint_dir = (
        Path(args.resume) if args.resume else (ROOT / cfg.checkpoint_dir).resolve()
    )
    checkpoint_dir.mkdir(parents=True, exist_ok=True)
    state_path = checkpoint_dir / "state.json"
    weights_path = checkpoint_dir / "network.pt"
    replay_path = checkpoint_dir / "replay.json"
    opponent_dir = checkpoint_dir / "opponents"

    network = PolicyValueNet(
        state_dim=state_dim,
        action_dim=action_dim,
        hidden_sizes=cfg.network.hidden_sizes,
        action_hidden=cfg.network.action_hidden,
        activation=cfg.network.activation,
        dropout=cfg.network.dropout,
    ).to(device)
    optimizer = torch.optim.Adam(
        network.parameters(),
        lr=cfg.trainer.learning_rate,
        weight_decay=cfg.trainer.weight_decay,
    )
    replay = ReplayBuffer(capacity=cfg.replay.capacity)
    pool = OpponentPool.load(opponent_dir)

    iteration_start = 0
    state_meta = utils.read_json(state_path) if not args.fresh else None
    if state_meta and state_meta.get("rules_fingerprint") == rules_fingerprint and weights_path.exists():
        utils.info(f"resuming from {checkpoint_dir} (iter {state_meta['iteration']})")
        network.load_state_dict(torch.load(weights_path, map_location=device))
        if replay_path.exists():
            try:
                replay = ReplayBuffer.deserialize(json.loads(replay_path.read_text(encoding="utf-8")))
                utils.info(f"replay buffer restored: {len(replay)} samples")
            except Exception as exc:
                utils.fatal(f"failed to restore replay: {exc}; starting empty")
                replay = ReplayBuffer(capacity=cfg.replay.capacity)
        iteration_start = int(state_meta["iteration"]) + 1
    elif state_meta and state_meta.get("rules_fingerprint") != rules_fingerprint:
        utils.info(
            f"rules changed (was {state_meta.get('rules_fingerprint')}, now {rules_fingerprint}); "
            "starting fresh training run"
        )
    else:
        utils.info(f"starting fresh training in {checkpoint_dir}")

    rng = np.random.default_rng(cfg.seed + iteration_start)
    deadline = time.time() + cfg.hours * 3600 if cfg.hours else None
    last_iter = iteration_start
    last_metrics: dict[str, Any] = {}

    def save_checkpoint(iteration: int, metrics: dict | None) -> None:
        torch.save(network.state_dict(), weights_path)
        replay_path.write_text(json.dumps(replay.serialize()), encoding="utf-8")
        utils.write_json(
            state_path,
            {
                "iteration": iteration,
                "rules_fingerprint": rules_fingerprint,
                "preset": cfg.preset,
                "savedAt": int(time.time()),
                "metrics": metrics or {},
                "config": cfg.to_dict(),
            },
        )

    interrupted = {"flag": False}

    def handle_sigint(signum, frame):
        if interrupted["flag"]:
            utils.fatal("second Ctrl-C, exiting hard")
            sys.exit(130)
        utils.info("Ctrl-C received - finishing current iteration then saving checkpoint")
        interrupted["flag"] = True

    signal.signal(signal.SIGINT, handle_sigint)

    try:
        target_iters = max(iteration_start + 1, cfg.iterations)
        for iteration in range(iteration_start, target_iters):
            last_iter = iteration
            t_iter = time.time()

            # 1. Self-play.
            new_samples = 0
            zs = []
            game_count = max(1, cfg.selfplay.games_per_worker_iter * max(1, cfg.selfplay.workers))
            for game in range(game_count):
                seed = (iteration * 10_000 + game) ^ cfg.seed
                samples, stats = play_game(
                    env_module=env_module,
                    network=network,
                    device=device,
                    config=cfg,
                    seed=seed,
                    rng=rng,
                    opponent_difficulty=cfg.selfplay.opponent_difficulty,
                )
                replay.extend(samples)
                new_samples += len(samples)
                zs.append(stats.final_value)
            avg_z = sum(zs) / max(1, len(zs))
            utils.info(
                f"iter {iteration} self-play: {new_samples} samples, "
                f"avg_z={avg_z:+.3f}, replay={len(replay)}"
            )

            # 2. Train.
            metrics = train_one_iteration(network, optimizer, replay, cfg, device)
            if metrics.iterations:
                utils.info(
                    f"iter {iteration} train: pi_loss={metrics.avg_policy_loss:.4f} "
                    f"v_loss={metrics.avg_value_loss:.4f} samples={metrics.samples_seen}"
                )

            # 3. Arena.
            if (iteration + 1) % cfg.arena.eval_every_iter == 0:
                result = evaluate_vs_difficulty(
                    env_module=env_module,
                    network=network,
                    device=device,
                    config=cfg,
                    difficulty="Normal",
                )
                utils.info(
                    f"iter {iteration} arena vs Normal: "
                    f"{result.wins}/{result.games} = {result.win_rate:.2%}"
                )
                last_metrics = {
                    "win_rate_vs_normal": result.win_rate,
                    "wins": result.wins,
                    "games": result.games,
                    "policy_loss": metrics.avg_policy_loss,
                    "value_loss": metrics.avg_value_loss,
                    "self_play_avg_z": avg_z,
                }

            # 4. Snapshot for opponent pool.
            if (
                cfg.selfplay.use_opponent_pool
                and (iteration + 1) % cfg.selfplay.opponent_snapshot_every_iter == 0
            ):
                pool.add(iteration, {k: v.detach().cpu() for k, v in network.state_dict().items()})
                utils.info(f"iter {iteration} added to opponent pool (size={len(pool.entries)})")

            # 5. Checkpoint.
            if (iteration + 1) % cfg.checkpoint_every_iter == 0:
                save_checkpoint(iteration, last_metrics)
                utils.info(f"iter {iteration} checkpoint saved ({time.time() - t_iter:.1f}s)")

            if interrupted["flag"]:
                break
            if deadline and time.time() >= deadline:
                utils.info("hour budget reached, finishing")
                break

    finally:
        save_checkpoint(last_iter, last_metrics)

        export_dir = (
            Path(cfg.export_dir)
            if Path(cfg.export_dir).is_absolute()
            else (ROOT / cfg.export_dir)
        ).resolve()
        utils.info(f"distilling network -> linear policy at {export_dir}")

        # Pick a weak-but-coherent snapshot for Hard. We prefer the median
        # opponent-pool entry (mid-training strength) so Hard plays
        # noticeably below SuperHard. If the pool is empty (very short
        # runs), export.py falls back to scaled-noise synthesis.
        weak_network = None
        try:
            if pool.entries:
                mid = pool.entries[len(pool.entries) // 2]
                weak_network = PolicyValueNet(
                    state_dim=state_dim,
                    action_dim=action_dim,
                    hidden_sizes=cfg.network.hidden_sizes,
                    action_hidden=cfg.network.action_hidden,
                    activation=cfg.network.activation,
                    dropout=cfg.network.dropout,
                ).to(device)
                state_dict = torch.load(mid.state_dict_path, map_location=device)
                weak_network.load_state_dict(state_dict)
                weak_network.eval()
                utils.info(
                    f"using opponent-pool snapshot iter={mid.iteration} for Hard distillation"
                )
        except Exception as exc:
            utils.fatal(f"weak-network load failed ({exc}); falling back to synthetic Hard")
            weak_network = None

        try:
            written = export_policies(
                env_module=env_module,
                network=network,
                device=device,
                state_dim=state_dim,
                action_dim=action_dim,
                rules_fingerprint=rules_fingerprint,
                export_dir=export_dir,
                metrics=last_metrics,
                weak_network=weak_network,
                seed=cfg.seed,
            )
            for path in written:
                utils.info(f"wrote {path}")
        except Exception as exc:
            utils.fatal(f"export failed: {exc}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
