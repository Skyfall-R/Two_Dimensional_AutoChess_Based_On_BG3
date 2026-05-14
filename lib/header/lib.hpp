#ifndef AUTOCHESS_CORE_HPP
#define AUTOCHESS_CORE_HPP

#include <array>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace autochess {

constexpr int kBoardWidth = 33;
constexpr int kBoardHeight = 19;
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
enum class GameStage { Exploration, MainBattle };
enum class MapKind { ExplorationA, ExplorationB, MainBattle };
enum class Phase { Preparation, Combat, Finished };
enum class UnitLayer { Land, Air };
enum class TerrainKind : unsigned char {
    Open,
    Wall,
    MainRoad,
    SideRoad,
    Base,
    TowerPad,
    NeutralCamp,
    Trap,
    BossSite,
    ClearedObjective,
    ClearedBoss
};
enum class RouteNodeType { Combat, Neutral, Elite, Shop, Event, Boss };
enum class NeutralFamily { Swarm, Guardian, Caster, Assassin, Artillery };
enum class RelicTier { Basic, Build, Transform, Unique };
enum class ExplorationObjectiveKind { Camp, Elite, Boss, Trap };
enum class HiddenExplorationEventKind { GoldCache, HealingSpring };
enum class NeutralBehavior { PassiveGuardian, HostileAmbusher };
enum class AbilityKind {
    None,
    GithyankiAstralRaid,
    BarbarianHeavySwing,
    NecromancerSummon,
    MephitDeathBurst,
    PaladinCharge,
    DragonBreath,
    GuardianShield,
    ClericHeal,
    FrostNova,
    RogueAmbush,
    DruidSummon,
    KarnissCruelSting,
    SpectatorWoundingRay,
    MindBlast,
    Counterspell,
    AnimatingSpores,
    OwlbearMultiattack,
    HiemalStrike,
    VenomousBite,
    DiabolicChains,
    KethericSmite,
    SelunesIre,
    EvokerMagicMissile,
    DominatePerson,
    ExtractBrain,
    StrikeOfTheGuardian,
    MinotaurCharge,
    Blight,
    StaggeringSmite,
    ElectrifiedFlail
};
enum class StatusKind {
    Shield,
    Taunt,
    Slow,
    Summoned,
    FirstStrike,
    Stunned,
    Prone,
    Chilled,
    Poisoned,
    Blinded,
    Frightened,
    Staggered
};
enum class DamageType {
    Piercing,
    Slashing,
    Bludgeoning,
    Fire,
    Cold,
    Poison,
    Necrotic,
    Radiant,
    Psychic,
    Force,
    Lightning
};
enum class DamageAffinity {
    Immune,
    Resistant,
    Normal,
    Vulnerable
};
enum class EventType {
    UnitMoved,
    UnitAttacked,
    DamageDealt,
    Healed,
    Shielded,
    StatusApplied,
    AiPolicyStatus,
    UnitDied,
    GoldGained,
    Bought,
    Deployed,
    Upgraded,
    RoundStarted,
    CombatStarted,
    Victory
};
enum class UnitType {
    Skeleton,
    SkeletonByNecromancer,
    GithyankiWarrior,
    Ranger,
    Barbarian,
    Necromancer,
    FireMephit,
    ImpSwarm,
    GoblinSkirmisher,
    Paladin,
    DragonWyrmling,
    NeutralSpectator,
    NeutralOwlbear,
    NeutralMindFlayer,
    NeutralSovereignSpaw,
    NeutralKarniss,
    NeutralRedcap,
    NeutralWaterMyrmidon,
    NeutralPhaseSpiderMatriarch,
    NeutralRaphael,
    NeutralKethericThorm,
    NeutralMoonlightSliver,
    NeutralGuardianOfFaith,
    NeutralMinotaur,
    NeutralDeathKnight,
    NeutralAirMyrmidon,
    DefenseTower,
    ShieldGuardian,
    Cleric,
    Evoker,
    RogueAssassin,
    Druid,
    Treant,
    SporeServant,
    NeutralTamiaHolzt
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

struct RunModifiers {
    int costDiscount = 0;
    int upgradeDiscount = 0;
    int roundIncomeBonus = 0;
    int interestBonus = 0;
    int benchBonus = 0;
    int extraRelicChoices = 0;
    int bonusGoldOnClear = 0;
    int summonLimitBonus = 0;
    std::array<int, 5> familyBias{};
    std::vector<std::string> relicIds;
};

struct EncounterContext {
    bool active = false;
    RouteNodeType nodeType = RouteNodeType::Combat;
    NeutralFamily family = NeutralFamily::Swarm;
    int depth = 0;
    int threatBudget = 0;
    int rewardGold = 0;
    int rewardQuality = 0;
    bool boss = false;
    std::vector<std::string> riskTags;
    std::vector<std::string> rewardTags;
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
    int armorClass = 10;
    int attackBonus = 2;
    int savingThrowBonus = 0;
    int spellSaveDc = 10;
};

struct DamageRoll {
    int diceCount = 0;
    int diceSides = 0;
    int flatBonus = 0;
    DamageType type = DamageType::Force;
};

struct DamagePacket {
    std::vector<DamageRoll> rolls;
    int flatDamage = 0;
    std::string sourceLabel;
};

std::string toString(DamageType type);
std::string toString(DamageAffinity affinity);
DamageType basicDamageTypeFor(UnitType type);
DamageRoll damageRollForValue(int averageDamage, DamageType type);
DamagePacket basicDamagePacketFor(const UnitSpec& spec);
DamagePacket abilityDamagePacketFor(const UnitSpec& spec, AbilityKind ability);
int damagePacketMin(const DamagePacket& packet);
int damagePacketMax(const DamagePacket& packet);
std::string damageFormula(const DamagePacket& packet);
std::string damageRange(const DamagePacket& packet);
std::string damageLine(const DamagePacket& packet);
DamageAffinity damageAffinity(UnitType type, DamageType damageType);
std::string damageAffinitySummary(UnitType type);

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

} // namespace autochess

#include <exploration.hpp>

namespace autochess {

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
    int purchaseValue = 0;
    std::vector<int> hp;
    int shield = 0;
    double moveProgress = 0.0;
    double attackTimer = 0.0;
    double abilityTimer = 0.0;
    double retargetTimer = 0.0;
    double lifespan = -1.0;
    UnitId target = kInvalidUnitId;
    bool firstStrikeReady = false;
    bool neutralProvoked = false;
    bool neutralReturningHome = false;
    NeutralBehavior neutralBehavior = NeutralBehavior::PassiveGuardian;
    UnitId provokedBy = kInvalidUnitId;
    int specialCounter = 0;
    bool counterspellUsed = false;
    bool oneShotAbilityUsed = false;
    bool neutralControlled = false;
    int stuckTicks = 0;
    Coord lastCoord;
    Coord homeCoord;
    std::vector<StatusEffect> statuses;
};

struct Cell {
    std::vector<UnitId> land;
    std::vector<UnitId> air;
    TerrainKind terrain = TerrainKind::Open;
};

struct Board {
    int width = kBoardWidth;
    int height = kBoardHeight;
    std::vector<Cell> cells;

    Board();
    bool inBounds(Coord coord) const;
    Cell& at(Coord coord);
    const Cell& at(Coord coord) const;
    TerrainKind terrainAt(Coord coord) const;
    void setTerrain(Coord coord, TerrainKind terrain);
    bool blocked(Coord coord) const;
    UnitId occupant(Coord coord, UnitLayer layer) const;
    const std::vector<UnitId>& occupants(Coord coord, UnitLayer layer) const;
    void setOccupant(Coord coord, UnitLayer layer, UnitId id);
    void addOccupant(Coord coord, UnitLayer layer, UnitId id);
    void removeOccupant(Coord coord, UnitLayer layer, UnitId id);
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
    AbilityKind ability = AbilityKind::None;
    bool deployed = false;
    bool alive = false;
    bool upgraded = false;
    bool neutralControlled = false;
    bool neutralActivated = false;
    bool neutralReturningHome = false;
    UnitId targetId = kInvalidUnitId;
    int units = 0;
    int maxUnits = 0;
    std::vector<int> hp;
    int totalHp = 0;
    int maxTotalHp = 0;
    int shield = 0;
    int attack = 0;
    int range = 0;
    int cost = 0;
    int armorClass = 10;
    int attackBonus = 0;
    int savingThrowBonus = 0;
    int spellSaveDc = 10;
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
    GameStage stage = GameStage::Exploration;
    MapKind mapKind = MapKind::ExplorationA;
    int explorationRound = 0;
    int explorationRoundLimit = 8;
    int explorationRoundsRemaining = 8;
    bool explorationRoundLimitLocked = false;
    int explorationObjectivesCleared = 0;
    int explorationObjectivesTotal = 0;
    int bossesCleared = 0;
    int eventsTriggered = 0;
    int trapsTriggered = 0;
    int hiddenEventsClaimed = 0;
    int randomGoldEventsClaimed = 0;
    int randomGoldEventsTotal = 0;
    Phase phase = Phase::Preparation;
    std::optional<PlayerId> winner;
    std::array<PlayerView, 2> players;
    std::vector<TerrainKind> terrain;
    std::vector<UnitView> units;
};

class GameEngine;

class ScriptedNormalAiPlanner;
class AiPlanner {
public:
    virtual ~AiPlanner() = default;
    virtual std::optional<AiAction> chooseAction(GameEngine& engine,
                                                 PlayerId player,
                                                 const std::vector<AiAction>& legalActions) = 0;
    virtual std::string name() const = 0;
    // Polymorphic deep copy used by GameEngine's copy constructor (MCTS clone).
    virtual std::unique_ptr<AiPlanner> clone() const = 0;
};

class HeuristicAiPlanner;
class PolicyAiPlanner;

class GameEngine {
public:
    explicit GameEngine(unsigned seed = 1);
    // Deep copy for MCTS forking. Copies engine state and clones the AI planner.
    GameEngine(const GameEngine& other);
    GameEngine& operator=(const GameEngine& other);
    GameEngine(GameEngine&&) noexcept = default;
    GameEngine& operator=(GameEngine&&) noexcept = default;

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
    int effectiveBuyCost(PlayerId player, const UnitSpec& spec) const;
    int effectiveUpgradeCost(PlayerId player, const UnitSpec& spec) const;
    int benchLimit(PlayerId player) const;
    bool canDeploy(PlayerId player, Coord coord, UnitLayer layer) const;
    bool isDeploymentCell(PlayerId player, Coord coord) const;
    bool isExplorationStagingCell(PlayerId player, Coord coord) const;
    Coord baseCoord(PlayerId player) const;
    void setExplorationRoundLimit(int rounds);
    void lockExplorationRoundLimit();
    int explorationRoundLimit() const;
    GameStage stage() const;
    void setRunModifiers(PlayerId player, const RunModifiers& modifiers);
    const RunModifiers& runModifiers(PlayerId player) const;
    void setEncounterContext(const EncounterContext& context);
    const EncounterContext& encounterContext() const;
    void grantGold(PlayerId player, int amount);
    void setAiDifficulty(AiDifficulty difficulty);
    AiDifficulty aiDifficulty() const;
    void setAiPolicyDirectory(const std::string& directory);
    const std::string& aiPolicyDirectory() const;
    const AiPolicyMetadata& aiPolicyMetadata() const;
    AiFeatureSchema aiFeatureSchema() const;
    std::string rulesFingerprint() const;
    bool debugTriggerTrap(PlayerId triggeringPlayer, Coord coord);
    bool debugTriggerRandomGold(PlayerId triggeringPlayer, Coord coord);
    bool debugTriggerHiddenEvent(PlayerId triggeringPlayer, Coord coord, UnitId triggerUnitId = kInvalidUnitId);
    bool debugApplyDamage(UnitId targetId, int amount, DamageType type = DamageType::Force);
    bool debugApplyDamageFrom(UnitId sourceId, UnitId targetId, int amount, DamageType type = DamageType::Force);
    UnitId debugCreateUnit(PlayerId owner, UnitType type, Coord coord);
    bool debugKnockback(UnitId targetId, Coord source, int distance, UnitId sourceId = kInvalidUnitId);
    bool debugRadialKnockback(Coord center, int radius, int distance, UnitId sourceId = kInvalidUnitId);
    std::vector<Coord> debugRandomGoldCoords() const;
    std::vector<Coord> debugHiddenHealingCoords() const;
    std::vector<AiAction> legalActions(PlayerId player) const;
    bool applyAiAction(PlayerId player, const AiAction& action);
    std::vector<double> stateFeatures(PlayerId player) const;
    std::vector<double> actionFeatures(PlayerId player, const AiAction& action) const;
    void prepareAiPlayer(PlayerId player);

private:
    friend class ScriptedNormalAiPlanner;
    friend class HeuristicAiPlanner;
    friend class PolicyAiPlanner;

    struct PathResult {
        bool found = false;
        std::vector<Coord> steps;
    };

    struct KnockbackMove {
        UnitId id = kInvalidUnitId;
        Coord from;
        Coord to;
        bool primary = false;
    };

    struct KnockbackResult {
        bool moved = false;
        bool blockedAtEnd = false;
        std::vector<KnockbackMove> moves;
    };

    struct CorpseState {
        Coord coord;
        PlayerId owner = PlayerId::One;
        UnitType type = UnitType::Skeleton;
        int round = 0;
        bool consumed = false;
    };

    std::array<PlayerState, 2> players_;
    Board board_;
    std::vector<Unit> units_;
    ExplorationState exploration_;
    std::vector<CorpseState> corpses_;
    std::vector<UnitSpec> specs_;
    std::vector<Event> events_;
    std::unique_ptr<AiPlanner> aiPlanner_;
    GameConfig config_;
    AiPolicyMetadata aiPolicyMetadata_;
    mutable std::mt19937 rng_;
    GameMode mode_ = GameMode::SinglePlayerVsAi;
    GameStage stage_ = GameStage::Exploration;
    MapKind mapKind_ = MapKind::ExplorationA;
    int explorationMapTemplate_ = 0;
    Phase phase_ = Phase::Preparation;
    std::optional<PlayerId> winner_;
    int round_ = 0;
    double time_ = 0.0;
    double combatTime_ = 0.0;
    int explorationRound_ = 0;
    int explorationRoundLimit_ = 8;
    bool explorationRoundLimitLocked_ = false;
    int nextUnitId_ = 0;
    std::array<RunModifiers, 2> runModifiers_{};
    EncounterContext encounterContext_{};

    PlayerState& player(PlayerId id);
    const PlayerState& player(PlayerId id) const;
    PlayerId opponent(PlayerId id) const;
    Unit& unit(UnitId id);
    const Unit& unit(UnitId id) const;

    void resetBoard(MapKind kind);
    void spawnDefenseTowers();
    UnitId createUnit(PlayerId owner, UnitType type);
    bool placeUnit(UnitId id, Coord coord);
    void removeFromBoard(UnitId id);
    void convertUnitOwner(UnitId id, PlayerId newOwner);
    void pushEvent(Event event);
    void addRecentBuy(PlayerState& player, UnitType type);
    void randomizeExplorationObjectives();
    void initializeNeutralObjectives();
    void initializeHiddenExplorationEvents();
    void applyExplorationObjectiveTerrain();
    bool triggerTrapAt(PlayerId triggeringPlayer, Coord coord, UnitId triggerUnitId = kInvalidUnitId);
    bool triggerRandomGoldEventAt(PlayerId triggeringPlayer, Coord coord, UnitId triggerUnitId);
    bool canTriggerHiddenEvent(const Unit& unit) const;
    bool triggerHiddenEventAt(PlayerId triggeringPlayer, Coord coord, UnitId triggerUnitId);
    int spawnRedcapAmbush(PlayerId triggeringPlayer, Coord origin, UnitId triggerUnitId);
    void clearExplorationObjective(size_t index, PlayerId clearer, UnitId actorId);
    void updateExplorationObjectiveForDeath(UnitId deadId, UnitId sourceId);
    void startCombatIfReady();
    void startCombat();
    void finishCombat(PlayerId winner);
    void startNextRound();
    void startNextRound(const std::string& reason);
    void transitionToMainBattle();
    bool shouldTransitionFromExploration() const;
    std::string explorationCompletionReason() const;
    void resolveExplorationEndEconomy();
    int explorationRefundFor(const Unit& unit) const;
    int explorationObjectivesCleared() const;
    int explorationObjectivesTotal() const;
    int explorationBossesCleared() const;
    int explorationEventsTriggered() const;
    int explorationTrapsTriggered() const;
    int hiddenEventsClaimed() const;
    int randomGoldEventsClaimed() const;
    void resetCombatantsForPreparation();
    void ensureAiPlanner();
    void loadAiPlanner();
    void useScriptedNormalAiPlanner();
    void useHeuristicAiPlanner(const std::string& status);
    void aiPrepare(PlayerId player);
    std::optional<AiAction> chooseScriptedNormalAction(PlayerId player,
                                                       const std::vector<AiAction>& legalActions) const;
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
    std::optional<size_t> selectCorpseForSpores(const Unit& unit) const;
    std::optional<Coord> selectSporeSpawnCell(Coord corpseCoord) const;
    bool canAttack(const Unit& attacker, const Unit& target) const;
    bool inAttackRange(const Unit& attacker, const Unit& target) const;
    double effectiveSpeed(const Unit& unit) const;
    double effectiveAttackCooldown(const Unit& unit) const;
    int effectiveArmorClass(const Unit& unit) const;
    int effectiveAttackBonus(const Unit& unit) const;
    int rosterCount(PlayerId player) const;
    bool hasStatus(const Unit& unit, StatusKind kind) const;
    void addStatus(UnitId id, StatusEffect status);
    bool canNeutralAct(const Unit& unit) const;
    bool canActivateNeutralFrom(UnitId sourceId) const;
    void activateNeutral(UnitId neutralId, UnitId sourceId, const std::string& reason);
    void provokeNeutral(UnitId neutralId, UnitId sourceId);
    bool isNeutralGuardianUnit(const Unit& unit) const;
    bool isNeutralSpawServant(const Unit& unit) const;
    bool isNeutralLikeCombatant(const Unit& unit) const;
    bool withinNeutralLeash(const Unit& unit, Coord coord) const;
    UnitId selectDominatePersonTarget(const Unit& caster) const;
    void triggerTamiaDominate(UnitId id);
    int baseSummonLimitFor(const Unit& caster) const;
    int activeSummonCountFor(UnitId casterId) const;
    int summonLimitFor(const Unit& caster) const;
    bool canSummonMore(UnitId casterId) const;
    int rollD20(int advantageScore);
    int rollDice(int count, int sides);
    bool savingThrowSucceeds(UnitId targetId, UnitId sourceId, const std::string& saveName,
                             int advantageScore = 0);
    bool knockbackUnit(UnitId targetId, Coord source, int distance, UnitId sourceId);
    KnockbackResult resolveKnockbackLine(UnitId targetId, Coord source, int distance, UnitId sourceId);
    KnockbackResult resolveRadialKnockback(Coord center, int radius, int distance, UnitId sourceId);
    bool isPushableLandUnit(const Unit& unit) const;
    bool isLandOccupiedByBlockingUnit(Coord coord) const;
    int rollDamageRoll(const DamageRoll& roll);
    DamagePacket scaledDamagePacket(DamagePacket packet, int numerator, int denominator = 1) const;
    void applyDamage(UnitId targetId, const DamagePacket& packet, UnitId sourceId);
    void applyDamage(UnitId targetId, int amount, DamageType type, UnitId sourceId);
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
    bool neutralCanLeaveHome(const Unit& unit) const;
    std::optional<Coord> chooseNeutralGuardianStep(const Unit& unit, const std::vector<Coord>& reserved) const;
    std::optional<Coord> chooseNeutralSummonGuardStep(const Unit& unit, const std::vector<Coord>& reserved) const;
    std::optional<Coord> chooseSupportStep(const Unit& unit, const std::vector<Coord>& reserved) const;
    std::optional<Coord> chooseFollowAllyGoal(const Unit& unit) const;
    std::optional<Coord> chooseExplorationGoal(const Unit& unit) const;
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
    void performGithyankiAstralRaid(UnitId id);
};

std::string toString(PlayerId player);
std::string toString(AiDifficulty difficulty);
std::string toString(AiActionKind kind);
std::string toString(GameStage stage);
std::string toString(Phase phase);
std::string toString(UnitLayer layer);
std::string toString(UnitType type);
bool isInternalUnit(UnitType type);
bool isNeutralMonster(UnitType type);
AiDifficulty aiDifficultyFromString(const std::string& text);

} // namespace autochess

#endif
