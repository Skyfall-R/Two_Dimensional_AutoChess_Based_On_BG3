# Two Dimensional AutoChess

Two Dimensional AutoChess is a C++ / raylib auto-battler prototype built around
a two-stage match loop. Instead of sending armies down one lane immediately, the
game opens with a shared full-board exploration phase where both sides develop
their economy, fight neutral encounters, trigger hidden events, and contest
boss rooms. After exploration ends, the game switches to a cleaner main-battle
map for the final tower-pushing fight.

The project is still a prototype, but the core loop, GUI, tests, BG3-inspired
unit rules, and trainable AI pipeline are all in the repository.

## Highlights

- Two-stage game flow:
  - Stage 1: Exploration on a shared 33x19 dungeon map.
  - Stage 2: Main Battle on a direct tower-push map.
- Two randomized Stage 1 map templates:
  - underground ruin style with central boss rooms and outer loops;
  - Underdark camp-network style with diagonal routes and edge passages.
- Controlled exploration-objective placement:
  - each run starts from authored objective anchors;
  - objectives get small per-run position variation;
  - bosses stay in the middle, regular camps stay toward the outer areas;
  - objectives avoid deployment zones and keep spacing so monsters do not clump.
- BG3-inspired combat language:
  - damage formulas such as `1d6 + 7 Piercing`;
  - backend dice rolling;
  - damage types, resistance, vulnerability, and immunity;
  - attack rolls, Armor Class, saving throws, and status effects.
- Exploration objectives:
  - camps, elite camps, bosses, traps, hidden gold caches, and hidden healing
    springs;
  - hidden events look like normal floor tiles until triggered;
  - cleared bosses and objectives use unified cleared markers.
- Neutral monster behavior:
  - ordinary neutral monsters and bosses start dormant;
  - they activate only after hostile action affects them;
  - activated guardians pursue within a leash instead of chasing across the map;
  - Redcap traps spawn active ambushers with their own short leash.
- Permanent-death economy:
  - ordinary purchased units do not automatically revive after a round;
  - surviving Stage 1 units are converted into gold before Stage 2;
  - dead units refund nothing.
- Board movement rules:
  - ground units cannot stack;
  - flying units can overlap ground and other flying units;
  - knockback uses domino-style push resolution for ground units.
- Relic and roster systems:
  - relic drafts can expand to extra choices;
  - roster capacity counts bench plus deployed units;
  - relics can alter economy, roster space, summons, and reward patterns.
- Raylib GUI:
  - shop, bench, deployment, unit details, relic draft, log, combat cues, and
    BG3-style icon assets;
  - larger readable typography and compact status layout.
- AI support:
  - built-in Normal AI;
  - Hard / Super policy packages;
  - optional AlphaZero-style training pipeline and Python environment binding.

## How A Match Plays

### 1. Stage 1: Exploration

The match starts in `Stage 1: Exploration + Preparation`.

The player buys units, deploys them in the left-side staging area, and selects
an exploration-round limit from `2 / 4 / 6 / 8 / 10`. The AI deploys from the
opposite side. When both sides are ready, units explore automatically.

Exploration is not a route-clicking minigame. Both armies move on the same
shared 33x19 board and evaluate objectives by reward, distance, risk, health,
and enemy contest pressure. They can fight each other while also fighting the
map.

Visible objectives include:

- regular neutral camps;
- elite camps;
- boss encounters;
- Redcap trap sites.

Hidden events include:

- random gold caches;
- hidden healing springs.

Hidden events are not visible, not searchable, and do not count toward the
objective total. They trigger only when a non-neutral player or AI unit steps on
the tile during normal movement.

Exploration ends when the selected round limit is reached or all visible
objectives are cleared.

### 2. Stage 1 Economy Conversion

Stage 1 units do not directly walk into Stage 2.

At transition time:

- surviving ordinary purchased units refund gold based on value and current HP;
- dead ordinary units refund `0`;
- neutral monsters, temporary summons, towers, and internal units do not refund;
- gold, relics, and persistent run modifiers remain.

This makes Stage 1 a development race rather than a disposable warmup.

### 3. Stage 2: Main Battle

The game switches to `Stage 2: Main Battle + Preparation`.

Stage 2 uses a separate main-battle template with bases, towers, and a direct
lane. Stage 1 camps, traps, hidden events, and boss markers are cleared. The
player spends the exploration economy on a fresh main-battle army, deploys, and
then fights the final tower-pushing battle.

Stage 2 victory is decided by the main-battle rules: destroying defenses,
reaching the enemy base, or eliminating active combat units depending on the
current state.

## Units And Bosses

Playable units include D&D / BG3-flavored roles such as:

- Skeleton Mob;
- Goblin Ambusher;
- Githyanki Warrior;
- Elven Ranger;
- Imp Swarm;
- Grave Necromancer;
- Fire Mephit;
- Life Cleric;
- Shield Guardian;
- Evoker;
- Shadow Rogue;
- Circle Druid;
- Dragon Wyrmling;
- Berserker;
- Oathbound Paladin.

Exploration monsters and bosses include:

- Spectator Raycaster;
- Owlbear Matriarch;
- Mind Flayer Arcanist;
- Sovereign Spaw;
- Kar'niss Drider;
- Water Myrmidon;
- Phase Spider Matriarch;
- Raphael;
- Ketheric Thorm;
- Moonlight Sliver;
- Guardian of Faith;
- Minotaur;
- Death Knight;
- Air Myrmidon;
- Tamia Holzt;
- Redcap Ambushers from traps.

Bosses are unique per run and are kept away from the edges. Regular monsters
can repeat, but objective generation is constrained so the map keeps a readable
structure.

## Combat Model

The combat system is a simplified BG3-inspired layer on top of auto-chess
pacing.

Examples:

```text
Damage: 8-13
1d6 + 7 Piercing

Damage: 16-72
4d8 + 4 Bludgeoning + 4d8 + 4 Bludgeoning

Damage: 5-30
5d6 Radiant
```

The UI shows the formula and expected range. The backend rolls dice and applies
type affinity:

- resistant damage is halved;
- vulnerable damage is doubled;
- immune damage is negated.

Statuses and effects include stun, prone, frightened, chilled, poisoned,
blinded, staggered, slow, shield, summon, dominate, knockback, and limited
ambush/leap behavior.

## Controls

- Click a shop card or press number keys to buy a unit.
- Drag a bench unit to a highlighted deployment cell.
- Drag a deployed unit during preparation to reposition it.
- Drag a deployed unit back to the bench to undeploy it.
- Right-click or press `Esc` to cancel dragging.
- During preparation, choose `Normal`, `Hard`, or `Super`.
- Click the main action button to ready / switch view / pick relic depending on
  the current state.

## Build And Run

The repository uses CMake presets and Visual Studio 2022 on Windows.

```powershell
cmake --preset vscode-debug
cmake --build --preset vscode-debug --target autochess_gui
.\build\vscode-vs-debug\Debug\autochess_gui.exe
```

Run tests:

```powershell
cmake --build --preset vscode-debug --target autochess_tests
ctest --preset vscode-debug --output-on-failure
```

Main targets:

- `autochess_core`: game rules, maps, AI, combat, events.
- `autochess_gui`: raylib GUI.
- `autochess_cli`: text debug entry point.
- `autochess_tests`: assert-based regression tests.
- `autochess_env`: optional Python extension for training and headless AI.

## AI Training

Normal difficulty uses a built-in strategy. Hard and Super load policy packages:

- `assets/ai/hard.policy.json`
- `assets/ai/superhard.policy.json`

The training pipeline lives under `tools/training/alphazero/` and is driven by:

```powershell
python tools/train_ai.py --preset smoke
python tools/train_ai.py --preset short
python tools/train_ai.py --preset full
```

The C++ engine fingerprints the rules. If a policy is stale or missing, it
falls back to the built-in heuristic rather than silently using incompatible
weights.

## Project Status

This is an active prototype. The current focus is:

- making Stage 1 exploration feel dense, readable, and fair;
- tightening UI readability and icon quality;
- expanding unit, boss, relic, and event variety;
- improving AI movement, target selection, and performance;
- preserving robust regression coverage for death rules, stage transitions,
  map generation, knockback, damage types, and neutral activation.
