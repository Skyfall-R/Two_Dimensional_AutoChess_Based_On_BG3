"""Multi-process self-play orchestration.

`run_iteration_parallel(cfg, network, ...)` spawns N worker processes, each of
which independently imports the C++ extension, holds its own GameEngine
handles, and runs self-play games. Workers return their generated training
samples plus per-game stats.

Implementation notes:
- Workers run NN inference on CPU. The bottleneck for AutoChess self-play is
  C++ engine ticks (combat simulation + clone for MCTS), so doing inference
  on the CPU lets the main process keep the GPU free for training and avoids
  CUDA-context-per-worker headaches. With 16+ workers on a typical cloud
  instance, total throughput dwarfs single-process GPU inference for this
  model size (~few hundred K params).
- The autochess_env extension is imported per worker by scanning the same
  build directory we passed in. The C++ engine global state lives inside the
  worker process and is fully isolated from siblings.
- Samples are returned as plain numpy arrays so pickling is fast.
"""

from __future__ import annotations

import importlib.util
import os
import time
from dataclasses import dataclass
from pathlib import Path

import numpy as np
import torch
import torch.multiprocessing as mp

from .config import TrainingConfig
from .network import PolicyValueNet
from .replay import Sample
from .selfplay import play_game, GameStats


@dataclass
class WorkerResult:
    samples: list[Sample]
    stats: list[GameStats]
    duration_sec: float


def _worker_import_env(build_dir: str):
    """Import the autochess_env extension in this worker process."""
    candidates: list[Path] = []
    base = Path(build_dir)
    for pattern in ("autochess_env*.pyd", "autochess_env*.so", "autochess_env*.dll"):
        candidates.extend(base.rglob(pattern))
    candidates = [p for p in candidates if p.is_file() and not p.name.endswith((".lib", ".exp"))]
    if not candidates:
        raise RuntimeError(f"autochess_env not found under {build_dir}")
    module_path = sorted(candidates, key=lambda p: len(p.parts))[0]
    spec = importlib.util.spec_from_file_location("autochess_env", module_path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _worker_entry(args: dict) -> WorkerResult:
    """Run inside a child process: import env, build network on CPU, play games."""
    build_dir = args["build_dir"]
    state_dim = args["state_dim"]
    action_dim = args["action_dim"]
    state_dict = args["state_dict"]
    cfg_dict = args["cfg_dict"]
    seeds = args["seeds"]
    opponent_difficulty = args.get("opponent_difficulty", "Hard")

    # Each worker gets its own RNG, seeded deterministically from the first
    # job seed so that re-running a config + iteration gives the same data
    # if the user wants to debug.
    rng = np.random.default_rng(seeds[0] if seeds else 0)

    # Reconstitute the typed config from the dict so play_game can read it.
    cfg = TrainingConfig.from_dict(cfg_dict)

    env_module = _worker_import_env(build_dir)

    device = torch.device("cpu")
    network = PolicyValueNet(
        state_dim=state_dim,
        action_dim=action_dim,
        hidden_sizes=cfg.network.hidden_sizes,
        action_hidden=cfg.network.action_hidden,
        activation=cfg.network.activation,
        dropout=cfg.network.dropout,
    ).to(device)
    network.load_state_dict(state_dict)
    network.eval()

    # Limit each worker to a single intra-op thread so 16 workers don't all
    # try to use 16 BLAS threads each (the cloud instance would thrash).
    torch.set_num_threads(1)

    samples_out: list[Sample] = []
    stats_out: list[GameStats] = []
    started = time.time()
    for seed in seeds:
        try:
            samples, stats = play_game(
                env_module=env_module,
                network=network,
                device=device,
                config=cfg,
                seed=seed,
                rng=rng,
                opponent_difficulty=opponent_difficulty,
            )
            samples_out.extend(samples)
            stats_out.append(stats)
        except Exception as exc:  # pragma: no cover - keep workers alive
            # Surface but don't kill the worker; missing samples are fine.
            print(f"[selfplay-worker {os.getpid()}] game seed={seed} failed: {exc}", flush=True)

    return WorkerResult(samples=samples_out, stats=stats_out, duration_sec=time.time() - started)


def run_iteration_parallel(
    cfg: TrainingConfig,
    network: PolicyValueNet,
    build_dir: str,
    state_dim: int,
    action_dim: int,
    iteration: int,
    base_seed: int,
    opponent_difficulty: str,
) -> WorkerResult:
    """Dispatch self-play across cfg.selfplay.workers processes.

    Returns a combined WorkerResult.
    """
    workers = max(1, int(cfg.selfplay.workers))
    games_per_worker = max(1, int(cfg.selfplay.games_per_worker_iter))
    total_games = workers * games_per_worker

    # Network weights to ship to children. CPU-side so each worker can load
    # them onto its own CPU device without CUDA negotiation.
    cpu_state = {k: v.detach().cpu() for k, v in network.state_dict().items()}
    cfg_dict = cfg.to_dict()

    # Pre-compute seeds so every game is reproducible from (iteration, slot).
    seeds_per_worker = []
    for w in range(workers):
        seeds_per_worker.append(
            [int((iteration * 10_000 + w * 1_000 + g) ^ base_seed) for g in range(games_per_worker)]
        )

    payloads = [
        {
            "build_dir": str(build_dir),
            "state_dim": state_dim,
            "action_dim": action_dim,
            "state_dict": cpu_state,
            "cfg_dict": cfg_dict,
            "seeds": seeds_per_worker[w],
            "opponent_difficulty": opponent_difficulty,
        }
        for w in range(workers)
    ]

    started = time.time()
    if workers == 1:
        # Skip the multiprocessing overhead for tiny presets.
        results = [_worker_entry(payloads[0])]
    else:
        ctx = mp.get_context("spawn")
        with ctx.Pool(processes=workers) as pool:
            results = pool.map(_worker_entry, payloads)

    samples: list[Sample] = []
    stats: list[GameStats] = []
    for r in results:
        samples.extend(r.samples)
        stats.extend(r.stats)
    _ = total_games  # silence unused if total_games drifts from len(stats)
    return WorkerResult(samples=samples, stats=stats, duration_sec=time.time() - started)
