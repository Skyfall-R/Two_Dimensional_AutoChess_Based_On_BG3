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
enum class MapKind { ExplorationA, ExplorationB };
enum class Phase { Preparation, Combat, Finished };
enum class UnitLayer { Land, Air };
enum class TerrainKind : unsigned char {
    Open,
    Wall,
    SideRoad,
    NeutralCamp,
    Trap,
    BossSite,
    ClearedObjective,
    ClearedBoss
};
enum class NeutralFamily { Swarm, Guardian, Caster, Assassin, Artillery };
enum class RelicTier { Basic, Build, Transform, Unique };
enum class ExplorationObjectiveKind { Camp, Elite, Boss, Trap };
enum class HiddenExplorationEventKind {
    GoldCache,
    HealingSpring,
    ArcaneFont,
    SmugglerCache,
    CursedIdol,
    RallyBanner,
    SilentWaystone
};
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
    ElectrifiedFlail,
    BossFireball
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
enum class ProfileKind {
    PlayableCharacter,
    NamedActor,
    PureMonster,
    Summon
};
enum class AbilityScoreKind {
    Strength,
    Dexterity,
    Constitution,
    Intelligence,
    Wisdom,
    Charisma
};
enum class Race {
    None,
    Human,
    Elf,
    Dwarf,
    Tiefling,
    Githyanki,
    Goblin,
    Myconid
};
enum class Subrace {
    None,
    HighElf,
    WoodElf,
    ShieldDwarf,
    GoldDwarf,
    Drow
};
enum class CharacterClass {
    None,
    Barbarian,
    Bard,
    Cleric,
    Druid,
    Fighter,
    Monk,
    Paladin,
    Ranger,
    Rogue,
    Sorcerer,
    Warlock,
    Wizard
};
enum class Background {
    None,
    Acolyte,
    Charlatan,
    Criminal,
    FolkHero,
    Noble,
    Outlander,
    Sage,
    Soldier,
    Urchin
};
enum class CreatureType {
    None,
    Humanoid,
    Undead,
    Fiend,
    Construct,
    Monstrosity,
    Aberration,
    Plant,
    Fey,
    Elemental,
    Celestial,
    Dragon,
    Beast
};
enum class ArmorTraining {
    None,
    Light,
    Medium,
    Heavy,
    Shield
};
enum class WeaponTraining {
    None,
    Simple,
    Martial,
    Natural
};
enum class SkillTag {
    None,
    Perception,
    Deception,
    Stealth,
    SleightOfHand,
    Persuasion,
    Intimidation,
    Arcana,
    Religion,
    Nature,
    Survival,
    Athletics,
    Acrobatics,
    Medicine,
    Insight,
    History
};
enum class TraitTag {
    None,
    Darkvision,
    FeyAncestry,
    MartialTraining,
    Spellcasting,
    NaturalArmor,
    Charge,
    Flying,
    Summoned,
    Constructed,
    UndeadFortitude,
    Psionics,
    Regeneration,
    Taunt,
    PackTactics,
    FireAffinity,
    Poison,
    RadiantAura,
    Shapeshift,
    Ambusher
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

struct AbilityScores {
    int strength = 10;
    int dexterity = 10;
    int constitution = 10;
    int intelligence = 10;
    int wisdom = 10;
    int charisma = 10;
};

struct UnitProfile {
    ProfileKind kind = ProfileKind::PureMonster;
    Race race = Race::None;
    Subrace subrace = Subrace::None;
    CharacterClass characterClass = CharacterClass::None;
    Background background = Background::None;
    CreatureType creatureType = CreatureType::None;
    std::string archetype;
    int level = 1;
    int tier = 1;
    AbilityScores abilityScores;
    AbilityScoreKind attackAbility = AbilityScoreKind::Strength;
    AbilityScoreKind castingAbility = AbilityScoreKind::Intelligence;
    std::vector<AbilityScoreKind> savingThrowProficiencies;
    std::vector<SkillTag> skills;
    ArmorTraining armorTraining = ArmorTraining::None;
    WeaponTraining weaponTraining = WeaponTraining::Simple;
    int armorBase = 10;
    int armorDexCap = 99;
    int shieldBonus = 0;
    int naturalArmorBonus = 0;
    int hitDie = 8;
    int hitDice = 1;
    int weaponDamageAverage = 4;
    double movementMeters = 9.0;
    std::vector<TraitTag> traits;
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
    int hiddenEventRevealBonus = 0;
    int hiddenEventPayoutBonus = 0;
    int trapCheckBonus = 0;
    int roundStartShield = 0;
    int eventHealBonus = 0;
    int hiddenEventCountBonus = 0;
    int trapDisarmGoldBonus = 0;
    int cursedIdolPayoutBonus = 0;
    int cursedIdolDamageReductionPercent = 0;
    int rallyDurationBonus = 0;
    bool smugglerAlwaysSucceeds = false;
    bool healingSpringCleanses = false;
    std::array<int, 5> familyBias{};
    std::vector<std::string> relicIds;
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
    UnitProfile profile;
    double hpScale = 1.0;
    double damageScale = 1.0;
    int flatHpBonus = 0;
    int attackTuning = 0;
    int damageTuning = 0;
    int acTuning = 0;
    int dcTuning = 0;
    double speedTuning = 0.0;
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
std::string toString(ProfileKind kind);
std::string toString(AbilityScoreKind ability);
std::string toString(Race race);
std::string toString(Subrace subrace);
std::string toString(CharacterClass characterClass);
std::string toString(Background background);
std::string toString(CreatureType creatureType);
std::string toString(SkillTag skill);
std::string toString(TraitTag trait);
int abilityModifier(int score);
int proficiencyBonusForLevel(int levelOrTier);
int abilityScore(const AbilityScores& scores, AbilityScoreKind ability);
int abilityScore(const UnitProfile& profile, AbilityScoreKind ability);
int savingThrowBonusFor(const UnitSpec& spec, AbilityScoreKind ability);
int skillBonusFor(const UnitSpec& spec, SkillTag skill);
bool hasSkillProficiency(const UnitProfile& profile, SkillTag skill);
std::string profileSummary(const UnitProfile& profile);
std::string abilityScoreSummary(const AbilityScores& scores);
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
    std::array<int, 6> savingThrowBonuses{};
    int spellSaveDc = 10;
    UnitProfile profile;
    std::string profileSummary;
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
    MapKind mapKind = MapKind::ExplorationA;
    int explorationMapVariant = 0;
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
    std::array<int, 2> explorationScores{};
    std::array<int, 2> explorationObjectivesClearedByPlayer{};
    std::array<int, 2> explorationBossesClearedByPlayer{};
    std::array<int, 2> hiddenEventsClaimedByPlayer{};
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
class MlpAiPlanner;
class MctsAiPlanner;

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
    void setExplorationRoundLimit(int rounds);
    void lockExplorationRoundLimit();
    int explorationRoundLimit() const;
    void setRunModifiers(PlayerId player, const RunModifiers& modifiers);
    const RunModifiers& runModifiers(PlayerId player) const;
    void grantGold(PlayerId player, int amount);
    void setAiDifficulty(AiDifficulty difficulty);
    AiDifficulty aiDifficulty() const;
    void setAiPolicyDirectory(const std::string& directory);
    const std::string& aiPolicyDirectory() const;
    const AiPolicyMetadata& aiPolicyMetadata() const;
    AiFeatureSchema aiFeatureSchema() const;
    std::string rulesFingerprint() const;
    bool debugTriggerTrap(PlayerId triggeringPlayer, Coord coord, UnitId triggerUnitId = kInvalidUnitId);
    bool debugTriggerRandomGold(PlayerId triggeringPlayer, Coord coord);
    bool debugTriggerHiddenEvent(PlayerId triggeringPlayer, Coord coord, UnitId triggerUnitId = kInvalidUnitId);
    bool debugClearVisibleObjectives(PlayerId clearer);
    bool debugApplyDamage(UnitId targetId, int amount, DamageType type = DamageType::Force);
    bool debugApplyDamageFrom(UnitId sourceId, UnitId targetId, int amount, DamageType type = DamageType::Force);
    UnitId debugCreateUnit(PlayerId owner, UnitType type, Coord coord);
    bool debugSetUnitArmorClass(UnitId unitId, int armorClass);
    bool debugSetUnitSpeed(UnitId unitId, double speed);
    bool debugSetUnitAbilityScore(UnitId unitId, AbilityScoreKind ability, int score);
    bool debugSetUnitSkillProficiency(UnitId unitId, SkillTag skill, bool proficient);
    bool debugDetectHiddenEvent(PlayerId triggeringPlayer, Coord movedCoord, UnitId triggerUnitId);
    bool debugKnockback(UnitId targetId, Coord source, int distance, UnitId sourceId = kInvalidUnitId);
    bool debugRadialKnockback(Coord center, int radius, int distance, UnitId sourceId = kInvalidUnitId);
    std::vector<Coord> debugRandomGoldCoords() const;
    std::vector<Coord> debugHiddenHealingCoords() const;
    std::vector<Coord> debugHiddenEventCoords(HiddenExplorationEventKind kind) const;
    std::vector<AiAction> legalActions(PlayerId player) const;
    bool applyAiAction(PlayerId player, const AiAction& action);
    std::vector<double> stateFeatures(PlayerId player) const;
    std::vector<double> actionFeatures(PlayerId player, const AiAction& action) const;
    void prepareAiPlayer(PlayerId player);

private:
    friend class ScriptedNormalAiPlanner;
    friend class HeuristicAiPlanner;
    friend class PolicyAiPlanner;
    friend class MlpAiPlanner;
    friend class MctsAiPlanner;

    struct PathResult {
        bool found = false;
        std::vector<Coord> steps;
    };

    struct TargetCandidate {
        UnitId id = kInvalidUnitId;
        int distance = 0;
        int pathCost = std::numeric_limits<int>::max();
        bool inRange = false;
        bool reachable = false;
        bool attacksMe = false;
        bool forced = false;
        double score = -std::numeric_limits<double>::infinity();
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
    MapKind mapKind_ = MapKind::ExplorationA;
    int explorationMapTemplate_ = 0;
    int explorationMapVariant_ = 0;
    Phase phase_ = Phase::Preparation;
    std::optional<PlayerId> winner_;
    int round_ = 0;
    double time_ = 0.0;
    double combatTime_ = 0.0;
    double lastCombatProgressTime_ = 0.0;
    int explorationRound_ = 0;
    int explorationRoundLimit_ = 8;
    bool explorationRoundLimitLocked_ = false;
    int nextUnitId_ = 0;
    std::array<RunModifiers, 2> runModifiers_{};
    std::array<int, 2> explorationScores_{};
    std::array<int, 2> explorationObjectivesClearedByPlayer_{};
    std::array<int, 2> explorationBossesClearedByPlayer_{};
    std::array<int, 2> hiddenEventsClaimedByPlayer_{};

    PlayerState& player(PlayerId id);
    const PlayerState& player(PlayerId id) const;
    PlayerId opponent(PlayerId id) const;
    Unit& unit(UnitId id);
    const Unit& unit(UnitId id) const;

    void resetBoard(MapKind kind);
    UnitId createUnit(PlayerId owner, UnitType type);
    bool placeUnit(UnitId id, Coord coord);
    void removeFromBoard(UnitId id);
    void convertUnitOwner(UnitId id, PlayerId newOwner);
    void pushEvent(Event event);
    void addRecentBuy(PlayerState& player, UnitType type);
    void randomizeExplorationObjectives();
    void initializeNeutralObjectives();
    void initializeHiddenExplorationEvents();
    std::vector<Coord> hiddenEventPlacementCandidates() const;
    void applyExplorationObjectiveTerrain();
    bool triggerTrapAt(PlayerId triggeringPlayer, Coord coord, UnitId triggerUnitId = kInvalidUnitId);
    bool triggerRandomGoldEventAt(PlayerId triggeringPlayer, Coord coord, UnitId triggerUnitId);
    bool canTriggerHiddenEvent(const Unit& unit) const;
    bool tryDetectHiddenEvent(PlayerId triggeringPlayer, Coord movedCoord, UnitId triggerUnitId);
    bool triggerHiddenEventAt(PlayerId triggeringPlayer, Coord coord, UnitId triggerUnitId);
    bool explorationCheckSucceeds(const Unit& unit, AbilityScoreKind ability, int dc,
                                  SkillTag skill = SkillTag::None, int extraBonus = 0);
    int spawnRedcapAmbush(PlayerId triggeringPlayer, Coord origin, UnitId triggerUnitId);
    void clearExplorationObjective(size_t index, PlayerId clearer, UnitId actorId);
    void updateExplorationObjectiveForDeath(UnitId deadId, UnitId sourceId);
    void addExplorationScore(PlayerId player, int amount);
    int explorationTiebreakThreat(PlayerId player) const;
    std::optional<PlayerId> explorationWinner() const;
    void startCombatIfReady();
    void startCombat();
    void finishCombat(PlayerId winner);
    void finishExplorationRun();
    void startNextRound();
    void startNextRound(const std::string& reason);
    bool shouldFinishExplorationRun() const;
    std::string explorationCompletionReason() const;
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
    void markCombatProgress();
    bool shouldEndStalledCombat() const;
    bool shouldCastGuardianShield(const Unit& unit) const;
    bool isMajorObjective(ExplorationObjectiveKind kind) const;
    bool isBossObjective(ExplorationObjectiveKind kind) const;
    Coord majorObjectiveAnchor(const ExplorationObjectiveState& objective) const;
    std::optional<size_t> majorObjectiveIndexForUnit(UnitId unitId) const;
    bool isRealObjectiveProvoker(const Unit& unit) const;
    bool triggerBossGuardian(size_t objectiveIndex, UnitId provokerId,
                             const std::string& reason);
    UnitId spawnBossGuardian(size_t objectiveIndex, UnitId provokerId);
    UnitId selectBossFireballTarget(const Unit& boss) const;
    bool castBossFireball(UnitId bossId, UnitId targetId);
    void updateBossFireball(double dt);
    void refreshTarget(Unit& unit, bool force);
    UnitId selectTarget(const Unit& unit) const;
    TargetCandidate evaluateTargetCandidate(const Unit& unit, const Unit& candidate) const;
    bool targetAllowedByAggro(const Unit& unit, const Unit& candidate) const;
    bool shouldKeepCurrentTarget(const Unit& unit, const TargetCandidate& current,
                                 const TargetCandidate& best) const;
    bool shouldForceRetarget(const Unit& unit) const;
    UnitId selectHealTarget(const Unit& healer) const;
    UnitId selectGlobalHealTarget(const Unit& healer) const;
    UnitId selectFollowAlly(const Unit& unit) const;
    std::optional<size_t> selectCorpseForSpores(const Unit& unit, Coord guardOrigin) const;
    std::optional<Coord> selectSporeSpawnCell(Coord corpseCoord, Coord guardOrigin) const;
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
    void activateNeutralOnAttackIntent(UnitId attackerId, UnitId targetId);
    void provokeNeutral(UnitId neutralId, UnitId sourceId);
    bool isNeutralGuardianUnit(const Unit& unit) const;
    bool isNeutralSpawServant(const Unit& unit) const;
    bool isNeutralLikeCombatant(const Unit& unit) const;
    Coord neutralLeashOrigin(const Unit& unit) const;
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
    bool resolveCounterspellReaction(UnitId attackerId, UnitId targetId);
    bool tryResolveExtractBrain(UnitId attackerId, UnitId targetId);
    bool tryResolveActiveAbilityAttack(UnitId attackerId, UnitId targetId);
    void resolveWeaponAttack(UnitId attackerId, UnitId targetId);
    void applyWeaponStrikes(UnitId attackerId, UnitId targetId,
                            const DamagePacket& strikeDamage, int strikeCount);
    void applyDamage(UnitId targetId, int amount, UnitId sourceId);
    void applyHeal(UnitId targetId, int amount, UnitId sourceId);
    int addShield(UnitId targetId, int amount, UnitId sourceId);
    void killUnit(UnitId id, UnitId sourceId);
    void clearDeadUnits();
    void resolveVictory();
    bool hasActiveCombatUnit(PlayerId player) const;

    void moveUnits(double dt);
    bool isEffectiveCombatMove(const Unit& unit, Coord from, Coord to) const;
    std::optional<Coord> chooseNextStep(UnitId id, const std::vector<Coord>& reserved) const;
    bool hasImmediateAttackTarget(const Unit& unit) const;
    std::optional<Coord> chooseLandStepToward(const Unit& unit, Coord target, bool attackTarget,
                                              const std::vector<Coord>& reserved,
                                              int attackRange,
                                              std::optional<Coord> leashCenter = std::nullopt,
                                              int leashRadius = 0) const;
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
std::string toString(Phase phase);
std::string toString(UnitLayer layer);
std::string toString(UnitType type);
bool isInternalUnit(UnitType type);
bool isNeutralMonster(UnitType type);
AiDifficulty aiDifficultyFromString(const std::string& text);

} // namespace autochess

#endif
