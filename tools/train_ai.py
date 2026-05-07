#!/usr/bin/env python3
"""One-command AutoChess AI training and policy export.

Smoke mode is intentionally lightweight: it builds the C++ headless
environment, validates the Python extension, runs a few deterministic episodes,
and exports valid three-tier policy packages. Full mode uses torch when
available and otherwise exits with a clear Python-version/dependency message.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import math
import os
from pathlib import Path
import random
import shutil
import subprocess
import sys
import sysconfig
import time
import venv


ROOT = Path(__file__).resolve().parents[1]
VENV_DIR = ROOT / ".cache" / "ai-venv"
TRAINING_PRESET = os.environ.get(
    "AUTOCHESS_TRAINING_PRESET",
    "training-x64-debug" if os.name == "nt" else "training-linux-release",
)
BUILD_DIR = ROOT / "build" / TRAINING_PRESET
VS_CMAKE = Path(
    r"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
)
POLICY_FORMAT = "autochess_policy_v1"
MODEL_VERSION = "linear-v1"
DIFFICULTIES = ["Normal", "Hard", "SuperHard"]


def run(command: list[str | os.PathLike[str]], cwd: Path = ROOT) -> None:
    printable = " ".join(str(part) for part in command)
    print(f"[train-ai] {printable}")
    subprocess.run([str(part) for part in command], cwd=cwd, check=True)


def ensure_venv() -> None:
    if os.environ.get("AUTOCHESS_TRAINING_VENV_ACTIVE") == "1":
        return
    if os.environ.get("AUTOCHESS_SKIP_TRAINING_VENV") == "1":
        return

    python_exe = VENV_DIR / ("Scripts/python.exe" if os.name == "nt" else "bin/python")
    if not python_exe.exists():
        print(f"[train-ai] creating training venv: {VENV_DIR}")
        VENV_DIR.parent.mkdir(parents=True, exist_ok=True)
        venv.EnvBuilder(with_pip=True).create(VENV_DIR)

    pip_exe = VENV_DIR / ("Scripts/pip.exe" if os.name == "nt" else "bin/pip")
    requirements = ROOT / "requirements-train.txt"
    run([pip_exe, "install", "-r", requirements])

    env = os.environ.copy()
    env["AUTOCHESS_TRAINING_VENV_ACTIVE"] = "1"
    subprocess.run([str(python_exe), *sys.argv], cwd=ROOT, env=env, check=True)
    raise SystemExit(0)


def find_cmake() -> Path:
    cmake = shutil.which("cmake")
    if cmake:
        return Path(cmake)
    if VS_CMAKE.exists():
        return VS_CMAKE
    raise RuntimeError(
        "cmake was not found in PATH and the Visual Studio bundled cmake path does not exist. "
        "Open the project once in VSCode or install CMake, then rerun this command."
    )


def build_extension(cmake: Path) -> None:
    include_dir = Path(sysconfig.get_path("include"))
    lib_dir = Path(sysconfig.get_config_var("LIBDIR") or "")
    library_name = sysconfig.get_config_var("LDLIBRARY") or sysconfig.get_config_var("LIBRARY") or ""
    library_path = lib_dir / library_name
    if os.name == "nt" and library_path.suffix.lower() != ".lib":
        version = f"{sys.version_info.major}{sys.version_info.minor}"
        roots = [lib_dir, Path(sys.base_prefix) / "libs", Path(sys.prefix) / "libs"]
        candidates = []
        for root in roots:
            candidates.extend([root / f"python{version}.lib", root / "python3.lib"])
        library_path = next((candidate for candidate in candidates if candidate.exists()), library_path)

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
    candidates = []
    for pattern in ("autochess_env*.pyd", "autochess_env*.so", "autochess_env*.dll"):
        candidates.extend(BUILD_DIR.rglob(pattern))
    candidates = [path for path in candidates if path.is_file() and not path.name.endswith((".lib", ".exp"))]
    if not candidates:
        raise RuntimeError(f"autochess_env extension was not found under {BUILD_DIR}")
    module_path = sorted(candidates, key=lambda path: len(path.parts))[0]
    spec = importlib.util.spec_from_file_location("autochess_env", module_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot import extension: {module_path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    print(f"[train-ai] imported {module_path}")
    return module


def combined_features(env, action: dict) -> list[float]:
    return list(env.state_features()) + list(action["features"])


def score_action(weights: list[float], bias: float, features: list[float]) -> float:
    return bias + sum(weight * value for weight, value in zip(weights, features))


def pick_action(env, weights: list[float], bias: float, epsilon: float, rng: random.Random) -> int:
    actions = env.legal_actions()
    if not actions:
        return 0
    if rng.random() < epsilon:
        return rng.randrange(len(actions))
    best_index = 0
    best_score = -math.inf
    for action in actions:
        features = combined_features(env, action)
        score = score_action(weights, bias, features)
        if score > best_score:
            best_score = score
            best_index = int(action["index"])
    return best_index


def evaluate_policy(env, weights: list[float], bias: float, episodes: int, seed: int) -> dict:
    rng = random.Random(seed)
    total_reward = 0.0
    wins = 0
    for episode in range(episodes):
        env.reset(seed=seed + episode, max_steps=160, difficulty="Normal")
        done = False
        winner = ""
        while not done:
            action_index = pick_action(env, weights, bias, epsilon=0.02, rng=rng)
            result = env.step(action_index)
            total_reward += float(result["reward"])
            done = bool(result["done"])
            winner = result["observation"].get("winner", "")
        if winner == "Player1":
            wins += 1
    return {"episodes": episodes, "totalReward": total_reward, "winRate": wins / max(1, episodes)}


def smoke_train(env, schema: dict, episodes: int, seed: int) -> dict[str, dict]:
    rng = random.Random(seed)
    feature_count = int(schema["stateFeatureCount"]) + int(schema["actionFeatureCount"])
    base_weights = [rng.uniform(-0.015, 0.015) for _ in range(feature_count)]
    policies: dict[str, dict] = {}
    profiles = {
        "Normal": {"heuristicBlend": 0.65, "noise": 0.025},
        "Hard": {"heuristicBlend": 1.20, "noise": 0.010},
        "SuperHard": {"heuristicBlend": 3.00, "noise": 0.000},
    }
    for difficulty in DIFFICULTIES:
        profile = profiles[difficulty]
        if difficulty == "SuperHard":
            weights = [0.0 for _ in range(feature_count)]
        else:
            weights = [value + rng.uniform(-profile["noise"], profile["noise"]) for value in base_weights]
        metrics = evaluate_policy(env, weights, 0.0, episodes=max(1, episodes), seed=seed + len(policies) * 100)
        policies[difficulty] = {
            "weights": weights,
            "bias": 0.0,
            "heuristicBlend": profile["heuristicBlend"],
            "metrics": metrics,
        }
    return policies


def write_checkpoint(
    checkpoint_dir: Path,
    episode: int,
    weights: list[float],
    bias: float,
    total_reward: float,
    device: str,
) -> None:
    checkpoint_dir.mkdir(parents=True, exist_ok=True)
    payload = {
        "episode": episode,
        "bias": bias,
        "totalReward": total_reward,
        "device": device,
        "weights": [round(float(value), 8) for value in weights],
        "savedAt": int(time.time()),
    }
    path = checkpoint_dir / f"checkpoint-{episode:06d}.json"
    path.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    print(f"[train-ai] checkpoint {episode} -> {path}")


def full_train(
    env,
    schema: dict,
    episodes: int | None,
    seed: int,
    device: str,
    hours: float | None,
    checkpoint_every: int,
    checkpoint_dir: Path,
) -> dict[str, dict]:
    try:
        import torch
    except Exception as exc:  # pragma: no cover - depends on local Python wheels
        raise RuntimeError(
            "Full PPO training needs PyTorch. The current Python cannot import torch. "
            "On Python 3.13 this is expected until compatible wheels are installed; "
            "run --preset smoke for a build/export check or use Python 3.12 for full training."
        ) from exc

    if device.startswith("cuda") and not torch.cuda.is_available():
        raise RuntimeError(
            "CUDA was requested, but torch.cuda.is_available() is False. "
            "Install a CUDA-enabled PyTorch wheel in the training environment first, "
            "then rerun with --device cuda."
        )

    rng = random.Random(seed)
    torch.manual_seed(seed)
    if device.startswith("cuda"):
        torch.cuda.manual_seed_all(seed)
        print(f"[train-ai] CUDA device: {torch.cuda.get_device_name(0)}")

    feature_count = int(schema["stateFeatureCount"]) + int(schema["actionFeatureCount"])
    weights = torch.zeros(feature_count, device=device, requires_grad=True)
    bias = torch.zeros((), device=device, requires_grad=True)
    optimizer = torch.optim.Adam([weights, bias], lr=0.01)
    checkpoint_pool: list[list[float]] = []
    deadline = time.time() + hours * 3600.0 if hours and hours > 0 else None
    episode = 0
    total_training_reward = 0.0

    while True:
        if deadline is not None and time.time() >= deadline:
            break
        if deadline is None and episodes is not None and episode >= max(1, episodes):
            break
        if deadline is not None and episodes is not None and episode >= episodes:
            break

        opponent = rng.choice(DIFFICULTIES)
        env.reset(seed=seed + episode, max_steps=192, difficulty=opponent)
        log_probs = []
        rewards = []
        done = False
        while not done:
            actions = env.legal_actions()
            if not actions:
                break
            feature_rows = [combined_features(env, action) for action in actions]
            tensor = torch.tensor(feature_rows, dtype=torch.float32, device=device)
            logits = tensor.matmul(weights) + bias
            dist = torch.distributions.Categorical(logits=logits)
            action_offset = dist.sample()
            log_prob = dist.log_prob(action_offset)
            result = env.step(int(actions[int(action_offset)]["index"]))
            log_probs.append(log_prob)
            rewards.append(float(result["reward"]))
            done = bool(result["done"])
            if done:
                break

        returns = []
        running = 0.0
        for reward in reversed(rewards):
            running = reward + 0.98 * running
            returns.append(running)
        returns.reverse()
        if returns:
            returns_tensor = torch.tensor(returns, dtype=torch.float32, device=device)
            returns_tensor = (returns_tensor - returns_tensor.mean()) / (returns_tensor.std(unbiased=False) + 1e-6)
            policy_loss = []
            for log_prob, advantage in zip(log_probs, returns_tensor):
                ratio = torch.exp(log_prob - log_prob.detach())
                clipped = torch.clamp(ratio, 0.8, 1.2) * advantage
                policy_loss.append(-torch.minimum(ratio * advantage, clipped))
            loss = torch.stack(policy_loss).mean()
            optimizer.zero_grad()
            loss.backward()
            optimizer.step()

        episode_reward = sum(rewards)
        total_training_reward += episode_reward
        if episode % 10 == 0:
            checkpoint_pool.append(weights.detach().cpu().tolist())
        if episode > 0 and checkpoint_every > 0 and episode % checkpoint_every == 0:
            write_checkpoint(
                checkpoint_dir,
                episode,
                weights.detach().cpu().tolist(),
                float(bias.detach().cpu()),
                total_training_reward,
                device,
            )
        if episode % 25 == 0:
            elapsed = time.strftime("%H:%M:%S", time.gmtime(time.time() - (deadline - hours * 3600.0) if deadline else 0))
            print(
                f"[train-ai] episode={episode} opponent={opponent} reward={episode_reward:.3f} "
                f"total={total_training_reward:.3f} elapsed={elapsed}"
            )
        episode += 1

    trained = weights.detach().cpu().tolist()
    write_checkpoint(
        checkpoint_dir,
        max(0, episode),
        trained,
        float(bias.detach().cpu()),
        total_training_reward,
        device,
    )

    policies: dict[str, dict] = {}
    blends = {"Normal": 0.65, "Hard": 1.20, "SuperHard": 3.00}
    for index, difficulty in enumerate(DIFFICULTIES):
        if difficulty == "SuperHard":
            source = trained
            scale = 1.40
            noise = 0.0
        else:
            source_index = int((index + 1) * max(0, len(checkpoint_pool) - 1) / max(1, len(DIFFICULTIES)))
            source = checkpoint_pool[source_index] if checkpoint_pool else trained
            scale = 0.65 + index * 0.30
            noise = 0.004
        mixed = [scale * value + rng.uniform(-noise, noise) for value in source]
        metrics = evaluate_policy(env, mixed, float(bias.detach().cpu()), episodes=8, seed=seed + index * 200)
        policies[difficulty] = {
            "weights": mixed,
            "bias": float(bias.detach().cpu()),
            "heuristicBlend": blends[difficulty],
            "metrics": metrics,
        }
    return policies


def export_policies(export_dir: Path, env, schema: dict, policies: dict[str, dict], preset: str) -> None:
    export_dir.mkdir(parents=True, exist_ok=True)
    fingerprint = env.rules_fingerprint()
    expected_names = {f"{difficulty.lower()}.policy.json" for difficulty in DIFFICULTIES}
    for stale in export_dir.glob("*.policy.json"):
        if stale.name not in expected_names:
            stale.unlink()
            print(f"[train-ai] removed stale policy {stale}")
    for difficulty, policy in policies.items():
        payload = {
            "format": POLICY_FORMAT,
            "modelVersion": MODEL_VERSION,
            "difficulty": difficulty,
            "rulesFingerprint": fingerprint,
            "stateFeatureCount": int(schema["stateFeatureCount"]),
            "actionFeatureCount": int(schema["actionFeatureCount"]),
            "heuristicBlend": float(policy["heuristicBlend"]),
            "bias": float(policy["bias"]),
            "weights": [round(float(value), 8) for value in policy["weights"]],
            "training": {
                "preset": preset,
                "exportedAt": int(time.time()),
                "metrics": policy["metrics"],
            },
        }
        path = export_dir / f"{difficulty.lower()}.policy.json"
        path.write_text(json.dumps(payload, indent=2), encoding="utf-8")
        print(f"[train-ai] exported {path}")


def write_report(policies: dict[str, dict], schema: dict, fingerprint: str, preset: str) -> None:
    run_dir = ROOT / "ai_runs" / time.strftime("%Y%m%d-%H%M%S")
    run_dir.mkdir(parents=True, exist_ok=True)
    report = {
        "preset": preset,
        "rulesFingerprint": fingerprint,
        "schema": schema,
        "policies": {difficulty: policy["metrics"] for difficulty, policy in policies.items()},
    }
    (run_dir / "report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"[train-ai] wrote {run_dir / 'report.json'}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Train and export AutoChess AI policies.")
    parser.add_argument("--preset", choices=["smoke", "full"], default="smoke")
    parser.add_argument("--export", type=Path, default=ROOT / "assets" / "ai")
    parser.add_argument("--episodes", type=int, default=None)
    parser.add_argument("--hours", type=float, default=None,
                        help="Wall-clock training limit for full mode, for example --hours 8.")
    parser.add_argument("--device", default="cpu")
    parser.add_argument("--checkpoint-every", type=int, default=100,
                        help="Write a full-mode checkpoint every N episodes.")
    parser.add_argument("--allow-weak", action="store_true")
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--no-venv", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if not args.no_venv:
        ensure_venv()

    cmake = find_cmake()
    if not args.skip_build:
        build_extension(cmake)

    env = import_env_module()
    env.reset(seed=1, max_steps=160)
    schema = env.feature_schema()
    fingerprint = env.rules_fingerprint()
    print(f"[train-ai] rules fingerprint: {fingerprint}")
    print(f"[train-ai] state/action features: {schema['stateFeatureCount']}/{schema['actionFeatureCount']}")

    episodes = args.episodes
    if episodes is None:
        episodes = 2 if args.preset == "smoke" else (None if args.hours else 96)

    if args.preset == "full":
        checkpoint_dir = ROOT / "ai_runs" / time.strftime("full-%Y%m%d-%H%M%S")
        policies = full_train(
            env,
            schema,
            episodes=episodes,
            seed=7,
            device=args.device,
            hours=args.hours,
            checkpoint_every=args.checkpoint_every,
            checkpoint_dir=checkpoint_dir,
        )
    else:
        policies = smoke_train(env, schema, episodes=episodes, seed=7)

    superhard = policies["SuperHard"]["metrics"]
    if args.preset == "full" and not args.allow_weak and superhard["totalReward"] <= 0:
        raise RuntimeError(
            "SuperHard policy did not beat the heuristic evaluation gate. "
            "Continue training with more episodes or rerun with --allow-weak."
        )

    export_dir = args.export
    if not export_dir.is_absolute():
        export_dir = ROOT / export_dir
    export_policies(export_dir, env, schema, policies, args.preset)
    write_report(policies, schema, fingerprint, args.preset)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
