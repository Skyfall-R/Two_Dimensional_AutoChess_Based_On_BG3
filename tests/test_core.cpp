#include <lib.hpp>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace autochess;

namespace {

UnitId firstBenchUnit(const GameEngine& engine, PlayerId player) {
    GameSnapshot snapshot = engine.snapshot();
    const PlayerView& view = snapshot.players[player == PlayerId::One ? 0 : 1];
    assert(!view.bench.empty());
    return view.bench.front();
}

const UnitView* findUnit(const GameSnapshot& snapshot, UnitId id) {
    for (const UnitView& unit : snapshot.units) {
        if (unit.id == id) return &unit;
    }
    return nullptr;
}

int towerHpFor(const GameSnapshot& snapshot, PlayerId player) {
    int total = 0;
    for (const UnitView& unit : snapshot.units) {
        if (unit.type == UnitType::DefenseTower && unit.owner == player && unit.alive) {
            total += unit.totalHp;
        }
    }
    return total;
}

int maxTowerHpFor(const GameSnapshot& snapshot, PlayerId player) {
    int total = 0;
    for (const UnitView& unit : snapshot.units) {
        if (unit.type == UnitType::DefenseTower && unit.owner == player && unit.alive) {
            total += unit.maxTotalHp;
        }
    }
    return total;
}

UnitId buyAndDeploy(GameEngine& engine, PlayerId player, UnitType type, Coord coord) {
    assert(engine.buyUnit(player, type));
    UnitId id = firstBenchUnit(engine, player);
    assert(engine.deployUnit(player, id, coord));
    return id;
}

void advance(GameEngine& engine, double seconds) {
    int ticks = static_cast<int>(seconds * 30.0);
    for (int i = 0; i < ticks; ++i) engine.tick(1.0 / 30.0);
}

void assertNoLandOverlap(const GameSnapshot& snapshot) {
    for (int y = 0; y < snapshot.height; ++y) {
        for (int x = 0; x < snapshot.width; ++x) {
            int landCount = 0;
            for (const UnitView& unit : snapshot.units) {
                if (unit.alive && unit.deployed && unit.layer == UnitLayer::Land &&
                    unit.coord == Coord{x, y}) {
                    ++landCount;
                }
            }
            assert(landCount <= 1);
        }
    }
}

void setupAiActionState(GameEngine& engine) {
    engine.startNewGame(GameMode::TwoPlayer);
    assert(engine.buyUnit(PlayerId::One, UnitType::Knight));
    UnitId knight = firstBenchUnit(engine, PlayerId::One);
    assert(engine.deployUnit(PlayerId::One, knight, Coord{2, 3}));
    assert(engine.buyUnit(PlayerId::One, UnitType::Archer));
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
        assertNoLandOverlap(probe.snapshot());
    }

    assert(sawBuy);
    assert(sawDeploy);
    assert(sawMove);
    assert(sawReturn);
    assert(sawUpgrade);
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

void test_missing_policy_falls_back_to_heuristic_ai() {
    GameEngine engine(17);
    GameConfig config;
    config.mode = GameMode::SinglePlayerVsAi;
    config.aiDifficulty = AiDifficulty::Hard;
    config.aiPolicyDirectory = "missing-ai-policies";
    engine.startNewGame(config);
    assert(!engine.aiPolicyMetadata().valid);
    assert(engine.aiPolicyMetadata().status.find("missing") != std::string::npos);

    buyAndDeploy(engine, PlayerId::One, UnitType::ShieldGuard, Coord{2, 3});
    engine.setReady(PlayerId::One, true);
    assert(engine.snapshot().phase == Phase::Combat);
}

void test_stale_policy_falls_back_to_heuristic_ai() {
    std::filesystem::path directory = std::filesystem::path(".cache") / "test-ai-policies";
    std::filesystem::create_directories(directory);

    GameEngine schemaEngine(18);
    schemaEngine.startNewGame(GameMode::SinglePlayerVsAi);
    AiFeatureSchema schema = schemaEngine.aiFeatureSchema();
    std::filesystem::path policy = directory / "hard.policy.json";
    std::ofstream out(policy);
    out << "{\n"
        << "  \"format\": \"autochess_policy_v1\",\n"
        << "  \"modelVersion\": \"linear-v1\",\n"
        << "  \"difficulty\": \"Hard\",\n"
        << "  \"rulesFingerprint\": \"stale\",\n"
        << "  \"stateFeatureCount\": " << schema.stateFeatureCount << ",\n"
        << "  \"actionFeatureCount\": " << schema.actionFeatureCount << ",\n"
        << "  \"heuristicBlend\": 1.0,\n"
        << "  \"bias\": 0.0,\n"
        << "  \"weights\": [";
    int weightCount = schema.stateFeatureCount + schema.actionFeatureCount;
    for (int i = 0; i < weightCount; ++i) {
        if (i > 0) out << ",";
        out << "0";
    }
    out << "]\n}\n";
    out.close();

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
    std::filesystem::path root = std::filesystem::current_path();
    for (int i = 0; i < 6 && !std::filesystem::exists(root / "assets" / "ai" / "normal.policy.json"); ++i) {
        root = root.parent_path();
    }
    std::filesystem::path policyDirectory = root / "assets" / "ai";
    if (!std::filesystem::exists(policyDirectory / "normal.policy.json") ||
        !std::filesystem::exists(policyDirectory / "hard.policy.json") ||
        !std::filesystem::exists(policyDirectory / "superhard.policy.json")) {
        return;
    }

    GameEngine engine(20);
    GameConfig config;
    config.mode = GameMode::SinglePlayerVsAi;
    config.aiDifficulty = AiDifficulty::Normal;
    config.aiPolicyDirectory = policyDirectory.string();
    engine.startNewGame(config);
    assert(engine.aiPolicyMetadata().loaded);
    assert(engine.aiPolicyMetadata().valid);

    config.aiDifficulty = AiDifficulty::SuperHard;
    engine.startNewGame(config);
    assert(engine.aiPolicyMetadata().loaded);
    assert(engine.aiPolicyMetadata().valid);
}

void test_buy_and_deploy_rules() {
    GameEngine engine(1);
    engine.startNewGame(GameMode::TwoPlayer);

    assert(engine.buyUnit(PlayerId::One, UnitType::ShieldGuard));
    UnitId guard = firstBenchUnit(engine, PlayerId::One);

    assert(!engine.deployUnit(PlayerId::One, guard, Coord{5, 3}));
    assert(engine.deployUnit(PlayerId::One, guard, Coord{2, 3}));
    assert(!engine.moveDeployedUnit(PlayerId::One, guard, Coord{8, 3}));
    assert(engine.moveDeployedUnit(PlayerId::One, guard, Coord{0, 3}));
    assert(engine.returnToBench(PlayerId::One, guard));
    assert(engine.deployUnit(PlayerId::One, guard, Coord{1, 3}));

    GameSnapshot snapshot = engine.snapshot();
    const UnitView* view = findUnit(snapshot, guard);
    assert((view && view->coord == Coord{1, 3}));
}

void test_assassin_leaps_to_backline() {
    GameEngine engine(2);
    engine.startNewGame(GameMode::TwoPlayer);

    UnitId assassin = buyAndDeploy(engine, PlayerId::One, UnitType::ShadowAssassin, Coord{2, 0});
    buyAndDeploy(engine, PlayerId::Two, UnitType::Cleric, Coord{8, 6});

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    GameSnapshot snapshot = engine.snapshot();
    assert(snapshot.phase == Phase::Combat);
    const UnitView* view = findUnit(snapshot, assassin);
    assert(view);
    assert(view->coord.x >= 7);
}

void test_air_targeting_and_damage() {
    GameEngine engine(3);
    engine.startNewGame(GameMode::TwoPlayer);

    buyAndDeploy(engine, PlayerId::One, UnitType::Archer, Coord{2, 3});
    UnitId balloon = buyAndDeploy(engine, PlayerId::Two, UnitType::Balloon, Coord{8, 3});

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);
    advance(engine, 4.0);

    GameSnapshot snapshot = engine.snapshot();
    const UnitView* view = findUnit(snapshot, balloon);
    assert(view);
    assert(view->totalHp < view->maxTotalHp || !view->alive);
}

void test_druid_summons_treant() {
    GameEngine engine(4);
    engine.startNewGame(GameMode::TwoPlayer);

    buyAndDeploy(engine, PlayerId::One, UnitType::Druid, Coord{0, 0});
    buyAndDeploy(engine, PlayerId::One, UnitType::ShieldGuard, Coord{2, 3});
    buyAndDeploy(engine, PlayerId::Two, UnitType::ShieldGuard, Coord{8, 3});
    buyAndDeploy(engine, PlayerId::Two, UnitType::Knight, Coord{10, 6});

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

void test_no_land_overlap_after_movement() {
    GameEngine engine(5);
    engine.startNewGame(GameMode::TwoPlayer);

    buyAndDeploy(engine, PlayerId::One, UnitType::Knight, Coord{2, 2});
    buyAndDeploy(engine, PlayerId::One, UnitType::Goblin, Coord{2, 3});
    buyAndDeploy(engine, PlayerId::Two, UnitType::Knight, Coord{8, 3});

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);
    advance(engine, 5.0);

    GameSnapshot snapshot = engine.snapshot();
    for (int y = 0; y < snapshot.height; ++y) {
        for (int x = 0; x < snapshot.width; ++x) {
            int landCount = 0;
            for (const UnitView& unit : snapshot.units) {
                if (unit.alive && unit.deployed && unit.layer == UnitLayer::Land &&
                    unit.coord == Coord{x, y}) {
                    ++landCount;
                }
            }
            assert(landCount <= 1);
        }
    }
}

void test_round_ends_when_combat_units_are_gone() {
    GameEngine engine(6);
    engine.startNewGame(GameMode::TwoPlayer);

    UnitId p1SkeletonId = buyAndDeploy(engine, PlayerId::One, UnitType::Skeleton, Coord{2, 3});
    UnitId p2SkeletonId = buyAndDeploy(engine, PlayerId::Two, UnitType::Skeleton, Coord{8, 3});

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    for (int i = 0; i < 30 * 20 && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
    }

    GameSnapshot snapshot = engine.snapshot();
    assert(snapshot.phase == Phase::Preparation);
    assert(snapshot.round == 2);
    assert(!snapshot.winner);
    assert(!snapshot.players[0].ready);
    assert(!snapshot.players[1].ready);

    assert(!findUnit(snapshot, p1SkeletonId));
    assert(!findUnit(snapshot, p2SkeletonId));
    assert(snapshot.players[0].bench.empty());
    assert(snapshot.players[1].bench.empty());
    for (const UnitView& unit : snapshot.units) {
        assert(!unit.deployed || unit.type == UnitType::DefenseTower);
    }
}

void test_round_income_grows_and_uses_interest() {
    GameEngine engine(13);
    engine.startNewGame(GameMode::TwoPlayer);

    GameSnapshot opening = engine.snapshot();
    assert(opening.players[0].money == 12);
    assert(opening.players[1].money == 12);

    buyAndDeploy(engine, PlayerId::One, UnitType::Skeleton, Coord{2, 3});
    buyAndDeploy(engine, PlayerId::Two, UnitType::Skeleton, Coord{8, 3});

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    for (int i = 0; i < 30 * 20 && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
    }

    GameSnapshot snapshot = engine.snapshot();
    assert(snapshot.phase == Phase::Preparation);
    assert(snapshot.round == 2);
    assert(snapshot.players[0].money == 20);
    assert(snapshot.players[1].money == 20);
}

void test_tower_damage_persists_between_rounds() {
    GameEngine engine(14);
    engine.startNewGame(GameMode::TwoPlayer);

    GameSnapshot opening = engine.snapshot();
    int openingTowerHp = towerHpFor(opening, PlayerId::Two);
    assert(openingTowerHp == maxTowerHpFor(opening, PlayerId::Two));

    buyAndDeploy(engine, PlayerId::One, UnitType::Pekka, Coord{2, 3});

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    for (int i = 0; i < 30 * 40 && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
    }

    GameSnapshot snapshot = engine.snapshot();
    assert(snapshot.phase == Phase::Preparation);
    int nextRoundTowerHp = towerHpFor(snapshot, PlayerId::Two);
    assert(nextRoundTowerHp > 0);
    assert(nextRoundTowerHp < openingTowerHp);
}

void test_survivor_keeps_fighting_after_enemy_army_is_gone() {
    GameEngine engine(7);
    engine.startNewGame(GameMode::TwoPlayer);

    UnitId dragon = buyAndDeploy(engine, PlayerId::One, UnitType::BabyDragon, Coord{2, 3});
    UnitId skeleton = buyAndDeploy(engine, PlayerId::Two, UnitType::Skeleton, Coord{8, 3});

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    bool sawDragonContinue = false;
    for (int i = 0; i < 30 * 15 && engine.snapshot().phase == Phase::Combat; ++i) {
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

    UnitId witch = buyAndDeploy(engine, PlayerId::One, UnitType::Witch, Coord{2, 3});
    UnitId knight = buyAndDeploy(engine, PlayerId::Two, UnitType::Knight, Coord{8, 3});

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    for (int i = 0; i < 30 * 46 && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
    }

    GameSnapshot snapshot = engine.snapshot();
    assert(snapshot.phase == Phase::Preparation);
    assert(snapshot.round == 2);

    assert(!findUnit(snapshot, witch));
    assert(!findUnit(snapshot, knight));
    assert(snapshot.players[0].bench.empty());
    assert(snapshot.players[1].bench.empty());

    for (const UnitView& unit : snapshot.units) {
        assert(unit.type != UnitType::SkeletonByWitch);
        assert(unit.type != UnitType::Treant);
        assert(!unit.deployed || unit.type == UnitType::DefenseTower);
    }
}

void test_cleric_moves_to_heal_distant_ally() {
    GameEngine engine(9);
    engine.startNewGame(GameMode::TwoPlayer);

    UnitId cleric = buyAndDeploy(engine, PlayerId::One, UnitType::Cleric, Coord{0, 0});
    UnitId knight = buyAndDeploy(engine, PlayerId::One, UnitType::Knight, Coord{2, 3});
    buyAndDeploy(engine, PlayerId::Two, UnitType::Knight, Coord{8, 3});
    buyAndDeploy(engine, PlayerId::Two, UnitType::Archer, Coord{8, 2});

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    bool healedKnight = false;
    bool clericMovedCloser = false;
    int previousDistance = 99;
    for (int i = 0; i < 30 * 24 && engine.snapshot().phase == Phase::Combat; ++i) {
        GameSnapshot before = engine.snapshot();
        const UnitView* clericBefore = findUnit(before, cleric);
        const UnitView* knightBefore = findUnit(before, knight);
        if (clericBefore && knightBefore) previousDistance = manhattan(clericBefore->coord, knightBefore->coord);

        engine.tick(1.0 / 30.0);

        GameSnapshot after = engine.snapshot();
        const UnitView* clericAfter = findUnit(after, cleric);
        const UnitView* knightAfter = findUnit(after, knight);
        if (clericAfter && knightAfter && manhattan(clericAfter->coord, knightAfter->coord) < previousDistance) {
            clericMovedCloser = true;
        }

        for (const Event& event : engine.consumeEvents()) {
            if (event.type == EventType::Healed && event.actor == cleric && event.target == knight) {
                healedKnight = true;
            }
        }
        if (healedKnight) break;
    }

    assert(clericMovedCloser);
    assert(healedKnight);
}

void test_cleric_follows_frontline_when_no_one_is_wounded() {
    GameEngine engine(10);
    engine.startNewGame(GameMode::TwoPlayer);

    UnitId cleric = buyAndDeploy(engine, PlayerId::One, UnitType::Cleric, Coord{0, 3});
    UnitId knight = buyAndDeploy(engine, PlayerId::One, UnitType::Knight, Coord{2, 3});
    buyAndDeploy(engine, PlayerId::Two, UnitType::Knight, Coord{8, 3});

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);
    advance(engine, 3.0);

    GameSnapshot snapshot = engine.snapshot();
    const UnitView* clericView = findUnit(snapshot, cleric);
    const UnitView* knightView = findUnit(snapshot, knight);
    assert(clericView);
    assert(knightView);
    assert(clericView->coord.x <= knightView->coord.x);
}

void test_solo_cleric_falls_back_to_attacking() {
    GameEngine engine(11);
    engine.startNewGame(GameMode::TwoPlayer);

    UnitId cleric = buyAndDeploy(engine, PlayerId::One, UnitType::Cleric, Coord{2, 3});
    UnitId skeleton = buyAndDeploy(engine, PlayerId::Two, UnitType::Skeleton, Coord{8, 3});

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);
    advance(engine, 7.0);

    GameSnapshot snapshot = engine.snapshot();
    const UnitView* clericView = findUnit(snapshot, cleric);
    const UnitView* skeletonView = findUnit(snapshot, skeleton);
    assert(clericView);
    assert(clericView->coord.x > 2 || (skeletonView && skeletonView->totalHp < skeletonView->maxTotalHp));
}

void test_aoe_unit_prefers_clustered_targets() {
    GameEngine engine(12);
    engine.startNewGame(GameMode::TwoPlayer);

    UnitId dragon = buyAndDeploy(engine, PlayerId::One, UnitType::BabyDragon, Coord{2, 3});
    UnitId archer = buyAndDeploy(engine, PlayerId::Two, UnitType::Archer, Coord{8, 3});
    UnitId cleric = buyAndDeploy(engine, PlayerId::Two, UnitType::Cleric, Coord{8, 4});

    engine.setReady(PlayerId::One, true);
    engine.setReady(PlayerId::Two, true);

    UnitId firstDragonTarget = kInvalidUnitId;
    for (int i = 0; i < 30 * 12 && engine.snapshot().phase == Phase::Combat; ++i) {
        engine.tick(1.0 / 30.0);
        for (const Event& event : engine.consumeEvents()) {
            if (event.type == EventType::UnitAttacked && event.actor == dragon) {
                firstDragonTarget = event.target;
                break;
            }
        }
        if (firstDragonTarget != kInvalidUnitId) break;
    }

    assert(firstDragonTarget == archer || firstDragonTarget == cleric);
}

} // namespace

int main() {
    test_ai_actions_are_generated_and_apply_through_rules();
    test_feature_schema_matches_exported_features();
    test_missing_policy_falls_back_to_heuristic_ai();
    test_stale_policy_falls_back_to_heuristic_ai();
    test_exported_policy_loads_when_rules_match();
    test_buy_and_deploy_rules();
    test_assassin_leaps_to_backline();
    test_air_targeting_and_damage();
    test_druid_summons_treant();
    test_no_land_overlap_after_movement();
    test_round_ends_when_combat_units_are_gone();
    test_round_income_grows_and_uses_interest();
    test_tower_damage_persists_between_rounds();
    test_survivor_keeps_fighting_after_enemy_army_is_gone();
    test_round_cleanup_removes_summons_and_restores_setup();
    test_cleric_moves_to_heal_distant_ally();
    test_cleric_follows_frontline_when_no_one_is_wounded();
    test_solo_cleric_falls_back_to_attacking();
    test_aoe_unit_prefers_clustered_targets();

    std::cout << "autochess core tests passed\n";
    return 0;
}
