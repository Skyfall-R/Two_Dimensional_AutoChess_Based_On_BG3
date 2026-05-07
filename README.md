# AutoChess 2D

This project is a 2D C++ auto-chess prototype. The old one-dimensional lane
logic has been replaced by a reusable core engine, a debug CLI, a raylib GUI,
and a small test executable.

## Targets

- `autochess_core`: game rules, 11x7 board, targeting, movement, AI, events.
- `autochess_cli`: text debug entry point for quick simulation checks.
- `autochess_gui`: raylib 2D interface with shop, bench, drag-and-drop deploy,
  start button, health bars, and event log.
- `autochess_tests`: assert-based core tests for deployment, movement,
  targeting, summoning, round reset, and no-overlap behavior.
- `autochess_env`: optional Python extension for headless AI training when
  `AUTOCHESS_BUILD_PYTHON=ON`.

## VSCode Workflow

Open this folder in VSCode:

```text
E:\Cowork\自走棋\AutoChess
```

Then use one of these:

- `Terminal > Run Task > CMake: Build`
- `Terminal > Run Task > Run AutoChess GUI`
- `Run and Debug > Debug AutoChess GUI`

The VSCode tasks use `CMakePresets.json` with the `Visual Studio 17 2022`
generator. You do not need to open the Visual Studio IDE; CMake only uses the
MSVC compiler toolchain. Build output is written inside the project under
`build/vscode-vs-debug`, which is ignored by git.

## Command Line Build

Configure, build, and test the default VSCode preset:

```powershell
cmake --preset vscode-debug
cmake --build --preset vscode-debug
ctest --preset vscode-debug
```

Run the GUI after building:

```powershell
.\build\vscode-vs-debug\Debug\autochess_gui.exe
```

If raylib is not installed, CMake uses `FetchContent` to download raylib 5.0 on
the first configure. The project downloads the raylib release zip instead of
using a Git submodule.

## Trainable AI

The single-player AI supports three difficulty policy packages:

- `assets/ai/normal.policy.json`
- `assets/ai/hard.policy.json`
- `assets/ai/superhard.policy.json`

If a package is missing or was trained against older rules, the game logs the
reason and falls back to the built-in heuristic AI. To rebuild policies after
changing rules or units, run:

```powershell
python tools/train_ai.py --preset smoke --export assets/ai
python tools/train_ai.py --preset full --export assets/ai
```

Smoke mode is a fast build/export check. Full mode uses PyTorch when available;
on Python versions without compatible PyTorch wheels, use smoke mode or create
a training environment with a supported Python version.

## GUI Controls

- Click a shop card, or press number keys `1` through `9`, to buy a unit.
- Drag a unit from the bench to a highlighted Player1 deployment cell.
- Drag a deployed unit during preparation to reposition it.
- Drag a deployed unit back to the bench to undeploy it.
- Right-click or press `Esc` to cancel a drag.
- Click `Normal`, `Hard`, or `Super Hard` during preparation to switch
  the AI policy package.
- Click `Start` to let the AI finish preparation and start combat.
