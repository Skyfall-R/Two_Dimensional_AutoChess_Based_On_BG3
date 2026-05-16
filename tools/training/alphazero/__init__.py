"""AutoChess AlphaZero training package.

The pipeline lives in tools/training/alphazero and is orchestrated by
tools/train_ai.py (or tools/training/train.py for direct invocation).

Modules:
    config       - Hyperparameter configs / presets.
    encoder      - State / action tensorization (pure Python, no torch).
    network      - Policy + value MLP (torch).
    mcts         - PUCT MCTS using env.clone() for branch expansion.
    selfplay     - Multi-process self-play workers producing (s, pi, z) tuples.
    replay       - Rolling replay buffer.
    trainer      - Training loop (cross-entropy + MSE).
    arena        - Evaluation against scripted / previous checkpoints.
    opponent_pool - Frozen opponent snapshots (PFSP sampling).
    export       - Distill trained net into linear-v2 policy JSON for C++.
    utils        - Logging / checkpoint paths.
"""
