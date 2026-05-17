#!/usr/bin/env bash
# Headless training entry point for Paratera (or any Linux GPU instance).
#
# Usage:
#   tools/run_cloud.sh                      # cloud preset (default: 60h, 8 workers)
#   tools/run_cloud.sh short                # smaller preset
#   tools/run_cloud.sh cloud --hours 6      # cap wall clock
#   tools/run_cloud.sh cloud --resume ai_runs/cloud
#
# Note: cloud preset writes replay checkpoints as compressed npz (~500MB
# at 200k samples), not multi-GB JSON, so a 40GB system disk is sufficient.
#
# Assumptions on the instance:
#   - Ubuntu 22.04 or similar
#   - CUDA driver installed (any modern NVIDIA wheel will pick the matching toolkit)
#   - g++ / cmake / python3.11+ available
#   - The repo is checked out into the current directory (this script lives in tools/)
#
# What this does:
#   1. Installs requirements-train.txt into .cache/ai-venv if torch is not already importable.
#   2. Configures + builds the training preset for Python extension.
#   3. Runs ctest gating tests (autochess_tests).
#   4. Invokes tools/train_ai.py with the chosen preset.
#   5. Final policy files land in assets/ai/{hard,superhard}.policy.json.
#
# To pull the trained artifacts back to your laptop after the job:
#   scp -r user@<instance>:<repo>/assets/ai ./assets/ai

set -euo pipefail

PRESET="${1:-cloud}"
shift || true

REPO_ROOT="$(cd "$(dirname "$0")"/.. && pwd)"
cd "$REPO_ROOT"

echo "[cloud] repo: $REPO_ROOT"
echo "[cloud] preset: $PRESET"
echo "[cloud] extra args: $*"

# Fail fast if no GPU - cloud preset really needs one.
if ! command -v nvidia-smi >/dev/null 2>&1; then
    echo "[cloud] WARNING: nvidia-smi not found; preset=$PRESET assumes a GPU instance."
fi
nvidia-smi || true

# Run the orchestrator. It bootstraps a venv on first run if needed.
python3 tools/train_ai.py --preset "$PRESET" "$@"

echo "[cloud] done. Final policy files:"
ls -la assets/ai/*.policy.json || true
