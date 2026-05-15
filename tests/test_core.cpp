#include <lib.hpp>
#include <campaign.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

using namespace autochess;

namespace {

UnitId firstBenchUnit(const GameEngine& engine, PlayerId player) {
    GameSnapshot snapshot = engine.snapshot();
    const PlayerView& view = snapshot.players[player == PlayerId::One ? 0 : 1];
    assert(!view.bench.empty());
    return view.bench.front();
}

UnitId firstBenchUnitOfType(const GameEngine& engine, PlayerId player, UnitType type) {
    GameSnapshot snapshot = engine.snapshot();
    const PlayerView& view = snapshot.players[player == PlayerId::One ? 0 : 1];
    for (UnitId id : view.bench) {
        for (const UnitView& unit : snapshot.units) {
            if (unit.id == id && unit.type == type) return id;
        }
    }
    assert(false);
    return kInvalidUnitId;
}

const UnitView* findUnit(const GameSnapshot& snapshot, UnitId id) {
    for (const UnitView& unit : snapshot.units) {
        if (unit.id == id) return &unit;
    }
    return nullptr;
}

TerrainKind terrainAtSnapshot(const GameSnapshot& snapshot, Coord coord);

int landOccupantsAt(const GameSnapshot& snapshot, Coord coord) {
    int count = 0;
    for (const UnitView& unit : snapshot.units) {
        if (unit.alive && unit.deployed && unit.layer == UnitLayer::Land && unit.coord == coord) {
            ++count;
        }
    }
    return count;
}

UnitId findUnitIdAt(const GameSnapshot& snapshot, UnitType type, Coord coord) {
    for (const UnitView& unit : snapshot.units) {
        if (unit.type == type && unit.alive && unit.deployed && unit.coord == coord) {
            return unit.id;
        }
    }
    return kInvalidUnitId;
}

bool hasAliveUnitAt(const GameSnapshot& snapshot, Coord coord) {
    for (const UnitView& unit : snapshot.units) {
        if (unit.alive && unit.deployed && unit.coord == coord) return true;
    }
    return false;
}

Coord openCoordNear(const GameSnapshot& snapshot, Coord preferred, int radius = 4) {
    Coord best{-1, -1};
    int bestScore = std::numeric_limits<int>::max();
    for (int y = 0; y < snapshot.height; ++y) {
        for (int x = 0; x < snapshot.width; ++x) {
            Coord coord{x, y};
            if (manhattan(coord, preferred) > radius) continue;
            if (terrainAtSnapshot(snapshot, coord) == TerrainKind::Wall) continue;
            if (hasAliveUnitAt(snapshot, coord)) continue;
            int score = manhattan(coord, preferred) * 10 + std::abs(coord.y - preferred.y);
            if (score < bestScore) {
                bestScore = score;
                best = coord;
            }
        }
    }
    assert(best.x >= 0);
    return best;
}

std::array<Coord, 2> adjacentOpenCoords(const GameSnapshot& snapshot, Coord preferred) {
    std::array<Coord, 2> best{Coord{-1, -1}, Coord{-1, -1}};
    int bestScore = std::numeric_limits<int>::max();
    for (int y = 0; y < snapshot.height; ++y) {
        for (int x = 0; x < snapshot.width; ++x) {
            Coord coord{x, y};
            if (terrainAtSnapshot(snapshot, coord) == TerrainKind::Wall || hasAliveUnitAt(snapshot, coord)) continue;
            const std::array<Coord, 4> neighbors = {
                Coord{x + 1, y},
                Coord{x - 1, y},
                Coord{x, y + 1},
                Coord{x, y - 1}
            };
            for (Coord next : neighbors) {
                if (next.x < 0 || next.x >= snapshot.width ||
                    next.y < 0 || next.y >= snapshot.height) {
                    continue;
                }
                if (terrainAtSnapshot(snapshot, next) == TerrainKind::Wall || hasAliveUnitAt(snapshot, next)) continue;
                int score = manhattan(coord, preferred) * 10 + std::abs(coord.y - preferred.y);
                if (score < bestScore) {
                    bestScore = score;
                    best = {coord, next};
                }
            }
        }
    }
    assert(best[0].x >= 0);
    return best;
}

std::vector<Coord> horizontalOpenRun(const GameSnapshot& snapshot, Coord preferred, int length) {
    assert(length > 0);
    std::vector<Coord> best;
    int bestScore = std::numeric_limits<int>::max();
    for (int y = 0; y < snapshot.height; ++y) {
        for (int x = 0; x + length <= snapshot.width; ++x) {
            std::vector<Coord> run;
            run.reserve(static_cast<size_t>(length));
            bool valid = true;
            for (int offset = 0; offset < length; ++offset) {
                Coord coord{x + offset, y};
                if (terrainAtSnapshot(snapshot, coord) == TerrainKind::Wall || hasAliveUnitAt(snapshot, coord)) {
                    valid = false;
                    break;
                }
                run.push_back(coord);
            }
            if (!valid) continue;
            Coord midpoint{x + length / 2, y};
            int score = manhattan(midpoint, preferred) * 10 + std::abs(y - preferred.y);
            if (score < bestScore) {
                bestScore = score;
                best = std::move(run);
            }
        }
    }
    assert(static_cast<int>(best.size()) == length);
    return best;
}

const UnitView* firstNeutralCampUnit(const GameSnapshot& snapshot, PlayerId side = PlayerId::Two) {
    const UnitView* fallback = nullptr;
    for (const UnitView& unit : snapshot.units) {
        if (!unit.alive || !unit.deployed || !isNeutralMonster(unit.type) ||
            unit.type == UnitType::NeutralRedcap) {
            continue;
        }
        if (!fallback) fallback = &unit;
        if (unit.owner == side) return &unit;
    }
    return fallback;
}

std::vector<UnitId> visibleNeutralObjectiveUnits(const GameSnapshot& snapshot) {
    std::vector<UnitId> result;
    for (const UnitView& unit : snapshot.units) {
        if (!unit.alive || !unit.deployed || !isNeutralMonster(unit.type) ||
            unit.type == UnitType::NeutralRedcap) {
            continue;
        }
        TerrainKind terrain = terrainAtSnapshot(snapshot, unit.coord);
        if (terrain == TerrainKind::NeutralCamp || terrain == TerrainKind::BossSite) {
            result.push_back(unit.id);
        }
    }
    return result;
}

Coord firstNeutralCampCoord(const GameSnapshot& snapshot, PlayerId side = PlayerId::Two) {
    const UnitView* unit = firstNeutralCampUnit(snapshot, side);
    assert(unit);
    return unit->coord;
}

TerrainKind terrainAtSnapshot(const GameSnapshot& snapshot, Coord coord) {
    assert(coord.x >= 0 && coord.x < snapshot.width);
    assert(coord.y >= 0 && coord.y < snapshot.height);
    size_t index = static_cast<size_t>(coord.y * snapshot.width + coord.x);
    assert(index < snapshot.terrain.size());
    return snapshot.terrain[index];
}

bool isPassableTerrain(const GameSnapshot& snapshot, Coord coord) {
    return terrainAtSnapshot(snapshot, coord) != TerrainKind::Wall;
}

std::vector<Coord> terrainCoords(const GameSnapshot& snapshot, TerrainKind terrain) {
    std::vector<Coord> coords;
    for (int y = 0; y < snapshot.height; ++y) {
        for (int x = 0; x < snapshot.width; ++x) {
            Coord coord{x, y};
            if (terrainAtSnapshot(snapshot, coord) == terrain) coords.push_back(coord);
        }
    }
    return coords;
}

bool snapshotReachable(const GameSnapshot& snapshot, Coord start, Coord goal) {
    if (!snapshot.width || !snapshot.height) return false;
    if (start.x < 0 || start.x >= snapshot.width || start.y < 0 || start.y >= snapshot.height) return false;
    if (goal.x < 0 || goal.x >= snapshot.width || goal.y < 0 || goal.y >= snapshot.height) return false;
    if (!isPassableTerrain(snapshot, start) || !isPassableTerrain(snapshot, goal)) return false;

    std::vector<char> seen(static_cast<size_t>(snapshot.width * snapshot.height), 0);
    std::vector<Coord> frontier;
    auto indexFor = [&](Coord c) {
        return static_cast<size_t>(c.y * snapshot.width + c.x);
    };
    seen[indexFor(start)] = 1;
    frontier.push_back(start);

    for (size_t i = 0; i < frontier.size(); ++i) {
        Coord current = frontier[i];
        if (current == goal) return true;
        const std::array<Coord, 4> next = {
            Coord{current.x + 1, current.y},
            Coord{current.x - 1, current.y},
            Coord{current.x, current.y + 1},
            Coord{current.x, current.y - 1}
        };
        for (Coord coord : next) {
            if (coord.x < 0 || coord.x >= snapshot.width || coord.y < 0 || coord.y >= snapshot.height) {
                continue;
            }
            size_t idx = indexFor(coord);
            if (seen[idx] || !isPassableTerrain(snapshot, coord)) continue;
            seen[idx] = 1;
            frontier.push_back(coord);
        }
    }
    return false;
}

Coord p1MainDeploy() {
    return Coord{2, kBoardHeight / 2};
}

Coord p2MainDeploy() {
    return Coord{kBoardWidth - 4, kBoardHeight / 2};
}

Coord legalDeployCoordNear(const GameEngine& engine, PlayerId player, UnitLayer layer, Coord preferred) {
    if (engine.canDeploy(player, preferred, layer)) return preferred;

    Coord best{-1, -1};
    int bestScore = std::numeric_limits<int>::max();
    int minX = player == PlayerId::One ? 0 : kBoardWidth - 4;
    int maxX = player == PlayerId::One ? 3 : kBoardWidth - 1;
    for (int x = minX; x <= maxX; ++x) {
        for (int y = 0; y < kBoardHeight; ++y) {
            Coord candidate{x, y};
            if (!engine.canDeploy(player, candidate, layer)) continue;
            int score = manhattan(candidate, preferred) * 10 + std::abs(y - preferred.y);
            if (score < bestScore) {
                bestScore = score;
                best = candidate;
            }
        }
    }

    assert(best.x >= 0);
    return best;
}

UnitId buyAndDeploy(GameEngine& engine, PlayerId player, UnitType type, Coord coord) {
    assert(engine.buyUnit(player, type));
    UnitId id = firstBenchUnit(engine, player);
    if (!engine.deployUnit(player, id, coord)) {
        GameSnapshot snapshot = engine.snapshot();
        std::cerr << "deploy failed: player=" << toString(player)
                  << " type=" << toString(type)
                  << " coord=(" << coord.x << "," << coord.y << ")"
                  << " phase=" << toString(snapshot.phase)
                  << " round=" << snapshot.round << "\n";
        assert(false);
    }
    return id;
}

UnitId buyAndDeployNear(GameEngine& engine, PlayerId player, UnitType type, Coord preferred) {
    const UnitSpec* spec = engine.specFor(type);
    assert(spec);
    Coord coord = legalDeployCoordNear(engine, player, spec->layer, preferred);
    return buyAndDeploy(engine, player, type, coord);
}

void advance(GameEngine& engine, double seconds) {
    int ticks = static_cast<int>(seconds * 30.0);
    for (int i = 0; i < ticks; ++i) engine.tick(1.0 / 30.0);
}

void finishRunByRoundLimit(GameEngine& engine) {
    for (int i = 0; i < 120 && engine.snapshot().phase != Phase::Finished; ++i) {
        if (engine.snapshot().phase == Phase::Preparation) {
            engine.setReady(PlayerId::One, true);
            engine.setReady(PlayerId::Two, true);
        }
        engine.tick(1.0);
    }
    assert(engine.snapshot().phase == Phase::Finished);
}

void setupExplorationCombat(GameEngine& engine, int explorationRounds = 10) {
    engine.setExplorationRoundLimit(explorationRounds);
    GameSnapshot snapshot = engine.snapshot();
    assert(snapshot.mapKind == MapKind::ExplorationA || snapshot.mapKind == MapKind::ExplorationB);
    assert(snapshot.phase == Phase::Preparation);
}

void setupAiActionState(GameEngine& engine) {
    engine.startNewGame(GameMode::TwoPlayer);
    assert(engine.buyUnit(PlayerId::One, UnitType::GithyankiWarrior));
    UnitId fighter = firstBenchUnit(engine, PlayerId::One);
    assert(engine.deployUnit(PlayerId::One, fighter, p1MainDeploy()));
    assert(engine.buyUnit(PlayerId::One, UnitType::Ranger));
}

void writePolicyFile(const std::filesystem::path& directory,
                     const std::string& difficulty,
                     const std::string& fingerprint,
                     const AiFeatureSchema& schema,
                     double heuristicBlend) {
    std::filesystem::create_directories(directory);
    std::string filename = difficulty == "SuperHard" ? "superhard.policy.json" : "hard.policy.json";
    std::ofstream out(directory / filename);
    out << "{\n"
        << "  \"format\": \"autochess_policy_v1\",\n"
        << "  \"modelVersion\": \"linear-v1\",\n"
        << "  \"difficulty\": \"" << difficulty << "\",\n"
        << "  \"rulesFingerprint\": \"" << fingerprint << "\",\n"
        << "  \"stateFeatureCount\": " << schema.stateFeatureCount << ",\n"
        << "  \"actionFeatureCount\": " << schema.actionFeatureCount << ",\n"
        << "  \"heuristicBlend\": " << heuristicBlend << ",\n"
        << "  \"bias\": 0.0,\n"
        << "  \"weights\": [";
    int weightCount = schema.stateFeatureCount + schema.actionFeatureCount;
    for (int i = 0; i < weightCount; ++i) {
        if (i > 0) out << ",";
        out << "0";
    }
    out << "]\n}\n";
}

void test_relic_taxonomy_has_four_layers_and_family_pools() {
    std::vector<RelicSpec> relics = relicCatalog();
    bool sawBasic = false;
    bool sawBuild = false;
    bool sawTransform = false;
    bool sawUnique = false;
    bool sawExtraChoices = false;
    bool sawRosterBonus = false;
    bool sawClearGold = false;
    bool sawSummonLimitBonus = false;
    for (const RelicSpec& relic : relics) {
        sawBasic = sawBasic || relic.tier == RelicTier::Basic;
        sawBuild = sawBuild || relic.tier == RelicTier::Build;
        sawTransform = sawTransform || relic.tier == RelicTier::Transform;
        sawUnique = sawUnique || relic.tier == RelicTier::Unique;
        assert(relic.modifiers.upgradeDiscount == 0);
        assert(relic.summary.find("Upgrade") == std::string::npos);
        assert(relic.summary.find("upgrade") == std::string::npos);
        assert(std::find(relic.tags.begin(), relic.tags.end(), "upgrade") == relic.tags.end());
        sawExtraChoices = sawExtraChoices || relic.modifiers.extraRelicChoices > 0;
        sawRosterBonus = sawRosterBonus || relic.modifiers.benchBonus > 0;
        sawClearGold = sawClearGold || relic.modifiers.bonusGoldOnClear > 0;
        sawSummonLimitBonus = sawSummonLimitBonus || relic.modifiers.summonLimitBonus > 0;
    }
    assert(sawBasic);
    assert(sawBuild);
    assert(sawTransform);
    assert(sawUnique);
    assert(sawExtraChoices);
    assert(sawRosterBonus);
    assert(sawClearGold);
    assert(sawSummonLimitBonus);

    for (NeutralFamily family : {NeutralFamily::Swarm, NeutralFamily::Guardian,
                                 NeutralFamily::Caster, NeutralFamily::Assassin,
                                 NeutralFamily::Artillery}) {
        std::vector<RelicSpec> pool = relicPoolForFamily(family);
        assert(pool.size() >= 5);
    }
}

void test_battle_board_uses_33x19_isolated_wilds_map() {
    std::vector<MapKind> seenMapKinds;
    for (unsigned seed = 1; seed <= 24; ++seed) {
        GameEngine engine(seed);
        engine.startNewGame(GameMode::TwoPlayer);
        GameSnapshot snapshot = engine.snapshot();

        assert(snapshot.width == 33);
        assert(snapshot.height == 19);
        assert(snapshot.terrain.size() == static_cast<size_t>(snapshot.width * snapshot.height));

        int mirroredDifferences = 0;
        for (int y = 0; y < snapshot.height; ++y) {
            for (int x = 0; x < snapshot.width; ++x) {
                if (terrainAtSnapshot(snapshot, {x, y}) !=
                    terrainAtSnapshot(snapshot, {snapshot.width - 1 - x, y})) {
                    ++mirroredDifferences;
                }
            }
        }
        assert(mirroredDifferences > 0);

        assert(snapshotReachable(snapshot, Coord{0, snapshot.height / 2},
                                 Coord{snapshot.width - 1, snapshot.height / 2}));

        int neutralCampTerrain = 0;
        int bossTerrain = 0;
        int topEdgeOpenCount = 0;
        int bottomEdgeOpenCount = 0;
        int nearTopOpenCount = 0;
        int nearBottomOpenCount = 0;
        Coord topLeftOpen{-1, -1};
        Coord topRightOpen{-1, -1};
        Coord bottomLeftOpen{-1, -1};
        Coord bottomRightOpen{-1, -1};
        int sawNeutralCamps = 0;
        std::vector<int> neutralTypeCounts(static_cast<size_t>(UnitType::NeutralTamiaHolzt) + 1, 0);
        for (int y = 0; y < snapshot.height; ++y) {
            for (int x = 0; x < snapshot.width; ++x) {
                TerrainKind terrain = terrainAtSnapshot(snapshot, {x, y});
                if (terrain != TerrainKind::Wall) {
                    if (y == 0) {
                        ++topEdgeOpenCount;
                        if (x <= 10 && topLeftOpen.x < 0) topLeftOpen = {x, y};
                        if (x >= snapshot.width - 11 && topRightOpen.x < 0) topRightOpen = {x, y};
                    }
                    if (y == snapshot.height - 1) {
                        ++bottomEdgeOpenCount;
                        if (x <= 10 && bottomLeftOpen.x < 0) bottomLeftOpen = {x, y};
                        if (x >= snapshot.width - 11 && bottomRightOpen.x < 0) bottomRightOpen = {x, y};
                    }
                    if (y <= 1) ++nearTopOpenCount;
                    if (y >= snapshot.height - 2) ++nearBottomOpenCount;
                }
                if (terrain == TerrainKind::NeutralCamp) {
                    ++neutralCampTerrain;
                }
                if (engine.isExplorationStagingCell(PlayerId::One, {x, y}) ||
                    engine.isExplorationStagingCell(PlayerId::Two, {x, y})) {
                    assert(terrain != TerrainKind::NeutralCamp);
                    assert(terrain != TerrainKind::BossSite);
                    assert(terrain != TerrainKind::Trap);
                }
                if (terrain == TerrainKind::BossSite) ++bossTerrain;
            }
        }
        for (const UnitView& unit : snapshot.units) {
            if (!unit.alive || !unit.deployed) continue;
            assert(!engine.isExplorationStagingCell(PlayerId::One, unit.coord));
            assert(!engine.isExplorationStagingCell(PlayerId::Two, unit.coord));
            if (isNeutralMonster(unit.type) && unit.type != UnitType::NeutralRedcap) {
                TerrainKind terrain = terrainAtSnapshot(snapshot, unit.coord);
                assert(terrain == TerrainKind::NeutralCamp || terrain == TerrainKind::BossSite);
                assert(snapshotReachable(snapshot, Coord{0, snapshot.height / 2}, unit.coord));
                assert(snapshotReachable(snapshot, Coord{snapshot.width - 1, snapshot.height / 2},
                                         unit.coord));
                if (unit.name.rfind("Boss ", 0) == 0) {
                    assert(unit.coord.x > 6);
                    assert(unit.coord.x < snapshot.width - 7);
                    assert(unit.coord.y > 3);
                    assert(unit.coord.y < snapshot.height - 4);
                }
                ++sawNeutralCamps;
                ++neutralTypeCounts[static_cast<size_t>(unit.type)];
            }
        }
        std::vector<Coord> neutralCoords;
        for (const UnitView& unit : snapshot.units) {
            if (!unit.alive || !unit.deployed || !isNeutralMonster(unit.type) ||
                unit.type == UnitType::NeutralRedcap) {
                continue;
            }
            neutralCoords.push_back(unit.coord);
        }
        for (size_t i = 0; i < neutralCoords.size(); ++i) {
            for (size_t j = i + 1; j < neutralCoords.size(); ++j) {
                assert(manhattan(neutralCoords[i], neutralCoords[j]) >= 3);
            }
        }
        assert(neutralCampTerrain == 16);
        assert(bossTerrain == 7);
        assert(sawNeutralCamps == 23);
        assert(topEdgeOpenCount >= 18);
        assert(bottomEdgeOpenCount >= 18);
        assert(nearTopOpenCount >= 45);
        assert(nearBottomOpenCount >= 45);
        for (Coord edge : {topLeftOpen, topRightOpen, bottomLeftOpen, bottomRightOpen}) {
            assert(edge.x >= 0);
            assert(snapshotReachable(snapshot, Coord{0, snapshot.height / 2}, edge));
            assert(snapshotReachable(snapshot, Coord{snapshot.width - 1, snapshot.height / 2}, edge));
        }
        assert(snapshot.explorationObjectivesTotal == 28);
        assert(snapshot.explorationObjectivesCleared == 0);
        assert(snapshot.bossesCleared == 0);
        assert(snapshot.eventsTriggered == 0);
        assert(snapshot.trapsTriggered == 0);
        assert(snapshot.randomGoldEventsTotal == 0);
        assert(snapshot.randomGoldEventsClaimed == 0);
        assert(snapshot.hiddenEventsClaimed == 0);
        for (int count : neutralTypeCounts) assert(count <= 2);
        assert(neutralTypeCounts[static_cast<size_t>(UnitType::NeutralGuardianOfFaith)] >= 1);
        assert(neutralTypeCounts[static_cast<size_t>(UnitType::NeutralMinotaur)] >= 1);
        assert(neutralTypeCounts[static_cast<size_t>(UnitType::NeutralDeathKnight)] >= 1);
        assert(neutralTypeCounts[static_cast<size_t>(UnitType::NeutralAirMyrmidon)] >= 1);
        assert(neutralTypeCounts[static_cast<size_t>(UnitType::NeutralMindFlayer)] == 1);
        for (UnitType bossType : {UnitType::NeutralMindFlayer,
                                  UnitType::NeutralWaterMyrmidon,
                                  UnitType::NeutralRaphael,
                                  UnitType::NeutralKethericThorm,
                                  UnitType::NeutralMoonlightSliver,
                                  UnitType::NeutralPhaseSpiderMatriarch,
                                  UnitType::NeutralTamiaHolzt}) {
            assert(neutralTypeCounts[static_cast<size_t>(bossType)] == 1);
        }
        if (std::find(seenMapKinds.begin(), seenMapKinds.end(), snapshot.mapKind) == seenMapKinds.end()) {
            seenMapKinds.push_back(snapshot.mapKind);
        }
    }
    assert(seenMapKinds.size() == 2);
}

void test_exploration_state_tracks_objectives_and_random_gold() {
    for (int templateIndex : {0, 1}) {
        ExplorationState state;
        state.resetObjectives(templateIndex);

        ExplorationStats stats = state.stats();
        assert(stats.objectivesTotal == 28);
        assert(stats.objectivesCleared == 0);
        assert(stats.bossesCleared == 0);
        assert(stats.eventsTriggered == 0);
        assert(stats.trapsTriggered == 0);
        assert(state.neutralCamps().size() == 23);
        assert(state.traps().size() == 5);

        int campOrEliteCount = 0;
        int bossCount = 0;
        int trapCount = 0;
        std::vector<int> neutralTypeCounts(static_cast<size_t>(UnitType::NeutralTamiaHolzt) + 1, 0);
        Coord firstTrap{-1, -1};
        for (const ExplorationObjectiveState& objective : state.objectives()) {
            assert(objective.coord.x > 3);
            assert(objective.coord.x < kBoardWidth - 4);
            assert(!(objective.coord.y <= 1 && objective.coord.x <= 8));
            assert(!(objective.coord.y >= kBoardHeight - 2 && objective.coord.x <= 8));
            assert(!(objective.coord.y <= 1 && objective.coord.x >= kBoardWidth - 9));
            assert(!(objective.coord.y >= kBoardHeight - 2 && objective.coord.x >= kBoardWidth - 9));
            if (objective.kind == ExplorationObjectiveKind::Camp ||
                objective.kind == ExplorationObjectiveKind::Elite) {
                ++campOrEliteCount;
                ++neutralTypeCounts[static_cast<size_t>(objective.type)];
            } else if (objective.kind == ExplorationObjectiveKind::Boss) {
                ++bossCount;
                ++neutralTypeCounts[static_cast<size_t>(objective.type)];
            } else if (objective.kind == ExplorationObjectiveKind::Trap) {
                ++trapCount;
                if (firstTrap.x < 0) firstTrap = objective.coord;
            } else {
                assert(false);
            }
        }
        assert(campOrEliteCount == 16);
        assert(bossCount == 7);
        assert(trapCount == 5);
        for (int count : neutralTypeCounts) assert(count <= 2);
        assert(neutralTypeCounts[static_cast<size_t>(UnitType::NeutralGuardianOfFaith)] >= 1);
        assert(neutralTypeCounts[static_cast<size_t>(UnitType::NeutralMinotaur)] >= 1);
        assert(neutralTypeCounts[static_cast<size_t>(UnitType::NeutralDeathKnight)] >= 1);
        assert(neutralTypeCounts[static_cast<size_t>(UnitType::NeutralAirMyrmidon)] >= 1);
        for (UnitType bossType : {UnitType::NeutralMindFlayer,
                                  UnitType::NeutralWaterMyrmidon,
                                  UnitType::NeutralRaphael,
                                  UnitType::NeutralKethericThorm,
                                  UnitType::NeutralMoonlightSliver,
                                  UnitType::NeutralPhaseSpiderMatriarch,
                                  UnitType::NeutralTamiaHolzt}) {
            assert(neutralTypeCounts[static_cast<size_t>(bossType)] == 1);
        }

        std::optional<size_t> trapIndex = state.findTriggerableTrap(firstTrap);
        assert(trapIndex);
        state.markTriggered(*trapIndex);
        assert(!state.findTriggerableTrap(firstTrap));
        state.recordTrapSpawn(*trapIndex, 101);
        assert(state.findObjectiveForUnit(101) == trapIndex);
        assert(state.stats().trapsTriggered == 1);
        state.markCleared(*trapIndex);
        state.clearTrapSpawns(*trapIndex);
        const ExplorationObjectiveState* trap = state.objective(*trapIndex);
        assert(trap && trap->cleared && trap->spawnedUnitIds.empty());

        stats = state.stats();
        assert(stats.eventsTriggered == 0);
        assert(stats.objectivesCleared == 1);
    }

    ExplorationState state;
    std::vector<Coord> candidates = {Coord{4, 4}, Coord{5, 5}, Coord{6, 6}};
    std::mt19937 rng(7);
    state.resetHiddenEvents(candidates, rng);
    std::vector<Coord> coords = state.randomGoldCoords();
    assert(!coords.empty());
    assert(coords.size() <= candidates.size());

    std::optional<RandomGoldEventState> claimed =
        state.claimRandomGold(PlayerId::One, coords.front(), kInvalidUnitId);
    assert(claimed);
    assert(claimed->amount >= 4);
    assert(claimed->amount <= 8);
    assert(!state.claimRandomGold(PlayerId::Two, coords.front(), kInvalidUnitId));
    assert(state.stats().randomGoldEventsClaimed == 1);
}

void test_hidden_events_are_not_visible_and_can_use_edge_rows() {
    GameEngine engine(20260513);
    std::vector<std::vector<Coord>> seenEventLayouts;

    for (int run = 0; run < 6; ++run) {
        engine.startNewGame(GameMode::TwoPlayer);
        GameSnapshot snapshot = engine.snapshot();

        std::vector<Coord> eventCoords = engine.debugRandomGoldCoords();
        std::vector<Coord> healCoords = engine.debugHiddenHealingCoords();
        eventCoords.insert(eventCoords.end(), healCoords.begin(), healCoords.end());
        std::sort(eventCoords.begin(), eventCoords.end(), [](Coord lhs, Coord rhs) {
            if (lhs.y != rhs.y) return lhs.y < rhs.y;
            return lhs.x < rhs.x;
        });

        assert(eventCoords.size() >= 2);
        for (Coord coord : eventCoords) {
            TerrainKind terrain = terrainAtSnapshot(snapshot, coord);
            assert(terrain != TerrainKind::NeutralCamp);
            assert(terrain != TerrainKind::BossSite);
            assert(terrain != TerrainKind::Trap);
            assert(snapshotReachable(snapshot, Coord{0, snapshot.height / 2}, coord));
            assert(snapshotReachable(snapshot, Coord{snapshot.width - 1, snapshot.height / 2}, coord));
        }

        if (std::find(seenEventLayouts.begin(), seenEventLayouts.end(), eventCoords) ==
            seenEventLayouts.end()) {
            seenEventLayouts.push_back(eventCoords);
        }
    }

    assert(seenEventLayouts.size() > 1);
}

void test_exploration_round_selector_uses_allowed_values() {
    GameEngine engine(45);
    engine.setExplorationRoundLimit(4);
    engine.startNewGame(GameMode::TwoPlayer);
    GameSnapshot snapshot = engine.snapshot();
    assert(snapshot.explorationRoundLimit == 4);
    assert(snapshot.explorationRoundsRemaining == 4);

    engine.setExplorationRoundLimit(9);
    snapshot = engine.snapshot();
    assert(snapshot.explorationRoundLimit == 10);
    engine.lockExplorationRoundLimit();
    snapshot = engine.snapshot();
    assert(snapshot.explorationRoundLimitLocked);
    engine.setExplorationRoundLimit(2);
    assert(engine.snapshot().explorationRoundLimit == 10);
}

void test_neutral_camps_are_targetable_and_reward_kills() {
    GameEngine engine(46);
    engine.startNewGame(GameMode::TwoPlayer);

    Coord campCoord = firstNeutralCampCoord(engine.snapshot(), PlayerId::Two);
    Coord deploy{std::min(3, std::max(0, campCoord.x - 3)), campCoord.y};
    for (int i = 0; i < 5; ++i) {
        buyAndDeployNear(engine, PlayerId::One, UnitType::Ranger, deploy);
    }

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    GameSnapshot snapshot = engine.snapshot();
    const UnitView* campUnit = firstNeutralCampUnit(snapshot, PlayerId::Two);
    assert(campUnit);
    UnitId camp = campUnit->id;

    bool sawCampAttack = false;
    bool sawCampDamage = false;
    bool sawBounty = false;
    int initialCampHp = findUnit(snapshot, camp)->totalHp;
    for (int i = 0; i < 30 * 45 && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
        for (const Event& event : engine.consumeEvents()) {
            if (event.type == EventType::UnitAttacked && event.player == PlayerId::One &&
                event.target == camp) {
                sawCampAttack = true;
            }
            if (event.type == EventType::DamageDealt && event.target == camp) {
                sawCampDamage = true;
            }
            if (event.type == EventType::GoldGained && event.player == PlayerId::One &&
                event.target == camp && event.amount > 0) {
                sawBounty = true;
            }
        }
        if (sawCampAttack && sawCampDamage && sawBounty) break;
    }

    snapshot = engine.snapshot();
    const UnitView* campView = findUnit(snapshot, camp);
    assert(sawCampAttack);
    assert(sawCampDamage);
    assert(sawBounty);
    assert(!campView || campView->totalHp < initialCampHp);
    assert(!campView || !campView->alive);
}

void test_main_lane_units_ignore_unreachable_side_camps() {
    GameEngine engine(47);
    engine.startNewGame(GameMode::TwoPlayer);

    GameSnapshot opening = engine.snapshot();
    assert(opening.explorationObjectivesTotal == 28);
    assert(opening.mapKind == MapKind::ExplorationA || opening.mapKind == MapKind::ExplorationB);

    int passable = 0;
    for (int y = 0; y < opening.height; ++y) {
        for (int x = 0; x < opening.width; ++x) {
            if (terrainAtSnapshot(opening, {x, y}) != TerrainKind::Wall) ++passable;
        }
    }
    assert(passable > opening.width * opening.height / 2);
    Coord upperLeft = openCoordNear(opening, Coord{1, 1}, 6);
    Coord lowerRight = openCoordNear(opening, Coord{opening.width - 2, opening.height - 2}, 6);
    assert(snapshotReachable(opening, upperLeft, lowerRight));
}

void test_random_gold_bricks_are_hidden_and_once_only() {
    GameEngine engine(91);
    engine.startNewGame(GameMode::TwoPlayer);

    GameSnapshot opening = engine.snapshot();
    std::vector<Coord> coords = engine.debugRandomGoldCoords();
    assert(coords.size() >= 1);
    assert(coords.size() <= 3);
    assert(opening.randomGoldEventsTotal == 0);
    assert(opening.explorationObjectivesTotal == 28);

    for (Coord coord : coords) {
        TerrainKind terrain = terrainAtSnapshot(opening, coord);
        assert(terrain != TerrainKind::NeutralCamp);
        assert(terrain != TerrainKind::BossSite);
        assert(terrain != TerrainKind::Trap);
    }

    int moneyBefore = opening.players[0].money;
    int scoreBefore = opening.explorationScores[0];
    assert(engine.debugTriggerRandomGold(PlayerId::One, coords.front()));
    GameSnapshot claimed = engine.snapshot();
    int reward = claimed.players[0].money - moneyBefore;
    assert(reward >= 3);
    assert(reward <= 6);
    assert(claimed.explorationScores[0] == scoreBefore + reward);
    assert(claimed.hiddenEventsClaimedByPlayer[0] == opening.hiddenEventsClaimedByPlayer[0] + 1);
    assert(claimed.randomGoldEventsClaimed == 1);
    assert(claimed.explorationObjectivesTotal == 28);
    assert(claimed.explorationObjectivesCleared == 0);

    assert(!engine.debugTriggerRandomGold(PlayerId::Two, coords.front()));
    assert(engine.snapshot().players[1].money == opening.players[1].money);
}

void test_hidden_healing_spring_is_hidden_and_once_only() {
    GameEngine engine(92);
    engine.startNewGame(GameMode::TwoPlayer);

    std::vector<Coord> heals = engine.debugHiddenHealingCoords();
    assert(!heals.empty());

    GameSnapshot opening = engine.snapshot();
    for (Coord coord : heals) {
        TerrainKind terrain = terrainAtSnapshot(opening, coord);
        assert(terrain != TerrainKind::NeutralCamp);
        assert(terrain != TerrainKind::BossSite);
        assert(terrain != TerrainKind::Trap);
    }

    UnitId trigger = buyAndDeployNear(engine, PlayerId::One, UnitType::GithyankiWarrior, Coord{2, 9});
    assert(trigger != kInvalidUnitId);
    assert(engine.debugApplyDamage(trigger, 20, DamageType::Slashing));
    GameSnapshot before = engine.snapshot();
    const UnitView* triggerBefore = findUnit(before, trigger);
    assert(triggerBefore);
    int hpBefore = triggerBefore->totalHp;

    assert(engine.debugTriggerHiddenEvent(PlayerId::One, heals.front(), trigger));
    GameSnapshot after = engine.snapshot();
    const UnitView* triggerAfter = findUnit(after, trigger);
    assert(triggerAfter);
    assert(triggerAfter->totalHp > hpBefore);
    assert(after.hiddenEventsClaimed == before.hiddenEventsClaimed + 1);
    assert(after.hiddenEventsClaimedByPlayer[0] == before.hiddenEventsClaimedByPlayer[0] + 1);
    assert(after.explorationObjectivesCleared == before.explorationObjectivesCleared);
    assert(!engine.debugTriggerHiddenEvent(PlayerId::One, heals.front(), trigger));
}

void test_high_wis_perception_detects_hidden_gold_and_healing() {
    GameEngine engine(733);
    engine.startNewGame(GameMode::TwoPlayer);

    std::vector<Coord> gold = engine.debugRandomGoldCoords();
    std::vector<Coord> healing = engine.debugHiddenHealingCoords();
    assert(!gold.empty());
    assert(!healing.empty());

    UnitId scout = buyAndDeployNear(engine, PlayerId::One, UnitType::Druid, Coord{2, 9});
    assert(scout != kInvalidUnitId);
    assert(engine.debugSetUnitAbilityScore(scout, AbilityScoreKind::Wisdom, 50));
    assert(engine.debugSetUnitSkillProficiency(scout, SkillTag::Perception, true));

    GameSnapshot before = engine.snapshot();
    assert(engine.debugDetectHiddenEvent(PlayerId::One, gold.front(), scout));
    GameSnapshot afterGold = engine.snapshot();
    assert(afterGold.hiddenEventsClaimed == before.hiddenEventsClaimed + 1);
    assert(afterGold.hiddenEventsClaimedByPlayer[0] == before.hiddenEventsClaimedByPlayer[0] + 1);
    assert(afterGold.players[0].money > before.players[0].money);
    assert(afterGold.explorationScores[0] > before.explorationScores[0]);

    assert(engine.debugApplyDamage(scout, 12, DamageType::Slashing));
    GameSnapshot beforeHeal = engine.snapshot();
    const UnitView* wounded = findUnit(beforeHeal, scout);
    assert(wounded);
    int hpBefore = wounded->totalHp;
    assert(engine.debugDetectHiddenEvent(PlayerId::One, healing.front(), scout));
    GameSnapshot afterHeal = engine.snapshot();
    const UnitView* healed = findUnit(afterHeal, scout);
    assert(healed);
    assert(healed->totalHp > hpBefore);
    assert(afterHeal.hiddenEventsClaimed == beforeHeal.hiddenEventsClaimed + 1);
}

void test_low_wis_unit_can_miss_hidden_detection() {
    bool sawMiss = false;
    for (unsigned seed = 734; seed < 770 && !sawMiss; ++seed) {
        GameEngine engine(seed);
        engine.startNewGame(GameMode::TwoPlayer);
        std::vector<Coord> gold = engine.debugRandomGoldCoords();
        assert(!gold.empty());

        UnitId scout = buyAndDeployNear(engine, PlayerId::One, UnitType::Ranger, Coord{2, 9});
        assert(scout != kInvalidUnitId);
        assert(engine.debugSetUnitAbilityScore(scout, AbilityScoreKind::Wisdom, 1));
        assert(engine.debugSetUnitSkillProficiency(scout, SkillTag::Perception, false));

        Coord probe = gold.front();
        probe.x = probe.x + 1 < kBoardWidth ? probe.x + 1 : probe.x - 1;
        GameSnapshot before = engine.snapshot();
        bool detected = engine.debugDetectHiddenEvent(PlayerId::One, probe, scout);
        GameSnapshot after = engine.snapshot();
        if (!detected && after.hiddenEventsClaimed == before.hiddenEventsClaimed) {
            sawMiss = true;
        }
    }
    assert(sawMiss);
}

void test_internal_units_cannot_detect_hidden_events() {
    GameEngine engine(771);
    engine.startNewGame(GameMode::TwoPlayer);
    std::vector<Coord> gold = engine.debugRandomGoldCoords();
    assert(!gold.empty());

    Coord coord = openCoordNear(engine.snapshot(), Coord{2, 9});
    UnitId internal = engine.debugCreateUnit(PlayerId::One, UnitType::SkeletonByNecromancer, coord);
    assert(internal != kInvalidUnitId);
    assert(engine.debugSetUnitAbilityScore(internal, AbilityScoreKind::Wisdom, 50));
    assert(engine.debugSetUnitSkillProficiency(internal, SkillTag::Perception, true));

    GameSnapshot before = engine.snapshot();
    assert(!engine.debugDetectHiddenEvent(PlayerId::One, gold.front(), internal));
    GameSnapshot after = engine.snapshot();
    assert(after.hiddenEventsClaimed == before.hiddenEventsClaimed);
    assert(after.players[0].money == before.players[0].money);
}

void test_neutral_camps_hold_their_guard_posts() {
    GameEngine engine(65);
    engine.startNewGame(GameMode::TwoPlayer);

    Coord campCoord = firstNeutralCampCoord(engine.snapshot(), PlayerId::Two);
    buyAndDeployNear(engine, PlayerId::One, UnitType::ShieldGuardian, Coord{0, kBoardHeight / 2});

    GameSnapshot opening = engine.snapshot();
    const UnitView* campUnit = firstNeutralCampUnit(opening, PlayerId::Two);
    assert(campUnit);
    UnitId camp = campUnit->id;

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    bool sawCampAttack = false;
    bool sawCampMove = false;
    for (int i = 0; i < 30 * 1 && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
        for (const Event& event : engine.consumeEvents()) {
            if (event.actor == camp && event.type == EventType::UnitAttacked) sawCampAttack = true;
            if (event.actor == camp && event.type == EventType::UnitMoved) sawCampMove = true;
        }
    }

    GameSnapshot snapshot = engine.snapshot();
    const UnitView* campView = findUnit(snapshot, camp);
    assert(campView);
    assert(campView->coord == campCoord);
    assert(!campView->neutralActivated);
    assert(!sawCampAttack);
    assert(!sawCampMove);
}

void test_neutral_camps_wait_until_attacked_before_countering() {
    GameEngine engine(66);
    engine.startNewGame(GameMode::TwoPlayer);

    Coord campCoord = openCoordNear(engine.snapshot(), Coord{10, 9}, 5);
    UnitId camp = engine.debugCreateUnit(PlayerId::Two, UnitType::NeutralOwlbear, campCoord);
    UnitId guardian = engine.debugCreateUnit(
        PlayerId::One,
        UnitType::ShieldGuardian,
        openCoordNear(engine.snapshot(), Coord{campCoord.x - 1, campCoord.y}, 3));
    assert(camp != kInvalidUnitId);
    assert(guardian != kInvalidUnitId);

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    bool neutralAttackedBeforePlayer = false;
    bool neutralCountered = false;
    for (int i = 0; i < 30 * 1 && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
        for (const Event& event : engine.consumeEvents()) {
            if (event.actor == camp && event.type == EventType::UnitAttacked) {
                neutralAttackedBeforePlayer = true;
            }
        }
    }
    assert(!neutralAttackedBeforePlayer);

    assert(engine.debugApplyDamageFrom(guardian, camp, 1, DamageType::Piercing));
    GameSnapshot awakened = engine.snapshot();
    const UnitView* awakenedCamp = findUnit(awakened, camp);
    assert(awakenedCamp && awakenedCamp->neutralActivated);
    for (int i = 0; i < 30 * 4 && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
        for (const Event& event : engine.consumeEvents()) {
            if (event.actor == camp && event.target == guardian &&
                event.type == EventType::UnitAttacked) {
                neutralCountered = true;
            }
        }
        if (neutralCountered) break;
    }

    assert(neutralCountered);
}

void test_neutral_activation_ignores_proximity_hidden_events_and_sourceless_damage() {
    GameEngine engine(522);
    engine.startNewGame(GameMode::TwoPlayer);

    Coord campCoord = openCoordNear(engine.snapshot(), Coord{12, 9}, 5);
    UnitId camp = engine.debugCreateUnit(PlayerId::Two, UnitType::NeutralOwlbear, campCoord);
    UnitId cleric = engine.debugCreateUnit(
        PlayerId::One,
        UnitType::Cleric,
        openCoordNear(engine.snapshot(), Coord{campCoord.x - 1, campCoord.y}, 3));
    assert(camp != kInvalidUnitId);
    assert(cleric != kInvalidUnitId);

    GameSnapshot before = engine.snapshot();
    const UnitView* beforeCamp = findUnit(before, camp);
    assert(beforeCamp && !beforeCamp->neutralActivated);

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    bool neutralMovedOrAttacked = false;
    for (int i = 0; i < 30 / 2 && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
        for (const Event& event : engine.consumeEvents()) {
            if (event.actor == camp &&
                (event.type == EventType::UnitMoved || event.type == EventType::UnitAttacked)) {
                neutralMovedOrAttacked = true;
            }
        }
    }
    assert(!neutralMovedOrAttacked);
    GameSnapshot afterProximity = engine.snapshot();
    const UnitView* proximityCamp = findUnit(afterProximity, camp);
    assert(proximityCamp && !proximityCamp->neutralActivated);

    assert(engine.debugApplyDamage(camp, 1, DamageType::Force));
    GameSnapshot afterSourcelessDamage = engine.snapshot();
    const UnitView* sourcelessCamp = findUnit(afterSourcelessDamage, camp);
    assert(sourcelessCamp && !sourcelessCamp->neutralActivated);

    UnitId immuneNeutral = engine.debugCreateUnit(
        PlayerId::Two,
        UnitType::NeutralAirMyrmidon,
        openCoordNear(engine.snapshot(), Coord{campCoord.x + 3, campCoord.y + 1}, 5));
    assert(immuneNeutral != kInvalidUnitId);
    assert(engine.debugApplyDamageFrom(cleric, immuneNeutral, 6, DamageType::Poison));
    GameSnapshot afterImmuneHit = engine.snapshot();
    const UnitView* immuneView = findUnit(afterImmuneHit, immuneNeutral);
    assert(immuneView && !immuneView->neutralActivated);

    std::vector<Coord> hidden = engine.debugHiddenHealingCoords();
    std::vector<Coord> gold = engine.debugRandomGoldCoords();
    hidden.insert(hidden.end(), gold.begin(), gold.end());
    assert(!hidden.empty());
    assert(engine.debugTriggerHiddenEvent(PlayerId::One, hidden.front(), cleric));
    GameSnapshot afterHidden = engine.snapshot();
    const UnitView* hiddenCamp = findUnit(afterHidden, camp);
    assert(hiddenCamp && !hiddenCamp->neutralActivated);
}

void test_neutral_knockback_activates_only_the_shoved_guardian() {
    GameEngine engine(523);
    engine.startNewGame(GameMode::TwoPlayer);

    GameSnapshot opening = engine.snapshot();
    Coord sourceCoord{-1, -1};
    Coord campCoord{-1, -1};
    for (int y = 2; y < opening.height - 2 && sourceCoord.x < 0; ++y) {
        for (int x = 4; x < opening.width - 5; ++x) {
            Coord a{x, y};
            Coord b{x + 1, y};
            Coord c{x + 2, y};
            if (terrainAtSnapshot(opening, a) == TerrainKind::Wall ||
                terrainAtSnapshot(opening, b) == TerrainKind::Wall ||
                terrainAtSnapshot(opening, c) == TerrainKind::Wall) {
                continue;
            }
            if (hasAliveUnitAt(opening, a) || hasAliveUnitAt(opening, b) || hasAliveUnitAt(opening, c)) {
                continue;
            }
            sourceCoord = a;
            campCoord = b;
            break;
        }
    }
    assert(sourceCoord.x >= 0);

    UnitId source = engine.debugCreateUnit(PlayerId::One, UnitType::Barbarian, sourceCoord);
    UnitId camp = engine.debugCreateUnit(PlayerId::Two, UnitType::NeutralOwlbear, campCoord);
    assert(source != kInvalidUnitId);
    assert(camp != kInvalidUnitId);
    assert(engine.debugKnockback(camp, sourceCoord, 1, source));

    GameSnapshot after = engine.snapshot();
    const UnitView* campView = findUnit(after, camp);
    assert(campView && campView->neutralActivated);
    assert(manhattan(campView->coord, campCoord) == 1);
}

void test_neutral_activation_resets_next_preparation_without_healing() {
    GameEngine engine(524);
    engine.startNewGame(GameMode::TwoPlayer);

    Coord campCoord = openCoordNear(engine.snapshot(), Coord{16, 9}, 5);
    UnitId camp = engine.debugCreateUnit(PlayerId::Two, UnitType::NeutralOwlbear, campCoord);
    UnitId source = engine.debugCreateUnit(PlayerId::One,
                                           UnitType::Cleric,
                                           openCoordNear(engine.snapshot(), Coord{1, 1}, 8));
    assert(camp != kInvalidUnitId);
    assert(source != kInvalidUnitId);

    GameSnapshot beforeDamage = engine.snapshot();
    int maxHp = findUnit(beforeDamage, camp)->maxTotalHp;
    assert(engine.debugApplyDamageFrom(source, camp, 7, DamageType::Force));
    GameSnapshot damaged = engine.snapshot();
    const UnitView* damagedCamp = findUnit(damaged, camp);
    assert(damagedCamp && damagedCamp->neutralActivated);
    assert(damagedCamp->totalHp < maxHp);
    int hpAfterActivation = damagedCamp->totalHp;

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);
    for (int i = 0; i < 30 * 48 && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
        engine.consumeEvents();
    }

    GameSnapshot nextPrep = engine.snapshot();
    assert(nextPrep.phase == Phase::Preparation);
    const UnitView* resetCamp = findUnit(nextPrep, camp);
    assert(resetCamp && resetCamp->alive);
    assert(!resetCamp->neutralActivated);
    assert(resetCamp->totalHp <= hpAfterActivation);
    assert(resetCamp->totalHp < maxHp);
}

void test_phase_spider_retaliates_after_ranged_attack_with_leash() {
    GameEngine engine(520);
    engine.startNewGame(GameMode::TwoPlayer);

    Coord spiderStart = openCoordNear(engine.snapshot(), Coord{10, 9}, 5);
    UnitId spider = engine.debugCreateUnit(PlayerId::Two,
                                           UnitType::NeutralPhaseSpiderMatriarch,
                                           spiderStart);
    Coord evokerCoord{-1, -1};
    GameSnapshot withSpider = engine.snapshot();
    for (int y = 0; y < withSpider.height && evokerCoord.x < 0; ++y) {
        for (int x = 0; x < withSpider.width; ++x) {
            Coord coord{x, y};
            if (manhattan(coord, spiderStart) > 4) continue;
            if (coord == spiderStart) continue;
            if (terrainAtSnapshot(withSpider, coord) == TerrainKind::Wall) continue;
            if (hasAliveUnitAt(withSpider, coord)) continue;
            evokerCoord = coord;
            break;
        }
    }
    assert(evokerCoord.x >= 0);
    UnitId evoker = engine.debugCreateUnit(PlayerId::One, UnitType::Evoker, evokerCoord);
    assert(spider != kInvalidUnitId);
    assert(evoker != kInvalidUnitId);
    assert(engine.debugApplyDamageFrom(evoker, spider, 1, DamageType::Force));

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    bool spiderProvoked = false;
    bool spiderResponded = false;
    Coord spiderLast = spiderStart;
    for (int i = 0; i < 30 * 12 && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
        GameSnapshot snapshot = engine.snapshot();
        if (const UnitView* view = findUnit(snapshot, spider)) {
            spiderLast = view->coord;
            assert(manhattan(view->coord, spiderStart) <= 5);
        }
        for (const Event& event : engine.consumeEvents()) {
            if (event.target == spider &&
                event.text.find("awakens") != std::string::npos) {
                spiderProvoked = true;
            }
            if (event.actor == spider &&
                (event.type == EventType::UnitMoved || event.type == EventType::UnitAttacked)) {
                spiderResponded = true;
            }
        }
        if (spiderProvoked && spiderResponded) break;
    }

    assert(spiderProvoked);
    assert(spiderResponded);
    assert(manhattan(spiderLast, spiderStart) <= 5);
}

void test_neutral_guardian_returns_home_visibly_after_round_reset() {
    GameEngine engine(525);
    engine.startNewGame(GameMode::TwoPlayer);

    std::vector<Coord> coords = horizontalOpenRun(engine.snapshot(), Coord{kBoardWidth / 2, kBoardHeight / 2}, 3);
    Coord home = coords[1];
    UnitId guardian = engine.debugCreateUnit(PlayerId::Two, UnitType::NeutralOwlbear, home);
    UnitId attacker = engine.debugCreateUnit(PlayerId::One, UnitType::ShieldGuardian, coords[2]);
    assert(guardian != kInvalidUnitId);
    assert(attacker != kInvalidUnitId);

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    assert(engine.snapshot().phase == Phase::Combat);
    assert(engine.debugKnockback(guardian, coords[2], 1, attacker));
    GameSnapshot knocked = engine.snapshot();
    Coord away = findUnit(knocked, guardian)->coord;
    assert(away != home);
    assert(engine.debugApplyDamage(attacker, 9999, DamageType::Force));

    for (int i = 0; i < 5 && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
        engine.consumeEvents();
    }
    GameSnapshot prep = engine.snapshot();
    assert(prep.phase == Phase::Preparation);
    const UnitView* resting = findUnit(prep, guardian);
    assert(resting);
    assert(resting->coord == away);
    assert(!resting->neutralActivated);
    assert(resting->neutralReturningHome);

    UnitId newGuard = buyAndDeployNear(engine, PlayerId::One, UnitType::ShieldGuardian, Coord{1, 9});
    assert(newGuard != kInvalidUnitId);
    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);
    bool sawReturnCue = false;
    bool attackedWhileReturning = false;
    for (int i = 0; i < 30 * 10 && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
        for (const Event& event : engine.consumeEvents()) {
            if (event.actor == guardian && event.text.find("returned to its guard post") != std::string::npos) {
                sawReturnCue = true;
            }
            if (event.actor == guardian && event.type == EventType::UnitAttacked) {
                attackedWhileReturning = true;
            }
        }
        GameSnapshot current = engine.snapshot();
        const UnitView* view = findUnit(current, guardian);
        if (view && view->coord == home && !view->neutralReturningHome) break;
    }

    GameSnapshot returned = engine.snapshot();
    const UnitView* returnedView = findUnit(returned, guardian);
    assert(returnedView);
    assert(manhattan(returnedView->coord, home) < manhattan(away, home) ||
           (returnedView->coord == home && !returnedView->neutralReturningHome));
    assert(!returnedView->neutralActivated);
    assert(sawReturnCue || returnedView->neutralReturningHome);
    assert(!attackedWhileReturning);
}

void test_redcap_imp_and_guardian_targeting_priorities() {
    GameEngine engine(526);
    engine.startNewGame(GameMode::TwoPlayer);

    Coord redcapCoord = openCoordNear(engine.snapshot(), Coord{6, 9}, 6);
    UnitId redcap = engine.debugCreateUnit(PlayerId::Two, UnitType::NeutralRedcap, redcapCoord);
    UnitId imp = engine.debugCreateUnit(
        PlayerId::One,
        UnitType::ImpSwarm,
        openCoordNear(engine.snapshot(), Coord{redcapCoord.x - 1, redcapCoord.y}, 4));
    UnitId guardian = engine.debugCreateUnit(
        PlayerId::Two,
        UnitType::NeutralGuardianOfFaith,
        openCoordNear(engine.snapshot(), Coord{redcapCoord.x + 3, redcapCoord.y}, 5));
    assert(redcap != kInvalidUnitId);
    assert(imp != kInvalidUnitId);
    assert(guardian != kInvalidUnitId);

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    bool impAttackedRedcap = false;
    bool dormantGuardianAttacked = false;
    for (int i = 0; i < 30 * 4 && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
        for (const Event& event : engine.consumeEvents()) {
            if (event.actor == imp && event.target == redcap && event.type == EventType::UnitAttacked) {
                impAttackedRedcap = true;
            }
            if (event.actor == guardian && event.type == EventType::UnitAttacked) {
                dormantGuardianAttacked = true;
            }
        }
        if (impAttackedRedcap) break;
    }
    assert(impAttackedRedcap);
    assert(!dormantGuardianAttacked);

    assert(engine.debugApplyDamageFrom(imp, guardian, 1, DamageType::Force));
    for (const Event& ignored : engine.consumeEvents()) {
        (void)ignored;
    }
    bool guardianAttackedImp = false;
    for (int i = 0; i < 30 * 5 && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
        for (const Event& event : engine.consumeEvents()) {
            if (event.actor == guardian && event.target == imp && event.type == EventType::UnitAttacked) {
                guardianAttackedImp = true;
            }
        }
        if (guardianAttackedImp) break;
    }
    assert(guardianAttackedImp);
}

void test_neutral_spaw_caps_servants_and_servants_guard_locally() {
    GameEngine engine(521);
    engine.startNewGame(GameMode::TwoPlayer);

    GameSnapshot opening = engine.snapshot();
    Coord spawCoord{-1, -1};
    for (int y = 4; y < kBoardHeight - 4 && spawCoord.x < 0; ++y) {
        for (int x = 10; x < kBoardWidth - 10; ++x) {
            Coord coord{x, y};
            if (terrainAtSnapshot(opening, coord) == TerrainKind::Wall) continue;
            if (hasAliveUnitAt(opening, coord)) continue;
            if (hasAliveUnitAt(opening, {x - 1, y}) ||
                hasAliveUnitAt(opening, {x, y - 1}) ||
                hasAliveUnitAt(opening, {x, y + 1}) ||
                hasAliveUnitAt(opening, {x + 1, y})) {
                continue;
            }
            spawCoord = coord;
            break;
        }
    }
    assert(spawCoord.x >= 0);

    UnitId spaw = engine.debugCreateUnit(PlayerId::Two, UnitType::NeutralSovereignSpaw, spawCoord);
    UnitId attacker = engine.debugCreateUnit(
        PlayerId::One,
        UnitType::Ranger,
        openCoordNear(engine.snapshot(), Coord{spawCoord.x - 3, spawCoord.y}, 5));
    assert(spaw != kInvalidUnitId);
    assert(attacker != kInvalidUnitId);

    for (Coord preferred : {Coord{spawCoord.x - 1, spawCoord.y},
                           Coord{spawCoord.x, spawCoord.y - 1},
                           Coord{spawCoord.x, spawCoord.y + 1},
                           Coord{spawCoord.x + 1, spawCoord.y}}) {
        UnitId victim = engine.debugCreateUnit(PlayerId::One,
                                               UnitType::Skeleton,
                                               openCoordNear(engine.snapshot(), preferred, 3));
        assert(victim != kInvalidUnitId);
        assert(engine.debugApplyDamage(victim, 999, DamageType::Bludgeoning));
    }
    assert(engine.debugApplyDamage(spaw, 1, DamageType::Piercing));

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    for (int i = 0; i < 30 * 12 && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
        engine.consumeEvents();
    }

    GameSnapshot snapshot = engine.snapshot();
    std::vector<Coord> spawOrigins{spawCoord};
    for (const UnitView& unit : snapshot.units) {
        if (unit.type != UnitType::NeutralSovereignSpaw || !unit.deployed) continue;
        if (std::find(spawOrigins.begin(), spawOrigins.end(), unit.coord) == spawOrigins.end()) {
            spawOrigins.push_back(unit.coord);
        }
    }

    int localServants = 0;
    for (const UnitView& unit : snapshot.units) {
        if (unit.type != UnitType::SporeServant || !unit.alive || !unit.deployed) continue;
        assert(unit.owner == PlayerId::Two);
        bool guardedBySpaw = false;
        for (Coord origin : spawOrigins) {
            if (manhattan(unit.coord, origin) <= 4) guardedBySpaw = true;
        }
        if (manhattan(unit.coord, spawCoord) <= 4) ++localServants;
        assert(guardedBySpaw);
    }
    assert(localServants <= 2);
    assert(localServants > 0);
}

void test_side_neutral_camps_do_not_respawn_after_death() {
    GameEngine engine(48);
    engine.startNewGame(GameMode::TwoPlayer);

    GameSnapshot opening = engine.snapshot();
    UnitId camp = kInvalidUnitId;
    Coord campCoord{-1, -1};
    for (const UnitView& unit : opening.units) {
        if (!unit.alive || !unit.deployed || !isNeutralMonster(unit.type) ||
            unit.type == UnitType::NeutralRedcap) {
            continue;
        }
        if (terrainAtSnapshot(opening, unit.coord) != TerrainKind::NeutralCamp) continue;
        camp = unit.id;
        campCoord = unit.coord;
        break;
    }
    assert(camp != kInvalidUnitId);
    Coord deploy{std::min(3, std::max(0, campCoord.x - 4)), campCoord.y};
    engine.grantGold(PlayerId::One, 120);
    buyAndDeployNear(engine, PlayerId::One, UnitType::Barbarian, deploy);
    buyAndDeployNear(engine, PlayerId::One, UnitType::Barbarian, deploy);
    buyAndDeployNear(engine, PlayerId::One, UnitType::Paladin, deploy);
    buyAndDeployNear(engine, PlayerId::One, UnitType::Paladin, deploy);

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    GameSnapshot snapshot = engine.snapshot();

    bool sawCampDeath = false;
    for (int i = 0; i < 30 * 60; ++i) {
        engine.tick(1.0 / 30.0);
        for (const Event& event : engine.consumeEvents()) {
            if (event.type == EventType::UnitDied && event.target == camp) sawCampDeath = true;
        }
        snapshot = engine.snapshot();
        if (sawCampDeath) break;
    }

    snapshot = engine.snapshot();
    const UnitView* campView = findUnit(snapshot, camp);
    assert(sawCampDeath);
    assert(!campView || !campView->alive || !campView->deployed);
    for (const UnitView& unit : snapshot.units) {
        assert(!(unit.alive && unit.deployed && unit.coord == campCoord && unit.id != camp));
    }
}

void test_side_trap_spawns_redcap_ambush_once() {
    const std::array<Coord, 10> candidates = {
        Coord{7, 5}, Coord{25, 5}, Coord{7, 13}, Coord{25, 13}, Coord{16, 6},
        Coord{11, 5}, Coord{21, 5}, Coord{11, 13}, Coord{21, 13}, Coord{16, 11}
    };

    bool testedActiveAmbush = false;
    for (unsigned seed = 1; seed <= 80 && !testedActiveAmbush; ++seed) {
        for (PlayerId triggeringPlayer : {PlayerId::One, PlayerId::Two}) {
            GameEngine engine(seed);
            engine.startNewGame(GameMode::TwoPlayer);

            Coord triggered{-1, -1};
            for (Coord coord : candidates) {
                if (engine.debugTriggerTrap(triggeringPlayer, coord)) {
                    triggered = coord;
                    break;
                }
            }
            if (triggered.x < 0) continue;

            int ambushEvents = 0;
            int spawnedRedcaps = 0;
            for (const Event& event : engine.consumeEvents()) {
                if (event.text.find("Redcap ambush triggered") != std::string::npos) {
                    ++ambushEvents;
                    spawnedRedcaps += event.amount;
                }
            }
            assert(ambushEvents == 1);
            assert(spawnedRedcaps == 3);
            assert(!engine.debugTriggerTrap(triggeringPlayer, triggered));
            assert(engine.snapshot().trapsTriggered == 1);

            GameSnapshot snapshot = engine.snapshot();
            std::vector<UnitId> redcapIds;
            for (const UnitView& unit : snapshot.units) {
                if (unit.type == UnitType::NeutralRedcap) {
                    redcapIds.push_back(unit.id);
                }
            }
            assert(redcapIds.size() == 3);

            Coord deploy = triggeringPlayer == PlayerId::One ? Coord{3, triggered.y}
                                                             : Coord{kBoardWidth - 4, triggered.y};
            if (!engine.canDeploy(triggeringPlayer, deploy, UnitLayer::Land)) {
                deploy = triggeringPlayer == PlayerId::One ? Coord{3, kBoardHeight / 2}
                                                           : Coord{kBoardWidth - 4, kBoardHeight / 2};
            }
            if (!engine.canDeploy(triggeringPlayer, deploy, UnitLayer::Land)) continue;

            assert(engine.buyUnit(triggeringPlayer, UnitType::ShieldGuardian));
            UnitId guard = firstBenchUnit(engine, triggeringPlayer);
            assert(engine.deployUnit(triggeringPlayer, guard, deploy));
            engine.setReady(PlayerId::One, true);
            engine.setReady(PlayerId::Two, true);

            bool redcapActed = false;
            for (int i = 0; i < 30 * 12 && engine.snapshot().phase == Phase::Combat; ++i) {
                engine.tick(1.0 / 30.0);
                for (const Event& event : engine.consumeEvents()) {
                    if ((event.type == EventType::UnitMoved || event.type == EventType::UnitAttacked) &&
                        std::find(redcapIds.begin(), redcapIds.end(), event.actor) != redcapIds.end()) {
                        redcapActed = true;
                    }
                }
                if (redcapActed) break;
            }
            assert(redcapActed);
            testedActiveAmbush = true;
            break;
        }
    }
    assert(testedActiveAmbush);
}

void test_high_dex_unit_avoids_redcap_trap_and_clears_it() {
    const std::array<Coord, 10> candidates = {
        Coord{7, 5}, Coord{25, 5}, Coord{7, 13}, Coord{25, 13}, Coord{16, 6},
        Coord{11, 5}, Coord{21, 5}, Coord{11, 13}, Coord{21, 13}, Coord{16, 11}
    };

    GameEngine engine(772);
    engine.startNewGame(GameMode::TwoPlayer);
    UnitId scout = buyAndDeployNear(engine, PlayerId::One, UnitType::Ranger, Coord{2, 9});
    assert(scout != kInvalidUnitId);
    assert(engine.debugSetUnitAbilityScore(scout, AbilityScoreKind::Dexterity, 50));

    GameSnapshot before = engine.snapshot();
    Coord triggered{-1, -1};
    for (Coord coord : candidates) {
        if (engine.debugTriggerTrap(PlayerId::One, coord, scout)) {
            triggered = coord;
            break;
        }
    }
    assert(triggered.x >= 0);

    bool sawAvoided = false;
    bool sawAmbush = false;
    for (const Event& event : engine.consumeEvents()) {
        if (event.text.find("Trap avoided") != std::string::npos) sawAvoided = true;
        if (event.text.find("Redcap ambush triggered") != std::string::npos) sawAmbush = true;
    }
    GameSnapshot after = engine.snapshot();
    int redcaps = 0;
    for (const UnitView& unit : after.units) {
        if (unit.type == UnitType::NeutralRedcap && unit.alive && unit.deployed) ++redcaps;
    }
    assert(sawAvoided);
    assert(!sawAmbush);
    assert(redcaps == 0);
    assert(after.trapsTriggered == before.trapsTriggered);
    assert(after.explorationObjectivesCleared == before.explorationObjectivesCleared + 1);
    assert(after.explorationScores[0] > before.explorationScores[0]);
}

void test_low_dex_unit_can_fail_redcap_trap_save() {
    const std::array<Coord, 10> candidates = {
        Coord{7, 5}, Coord{25, 5}, Coord{7, 13}, Coord{25, 13}, Coord{16, 6},
        Coord{11, 5}, Coord{21, 5}, Coord{11, 13}, Coord{21, 13}, Coord{16, 11}
    };

    bool sawFailure = false;
    for (unsigned seed = 773; seed < 820 && !sawFailure; ++seed) {
        GameEngine engine(seed);
        engine.startNewGame(GameMode::TwoPlayer);
        UnitId scout = buyAndDeployNear(engine, PlayerId::One, UnitType::ShieldGuardian, Coord{2, 9});
        assert(scout != kInvalidUnitId);
        assert(engine.debugSetUnitAbilityScore(scout, AbilityScoreKind::Dexterity, 1));

        Coord triggered{-1, -1};
        for (Coord coord : candidates) {
            if (engine.debugTriggerTrap(PlayerId::One, coord, scout)) {
                triggered = coord;
                break;
            }
        }
        if (triggered.x < 0) continue;

        int ambushEvents = 0;
        int spawnedRedcaps = 0;
        for (const Event& event : engine.consumeEvents()) {
            if (event.text.find("Redcap ambush triggered") != std::string::npos) {
                ++ambushEvents;
                spawnedRedcaps += event.amount;
            }
        }
        if (ambushEvents == 1 && spawnedRedcaps == 3 && engine.snapshot().trapsTriggered == 1) {
            sawFailure = true;
        }
    }
    assert(sawFailure);
}

void test_redcap_ambush_persists_next_round_until_killed() {
    GameEngine engine(52);
    engine.startNewGame(GameMode::TwoPlayer);

    Coord triggered{-1, -1};
    for (Coord coord : {Coord{7, 5}, Coord{25, 5}, Coord{7, 13}, Coord{25, 13}, Coord{16, 6},
                        Coord{11, 5}, Coord{21, 5}, Coord{11, 13}, Coord{21, 13}, Coord{16, 11}}) {
        if (engine.debugTriggerTrap(PlayerId::One, coord)) {
            triggered = coord;
            break;
        }
    }
    assert(triggered.x >= 0);
    engine.consumeEvents();

    int spawned = 0;
    for (const UnitView& unit : engine.snapshot().units) {
        if (unit.type == UnitType::NeutralRedcap && unit.alive && unit.deployed) ++spawned;
    }
    assert(spawned == 3);

    buyAndDeploy(engine, PlayerId::One, UnitType::Skeleton, Coord{2, 17});
    buyAndDeploy(engine, PlayerId::Two, UnitType::Skeleton, Coord{kBoardWidth - 3, 17});
    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);
    for (int i = 0; i < 30 * 47 && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
        engine.consumeEvents();
    }
    engine.tick(1.0 / 30.0);

    GameSnapshot snapshot = engine.snapshot();
    assert(snapshot.phase == Phase::Preparation);
    int liveRedcaps = 0;
    for (const UnitView& unit : snapshot.units) {
        if (unit.type != UnitType::NeutralRedcap || !unit.alive || !unit.deployed) continue;
        ++liveRedcaps;
        assert(manhattan(unit.coord, triggered) <= 5);
        assert(landOccupantsAt(snapshot, unit.coord) == 1);
    }
    assert(liveRedcaps == spawned);
}

void test_redcap_ambush_has_short_leash() {
    GameEngine engine(53);
    engine.startNewGame(GameMode::TwoPlayer);

    Coord trap{-1, -1};
    for (Coord candidate : {Coord{7, 5}, Coord{25, 5}, Coord{7, 13}, Coord{25, 13}, Coord{16, 6},
                            Coord{11, 5}, Coord{21, 5}, Coord{11, 13}, Coord{21, 13}, Coord{16, 11}}) {
        if (engine.debugTriggerTrap(PlayerId::One, candidate)) {
            trap = candidate;
            break;
        }
    }
    assert(trap.x >= 0);
    engine.consumeEvents();

    std::vector<UnitId> redcaps;
    for (const UnitView& unit : engine.snapshot().units) {
        if (unit.type == UnitType::NeutralRedcap) redcaps.push_back(unit.id);
    }
    assert(redcaps.size() == 3);

    buyAndDeploy(engine, PlayerId::One, UnitType::Skeleton, Coord{2, 17});
    buyAndDeploy(engine, PlayerId::Two, UnitType::Skeleton, Coord{kBoardWidth - 3, 17});
    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    bool redcapMovedOrAttacked = false;
    for (int i = 0; i < 30 * 8 && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
        for (const Event& event : engine.consumeEvents()) {
            if ((event.type == EventType::UnitMoved || event.type == EventType::UnitAttacked) &&
                std::find(redcaps.begin(), redcaps.end(), event.actor) != redcaps.end()) {
                redcapMovedOrAttacked = true;
            }
        }
    }
    assert(!redcapMovedOrAttacked);
}

void test_neutral_monster_roster_has_bg3_camp_and_trap_units() {
    GameEngine engine(49);
    engine.startNewGame(GameMode::TwoPlayer);

    for (UnitType type : {UnitType::NeutralSpectator,
                          UnitType::NeutralOwlbear,
                          UnitType::NeutralMindFlayer,
                          UnitType::NeutralSovereignSpaw,
                          UnitType::NeutralKarniss,
                          UnitType::NeutralRedcap,
                          UnitType::NeutralWaterMyrmidon,
                          UnitType::NeutralPhaseSpiderMatriarch,
                          UnitType::NeutralRaphael,
                          UnitType::NeutralKethericThorm,
                          UnitType::NeutralMoonlightSliver,
                          UnitType::NeutralGuardianOfFaith,
                          UnitType::NeutralMinotaur,
                          UnitType::NeutralDeathKnight,
                          UnitType::NeutralAirMyrmidon,
                          UnitType::NeutralTamiaHolzt}) {
        const UnitSpec* spec = engine.specFor(type);
        assert(spec);
        assert(spec->cost == 0);
        assert(spec->ability != AbilityKind::None);
        assert(isNeutralMonster(type));
        assert(isInternalUnit(type));
        assert(!engine.buyUnit(PlayerId::One, type));
        assert(!toString(type).empty());
    }

    assert(engine.specFor(UnitType::NeutralSpectator)->ability == AbilityKind::SpectatorWoundingRay);
    assert(engine.specFor(UnitType::NeutralOwlbear)->ability == AbilityKind::OwlbearMultiattack);
    assert(engine.specFor(UnitType::NeutralMindFlayer)->ability == AbilityKind::MindBlast);
    assert(engine.specFor(UnitType::NeutralMindFlayer)->spellSaveDc >= 18);
    assert(engine.specFor(UnitType::NeutralMindFlayer)->maxHp >= 320);
    assert(engine.specFor(UnitType::NeutralSovereignSpaw)->ability == AbilityKind::AnimatingSpores);
    assert(engine.specFor(UnitType::NeutralKarniss)->ability == AbilityKind::KarnissCruelSting);
    assert(engine.specFor(UnitType::NeutralKarniss)->ability != AbilityKind::RogueAmbush);
    assert(engine.specFor(UnitType::NeutralWaterMyrmidon)->ability == AbilityKind::HiemalStrike);
    assert(engine.specFor(UnitType::NeutralPhaseSpiderMatriarch)->ability == AbilityKind::VenomousBite);
    assert(engine.specFor(UnitType::NeutralRaphael)->ability == AbilityKind::DiabolicChains);
    assert(engine.specFor(UnitType::NeutralKethericThorm)->ability == AbilityKind::KethericSmite);
    assert(engine.specFor(UnitType::NeutralMoonlightSliver)->ability == AbilityKind::SelunesIre);
    assert(engine.specFor(UnitType::NeutralGuardianOfFaith)->ability == AbilityKind::StrikeOfTheGuardian);
    assert(engine.specFor(UnitType::NeutralGuardianOfFaith)->maxHp < 120);
    assert(engine.specFor(UnitType::NeutralMinotaur)->ability == AbilityKind::MinotaurCharge);
    assert(engine.specFor(UnitType::NeutralDeathKnight)->ability == AbilityKind::StaggeringSmite);
    assert(engine.specFor(UnitType::NeutralAirMyrmidon)->ability == AbilityKind::ElectrifiedFlail);
    assert(engine.specFor(UnitType::NeutralAirMyrmidon)->layer == UnitLayer::Air);
    assert(engine.specFor(UnitType::NeutralAirMyrmidon)->maxHp < 160);
    assert(engine.specFor(UnitType::NeutralTamiaHolzt)->ability == AbilityKind::Blight);
    assert(isInternalUnit(UnitType::SporeServant));
}

void test_run_modifiers_change_costs_and_bench_limit() {
    GameEngine engine(33);
    engine.startNewGame(GameMode::TwoPlayer);

    RunModifiers modifiers;
    modifiers.costDiscount = 2;
    modifiers.upgradeDiscount = 2;
    modifiers.benchBonus = 2;
    engine.setRunModifiers(PlayerId::One, modifiers);

    const UnitSpec* skeleton = engine.specFor(UnitType::Skeleton);
    assert(skeleton);
    assert(engine.effectiveBuyCost(PlayerId::One, *skeleton) == 2);
    assert(engine.effectiveUpgradeCost(PlayerId::One, *skeleton) == 3);
    assert(engine.benchLimit(PlayerId::One) == 12);

    for (int i = 0; i < 11; ++i) {
        assert(engine.buyUnit(PlayerId::One, UnitType::Skeleton));
    }
    assert(engine.snapshot().players[0].bench.size() == 11);
}

void test_roster_cap_counts_deployed_units() {
    GameEngine engine(34);
    engine.startNewGame(GameMode::TwoPlayer);
    engine.grantGold(PlayerId::One, 200);
    engine.consumeEvents();

    for (int i = 0; i < 10; ++i) {
        assert(engine.buyUnit(PlayerId::One, UnitType::Skeleton));
        UnitId id = firstBenchUnit(engine, PlayerId::One);
        Coord coord = legalDeployCoordNear(engine, PlayerId::One, UnitLayer::Land, Coord{2, 3});
        assert(engine.deployUnit(PlayerId::One, id, coord));
    }
    assert(engine.snapshot().players[0].bench.empty());
    assert(!engine.buyUnit(PlayerId::One, UnitType::Skeleton));

    RunModifiers modifiers;
    modifiers.benchBonus = 3;
    engine.setRunModifiers(PlayerId::One, modifiers);
    assert(engine.buyUnit(PlayerId::One, UnitType::Skeleton));
    assert(engine.buyUnit(PlayerId::One, UnitType::Skeleton));
    assert(engine.buyUnit(PlayerId::One, UnitType::Skeleton));
    assert(!engine.buyUnit(PlayerId::One, UnitType::Skeleton));
}

void test_shop_roster_uses_fifteen_dnd_units() {
    GameEngine engine(21);
    engine.startNewGame(GameMode::TwoPlayer);

    std::vector<std::string> names;
    for (const UnitSpec& spec : engine.shop()) {
        if (spec.cost > 0 && spec.type != UnitType::SkeletonByNecromancer && spec.type != UnitType::Treant &&
            spec.type != UnitType::SporeServant) {
            names.push_back(spec.name);
        }
    }

    const std::vector<std::string> expected = {
        "Skeleton Mob",
        "Githyanki Warrior",
        "Elven Ranger",
        "Berserker",
        "Grave Necromancer",
        "Fire Mephit",
        "Imp Swarm",
        "Goblin Ambusher",
        "Oathbound Paladin",
        "Dragon Wyrmling",
        "Shield Guardian",
        "Life Cleric",
        "Arcane Evoker",
        "Shadow Rogue",
        "Circle Druid"
    };
    assert(names == expected);
}

void test_shop_costs_use_expanded_budget_tiers() {
    GameEngine engine(25);
    engine.startNewGame(GameMode::TwoPlayer);

    std::vector<int> costs;
    for (const UnitSpec& spec : engine.shop()) {
        if (spec.cost > 0 && spec.type != UnitType::SkeletonByNecromancer && spec.type != UnitType::Treant &&
            spec.type != UnitType::SporeServant) {
            costs.push_back(spec.cost);
        }
    }

    std::sort(costs.begin(), costs.end());
    std::vector<int> uniqueCosts = costs;
    uniqueCosts.erase(std::unique(uniqueCosts.begin(), uniqueCosts.end()), uniqueCosts.end());
    assert(costs.front() == 4);
    assert(costs.back() == 22);
    assert(uniqueCosts.size() >= 11);
}

void test_shop_units_expose_dnd_combat_stats() {
    GameEngine engine(29);
    engine.startNewGame(GameMode::TwoPlayer);

    const UnitSpec* paladin = engine.specFor(UnitType::Paladin);
    const UnitSpec* evoker = engine.specFor(UnitType::Evoker);
    const UnitSpec* rogue = engine.specFor(UnitType::RogueAssassin);

    assert(paladin);
    assert(evoker);
    assert(rogue);
    assert(paladin->armorClass >= 18);
    assert(paladin->attackBonus > 0);
    assert(evoker->spellSaveDc >= 15);
    assert(evoker->ability == AbilityKind::EvokerMagicMissile);
    assert(evoker->maxHp < 70);
    assert(evoker->attack >= 50);
    assert(rogue->abilityValue > 0);
}

void test_unit_profiles_cover_every_unit_type() {
    GameEngine engine(730);
    engine.startNewGame(GameMode::TwoPlayer);

    const std::vector<UnitType> allTypes = {
        UnitType::Skeleton,
        UnitType::SkeletonByNecromancer,
        UnitType::GithyankiWarrior,
        UnitType::Ranger,
        UnitType::Barbarian,
        UnitType::Necromancer,
        UnitType::FireMephit,
        UnitType::ImpSwarm,
        UnitType::GoblinSkirmisher,
        UnitType::Paladin,
        UnitType::DragonWyrmling,
        UnitType::NeutralSpectator,
        UnitType::NeutralOwlbear,
        UnitType::NeutralMindFlayer,
        UnitType::NeutralSovereignSpaw,
        UnitType::NeutralKarniss,
        UnitType::NeutralRedcap,
        UnitType::NeutralWaterMyrmidon,
        UnitType::NeutralPhaseSpiderMatriarch,
        UnitType::NeutralRaphael,
        UnitType::NeutralKethericThorm,
        UnitType::NeutralMoonlightSliver,
        UnitType::NeutralGuardianOfFaith,
        UnitType::NeutralMinotaur,
        UnitType::NeutralDeathKnight,
        UnitType::NeutralAirMyrmidon,
        UnitType::ShieldGuardian,
        UnitType::Cleric,
        UnitType::Evoker,
        UnitType::RogueAssassin,
        UnitType::Druid,
        UnitType::Treant,
        UnitType::SporeServant,
        UnitType::NeutralTamiaHolzt
    };

    for (UnitType type : allTypes) {
        const UnitSpec* spec = engine.specFor(type);
        assert(spec);
        const UnitProfile& profile = spec->profile;
        assert(profile.level >= 1);
        assert(profile.tier >= 1);
        assert(profile.abilityScores.strength >= 1);
        assert(profile.abilityScores.dexterity >= 1);
        assert(profile.abilityScores.constitution >= 1);
        assert(profile.abilityScores.intelligence >= 1);
        assert(profile.abilityScores.wisdom >= 1);
        assert(profile.abilityScores.charisma >= 1);
        assert(!profileSummary(profile).empty());

        if (profile.kind == ProfileKind::PlayableCharacter) {
            assert(profile.race != Race::None);
            assert(profile.characterClass != CharacterClass::None);
            assert(profile.creatureType == CreatureType::Humanoid);
        } else if (profile.kind == ProfileKind::PureMonster || profile.kind == ProfileKind::Summon ||
                   profile.kind == ProfileKind::NamedActor) {
            assert(profile.creatureType != CreatureType::None);
            assert(!profile.archetype.empty());
        }
    }

    for (UnitType named : {UnitType::NeutralRaphael,
                           UnitType::NeutralKethericThorm,
                           UnitType::NeutralKarniss,
                           UnitType::NeutralTamiaHolzt,
                           UnitType::NeutralSovereignSpaw}) {
        assert(engine.specFor(named)->profile.kind == ProfileKind::NamedActor);
    }

    for (UnitType monster : {UnitType::NeutralOwlbear,
                             UnitType::NeutralMinotaur,
                             UnitType::NeutralRedcap,
                             UnitType::NeutralSpectator,
                             UnitType::NeutralWaterMyrmidon,
                             UnitType::NeutralPhaseSpiderMatriarch}) {
        assert(engine.specFor(monster)->profile.kind == ProfileKind::PureMonster);
    }

    assert(engine.specFor(UnitType::Skeleton)->profile.kind == ProfileKind::Summon);
    assert(engine.specFor(UnitType::Treant)->profile.kind == ProfileKind::Summon);
    assert(engine.specFor(UnitType::SporeServant)->profile.kind == ProfileKind::Summon);
}

void test_derived_combat_stats_use_bg3_profile_abilities() {
    GameEngine engine(731);
    engine.startNewGame(GameMode::TwoPlayer);

    auto expectedHit = [](const UnitSpec& spec) {
        return proficiencyBonusForLevel(spec.profile.level) +
               abilityModifier(abilityScore(spec.profile, spec.profile.attackAbility)) +
               spec.attackTuning;
    };
    auto expectedDc = [](const UnitSpec& spec) {
        return 8 + proficiencyBonusForLevel(spec.profile.level) +
               abilityModifier(abilityScore(spec.profile, spec.profile.castingAbility)) +
               spec.dcTuning;
    };
    auto expectedAc = [](const UnitSpec& spec) {
        int dexMod = abilityModifier(abilityScore(spec.profile, AbilityScoreKind::Dexterity));
        return spec.profile.armorBase + std::min(dexMod, spec.profile.armorDexCap) +
               spec.profile.shieldBonus + spec.profile.naturalArmorBonus + spec.acTuning;
    };

    const UnitSpec* ranger = engine.specFor(UnitType::Ranger);
    const UnitSpec* evoker = engine.specFor(UnitType::Evoker);
    const UnitSpec* cleric = engine.specFor(UnitType::Cleric);
    const UnitSpec* druid = engine.specFor(UnitType::Druid);
    const UnitSpec* raphael = engine.specFor(UnitType::NeutralRaphael);
    const UnitSpec* paladin = engine.specFor(UnitType::Paladin);
    const UnitSpec* guardian = engine.specFor(UnitType::ShieldGuardian);
    assert(ranger && evoker && cleric && druid && raphael && paladin && guardian);

    assert(ranger->profile.attackAbility == AbilityScoreKind::Dexterity);
    assert(ranger->attackBonus == expectedHit(*ranger));
    assert(evoker->profile.castingAbility == AbilityScoreKind::Intelligence);
    assert(evoker->spellSaveDc == expectedDc(*evoker));
    assert(cleric->profile.castingAbility == AbilityScoreKind::Wisdom);
    assert(cleric->spellSaveDc == expectedDc(*cleric));
    assert(druid->profile.castingAbility == AbilityScoreKind::Wisdom);
    assert(druid->spellSaveDc == expectedDc(*druid));
    assert(raphael->profile.castingAbility == AbilityScoreKind::Charisma);
    assert(raphael->spellSaveDc == expectedDc(*raphael));
    assert(paladin->armorClass == expectedAc(*paladin));
    assert(guardian->armorClass == expectedAc(*guardian));
}

void test_saving_throws_use_specific_ability_scores() {
    GameEngine engine(732);
    engine.startNewGame(GameMode::TwoPlayer);

    const UnitSpec* ranger = engine.specFor(UnitType::Ranger);
    const UnitSpec* shieldGuardian = engine.specFor(UnitType::ShieldGuardian);
    assert(ranger && shieldGuardian);
    assert(savingThrowBonusFor(*ranger, AbilityScoreKind::Dexterity) >
           savingThrowBonusFor(*ranger, AbilityScoreKind::Wisdom));
    assert(savingThrowBonusFor(*shieldGuardian, AbilityScoreKind::Constitution) >
           savingThrowBonusFor(*shieldGuardian, AbilityScoreKind::Dexterity));
}

void test_unit_attack_ranges_match_roles() {
    GameEngine engine(59);
    engine.startNewGame(GameMode::TwoPlayer);

    struct ExpectedRange {
        UnitType type;
        int range;
    };
    const std::vector<ExpectedRange> expected = {
        {UnitType::Skeleton, 1},
        {UnitType::SkeletonByNecromancer, 1},
        {UnitType::GithyankiWarrior, 1},
        {UnitType::Ranger, 3},
        {UnitType::Barbarian, 1},
        {UnitType::Necromancer, 2},
        {UnitType::FireMephit, 2},
        {UnitType::ImpSwarm, 3},
        {UnitType::GoblinSkirmisher, 1},
        {UnitType::Paladin, 1},
        {UnitType::DragonWyrmling, 3},
        {UnitType::NeutralSpectator, 4},
        {UnitType::NeutralOwlbear, 1},
        {UnitType::NeutralMindFlayer, 4},
        {UnitType::NeutralSovereignSpaw, 2},
        {UnitType::NeutralKarniss, 1},
        {UnitType::NeutralRedcap, 1},
        {UnitType::NeutralWaterMyrmidon, 1},
        {UnitType::NeutralPhaseSpiderMatriarch, 1},
        {UnitType::NeutralRaphael, 4},
        {UnitType::NeutralKethericThorm, 1},
        {UnitType::NeutralMoonlightSliver, 4},
        {UnitType::NeutralGuardianOfFaith, 3},
        {UnitType::NeutralMinotaur, 4},
        {UnitType::NeutralDeathKnight, 1},
        {UnitType::NeutralAirMyrmidon, 1},
        {UnitType::ShieldGuardian, 1},
        {UnitType::Cleric, 3},
        {UnitType::Evoker, 4},
        {UnitType::RogueAssassin, 1},
        {UnitType::Druid, 2},
        {UnitType::Treant, 1},
        {UnitType::SporeServant, 1},
        {UnitType::NeutralTamiaHolzt, 4}
    };

    for (const ExpectedRange& entry : expected) {
        const UnitSpec* spec = engine.specFor(entry.type);
        assert(spec);
        assert(spec->range == entry.range);
        assert(spec->range >= 1);
        assert(spec->range <= 4);
    }
}

void test_damage_packets_use_bg3_style_ranges_and_types() {
    DamagePacket piercing{{DamageRoll{1, 6, 7, DamageType::Piercing}}, 0, "test"};
    assert(damagePacketMin(piercing) == 8);
    assert(damagePacketMax(piercing) == 13);
    assert(damageRange(piercing) == "8~13");
    assert(damageFormula(piercing).find("1d6 + 7 Piercing") != std::string::npos);

    DamagePacket twoPart{{DamageRoll{4, 8, 4, DamageType::Bludgeoning},
                          DamageRoll{4, 8, 4, DamageType::Bludgeoning}},
                         0,
                         "test"};
    assert(damagePacketMin(twoPart) == 16);
    assert(damagePacketMax(twoPart) == 72);
    assert(damageFormula(twoPart).find("4d8 + 4 Bludgeoning + 4d8 + 4 Bludgeoning") != std::string::npos);

    DamagePacket radiant{{DamageRoll{5, 6, 0, DamageType::Radiant}}, 0, "test"};
    assert(damagePacketMin(radiant) == 5);
    assert(damagePacketMax(radiant) == 30);
    assert(damageFormula(radiant).find("5d6 Radiant") != std::string::npos);

    const UnitSpec mindFlayer{UnitType::NeutralMindFlayer};
    DamagePacket extractBrain = abilityDamagePacketFor(mindFlayer, AbilityKind::ExtractBrain);
    assert(damagePacketMin(extractBrain) == 11);
    assert(damagePacketMax(extractBrain) == 101);
    assert(damageFormula(extractBrain).find("10d10 + 1 Piercing") != std::string::npos);

    const UnitSpec guardian{UnitType::NeutralGuardianOfFaith};
    DamagePacket strike = abilityDamagePacketFor(guardian, AbilityKind::StrikeOfTheGuardian);
    assert(damagePacketMin(strike) == 40);
    assert(damagePacketMax(strike) == 40);
    assert(damageFormula(strike).find("20 Radiant + 20 Radiant") != std::string::npos);

    const UnitSpec minotaur{UnitType::NeutralMinotaur};
    DamagePacket charge = abilityDamagePacketFor(minotaur, AbilityKind::MinotaurCharge);
    assert(damagePacketMin(charge) == 8);
    assert(damagePacketMax(charge) == 36);
    assert(damageFormula(charge).find("4d8 + 4 Piercing") != std::string::npos);

    UnitSpec deathKnight{UnitType::NeutralDeathKnight};
    deathKnight.attack = 26;
    DamagePacket smite = abilityDamagePacketFor(deathKnight, AbilityKind::StaggeringSmite);
    assert(damageFormula(smite).find("Psychic") != std::string::npos);
    assert(damagePacketMin(smite) >= damagePacketMin(basicDamagePacketFor(deathKnight)) + 4);
    assert(damagePacketMax(smite) >= damagePacketMax(basicDamagePacketFor(deathKnight)) + 24);

    const UnitSpec airMyrmidon{UnitType::NeutralAirMyrmidon};
    DamagePacket flail = abilityDamagePacketFor(airMyrmidon, AbilityKind::ElectrifiedFlail);
    assert(damagePacketMin(flail) == 10);
    assert(damagePacketMax(flail) == 33);
    assert(damageFormula(flail).find("1d8 + 7 Bludgeoning + 1d8 Lightning + 1d10 Lightning") != std::string::npos);
}

void test_damage_affinities_are_unit_specific() {
    assert(damageAffinity(UnitType::Skeleton, DamageType::Piercing) == DamageAffinity::Resistant);
    assert(damageAffinity(UnitType::Skeleton, DamageType::Bludgeoning) == DamageAffinity::Vulnerable);
    assert(damageAffinity(UnitType::Skeleton, DamageType::Poison) == DamageAffinity::Immune);
    assert(damageAffinity(UnitType::Treant, DamageType::Fire) == DamageAffinity::Vulnerable);
    assert(damageAffinity(UnitType::Treant, DamageType::Piercing) == DamageAffinity::Resistant);
    assert(damageAffinity(UnitType::ShieldGuardian, DamageType::Slashing) == DamageAffinity::Resistant);
    assert(damageAffinity(UnitType::NeutralKethericThorm, DamageType::Radiant) == DamageAffinity::Vulnerable);
    assert(damageAffinity(UnitType::NeutralKethericThorm, DamageType::Necrotic) == DamageAffinity::Resistant);
    assert(damageAffinity(UnitType::NeutralDeathKnight, DamageType::Radiant) == DamageAffinity::Vulnerable);
    assert(damageAffinity(UnitType::NeutralAirMyrmidon, DamageType::Lightning) == DamageAffinity::Resistant);
    assert(damageAffinity(UnitType::NeutralAirMyrmidon, DamageType::Poison) == DamageAffinity::Immune);
    assert(damageAffinity(UnitType::Ranger, DamageType::Piercing) == DamageAffinity::Normal);

    std::string skeletonSummary = damageAffinitySummary(UnitType::Skeleton);
    assert(skeletonSummary.find("Resists Piercing") != std::string::npos);
    assert(skeletonSummary.find("Vulnerable Bludgeoning") != std::string::npos);
    assert(skeletonSummary.find("Immune Poison") != std::string::npos);
}

void test_damage_affinity_modifies_combat_damage_events() {
    {
        GameEngine engine(301);
        engine.startNewGame(GameMode::TwoPlayer);
        setupExplorationCombat(engine);

        std::array<Coord, 2> coords = adjacentOpenCoords(engine.snapshot(), Coord{kBoardWidth / 2, kBoardHeight / 2});
        assert(engine.debugCreateUnit(PlayerId::One, UnitType::Ranger, coords[0]) != kInvalidUnitId);
        UnitId skeleton = engine.debugCreateUnit(PlayerId::Two, UnitType::Skeleton, coords[1]);
        assert(skeleton != kInvalidUnitId);
        engine.setReady(PlayerId::One, true);
        engine.setReady(PlayerId::Two, true);

        bool sawPiercingResistance = false;
        for (int i = 0; i < 30 * 20 && !sawPiercingResistance && engine.snapshot().phase == Phase::Combat; ++i) {
            engine.tick(1.0 / 30.0);
            for (const Event& event : engine.consumeEvents()) {
                if (event.type == EventType::DamageDealt && event.target == skeleton &&
                    event.text.find("resistant to Piercing") != std::string::npos) {
                    sawPiercingResistance = true;
                }
            }
        }
        assert(sawPiercingResistance);
    }

    {
        GameEngine engine(302);
        engine.startNewGame(GameMode::TwoPlayer);
        setupExplorationCombat(engine);

        std::array<Coord, 2> coords = adjacentOpenCoords(engine.snapshot(), Coord{kBoardWidth / 2, kBoardHeight / 2});
        assert(engine.debugCreateUnit(PlayerId::One, UnitType::Paladin, coords[0]) != kInvalidUnitId);
        UnitId skeleton = engine.debugCreateUnit(PlayerId::Two, UnitType::Skeleton, coords[1]);
        assert(skeleton != kInvalidUnitId);
        engine.setReady(PlayerId::One, true);
        engine.setReady(PlayerId::Two, true);

        bool sawBludgeoningVulnerability = false;
        for (int i = 0; i < 30 * 20 && !sawBludgeoningVulnerability && engine.snapshot().phase == Phase::Combat; ++i) {
            engine.tick(1.0 / 30.0);
            for (const Event& event : engine.consumeEvents()) {
                if (event.type == EventType::DamageDealt && event.target == skeleton &&
                    event.text.find("vulnerable to Bludgeoning") != std::string::npos) {
                    sawBludgeoningVulnerability = true;
                }
            }
        }
        assert(sawBludgeoningVulnerability);
    }
}

void test_attack_events_resolve_without_exposing_dice_math() {
    GameEngine engine(30);
    engine.startNewGame(GameMode::TwoPlayer);
    setupExplorationCombat(engine);

    std::array<Coord, 2> coords = adjacentOpenCoords(engine.snapshot(), Coord{kBoardWidth / 2, kBoardHeight / 2});
    assert(engine.debugCreateUnit(PlayerId::One, UnitType::RogueAssassin, coords[0]) != kInvalidUnitId);
    assert(engine.debugCreateUnit(PlayerId::Two, UnitType::Cleric, coords[1]) != kInvalidUnitId);

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);
    advance(engine, 1.4);

    bool sawAttack = false;
    bool exposedDiceMath = false;
    for (const Event& event : engine.consumeEvents()) {
        if (event.type == EventType::UnitAttacked) {
            sawAttack = true;
            exposedDiceMath = exposedDiceMath ||
                              event.text.find("d20") != std::string::npos ||
                              event.text.find("AC") != std::string::npos;
        }
    }
    assert(sawAttack);
    assert(!exposedDiceMath);
}

void test_berserker_no_longer_skips_first_swing() {
    GameEngine engine(63);
    engine.startNewGame(GameMode::TwoPlayer);
    setupExplorationCombat(engine);

    std::array<Coord, 2> coords = adjacentOpenCoords(engine.snapshot(), Coord{kBoardWidth / 2, kBoardHeight / 2});
    UnitId berserker = engine.debugCreateUnit(PlayerId::One, UnitType::Barbarian, coords[0]);
    assert(berserker != kInvalidUnitId);
    assert(engine.debugCreateUnit(PlayerId::Two, UnitType::ShieldGuardian, coords[1]) != kInvalidUnitId);

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    bool sawAttackEvent = false;
    bool sawOldReadiedTurn = false;
    bool sawRageReduction = false;
    bool sawFrenziedStrike = false;
    for (int i = 0; i < 30 * 24 && !(sawAttackEvent && sawRageReduction && sawFrenziedStrike); ++i) {
        engine.tick(1.0 / 30.0);
        for (const Event& event : engine.consumeEvents()) {
            if (event.actor == berserker && event.type == EventType::UnitAttacked) {
                sawAttackEvent = true;
                if (event.text.find("readied a reckless swing") != std::string::npos) {
                    sawOldReadiedTurn = true;
                }
                if (event.text.find("Frenzied Strike") != std::string::npos) {
                    sawFrenziedStrike = true;
                }
            }
            if (event.target == berserker && event.text.find("Rage") != std::string::npos) {
                sawRageReduction = true;
            }
        }
    }

    assert(sawAttackEvent);
    assert(!sawOldReadiedTurn);
    assert(sawRageReduction);
    assert(sawFrenziedStrike);
}

void test_spell_events_resolve_saves_without_exposing_dice_math() {
    GameEngine engine(31);
    engine.startNewGame(GameMode::TwoPlayer);
    setupExplorationCombat(engine);

    UnitId caster = buyAndDeploy(engine, PlayerId::One, UnitType::DragonWyrmling, p1MainDeploy());
    buyAndDeploy(engine, PlayerId::Two, UnitType::ShieldGuardian, p2MainDeploy());

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    bool sawSave = false;
    bool exposedDiceMath = false;
    for (int i = 0; i < 30 * 18 && !sawSave && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
        for (const Event& event : engine.consumeEvents()) {
            if (event.actor == caster &&
                event.text.find("save") != std::string::npos &&
                event.text.find("DC") != std::string::npos) {
                sawSave = true;
                exposedDiceMath = exposedDiceMath ||
                                  event.text.find("d20") != std::string::npos ||
                                  event.text.find(" = ") != std::string::npos;
            }
        }
    }
    assert(sawSave);
    assert(!exposedDiceMath);
}

void test_tamia_blight_uses_hidden_8d8_necrotic_damage() {
    GameEngine engine(300);
    engine.startNewGame(GameMode::TwoPlayer);
    const UnitSpec* tamia = engine.specFor(UnitType::NeutralTamiaHolzt);
    assert(tamia);
    DamagePacket packet = abilityDamagePacketFor(*tamia, AbilityKind::Blight);
    assert(damagePacketMin(packet) == 8);
    assert(damagePacketMax(packet) == 64);
    assert(damageFormula(packet).find("8d8 Necrotic") != std::string::npos);
    assert(damageLine(packet).find("Damage: 8~64") != std::string::npos);
}

void test_tamia_dominate_creates_neutral_controlled_hostile() {
    bool verified = false;
    for (unsigned seed = 522; seed < 560 && !verified; ++seed) {
        GameEngine engine(seed);
        engine.startNewGame(GameMode::TwoPlayer);
        setupExplorationCombat(engine);

        std::vector<Coord> coords = horizontalOpenRun(engine.snapshot(), Coord{kBoardWidth / 2, kBoardHeight / 2}, 4);
        UnitId aiUnit = engine.debugCreateUnit(PlayerId::Two, UnitType::Skeleton, coords[0]);
        UnitId tamia = engine.debugCreateUnit(PlayerId::Two, UnitType::NeutralTamiaHolzt, coords[1]);
        UnitId victim = engine.debugCreateUnit(PlayerId::One, UnitType::GoblinSkirmisher, coords[2]);
        UnitId playerUnit = engine.debugCreateUnit(PlayerId::One, UnitType::Skeleton, coords[3]);
        assert(tamia != kInvalidUnitId);
        assert(victim != kInvalidUnitId);
        assert(playerUnit != kInvalidUnitId);
        assert(aiUnit != kInvalidUnitId);
        assert(engine.debugApplyDamageFrom(victim, tamia, 1, DamageType::Piercing));

        engine.setReady(PlayerId::One, true);
        engine.setReady(PlayerId::Two, true);

        bool sawDominate = false;
        bool dominatedAttackedPlayer = false;
        bool dominatedAttackedAi = false;
        for (int i = 0; i < 30 * 18 && engine.snapshot().phase == Phase::Combat; ++i) {
            engine.tick(1.0 / 30.0);
            for (const Event& event : engine.consumeEvents()) {
                if (event.target == victim &&
                    event.text.find("turned hostile to both parties") != std::string::npos) {
                    sawDominate = true;
                }
                if (event.actor == victim && event.target == playerUnit &&
                    event.type == EventType::UnitAttacked) {
                    dominatedAttackedPlayer = true;
                }
                if (event.actor == victim && event.target == aiUnit &&
                    event.type == EventType::UnitAttacked) {
                    dominatedAttackedAi = true;
                }
            }
            if (sawDominate && (dominatedAttackedPlayer || dominatedAttackedAi)) break;
        }
        verified = sawDominate && (dominatedAttackedPlayer || dominatedAttackedAi);
    }

    assert(verified);
}

void test_mind_blast_stuns_multiple_targets_and_extracts_stunned() {
    GameEngine engine(501);
    engine.startNewGame(GameMode::TwoPlayer);
    setupExplorationCombat(engine);

    std::vector<Coord> coords = horizontalOpenRun(engine.snapshot(), Coord{kBoardWidth / 2, kBoardHeight / 2}, 5);
    UnitId mindFlayer = engine.debugCreateUnit(PlayerId::One, UnitType::NeutralMindFlayer, coords[0]);
    UnitId front = engine.debugCreateUnit(PlayerId::Two, UnitType::Skeleton, coords[3]);
    UnitId clustered = engine.debugCreateUnit(PlayerId::Two, UnitType::Skeleton, coords[4]);
    assert(mindFlayer != kInvalidUnitId);
    assert(front != kInvalidUnitId);
    assert(clustered != kInvalidUnitId);
    assert(engine.debugApplyDamageFrom(front, mindFlayer, 1, DamageType::Piercing));

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    bool sawMindBlast = false;
    bool sawClusterStun = false;
    bool sawExtract = false;
    for (int i = 0; i < 30 * 20 && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
        GameSnapshot snapshot = engine.snapshot();
        const UnitView* frontView = findUnit(snapshot, front);
        for (const Event& event : engine.consumeEvents()) {
            if (event.actor == mindFlayer && event.text.find("Mind Blast") != std::string::npos) {
                sawMindBlast = true;
            }
            if (event.target == clustered && event.text.find("became Stunned") != std::string::npos) {
                sawClusterStun = true;
            }
            if (event.actor == mindFlayer && event.text.find("Extract Brain") != std::string::npos) {
                sawExtract = true;
            }
        }
        if (frontView && !frontView->alive && sawMindBlast && sawClusterStun && sawExtract) break;
    }

    GameSnapshot snapshot = engine.snapshot();
    const UnitView* frontView = findUnit(snapshot, front);
    assert(sawMindBlast);
    assert(sawClusterStun);
    assert(sawExtract);
    assert(!frontView || !frontView->alive);
}

void test_ketheric_wrathful_smite_can_frighten_target() {
    bool sawWrathfulAny = false;
    bool sawWisSaveAny = false;
    bool sawFrightenedAny = false;
    for (unsigned seed = 502; seed < 540 && !sawFrightenedAny; ++seed) {
        GameEngine engine(seed);
        engine.startNewGame(GameMode::TwoPlayer);
        setupExplorationCombat(engine);

        std::array<Coord, 2> coords = adjacentOpenCoords(engine.snapshot(), Coord{kBoardWidth / 2, kBoardHeight / 2});
        UnitId ketheric = engine.debugCreateUnit(PlayerId::One, UnitType::NeutralKethericThorm, coords[0]);
        UnitId target = engine.debugCreateUnit(PlayerId::Two, UnitType::Skeleton, coords[1]);
        assert(ketheric != kInvalidUnitId);
        assert(target != kInvalidUnitId);
        assert(engine.debugApplyDamageFrom(target, ketheric, 1, DamageType::Piercing));
        engine.setReady(PlayerId::One, true);
        engine.setReady(PlayerId::Two, true);

        for (int i = 0; i < 30 * 12 && engine.snapshot().phase == Phase::Combat; ++i) {
            engine.tick(1.0 / 30.0);
            for (const Event& event : engine.consumeEvents()) {
                if (event.actor == ketheric && event.text.find("Wrathful Smite") != std::string::npos) {
                    sawWrathfulAny = true;
                }
                if (event.target == target && event.text.find("became Frightened") != std::string::npos) {
                    sawFrightenedAny = true;
                }
                if (event.target == target &&
                    event.text.find("Wis save") != std::string::npos &&
                    event.text.find("Ketheric Thorm") != std::string::npos) {
                    sawWisSaveAny = true;
                }
            }
            if (sawFrightenedAny) break;
        }
    }

    assert(sawWrathfulAny);
    assert(sawWisSaveAny);
    assert(sawFrightenedAny);
}

void test_ketheric_wrathful_smite_repels_nearby_enemies() {
    GameEngine engine(509);
    engine.startNewGame(GameMode::TwoPlayer);
    setupExplorationCombat(engine);

    std::vector<Coord> coords = horizontalOpenRun(engine.snapshot(), Coord{kBoardWidth / 2, kBoardHeight / 2}, 4);
    UnitId ketheric = engine.debugCreateUnit(PlayerId::One, UnitType::NeutralKethericThorm, coords[0]);
    UnitId target = engine.debugCreateUnit(PlayerId::Two, UnitType::GoblinSkirmisher, coords[1]);
    UnitId behind = engine.debugCreateUnit(PlayerId::Two, UnitType::Skeleton, coords[2]);
    assert(ketheric != kInvalidUnitId);
    assert(target != kInvalidUnitId);
    assert(behind != kInvalidUnitId);
    assert(engine.debugApplyDamageFrom(target, ketheric, 1, DamageType::Piercing));

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    bool sawKethericKnockback = false;
    for (int i = 0; i < 30 * 18 && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
        for (const Event& event : engine.consumeEvents()) {
            if (event.actor == ketheric &&
                event.text.find("Ketheric's Smite") != std::string::npos &&
                event.text.find("knocked back") != std::string::npos) {
                sawKethericKnockback = true;
            }
        }
        if (sawKethericKnockback) break;
    }

    GameSnapshot snapshot = engine.snapshot();
    assert(sawKethericKnockback);
    assert(landOccupantsAt(snapshot, coords[1]) <= 1);
    assert(landOccupantsAt(snapshot, coords[2]) <= 1);
    assert(landOccupantsAt(snapshot, coords[3]) <= 1);
}

void test_minotaur_charge_knocks_back_and_prones_line_targets() {
    GameEngine engine(503);
    engine.startNewGame(GameMode::TwoPlayer);
    setupExplorationCombat(engine);

    std::vector<Coord> coords = horizontalOpenRun(engine.snapshot(), Coord{kBoardWidth / 2, kBoardHeight / 2}, 5);
    UnitId minotaur = engine.debugCreateUnit(PlayerId::One, UnitType::NeutralMinotaur, coords[0]);
    UnitId first = engine.debugCreateUnit(PlayerId::Two, UnitType::Skeleton, coords[2]);
    UnitId second = engine.debugCreateUnit(PlayerId::Two, UnitType::Skeleton, coords[3]);
    assert(minotaur != kInvalidUnitId);
    assert(first != kInvalidUnitId);
    assert(second != kInvalidUnitId);
    assert(engine.debugApplyDamageFrom(first, minotaur, 1, DamageType::Piercing));

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    bool sawCharge = false;
    bool sawKnockback = false;
    bool sawProne = false;
    for (int i = 0; i < 30 * 12 && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
        for (const Event& event : engine.consumeEvents()) {
            if (event.actor == minotaur && event.text.find("used Charge") != std::string::npos) {
                sawCharge = true;
            }
            if (event.actor == minotaur && event.text.find("knocked back") != std::string::npos) {
                sawKnockback = true;
            }
            if ((event.target == first || event.target == second) &&
                event.text.find("became Prone") != std::string::npos) {
                sawProne = true;
            }
        }
        if (sawCharge && sawKnockback && sawProne) break;
    }

    assert(sawCharge);
    assert(sawKnockback);
    assert(sawProne);
}

void test_owlbear_multiattack_knocks_prone_target_back() {
    GameEngine engine(510);
    engine.startNewGame(GameMode::TwoPlayer);
    setupExplorationCombat(engine);

    std::vector<Coord> coords = horizontalOpenRun(engine.snapshot(), Coord{kBoardWidth / 2, kBoardHeight / 2}, 4);
    UnitId owlbear = engine.debugCreateUnit(PlayerId::One, UnitType::NeutralOwlbear, coords[0]);
    UnitId target = engine.debugCreateUnit(PlayerId::Two, UnitType::ShieldGuardian, coords[1]);
    UnitId behind = engine.debugCreateUnit(PlayerId::Two, UnitType::Skeleton, coords[2]);
    assert(owlbear != kInvalidUnitId);
    assert(target != kInvalidUnitId);
    assert(behind != kInvalidUnitId);
    assert(engine.debugApplyDamageFrom(target, owlbear, 1, DamageType::Piercing));

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    bool sawOwlbearKnockback = false;
    bool sawProne = false;
    for (int i = 0; i < 30 * 10 && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
        for (const Event& event : engine.consumeEvents()) {
            if (event.actor == owlbear &&
                event.text.find("Multiattack") != std::string::npos &&
                event.text.find("knocked back") != std::string::npos) {
                sawOwlbearKnockback = true;
            }
            if (event.target == target && event.text.find("became Prone") != std::string::npos) {
                sawProne = true;
            }
        }
        if (sawOwlbearKnockback && sawProne) break;
    }

    GameSnapshot snapshot = engine.snapshot();
    const UnitView* targetView = findUnit(snapshot, target);
    const UnitView* behindView = findUnit(snapshot, behind);
    assert(sawOwlbearKnockback);
    assert(sawProne);
    assert(targetView);
    assert(behindView);
    assert(targetView->coord.x >= 16);
    assert(behindView->coord.x >= 17);
}

void test_raphael_diabolic_chains_can_shove_failed_saves() {
    GameEngine engine(511);
    engine.startNewGame(GameMode::TwoPlayer);
    setupExplorationCombat(engine);

    std::vector<Coord> coords = horizontalOpenRun(engine.snapshot(), Coord{kBoardWidth / 2, kBoardHeight / 2}, 5);
    UnitId raphael = engine.debugCreateUnit(PlayerId::One, UnitType::NeutralRaphael, coords[0]);
    UnitId target = engine.debugCreateUnit(PlayerId::Two, UnitType::GoblinSkirmisher, coords[3]);
    UnitId behind = engine.debugCreateUnit(PlayerId::Two, UnitType::Skeleton, coords[4]);
    assert(raphael != kInvalidUnitId);
    assert(target != kInvalidUnitId);
    assert(behind != kInvalidUnitId);
    assert(engine.debugApplyDamageFrom(target, raphael, 1, DamageType::Piercing));

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    bool sawChainKnockback = false;
    Coord knockbackFrom{};
    Coord knockbackTo{};
    for (int i = 0; i < 30 * 16 && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
        for (const Event& event : engine.consumeEvents()) {
            if (event.actor == raphael &&
                event.text.find("Diabolic Chains") != std::string::npos &&
                event.text.find("knocked back") != std::string::npos) {
                sawChainKnockback = true;
                knockbackFrom = event.from;
                knockbackTo = event.to;
            }
        }
        if (sawChainKnockback) break;
    }

    GameSnapshot snapshot = engine.snapshot();
    assert(sawChainKnockback);
    assert(manhattan(knockbackFrom, knockbackTo) >= 1);
    for (Coord coord : coords) {
        assert(landOccupantsAt(snapshot, coord) <= 1);
    }
}

void test_knockback_pushes_land_units_domino_style() {
    GameEngine engine(504);
    engine.startNewGame(GameMode::TwoPlayer);
    setupExplorationCombat(engine);

    std::vector<Coord> coords = horizontalOpenRun(engine.snapshot(), Coord{kBoardWidth / 2, kBoardHeight / 2}, 4);
    UnitId first = engine.debugCreateUnit(PlayerId::Two, UnitType::Skeleton, coords[1]);
    UnitId second = engine.debugCreateUnit(PlayerId::Two, UnitType::GoblinSkirmisher, coords[2]);
    assert(first != kInvalidUnitId);
    assert(second != kInvalidUnitId);

    assert(engine.debugKnockback(first, coords[0], 1));
    GameSnapshot snapshot = engine.snapshot();
    const UnitView* firstView = findUnit(snapshot, first);
    const UnitView* secondView = findUnit(snapshot, second);
    assert(firstView);
    assert(secondView);
    assert((firstView->coord == coords[2]));
    assert((secondView->coord == coords[3]));

    assert(landOccupantsAt(snapshot, coords[2]) == 1);
    assert(landOccupantsAt(snapshot, coords[3]) == 1);
}

void test_knockback_stops_at_wall_without_land_overlap() {
    GameEngine engine(505);
    engine.startNewGame(GameMode::TwoPlayer);
    setupExplorationCombat(engine);

    UnitId first = engine.debugCreateUnit(PlayerId::Two, UnitType::Skeleton, Coord{31, 9});
    UnitId second = engine.debugCreateUnit(PlayerId::Two, UnitType::GoblinSkirmisher, Coord{32, 9});
    assert(first != kInvalidUnitId);
    assert(second != kInvalidUnitId);

    assert(!engine.debugKnockback(first, Coord{30, 9}, 1));
    GameSnapshot snapshot = engine.snapshot();
    const UnitView* firstView = findUnit(snapshot, first);
    const UnitView* secondView = findUnit(snapshot, second);
    assert(firstView);
    assert(secondView);
    assert((firstView->coord == Coord{31, 9}));
    assert((secondView->coord == Coord{32, 9}));

    assert(landOccupantsAt(snapshot, Coord{32, 9}) == 1);
}

void test_knockback_ignores_air_units_on_destination() {
    GameEngine engine(506);
    engine.startNewGame(GameMode::TwoPlayer);
    setupExplorationCombat(engine);

    std::vector<Coord> coords = horizontalOpenRun(engine.snapshot(), Coord{kBoardWidth / 2, kBoardHeight / 2}, 3);
    UnitId land = engine.debugCreateUnit(PlayerId::Two, UnitType::Skeleton, coords[1]);
    UnitId airOne = engine.debugCreateUnit(PlayerId::Two, UnitType::FireMephit, coords[2]);
    UnitId airTwo = engine.debugCreateUnit(PlayerId::Two, UnitType::DragonWyrmling, coords[2]);
    assert(land != kInvalidUnitId);
    assert(airOne != kInvalidUnitId);
    assert(airTwo != kInvalidUnitId);

    assert(engine.debugKnockback(land, coords[0], 1));
    GameSnapshot snapshot = engine.snapshot();
    const UnitView* landView = findUnit(snapshot, land);
    const UnitView* airOneView = findUnit(snapshot, airOne);
    const UnitView* airTwoView = findUnit(snapshot, airTwo);
    assert(landView);
    assert(airOneView);
    assert(airTwoView);
    assert((landView->coord == coords[2]));
    assert((airOneView->coord == coords[2]));
    assert((airTwoView->coord == coords[2]));

    int landAt15 = 0;
    int airAt15 = 0;
    for (const UnitView& unit : snapshot.units) {
        if (!unit.alive || !unit.deployed || unit.coord != coords[2]) continue;
        if (unit.layer == UnitLayer::Land) ++landAt15;
        if (unit.layer == UnitLayer::Air) ++airAt15;
    }
    assert(landAt15 == 1);
    assert(airAt15 == 2);
}

void test_boss_units_resist_standard_knockback() {
    GameEngine engine(507);
    engine.startNewGame(GameMode::TwoPlayer);
    setupExplorationCombat(engine);

    std::array<Coord, 2> coords = adjacentOpenCoords(engine.snapshot(), Coord{kBoardWidth / 2, kBoardHeight / 2});
    UnitId boss = engine.debugCreateUnit(PlayerId::Two, UnitType::NeutralKethericThorm, coords[1]);
    assert(boss != kInvalidUnitId);
    assert(!engine.debugKnockback(boss, coords[0], 1));

    GameSnapshot snapshot = engine.snapshot();
    const UnitView* bossView = findUnit(snapshot, boss);
    assert(bossView);
    assert((bossView->coord == coords[1]));
}

void test_radial_knockback_pushes_hostile_units_away_from_center() {
    GameEngine engine(508);
    engine.startNewGame(GameMode::TwoPlayer);
    setupExplorationCombat(engine);

    std::vector<Coord> coords = horizontalOpenRun(engine.snapshot(), Coord{kBoardWidth / 2, kBoardHeight / 2}, 5);
    UnitId ally = engine.debugCreateUnit(PlayerId::One, UnitType::Skeleton, coords[0]);
    UnitId source = engine.debugCreateUnit(PlayerId::One, UnitType::Paladin, coords[1]);
    UnitId rightFront = engine.debugCreateUnit(PlayerId::Two, UnitType::Skeleton, coords[2]);
    UnitId rightBack = engine.debugCreateUnit(PlayerId::Two, UnitType::GoblinSkirmisher, coords[3]);
    assert(source != kInvalidUnitId);
    assert(rightFront != kInvalidUnitId);
    assert(rightBack != kInvalidUnitId);
    assert(ally != kInvalidUnitId);

    assert(engine.debugRadialKnockback(coords[1], 1, 1, source));
    GameSnapshot snapshot = engine.snapshot();
    const UnitView* rightFrontView = findUnit(snapshot, rightFront);
    const UnitView* rightBackView = findUnit(snapshot, rightBack);
    const UnitView* allyView = findUnit(snapshot, ally);
    assert(rightFrontView);
    assert(rightBackView);
    assert(allyView);
    assert((rightFrontView->coord == coords[3]));
    assert((rightBackView->coord == coords[4]));
    assert((allyView->coord == coords[0]));
}

void test_ai_actions_are_generated_and_apply_through_rules() {
    GameEngine engine(15);
    setupAiActionState(engine);
    std::vector<AiAction> actions = engine.legalActions(PlayerId::One);
    assert(!actions.empty());

    bool sawBuy = false;
    bool sawDeploy = false;
    bool sawMove = false;
    bool sawReturn = false;
    bool sawUpgrade = false;
    bool sawReady = false;
    for (const AiAction& action : actions) {
        sawBuy = sawBuy || action.kind == AiActionKind::Buy;
        sawDeploy = sawDeploy || action.kind == AiActionKind::Deploy;
        sawMove = sawMove || action.kind == AiActionKind::MoveDeployed;
        sawReturn = sawReturn || action.kind == AiActionKind::ReturnToBench;
        sawUpgrade = sawUpgrade || action.kind == AiActionKind::Upgrade;
        sawReady = sawReady || action.kind == AiActionKind::Ready;

        GameEngine probe(15);
        setupAiActionState(probe);
        assert(probe.applyAiAction(PlayerId::One, action));
    }

    assert(sawBuy);
    assert(sawDeploy);
    assert(sawMove);
    assert(sawReturn);
    assert(!sawUpgrade);
    assert(sawReady);
}

void test_feature_schema_matches_exported_features() {
    GameEngine engine(16);
    engine.startNewGame(GameMode::SinglePlayerVsAi);
    AiFeatureSchema schema = engine.aiFeatureSchema();
    assert(schema.stateFeatureCount > 0);
    assert(schema.actionFeatureCount > 0);
    assert(static_cast<int>(engine.stateFeatures(PlayerId::One).size()) == schema.stateFeatureCount);
    std::vector<AiAction> actions = engine.legalActions(PlayerId::One);
    assert(!actions.empty());
    assert(static_cast<int>(engine.actionFeatures(PlayerId::One, actions.front()).size()) ==
           schema.actionFeatureCount);
    assert(!engine.rulesFingerprint().empty());
}

void test_normal_ai_uses_built_in_strategy_without_policy_file() {
    GameEngine engine(22);
    GameConfig config;
    config.mode = GameMode::SinglePlayerVsAi;
    config.aiDifficulty = AiDifficulty::Normal;
    config.aiPolicyDirectory = "missing-ai-policies";
    engine.startNewGame(config);

    assert(!engine.aiPolicyMetadata().loaded);
    assert(engine.aiPolicyMetadata().valid);
    assert(engine.aiPolicyMetadata().path == "built-in");
    assert(engine.aiPolicyMetadata().status.find("built-in normal strategy") != std::string::npos);

    buyAndDeploy(engine, PlayerId::One, UnitType::ShieldGuardian, p1MainDeploy());
    engine.setReady(PlayerId::One, true);
    GameSnapshot snapshot = engine.snapshot();
    assert(snapshot.phase == Phase::Combat);

    bool sawAiCombatUnit = false;
    for (const UnitView& unit : snapshot.units) {
        if (unit.owner == PlayerId::Two && unit.alive && unit.deployed &&
            !isInternalUnit(unit.type)) {
            sawAiCombatUnit = true;
        }
    }
    assert(sawAiCombatUnit);
}

void test_normal_ai_round_one_opener_stays_readable() {
    GameEngine engine(28);
    GameConfig config;
    config.mode = GameMode::SinglePlayerVsAi;
    config.aiDifficulty = AiDifficulty::Normal;
    engine.startNewGame(config);

    buyAndDeploy(engine, PlayerId::One, UnitType::ShieldGuardian, p1MainDeploy());
    engine.setReady(PlayerId::One, true);

    GameSnapshot snapshot = engine.snapshot();
    int aiCombatUnits = 0;
    int aiArmyCost = 0;
    for (const UnitView& unit : snapshot.units) {
        if (unit.owner != PlayerId::Two || !unit.alive || !unit.deployed ||
            isInternalUnit(unit.type)) {
            continue;
        }
        ++aiCombatUnits;
        aiArmyCost += unit.cost;
    }

    assert(aiCombatUnits >= 3);
    assert(aiCombatUnits <= 4);
    assert(aiArmyCost >= 30);
    assert(aiArmyCost <= 40);
}

void test_normal_ai_exploration_opener_covers_center_and_wing() {
    GameEngine engine(128);
    GameConfig config;
    config.mode = GameMode::SinglePlayerVsAi;
    config.aiDifficulty = AiDifficulty::Normal;
    engine.startNewGame(config);

    buyAndDeploy(engine, PlayerId::One, UnitType::ShieldGuardian, p1MainDeploy());
    engine.setReady(PlayerId::One, true);

    GameSnapshot snapshot = engine.snapshot();
    int centerUnits = 0;
    int wingUnits = 0;
    bool hasTank = false;
    bool hasRanged = false;
    bool hasSupport = false;
    for (const UnitView& unit : snapshot.units) {
        if (unit.owner != PlayerId::Two || !unit.alive || !unit.deployed ||
            isInternalUnit(unit.type)) {
            continue;
        }
        if (unit.coord.y >= 3 && unit.coord.y <= kBoardHeight - 4) ++centerUnits;
        if (unit.coord.y <= 1 || unit.coord.y >= kBoardHeight - 2) ++wingUnits;
        hasTank = hasTank || unit.type == UnitType::ShieldGuardian;
        hasRanged = hasRanged || unit.type == UnitType::Ranger;
        hasSupport = hasSupport || unit.type == UnitType::Cleric;
    }

    assert(centerUnits >= 1);
    assert(wingUnits >= 1);
    assert(hasTank);
    assert(hasRanged);
    assert(hasSupport);
}

void test_normal_ai_buys_air_answer_when_player_fields_air() {
    GameEngine engine(129);
    GameConfig config;
    config.mode = GameMode::SinglePlayerVsAi;
    config.aiDifficulty = AiDifficulty::Normal;
    engine.startNewGame(config);

    buyAndDeploy(engine, PlayerId::One, UnitType::DragonWyrmling, p1MainDeploy());
    engine.setReady(PlayerId::One, true);

    GameSnapshot snapshot = engine.snapshot();
    bool hasAirAnswer = false;
    for (const UnitView& unit : snapshot.units) {
        if (unit.owner != PlayerId::Two || !unit.alive || !unit.deployed ||
            isInternalUnit(unit.type)) {
            continue;
        }
        hasAirAnswer = hasAirAnswer || unit.type == UnitType::Ranger ||
                       unit.type == UnitType::ImpSwarm ||
                       unit.type == UnitType::DragonWyrmling ||
                       unit.type == UnitType::Evoker ||
                       unit.type == UnitType::Cleric;
    }

    assert(hasAirAnswer);
}

void test_missing_policy_falls_back_to_heuristic_ai() {
    GameEngine engine(17);
    GameConfig config;
    config.mode = GameMode::SinglePlayerVsAi;
    config.aiDifficulty = AiDifficulty::Hard;
    config.aiPolicyDirectory = "missing-ai-policies";
    engine.startNewGame(config);
    assert(!engine.aiPolicyMetadata().valid);
    assert(engine.aiPolicyMetadata().status.find("missing") != std::string::npos);

    buyAndDeploy(engine, PlayerId::One, UnitType::ShieldGuardian, p1MainDeploy());
    engine.setReady(PlayerId::One, true);
    assert(engine.snapshot().phase == Phase::Combat);
}

void test_stale_policy_falls_back_to_heuristic_ai() {
    std::filesystem::path directory = std::filesystem::path(".cache") / "test-ai-policies";

    GameEngine schemaEngine(18);
    schemaEngine.startNewGame(GameMode::SinglePlayerVsAi);
    AiFeatureSchema schema = schemaEngine.aiFeatureSchema();
    writePolicyFile(directory, "Hard", "stale", schema, 1.0);

    GameEngine engine(19);
    GameConfig config;
    config.mode = GameMode::SinglePlayerVsAi;
    config.aiDifficulty = AiDifficulty::Hard;
    config.aiPolicyDirectory = directory.string();
    engine.startNewGame(config);
    assert(!engine.aiPolicyMetadata().valid);
    assert(engine.aiPolicyMetadata().status.find("fingerprint") != std::string::npos);
}

void test_exported_policy_loads_when_rules_match() {
    std::filesystem::path policyDirectory = std::filesystem::path(".cache") / "test-ai-policies-current";
    GameEngine schemaEngine(20);
    schemaEngine.startNewGame(GameMode::SinglePlayerVsAi);
    AiFeatureSchema schema = schemaEngine.aiFeatureSchema();
    std::string fingerprint = schemaEngine.rulesFingerprint();
    writePolicyFile(policyDirectory, "Hard", fingerprint, schema, 1.2);
    writePolicyFile(policyDirectory, "SuperHard", fingerprint, schema, 3.0);

    GameEngine engine(21);
    GameConfig config;
    config.mode = GameMode::SinglePlayerVsAi;
    config.aiDifficulty = AiDifficulty::Hard;
    config.aiPolicyDirectory = policyDirectory.string();
    engine.startNewGame(config);
    assert(engine.aiPolicyMetadata().loaded);
    assert(engine.aiPolicyMetadata().valid);

    config.aiDifficulty = AiDifficulty::SuperHard;
    engine.startNewGame(config);
    assert(engine.aiPolicyMetadata().loaded);
    assert(engine.aiPolicyMetadata().valid);
}

void test_policy_ai_builds_roster_before_round_one_upgrades() {
    std::filesystem::path policyDirectory = std::filesystem::path(".cache") / "test-ai-policies-roster";
    GameEngine schemaEngine(26);
    schemaEngine.startNewGame(GameMode::SinglePlayerVsAi);
    AiFeatureSchema schema = schemaEngine.aiFeatureSchema();
    std::string fingerprint = schemaEngine.rulesFingerprint();
    writePolicyFile(policyDirectory, "SuperHard", fingerprint, schema, 3.0);

    GameEngine engine(27);
    GameConfig config;
    config.mode = GameMode::SinglePlayerVsAi;
    config.aiDifficulty = AiDifficulty::SuperHard;
    config.aiPolicyDirectory = policyDirectory.string();
    engine.startNewGame(config);

    buyAndDeploy(engine, PlayerId::One, UnitType::ShieldGuardian, p1MainDeploy());
    engine.setReady(PlayerId::One, true);

    GameSnapshot snapshot = engine.snapshot();
    assert(snapshot.phase == Phase::Combat);

    int aiCombatUnits = 0;
    int aiArmyCost = 0;
    bool upgradedBerserker = false;
    for (const UnitView& unit : snapshot.units) {
        if (unit.owner != PlayerId::Two || !unit.alive || !unit.deployed ||
            isInternalUnit(unit.type)) {
            continue;
        }
        ++aiCombatUnits;
        aiArmyCost += unit.cost;
        if (unit.type == UnitType::Barbarian && unit.upgraded) upgradedBerserker = true;
    }

    assert(aiCombatUnits >= 3);
    assert(aiArmyCost >= 34);
    assert(!upgradedBerserker);
}

void test_buy_and_deploy_rules() {
    GameEngine engine(1);
    engine.startNewGame(GameMode::TwoPlayer);

    assert(engine.buyUnit(PlayerId::One, UnitType::ShieldGuardian));
    UnitId guard = firstBenchUnit(engine, PlayerId::One);

    assert(!engine.deployUnit(PlayerId::One, guard, Coord{5, 3}));
    assert(engine.deployUnit(PlayerId::One, guard, Coord{2, 3}));
    assert(!engine.moveDeployedUnit(PlayerId::One, guard, Coord{10, 3}));
    assert(engine.moveDeployedUnit(PlayerId::One, guard, Coord{0, 3}));
    assert(engine.returnToBench(PlayerId::One, guard));
    assert(engine.deployUnit(PlayerId::One, guard, Coord{1, 4}));

    GameSnapshot snapshot = engine.snapshot();
    const UnitView* view = findUnit(snapshot, guard);
    assert((view && view->coord == Coord{1, 4}));
}

void test_exploration_deployment_uses_top_and_bottom_staging_rows() {
    GameEngine engine(515);
    engine.startNewGame(GameMode::TwoPlayer);

    assert(engine.canDeploy(PlayerId::One, Coord{5, 0}, UnitLayer::Land));
    assert(engine.canDeploy(PlayerId::One, Coord{5, kBoardHeight - 1}, UnitLayer::Land));
    assert(!engine.canDeploy(PlayerId::One, Coord{7, 0}, UnitLayer::Land));
    assert(!engine.canDeploy(PlayerId::One, Coord{8, kBoardHeight - 1}, UnitLayer::Land));
    assert(!engine.canDeploy(PlayerId::One, Coord{10, 0}, UnitLayer::Land));
    assert(!engine.canDeploy(PlayerId::One, Coord{20, kBoardHeight - 1}, UnitLayer::Land));
    assert(!engine.canDeploy(PlayerId::One, Coord{kBoardWidth - 2, 0}, UnitLayer::Land));

    assert(engine.canDeploy(PlayerId::Two, Coord{kBoardWidth - 6, 0}, UnitLayer::Land));
    assert(engine.canDeploy(PlayerId::Two, Coord{kBoardWidth - 6, kBoardHeight - 1}, UnitLayer::Land));
    assert(!engine.canDeploy(PlayerId::Two, Coord{kBoardWidth - 8, 0}, UnitLayer::Land));
    assert(!engine.canDeploy(PlayerId::Two, Coord{22, kBoardHeight - 1}, UnitLayer::Land));
    assert(!engine.canDeploy(PlayerId::Two, Coord{10, 0}, UnitLayer::Land));
    assert(!engine.canDeploy(PlayerId::Two, Coord{1, kBoardHeight - 1}, UnitLayer::Land));

    assert(engine.buyUnit(PlayerId::One, UnitType::Skeleton));
    UnitId topUnit = firstBenchUnit(engine, PlayerId::One);
    assert(engine.deployUnit(PlayerId::One, topUnit, Coord{5, 0}));

    assert(engine.buyUnit(PlayerId::Two, UnitType::Skeleton));
    UnitId bottomUnit = firstBenchUnit(engine, PlayerId::Two);
    assert(engine.deployUnit(PlayerId::Two, bottomUnit, Coord{kBoardWidth - 6, kBoardHeight - 1}));
}

void test_githyanki_opens_with_astral_raid() {
    GameEngine engine(23);
    engine.startNewGame(GameMode::TwoPlayer);
    setupExplorationCombat(engine);

    std::vector<Coord> coords = horizontalOpenRun(engine.snapshot(), Coord{kBoardWidth / 2, kBoardHeight / 2}, 4);
    UnitId githyanki = engine.debugCreateUnit(PlayerId::One, UnitType::GithyankiWarrior, coords[0]);
    assert(githyanki != kInvalidUnitId);
    assert(engine.debugCreateUnit(PlayerId::Two, UnitType::Cleric, coords[3]) != kInvalidUnitId);

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    GameSnapshot snapshot = engine.snapshot();
    const UnitView* view = findUnit(snapshot, githyanki);
    assert(view);
    assert(view->coord != coords[0]);
    assert(manhattan(view->coord, coords[3]) < manhattan(coords[0], coords[3]));

    bool sawAstralRaid = false;
    for (const Event& event : engine.consumeEvents()) {
        if (event.actor == githyanki && event.text.find("astral raid") != std::string::npos) {
            sawAstralRaid = true;
        }
    }
    assert(sawAstralRaid);
}

void test_assassin_uses_limited_range_ambush() {
    GameEngine engine(2);
    engine.startNewGame(GameMode::TwoPlayer);
    setupExplorationCombat(engine);

    UnitId assassin = buyAndDeploy(engine, PlayerId::One, UnitType::RogueAssassin, p1MainDeploy());
    UnitId farTarget = buyAndDeploy(engine, PlayerId::Two, UnitType::Cleric, p2MainDeploy());

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    GameSnapshot snapshot = engine.snapshot();
    assert(snapshot.phase == Phase::Combat);
    const UnitView* view = findUnit(snapshot, assassin);
    assert(view);
    bool ambushedFarTarget = false;
    for (const Event& event : engine.consumeEvents()) {
        if (event.actor == assassin && event.target == farTarget &&
            event.text.find("used Ambush") != std::string::npos) {
            ambushedFarTarget = true;
        }
    }
    assert(!ambushedFarTarget);

    for (int i = 0; i < 30 * 30 && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
        engine.consumeEvents();
    }

    GameEngine closeEngine(202);
    closeEngine.startNewGame(GameMode::TwoPlayer);
    setupExplorationCombat(closeEngine);
    std::vector<Coord> closeCoords =
        horizontalOpenRun(closeEngine.snapshot(), Coord{kBoardWidth / 2, kBoardHeight / 2}, 6);
    UnitId closeAssassin = closeEngine.debugCreateUnit(PlayerId::One, UnitType::RogueAssassin, closeCoords[0]);
    UnitId closeTarget = closeEngine.debugCreateUnit(PlayerId::Two, UnitType::Cleric, closeCoords[5]);
    assert(closeAssassin != kInvalidUnitId);
    assert(closeTarget != kInvalidUnitId);
    closeEngine.setReady(PlayerId::One, true);
    closeEngine.setReady(PlayerId::Two, true);

    bool sawAmbush = false;
    for (const Event& event : closeEngine.consumeEvents()) {
        if (event.actor == closeAssassin &&
            event.text.find("used Ambush") != std::string::npos) {
            sawAmbush = true;
        }
    }
    assert(sawAmbush);
}

void test_air_targeting_and_damage() {
    GameEngine engine(3);
    engine.startNewGame(GameMode::TwoPlayer);
    setupExplorationCombat(engine);

    std::vector<Coord> coords = horizontalOpenRun(engine.snapshot(), Coord{kBoardWidth / 2, kBoardHeight / 2}, 4);
    assert(engine.debugCreateUnit(PlayerId::One, UnitType::Ranger, coords[0]) != kInvalidUnitId);
    UnitId mephit = engine.debugCreateUnit(PlayerId::Two, UnitType::FireMephit, coords[3]);
    assert(mephit != kInvalidUnitId);

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);
    advance(engine, 12.0);

    GameSnapshot snapshot = engine.snapshot();
    const UnitView* view = findUnit(snapshot, mephit);
    assert(!view || view->totalHp < view->maxTotalHp || !view->alive);
}

void test_melee_switches_to_immediate_threat_instead_of_chasing_far_target() {
    GameEngine engine(701);
    engine.startNewGame(GameMode::TwoPlayer);
    setupExplorationCombat(engine);

    std::vector<Coord> coords =
        horizontalOpenRun(engine.snapshot(), Coord{kBoardWidth / 2, kBoardHeight / 2}, 9);
    UnitId guardian = engine.debugCreateUnit(PlayerId::One, UnitType::ShieldGuardian, coords[0]);
    UnitId farCleric = engine.debugCreateUnit(PlayerId::Two, UnitType::Cleric, coords[6]);
    UnitId closeSkeleton = engine.debugCreateUnit(PlayerId::Two, UnitType::Skeleton, coords[1]);
    assert(guardian != kInvalidUnitId);
    assert(farCleric != kInvalidUnitId);
    assert(closeSkeleton != kInvalidUnitId);

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    bool attackedClose = false;
    bool attackedFarFirst = false;
    for (int i = 0; i < 30 * 4 && !attackedClose && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
        for (const Event& event : engine.consumeEvents()) {
            if (event.type != EventType::UnitAttacked || event.actor != guardian) continue;
            if (event.target == farCleric) attackedFarFirst = true;
            if (event.target == closeSkeleton) attackedClose = true;
        }
    }

    assert(attackedClose);
    assert(!attackedFarFirst);
}

void test_ranged_stops_after_entering_attack_range() {
    GameEngine engine(702);
    engine.startNewGame(GameMode::TwoPlayer);
    setupExplorationCombat(engine);

    std::vector<Coord> coords =
        horizontalOpenRun(engine.snapshot(), Coord{kBoardWidth / 2, kBoardHeight / 2}, 9);
    UnitId ranger = engine.debugCreateUnit(PlayerId::One, UnitType::Ranger, coords[0]);
    UnitId target = engine.debugCreateUnit(PlayerId::Two, UnitType::Ranger, coords[4]);
    assert(ranger != kInvalidUnitId);
    assert(target != kInvalidUnitId);

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    int attackDistance = -1;
    for (int i = 0; i < 30 * 8 && attackDistance < 0 && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
        GameSnapshot snapshot = engine.snapshot();
        const UnitView* rangerView = findUnit(snapshot, ranger);
        if (!rangerView || !rangerView->alive) break;
        for (const Event& event : engine.consumeEvents()) {
            if (event.type == EventType::UnitAttacked && event.actor == ranger) {
                attackDistance = manhattan(event.from, event.to);
            }
        }
    }

    assert(attackDistance >= 2);
    assert(attackDistance <= 3);
}

void test_dynamic_chase_retargets_when_current_target_moves_out_of_reach() {
    GameEngine engine(703);
    engine.startNewGame(GameMode::TwoPlayer);
    setupExplorationCombat(engine);

    std::vector<Coord> coords =
        horizontalOpenRun(engine.snapshot(), Coord{kBoardWidth / 2, kBoardHeight / 2}, 9);
    UnitId guardian = engine.debugCreateUnit(PlayerId::One, UnitType::ShieldGuardian, coords[0]);
    UnitId farCleric = engine.debugCreateUnit(PlayerId::Two, UnitType::Cleric, coords[4]);
    UnitId closeSkeleton = engine.debugCreateUnit(PlayerId::Two, UnitType::Skeleton, coords[1]);
    assert(guardian != kInvalidUnitId);
    assert(farCleric != kInvalidUnitId);
    assert(closeSkeleton != kInvalidUnitId);

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    assert(engine.debugKnockback(farCleric, coords[0], 3, guardian));

    bool attackedClose = false;
    for (int i = 0; i < 30 * 4 && !attackedClose && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
        for (const Event& event : engine.consumeEvents()) {
            if (event.type == EventType::UnitAttacked && event.actor == guardian &&
                event.target == closeSkeleton) {
                attackedClose = true;
            }
        }
    }

    assert(attackedClose);
}

Coord openCoordFarthestFromNeutral(const GameSnapshot& snapshot) {
    Coord best{-1, -1};
    int bestDistance = -1;
    for (int y = 0; y < snapshot.height; ++y) {
        for (int x = 0; x < snapshot.width; ++x) {
            Coord coord{x, y};
            if (terrainAtSnapshot(snapshot, coord) == TerrainKind::Wall || hasAliveUnitAt(snapshot, coord)) continue;
            int nearestNeutral = std::numeric_limits<int>::max();
            for (const UnitView& unit : snapshot.units) {
                if (!unit.alive || !unit.deployed || !isNeutralMonster(unit.type)) continue;
                nearestNeutral = std::min(nearestNeutral, manhattan(coord, unit.coord));
            }
            if (nearestNeutral > bestDistance) {
                bestDistance = nearestNeutral;
                best = coord;
            }
        }
    }
    assert(best.x >= 0);
    return best;
}

void test_ranged_attack_intent_awakens_minotaur_even_on_miss() {
    bool sawMissScenario = false;
    for (unsigned seed = 720; seed < 730 && !sawMissScenario; ++seed) {
        GameEngine engine(seed);
        engine.startNewGame(GameMode::TwoPlayer);
        setupExplorationCombat(engine);

        std::vector<Coord> coords =
            horizontalOpenRun(engine.snapshot(), openCoordFarthestFromNeutral(engine.snapshot()), 4);
        UnitId ranger = engine.debugCreateUnit(PlayerId::One, UnitType::Ranger, coords[0]);
        UnitId minotaur = engine.debugCreateUnit(PlayerId::Two, UnitType::NeutralMinotaur, coords[3]);
        assert(ranger != kInvalidUnitId);
        assert(minotaur != kInvalidUnitId);
        assert(engine.debugSetUnitArmorClass(minotaur, 40));

        engine.setReady(PlayerId::One, true);
        engine.setReady(PlayerId::Two, true);
        bool firstRangerAttackResolved = false;
        for (int i = 0; i < 30 * 4 && !firstRangerAttackResolved && engine.snapshot().phase == Phase::Combat; ++i) {
            engine.tick(1.0 / 30.0);
            for (const Event& event : engine.consumeEvents()) {
                if (event.type != EventType::UnitAttacked || event.actor != ranger ||
                    event.target != minotaur) {
                    continue;
                }
                firstRangerAttackResolved = true;
                if (event.amount == 0) {
                    GameSnapshot snapshot = engine.snapshot();
                    const UnitView* minotaurView = findUnit(snapshot, minotaur);
                    assert(minotaurView);
                    assert(minotaurView->neutralActivated);
                    sawMissScenario = true;
                }
                break;
            }
        }
    }
    assert(sawMissScenario);
}

void test_shield_guardian_does_not_refresh_shield_without_pressure() {
    GameEngine engine(704);
    engine.startNewGame(GameMode::TwoPlayer);
    setupExplorationCombat(engine);

    Coord safeCoord = openCoordFarthestFromNeutral(engine.snapshot());
    UnitId guardian = engine.debugCreateUnit(PlayerId::One, UnitType::ShieldGuardian, safeCoord);
    assert(guardian != kInvalidUnitId);

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    bool sawShield = false;
    for (int i = 0; i < 30 * 6 && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
        for (const Event& event : engine.consumeEvents()) {
            if (event.type == EventType::Shielded && event.actor == guardian &&
                event.target == guardian && event.text.find("gained shield") != std::string::npos) {
                sawShield = true;
            }
        }
    }

    GameSnapshot snapshot = engine.snapshot();
    const UnitView* guardianView = findUnit(snapshot, guardian);
    assert(guardianView);
    assert(!sawShield);
    assert(guardianView->shield == 0);
}

void test_continuing_combat_is_not_cut_off_at_forty_five_seconds() {
    GameEngine engine(705);
    engine.startNewGame(GameMode::TwoPlayer);
    setupExplorationCombat(engine, 10);

    std::array<Coord, 2> coords =
        adjacentOpenCoords(engine.snapshot(), Coord{kBoardWidth / 2, kBoardHeight / 2});
    UnitId left = engine.debugCreateUnit(PlayerId::One, UnitType::ShieldGuardian, coords[0]);
    UnitId right = engine.debugCreateUnit(PlayerId::Two, UnitType::ShieldGuardian, coords[1]);
    assert(left != kInvalidUnitId);
    assert(right != kInvalidUnitId);

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    bool sawAttackAfterOldCap = false;
    for (int i = 0; i < 30 * 52 && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
        GameSnapshot snapshot = engine.snapshot();
        for (const Event& event : engine.consumeEvents()) {
            if (event.type == EventType::UnitAttacked && snapshot.combatTime > 45.0) {
                sawAttackAfterOldCap = true;
            }
        }
    }

    GameSnapshot snapshot = engine.snapshot();
    assert(snapshot.phase == Phase::Combat);
    assert(snapshot.combatTime > 45.0);
    assert(sawAttackAfterOldCap);
}

void test_stalled_combat_ends_after_no_attacks_or_movement() {
    GameEngine engine(706);
    engine.startNewGame(GameMode::TwoPlayer);
    setupExplorationCombat(engine, 10);

    std::vector<Coord> coords =
        horizontalOpenRun(engine.snapshot(), openCoordFarthestFromNeutral(engine.snapshot()), 8);
    UnitId skeleton = engine.debugCreateUnit(PlayerId::One, UnitType::Skeleton, coords[0]);
    UnitId mephit = engine.debugCreateUnit(PlayerId::Two, UnitType::FireMephit, coords[7]);
    assert(skeleton != kInvalidUnitId);
    assert(mephit != kInvalidUnitId);
    assert(engine.debugSetUnitSpeed(skeleton, 0.0));
    assert(engine.debugSetUnitSpeed(mephit, 0.0));

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    bool sawStalled = false;
    for (int i = 0; i < 30 * 24 && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
        for (const Event& event : engine.consumeEvents()) {
            if (event.text.find("combat stalled") != std::string::npos) {
                sawStalled = true;
            }
        }
    }

    GameSnapshot snapshot = engine.snapshot();
    assert(snapshot.phase == Phase::Preparation);
    assert(sawStalled);
}

void test_druid_summons_treant() {
    GameEngine engine(4);
    engine.startNewGame(GameMode::TwoPlayer);
    setupExplorationCombat(engine);

    std::vector<Coord> coords = horizontalOpenRun(engine.snapshot(), Coord{kBoardWidth / 2, kBoardHeight / 2}, 9);
    assert(engine.debugCreateUnit(PlayerId::One, UnitType::Druid, coords[0]) != kInvalidUnitId);
    assert(engine.debugCreateUnit(PlayerId::One, UnitType::ShieldGuardian, coords[3]) != kInvalidUnitId);
    assert(engine.debugCreateUnit(PlayerId::Two, UnitType::ShieldGuardian, coords[8]) != kInvalidUnitId);

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);
    advance(engine, 6.0);

    GameSnapshot snapshot = engine.snapshot();
    bool foundTreant = false;
    for (const UnitView& unit : snapshot.units) {
        if (unit.type == UnitType::Treant && unit.owner == PlayerId::One && unit.alive) {
            foundTreant = true;
        }
    }
    assert(foundTreant);
}

void test_summoners_respect_per_caster_summon_limits_and_relic_bonus() {
    auto countRaisedSkeletons = [](const GameSnapshot& snapshot, PlayerId owner) {
        int count = 0;
        for (const UnitView& unit : snapshot.units) {
            if (unit.type == UnitType::SkeletonByNecromancer && unit.owner == owner &&
                unit.alive && unit.deployed) {
                ++count;
            }
        }
        return count;
    };

    {
        GameEngine engine(305);
        engine.startNewGame(GameMode::TwoPlayer);
        setupExplorationCombat(engine);

        buyAndDeploy(engine, PlayerId::One, UnitType::Necromancer, Coord{0, kBoardHeight / 2 - 2});
        buyAndDeploy(engine, PlayerId::One, UnitType::ShieldGuardian, p1MainDeploy());
        buyAndDeploy(engine, PlayerId::Two, UnitType::ShieldGuardian, p2MainDeploy());
        engine.setReady(PlayerId::One, true);
        engine.setReady(PlayerId::Two, true);
        advance(engine, 4.5);

        assert(countRaisedSkeletons(engine.snapshot(), PlayerId::One) == 2);
    }

    {
        GameEngine engine(306);
        engine.startNewGame(GameMode::TwoPlayer);
        setupExplorationCombat(engine);
        RunModifiers modifiers;
        modifiers.summonLimitBonus = 2;
        engine.setRunModifiers(PlayerId::One, modifiers);

        buyAndDeploy(engine, PlayerId::One, UnitType::Necromancer, Coord{0, kBoardHeight / 2 - 2});
        buyAndDeploy(engine, PlayerId::One, UnitType::ShieldGuardian, p1MainDeploy());
        buyAndDeploy(engine, PlayerId::Two, UnitType::ShieldGuardian, p2MainDeploy());
        engine.setReady(PlayerId::One, true);
        engine.setReady(PlayerId::Two, true);
        advance(engine, 4.5);

        assert(countRaisedSkeletons(engine.snapshot(), PlayerId::One) == 4);
    }
}

void test_land_units_cannot_stack_but_air_units_can() {
    GameEngine engine(5);
    engine.startNewGame(GameMode::TwoPlayer);

    buyAndDeploy(engine, PlayerId::One, UnitType::GithyankiWarrior, Coord{2, 3});
    assert(engine.buyUnit(PlayerId::One, UnitType::GoblinSkirmisher));
    UnitId goblin = firstBenchUnit(engine, PlayerId::One);
    assert(!engine.deployUnit(PlayerId::One, goblin, Coord{2, 3}));
    assert(engine.buyUnit(PlayerId::One, UnitType::FireMephit));
    UnitId mephit = firstBenchUnitOfType(engine, PlayerId::One, UnitType::FireMephit);
    assert(engine.deployUnit(PlayerId::One, mephit, Coord{2, 3}));
    assert(engine.buyUnit(PlayerId::One, UnitType::DragonWyrmling));
    UnitId dragon = firstBenchUnitOfType(engine, PlayerId::One, UnitType::DragonWyrmling);
    assert(engine.deployUnit(PlayerId::One, dragon, Coord{2, 3}));

    GameSnapshot snapshot = engine.snapshot();
    int landOnCell = 0;
    int airOnCell = 0;
    for (const UnitView& unit : snapshot.units) {
        if (unit.alive && unit.deployed && unit.coord == Coord{2, 3} && unit.owner == PlayerId::One) {
            if (unit.layer == UnitLayer::Land) ++landOnCell;
            if (unit.layer == UnitLayer::Air) ++airOnCell;
        }
    }
    assert(findUnit(snapshot, goblin));
    assert(!findUnit(snapshot, goblin)->deployed);
    assert(findUnit(snapshot, mephit)->deployed);
    assert(findUnit(snapshot, dragon)->deployed);
    assert(landOnCell == 1);
    assert(airOnCell == 2);
}

void test_land_units_block_land_movement_but_not_air() {
    GameEngine engine(50);
    engine.startNewGame(GameMode::TwoPlayer);
    setupExplorationCombat(engine);

    std::vector<Coord> coords = horizontalOpenRun(engine.snapshot(), Coord{kBoardWidth / 2, kBoardHeight / 2}, 4);
    UnitId landFollower = engine.debugCreateUnit(PlayerId::One, UnitType::Skeleton, coords[0]);
    UnitId airFollower = engine.debugCreateUnit(PlayerId::One, UnitType::FireMephit, coords[0]);
    UnitId blocker = engine.debugCreateUnit(PlayerId::One, UnitType::ShieldGuardian, coords[1]);
    assert(engine.debugCreateUnit(PlayerId::Two, UnitType::Skeleton, coords[3]) != kInvalidUnitId);
    assert(blocker != kInvalidUnitId);
    assert(landFollower != kInvalidUnitId);
    assert(airFollower != kInvalidUnitId);

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);
    engine.tick(0.55);

    GameSnapshot snapshot = engine.snapshot();
    const UnitView* blockerView = findUnit(snapshot, blocker);
    const UnitView* landView = findUnit(snapshot, landFollower);
    const UnitView* airView = findUnit(snapshot, airFollower);
    assert(blockerView);
    assert(landView);
    assert(airView);
    assert((blockerView->coord == coords[1]));
    assert(landView->coord != blockerView->coord);
    assert(airView->layer == UnitLayer::Air);
}

void test_round_ends_when_combat_units_are_gone() {
    GameEngine engine(6);
    engine.startNewGame(GameMode::TwoPlayer);

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    assert(engine.snapshot().phase == Phase::Combat);
    engine.tick(1.0 / 30.0);

    GameSnapshot snapshot = engine.snapshot();
    assert(snapshot.phase == Phase::Preparation);
    assert(snapshot.round >= 2);
    assert(!snapshot.winner);
    assert(!snapshot.players[0].ready);
    assert(!snapshot.players[1].ready);
    assert(snapshot.players[0].bench.empty());
    assert(snapshot.players[1].bench.empty());
}

void test_exploration_survivors_do_not_timeout_or_return_home() {
    GameEngine engine(108);
    engine.startNewGame(GameMode::TwoPlayer);

    Coord deploy{2, 3};
    UnitId barbarian = buyAndDeploy(engine, PlayerId::One, UnitType::ShieldGuardian, deploy);

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    Coord lastCombatCoord = deploy;
    bool moved = false;
    for (int i = 0; i < 30 * 12 && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
        GameSnapshot snapshot = engine.snapshot();
        if (snapshot.phase != Phase::Combat) break;
        const UnitView* view = findUnit(snapshot, barbarian);
        assert(view);
        assert(view->alive);
        assert(view->deployed);
        if (view->coord != deploy) moved = true;
        lastCombatCoord = view->coord;
        engine.consumeEvents();
    }

    GameSnapshot snapshot = engine.snapshot();
    const UnitView* view = findUnit(snapshot, barbarian);
    assert(snapshot.phase == Phase::Combat);
    assert(view);
    assert(view->alive);
    assert(view->deployed);
    assert(moved);
    assert(view->coord == lastCombatCoord);
    assert(std::find(snapshot.players[0].bench.begin(), snapshot.players[0].bench.end(), barbarian) ==
           snapshot.players[0].bench.end());
    assert(std::find(snapshot.players[0].deployed.begin(), snapshot.players[0].deployed.end(), barbarian) !=
           snapshot.players[0].deployed.end());
}

void test_dead_units_do_not_revive_between_rounds() {
    GameEngine engine(106);
    engine.startNewGame(GameMode::TwoPlayer);
    setupExplorationCombat(engine);

    UnitId doomed = buyAndDeploy(engine, PlayerId::One, UnitType::Skeleton, p1MainDeploy());
    buyAndDeploy(engine, PlayerId::Two, UnitType::DragonWyrmling, p2MainDeploy());
    buyAndDeploy(engine, PlayerId::Two, UnitType::DragonWyrmling, Coord{kBoardWidth - 4, kBoardHeight / 2 + 1});

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    bool sawDeath = false;
    for (int i = 0; i < 30 * 45; ++i) {
        engine.tick(1.0 / 30.0);
        for (const Event& event : engine.consumeEvents()) {
            if (event.type == EventType::UnitDied && event.target == doomed) sawDeath = true;
        }
        if (sawDeath && engine.snapshot().phase == Phase::Preparation) break;
    }

    GameSnapshot snapshot = engine.snapshot();
    const UnitView* doomedView = findUnit(snapshot, doomed);
    assert(sawDeath);
    assert(snapshot.phase == Phase::Preparation);
    assert(!doomedView || !doomedView->alive);
}

void test_round_income_grows_and_uses_interest() {
    GameEngine engine(13);
    engine.startNewGame(GameMode::TwoPlayer);

    GameSnapshot opening = engine.snapshot();
    assert(opening.players[0].money == 48);
    assert(opening.players[1].money == 48);

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);
    engine.tick(1.0 / 30.0);

    GameSnapshot snapshot = engine.snapshot();
    assert(snapshot.phase == Phase::Preparation);
    assert(snapshot.round >= 2);
    assert(snapshot.players[0].money == opening.players[0].money + 14);
    assert(snapshot.players[1].money == opening.players[1].money + 14);
}

void test_kill_bounty_adds_combat_economy() {
    GameEngine engine(31);
    engine.startNewGame(GameMode::TwoPlayer);

    Coord campCoord = firstNeutralCampCoord(engine.snapshot(), PlayerId::Two);
    Coord deploy{std::min(3, std::max(0, campCoord.x - 4)), campCoord.y};
    buyAndDeploy(engine, PlayerId::One, UnitType::Barbarian, deploy);
    GameSnapshot opening = engine.snapshot();
    const UnitView* campUnit = firstNeutralCampUnit(opening, PlayerId::Two);
    assert(campUnit);
    UnitId camp = campUnit->id;

    int before = engine.snapshot().players[0].money;
    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    bool sawBounty = false;
    for (int i = 0; i < 30 * 20 && !sawBounty; ++i) {
        engine.tick(1.0 / 30.0);
        for (const Event& event : engine.consumeEvents()) {
            if (event.type == EventType::GoldGained && event.player == PlayerId::One &&
                event.target == camp && event.amount > 0) {
                sawBounty = true;
            }
        }
    }

    assert(sawBounty);
    assert(engine.snapshot().players[0].money > before);
}

void test_player_objective_clear_adds_exploration_score() {
    GameEngine engine(108);
    engine.startNewGame(GameMode::TwoPlayer);

    GameSnapshot opening = engine.snapshot();
    const UnitView* campUnit = firstNeutralCampUnit(opening, PlayerId::Two);
    assert(campUnit);

    UnitId scorer = engine.debugCreateUnit(PlayerId::One,
                                           UnitType::Ranger,
                                           openCoordNear(opening, campUnit->coord, 3));
    assert(scorer != kInvalidUnitId);
    assert(engine.debugApplyDamageFrom(scorer, campUnit->id, campUnit->totalHp + 50));

    GameSnapshot snapshot = engine.snapshot();
    assert(snapshot.explorationScores[0] > opening.explorationScores[0]);
    assert(snapshot.explorationObjectivesClearedByPlayer[0] ==
           opening.explorationObjectivesClearedByPlayer[0] + 1);
    assert(snapshot.explorationObjectivesCleared == opening.explorationObjectivesCleared + 1);
}

void test_ai_objective_clear_adds_exploration_score() {
    GameEngine engine(109);
    engine.startNewGame(GameMode::TwoPlayer);

    GameSnapshot opening = engine.snapshot();
    const UnitView* campUnit = firstNeutralCampUnit(opening, PlayerId::One);
    assert(campUnit);

    UnitId scorer = engine.debugCreateUnit(PlayerId::Two,
                                           UnitType::Ranger,
                                           openCoordNear(opening, campUnit->coord, 3));
    assert(scorer != kInvalidUnitId);
    assert(engine.debugApplyDamageFrom(scorer, campUnit->id, campUnit->totalHp + 50));

    GameSnapshot snapshot = engine.snapshot();
    assert(snapshot.explorationScores[1] > opening.explorationScores[1]);
    assert(snapshot.explorationObjectivesClearedByPlayer[1] ==
           opening.explorationObjectivesClearedByPlayer[1] + 1);
}

void test_upgrades_are_disabled_until_system_is_ready() {
    GameEngine engine(28);
    engine.startNewGame(GameMode::TwoPlayer);

    assert(engine.buyUnit(PlayerId::One, UnitType::Skeleton));
    UnitId skeleton = firstBenchUnit(engine, PlayerId::One);
    int moneyBefore = engine.snapshot().players[0].money;
    assert(!engine.upgradeUnit(PlayerId::One, skeleton));

    GameSnapshot snapshot = engine.snapshot();
    assert(snapshot.players[0].money == moneyBefore);

    const UnitView* view = findUnit(snapshot, skeleton);
    assert(view);
    assert(!view->upgraded);
    assert(view->attack == 10);
    assert(view->maxTotalHp == 45);

    std::vector<AiAction> actions = engine.legalActions(PlayerId::One);
    for (const AiAction& action : actions) {
        assert(action.kind != AiActionKind::Upgrade);
    }
}

void test_survivor_keeps_fighting_after_enemy_army_is_gone() {
    GameEngine engine(7);
    engine.startNewGame(GameMode::TwoPlayer);
    setupExplorationCombat(engine);

    UnitId dragon = buyAndDeploy(engine, PlayerId::One, UnitType::DragonWyrmling, p1MainDeploy());
    buyAndDeploy(engine, PlayerId::One, UnitType::Ranger, Coord{1, kBoardHeight / 2});
    UnitId skeleton = buyAndDeploy(engine, PlayerId::Two, UnitType::Skeleton, p2MainDeploy());

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    bool sawDragonContinue = false;
    for (int i = 0; i < 30 * 30 && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
        GameSnapshot snapshot = engine.snapshot();
        const UnitView* dragonView = findUnit(snapshot, dragon);
        const UnitView* skeletonView = findUnit(snapshot, skeleton);
        if (dragonView && dragonView->alive && (!skeletonView || !skeletonView->alive)) {
            assert(snapshot.phase == Phase::Combat);
            sawDragonContinue = true;
            break;
        }
    }

    assert(sawDragonContinue);
}

void test_round_cleanup_removes_summons_and_restores_setup() {
    GameEngine engine(8);
    engine.startNewGame(GameMode::TwoPlayer);
    setupExplorationCombat(engine);

    UnitId necromancer = buyAndDeploy(engine, PlayerId::One, UnitType::Necromancer, p1MainDeploy());
    UnitId fighter = buyAndDeploy(engine, PlayerId::Two, UnitType::GithyankiWarrior, p2MainDeploy());

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    for (int i = 0; i < 30 * 46 && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
    }

    GameSnapshot snapshot = engine.snapshot();
    assert(snapshot.phase == Phase::Preparation);
    assert(snapshot.round >= 2);

    const UnitView* necromancerView = findUnit(snapshot, necromancer);
    const UnitView* fighterView = findUnit(snapshot, fighter);
    if (necromancerView) {
        assert(necromancerView->alive);
        assert(necromancerView->deployed);
    }
    if (fighterView) {
        assert(fighterView->alive);
        assert(fighterView->deployed);
    }
    assert(snapshot.players[0].bench.empty());
    assert(snapshot.players[1].bench.empty());

    for (const UnitView& unit : snapshot.units) {
        assert(unit.type != UnitType::SkeletonByNecromancer);
        assert(unit.type != UnitType::Treant);
        assert(unit.type != UnitType::SporeServant);
    }
}

void test_round_limit_finishes_exploration_run_without_switching_maps() {
    GameEngine engine(107);
    engine.setExplorationRoundLimit(2);
    engine.startNewGame(GameMode::TwoPlayer);

    MapKind openingKind = engine.snapshot().mapKind;
    finishRunByRoundLimit(engine);

    GameSnapshot snapshot = engine.snapshot();
    assert(snapshot.phase == Phase::Finished);
    assert(snapshot.mapKind == openingKind);
    assert(snapshot.explorationRound == 2);
}

void test_all_visible_objectives_cleared_finishes_exploration_run() {
    GameEngine engine(115);
    engine.setExplorationRoundLimit(10);
    engine.startNewGame(GameMode::TwoPlayer);

    GameSnapshot opening = engine.snapshot();
    MapKind openingKind = opening.mapKind;
    assert(opening.explorationObjectivesCleared == 0);
    assert(opening.explorationObjectivesTotal == 28);

    assert(engine.debugClearVisibleObjectives(PlayerId::One));

    engine.tick(0.1);

    GameSnapshot snapshot = engine.snapshot();
    assert(snapshot.phase == Phase::Finished);
    assert(snapshot.mapKind == openingKind);
    assert(snapshot.explorationObjectivesCleared == snapshot.explorationObjectivesTotal);
    assert(snapshot.explorationRound < snapshot.explorationRoundLimit);
    assert(snapshot.winner == PlayerId::One);
}

void test_exploration_score_leader_wins_finished_run() {
    GameEngine engine(110);
    engine.setExplorationRoundLimit(2);
    engine.startNewGame(GameMode::TwoPlayer);

    std::vector<Coord> gold = engine.debugRandomGoldCoords();
    assert(!gold.empty());
    assert(engine.debugTriggerRandomGold(PlayerId::One, gold.front()));

    finishRunByRoundLimit(engine);

    GameSnapshot snapshot = engine.snapshot();
    assert(snapshot.phase == Phase::Finished);
    assert(snapshot.explorationScores[0] > snapshot.explorationScores[1]);
    assert(snapshot.winner == PlayerId::One);
}

void test_exploration_tie_breaks_on_gold() {
    GameEngine engine(111);
    engine.setExplorationRoundLimit(2);
    engine.startNewGame(GameMode::TwoPlayer);
    engine.grantGold(PlayerId::Two, 3);

    finishRunByRoundLimit(engine);

    GameSnapshot snapshot = engine.snapshot();
    assert(snapshot.phase == Phase::Finished);
    assert(snapshot.explorationScores[0] == snapshot.explorationScores[1]);
    assert(snapshot.players[1].money > snapshot.players[0].money);
    assert(snapshot.winner == PlayerId::Two);
}

void test_exploration_tie_breaks_on_boss_clears() {
    GameEngine engine(112);
    engine.setExplorationRoundLimit(2);
    engine.startNewGame(GameMode::TwoPlayer);

    GameSnapshot opening = engine.snapshot();
    const UnitView* p2Boss = nullptr;
    std::vector<const UnitView*> p1Camps;
    for (const UnitView& unit : opening.units) {
        if (!unit.alive || !unit.deployed || !isNeutralMonster(unit.type)) continue;
        TerrainKind terrain = terrainAtSnapshot(opening, unit.coord);
        if (unit.owner == PlayerId::One && terrain == TerrainKind::BossSite && !p2Boss) {
            p2Boss = &unit;
        } else if (unit.owner == PlayerId::Two && terrain == TerrainKind::NeutralCamp &&
                   p1Camps.size() < 3) {
            p1Camps.push_back(&unit);
        }
    }
    assert(p2Boss);
    assert(p1Camps.size() == 3);

    UnitId p2Scorer = engine.debugCreateUnit(PlayerId::Two,
                                             UnitType::Ranger,
                                             openCoordNear(opening, p2Boss->coord, 3));
    assert(p2Scorer != kInvalidUnitId);
    assert(engine.debugApplyDamageFrom(p2Scorer, p2Boss->id, p2Boss->totalHp + 50));
    std::vector<UnitId> scoringUnits{p2Scorer};
    for (const UnitView* camp : p1Camps) {
        UnitId p1Scorer = engine.debugCreateUnit(PlayerId::One,
                                                 UnitType::Ranger,
                                                 openCoordNear(engine.snapshot(), camp->coord, 3));
        assert(p1Scorer != kInvalidUnitId);
        assert(engine.debugApplyDamageFrom(p1Scorer, camp->id, camp->totalHp + 50));
        scoringUnits.push_back(p1Scorer);
    }

    GameSnapshot afterClears = engine.snapshot();
    assert(afterClears.explorationScores[0] == afterClears.explorationScores[1]);
    assert(afterClears.explorationBossesClearedByPlayer[1] >
           afterClears.explorationBossesClearedByPlayer[0]);
    int moneyDelta = afterClears.players[0].money - afterClears.players[1].money;
    if (moneyDelta > 0) {
        engine.grantGold(PlayerId::Two, moneyDelta);
    } else if (moneyDelta < 0) {
        engine.grantGold(PlayerId::One, -moneyDelta);
    }
    for (UnitId id : scoringUnits) {
        assert(engine.debugApplyDamage(id, 9999, DamageType::Force));
    }

    finishRunByRoundLimit(engine);

    GameSnapshot snapshot = engine.snapshot();
    assert(snapshot.phase == Phase::Finished);
    assert(snapshot.explorationScores[0] == snapshot.explorationScores[1]);
    assert(snapshot.players[0].money == snapshot.players[1].money);
    assert(snapshot.explorationBossesClearedByPlayer[1] >
           snapshot.explorationBossesClearedByPlayer[0]);
    assert(snapshot.winner == PlayerId::Two);
}

void test_exploration_tie_breaks_on_alive_threat() {
    GameEngine engine(113);
    engine.setExplorationRoundLimit(2);
    engine.startNewGame(GameMode::TwoPlayer);

    assert(engine.buyUnit(PlayerId::One, UnitType::Skeleton));
    assert(engine.buyUnit(PlayerId::Two, UnitType::ShieldGuardian));
    int moneyDelta = engine.snapshot().players[0].money - engine.snapshot().players[1].money;
    if (moneyDelta > 0) {
        engine.grantGold(PlayerId::Two, moneyDelta);
    } else if (moneyDelta < 0) {
        engine.grantGold(PlayerId::One, -moneyDelta);
    }

    finishRunByRoundLimit(engine);

    GameSnapshot snapshot = engine.snapshot();
    assert(snapshot.phase == Phase::Finished);
    assert(snapshot.explorationScores[0] == snapshot.explorationScores[1]);
    assert(snapshot.players[0].money == snapshot.players[1].money);
    assert(snapshot.explorationBossesClearedByPlayer[0] ==
           snapshot.explorationBossesClearedByPlayer[1]);
    assert(snapshot.winner == PlayerId::Two);
}

void test_exploration_complete_tie_has_no_winner() {
    GameEngine engine(114);
    engine.setExplorationRoundLimit(2);
    engine.startNewGame(GameMode::TwoPlayer);

    finishRunByRoundLimit(engine);

    GameSnapshot snapshot = engine.snapshot();
    assert(snapshot.phase == Phase::Finished);
    assert(snapshot.explorationScores[0] == snapshot.explorationScores[1]);
    assert(snapshot.players[0].money == snapshot.players[1].money);
    assert(!snapshot.winner);
}

void test_cleric_moves_to_heal_distant_ally() {
    GameEngine engine(9);
    engine.startNewGame(GameMode::TwoPlayer);
    setupExplorationCombat(engine);

    std::vector<Coord> coords = horizontalOpenRun(engine.snapshot(), Coord{kBoardWidth / 2, kBoardHeight / 2}, 9);
    UnitId cleric = engine.debugCreateUnit(PlayerId::One, UnitType::Cleric, coords[0]);
    UnitId fighter = engine.debugCreateUnit(PlayerId::One, UnitType::ShieldGuardian, coords[4]);
    assert(engine.debugCreateUnit(PlayerId::Two, UnitType::DragonWyrmling, coords[7]) != kInvalidUnitId);
    assert(engine.debugCreateUnit(PlayerId::Two, UnitType::Ranger, coords[8]) != kInvalidUnitId);
    assert(cleric != kInvalidUnitId);
    assert(fighter != kInvalidUnitId);
    assert(engine.debugApplyDamage(fighter, 40, DamageType::Force));

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    bool healedFighter = false;
    bool clericMovedCloser = false;
    int startingDistance = -1;
    int previousDistance = 99;
    for (int i = 0; i < 30 * 36 && engine.snapshot().phase == Phase::Combat; ++i) {
        GameSnapshot before = engine.snapshot();
        const UnitView* clericBefore = findUnit(before, cleric);
        const UnitView* fighterBefore = findUnit(before, fighter);
        if (clericBefore && fighterBefore) {
            previousDistance = manhattan(clericBefore->coord, fighterBefore->coord);
            if (startingDistance < 0) startingDistance = previousDistance;
        }

        engine.tick(1.0 / 30.0);

        GameSnapshot after = engine.snapshot();
        const UnitView* clericAfter = findUnit(after, cleric);
        const UnitView* fighterAfter = findUnit(after, fighter);
        if (clericAfter && fighterAfter && manhattan(clericAfter->coord, fighterAfter->coord) < previousDistance) {
            clericMovedCloser = true;
        }

        for (const Event& event : engine.consumeEvents()) {
            if (event.type == EventType::Healed && event.actor == cleric && event.target == fighter) {
                healedFighter = true;
            }
        }
        if (healedFighter) break;
    }

    assert(startingDistance > 3 || clericMovedCloser);
    assert(healedFighter);
}

void test_cleric_follows_frontline_when_no_one_is_wounded() {
    GameEngine engine(10);
    engine.startNewGame(GameMode::TwoPlayer);
    setupExplorationCombat(engine);

    std::vector<Coord> coords = horizontalOpenRun(engine.snapshot(), Coord{kBoardWidth / 2, kBoardHeight / 2}, 7);
    UnitId cleric = engine.debugCreateUnit(PlayerId::One, UnitType::Cleric, coords[0]);
    UnitId fighter = engine.debugCreateUnit(PlayerId::One, UnitType::ShieldGuardian, coords[3]);
    assert(engine.debugCreateUnit(PlayerId::Two, UnitType::ShieldGuardian, coords[6]) != kInvalidUnitId);
    assert(cleric != kInvalidUnitId);
    assert(fighter != kInvalidUnitId);

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);
    advance(engine, 3.0);

    GameSnapshot snapshot = engine.snapshot();
    const UnitView* clericView = findUnit(snapshot, cleric);
    const UnitView* fighterView = findUnit(snapshot, fighter);
    assert(clericView);
    assert(fighterView);
    assert(clericView->coord.x <= fighterView->coord.x);
}

void test_solo_cleric_falls_back_to_attacking() {
    GameEngine engine(11);
    engine.startNewGame(GameMode::TwoPlayer);
    setupExplorationCombat(engine);

    std::vector<Coord> coords = horizontalOpenRun(engine.snapshot(), Coord{kBoardWidth / 2, kBoardHeight / 2}, 4);
    UnitId cleric = engine.debugCreateUnit(PlayerId::One, UnitType::Cleric, coords[0]);
    UnitId skeleton = engine.debugCreateUnit(PlayerId::Two, UnitType::Skeleton, coords[3]);
    assert(cleric != kInvalidUnitId);
    assert(skeleton != kInvalidUnitId);

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    bool sawClericAttack = false;
    for (int i = 0; i < 30 * 12 && !sawClericAttack && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
        for (const Event& event : engine.consumeEvents()) {
            if (event.type == EventType::UnitAttacked && event.actor == cleric) {
                sawClericAttack = true;
            }
        }
    }
    assert(sawClericAttack);
}

} // namespace

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif

#define RUN_TEST(fn) do { fn(); } while (false)
    RUN_TEST(test_relic_taxonomy_has_four_layers_and_family_pools);
    RUN_TEST(test_battle_board_uses_33x19_isolated_wilds_map);
    RUN_TEST(test_exploration_state_tracks_objectives_and_random_gold);
    RUN_TEST(test_hidden_events_are_not_visible_and_can_use_edge_rows);
    RUN_TEST(test_exploration_round_selector_uses_allowed_values);
    RUN_TEST(test_neutral_camps_are_targetable_and_reward_kills);
    RUN_TEST(test_main_lane_units_ignore_unreachable_side_camps);
    RUN_TEST(test_random_gold_bricks_are_hidden_and_once_only);
    RUN_TEST(test_hidden_healing_spring_is_hidden_and_once_only);
    RUN_TEST(test_high_wis_perception_detects_hidden_gold_and_healing);
    RUN_TEST(test_low_wis_unit_can_miss_hidden_detection);
    RUN_TEST(test_internal_units_cannot_detect_hidden_events);
    RUN_TEST(test_neutral_camps_hold_their_guard_posts);
    RUN_TEST(test_neutral_camps_wait_until_attacked_before_countering);
    RUN_TEST(test_neutral_activation_ignores_proximity_hidden_events_and_sourceless_damage);
    RUN_TEST(test_neutral_knockback_activates_only_the_shoved_guardian);
    RUN_TEST(test_neutral_activation_resets_next_preparation_without_healing);
    RUN_TEST(test_phase_spider_retaliates_after_ranged_attack_with_leash);
    RUN_TEST(test_neutral_guardian_returns_home_visibly_after_round_reset);
    RUN_TEST(test_redcap_imp_and_guardian_targeting_priorities);
    RUN_TEST(test_neutral_spaw_caps_servants_and_servants_guard_locally);
    RUN_TEST(test_side_neutral_camps_do_not_respawn_after_death);
    RUN_TEST(test_side_trap_spawns_redcap_ambush_once);
    RUN_TEST(test_high_dex_unit_avoids_redcap_trap_and_clears_it);
    RUN_TEST(test_low_dex_unit_can_fail_redcap_trap_save);
    RUN_TEST(test_redcap_ambush_persists_next_round_until_killed);
    RUN_TEST(test_redcap_ambush_has_short_leash);
    RUN_TEST(test_neutral_monster_roster_has_bg3_camp_and_trap_units);
    RUN_TEST(test_run_modifiers_change_costs_and_bench_limit);
    RUN_TEST(test_roster_cap_counts_deployed_units);
    RUN_TEST(test_ai_actions_are_generated_and_apply_through_rules);
    RUN_TEST(test_shop_roster_uses_fifteen_dnd_units);
    RUN_TEST(test_shop_costs_use_expanded_budget_tiers);
    RUN_TEST(test_shop_units_expose_dnd_combat_stats);
    RUN_TEST(test_unit_profiles_cover_every_unit_type);
    RUN_TEST(test_derived_combat_stats_use_bg3_profile_abilities);
    RUN_TEST(test_saving_throws_use_specific_ability_scores);
    RUN_TEST(test_unit_attack_ranges_match_roles);
    RUN_TEST(test_damage_packets_use_bg3_style_ranges_and_types);
    RUN_TEST(test_damage_affinities_are_unit_specific);
    RUN_TEST(test_damage_affinity_modifies_combat_damage_events);
    RUN_TEST(test_attack_events_resolve_without_exposing_dice_math);
    RUN_TEST(test_berserker_no_longer_skips_first_swing);
    RUN_TEST(test_spell_events_resolve_saves_without_exposing_dice_math);
    RUN_TEST(test_tamia_blight_uses_hidden_8d8_necrotic_damage);
    RUN_TEST(test_tamia_dominate_creates_neutral_controlled_hostile);
    RUN_TEST(test_mind_blast_stuns_multiple_targets_and_extracts_stunned);
    RUN_TEST(test_ketheric_wrathful_smite_can_frighten_target);
    RUN_TEST(test_ketheric_wrathful_smite_repels_nearby_enemies);
    RUN_TEST(test_minotaur_charge_knocks_back_and_prones_line_targets);
    RUN_TEST(test_owlbear_multiattack_knocks_prone_target_back);
    RUN_TEST(test_raphael_diabolic_chains_can_shove_failed_saves);
    RUN_TEST(test_knockback_pushes_land_units_domino_style);
    RUN_TEST(test_knockback_stops_at_wall_without_land_overlap);
    RUN_TEST(test_knockback_ignores_air_units_on_destination);
    RUN_TEST(test_boss_units_resist_standard_knockback);
    RUN_TEST(test_radial_knockback_pushes_hostile_units_away_from_center);
    RUN_TEST(test_feature_schema_matches_exported_features);
    RUN_TEST(test_normal_ai_uses_built_in_strategy_without_policy_file);
    RUN_TEST(test_normal_ai_round_one_opener_stays_readable);
    RUN_TEST(test_normal_ai_exploration_opener_covers_center_and_wing);
    RUN_TEST(test_normal_ai_buys_air_answer_when_player_fields_air);
    RUN_TEST(test_missing_policy_falls_back_to_heuristic_ai);
    RUN_TEST(test_stale_policy_falls_back_to_heuristic_ai);
    RUN_TEST(test_exported_policy_loads_when_rules_match);
    RUN_TEST(test_policy_ai_builds_roster_before_round_one_upgrades);
    RUN_TEST(test_buy_and_deploy_rules);
    RUN_TEST(test_exploration_deployment_uses_top_and_bottom_staging_rows);
    RUN_TEST(test_githyanki_opens_with_astral_raid);
    RUN_TEST(test_assassin_uses_limited_range_ambush);
    RUN_TEST(test_air_targeting_and_damage);
    RUN_TEST(test_melee_switches_to_immediate_threat_instead_of_chasing_far_target);
    RUN_TEST(test_ranged_stops_after_entering_attack_range);
    RUN_TEST(test_dynamic_chase_retargets_when_current_target_moves_out_of_reach);
    RUN_TEST(test_ranged_attack_intent_awakens_minotaur_even_on_miss);
    RUN_TEST(test_shield_guardian_does_not_refresh_shield_without_pressure);
    RUN_TEST(test_continuing_combat_is_not_cut_off_at_forty_five_seconds);
    RUN_TEST(test_stalled_combat_ends_after_no_attacks_or_movement);
    RUN_TEST(test_druid_summons_treant);
    RUN_TEST(test_summoners_respect_per_caster_summon_limits_and_relic_bonus);
    RUN_TEST(test_land_units_cannot_stack_but_air_units_can);
    RUN_TEST(test_land_units_block_land_movement_but_not_air);
    RUN_TEST(test_round_ends_when_combat_units_are_gone);
    RUN_TEST(test_exploration_survivors_do_not_timeout_or_return_home);
    RUN_TEST(test_dead_units_do_not_revive_between_rounds);
    RUN_TEST(test_round_income_grows_and_uses_interest);
    RUN_TEST(test_kill_bounty_adds_combat_economy);
    RUN_TEST(test_player_objective_clear_adds_exploration_score);
    RUN_TEST(test_ai_objective_clear_adds_exploration_score);
    RUN_TEST(test_upgrades_are_disabled_until_system_is_ready);
    RUN_TEST(test_survivor_keeps_fighting_after_enemy_army_is_gone);
    RUN_TEST(test_round_cleanup_removes_summons_and_restores_setup);
    RUN_TEST(test_round_limit_finishes_exploration_run_without_switching_maps);
    RUN_TEST(test_all_visible_objectives_cleared_finishes_exploration_run);
    RUN_TEST(test_exploration_score_leader_wins_finished_run);
    RUN_TEST(test_exploration_tie_breaks_on_gold);
    RUN_TEST(test_exploration_tie_breaks_on_boss_clears);
    RUN_TEST(test_exploration_tie_breaks_on_alive_threat);
    RUN_TEST(test_exploration_complete_tie_has_no_winner);
    RUN_TEST(test_cleric_moves_to_heal_distant_ally);
    RUN_TEST(test_cleric_follows_frontline_when_no_one_is_wounded);
    RUN_TEST(test_solo_cleric_falls_back_to_attacking);
#undef RUN_TEST

    std::cout << "autochess core tests passed\n";
    return 0;
}
