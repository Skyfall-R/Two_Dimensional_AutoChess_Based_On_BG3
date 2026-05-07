#ifndef AUTOCHESS_CORE_HPP
#define AUTOCHESS_CORE_HPP

#include <array>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace autochess {

constexpr int kBoardWidth = 11;
constexpr int kBoardHeight = 7;
constexpr int kInvalidUnitId = -1;

using UnitId = int;

struct Coord {
    int x = 0;
    int y = 0;
};

bool operator==(Coord lhs, Coord rhs);
bool operator!=(Coord lhs, Coord rhs);
int manhattan(Coord lhs, Coord rhs);

enum class PlayerId { One = 0, Two = 1 };
enum class GameMode { SinglePlayerVsAi, TwoPlayer };
enum class AiDifficulty { Normal, Hard, SuperHard };
enum class Phase { Preparation, Combat, Finished };
enum class UnitLayer { Land, Air };
enum class AbilityKind {
    None,
    PekkaHeavySwing,
    WitchSummon,
    BalloonBomb,
    PrinceCharge,
    DragonBreath,
    ShieldGuard,
    ClericHeal,
    FrostNova,
    BomberSplash,
    AssassinLeap,
    DruidSummon,
    LancerPierce,
    StormChain
};
enum class StatusKind { Shield, Taunt, Slow, Summoned, FirstStrike };
enum class EventType {
    UnitMoved,
    UnitAttacked,
    DamageDealt,
    Healed,
    Shielded,
    StatusApplied,
    AiPolicyStatus,
    UnitDied,
    Bought,
    Deployed,
    Upgraded,
    RoundStarted,
    CombatStarted,
    Victory
};
enum class UnitType {
    Skeleton,
    SkeletonByWitch,
    Knight,
    Archer,
    Pekka,
    Witch,
    Balloon,
    Minions,
    Goblin,
    Prince,
    BabyDragon,
    DefenseTower,
    ShieldGuard,
    Cleric,
    FrostMage,
    Bomber,
    ShadowAssassin,
    Druid,
    Treant,
    Lancer,
    StormSpirit
};

enum class AiActionKind {
    Buy,
    Deploy,
    MoveDeployed,
    ReturnToBench,
    Upgrade,
    Ready
};

struct GameConfig {
    GameMode mode = GameMode::SinglePlayerVsAi;
    AiDifficulty aiDifficulty = AiDifficulty::Normal;
    std::string aiPolicyDirectory = "assets/ai";
};

struct UnitSpec {
    UnitType type = UnitType::Skeleton;
    std::string name;
    std::string shortName;
    int cost = 0;
    int unitCount = 1;
    int maxHp = 1;
    int attack = 1;
    int range = 1;
    double speed = 1.0;
    double attackCooldown = 0.8;
    UnitLayer layer = UnitLayer::Land;
    bool canAttackLand = true;
    bool canAttackAir = false;
    int threat = 10;
    int roleMask = 0;
    AbilityKind ability = AbilityKind::None;
    double abilityCooldown = 0.0;
    int abilityValue = 0;
    int abilityRange = 0;
    double abilityDuration = 0.0;
};

struct AiAction {
    AiActionKind kind = AiActionKind::Ready;
    UnitType type = UnitType::Skeleton;
    UnitId unitId = kInvalidUnitId;
    Coord coord;
};

struct AiPolicyMetadata {
    std::string format = "autochess_policy_v1";
    std::string modelVersion;
    std::string difficulty;
    std::string rulesFingerprint;
    int stateFeatureCount = 0;
    int actionFeatureCount = 0;
    double heuristicBlend = 0.0;
    double bias = 0.0;
    bool loaded = false;
    bool valid = false;
    std::string path;
    std::string status;
};

struct AiFeatureSchema {
    int stateFeatureCount = 0;
    int actionFeatureCount = 0;
    std::vector<std::string> stateFeatureGroups;
    std::vector<std::string> actionFeatureGroups;
};

struct StatusEffect {
    StatusKind kind = StatusKind::Slow;
    double remaining = 0.0;
    int value = 0;
    UnitId source = kInvalidUnitId;
};

struct Unit {
    UnitId id = kInvalidUnitId;
    UnitSpec spec;
    PlayerId owner = PlayerId::One;
    Coord coord;
    bool deployed = false;
    bool alive = true;
    bool upgraded = false;
    std::vector<int> hp;
    int shield = 0;
    double moveProgress = 0.0;
    double attackTimer = 0.0;
    double abilityTimer = 0.0;
    double retargetTimer = 0.0;
    double lifespan = -1.0;
    UnitId target = kInvalidUnitId;
    bool firstStrikeReady = false;
    int specialCounter = 0;
    int stuckTicks = 0;
    Coord lastCoord;
    Coord homeCoord;
    std::vector<StatusEffect> statuses;
};

struct Cell {
    UnitId land = kInvalidUnitId;
    UnitId air = kInvalidUnitId;
};

struct Board {
    int width = kBoardWidth;
    int height = kBoardHeight;
    std::vector<Cell> cells;

    Board();
    bool inBounds(Coord coord) const;
    Cell& at(Coord coord);
    const Cell& at(Coord coord) const;
    UnitId occupant(Coord coord, UnitLayer layer) const;
    void setOccupant(Coord coord, UnitLayer layer, UnitId id);
};

struct PlayerState {
    PlayerId id = PlayerId::One;
    std::string name;
    bool isAi = false;
    int money = 0;
    bool ready = false;
    std::vector<UnitId> bench;
    std::vector<UnitId> deployed;
    std::vector<UnitType> recentBuys;
};

struct Event {
    EventType type = EventType::UnitMoved;
    PlayerId player = PlayerId::One;
    UnitId actor = kInvalidUnitId;
    UnitId target = kInvalidUnitId;
    Coord from;
    Coord to;
    int amount = 0;
    std::string text;
};

struct UnitView {
    UnitId id = kInvalidUnitId;
    UnitType type = UnitType::Skeleton;
    std::string name;
    std::string shortName;
    PlayerId owner = PlayerId::One;
    Coord coord;
    UnitLayer layer = UnitLayer::Land;
    bool deployed = false;
    bool alive = false;
    bool upgraded = false;
    int units = 0;
    int maxUnits = 0;
    int totalHp = 0;
    int maxTotalHp = 0;
    int shield = 0;
    int attack = 0;
    int range = 0;
    int cost = 0;
    bool slowed = false;
    bool taunting = false;
};

struct PlayerView {
    PlayerId id = PlayerId::One;
    std::string name;
    bool isAi = false;
    int money = 0;
    bool ready = false;
    std::vector<UnitId> bench;
    std::vector<UnitId> deployed;
};

struct GameSnapshot {
    int width = kBoardWidth;
    int height = kBoardHeight;
    int round = 0;
    double time = 0.0;
    double combatTime = 0.0;
    Phase phase = Phase::Preparation;
    std::optional<PlayerId> winner;
    std::array<PlayerView, 2> players;
    std::vector<UnitView> units;
};

class GameEngine;

class AiPlanner {
public:
    virtual ~AiPlanner() = default;
    virtual std::optional<AiAction> chooseAction(GameEngine& engine,
                                                 PlayerId player,
                                                 const std::vector<AiAction>& legalActions) = 0;
    virtual std::string name() const = 0;
};

class HeuristicAiPlanner;
class PolicyAiPlanner;

class GameEngine {
public:
    explicit GameEngine(unsigned seed = 1);

    void startNewGame(GameMode mode = GameMode::SinglePlayerVsAi);
    void startNewGame(const GameConfig& config);
    bool buyUnit(PlayerId player, UnitType type);
    bool deployUnit(PlayerId player, UnitId unitId, Coord coord);
    bool moveDeployedUnit(PlayerId player, UnitId unitId, Coord coord);
    bool returnToBench(PlayerId player, UnitId unitId);
    bool upgradeUnit(PlayerId player, UnitId unitId);
    void setReady(PlayerId player, bool ready);
    void tick(double dt);

    GameSnapshot snapshot() const;
    std::vector<Event> consumeEvents();
    const std::vector<UnitSpec>& shop() const;
    const UnitSpec* specFor(UnitType type) const;
    bool canDeploy(PlayerId player, Coord coord, UnitLayer layer) const;
    bool isDeploymentCell(PlayerId player, Coord coord) const;
    Coord baseCoord(PlayerId player) const;
    void setAiDifficulty(AiDifficulty difficulty);
    AiDifficulty aiDifficulty() const;
    void setAiPolicyDirectory(const std::string& directory);
    const std::string& aiPolicyDirectory() const;
    const AiPolicyMetadata& aiPolicyMetadata() const;
    AiFeatureSchema aiFeatureSchema() const;
    std::string rulesFingerprint() const;
    std::vector<AiAction> legalActions(PlayerId player) const;
    bool applyAiAction(PlayerId player, const AiAction& action);
    std::vector<double> stateFeatures(PlayerId player) const;
    std::vector<double> actionFeatures(PlayerId player, const AiAction& action) const;
    void prepareAiPlayer(PlayerId player);

private:
    friend class HeuristicAiPlanner;
    friend class PolicyAiPlanner;

    struct PathResult {
        bool found = false;
        std::vector<Coord> steps;
    };

    std::array<PlayerState, 2> players_;
    Board board_;
    std::vector<Unit> units_;
    std::vector<UnitSpec> specs_;
    std::vector<Event> events_;
    std::unique_ptr<AiPlanner> aiPlanner_;
    GameConfig config_;
    AiPolicyMetadata aiPolicyMetadata_;
    mutable std::mt19937 rng_;
    GameMode mode_ = GameMode::SinglePlayerVsAi;
    Phase phase_ = Phase::Preparation;
    std::optional<PlayerId> winner_;
    int round_ = 0;
    double time_ = 0.0;
    double combatTime_ = 0.0;
    int nextUnitId_ = 0;

    PlayerState& player(PlayerId id);
    const PlayerState& player(PlayerId id) const;
    PlayerId opponent(PlayerId id) const;
    Unit& unit(UnitId id);
    const Unit& unit(UnitId id) const;

    void resetBoard();
    UnitId createUnit(PlayerId owner, UnitType type);
    bool placeUnit(UnitId id, Coord coord);
    void removeFromBoard(UnitId id);
    void pushEvent(Event event);
    void addRecentBuy(PlayerState& player, UnitType type);
    void startCombatIfReady();
    void startCombat();
    void finishCombat(PlayerId winner);
    void startNextRound();
    void resetCombatantsForPreparation();
    void ensureAiPlanner();
    void loadAiPlanner();
    void useHeuristicAiPlanner(const std::string& status);
    void aiPrepare(PlayerId player);
    std::optional<AiAction> chooseHeuristicAction(PlayerId player,
                                                  const std::vector<AiAction>& legalActions) const;
    double heuristicActionScore(PlayerId player, const AiAction& action) const;
    double aiPurchaseScore(PlayerId player, const UnitSpec& spec) const;
    int aiDeploymentScore(PlayerId player, const Unit& unit, Coord coord) const;

    void tickStatuses(Unit& unit, double dt);
    void tickAbilities(UnitId id, double dt);
    void tickCombat(double dt);
    void refreshTarget(Unit& unit, bool force);
    UnitId selectTarget(const Unit& unit) const;
    UnitId selectHealTarget(const Unit& healer) const;
    UnitId selectGlobalHealTarget(const Unit& healer) const;
    UnitId selectFollowAlly(const Unit& unit) const;
    bool canAttack(const Unit& attacker, const Unit& target) const;
    bool inAttackRange(const Unit& attacker, const Unit& target) const;
    double effectiveSpeed(const Unit& unit) const;
    double effectiveAttackCooldown(const Unit& unit) const;
    bool hasStatus(const Unit& unit, StatusKind kind) const;
    void addStatus(UnitId id, StatusEffect status);
    double clusterScoreAround(const Unit& attacker, Coord center) const;

    void attack(UnitId attackerId, UnitId targetId);
    void applyDamage(UnitId targetId, int amount, UnitId sourceId);
    void applyHeal(UnitId targetId, int amount, UnitId sourceId);
    void addShield(UnitId targetId, int amount, UnitId sourceId);
    void killUnit(UnitId id, UnitId sourceId);
    void clearDeadUnits();
    void resolveVictory();
    bool hasActiveCombatUnit(PlayerId player) const;

    void moveUnits(double dt);
    std::optional<Coord> chooseNextStep(UnitId id, const std::vector<Coord>& reserved) const;
    std::optional<Coord> chooseSupportStep(const Unit& unit, const std::vector<Coord>& reserved) const;
    std::optional<Coord> chooseFollowAllyGoal(const Unit& unit) const;
    std::optional<Coord> chooseAirStep(const Unit& unit, Coord target, const std::vector<Coord>& reserved) const;
    PathResult findPathToAttackCell(const Unit& unit, Coord target, int range) const;
    PathResult findPathToGoal(const Unit& unit, Coord goal) const;
    bool passableForLand(const Unit& unit, Coord coord) const;
    std::vector<Coord> neighbors4(Coord coord) const;
    std::vector<Coord> cellsInRange(Coord center, int range) const;
    std::vector<Coord> adjacentCells(Coord center) const;
    Coord forwardCoord(PlayerId player, Coord coord) const;
    std::optional<Coord> findSummonCell(PlayerId owner, Coord origin, UnitLayer layer) const;
    void performAssassinLeap(UnitId id);
};

std::string toString(PlayerId player);
std::string toString(AiDifficulty difficulty);
std::string toString(AiActionKind kind);
std::string toString(Phase phase);
std::string toString(UnitLayer layer);
std::string toString(UnitType type);
AiDifficulty aiDifficultyFromString(const std::string& text);

} // namespace autochess

#endif
