# AI policy packages

Run one of these from the project root to export difficulty policies:

```powershell
python tools/train_ai.py --preset smoke --export assets/ai
python tools/train_ai.py --preset full --export assets/ai
```

The game loads `normal.policy.json`, `hard.policy.json`, and
`superhard.policy.json` from this directory. If a file is missing, damaged, or
trained against stale rules, the core engine falls back to the built-in
heuristic AI and writes the reason to the event log.
