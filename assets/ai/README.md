# AI policy packages

Normal difficulty uses a built-in scripted strategy and does not load a policy
file. Difficult/Hard and Super Hard use AlphaZero-style MCTS training distilled
to `linear-v2` policy JSON. Run one of these from the project root to export
trained policy packages:

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
