# AI policy packages

Normal difficulty uses a built-in scripted strategy and does not load a policy
file. Run one of these from the project root to export trained Hard and Super
Hard policies:

```powershell
python tools/train_ai.py --preset smoke --export assets/ai
python tools/train_ai.py --preset full --export assets/ai
```

The game loads `hard.policy.json` and `superhard.policy.json` from this
directory. If a trained file is missing, damaged, or trained against stale
rules, the core engine falls back to the built-in heuristic AI and writes the
reason to the event log.
