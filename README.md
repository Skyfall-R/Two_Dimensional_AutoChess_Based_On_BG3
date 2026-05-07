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

## GUI Controls

- Click a shop card, or press number keys `1` through `9`, to buy a unit.
- Drag a unit from the bench to a highlighted Player1 deployment cell.
- Drag a deployed unit during preparation to reposition it.
- Drag a deployed unit back to the bench to undeploy it.
- Right-click or press `Esc` to cancel a drag.
- Click `Start` to let the AI finish preparation and start combat.
