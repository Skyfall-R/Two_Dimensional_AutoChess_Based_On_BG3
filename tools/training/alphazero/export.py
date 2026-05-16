"""Export trained PyTorch networks to JSON policy files consumable by C++.

Hard uses modelVersion="linear-v2" (one weight per state-feature + per
action-feature). We bridge by *distilling* the trained neural net into a
linear scorer:

    1. Sample many states by walking the env with random legal actions.
    2. For each state, enumerate legal actions and ask the NN for its policy
       logits over the action set.
    3. Treat (state || action_features) as the feature vector and the NN
       logit as the regression target. Solve a ridge regression to obtain a
       linear weight vector w = [w_state ; w_action] of length F_s + F_a that
       reproduces the NN's preferences as best a linear model can.
    4. Write the weights into hard.policy.json with the
       current rulesFingerprint baked in.

SuperHard uses modelVersion="mlp-v3" and serializes the full PolicyValueNet
action scorer so the game can run the trained network directly.
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
LINEAR_MODEL_VERSION = "linear-v2"  # Must match kPolicyModelVersion in lib.cpp
MLP_MODEL_VERSION = "mlp-v3"         # Must match kPolicyMlpModelVersion in lib.cpp


def _layer_arrays(linear: "torch.nn.Linear", prefix: str, index: int) -> dict:
    """Pack a torch.nn.Linear's weights into the flat row-major JSON layout
    that the C++ MlpAiPlanner expects: `{prefix}W_{i}` flat array of size
    in*out, `{prefix}B_{i}` flat array of size out.
    """
    w = linear.weight.detach().cpu().numpy()
    b = linear.bias.detach().cpu().numpy()
    return {
        f"{prefix}W_{index}": [round(float(v), 8) for v in w.flatten().tolist()],
        f"{prefix}B_{index}": [round(float(v), 8) for v in b.flatten().tolist()],
    }


def write_mlp_policy(
    path: Path,
    difficulty: str,
    network,
    state_dim: int,
    action_dim: int,
    rules_fingerprint: str,
    metrics: dict | None = None,
) -> None:
    """Serialize a PolicyValueNet to mlp-v3 JSON format consumed by C++.

    Structure (must match parser in GameEngine::loadAiPlanner):
      hiddenSizes        list of state-trunk layer widths (post-activation)
      actionHiddenSizes  list of action-head layer widths (post-activation)
      stateW_<i>/stateB_<i>   each layer's weights (out*in flat) + biases
      headW_<i>/headB_<i>     same for action head
      actionLogitW/actionLogitB  final scalar head
    """
    import torch.nn as nn

    # Walk the state trunk: assumes Sequential(Linear, ReLU, Linear, ReLU, ...).
    state_layers = [m for m in network.state_trunk if isinstance(m, nn.Linear)]
    action_layers = [m for m in network.action_head if isinstance(m, nn.Linear)]
    logit_layer = network.action_logit
    payload: dict = {
        "format": POLICY_FORMAT,
        "modelVersion": MLP_MODEL_VERSION,
        "difficulty": difficulty,
        "rulesFingerprint": rules_fingerprint,
        "stateFeatureCount": state_dim,
        "actionFeatureCount": action_dim,
        "heuristicBlend": 0.0,
        "bias": 0.0,
        "hiddenSizes": [int(layer.out_features) for layer in state_layers],
        "actionHiddenSizes": [int(layer.out_features) for layer in action_layers],
    }
    for i, layer in enumerate(state_layers):
        payload.update(_layer_arrays(layer, "state", i))
    for i, layer in enumerate(action_layers):
        payload.update(_layer_arrays(layer, "head", i))
    payload["actionLogitW"] = [
        round(float(v), 8) for v in logit_layer.weight.detach().cpu().numpy().flatten().tolist()
    ]
    payload["actionLogitB"] = [
        round(float(v), 8) for v in logit_layer.bias.detach().cpu().numpy().flatten().tolist()
    ]
    if metrics:
        payload["training"] = {
            "exportedAt": int(time.time()),
            "metrics": metrics,
        }
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2), encoding="utf-8")


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
    network,
    device,
    state_dim: int,
    action_dim: int,
    rules_fingerprint: str,
    export_dir: Path,
    metrics: dict | None = None,
    weak_network=None,
    seed: int = 0,
) -> list[Path]:
    """Export Hard (linear-v2 distilled) and SuperHard (mlp-v3 full network).

    SuperHard ships the full neural net so the in-game AI uses it at full
    strength via the C++ MlpAiPlanner. Hard uses a distilled linear policy
    from either an opponent-pool checkpoint or a scale+noise variant of the
    strong weights, giving a meaningful difficulty gap.
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

    written: list[Path] = []

    # SuperHard: full mlp-v3 network (no information loss).
    super_path = export_dir / "superhard.policy.json"
    per_metrics_super = dict(metrics) if metrics else {}
    per_metrics_super["exportedAs"] = "mlp-v3 (full network)"
    write_mlp_policy(
        path=super_path,
        difficulty="SuperHard",
        network=network,
        state_dim=state_dim,
        action_dim=action_dim,
        rules_fingerprint=rules_fingerprint,
        metrics=per_metrics_super,
    )
    written.append(super_path)

    # Hard: distilled linear-v2 from the weaker source.
    hard_path = export_dir / "hard.policy.json"
    per_metrics_hard = dict(metrics) if metrics else {}
    per_metrics_hard["distilledFrom"] = hard_source
    per_metrics_hard["exportedAs"] = "linear-v2 (distilled)"
    write_linear_policy(
        path=hard_path,
        difficulty="Hard",
        weights=weak_weights,
        bias=weak_bias,
        state_dim=state_dim,
        action_dim=action_dim,
        rules_fingerprint=rules_fingerprint,
        heuristic_blend=hard_blend,
        metrics=per_metrics_hard,
    )
    written.append(hard_path)

    return written
