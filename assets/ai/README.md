# AI policy packages

Normal difficulty uses a built-in scripted strategy and does not load a policy
file. The other two difficulties come from the AlphaZero-style training
pipeline:

- **Hard** ships as `modelVersion: linear-v2` (a ridge-regression distillation
  of an opponent-pool mid-checkpoint, so it plays noticeably below SuperHard).
- **Super Hard** ships as `modelVersion: mlp-v3` (the full trained neural
  network, evaluated in-game by `MlpAiPlanner` with no information loss).

Run one of these from the project root to export trained policy packages:

```powershell
python tools/train_ai.py --preset smoke --export assets/ai
python tools/train_ai.py --preset full --export assets/ai
```

The game loads `hard.policy.json` and `superhard.policy.json` from this
directory. If a trained file is missing, damaged, or trained against stale
rules, the core engine falls back to the built-in heuristic AI and writes the
reason to the event log.

`Difficult` is parsed as the same difficulty tier as `Hard`; the file name
stays `hard.policy.json` for compatibility.

The training replay buffer stores variable-length action lists and pads only
inside each minibatch. This keeps Exploration-mode states with many deployed
units from wasting memory on absent action slots; the `full` preset therefore
uses a 60,000-sample replay cap by default, and the `cloud` preset uses
200,000 with compressed-npz checkpoints.

The training orchestrator gates the final export on arena performance: if
`win_rate vs Normal` is below `arena.accept_threshold` (default 0.55), the
new artifacts are written to `<checkpoint_dir>/rejected/` rather than
overwriting `assets/ai/*.policy.json`, so a weak run cannot silently replace
a known-good policy.
