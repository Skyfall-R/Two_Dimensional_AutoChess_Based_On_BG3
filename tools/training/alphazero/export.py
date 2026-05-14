"""Export trained PyTorch network to a JSON policy file consumable by the
C++ PolicyAiPlanner.

Currently the C++ side only supports modelVersion="linear-v1" (one weight per
state-feature + per action-feature). We bridge by *distilling* the trained
neural net into a linear scorer:

    1. Sample many states by walking the env with random legal actions.
    2. For each state, enumerate legal actions and ask the NN for its policy
       logits over the action set.
    3. Treat (state || action_features) as the feature vector and the NN
       logit as the regression target. Solve a ridge regression to obtain a
       linear weight vector w = [w_state ; w_action] of length F_s + F_a that
       reproduces the NN's preferences as best a linear model can.
    4. Write the weights into a {Hard,SuperHard}.policy.json file with the
       current rulesFingerprint baked in.
"""

from __future__ import annotations

import json
import time
from pathlib import Path

import numpy as np
import torch

from .encoder import encode
from .network import PolicyValueNet

POLICY_FORMAT = "autochess_policy_v1"
LINEAR_MODEL_VERSION = "linear-v1"


def distill_linear(
    env_module,
    network: PolicyValueNet,
    device: torch.device,
    state_dim: int,
    action_dim: int,
    samples: int = 4000,
    ridge: float = 1e-2,
) -> tuple[np.ndarray, float]:
    """Returns (weights[F_s + F_a], bias).

    Walks the env with random legal actions to collect varied prep states.
    For each (state, legal_actions) pair we ask the NN for its logits over
    the action set, then solve a closed-form ridge regression so the linear
    scorer reproduces the NN's preferences as faithfully as a linear model
    can.
    """
    network.eval()
    rng = np.random.default_rng(0)
    handle = env_module.env_create(seed=42, max_steps=2000, difficulty="Hard")
    feature_rows: list[np.ndarray] = []
    target_logits: list[float] = []
    try:
        while len(feature_rows) < samples:
            obs = env_module.env_observation(handle)
            if obs.get("done"):
                env_module.env_close(handle)
                handle = env_module.env_create(
                    seed=int(rng.integers(0, 2**30)),
                    max_steps=2000,
                    difficulty="Hard",
                )
                continue
            if obs.get("phase") != "Preparation":
                env_module.env_finalize_round(handle)
                continue
            state_feats = env_module.env_state_features(handle)
            legal = env_module.env_legal_actions(handle)
            if not legal:
                break
            encoded = encode(state_feats, legal)
            state_t = torch.from_numpy(encoded.state).unsqueeze(0).to(device)
            action_t = torch.from_numpy(encoded.action_features).unsqueeze(0).to(device)
            mask_t = torch.zeros(1, encoded.legal_count, dtype=torch.bool, device=device)
            mask_t[0, :] = True
            with torch.no_grad():
                logits, _ = network(state_t, action_t, mask_t)
            logits = logits[0, : encoded.legal_count].cpu().numpy()
            for slot in range(encoded.legal_count):
                feature_rows.append(
                    np.concatenate([encoded.state, encoded.action_features[slot]])
                )
                target_logits.append(float(logits[slot]))
                if len(feature_rows) >= samples:
                    break
            engine_idx = int(legal[rng.integers(0, len(legal))]["index"])
            env_module.env_step(handle, engine_idx)
    finally:
        env_module.env_close(handle)

    if not feature_rows:
        return np.zeros(state_dim + action_dim, dtype=np.float32), 0.0

    # Use torch for the ridge solve. On Windows, mixing PyTorch and NumPy's
    # MKL-backed linalg can initialize duplicate OpenMP runtimes in one
    # process; torch is already loaded for inference, so keep the solve there.
    X_np = np.stack(feature_rows).astype(np.float32)
    y_np = np.asarray(target_logits, dtype=np.float32)
    solve_device = device if device.type == "cuda" else torch.device("cpu")
    X = torch.from_numpy(X_np).to(solve_device)
    y = torch.from_numpy(y_np).to(solve_device)
    n_feat = X.shape[1]
    eye = torch.eye(n_feat, dtype=X.dtype, device=solve_device)
    A = X.T @ X + ridge * eye
    b = X.T @ y
    w_t = torch.linalg.solve(A, b)
    bias_t = y.mean() - X.mean(dim=0) @ w_t
    w = w_t.detach().cpu().numpy().astype(np.float32)
    bias = float(bias_t.detach().cpu())
    return w, bias


def write_linear_policy(
    path: Path,
    difficulty: str,
    weights: np.ndarray,
    bias: float,
    state_dim: int,
    action_dim: int,
    rules_fingerprint: str,
    heuristic_blend: float,
    metrics: dict | None = None,
) -> None:
    payload = {
        "format": POLICY_FORMAT,
        "modelVersion": LINEAR_MODEL_VERSION,
        "difficulty": difficulty,
        "rulesFingerprint": rules_fingerprint,
        "stateFeatureCount": state_dim,
        "actionFeatureCount": action_dim,
        "heuristicBlend": float(heuristic_blend),
        "bias": float(bias),
        "weights": [round(float(v), 8) for v in weights.tolist()],
    }
    if metrics:
        payload["training"] = {
            "exportedAt": int(time.time()),
            "metrics": metrics,
        }
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2), encoding="utf-8")


def export_policies(
    env_module,
    network: PolicyValueNet,
    device: torch.device,
    state_dim: int,
    action_dim: int,
    rules_fingerprint: str,
    export_dir: Path,
    metrics: dict | None = None,
    weak_network: PolicyValueNet | None = None,
    seed: int = 0,
) -> list[Path]:
    """Distill the trained network into two distinct linear policies.

    SuperHard is distilled from `network` (latest / strongest snapshot).
    Hard is distilled from `weak_network` if provided (typically an earlier
    opponent-pool checkpoint); otherwise we synthesize a softer policy from
    the strong weights with scaling + Gaussian noise + heuristic blend.
    """
    rng = np.random.default_rng(seed)

    strong_weights, strong_bias = distill_linear(
        env_module=env_module,
        network=network,
        device=device,
        state_dim=state_dim,
        action_dim=action_dim,
        samples=4000,
    )

    if weak_network is not None:
        weak_weights, weak_bias = distill_linear(
            env_module=env_module,
            network=weak_network,
            device=device,
            state_dim=state_dim,
            action_dim=action_dim,
            samples=4000,
        )
        hard_blend = 0.0
        hard_source = "opponent-pool checkpoint"
    else:
        scale = 0.55
        noise_sigma = 0.02
        weak_weights = (
            strong_weights * scale
            + rng.normal(0.0, noise_sigma, size=strong_weights.shape)
        ).astype(np.float32)
        weak_bias = float(strong_bias * scale)
        hard_blend = 0.5
        hard_source = "scaled+noise from strong weights"

    profiles = {
        "Hard": {
            "weights": weak_weights,
            "bias": weak_bias,
            "blend": hard_blend,
            "source": hard_source,
        },
        "SuperHard": {
            "weights": strong_weights,
            "bias": strong_bias,
            "blend": 0.0,
            "source": "trained network (latest)",
        },
    }

    written: list[Path] = []
    for difficulty, profile in profiles.items():
        path = export_dir / f"{difficulty.lower()}.policy.json"
        per_metrics = dict(metrics) if metrics else {}
        per_metrics["distilledFrom"] = profile["source"]
        write_linear_policy(
            path=path,
            difficulty=difficulty,
            weights=profile["weights"],
            bias=profile["bias"],
            state_dim=state_dim,
            action_dim=action_dim,
            rules_fingerprint=rules_fingerprint,
            heuristic_blend=profile["blend"],
            metrics=per_metrics,
        )
        written.append(path)
    return written
