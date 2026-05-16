# Two Dimensional AutoChess

Two Dimensional AutoChess is a C++ / raylib BG3-flavored roguelike
auto-battler prototype. A match is a single dungeon run on a shared 33x19 map:
both sides buy and deploy units, then their armies explore automatically,
clear neutral objectives, uncover hidden events, fight bosses, collect relics,
and race for the best run score.

The project is still a prototype, but the core loop, GUI, tests,
BG3-inspired unit rules, relic system, and trainable AI pipeline are all in the
repository.

## Highlights

- Single exploration-run game flow:
  - one continuous 33x19 dungeon board;
  - combat, economy, relics, and scoring all resolve inside the dungeon run;
  - the run ends when the selected round limit is reached or all visible
    objectives are cleared.
- Two randomized dungeon map templates:
  - underground ruin style with central boss rooms and outer loops;
  - Underdark camp-network style with diagonal passages and edge paths.
- Controlled exploration-objective placement:
  - each run starts from authored objective anchors;
  - objectives get small per-run position variation;
  - bosses stay in the middle, regular camps stay toward the outer areas;
  - objectives avoid deployment zones and keep spacing so monsters do not clump.
- Exploration scoring:
  - camps, elites, bosses, traps, and hidden gold caches contribute run score;
  - visible objective score uses `rewardGold + rewardQuality * 4`;
  - hidden gold adds score equal to the gold found;
  - the higher score wins when the run ends.
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
- Permanent-death run economy:
  - ordinary purchased units do not automatically revive after a round;
  - dead units refund nothing;
  - gold, relics, and run modifiers stay within the current run.
- Board movement rules:
  - ground units cannot stack;
  - flying units can overlap ground and other flying units;
  - knockback uses domino-style push resolution for ground units.
- Relic and roster systems:
  - relic drafts can expand to extra choices;
  - roster capacity counts bench plus deployed units;
  - relics can alter economy, roster space, summons, family bias, and reward
    patterns.
- Raylib GUI:
  - shop, bench, deployment, unit details, relic draft, log, combat cues, and
    BG3-style icon assets;
  - larger readable typography and compact run-status layout.
- AI support:
  - built-in Normal AI with scripted roster and formation rules;
  - Difficult / Super policy packages;
  - optional AlphaZero-style training pipeline and Python environment binding.

## How A Run Plays

The match starts in `Exploration Run + Preparation`.

The player buys units, deploys them in the left-side staging area, and selects
an exploration-round limit from `2 / 4 / 6 / 8 / 10`. The AI deploys from the
opposite side. When both sides are ready, units explore automatically.

Both armies move on the same shared 33x19 board and evaluate objectives by
reward, distance, risk, health, and enemy contest pressure. They can fight each
other while also fighting the map.

Visible objectives include:

- regular neutral camps;
- elite camps;
- boss encounters;
- Redcap trap sites.

Hidden events include:

- random gold caches;
- hidden healing springs.

Hidden events are not visible, not searchable, and do not count toward the
visible objective total. They trigger only when a non-neutral player or AI unit
steps on the tile during normal movement.

The run ends when the selected round limit is reached or all visible objectives
are cleared.

## Run Scoring

Clearing visible objectives grants both gold and exploration score. The default
score formula is:

```text
rewardGold + rewardQuality * 4
```

Bosses and elites naturally matter more because their authored rewards are
higher. Hidden gold caches grant gold and also add the same amount to the
finder's exploration score.

When the run finishes, winner selection is:

1. Higher exploration score.
2. Higher current gold.
3. More bosses cleared.
4. Higher total threat among surviving non-internal units.
5. If everything is still tied, the run finishes with no winner.

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
- Click the main action button to ready or pick a relic depending on the
  current state.

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

Normal difficulty uses a built-in scripted strategy. Difficult and Super load
AlphaZero-style distilled policy packages:

- `assets/ai/hard.policy.json`
- `assets/ai/superhard.policy.json`

The training pipeline lives under `tools/training/alphazero/` and is driven by:

```powershell
python tools/train_ai.py --preset smoke    # ~1 min sanity check (CPU OK)
python tools/train_ai.py --preset short    # ~30 min, single GPU recommended
python tools/train_ai.py --preset full     # 8h+ with 8 parallel workers
python tools/train_ai.py --preset cloud    # 20h+, 16 workers, big net (cloud GPU)
```

The C++ engine fingerprints the rules. If a policy is stale or missing, it
falls back to the built-in heuristic rather than silently using incompatible
weights. Re-running `train_ai.py` after editing unit specs / economy / map
templates auto-detects the fingerprint mismatch and starts a fresh run.

Architecture: PUCT MCTS self-play over preparation actions, policy + value
neural network, true multi-process self-play workers (one C++ engine per
worker, NN inference on CPU), opponent pool with PFSP sampling, arena gate vs
the scripted Normal AI.

Export: SuperHard ships as `modelVersion: mlp-v3` so the in-game C++ AI runs
the full neural network at game time (no information loss). Hard ships as
`modelVersion: linear-v2` distilled from an opponent-pool mid-checkpoint, so
the two difficulties have a real strength gap.

### Cloud (Paratera) workflow

For "super strong" runs on a Paratera GPU instance:

```bash
# On the instance after `git clone`:
chmod +x tools/run_cloud.sh
tools/run_cloud.sh cloud                   # 20h cloud preset, hidden=[512,512,256], sims=256
tools/run_cloud.sh cloud --hours 6         # cap wall clock
tools/run_cloud.sh cloud --resume ai_runs/cloud   # resume an interrupted run
```

Recommended instance: any single GPU (A100 / V100 / 3090 / 4090 / L40) plus at
least 16 vCPUs (workers default to 16). The bottleneck is C++ env throughput,
not GPU FLOPs - small NN inference on CPU within workers is faster than
shuffling tensors to GPU every MCTS leaf.

After the job, pull artifacts back with `scp -r user@host:repo/assets/ai .`.

## Project Status

This is an active prototype. The current focus is:

- making the dungeon run dense, readable, and fair;
- tightening UI readability and icon quality;
- expanding unit, boss, relic, event, and map-template variety;
- improving AI movement, target selection, and performance;
- preserving robust regression coverage for scoring, death rules, map
  generation, knockback, damage types, hidden events, and neutral activation.
