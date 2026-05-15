#include <lib.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <queue>
#include <sstream>
#include <utility>

namespace autochess {

namespace {

constexpr int kRoleTank = 1 << 0;
constexpr int kRoleMelee = 1 << 1;
constexpr int kRoleRanged = 1 << 2;
constexpr int kRoleAir = 1 << 3;
constexpr int kRoleSupport = 1 << 4;
constexpr int kRoleControl = 1 << 5;
constexpr int kRoleSummoner = 1 << 6;
constexpr int kRoleAssassin = 1 << 7;
constexpr int kRoleAoe = 1 << 8;
constexpr int kStartingGold = 48;
constexpr int kBaseRoundIncome = 12;
constexpr int kRoundIncomeGrowth = 1;
constexpr int kMaxRoundIncomeGrowth = 4;
constexpr int kInterestGoldStep = 32;
constexpr int kMaxInterestIncome = 2;
constexpr int kKillBountyDivisor = 4;
constexpr int kMinimumKillBounty = 1;
constexpr int kDefaultExplorationRoundLimit = 8;
constexpr double kExplorationCombatRoundCap = 45.0;
constexpr int kRedcapAmbushRadius = 5;
constexpr int kNeutralGuardianLeashRadius = 5;
constexpr int kNeutralSummonGuardRadius = 4;
constexpr int kExplorationUnitAggroRadius = 7;
constexpr int kRogueAmbushMaxRange = 7;
constexpr double kRetargetInterval = 0.35;
constexpr int kBoardFeaturePlanes = 6;
constexpr int kGlobalStateFeatureCount = 16;
constexpr int kStateFeatureCount = kGlobalStateFeatureCount + kBoardWidth * kBoardHeight * kBoardFeaturePlanes;
constexpr int kActionFeatureCount = 20;
constexpr int kMaxAiActionsPerPreparation = 64;
constexpr double kNecromancerSkeletonLifespan = 8.0;
constexpr const char* kPolicyFormat = "autochess_policy_v1";
constexpr const char* kPolicyModelVersion = "linear-v1";
constexpr std::array<const char*, kBoardHeight> kExplorationMapTemplateA = {
    "ssssssssss#sssssssssss#ssssssssss",
    "ssssssssss#sssssssssss#ssssssssss",
    "ssss#sssss#ssss#sss#sssss#ssss#ss",
    "ssss##sssss#sss#sss#sssssss##ssss",
    "ssss#ssss#sssss#sss#sssss#ssss#ss",
    "ssss#ssss#sssss#sssss#ssssss#ssss",
    "ssssssss###sss#sss#sss#sssss#ssss",
    "sssss#sss#ssss#sss#ssss#sss#sssss",
    "sss##ssss#ssss#sss#ssss#sssss#sss",
    "ssssss#sss#sss#sss#sssss#sss#ssss",
    "ssss#sss#ssss#sssss#ssss#ssss#sss",
    "sssss#ssss#sss#sss#ssss#ssss#ssss",
    "ssss#sss#ssss#ssss#ssss#ssss#ssss",
    "sssss#sss#ssss#ssss#ssss#ssss#sss",
    "ssss#sssss#ssss#ssss#ssss#sss#sss",
    "sssss#sssss#ssss#ssss#sssss#sssss",
    "ssss#sssss#ssss#ssss#sssss#sss#ss",
    "ssssssssss#sssssssssss#ssssssssss",
    "ssssssssss#sssssssssss#ssssssssss"
};

constexpr std::array<const char*, kBoardHeight> kExplorationMapTemplateB = {
    "ssssss#sssssss#ssssss#sssss#sssss",
    "ssssssssssss#sssssss#ssssssssssss",
    "sss#sssss#sssss#ssssss#ssssss#sss",
    "ssss#sssss#sssss#ssssss#ssss#ssss",
    "sssssssssss#sssss#ssssss#sssss#ss",
    "ssss#sss#sssssssss#ssssssss#sssss",
    "sssss#sssssss#sss#sssssss#sss#sss",
    "ssss#ssss#sssss#ssssss#ssss#sssss",
    "ssssss#sssss#sssss#ssssss#ssss#ss",
    "sss#ssssss#sss#sssss#sssss#ssssss",
    "sssss#sssssssss#sssssss#ssss#ssss",
    "ssssssss#sssssssssss#ssss#sssssss",
    "ssss#sssssss#sss#ssssssssss#sssss",
    "sssss#ssssssss#ssssss#ssssss#ssss",
    "ssss#ssssssssssss#ssssss#ssss#sss",
    "ssssss#ssssss#ssssss#sssss#ssssss",
    "sssss#ssss#sssssss#sssssssss#ssss",
    "ssssssssssss#sssssss#ssssssssssss",
    "ssssss#sssssss#ssssss#sssss#sssss"
};

constexpr std::array<std::array<const char*, kBoardHeight>, 2> kExplorationMapTemplates = {
    kExplorationMapTemplateA,
    kExplorationMapTemplateB
};

constexpr int kExplorationMapTemplateCount = static_cast<int>(kExplorationMapTemplates.size());

int playerIndex(PlayerId player) {
    return player == PlayerId::One ? 0 : 1;
}

bool isBlockingTerrain(TerrainKind terrain) {
    return terrain == TerrainKind::Wall;
}

bool isExplorationMapKind(MapKind kind) {
    return kind == MapKind::ExplorationA || kind == MapKind::ExplorationB;
}

bool isNeutralMonsterType(UnitType type) {
    return type == UnitType::NeutralSpectator || type == UnitType::NeutralOwlbear ||
           type == UnitType::NeutralMindFlayer || type == UnitType::NeutralSovereignSpaw ||
           type == UnitType::NeutralKarniss || type == UnitType::NeutralRedcap ||
           type == UnitType::NeutralWaterMyrmidon ||
           type == UnitType::NeutralPhaseSpiderMatriarch ||
           type == UnitType::NeutralRaphael ||
           type == UnitType::NeutralKethericThorm ||
           type == UnitType::NeutralMoonlightSliver ||
           type == UnitType::NeutralGuardianOfFaith ||
           type == UnitType::NeutralMinotaur ||
           type == UnitType::NeutralDeathKnight ||
           type == UnitType::NeutralAirMyrmidon ||
           type == UnitType::NeutralTamiaHolzt;
}

bool isHumanoidUnitType(UnitType type) {
    return type == UnitType::GithyankiWarrior || type == UnitType::Ranger ||
           type == UnitType::Barbarian || type == UnitType::Necromancer ||
           type == UnitType::GoblinSkirmisher || type == UnitType::Paladin ||
           type == UnitType::Cleric || type == UnitType::Evoker ||
           type == UnitType::RogueAssassin || type == UnitType::Druid;
}

bool isPlantLikeUnitType(UnitType type) {
    return type == UnitType::Treant || type == UnitType::SporeServant;
}

bool isUndeadUnitType(UnitType type) {
    return type == UnitType::Skeleton || type == UnitType::SkeletonByNecromancer ||
           type == UnitType::NeutralDeathKnight;
}

bool isInternalUnitType(UnitType type) {
    return type == UnitType::SkeletonByNecromancer ||
           type == UnitType::Treant || type == UnitType::SporeServant || isNeutralMonsterType(type);
}

bool isHostileCombatTarget(const Unit& attacker, const Unit& target) {
    if (!attacker.alive || !target.alive || attacker.id == target.id) return false;

    const bool attackerNeutral = isNeutralMonsterType(attacker.spec.type);
    const bool targetNeutral = isNeutralMonsterType(target.spec.type);
    const bool attackerNeutralLike = attackerNeutral || attacker.neutralControlled;
    const bool targetNeutralLike = targetNeutral || target.neutralControlled;
    if (attackerNeutralLike && targetNeutralLike) return false;

    if (attackerNeutralLike || targetNeutralLike) return true;
    return attacker.owner != target.owner;
}

bool isRoundTransientUnit(UnitType type) {
    return type == UnitType::SkeletonByNecromancer || type == UnitType::Treant;
}

bool isSpellLikeAbility(AbilityKind ability) {
    switch (ability) {
        case AbilityKind::NecromancerSummon:
        case AbilityKind::MephitDeathBurst:
        case AbilityKind::DragonBreath:
        case AbilityKind::ClericHeal:
        case AbilityKind::FrostNova:
        case AbilityKind::DruidSummon:
        case AbilityKind::SpectatorWoundingRay:
        case AbilityKind::MindBlast:
        case AbilityKind::Counterspell:
        case AbilityKind::AnimatingSpores:
        case AbilityKind::DiabolicChains:
        case AbilityKind::SelunesIre:
        case AbilityKind::EvokerMagicMissile:
        case AbilityKind::DominatePerson:
        case AbilityKind::StrikeOfTheGuardian:
        case AbilityKind::Blight:
            return true;
        case AbilityKind::None:
        case AbilityKind::GithyankiAstralRaid:
        case AbilityKind::BarbarianHeavySwing:
        case AbilityKind::PaladinCharge:
        case AbilityKind::GuardianShield:
        case AbilityKind::RogueAmbush:
        case AbilityKind::KarnissCruelSting:
        case AbilityKind::OwlbearMultiattack:
        case AbilityKind::HiemalStrike:
        case AbilityKind::VenomousBite:
        case AbilityKind::KethericSmite:
        case AbilityKind::ExtractBrain:
        case AbilityKind::MinotaurCharge:
        case AbilityKind::StaggeringSmite:
        case AbilityKind::ElectrifiedFlail:
            return false;
    }
    return false;
}

std::string statusName(StatusKind kind) {
    switch (kind) {
        case StatusKind::Shield: return "Shield";
        case StatusKind::Taunt: return "Taunt";
        case StatusKind::Slow: return "Slow";
        case StatusKind::Summoned: return "Summoned";
        case StatusKind::FirstStrike: return "First Strike";
        case StatusKind::Stunned: return "Stunned";
        case StatusKind::Prone: return "Prone";
        case StatusKind::Chilled: return "Chilled";
        case StatusKind::Poisoned: return "Poisoned";
        case StatusKind::Blinded: return "Blinded";
        case StatusKind::Frightened: return "Frightened";
        case StatusKind::Staggered: return "Staggered";
    }
    return "Status";
}

template <typename T>
void eraseValue(std::vector<T>& values, const T& value) {
    values.erase(std::remove(values.begin(), values.end(), value), values.end());
}

int totalHp(const Unit& unit) {
    int total = 0;
    for (int hp : unit.hp) total += hp;
    return total;
}

int economyPayout(int amount) {
    if (amount <= 0) return 0;
    return std::max(1, amount * 3 / 4);
}

int roundIncomeFor(const PlayerState& player, int round, const RunModifiers& modifiers) {
    int growth = std::min(std::max(0, round - 1) * kRoundIncomeGrowth, kMaxRoundIncomeGrowth);
    int interestCap = kMaxInterestIncome + std::max(0, modifiers.interestBonus);
    int interest = std::min(interestCap, player.money / kInterestGoldStep);
    return kBaseRoundIncome + growth + modifiers.roundIncomeBonus + interest;
}

int killBountyFor(const Unit& dead) {
    if (isNeutralMonsterType(dead.spec.type)) {
        return economyPayout(std::max(kMinimumKillBounty, dead.spec.threat / 8));
    }
    if (dead.spec.cost <= 0 || isRoundTransientUnit(dead.spec.type)) return 0;
    return economyPayout(std::max(kMinimumKillBounty, dead.spec.cost / kKillBountyDivisor));
}

int upgradeCostFor(const UnitSpec& spec) {
    return std::max(5, (spec.cost * 4 + 4) / 5);
}

int effectiveBuyCostFor(const UnitSpec& spec, const RunModifiers& modifiers) {
    if (spec.cost <= 0) return 0;
    return std::max(1, spec.cost - std::max(0, modifiers.costDiscount));
}

int effectiveUpgradeCostFor(const UnitSpec& spec, const RunModifiers& modifiers) {
    return std::max(1, upgradeCostFor(spec) - std::max(0, modifiers.upgradeDiscount));
}

int benchLimitFor(const RunModifiers& modifiers) {
    return 10 + std::max(0, modifiers.benchBonus);
}

int familyIndexForUnitType(UnitType type) {
    switch (type) {
        case UnitType::Skeleton:
        case UnitType::SkeletonByNecromancer:
        case UnitType::GoblinSkirmisher:
        case UnitType::ImpSwarm:
            return static_cast<int>(NeutralFamily::Swarm);
        case UnitType::Barbarian:
        case UnitType::Paladin:
        case UnitType::ShieldGuardian:
        case UnitType::NeutralOwlbear:
        case UnitType::NeutralSovereignSpaw:
        case UnitType::NeutralWaterMyrmidon:
        case UnitType::NeutralKethericThorm:
        case UnitType::NeutralGuardianOfFaith:
        case UnitType::NeutralMinotaur:
        case UnitType::NeutralDeathKnight:
            return static_cast<int>(NeutralFamily::Guardian);
        case UnitType::Necromancer:
        case UnitType::Cleric:
        case UnitType::Evoker:
        case UnitType::Druid:
        case UnitType::Treant:
        case UnitType::SporeServant:
        case UnitType::NeutralSpectator:
        case UnitType::NeutralMindFlayer:
        case UnitType::NeutralRaphael:
        case UnitType::NeutralMoonlightSliver:
        case UnitType::NeutralTamiaHolzt:
            return static_cast<int>(NeutralFamily::Caster);
        case UnitType::GithyankiWarrior:
        case UnitType::RogueAssassin:
        case UnitType::NeutralKarniss:
        case UnitType::NeutralRedcap:
        case UnitType::NeutralPhaseSpiderMatriarch:
            return static_cast<int>(NeutralFamily::Assassin);
        case UnitType::Ranger:
        case UnitType::FireMephit:
        case UnitType::DragonWyrmling:
        case UnitType::NeutralAirMyrmidon:
            return static_cast<int>(NeutralFamily::Artillery);
    }
    return static_cast<int>(NeutralFamily::Swarm);
}

double familyCounterBonus(const UnitSpec& spec, NeutralFamily family) {
    switch (family) {
        case NeutralFamily::Swarm:
            if (spec.roleMask & kRoleAoe) return 24.0;
            if (spec.roleMask & kRoleSupport) return 10.0;
            if (spec.range >= 3) return 8.0;
            return spec.cost <= 6 ? 6.0 : 0.0;
        case NeutralFamily::Guardian:
            if (spec.roleMask & kRoleControl) return 18.0;
            if (spec.roleMask & kRoleAoe) return 14.0;
            if (spec.attackBonus >= 8) return 8.0;
            return 0.0;
        case NeutralFamily::Caster:
            if (spec.roleMask & kRoleAssassin) return 22.0;
            if (spec.speed >= 3.0) return 10.0;
            if (spec.canAttackAir) return 6.0;
            return 0.0;
        case NeutralFamily::Assassin:
            if (spec.roleMask & kRoleTank) return 20.0;
            if (spec.roleMask & kRoleSupport) return 14.0;
            if (spec.armorClass >= 17) return 8.0;
            return 0.0;
        case NeutralFamily::Artillery:
            if (spec.roleMask & kRoleAssassin) return 18.0;
            if (spec.layer == UnitLayer::Air) return 10.0;
            if (spec.range >= 3) return 8.0;
            return 0.0;
    }
    return 0.0;
}

std::vector<UnitSpec> makeSpecs() {
    std::vector<UnitSpec> specs = {
        {UnitType::Skeleton, "Skeleton Mob", "Sk", 4, 3, 15, 10, 1, 3.0, 0.8,
         UnitLayer::Land, true, false, 14, kRoleMelee, AbilityKind::None},
        {UnitType::SkeletonByNecromancer, "Raised Skeletons", "Rs", 0, 2, 16, 8, 1, 3.0, 0.8,
         UnitLayer::Land, true, false, 8, kRoleMelee, AbilityKind::None},
        {UnitType::GithyankiWarrior, "Githyanki Warrior", "Gi", 9, 1, 115, 24, 1, 3.0, 0.8,
         UnitLayer::Land, true, false, 38, kRoleMelee, AbilityKind::GithyankiAstralRaid,
         0.0, 15, 2, 0.0},
        {UnitType::Ranger, "Elven Ranger", "Er", 9, 2, 40, 16, 3, 3.0, 0.8,
         UnitLayer::Land, true, true, 36, kRoleRanged, AbilityKind::None},
        {UnitType::Barbarian, "Berserker", "Br", 22, 1, 300, 85, 1, 1.0, 0.8,
         UnitLayer::Land, true, false, 62, kRoleTank | kRoleMelee, AbilityKind::BarbarianHeavySwing},
        {UnitType::Necromancer, "Grave Necromancer", "Nc", 11, 1, 82, 19, 2, 2.0, 0.8,
         UnitLayer::Land, true, true, 44, kRoleRanged | kRoleSummoner, AbilityKind::NecromancerSummon,
         1.0, 0, 1, 0.0},
        {UnitType::FireMephit, "Fire Mephit", "Me", 12, 1, 155, 26, 2, 2.0, 0.9,
         UnitLayer::Air, true, true, 45, kRoleAir | kRoleAoe, AbilityKind::MephitDeathBurst},
        {UnitType::ImpSwarm, "Imp Swarm", "Im", 10, 3, 22, 15, 3, 3.0, 0.8,
         UnitLayer::Air, true, true, 39, kRoleAir | kRoleRanged, AbilityKind::None},
        {UnitType::GoblinSkirmisher, "Goblin Ambusher", "Gb", 6, 3, 30, 12, 1, 4.0, 0.7,
         UnitLayer::Land, true, false, 26, kRoleMelee, AbilityKind::None},
        {UnitType::Paladin, "Oathbound Paladin", "Pa", 15, 1, 210, 36, 1, 3.0, 0.8,
         UnitLayer::Land, true, false, 50, kRoleMelee, AbilityKind::PaladinCharge},
        {UnitType::DragonWyrmling, "Dragon Wyrmling", "Dw", 13, 1, 150, 24, 3, 3.0, 0.9,
         UnitLayer::Air, true, true, 48, kRoleAir | kRoleAoe, AbilityKind::DragonBreath},
        {UnitType::NeutralSpectator, "Spectator Raycaster", "Sp", 0, 1, 185, 34, 4, 1.6, 1.0,
         UnitLayer::Land, true, true, 62, kRoleRanged | kRoleControl, AbilityKind::SpectatorWoundingRay,
         0.0, 0, 4, 0.0},
        {UnitType::NeutralOwlbear, "Owlbear Matriarch", "Ow", 0, 1, 260, 42, 1, 1.5, 0.85,
         UnitLayer::Land, true, false, 68, kRoleTank | kRoleMelee, AbilityKind::OwlbearMultiattack},
        {UnitType::NeutralMindFlayer, "Mind Flayer Arcanist", "Mf", 0, 1, 320, 54, 4, 1.4, 1.05,
         UnitLayer::Land, true, true, 76, kRoleRanged | kRoleControl | kRoleAoe, AbilityKind::MindBlast,
         0.0, 0, 4, 1.8},
        {UnitType::NeutralSovereignSpaw, "Sovereign Spaw", "Ss", 0, 1, 320, 24, 2, 1.0, 0.95,
         UnitLayer::Land, true, false, 82, kRoleTank | kRoleSupport | kRoleSummoner, AbilityKind::AnimatingSpores,
         3.0, 0, 4, 12.0},
        {UnitType::NeutralKarniss, "Kar'niss Drider", "Kn", 0, 1, 285, 48, 1, 2.0, 0.75,
         UnitLayer::Land, true, false, 88, kRoleMelee | kRoleAssassin | kRoleTank, AbilityKind::KarnissCruelSting,
         0.0, 22, 1, 0.0},
        {UnitType::NeutralRedcap, "Redcap Ambusher", "Rc", 0, 3, 38, 14, 1, 3.2, 0.7,
         UnitLayer::Land, true, false, 24, kRoleMelee | kRoleAssassin, AbilityKind::RogueAmbush,
         0.0, 8, 1, 0.0},
        {UnitType::NeutralWaterMyrmidon, "Water Myrmidon", "Wm", 0, 1, 300, 52, 1, 1.4, 0.82,
         UnitLayer::Land, true, false, 86, kRoleTank | kRoleMelee | kRoleControl,
         AbilityKind::HiemalStrike, 0.0, 18, 1, 2.8},
        {UnitType::NeutralPhaseSpiderMatriarch, "Phase Spider Matriarch", "Ps", 0, 1, 245, 45, 1, 2.6, 0.72,
         UnitLayer::Land, true, false, 84, kRoleAssassin | kRoleMelee | kRoleControl,
         AbilityKind::VenomousBite, 0.0, 16, 1, 3.2},
        {UnitType::NeutralRaphael, "Raphael", "Rp", 0, 1, 360, 38, 4, 1.2, 1.10,
         UnitLayer::Land, true, true, 102, kRoleRanged | kRoleAoe | kRoleControl,
         AbilityKind::DiabolicChains, 0.0, 36, 4, 1.0},
        {UnitType::NeutralKethericThorm, "Ketheric Thorm", "Kt", 0, 1, 380, 58, 1, 1.2, 0.86,
         UnitLayer::Land, true, false, 104, kRoleTank | kRoleMelee | kRoleControl,
         AbilityKind::KethericSmite, 0.0, 24, 1, 2.0},
        {UnitType::NeutralMoonlightSliver, "Moonlight Sliver", "Ms", 0, 1, 340, 46, 4, 1.4, 1.05,
         UnitLayer::Land, true, true, 100, kRoleRanged | kRoleAoe | kRoleControl,
         AbilityKind::SelunesIre, 0.0, 30, 4, 1.8},
        {UnitType::NeutralGuardianOfFaith, "Guardian of Faith", "Gf", 0, 1, 92, 40, 3, 1.0, 1.10,
         UnitLayer::Land, true, true, 88, kRoleRanged | kRoleAoe | kRoleControl,
         AbilityKind::StrikeOfTheGuardian, 0.0, 40, 3, 0.0},
        {UnitType::NeutralMinotaur, "Minotaur", "Mn", 0, 1, 280, 34, 4, 2.0, 0.82,
         UnitLayer::Land, true, false, 86, kRoleTank | kRoleMelee | kRoleControl,
         AbilityKind::MinotaurCharge, 0.0, 36, 4, 1.4},
        {UnitType::NeutralDeathKnight, "Death Knight", "Dk", 0, 1, 190, 26, 1, 1.4, 0.86,
         UnitLayer::Land, true, false, 78, kRoleMelee | kRoleControl,
         AbilityKind::StaggeringSmite, 0.0, 0, 1, 2.8},
        {UnitType::NeutralAirMyrmidon, "Air Myrmidon", "Am", 0, 1, 130, 22, 1, 2.4, 1.18,
         UnitLayer::Air, true, true, 74, kRoleAir | kRoleMelee | kRoleControl,
         AbilityKind::ElectrifiedFlail, 0.0, 0, 1, 1.2},
        {UnitType::NeutralTamiaHolzt, "Tamia Holzt", "Th", 0, 1, 310, 38, 4, 1.4, 0.95,
         UnitLayer::Land, true, true, 104, kRoleRanged | kRoleControl | kRoleAoe,
         AbilityKind::Blight, 0.0, 0, 4, 0.0},
        {UnitType::ShieldGuardian, "Shield Guardian", "Sg", 14, 1, 190, 13, 1, 1.0, 0.9,
         UnitLayer::Land, true, false, 58, kRoleTank, AbilityKind::GuardianShield,
         4.0, 40, 2, 0.0},
        {UnitType::Cleric, "Life Cleric", "Cl", 12, 1, 75, 8, 3, 2.0, 0.9,
         UnitLayer::Land, true, true, 45, kRoleSupport | kRoleRanged, AbilityKind::ClericHeal,
         1.2, 25, 3, 0.0},
        {UnitType::Evoker, "Arcane Evoker", "Ev", 17, 1, 58, 54, 4, 2.0, 1.0,
         UnitLayer::Land, true, true, 72, kRoleRanged | kRoleAoe,
         AbilityKind::EvokerMagicMissile, 0.0, 0, 4, 0.0},
        {UnitType::RogueAssassin, "Shadow Rogue", "Ro", 13, 1, 95, 30, 1, 4.0, 0.7,
         UnitLayer::Land, true, false, 52, kRoleAssassin | kRoleMelee, AbilityKind::RogueAmbush},
        {UnitType::Druid, "Circle Druid", "Du", 13, 1, 90, 12, 2, 2.0, 0.9,
         UnitLayer::Land, true, false, 40, kRoleSummoner | kRoleSupport, AbilityKind::DruidSummon,
         5.0, 0, 1, 0.0},
        {UnitType::Treant, "Awakened Treant", "Tr", 0, 1, 60, 8, 1, 1.0, 0.9,
         UnitLayer::Land, true, false, 10, kRoleMelee | kRoleTank, AbilityKind::None},
        {UnitType::SporeServant, "Spore Servant", "Sv", 0, 1, 70, 12, 1, 1.5, 0.95,
         UnitLayer::Land, true, false, 18, kRoleMelee | kRoleTank, AbilityKind::None}
    };

    for (UnitSpec& spec : specs) {
        switch (spec.type) {
            case UnitType::Skeleton:
                spec.armorClass = 13;
                spec.attackBonus = 6;
                spec.savingThrowBonus = 1;
                break;
            case UnitType::SkeletonByNecromancer:
                spec.armorClass = 13;
                spec.attackBonus = 5;
                spec.savingThrowBonus = 1;
                break;
            case UnitType::GithyankiWarrior:
                spec.armorClass = 17;
                spec.attackBonus = 8;
                spec.savingThrowBonus = 4;
                spec.spellSaveDc = 12;
                break;
            case UnitType::Ranger:
                spec.armorClass = 14;
                spec.attackBonus = 8;
                spec.savingThrowBonus = 3;
                break;
            case UnitType::Barbarian:
                spec.armorClass = 14;
                spec.attackBonus = 9;
                spec.savingThrowBonus = 5;
                break;
            case UnitType::Necromancer:
                spec.armorClass = 12;
                spec.attackBonus = 7;
                spec.savingThrowBonus = 3;
                spec.spellSaveDc = 14;
                break;
            case UnitType::FireMephit:
                spec.armorClass = 12;
                spec.attackBonus = 8;
                spec.savingThrowBonus = 2;
                spec.spellSaveDc = 13;
                break;
            case UnitType::ImpSwarm:
                spec.armorClass = 13;
                spec.attackBonus = 8;
                spec.savingThrowBonus = 3;
                break;
            case UnitType::GoblinSkirmisher:
                spec.armorClass = 15;
                spec.attackBonus = 8;
                spec.savingThrowBonus = 3;
                break;
            case UnitType::Paladin:
                spec.armorClass = 18;
                spec.attackBonus = 8;
                spec.savingThrowBonus = 5;
                spec.spellSaveDc = 13;
                break;
            case UnitType::DragonWyrmling:
                spec.armorClass = 17;
                spec.attackBonus = 8;
                spec.savingThrowBonus = 5;
                spec.spellSaveDc = 15;
                break;
            case UnitType::NeutralSpectator:
                spec.armorClass = 16;
                spec.attackBonus = 8;
                spec.savingThrowBonus = 5;
                spec.spellSaveDc = 14;
                break;
            case UnitType::NeutralOwlbear:
                spec.armorClass = 14;
                spec.attackBonus = 8;
                spec.savingThrowBonus = 4;
                break;
            case UnitType::NeutralMindFlayer:
                spec.armorClass = 17;
                spec.attackBonus = 9;
                spec.savingThrowBonus = 7;
                spec.spellSaveDc = 18;
                break;
            case UnitType::NeutralSovereignSpaw:
                spec.armorClass = 13;
                spec.attackBonus = 7;
                spec.savingThrowBonus = 5;
                spec.spellSaveDc = 15;
                break;
            case UnitType::NeutralKarniss:
                spec.armorClass = 17;
                spec.attackBonus = 9;
                spec.savingThrowBonus = 5;
                break;
            case UnitType::NeutralRedcap:
                spec.armorClass = 14;
                spec.attackBonus = 7;
                spec.savingThrowBonus = 3;
                break;
            case UnitType::NeutralWaterMyrmidon:
                spec.armorClass = 18;
                spec.attackBonus = 9;
                spec.savingThrowBonus = 6;
                spec.spellSaveDc = 15;
                break;
            case UnitType::NeutralPhaseSpiderMatriarch:
                spec.armorClass = 16;
                spec.attackBonus = 9;
                spec.savingThrowBonus = 6;
                spec.spellSaveDc = 15;
                break;
            case UnitType::NeutralRaphael:
                spec.armorClass = 19;
                spec.attackBonus = 9;
                spec.savingThrowBonus = 8;
                spec.spellSaveDc = 17;
                break;
            case UnitType::NeutralKethericThorm:
                spec.armorClass = 18;
                spec.attackBonus = 10;
                spec.savingThrowBonus = 7;
                spec.spellSaveDc = 16;
                break;
            case UnitType::NeutralMoonlightSliver:
                spec.armorClass = 17;
                spec.attackBonus = 9;
                spec.savingThrowBonus = 7;
                spec.spellSaveDc = 17;
                break;
            case UnitType::NeutralGuardianOfFaith:
                spec.armorClass = 20;
                spec.attackBonus = 9;
                spec.savingThrowBonus = 8;
                spec.spellSaveDc = 16;
                break;
            case UnitType::NeutralMinotaur:
                spec.armorClass = 14;
                spec.attackBonus = 9;
                spec.savingThrowBonus = 6;
                spec.spellSaveDc = 15;
                break;
            case UnitType::NeutralDeathKnight:
                spec.armorClass = 18;
                spec.attackBonus = 8;
                spec.savingThrowBonus = 6;
                spec.spellSaveDc = 16;
                break;
            case UnitType::NeutralAirMyrmidon:
                spec.armorClass = 15;
                spec.attackBonus = 8;
                spec.savingThrowBonus = 6;
                spec.spellSaveDc = 16;
                break;
            case UnitType::NeutralTamiaHolzt:
                spec.armorClass = 16;
                spec.attackBonus = 8;
                spec.savingThrowBonus = 6;
                spec.spellSaveDc = 17;
                break;
            case UnitType::ShieldGuardian:
                spec.armorClass = 19;
                spec.attackBonus = 7;
                spec.savingThrowBonus = 4;
                break;
            case UnitType::Cleric:
                spec.armorClass = 14;
                spec.attackBonus = 7;
                spec.savingThrowBonus = 5;
                spec.spellSaveDc = 13;
                break;
            case UnitType::Evoker:
                spec.armorClass = 12;
                spec.attackBonus = 8;
                spec.savingThrowBonus = 4;
                spec.spellSaveDc = 15;
                break;
            case UnitType::RogueAssassin:
                spec.armorClass = 15;
                spec.attackBonus = 9;
                spec.savingThrowBonus = 4;
                spec.abilityValue = 20;
                break;
            case UnitType::Druid:
                spec.armorClass = 13;
                spec.attackBonus = 7;
                spec.savingThrowBonus = 4;
                spec.spellSaveDc = 14;
                break;
            case UnitType::Treant:
                spec.armorClass = 13;
                spec.attackBonus = 6;
                spec.savingThrowBonus = 2;
                break;
            case UnitType::SporeServant:
                spec.armorClass = 12;
                spec.attackBonus = 6;
                spec.savingThrowBonus = 2;
                break;
        }
    }

    return specs;
}

std::string eventUnitName(const Unit& unit) {
    return unit.spec.name + "#" + std::to_string(unit.id);
}

double clampFeature(double value) {
    if (value < -1.0) return -1.0;
    if (value > 1.0) return 1.0;
    return value;
}

std::string lowerCopy(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

std::string policyFileName(AiDifficulty difficulty) {
    switch (difficulty) {
        case AiDifficulty::Normal: return "built-in";
        case AiDifficulty::Hard: return "hard.policy.json";
        case AiDifficulty::SuperHard: return "superhard.policy.json";
    }
    return "built-in";
}

std::string joinPath(const std::string& directory, const std::string& file) {
    if (directory.empty()) return file;
    char back = directory.back();
    if (back == '/' || back == '\\') return directory + file;
    return directory + "/" + file;
}

bool readTextFile(const std::string& path, std::string& out) {
    std::ifstream input(path);
    if (!input) return false;
    std::ostringstream stream;
    stream << input.rdbuf();
    out = stream.str();
    return true;
}

std::optional<std::string> jsonStringValue(const std::string& json, const std::string& key) {
    std::string marker = "\"" + key + "\"";
    size_t keyPos = json.find(marker);
    if (keyPos == std::string::npos) return std::nullopt;
    size_t colon = json.find(':', keyPos + marker.size());
    if (colon == std::string::npos) return std::nullopt;
    size_t firstQuote = json.find('"', colon + 1);
    if (firstQuote == std::string::npos) return std::nullopt;
    std::string value;
    bool escaped = false;
    for (size_t i = firstQuote + 1; i < json.size(); ++i) {
        char ch = json[i];
        if (escaped) {
            value.push_back(ch);
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '"') return value;
        value.push_back(ch);
    }
    return std::nullopt;
}

std::optional<double> jsonNumberValue(const std::string& json, const std::string& key) {
    std::string marker = "\"" + key + "\"";
    size_t keyPos = json.find(marker);
    if (keyPos == std::string::npos) return std::nullopt;
    size_t colon = json.find(':', keyPos + marker.size());
    if (colon == std::string::npos) return std::nullopt;
    size_t start = json.find_first_of("-0123456789", colon + 1);
    if (start == std::string::npos) return std::nullopt;
    size_t end = start;
    while (end < json.size() &&
           (std::isdigit(static_cast<unsigned char>(json[end])) || json[end] == '-' ||
            json[end] == '+' || json[end] == '.' || json[end] == 'e' || json[end] == 'E')) {
        ++end;
    }
    try {
        return std::stod(json.substr(start, end - start));
    } catch (...) {
        return std::nullopt;
    }
}

std::vector<double> jsonNumberArray(const std::string& json, const std::string& key) {
    std::vector<double> values;
    std::string marker = "\"" + key + "\"";
    size_t keyPos = json.find(marker);
    if (keyPos == std::string::npos) return values;
    size_t open = json.find('[', keyPos + marker.size());
    size_t close = json.find(']', open == std::string::npos ? keyPos : open);
    if (open == std::string::npos || close == std::string::npos || close <= open) return values;

    std::string body = json.substr(open + 1, close - open - 1);
    std::stringstream stream(body);
    std::string token;
    while (std::getline(stream, token, ',')) {
        try {
            values.push_back(std::stod(token));
        } catch (...) {
            values.clear();
            return values;
        }
    }
    return values;
}

void hashAppend(uint64_t& hash, const std::string& text) {
    constexpr uint64_t kFnvPrime = 1099511628211ull;
    for (unsigned char ch : text) {
        hash ^= ch;
        hash *= kFnvPrime;
    }
}

std::string hashToHex(uint64_t hash) {
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

const std::array<const char*, kBoardHeight>& explorationMapMask(int templateIndex) {
    int index = ((templateIndex % kExplorationMapTemplateCount) + kExplorationMapTemplateCount) %
                kExplorationMapTemplateCount;
    return kExplorationMapTemplates[static_cast<size_t>(index)];
}

MapKind explorationMapKindForTemplate(int templateIndex) {
    return ((templateIndex % kExplorationMapTemplateCount) + kExplorationMapTemplateCount) %
                   kExplorationMapTemplateCount ==
               1
           ? MapKind::ExplorationB
           : MapKind::ExplorationA;
}

int templateIndexForMapKind(MapKind kind) {
    return kind == MapKind::ExplorationB ? 1 : 0;
}

bool isBossObjectiveKind(ExplorationObjectiveKind kind) {
    return kind == ExplorationObjectiveKind::Boss;
}

bool isCentralBossCoord(Coord coord) {
    return coord.x >= 10 && coord.x <= 22 && coord.y >= 4 && coord.y <= 14;
}

bool isOuterObjectiveCoord(Coord coord) {
    if (isCentralBossCoord(coord)) return false;
    return coord.x >= 4 && coord.x <= kBoardWidth - 5 && coord.y >= 2 && coord.y <= kBoardHeight - 3;
}

void paintMapFromMask(Board& board, const std::array<const char*, kBoardHeight>& mask) {
    for (int y = 0; y < board.height; ++y) {
        for (int x = 0; x < board.width; ++x) {
            char tile = mask[static_cast<size_t>(y)][x];
            TerrainKind terrain = TerrainKind::Wall;
            switch (tile) {
                case 's': terrain = TerrainKind::SideRoad; break;
                case 'N': terrain = TerrainKind::NeutralCamp; break;
                case 'X': terrain = TerrainKind::BossSite; break;
                case '!': terrain = TerrainKind::Trap; break;
                case '#':
                default:
                    terrain = TerrainKind::Wall;
                    break;
            }
            board.setTerrain({x, y}, terrain);
        }
    }
}

bool isExplorationStagingCoord(PlayerId playerId, Coord coord) {
    if (coord.x < 0 || coord.x >= kBoardWidth || coord.y < 0 || coord.y >= kBoardHeight) {
        return false;
    }
    const bool leftRoom = coord.x <= 3 && coord.y >= 3 && coord.y <= kBoardHeight - 4;
    const bool rightRoom = coord.x >= kBoardWidth - 4 && coord.y >= 3 && coord.y <= kBoardHeight - 4;
    const bool leftTopWing = coord.y <= 1 && coord.x <= 5;
    const bool leftBottomWing = coord.y >= kBoardHeight - 2 && coord.x <= 5;
    const bool rightTopWing = coord.y <= 1 && coord.x >= kBoardWidth - 6;
    const bool rightBottomWing = coord.y >= kBoardHeight - 2 && coord.x >= kBoardWidth - 6;
    if (playerId == PlayerId::One) return leftRoom || leftTopWing || leftBottomWing;
    return rightRoom || rightTopWing || rightBottomWing;
}

bool isEdgeWingExplorationStagingCoord(PlayerId playerId, Coord coord) {
    if (!isExplorationStagingCoord(playerId, coord)) return false;
    if (playerId == PlayerId::One) {
        return coord.x >= 4 && (coord.y <= 1 || coord.y >= kBoardHeight - 2);
    }
    return coord.x <= kBoardWidth - 5 && (coord.y <= 1 || coord.y >= kBoardHeight - 2);
}

void paintExplorationMap(Board& board, int templateIndex) {
    paintMapFromMask(board, explorationMapMask(templateIndex));
    for (int y = 0; y < board.height; ++y) {
        for (int x = 0; x < board.width; ++x) {
            Coord coord{x, y};
            if (isExplorationStagingCoord(PlayerId::One, coord) ||
                isExplorationStagingCoord(PlayerId::Two, coord)) {
                board.setTerrain(coord, TerrainKind::SideRoad);
            }
        }
    }
}

int normalizedExplorationRoundLimit(int rounds) {
    const std::array<int, 5> allowed = {2, 4, 6, 8, 10};
    int best = allowed.front();
    int bestDist = std::abs(rounds - best);
    for (int value : allowed) {
        int dist = std::abs(rounds - value);
        if (dist < bestDist || (dist == bestDist && value > best)) {
            best = value;
            bestDist = dist;
        }
    }
    return best;
}

} // namespace

bool isInternalUnit(UnitType type) {
    return isInternalUnitType(type);
}

bool isNeutralMonster(UnitType type) {
    return isNeutralMonsterType(type);
}

class HeuristicAiPlanner : public AiPlanner {
public:
    std::optional<AiAction> chooseAction(GameEngine& engine,
                                         PlayerId player,
                                         const std::vector<AiAction>& legalActions) override {
        return engine.chooseHeuristicAction(player, legalActions);
    }

    std::string name() const override {
        return "heuristic";
    }

    std::unique_ptr<AiPlanner> clone() const override {
        return std::make_unique<HeuristicAiPlanner>(*this);
    }
};

class ScriptedNormalAiPlanner : public AiPlanner {
public:
    std::optional<AiAction> chooseAction(GameEngine& engine,
                                         PlayerId player,
                                         const std::vector<AiAction>& legalActions) override {
        return engine.chooseScriptedNormalAction(player, legalActions);
    }

    std::string name() const override {
        return "scripted-normal";
    }

    std::unique_ptr<AiPlanner> clone() const override {
        return std::make_unique<ScriptedNormalAiPlanner>(*this);
    }
};

class PolicyAiPlanner : public AiPlanner {
public:
    PolicyAiPlanner(AiPolicyMetadata metadata, std::vector<double> weights)
        : metadata_(std::move(metadata)), weights_(std::move(weights)) {}

    std::optional<AiAction> chooseAction(GameEngine& engine,
                                         PlayerId player,
                                         const std::vector<AiAction>& legalActions) override {
        if (!metadata_.valid || legalActions.empty()) {
            return engine.chooseHeuristicAction(player, legalActions);
        }

        std::vector<double> state = engine.stateFeatures(player);
        if (static_cast<int>(state.size()) != metadata_.stateFeatureCount) {
            return engine.chooseHeuristicAction(player, legalActions);
        }

        const int expectedWeights = metadata_.stateFeatureCount + metadata_.actionFeatureCount;
        if (static_cast<int>(weights_.size()) != expectedWeights) {
            return engine.chooseHeuristicAction(player, legalActions);
        }

        const AiAction* bestAction = nullptr;
        double bestScore = -std::numeric_limits<double>::infinity();
        for (const AiAction& action : legalActions) {
            std::vector<double> actionFeatures = engine.actionFeatures(player, action);
            if (static_cast<int>(actionFeatures.size()) != metadata_.actionFeatureCount) continue;

            double score = metadata_.bias;
            for (size_t i = 0; i < state.size(); ++i) score += state[i] * weights_[i];
            for (size_t i = 0; i < actionFeatures.size(); ++i) {
                score += actionFeatures[i] * weights_[state.size() + i];
            }
            score += metadata_.heuristicBlend * engine.heuristicActionScore(player, action);

            if (!bestAction || score > bestScore) {
                bestAction = &action;
                bestScore = score;
            }
        }

        if (!bestAction) return engine.chooseHeuristicAction(player, legalActions);
        return *bestAction;
    }

    std::string name() const override {
        return "policy";
    }

    std::unique_ptr<AiPlanner> clone() const override {
        return std::make_unique<PolicyAiPlanner>(*this);
    }

private:
    AiPolicyMetadata metadata_;
    std::vector<double> weights_;
};

bool operator==(Coord lhs, Coord rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

bool operator!=(Coord lhs, Coord rhs) {
    return !(lhs == rhs);
}

int manhattan(Coord lhs, Coord rhs) {
    return std::abs(lhs.x - rhs.x) + std::abs(lhs.y - rhs.y);
}

Board::Board() : cells(width * height) {}

bool Board::inBounds(Coord coord) const {
    return coord.x >= 0 && coord.x < width && coord.y >= 0 && coord.y < height;
}

Cell& Board::at(Coord coord) {
    return cells[coord.y * width + coord.x];
}

const Cell& Board::at(Coord coord) const {
    return cells[coord.y * width + coord.x];
}

TerrainKind Board::terrainAt(Coord coord) const {
    if (!inBounds(coord)) return TerrainKind::Wall;
    return at(coord).terrain;
}

void Board::setTerrain(Coord coord, TerrainKind terrain) {
    if (!inBounds(coord)) return;
    at(coord).terrain = terrain;
}

bool Board::blocked(Coord coord) const {
    return !inBounds(coord) || isBlockingTerrain(terrainAt(coord));
}

UnitId Board::occupant(Coord coord, UnitLayer layer) const {
    if (!inBounds(coord)) return kInvalidUnitId;
    const Cell& cell = at(coord);
    const std::vector<UnitId>& ids = layer == UnitLayer::Land ? cell.land : cell.air;
    return ids.empty() ? kInvalidUnitId : ids.front();
}

const std::vector<UnitId>& Board::occupants(Coord coord, UnitLayer layer) const {
    static const std::vector<UnitId> empty;
    if (!inBounds(coord)) return empty;
    const Cell& cell = at(coord);
    return layer == UnitLayer::Land ? cell.land : cell.air;
}

void Board::setOccupant(Coord coord, UnitLayer layer, UnitId id) {
    if (!inBounds(coord)) return;
    Cell& cell = at(coord);
    std::vector<UnitId>& ids = layer == UnitLayer::Land ? cell.land : cell.air;
    ids.clear();
    if (id != kInvalidUnitId) ids.push_back(id);
}

void Board::addOccupant(Coord coord, UnitLayer layer, UnitId id) {
    if (!inBounds(coord) || id == kInvalidUnitId) return;
    Cell& cell = at(coord);
    std::vector<UnitId>& ids = layer == UnitLayer::Land ? cell.land : cell.air;
    if (std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
}

void Board::removeOccupant(Coord coord, UnitLayer layer, UnitId id) {
    if (!inBounds(coord)) return;
    Cell& cell = at(coord);
    std::vector<UnitId>& ids = layer == UnitLayer::Land ? cell.land : cell.air;
    eraseValue(ids, id);
}

std::string toString(DamageType type) {
    switch (type) {
        case DamageType::Piercing: return "Piercing";
        case DamageType::Slashing: return "Slashing";
        case DamageType::Bludgeoning: return "Bludgeoning";
        case DamageType::Fire: return "Fire";
        case DamageType::Cold: return "Cold";
        case DamageType::Poison: return "Poison";
        case DamageType::Necrotic: return "Necrotic";
        case DamageType::Radiant: return "Radiant";
        case DamageType::Psychic: return "Psychic";
        case DamageType::Force: return "Force";
        case DamageType::Lightning: return "Lightning";
    }
    return "Force";
}

std::string toString(DamageAffinity affinity) {
    switch (affinity) {
        case DamageAffinity::Immune: return "immune";
        case DamageAffinity::Resistant: return "resistant";
        case DamageAffinity::Normal: return "normal";
        case DamageAffinity::Vulnerable: return "vulnerable";
    }
    return "normal";
}

DamageType basicDamageTypeFor(UnitType type) {
    switch (type) {
        case UnitType::Skeleton:
        case UnitType::SkeletonByNecromancer:
        case UnitType::Ranger:
        case UnitType::RogueAssassin:
        case UnitType::GoblinSkirmisher:
        case UnitType::ImpSwarm:
        case UnitType::NeutralRedcap:
        case UnitType::NeutralPhaseSpiderMatriarch:
        case UnitType::NeutralMinotaur:
            return DamageType::Piercing;
        case UnitType::Barbarian:
        case UnitType::GithyankiWarrior:
        case UnitType::NeutralKarniss:
        case UnitType::NeutralOwlbear:
        case UnitType::NeutralDeathKnight:
            return DamageType::Slashing;
        case UnitType::Paladin:
        case UnitType::ShieldGuardian:
        case UnitType::Druid:
        case UnitType::Treant:
        case UnitType::SporeServant:
        case UnitType::NeutralWaterMyrmidon:
        case UnitType::NeutralKethericThorm:
        case UnitType::NeutralAirMyrmidon:
            return DamageType::Bludgeoning;
        case UnitType::DragonWyrmling:
        case UnitType::FireMephit:
        case UnitType::NeutralRaphael:
            return DamageType::Fire;
        case UnitType::Necromancer:
        case UnitType::NeutralSpectator:
        case UnitType::NeutralTamiaHolzt:
            return DamageType::Necrotic;
        case UnitType::Cleric:
        case UnitType::NeutralMoonlightSliver:
        case UnitType::NeutralGuardianOfFaith:
            return DamageType::Radiant;
        case UnitType::Evoker:
            return DamageType::Force;
        case UnitType::NeutralMindFlayer:
            return DamageType::Psychic;
        case UnitType::NeutralSovereignSpaw:
            return DamageType::Poison;
    }
    return DamageType::Force;
}

DamageRoll damageRollForValue(int averageDamage, DamageType type) {
    int diceCount = 1;
    int diceSides = 6;
    if (averageDamage <= 14) {
        diceCount = 1;
        diceSides = 6;
    } else if (averageDamage <= 24) {
        diceCount = 2;
        diceSides = 8;
    } else if (averageDamage <= 48) {
        diceCount = 4;
        diceSides = 8;
    } else if (averageDamage <= 72) {
        diceCount = 5;
        diceSides = 10;
    } else {
        diceCount = 8;
        diceSides = 10;
    }
    int diceAverageFloor = (diceCount * (diceSides + 1)) / 2;
    int flat = std::max(0, averageDamage - diceAverageFloor);
    return DamageRoll{diceCount, diceSides, flat, type};
}

DamagePacket basicDamagePacketFor(const UnitSpec& spec) {
    DamagePacket packet;
    packet.rolls.push_back(damageRollForValue(std::max(1, spec.attack), basicDamageTypeFor(spec.type)));
    packet.sourceLabel = "attack";
    return packet;
}

DamagePacket abilityDamagePacketFor(const UnitSpec& spec, AbilityKind ability) {
    DamagePacket packet = basicDamagePacketFor(spec);
    packet.sourceLabel = "ability";
    switch (ability) {
        case AbilityKind::GithyankiAstralRaid:
            packet.rolls.push_back(DamageRoll{0, 0, spec.abilityValue, DamageType::Psychic});
            return packet;
        case AbilityKind::MephitDeathBurst:
        case AbilityKind::DragonBreath:
        case AbilityKind::DiabolicChains:
            return DamagePacket{{damageRollForValue(std::max(1, spec.attack), DamageType::Fire)}, 0, "fire"};
        case AbilityKind::FrostNova:
            return DamagePacket{{damageRollForValue(std::max(1, spec.attack), DamageType::Cold)}, 0, "cold"};
        case AbilityKind::PaladinCharge:
            packet.rolls.push_back(packet.rolls.front());
            return packet;
        case AbilityKind::RogueAmbush:
            packet.rolls.push_back(DamageRoll{0, 0, spec.abilityValue, DamageType::Piercing});
            return packet;
        case AbilityKind::SpectatorWoundingRay:
            return DamagePacket{{damageRollForValue(std::max(1, spec.attack), DamageType::Necrotic)}, 0, "necrotic"};
        case AbilityKind::MindBlast:
            return DamagePacket{{damageRollForValue(std::max(1, spec.attack), DamageType::Psychic)}, 0, "psychic"};
        case AbilityKind::OwlbearMultiattack:
            return DamagePacket{{damageRollForValue(std::max(1, spec.attack), DamageType::Slashing),
                                  damageRollForValue(std::max(1, (spec.attack * 2) / 3), DamageType::Piercing)},
                                0,
                                "multiattack"};
        case AbilityKind::HiemalStrike:
            packet.rolls.push_back(DamageRoll{0, 0, spec.abilityValue, DamageType::Cold});
            return packet;
        case AbilityKind::VenomousBite:
            packet.rolls.push_back(DamageRoll{0, 0, spec.abilityValue, DamageType::Poison});
            return packet;
        case AbilityKind::KethericSmite:
            packet.rolls.push_back(DamageRoll{std::max(1, spec.abilityValue / 8), 8, 0, DamageType::Radiant});
            return packet;
        case AbilityKind::SelunesIre:
            packet.rolls.push_back(DamageRoll{0, 0, spec.abilityValue, DamageType::Radiant});
            return packet;
        case AbilityKind::EvokerMagicMissile:
            return DamagePacket{{DamageRoll{0, 0, std::max(1, spec.attack / 3), DamageType::Force}}, 0, "missile"};
        case AbilityKind::ExtractBrain:
            return DamagePacket{{DamageRoll{10, 10, 1, DamageType::Piercing}}, 0, "extract brain"};
        case AbilityKind::StrikeOfTheGuardian:
            return DamagePacket{{DamageRoll{0, 0, 20, DamageType::Radiant},
                                  DamageRoll{0, 0, 20, DamageType::Radiant}},
                                0,
                                "strike of the guardian"};
        case AbilityKind::MinotaurCharge:
            return DamagePacket{{DamageRoll{4, 8, 4, DamageType::Piercing}}, 0, "minotaur charge"};
        case AbilityKind::Blight:
            return DamagePacket{{DamageRoll{8, 8, 0, DamageType::Necrotic}}, 0, "blight"};
        case AbilityKind::StaggeringSmite:
            packet.rolls.push_back(DamageRoll{4, 6, 0, DamageType::Psychic});
            packet.sourceLabel = "staggering smite";
            return packet;
        case AbilityKind::ElectrifiedFlail:
            return DamagePacket{{DamageRoll{1, 8, 7, DamageType::Bludgeoning},
                                  DamageRoll{1, 8, 0, DamageType::Lightning},
                                  DamageRoll{1, 10, 0, DamageType::Lightning}},
                                0,
                                "electrified flail"};
        case AbilityKind::None:
        case AbilityKind::BarbarianHeavySwing:
        case AbilityKind::NecromancerSummon:
        case AbilityKind::GuardianShield:
        case AbilityKind::ClericHeal:
        case AbilityKind::DruidSummon:
        case AbilityKind::KarnissCruelSting:
        case AbilityKind::Counterspell:
        case AbilityKind::AnimatingSpores:
        case AbilityKind::DominatePerson:
            return packet;
    }
    return packet;
}

int damagePacketMin(const DamagePacket& packet) {
    int total = std::max(0, packet.flatDamage);
    for (const DamageRoll& roll : packet.rolls) {
        total += std::max(0, roll.flatBonus);
        if (roll.diceCount > 0 && roll.diceSides > 0) total += roll.diceCount;
    }
    return total;
}

int damagePacketMax(const DamagePacket& packet) {
    int total = std::max(0, packet.flatDamage);
    for (const DamageRoll& roll : packet.rolls) {
        total += std::max(0, roll.flatBonus);
        if (roll.diceCount > 0 && roll.diceSides > 0) total += roll.diceCount * roll.diceSides;
    }
    return total;
}

std::string damageFormula(const DamagePacket& packet) {
    std::vector<std::string> parts;
    for (const DamageRoll& roll : packet.rolls) {
        if (roll.diceCount <= 0 && roll.flatBonus <= 0) continue;
        std::ostringstream part;
        if (roll.diceCount > 0 && roll.diceSides > 0) {
            part << roll.diceCount << "d" << roll.diceSides;
            if (roll.flatBonus > 0) part << " + " << roll.flatBonus;
        } else {
            part << roll.flatBonus;
        }
        part << " " << toString(roll.type);
        parts.push_back(part.str());
    }
    if (packet.flatDamage > 0) parts.push_back(std::to_string(packet.flatDamage) + " Force");
    if (parts.empty()) return "0 Force";
    std::ostringstream out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) out << " + ";
        out << parts[i];
    }
    return out.str();
}

std::string damageRange(const DamagePacket& packet) {
    return std::to_string(damagePacketMin(packet)) + "~" + std::to_string(damagePacketMax(packet));
}

std::string damageLine(const DamagePacket& packet) {
    return "Damage: " + damageRange(packet) + "    " + damageFormula(packet);
}

DamageAffinity damageAffinity(UnitType type, DamageType damageType) {
    auto is = [damageType](DamageType candidate) { return damageType == candidate; };
    switch (type) {
        case UnitType::Skeleton:
        case UnitType::SkeletonByNecromancer:
            if (is(DamageType::Poison)) return DamageAffinity::Immune;
            if (is(DamageType::Bludgeoning)) return DamageAffinity::Vulnerable;
            if (is(DamageType::Piercing) || is(DamageType::Necrotic)) return DamageAffinity::Resistant;
            break;
        case UnitType::Treant:
        case UnitType::SporeServant:
        case UnitType::NeutralSovereignSpaw:
            if (is(DamageType::Fire)) return DamageAffinity::Vulnerable;
            if (is(DamageType::Piercing) || is(DamageType::Poison)) return DamageAffinity::Resistant;
            break;
        case UnitType::ShieldGuardian:
            if (is(DamageType::Piercing) || is(DamageType::Slashing) ||
                is(DamageType::Bludgeoning) || is(DamageType::Poison)) {
                return DamageAffinity::Resistant;
            }
            break;
        case UnitType::FireMephit:
        case UnitType::ImpSwarm:
        case UnitType::DragonWyrmling:
            if (is(DamageType::Fire)) return DamageAffinity::Resistant;
            break;
        case UnitType::NeutralMindFlayer:
            if (is(DamageType::Psychic)) return DamageAffinity::Resistant;
            break;
        case UnitType::NeutralSpectator:
            if (is(DamageType::Necrotic) || is(DamageType::Psychic)) return DamageAffinity::Resistant;
            break;
        case UnitType::NeutralRaphael:
            if (is(DamageType::Fire) || is(DamageType::Poison) || is(DamageType::Necrotic)) {
                return DamageAffinity::Resistant;
            }
            break;
        case UnitType::NeutralKethericThorm:
            if (is(DamageType::Radiant)) return DamageAffinity::Vulnerable;
            if (is(DamageType::Necrotic) || is(DamageType::Bludgeoning)) return DamageAffinity::Resistant;
            break;
        case UnitType::NeutralMoonlightSliver:
            if (is(DamageType::Necrotic)) return DamageAffinity::Vulnerable;
            if (is(DamageType::Radiant)) return DamageAffinity::Resistant;
            break;
        case UnitType::NeutralGuardianOfFaith:
            if (is(DamageType::Poison) || is(DamageType::Necrotic)) return DamageAffinity::Resistant;
            if (is(DamageType::Radiant)) return DamageAffinity::Resistant;
            break;
        case UnitType::NeutralDeathKnight:
            if (is(DamageType::Poison)) return DamageAffinity::Immune;
            if (is(DamageType::Necrotic)) return DamageAffinity::Resistant;
            if (is(DamageType::Radiant)) return DamageAffinity::Vulnerable;
            break;
        case UnitType::NeutralAirMyrmidon:
            if (is(DamageType::Poison)) return DamageAffinity::Immune;
            if (is(DamageType::Piercing) || is(DamageType::Slashing) ||
                is(DamageType::Bludgeoning) || is(DamageType::Lightning)) {
                return DamageAffinity::Resistant;
            }
            break;
        default:
            break;
    }
    return DamageAffinity::Normal;
}

std::string damageAffinitySummary(UnitType type) {
    std::vector<std::string> resists;
    std::vector<std::string> vulnerable;
    std::vector<std::string> immune;
    const std::array<DamageType, 11> types = {
        DamageType::Piercing, DamageType::Slashing, DamageType::Bludgeoning, DamageType::Fire,
        DamageType::Cold, DamageType::Poison, DamageType::Necrotic, DamageType::Radiant,
        DamageType::Psychic, DamageType::Force, DamageType::Lightning};
    for (DamageType damageType : types) {
        DamageAffinity affinity = damageAffinity(type, damageType);
        if (affinity == DamageAffinity::Resistant) resists.push_back(toString(damageType));
        if (affinity == DamageAffinity::Vulnerable) vulnerable.push_back(toString(damageType));
        if (affinity == DamageAffinity::Immune) immune.push_back(toString(damageType));
    }
    auto join = [](const std::vector<std::string>& values) {
        std::ostringstream out;
        for (size_t i = 0; i < values.size(); ++i) {
            if (i > 0) out << ", ";
            out << values[i];
        }
        return out.str();
    };
    std::vector<std::string> parts;
    if (!resists.empty()) parts.push_back("Resists " + join(resists));
    if (!vulnerable.empty()) parts.push_back("Vulnerable " + join(vulnerable));
    if (!immune.empty()) parts.push_back("Immune " + join(immune));
    if (parts.empty()) return "Damage affinity: normal";
    std::ostringstream out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) out << " | ";
        out << parts[i];
    }
    return out.str();
}

GameEngine::GameEngine(unsigned seed) : specs_(makeSpecs()), rng_(seed) {
    config_.mode = mode_;
    aiPolicyMetadata_.format = kPolicyFormat;
    aiPolicyMetadata_.modelVersion = kPolicyModelVersion;
    aiPolicyMetadata_.difficulty = toString(config_.aiDifficulty);
    aiPolicyMetadata_.stateFeatureCount = kStateFeatureCount;
    aiPolicyMetadata_.actionFeatureCount = kActionFeatureCount;
}

GameEngine::GameEngine(const GameEngine& other)
    : players_(other.players_),
      board_(other.board_),
      units_(other.units_),
      exploration_(other.exploration_),
      corpses_(other.corpses_),
      specs_(other.specs_),
      events_(other.events_),
      aiPlanner_(other.aiPlanner_ ? other.aiPlanner_->clone() : nullptr),
      config_(other.config_),
      aiPolicyMetadata_(other.aiPolicyMetadata_),
      rng_(other.rng_),
      mode_(other.mode_),
      mapKind_(other.mapKind_),
      explorationMapTemplate_(other.explorationMapTemplate_),
      phase_(other.phase_),
      winner_(other.winner_),
      round_(other.round_),
      time_(other.time_),
      combatTime_(other.combatTime_),
      explorationRound_(other.explorationRound_),
      explorationRoundLimit_(other.explorationRoundLimit_),
      explorationRoundLimitLocked_(other.explorationRoundLimitLocked_),
      nextUnitId_(other.nextUnitId_),
      runModifiers_(other.runModifiers_),
      explorationScores_(other.explorationScores_),
      explorationObjectivesClearedByPlayer_(other.explorationObjectivesClearedByPlayer_),
      explorationBossesClearedByPlayer_(other.explorationBossesClearedByPlayer_),
      hiddenEventsClaimedByPlayer_(other.hiddenEventsClaimedByPlayer_) {}

GameEngine& GameEngine::operator=(const GameEngine& other) {
    if (this == &other) return *this;
    players_ = other.players_;
    board_ = other.board_;
    units_ = other.units_;
    exploration_ = other.exploration_;
    corpses_ = other.corpses_;
    specs_ = other.specs_;
    events_ = other.events_;
    aiPlanner_ = other.aiPlanner_ ? other.aiPlanner_->clone() : nullptr;
    config_ = other.config_;
    aiPolicyMetadata_ = other.aiPolicyMetadata_;
    rng_ = other.rng_;
    mode_ = other.mode_;
    mapKind_ = other.mapKind_;
    explorationMapTemplate_ = other.explorationMapTemplate_;
    phase_ = other.phase_;
    winner_ = other.winner_;
    round_ = other.round_;
    time_ = other.time_;
    combatTime_ = other.combatTime_;
    explorationRound_ = other.explorationRound_;
    explorationRoundLimit_ = other.explorationRoundLimit_;
    explorationRoundLimitLocked_ = other.explorationRoundLimitLocked_;
    nextUnitId_ = other.nextUnitId_;
    runModifiers_ = other.runModifiers_;
    explorationScores_ = other.explorationScores_;
    explorationObjectivesClearedByPlayer_ = other.explorationObjectivesClearedByPlayer_;
    explorationBossesClearedByPlayer_ = other.explorationBossesClearedByPlayer_;
    hiddenEventsClaimedByPlayer_ = other.hiddenEventsClaimedByPlayer_;
    return *this;
}

void GameEngine::startNewGame(GameMode mode) {
    GameConfig config = config_;
    config.mode = mode;
    startNewGame(config);
}

void GameEngine::startNewGame(const GameConfig& config) {
    int configuredExplorationLimit = normalizedExplorationRoundLimit(explorationRoundLimit_);
    config_ = config;
    mode_ = config.mode;
    std::uniform_int_distribution<int> pickExplorationTemplate(0, kExplorationMapTemplateCount - 1);
    explorationMapTemplate_ = pickExplorationTemplate(rng_);
    mapKind_ = explorationMapKindForTemplate(explorationMapTemplate_);
    phase_ = Phase::Preparation;
    winner_.reset();
    round_ = 1;
    time_ = 0.0;
    combatTime_ = 0.0;
    explorationRound_ = 0;
    explorationRoundLimit_ = configuredExplorationLimit;
    explorationRoundLimitLocked_ = false;
    nextUnitId_ = 0;
    units_.clear();
    exploration_.clear();
    corpses_.clear();
    events_.clear();
    runModifiers_ = {};
    explorationScores_ = {};
    explorationObjectivesClearedByPlayer_ = {};
    explorationBossesClearedByPlayer_ = {};
    hiddenEventsClaimedByPlayer_ = {};
    resetBoard(mapKind_);

    players_[0] = PlayerState{PlayerId::One, "Player1", false, kStartingGold, false, {}, {}, {}};
    players_[1] = PlayerState{PlayerId::Two, mode_ == GameMode::SinglePlayerVsAi ? "AI" : "Player2",
                              mode_ == GameMode::SinglePlayerVsAi, kStartingGold, false, {}, {}, {}};
    loadAiPlanner();

    initializeNeutralObjectives();
    initializeHiddenExplorationEvents();

    pushEvent({EventType::RoundStarted, PlayerId::One, kInvalidUnitId, kInvalidUnitId,
               {}, {}, round_, "Exploration run started"});
}

bool GameEngine::buyUnit(PlayerId playerId, UnitType type) {
    if (phase_ != Phase::Preparation) return false;
    const UnitSpec* spec = specFor(type);
    if (!spec || spec->cost <= 0 || isInternalUnit(type)) return false;

    PlayerState& p = player(playerId);
    int cost = effectiveBuyCost(playerId, *spec);
    if (p.money < cost || rosterCount(playerId) >= benchLimit(playerId)) return false;

    UnitId id = createUnit(playerId, type);
    p.money -= cost;
    unit(id).purchaseValue = cost;
    p.bench.push_back(id);
    addRecentBuy(p, type);

    pushEvent({EventType::Bought, playerId, id, kInvalidUnitId, {}, {}, cost,
               p.name + " bought " + spec->name});
    return true;
}

bool GameEngine::deployUnit(PlayerId playerId, UnitId unitId, Coord coord) {
    if (phase_ != Phase::Preparation || unitId < 0 || unitId >= static_cast<int>(units_.size())) return false;
    Unit& u = unit(unitId);
        if (!u.alive || u.owner != playerId || u.neutralControlled) return false;
    if (u.deployed) return moveDeployedUnit(playerId, unitId, coord);
    if (!canDeploy(playerId, coord, u.spec.layer)) return false;

    PlayerState& p = player(playerId);
    auto it = std::find(p.bench.begin(), p.bench.end(), unitId);
    if (it == p.bench.end()) return false;

    placeUnit(unitId, coord);
    u.deployed = true;
    u.lastCoord = coord;
    u.homeCoord = coord;
    p.bench.erase(it);
    p.deployed.push_back(unitId);

    pushEvent({EventType::Deployed, playerId, unitId, kInvalidUnitId, {}, coord, 0,
               p.name + " deployed " + u.spec.name});
    return true;
}

bool GameEngine::moveDeployedUnit(PlayerId playerId, UnitId unitId, Coord coord) {
    if (phase_ != Phase::Preparation || unitId < 0 || unitId >= static_cast<int>(units_.size())) return false;
    Unit& u = unit(unitId);
    if (!u.alive || !u.deployed || u.owner != playerId || u.neutralControlled ||
        isInternalUnit(u.spec.type)) {
        return false;
    }
    if (u.coord == coord) return true;
    if (!canDeploy(playerId, coord, u.spec.layer)) return false;

    Coord from = u.coord;
    removeFromBoard(unitId);
    placeUnit(unitId, coord);
    u.lastCoord = coord;
    u.homeCoord = coord;

    pushEvent({EventType::Deployed, playerId, unitId, kInvalidUnitId, from, coord, 0,
               u.spec.name + " moved in deployment"});
    return true;
}

bool GameEngine::returnToBench(PlayerId playerId, UnitId unitId) {
    if (phase_ != Phase::Preparation || unitId < 0 || unitId >= static_cast<int>(units_.size())) return false;
    Unit& u = unit(unitId);
    PlayerState& p = player(playerId);
    if (!u.alive || !u.deployed || u.owner != playerId || u.neutralControlled ||
        isInternalUnit(u.spec.type)) {
        return false;
    }
    if (static_cast<int>(p.bench.size()) >= benchLimit(playerId)) return false;

    Coord from = u.coord;
    removeFromBoard(unitId);
    u.deployed = false;
    u.homeCoord = {-1, -1};
    eraseValue(p.deployed, unitId);
    p.bench.push_back(unitId);

    pushEvent({EventType::Deployed, playerId, unitId, kInvalidUnitId, from, {}, 0,
               u.spec.name + " returned to bench"});
    return true;
}

bool GameEngine::upgradeUnit(PlayerId playerId, UnitId unitId) {
    (void)playerId;
    (void)unitId;
    return false;
#if 0
    if (phase_ != Phase::Preparation || unitId < 0 || unitId >= static_cast<int>(units_.size())) return false;
    Unit& u = unit(unitId);
    PlayerState& p = player(playerId);
    if (!u.alive || u.owner != playerId || u.upgraded || isInternalUnit(u.spec.type)) return false;
    int upgradeCost = effectiveUpgradeCost(playerId, u.spec);
    if (p.money < upgradeCost) return false;

    p.money -= upgradeCost;
    u.upgraded = true;
    int attackBonus = std::max(4, (u.spec.attack + 2) / 3);
    int hpBonus = std::max(10, u.spec.maxHp / 4);
    u.spec.attack += attackBonus;
    u.spec.maxHp += hpBonus;
    u.spec.attackBonus += 1;
    u.spec.armorClass += 1;
    u.spec.savingThrowBonus += 1;
    u.spec.spellSaveDc += 1;
    u.spec.name += "+";
    for (int& hp : u.hp) hp = u.spec.maxHp;

    pushEvent({EventType::Upgraded, playerId, unitId, kInvalidUnitId, {}, {}, upgradeCost,
               p.name + " upgraded " + u.spec.name});
    return true;
#endif
}

void GameEngine::setReady(PlayerId playerId, bool ready) {
    if (phase_ != Phase::Preparation) return;
    player(playerId).ready = ready;
    if (ready && player(opponent(playerId)).isAi) {
        aiPrepare(opponent(playerId));
    }
    startCombatIfReady();
}

void GameEngine::tick(double dt) {
    if (phase_ == Phase::Finished) return;
    if (dt <= 0.0) return;

    time_ += dt;
    if (phase_ == Phase::Preparation) {
        if (shouldFinishExplorationRun()) {
            finishExplorationRun();
            return;
        }
        startCombatIfReady();
        return;
    }

    tickCombat(dt);
}

GameSnapshot GameEngine::snapshot() const {
    GameSnapshot snapshot;
    snapshot.width = board_.width;
    snapshot.height = board_.height;
    snapshot.round = round_;
    snapshot.time = time_;
    snapshot.combatTime = combatTime_;
    snapshot.mapKind = mapKind_;
    snapshot.explorationRound = explorationRound_;
    snapshot.explorationRoundLimit = explorationRoundLimit_;
    snapshot.explorationRoundsRemaining = std::max(0, explorationRoundLimit_ - explorationRound_);
    snapshot.explorationRoundLimitLocked = explorationRoundLimitLocked_;
    ExplorationStats explorationStats = exploration_.stats();
    snapshot.explorationObjectivesCleared = explorationStats.objectivesCleared;
    snapshot.explorationObjectivesTotal = explorationStats.objectivesTotal;
    snapshot.bossesCleared = explorationStats.bossesCleared;
    snapshot.eventsTriggered = explorationStats.eventsTriggered;
    snapshot.trapsTriggered = explorationStats.trapsTriggered;
    snapshot.hiddenEventsClaimed = explorationStats.hiddenEventsClaimed;
    snapshot.randomGoldEventsClaimed = explorationStats.randomGoldEventsClaimed;
    snapshot.randomGoldEventsTotal = 0;
    snapshot.explorationScores = explorationScores_;
    snapshot.explorationObjectivesClearedByPlayer = explorationObjectivesClearedByPlayer_;
    snapshot.explorationBossesClearedByPlayer = explorationBossesClearedByPlayer_;
    snapshot.hiddenEventsClaimedByPlayer = hiddenEventsClaimedByPlayer_;
    snapshot.phase = phase_;
    snapshot.winner = winner_;

    for (int i = 0; i < 2; ++i) {
        const PlayerState& p = players_[i];
        snapshot.players[i] = PlayerView{p.id, p.name, p.isAi, p.money, p.ready, p.bench, p.deployed};
    }

    snapshot.terrain.reserve(board_.cells.size());
    for (int y = 0; y < board_.height; ++y) {
        for (int x = 0; x < board_.width; ++x) {
            snapshot.terrain.push_back(board_.terrainAt({x, y}));
        }
    }

    for (const Unit& u : units_) {
        if (!u.alive && !u.deployed) continue;
        UnitView view;
        view.id = u.id;
        view.type = u.spec.type;
        view.name = u.spec.name;
        view.shortName = u.spec.shortName;
        view.owner = u.owner;
        view.coord = u.coord;
        view.layer = u.spec.layer;
        view.ability = u.spec.ability;
        view.deployed = u.deployed;
        view.alive = u.alive;
        view.upgraded = u.upgraded;
        view.neutralControlled = u.neutralControlled;
        view.neutralActivated = isNeutralMonsterType(u.spec.type) &&
                                u.neutralBehavior == NeutralBehavior::PassiveGuardian &&
                                u.neutralProvoked;
        view.neutralReturningHome = u.neutralReturningHome;
        view.targetId = u.target;
        view.units = static_cast<int>(u.hp.size());
        view.maxUnits = u.spec.unitCount;
        view.hp = u.hp;
        view.totalHp = totalHp(u);
        view.maxTotalHp = u.spec.maxHp * u.spec.unitCount;
        view.shield = u.shield;
        view.attack = u.spec.attack;
        view.range = u.spec.range;
        view.cost = effectiveBuyCost(u.owner, u.spec);
        view.armorClass = effectiveArmorClass(u);
        view.attackBonus = effectiveAttackBonus(u);
        view.savingThrowBonus = u.spec.savingThrowBonus;
        view.spellSaveDc = u.spec.spellSaveDc;
        view.slowed = hasStatus(u, StatusKind::Slow) || hasStatus(u, StatusKind::Chilled);
        view.taunting = hasStatus(u, StatusKind::Taunt) || u.spec.ability == AbilityKind::GuardianShield;
        snapshot.units.push_back(view);
    }

    return snapshot;
}

std::vector<Event> GameEngine::consumeEvents() {
    std::vector<Event> copy = events_;
    events_.clear();
    return copy;
}

const std::vector<UnitSpec>& GameEngine::shop() const {
    return specs_;
}

const UnitSpec* GameEngine::specFor(UnitType type) const {
    auto it = std::find_if(specs_.begin(), specs_.end(), [type](const UnitSpec& spec) {
        return spec.type == type;
    });
    return it == specs_.end() ? nullptr : &*it;
}

int GameEngine::effectiveBuyCost(PlayerId playerId, const UnitSpec& spec) const {
    return effectiveBuyCostFor(spec, runModifiers_[playerIndex(playerId)]);
}

int GameEngine::effectiveUpgradeCost(PlayerId playerId, const UnitSpec& spec) const {
    return effectiveUpgradeCostFor(spec, runModifiers_[playerIndex(playerId)]);
}

int GameEngine::benchLimit(PlayerId playerId) const {
    return benchLimitFor(runModifiers_[playerIndex(playerId)]);
}

int GameEngine::rosterCount(PlayerId playerId) const {
    std::vector<UnitId> ids = player(playerId).bench;
    ids.insert(ids.end(), player(playerId).deployed.begin(), player(playerId).deployed.end());
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

    int count = 0;
    for (UnitId id : ids) {
        if (id < 0 || id >= static_cast<int>(units_.size())) continue;
        const Unit& u = unit(id);
        if (!u.alive || isInternalUnitType(u.spec.type) || u.spec.cost <= 0) continue;
        ++count;
    }
    return count;
}

int GameEngine::baseSummonLimitFor(const Unit& caster) const {
    switch (caster.spec.ability) {
        case AbilityKind::NecromancerSummon:
            return 2;
        case AbilityKind::DruidSummon:
            return 1;
        case AbilityKind::AnimatingSpores:
            return 2;
        default:
            return 0;
    }
}

int GameEngine::activeSummonCountFor(UnitId casterId) const {
    if (casterId < 0 || casterId >= static_cast<int>(units_.size())) return 0;
    int count = 0;
    for (const Unit& summon : units_) {
        if (!summon.alive || !summon.deployed) continue;
        if (summon.spec.type != UnitType::SkeletonByNecromancer &&
            summon.spec.type != UnitType::Treant &&
            summon.spec.type != UnitType::SporeServant) {
            continue;
        }
        bool createdByCaster = false;
        for (const StatusEffect& status : summon.statuses) {
            if (status.kind == StatusKind::Summoned && status.source == casterId &&
                status.remaining > 0.0) {
                createdByCaster = true;
                break;
            }
        }
        if (createdByCaster) ++count;
    }
    return count;
}

int GameEngine::summonLimitFor(const Unit& caster) const {
    int base = baseSummonLimitFor(caster);
    if (base <= 0) return 0;
    if (isInternalUnitType(caster.spec.type)) return base;
    return base + std::max(0, runModifiers_[playerIndex(caster.owner)].summonLimitBonus);
}

bool GameEngine::canSummonMore(UnitId casterId) const {
    if (casterId < 0 || casterId >= static_cast<int>(units_.size())) return false;
    const Unit& caster = unit(casterId);
    int limit = summonLimitFor(caster);
    return limit > 0 && activeSummonCountFor(casterId) < limit;
}

void GameEngine::setExplorationRoundLimit(int rounds) {
    if (phase_ != Phase::Preparation || explorationRound_ > 0 || explorationRoundLimitLocked_) {
        return;
    }
    explorationRoundLimit_ = normalizedExplorationRoundLimit(rounds);
}

void GameEngine::lockExplorationRoundLimit() {
    if (phase_ != Phase::Preparation || explorationRound_ > 0) return;
    explorationRoundLimitLocked_ = true;
}

int GameEngine::explorationRoundLimit() const {
    return explorationRoundLimit_;
}

void GameEngine::setRunModifiers(PlayerId playerId, const RunModifiers& modifiers) {
    runModifiers_[playerIndex(playerId)] = modifiers;
}

const RunModifiers& GameEngine::runModifiers(PlayerId playerId) const {
    return runModifiers_[playerIndex(playerId)];
}

void GameEngine::grantGold(PlayerId playerId, int amount) {
    if (amount == 0) return;
    PlayerState& p = player(playerId);
    p.money += amount;
    pushEvent({EventType::GoldGained, playerId, kInvalidUnitId, kInvalidUnitId, {}, {}, amount,
               p.name + " gained " + std::to_string(amount) + " gold"});
}

bool GameEngine::canDeploy(PlayerId playerId, Coord coord, UnitLayer layer) const {
    if (!isDeploymentCell(playerId, coord)) return false;
    if (board_.blocked(coord)) return false;
    if (layer == UnitLayer::Land) {
        for (UnitId id : board_.occupants(coord, UnitLayer::Land)) {
            if (id < 0 || id >= static_cast<int>(units_.size())) continue;
            const Unit& u = unit(id);
            if (u.alive && u.deployed) return false;
        }
    }
    return true;
}

bool GameEngine::isDeploymentCell(PlayerId playerId, Coord coord) const {
    if (!board_.inBounds(coord)) return false;
    return isExplorationStagingCell(playerId, coord);
}

bool GameEngine::isExplorationStagingCell(PlayerId playerId, Coord coord) const {
    if (!board_.inBounds(coord) || board_.blocked(coord)) return false;
    return isExplorationStagingCoord(playerId, coord);
}

void GameEngine::setAiDifficulty(AiDifficulty difficulty) {
    config_.aiDifficulty = difficulty;
    if (phase_ == Phase::Preparation) loadAiPlanner();
}

AiDifficulty GameEngine::aiDifficulty() const {
    return config_.aiDifficulty;
}

void GameEngine::setAiPolicyDirectory(const std::string& directory) {
    config_.aiPolicyDirectory = directory;
    if (phase_ == Phase::Preparation) loadAiPlanner();
}

const std::string& GameEngine::aiPolicyDirectory() const {
    return config_.aiPolicyDirectory;
}

const AiPolicyMetadata& GameEngine::aiPolicyMetadata() const {
    return aiPolicyMetadata_;
}

AiFeatureSchema GameEngine::aiFeatureSchema() const {
    return AiFeatureSchema{
        kStateFeatureCount,
        kActionFeatureCount,
        {
            "global:round,time,phase,economy,bench,deployed,exploration_score,ready,combat,board_size",
            "board:friend_land_threat,enemy_land_threat,friend_air_threat,enemy_air_threat,friend_hp,enemy_hp"
        },
        {
            "kind:buy,deploy,move,return,upgrade-disabled,ready",
            "unit:cost,count,hp,attack,range,speed,threat,layer,targets_air",
            "coord:normalized_x,normalized_y,forward_depth,unit_flags"
        }
    };
}

std::string GameEngine::rulesFingerprint() const {
    uint64_t hash = 14695981039346656037ull;
    hashAppend(hash, "autochess-rules-v5-single-exploration-run-relics");
    hashAppend(hash, std::to_string(kBoardWidth));
    hashAppend(hash, std::to_string(kBoardHeight));
    hashAppend(hash, "dungeon-run-exploration-score-v1");
    hashAppend(hash, std::to_string(kStartingGold));
    hashAppend(hash, std::to_string(kBaseRoundIncome));
    hashAppend(hash, std::to_string(kRoundIncomeGrowth));
    hashAppend(hash, std::to_string(kMaxRoundIncomeGrowth));
    hashAppend(hash, std::to_string(kInterestGoldStep));
    hashAppend(hash, std::to_string(kMaxInterestIncome));
    for (const UnitSpec& spec : specs_) {
        hashAppend(hash, std::to_string(static_cast<int>(spec.type)));
        hashAppend(hash, spec.name);
        hashAppend(hash, spec.shortName);
        hashAppend(hash, std::to_string(spec.cost));
        hashAppend(hash, std::to_string(spec.unitCount));
        hashAppend(hash, std::to_string(spec.maxHp));
        hashAppend(hash, std::to_string(spec.attack));
        hashAppend(hash, std::to_string(spec.range));
        hashAppend(hash, std::to_string(spec.speed));
        hashAppend(hash, std::to_string(spec.attackCooldown));
        hashAppend(hash, std::to_string(static_cast<int>(spec.layer)));
        hashAppend(hash, std::to_string(spec.canAttackLand));
        hashAppend(hash, std::to_string(spec.canAttackAir));
        hashAppend(hash, std::to_string(spec.threat));
        hashAppend(hash, std::to_string(spec.roleMask));
        hashAppend(hash, std::to_string(static_cast<int>(spec.ability)));
        hashAppend(hash, std::to_string(spec.abilityCooldown));
        hashAppend(hash, std::to_string(spec.abilityValue));
        hashAppend(hash, std::to_string(spec.abilityRange));
        hashAppend(hash, std::to_string(spec.abilityDuration));
        hashAppend(hash, std::to_string(spec.armorClass));
        hashAppend(hash, std::to_string(spec.attackBonus));
        hashAppend(hash, std::to_string(spec.savingThrowBonus));
        hashAppend(hash, std::to_string(spec.spellSaveDc));
    }
    return hashToHex(hash);
}

bool GameEngine::debugTriggerTrap(PlayerId triggeringPlayer, Coord coord) {
    return triggerTrapAt(triggeringPlayer, coord);
}

bool GameEngine::debugTriggerRandomGold(PlayerId triggeringPlayer, Coord coord) {
    return triggerRandomGoldEventAt(triggeringPlayer, coord, kInvalidUnitId);
}

bool GameEngine::debugTriggerHiddenEvent(PlayerId triggeringPlayer, Coord coord, UnitId triggerUnitId) {
    return triggerHiddenEventAt(triggeringPlayer, coord, triggerUnitId);
}

bool GameEngine::debugClearVisibleObjectives(PlayerId clearer) {
    bool clearedAny = false;
    for (size_t index = 0; index < exploration_.objectives().size(); ++index) {
        const ExplorationObjectiveState* objective = exploration_.objective(index);
        if (!objective || objective->cleared) continue;
        clearExplorationObjective(index, clearer, kInvalidUnitId);
        clearedAny = true;
    }
    return clearedAny;
}

bool GameEngine::debugApplyDamage(UnitId targetId, int amount, DamageType type) {
    if (targetId < 0 || targetId >= static_cast<int>(units_.size()) || amount <= 0) return false;
    if (!unit(targetId).alive || unit(targetId).hp.empty()) return false;
    applyDamage(targetId, amount, type, kInvalidUnitId);
    return true;
}

bool GameEngine::debugApplyDamageFrom(UnitId sourceId, UnitId targetId, int amount, DamageType type) {
    if (sourceId < 0 || sourceId >= static_cast<int>(units_.size())) return false;
    if (targetId < 0 || targetId >= static_cast<int>(units_.size()) || amount <= 0) return false;
    if (!unit(sourceId).alive || !unit(sourceId).deployed) return false;
    if (!unit(targetId).alive || unit(targetId).hp.empty()) return false;
    applyDamage(targetId, amount, type, sourceId);
    return true;
}

UnitId GameEngine::debugCreateUnit(PlayerId owner, UnitType type, Coord coord) {
    UnitId id = createUnit(owner, type);
    Unit& u = unit(id);
    if (!board_.inBounds(coord) || !placeUnit(id, coord)) {
        u.alive = false;
        u.deployed = false;
        u.hp.clear();
        u.coord = {-1, -1};
        u.homeCoord = u.coord;
        return kInvalidUnitId;
    }

    u.deployed = true;
    u.homeCoord = coord;
    u.lastCoord = coord;
    if (isNeutralMonsterType(u.spec.type)) {
        u.neutralBehavior = u.spec.type == UnitType::NeutralRedcap
                                ? NeutralBehavior::HostileAmbusher
                                : NeutralBehavior::PassiveGuardian;
        u.neutralProvoked = u.neutralBehavior == NeutralBehavior::HostileAmbusher;
        u.neutralReturningHome = false;
    }
    player(owner).deployed.push_back(id);
    return id;
}

bool GameEngine::debugKnockback(UnitId targetId, Coord source, int distance, UnitId sourceId) {
    return knockbackUnit(targetId, source, distance, sourceId);
}

bool GameEngine::debugRadialKnockback(Coord center, int radius, int distance, UnitId sourceId) {
    return resolveRadialKnockback(center, radius, distance, sourceId).moved;
}

std::vector<Coord> GameEngine::debugRandomGoldCoords() const {
    return exploration_.randomGoldCoords();
}

std::vector<Coord> GameEngine::debugHiddenHealingCoords() const {
    return exploration_.hiddenHealingCoords();
}

std::vector<AiAction> GameEngine::legalActions(PlayerId playerId) const {
    std::vector<AiAction> actions;
    if (phase_ != Phase::Preparation) return actions;

    const PlayerState& p = player(playerId);
    if (rosterCount(playerId) < benchLimit(playerId)) {
        for (const UnitSpec& spec : specs_) {
            int cost = effectiveBuyCost(playerId, spec);
            if (spec.cost <= 0 || isInternalUnit(spec.type) || cost > p.money) continue;
            actions.push_back({AiActionKind::Buy, spec.type, kInvalidUnitId, {}});
        }
    }

    for (UnitId id : p.bench) {
        if (id < 0 || id >= static_cast<int>(units_.size())) continue;
        const Unit& u = unit(id);
        if (!u.alive || u.owner != playerId || u.deployed) continue;
        for (int y = 0; y < board_.height; ++y) {
            for (int x = 0; x < board_.width; ++x) {
                Coord coord{x, y};
                if (canDeploy(playerId, coord, u.spec.layer)) {
                    actions.push_back({AiActionKind::Deploy, u.spec.type, id, coord});
                }
            }
        }
    }

    for (UnitId id : p.deployed) {
        if (id < 0 || id >= static_cast<int>(units_.size())) continue;
        const Unit& u = unit(id);
        if (!u.alive || !u.deployed || u.owner != playerId || isInternalUnit(u.spec.type)) continue;
        for (int y = 0; y < board_.height; ++y) {
            for (int x = 0; x < board_.width; ++x) {
                Coord coord{x, y};
                if (coord != u.coord && canDeploy(playerId, coord, u.spec.layer)) {
                    actions.push_back({AiActionKind::MoveDeployed, u.spec.type, id, coord});
                }
            }
        }
        if (static_cast<int>(p.bench.size()) < benchLimit(playerId)) {
            actions.push_back({AiActionKind::ReturnToBench, u.spec.type, id, {}});
        }
    }

    if (hasActiveCombatUnit(playerId)) {
        actions.push_back({AiActionKind::Ready, UnitType::Skeleton, kInvalidUnitId, {}});
    }
    return actions;
}

bool GameEngine::applyAiAction(PlayerId playerId, const AiAction& action) {
    if (phase_ != Phase::Preparation) return false;
    switch (action.kind) {
        case AiActionKind::Buy:
            return buyUnit(playerId, action.type);
        case AiActionKind::Deploy:
            return deployUnit(playerId, action.unitId, action.coord);
        case AiActionKind::MoveDeployed:
            return moveDeployedUnit(playerId, action.unitId, action.coord);
        case AiActionKind::ReturnToBench:
            return returnToBench(playerId, action.unitId);
        case AiActionKind::Upgrade:
            return upgradeUnit(playerId, action.unitId);
        case AiActionKind::Ready:
            if (!hasActiveCombatUnit(playerId)) return false;
            setReady(playerId, true);
            return true;
    }
    return false;
}

std::vector<double> GameEngine::stateFeatures(PlayerId playerId) const {
    std::vector<double> features;
    features.reserve(kStateFeatureCount);

    PlayerId foeId = opponent(playerId);
    const PlayerState& self = player(playerId);
    const PlayerState& foe = player(foeId);

    features.push_back(std::min(1.0, round_ / 20.0));
    features.push_back(std::min(1.0, time_ / 300.0));
    features.push_back(phase_ == Phase::Preparation ? 1.0 : 0.0);
    features.push_back(std::min(1.0, self.money / 120.0));
    features.push_back(std::min(1.0, foe.money / 120.0));
    features.push_back(std::min(1.0, self.bench.size() / 10.0));
    features.push_back(std::min(1.0, foe.bench.size() / 10.0));
    features.push_back(std::min(1.0, self.deployed.size() / 20.0));
    features.push_back(std::min(1.0, foe.deployed.size() / 20.0));
    features.push_back(std::min(1.0, explorationScores_[playerIndex(playerId)] / 200.0));
    features.push_back(std::min(1.0, explorationScores_[playerIndex(foeId)] / 200.0));
    features.push_back(self.ready ? 1.0 : 0.0);
    features.push_back(foe.ready ? 1.0 : 0.0);
    features.push_back(std::min(1.0, combatTime_ / 45.0));
    features.push_back(board_.width / 20.0);
    features.push_back(board_.height / 20.0);

    for (int y = 0; y < board_.height; ++y) {
        for (int x = 0; x < board_.width; ++x) {
            std::array<double, kBoardFeaturePlanes> planes{};
            Coord coord{x, y};
            for (UnitLayer layer : {UnitLayer::Land, UnitLayer::Air}) {
                for (UnitId id : board_.occupants(coord, layer)) {
                    if (id == kInvalidUnitId || id >= static_cast<int>(units_.size())) continue;
                    const Unit& u = unit(id);
                    if (!u.alive) continue;
                    bool friendly = u.owner == playerId;
                    double threat = std::min(1.0, u.spec.threat / 100.0);
                    double hpRatio = u.spec.maxHp * u.spec.unitCount > 0
                                         ? static_cast<double>(totalHp(u)) / (u.spec.maxHp * u.spec.unitCount)
                                         : 0.0;
                    if (friendly && layer == UnitLayer::Land) planes[0] = std::max(planes[0], threat);
                    if (!friendly && layer == UnitLayer::Land) planes[1] = std::max(planes[1], threat);
                    if (friendly && layer == UnitLayer::Air) planes[2] = std::max(planes[2], threat);
                    if (!friendly && layer == UnitLayer::Air) planes[3] = std::max(planes[3], threat);
                    if (friendly) planes[4] = std::max(planes[4], hpRatio);
                    if (!friendly) planes[5] = std::max(planes[5], hpRatio);
                }
            }
            for (double value : planes) features.push_back(clampFeature(value));
        }
    }

    if (features.size() < kStateFeatureCount) features.resize(kStateFeatureCount, 0.0);
    if (features.size() > kStateFeatureCount) features.resize(kStateFeatureCount);
    return features;
}

std::vector<double> GameEngine::actionFeatures(PlayerId playerId, const AiAction& action) const {
    std::vector<double> features(kActionFeatureCount, 0.0);
    int kindIndex = static_cast<int>(action.kind);
    if (kindIndex >= 0 && kindIndex < 6) features[kindIndex] = 1.0;

    const UnitSpec* spec = specFor(action.type);
    const Unit* actionUnit = nullptr;
    if (action.unitId >= 0 && action.unitId < static_cast<int>(units_.size())) {
        actionUnit = &unit(action.unitId);
        spec = &actionUnit->spec;
    }
    if (spec) {
        constexpr double kMaxUnitType = static_cast<double>(static_cast<int>(UnitType::SporeServant));
        features[6] = kMaxUnitType > 0.0 ? static_cast<int>(spec->type) / kMaxUnitType : 0.0;
        features[7] = std::min(1.0, spec->cost / 24.0);
        features[8] = std::min(1.0, spec->unitCount / 5.0);
        features[9] = std::min(1.0, spec->maxHp / 400.0);
        features[10] = std::min(1.0, spec->attack / 120.0);
        features[11] = std::min(1.0, spec->range / 6.0);
        features[12] = std::min(1.0, spec->speed / 5.0);
        features[13] = std::min(1.0, spec->threat / 100.0);
        features[14] = spec->layer == UnitLayer::Air ? 1.0 : 0.0;
        features[15] = spec->canAttackAir ? 1.0 : 0.0;
    }
    features[16] = board_.width > 1 ? action.coord.x / static_cast<double>(board_.width - 1) : 0.0;
    features[17] = board_.height > 1 ? action.coord.y / static_cast<double>(board_.height - 1) : 0.0;
    features[18] = playerId == PlayerId::One ? features[16] : 1.0 - features[16];
    features[19] = actionUnit && actionUnit->upgraded ? 1.0 : 0.0;
    return features;
}

void GameEngine::prepareAiPlayer(PlayerId playerId) {
    aiPrepare(playerId);
}

PlayerState& GameEngine::player(PlayerId id) {
    return players_[playerIndex(id)];
}

const PlayerState& GameEngine::player(PlayerId id) const {
    return players_[playerIndex(id)];
}

PlayerId GameEngine::opponent(PlayerId id) const {
    return id == PlayerId::One ? PlayerId::Two : PlayerId::One;
}

Unit& GameEngine::unit(UnitId id) {
    return units_[id];
}

const Unit& GameEngine::unit(UnitId id) const {
    return units_[id];
}

void GameEngine::resetBoard(MapKind kind) {
    mapKind_ = kind;
    board_ = Board();
    explorationMapTemplate_ = templateIndexForMapKind(kind);
    paintExplorationMap(board_, explorationMapTemplate_);
    applyExplorationObjectiveTerrain();
}

void GameEngine::randomizeExplorationObjectives() {
    std::vector<Coord> placed;
    std::uniform_int_distribution<int> offset(-1, 1);
    auto legalFor = [&](const ExplorationObjectiveState& objective, Coord candidate, size_t selfIndex) {
        if (!board_.inBounds(candidate) || board_.blocked(candidate)) return false;
        if (isExplorationStagingCell(PlayerId::One, candidate) ||
            isExplorationStagingCell(PlayerId::Two, candidate)) {
            return false;
        }
        if (isBossObjectiveKind(objective.kind)) {
            if (!isCentralBossCoord(candidate)) return false;
        } else if (!isOuterObjectiveCoord(candidate) && objective.kind != ExplorationObjectiveKind::Trap) {
            return false;
        }
        for (Coord coord : placed) {
            if (manhattan(coord, candidate) < 3) return false;
        }
        for (size_t otherIndex = 0; otherIndex < exploration_.objectives().size(); ++otherIndex) {
            if (otherIndex == selfIndex) continue;
            const ExplorationObjectiveState* other = exploration_.objective(otherIndex);
            if (!other || other->coord != candidate) continue;
            return false;
        }
        return true;
    };

    for (size_t index = 0; index < exploration_.objectives().size(); ++index) {
        ExplorationObjectiveState* objective = exploration_.objective(index);
        if (!objective) continue;

        Coord anchor = objective->coord;
        std::vector<Coord> candidates;
        candidates.reserve(25);
        for (int dy = -2; dy <= 2; ++dy) {
            for (int dx = -2; dx <= 2; ++dx) {
                Coord candidate{anchor.x + dx, anchor.y + dy};
                if (manhattan(anchor, candidate) > 2) continue;
                candidates.push_back(candidate);
            }
        }
        std::shuffle(candidates.begin(), candidates.end(), rng_);
        std::stable_sort(candidates.begin(), candidates.end(), [anchor](Coord a, Coord b) {
            return manhattan(a, anchor) < manhattan(b, anchor);
        });

        Coord chosen{-1, -1};
        for (Coord candidate : candidates) {
            if (!legalFor(*objective, candidate, index)) continue;
            chosen = candidate;
            break;
        }
        if (chosen.x < 0) {
            int bestScore = std::numeric_limits<int>::max();
            for (int y = 0; y < board_.height; ++y) {
                for (int x = 0; x < board_.width; ++x) {
                    Coord candidate{x, y};
                    if (!legalFor(*objective, candidate, index)) continue;
                    int score = manhattan(anchor, candidate) * 10 + std::abs(anchor.y - candidate.y);
                    if (score < bestScore) {
                        bestScore = score;
                        chosen = candidate;
                    }
                }
            }
        }
        if (chosen.x < 0) chosen = anchor;
        objective->coord = chosen;
        objective->owner = chosen.x < kBoardWidth / 2 ? PlayerId::Two : PlayerId::One;
        placed.push_back(chosen);
    }
    exploration_.rebuildObjectiveIndexes();
}

void GameEngine::initializeNeutralObjectives() {
    exploration_.resetObjectives(explorationMapTemplate_);
    randomizeExplorationObjectives();

    for (size_t index = 0; index < exploration_.objectives().size(); ++index) {
        ExplorationObjectiveState* objective = exploration_.objective(index);
        if (!objective || !isCombatExplorationObjective(objective->kind)) continue;
        if (!board_.inBounds(objective->coord) || board_.blocked(objective->coord) ||
            isExplorationStagingCell(PlayerId::One, objective->coord) ||
            isExplorationStagingCell(PlayerId::Two, objective->coord)) {
            Coord fallback{-1, -1};
            int bestScore = std::numeric_limits<int>::max();
            for (int y = 0; y < board_.height; ++y) {
                for (int x = 0; x < board_.width; ++x) {
                    Coord candidate{x, y};
                    if (!board_.inBounds(candidate) || board_.blocked(candidate)) continue;
                    if (isExplorationStagingCell(PlayerId::One, candidate) ||
                        isExplorationStagingCell(PlayerId::Two, candidate)) {
                        continue;
                    }
                    TerrainKind terrain = board_.terrainAt(candidate);
                    if (terrain == TerrainKind::NeutralCamp || terrain == TerrainKind::BossSite ||
                        terrain == TerrainKind::Trap) {
                        continue;
                    }
                    bool occupiedObjective = false;
                    for (size_t otherIndex = 0; otherIndex < exploration_.objectives().size(); ++otherIndex) {
                        if (otherIndex == index) continue;
                        const ExplorationObjectiveState* other = exploration_.objective(otherIndex);
                        if (!other || other->cleared) continue;
                        if (other->coord == candidate || manhattan(other->coord, candidate) < 2) {
                            occupiedObjective = true;
                            break;
                        }
                    }
                    if (occupiedObjective) continue;
                    int sideBias = objective->owner == PlayerId::Two
                                       ? std::abs(candidate.x - std::max(5, objective->coord.x))
                                       : std::abs(candidate.x - std::min(kBoardWidth - 6, objective->coord.x));
                    int score = manhattan(candidate, objective->coord) * 10 + sideBias + std::abs(candidate.y - objective->coord.y);
                    if (score < bestScore) {
                        bestScore = score;
                        fallback = candidate;
                    }
                }
            }
            if (fallback.x >= 0) {
                objective->coord = fallback;
            }
        }
        if (isExplorationStagingCell(PlayerId::One, objective->coord) ||
            isExplorationStagingCell(PlayerId::Two, objective->coord)) {
            continue;
        }

        UnitId campUnit = createUnit(objective->owner, objective->type);
        Unit& neutral = unit(campUnit);
        if (objective->kind == ExplorationObjectiveKind::Elite) {
            neutral.spec.name = "Elite " + neutral.spec.name;
            neutral.spec.maxHp = static_cast<int>(std::lround(neutral.spec.maxHp * 1.18));
            neutral.spec.attack = static_cast<int>(std::lround(neutral.spec.attack * 1.12));
            neutral.spec.threat += 16;
            neutral.hp.assign(neutral.spec.unitCount, neutral.spec.maxHp);
        } else if (objective->kind == ExplorationObjectiveKind::Boss) {
            neutral.spec.name = "Boss " + neutral.spec.name;
            neutral.spec.maxHp = static_cast<int>(std::lround(neutral.spec.maxHp * 2.60));
            neutral.spec.attack = static_cast<int>(std::lround(neutral.spec.attack * 1.32));
            neutral.spec.armorClass += 2;
            neutral.spec.attackBonus += 2;
            neutral.spec.savingThrowBonus += 2;
            neutral.spec.spellSaveDc += 2;
            neutral.spec.threat += 70;
            neutral.hp.assign(neutral.spec.unitCount, neutral.spec.maxHp);
        }
        if (!placeUnit(campUnit, objective->coord)) {
            neutral.alive = false;
            neutral.deployed = false;
            neutral.hp.clear();
            neutral.coord = {-1, -1};
            neutral.lastCoord = neutral.coord;
            neutral.homeCoord = neutral.coord;
            continue;
        }
        neutral.deployed = true;
        neutral.homeCoord = objective->coord;
        neutral.neutralBehavior = NeutralBehavior::PassiveGuardian;
        exploration_.setObjectiveUnit(index, campUnit);
        player(objective->owner).deployed.push_back(campUnit);
    }

    applyExplorationObjectiveTerrain();
}

void GameEngine::initializeHiddenExplorationEvents() {
    exploration_.resetHiddenEvents({}, rng_);

    std::vector<Coord> candidates;
    for (int y = 0; y < board_.height; ++y) {
        for (int x = 0; x < board_.width; ++x) {
            Coord coord{x, y};
            if (!board_.inBounds(coord) || board_.blocked(coord)) continue;
            if (isDeploymentCell(PlayerId::One, coord) || isDeploymentCell(PlayerId::Two, coord)) continue;
            TerrainKind terrain = board_.terrainAt(coord);
            if (terrain != TerrainKind::SideRoad && terrain != TerrainKind::Open) continue;
            bool objectiveCell = false;
            for (const ExplorationObjectiveState& objective : exploration_.objectives()) {
                if (objective.coord == coord) {
                    objectiveCell = true;
                    break;
                }
            }
            if (objectiveCell) continue;
            bool occupied = false;
            for (UnitLayer layer : {UnitLayer::Land, UnitLayer::Air}) {
                if (!board_.occupants(coord, layer).empty()) {
                    occupied = true;
                    break;
                }
            }
            if (occupied) continue;
            candidates.push_back(coord);
        }
    }

    exploration_.resetHiddenEvents(std::move(candidates), rng_);
}

void GameEngine::applyExplorationObjectiveTerrain() {
    for (const ExplorationObjectiveState& objective : exploration_.objectives()) {
        if (!board_.inBounds(objective.coord)) continue;
        if (objective.cleared) {
            board_.setTerrain(objective.coord,
                              objective.kind == ExplorationObjectiveKind::Boss
                                  ? TerrainKind::ClearedBoss
                                  : TerrainKind::ClearedObjective);
            continue;
        }
        switch (objective.kind) {
            case ExplorationObjectiveKind::Camp:
            case ExplorationObjectiveKind::Elite:
                board_.setTerrain(objective.coord, TerrainKind::NeutralCamp);
                break;
            case ExplorationObjectiveKind::Boss:
                board_.setTerrain(objective.coord, TerrainKind::BossSite);
                break;
            case ExplorationObjectiveKind::Trap:
                if (objective.triggered) {
                    board_.setTerrain(objective.coord, TerrainKind::Trap);
                }
                break;
        }
    }
}

bool GameEngine::triggerTrapAt(PlayerId triggeringPlayer, Coord coord, UnitId triggerUnitId) {
    std::optional<size_t> objectiveIndex = exploration_.findTriggerableTrap(coord);
    if (!objectiveIndex) return false;

    exploration_.markTriggered(*objectiveIndex);
    board_.setTerrain(coord, TerrainKind::Trap);
    int spawned = spawnRedcapAmbush(triggeringPlayer, coord, triggerUnitId);
    pushEvent({EventType::StatusApplied, triggeringPlayer, kInvalidUnitId, kInvalidUnitId,
               coord, coord, spawned,
               "Redcap ambush triggered"});
    if (spawned <= 0) clearExplorationObjective(*objectiveIndex, triggeringPlayer, triggerUnitId);
    return true;
}

bool GameEngine::triggerRandomGoldEventAt(PlayerId triggeringPlayer, Coord coord, UnitId triggerUnitId) {
    if (triggerUnitId != kInvalidUnitId) {
        if (triggerUnitId < 0 || triggerUnitId >= static_cast<int>(units_.size())) return false;
        const Unit& trigger = unit(triggerUnitId);
        if (!canTriggerHiddenEvent(trigger)) return false;
    }
    std::optional<RandomGoldEventState> event =
        exploration_.claimRandomGold(triggeringPlayer, coord, triggerUnitId);
    if (!event) return false;

    int payout = economyPayout(event->amount);
    player(triggeringPlayer).money += payout;
    addExplorationScore(triggeringPlayer, payout);
    ++hiddenEventsClaimedByPlayer_[playerIndex(triggeringPlayer)];
    pushEvent({EventType::GoldGained, triggeringPlayer, triggerUnitId, kInvalidUnitId,
               coord, coord, payout,
               "Hidden cache found: +" + std::to_string(payout) + " gp"});
    return true;
}

bool GameEngine::canTriggerHiddenEvent(const Unit& trigger) const {
    if (!trigger.alive || !trigger.deployed) return false;
    if (trigger.neutralControlled) return false;
    if (isInternalUnitType(trigger.spec.type)) return false;
    if (isNeutralMonsterType(trigger.spec.type)) return false;
    return true;
}

bool GameEngine::triggerHiddenEventAt(PlayerId triggeringPlayer, Coord coord, UnitId triggerUnitId) {
    if (triggerUnitId != kInvalidUnitId) {
        if (triggerUnitId < 0 || triggerUnitId >= static_cast<int>(units_.size())) return false;
        const Unit& trigger = unit(triggerUnitId);
        if (trigger.owner != triggeringPlayer || !canTriggerHiddenEvent(trigger)) return false;
    }

    std::optional<HiddenExplorationEventState> event =
        exploration_.claimHiddenEvent(triggeringPlayer, coord, triggerUnitId);
    if (!event) return false;

    if (event->kind == HiddenExplorationEventKind::GoldCache) {
        int payout = economyPayout(event->amount);
        player(triggeringPlayer).money += payout;
        addExplorationScore(triggeringPlayer, payout);
        ++hiddenEventsClaimedByPlayer_[playerIndex(triggeringPlayer)];
        pushEvent({EventType::GoldGained, triggeringPlayer, triggerUnitId, kInvalidUnitId,
                   coord, coord, payout,
                   "Hidden cache found: +" + std::to_string(payout) + " gp"});
        return true;
    }

    int healedTotal = 0;
    std::vector<UnitId> allies = player(triggeringPlayer).deployed;
    for (UnitId id : allies) {
        if (id < 0 || id >= static_cast<int>(units_.size())) continue;
        Unit& ally = unit(id);
        if (!canTriggerHiddenEvent(ally) || ally.owner != triggeringPlayer || ally.hp.empty()) continue;
        for (int& hp : ally.hp) {
            int before = hp;
            hp = std::min(ally.spec.maxHp, hp + event->amount);
            healedTotal += hp - before;
        }
    }
    ++hiddenEventsClaimedByPlayer_[playerIndex(triggeringPlayer)];
    pushEvent({EventType::Healed, triggeringPlayer, triggerUnitId, kInvalidUnitId,
               coord, coord, healedTotal,
               "Hidden healing spring restored the party"});
    return true;
}

int GameEngine::spawnRedcapAmbush(PlayerId triggeringPlayer, Coord origin, UnitId triggerUnitId) {
    PlayerId redcapOwner = opponent(triggeringPlayer);
    std::vector<Coord> candidates = adjacentCells(origin);
    for (Coord coord : cellsInRange(origin, 2)) {
        if (std::find(candidates.begin(), candidates.end(), coord) == candidates.end()) {
            candidates.push_back(coord);
        }
    }

    auto landable = [&](Coord coord) {
        if (!board_.inBounds(coord) || board_.blocked(coord)) return false;
        for (UnitId id : board_.occupants(coord, UnitLayer::Land)) {
            if (id < 0 || id >= static_cast<int>(units_.size())) continue;
            const Unit& occupant = unit(id);
            if (occupant.alive && occupant.deployed) return false;
        }
        return true;
    };

    int spawned = 0;
    for (Coord coord : candidates) {
        if (spawned >= 3) break;
        if (!landable(coord)) continue;
        UnitId id = createUnit(redcapOwner, UnitType::NeutralRedcap);
        if (!placeUnit(id, coord)) {
            unit(id).alive = false;
            continue;
        }
        unit(id).deployed = true;
        unit(id).homeCoord = origin;
        unit(id).lastCoord = coord;
        unit(id).neutralBehavior = NeutralBehavior::HostileAmbusher;
        unit(id).neutralProvoked = true;
        unit(id).neutralReturningHome = false;
        unit(id).provokedBy = triggerUnitId;
        unit(id).target = triggerUnitId;
        unit(id).retargetTimer = 0.0;
        player(redcapOwner).deployed.push_back(id);
        if (std::optional<size_t> trapIndex = exploration_.findTrapForSpawn(origin)) {
            exploration_.recordTrapSpawn(*trapIndex, id);
        }
        ++spawned;
    }
    return spawned;
}

void GameEngine::clearExplorationObjective(size_t index, PlayerId clearer, UnitId actorId) {
    ExplorationObjectiveState* objective = exploration_.objective(index);
    if (!objective || objective->cleared) return;

    exploration_.markCleared(index);
    applyExplorationObjectiveTerrain();

    PlayerState& clearerState = player(clearer);
    int bonusGold = (objective->kind == ExplorationObjectiveKind::Elite ||
                     objective->kind == ExplorationObjectiveKind::Boss)
                        ? std::max(0, runModifiers_[playerIndex(clearer)].bonusGoldOnClear)
                        : 0;
    int totalRewardGold = economyPayout(objective->rewardGold + bonusGold);
    if (totalRewardGold > 0) {
        clearerState.money += totalRewardGold;
    }
    addExplorationScore(clearer, objective->rewardGold + objective->rewardQuality * 4);
    ++explorationObjectivesClearedByPlayer_[playerIndex(clearer)];
    if (objective->kind == ExplorationObjectiveKind::Boss) {
        ++explorationBossesClearedByPlayer_[playerIndex(clearer)];
    }

    std::string label;
    switch (objective->kind) {
        case ExplorationObjectiveKind::Camp: label = "Camp cleared"; break;
        case ExplorationObjectiveKind::Elite: label = "Elite cleared"; break;
        case ExplorationObjectiveKind::Boss: label = "Boss defeated"; break;
        case ExplorationObjectiveKind::Trap: label = "Trap cleared"; break;
    }

    std::string text = label + ": +" + std::to_string(totalRewardGold) + " gp";
    pushEvent({EventType::GoldGained,
               clearer,
               actorId,
               objective->unitId,
               objective->coord,
               objective->coord,
               totalRewardGold,
               std::move(text)});
}

void GameEngine::updateExplorationObjectiveForDeath(UnitId deadId, UnitId sourceId) {
    PlayerId clearer = PlayerId::One;
    if (sourceId >= 0 && sourceId < static_cast<int>(units_.size()) &&
        !isNeutralLikeCombatant(unit(sourceId))) {
        clearer = unit(sourceId).owner;
    }

    std::optional<size_t> objectiveIndex = exploration_.findObjectiveForUnit(deadId);
    if (!objectiveIndex) return;

    const ExplorationObjectiveState* objective = exploration_.objective(*objectiveIndex);
    if (!objective || objective->cleared) return;
    if (isCombatExplorationObjective(objective->kind) && objective->unitId == deadId) {
        clearExplorationObjective(*objectiveIndex, clearer, sourceId);
        return;
    }
    if (objective->kind == ExplorationObjectiveKind::Trap) {
        bool anyAlive = false;
        for (UnitId id : objective->spawnedUnitIds) {
            if (id >= 0 && id < static_cast<int>(units_.size()) && unit(id).alive) {
                anyAlive = true;
                break;
            }
        }
        if (!anyAlive && objective->triggered) {
            clearExplorationObjective(*objectiveIndex, clearer, sourceId);
        }
    }
}

void GameEngine::addExplorationScore(PlayerId playerId, int amount) {
    if (amount <= 0) return;
    explorationScores_[playerIndex(playerId)] += amount;
}

int GameEngine::explorationTiebreakThreat(PlayerId playerId) const {
    int total = 0;
    for (const Unit& u : units_) {
        if (!u.alive || u.owner != playerId || isInternalUnitType(u.spec.type) ||
            isNeutralLikeCombatant(u)) {
            continue;
        }
        total += std::max(0, u.spec.threat);
    }
    return total;
}

std::optional<PlayerId> GameEngine::explorationWinner() const {
    int one = explorationScores_[playerIndex(PlayerId::One)];
    int two = explorationScores_[playerIndex(PlayerId::Two)];
    if (one != two) return one > two ? PlayerId::One : PlayerId::Two;

    int oneGold = player(PlayerId::One).money;
    int twoGold = player(PlayerId::Two).money;
    if (oneGold != twoGold) return oneGold > twoGold ? PlayerId::One : PlayerId::Two;

    int oneBosses = explorationBossesClearedByPlayer_[playerIndex(PlayerId::One)];
    int twoBosses = explorationBossesClearedByPlayer_[playerIndex(PlayerId::Two)];
    if (oneBosses != twoBosses) return oneBosses > twoBosses ? PlayerId::One : PlayerId::Two;

    int oneThreat = explorationTiebreakThreat(PlayerId::One);
    int twoThreat = explorationTiebreakThreat(PlayerId::Two);
    if (oneThreat != twoThreat) return oneThreat > twoThreat ? PlayerId::One : PlayerId::Two;

    return std::nullopt;
}

UnitId GameEngine::createUnit(PlayerId owner, UnitType type) {
    const UnitSpec* spec = specFor(type);
    Unit unit;
    unit.id = nextUnitId_++;
    unit.spec = spec ? *spec : UnitSpec{};
    unit.owner = owner;
    const RunModifiers& modifiers =
        (isInternalUnitType(type) || isNeutralMonsterType(type))
            ? RunModifiers{}
            : runModifiers_[playerIndex(owner)];
    int familyBias = modifiers.familyBias[static_cast<size_t>(familyIndexForUnitType(type))];
    if (familyBias != 0) {
        switch (familyIndexForUnitType(type)) {
            case static_cast<int>(NeutralFamily::Swarm):
                unit.spec.maxHp += familyBias * 3;
                unit.spec.attack += familyBias;
                break;
            case static_cast<int>(NeutralFamily::Guardian):
                unit.spec.maxHp += familyBias * 8;
                unit.spec.armorClass += familyBias;
                unit.spec.attackBonus += familyBias / 2;
                break;
            case static_cast<int>(NeutralFamily::Caster):
                unit.spec.attackCooldown = std::max(0.25, unit.spec.attackCooldown - familyBias * 0.05);
                unit.spec.spellSaveDc += familyBias;
                unit.spec.range += familyBias > 1 ? 1 : 0;
                break;
            case static_cast<int>(NeutralFamily::Assassin):
                unit.spec.attackBonus += familyBias;
                unit.spec.speed += 0.15 * familyBias;
                break;
            case static_cast<int>(NeutralFamily::Artillery):
                unit.spec.range += familyBias;
                unit.spec.attack += familyBias * 2;
                break;
        }
    }
    unit.hp.assign(unit.spec.unitCount, unit.spec.maxHp);
    unit.coord = {-1, -1};
    unit.lastCoord = unit.coord;
    unit.homeCoord = unit.coord;
    unit.attackTimer = 0.0;
    unit.abilityTimer = 0.0;

    if (type == UnitType::Treant) {
        unit.lifespan = 8.0;
        unit.statuses.push_back({StatusKind::Summoned, 8.0, 0, kInvalidUnitId});
    } else if (type == UnitType::SporeServant) {
        unit.lifespan = 12.0;
        unit.statuses.push_back({StatusKind::Summoned, 12.0, 0, kInvalidUnitId});
    } else if (type == UnitType::SkeletonByNecromancer) {
        unit.lifespan = kNecromancerSkeletonLifespan;
        unit.statuses.push_back({StatusKind::Summoned, kNecromancerSkeletonLifespan, 0, kInvalidUnitId});
    }
    if (unit.spec.ability == AbilityKind::GuardianShield) {
        unit.statuses.push_back({StatusKind::Taunt, 9999.0, 80, unit.id});
    }

    units_.push_back(unit);
    return unit.id;
}

bool GameEngine::placeUnit(UnitId id, Coord coord) {
    Unit& u = unit(id);
    if (!board_.inBounds(coord)) return false;
    if (u.spec.layer == UnitLayer::Land && board_.blocked(coord)) return false;
    if (u.spec.layer == UnitLayer::Land) {
        for (UnitId other : board_.occupants(coord, UnitLayer::Land)) {
            if (other < 0 || other >= static_cast<int>(units_.size()) || other == id) continue;
            const Unit& occupant = unit(other);
            if (occupant.alive && occupant.deployed) return false;
        }
    }
    u.coord = coord;
    board_.addOccupant(coord, u.spec.layer, id);
    return true;
}

void GameEngine::removeFromBoard(UnitId id) {
    if (id < 0 || id >= static_cast<int>(units_.size())) return;
    Unit& u = unit(id);
    board_.removeOccupant(u.coord, u.spec.layer, id);
}

void GameEngine::pushEvent(Event event) {
    events_.push_back(std::move(event));
}

void GameEngine::addRecentBuy(PlayerState& p, UnitType type) {
    p.recentBuys.push_back(type);
    if (p.recentBuys.size() > 5) p.recentBuys.erase(p.recentBuys.begin());
}

void GameEngine::startCombatIfReady() {
    if (phase_ != Phase::Preparation) return;
    if (players_[0].ready && players_[1].ready) startCombat();
}

void GameEngine::startCombat() {
    phase_ = Phase::Combat;
    combatTime_ = 0.0;
    for (Unit& u : units_) {
        if (!u.alive || !u.deployed) continue;
        u.target = kInvalidUnitId;
        u.retargetTimer = 0.0;
        u.moveProgress = 0.0;
        u.lastCoord = u.coord;
        if (!isNeutralMonsterType(u.spec.type) && !isNeutralSpawServant(u)) {
            u.neutralReturningHome = false;
        }
        if (!isRoundTransientUnit(u.spec.type) && !isNeutralLikeCombatant(u)) u.homeCoord = u.coord;
        if (u.neutralBehavior == NeutralBehavior::HostileAmbusher) u.neutralProvoked = true;
        if (u.spec.ability == AbilityKind::PaladinCharge) u.firstStrikeReady = true;
        if (u.spec.ability == AbilityKind::GithyankiAstralRaid) {
            performGithyankiAstralRaid(u.id);
        }
        if (u.spec.ability == AbilityKind::RogueAmbush) {
            performAssassinLeap(u.id);
        }
    }
    pushEvent({EventType::CombatStarted, PlayerId::One, kInvalidUnitId, kInvalidUnitId,
               {}, {}, round_, "Combat started"});
}

void GameEngine::finishCombat(PlayerId winner) {
    winner_ = winner;
    phase_ = Phase::Finished;
    pushEvent({EventType::Victory, winner, kInvalidUnitId, kInvalidUnitId, {}, {}, round_,
               toString(winner) + " wins"});
}

void GameEngine::finishExplorationRun() {
    if (phase_ == Phase::Finished) return;
    winner_ = explorationWinner();
    phase_ = Phase::Finished;

    std::string result = "Exploration run complete: score " +
                         std::to_string(explorationScores_[playerIndex(PlayerId::One)]) + "/" +
                         std::to_string(explorationScores_[playerIndex(PlayerId::Two)]);
    if (winner_) {
        result += " - " + toString(*winner_) + " wins";
        pushEvent({EventType::Victory, *winner_, kInvalidUnitId, kInvalidUnitId, {}, {}, round_, result});
    } else {
        result += " - no winner";
        pushEvent({EventType::Victory, PlayerId::One, kInvalidUnitId, kInvalidUnitId, {}, {}, round_, result});
    }
}

void GameEngine::startNextRound() {
    startNextRound("");
}

void GameEngine::startNextRound(const std::string& reason) {
    if (!reason.empty()) {
        pushEvent({EventType::RoundStarted, PlayerId::One, kInvalidUnitId, kInvalidUnitId,
                   {}, {}, round_, "Round ended: " + reason});
    }
    resetCombatantsForPreparation();
    phase_ = Phase::Preparation;
    ++explorationRound_;
    ++round_;
    combatTime_ = 0.0;

    std::array<int, 2> incomes{};
    for (PlayerState& p : players_) {
        p.ready = false;
        int income = roundIncomeFor(p, round_, runModifiers_[playerIndex(p.id)]);
        p.money += income;
        incomes[playerIndex(p.id)] = income;
    }
    std::string roundText = "Exploration round " + std::to_string(explorationRound_) + "/" +
                            std::to_string(explorationRoundLimit_) +
                            " complete - income +" + std::to_string(incomes[0]) +
                            "/+" + std::to_string(incomes[1]);
    pushEvent({EventType::RoundStarted, PlayerId::One, kInvalidUnitId, kInvalidUnitId,
               {}, {}, incomes[0], roundText});

    if (shouldFinishExplorationRun()) {
        std::string completion = explorationCompletionReason();
        if (!completion.empty()) {
            pushEvent({EventType::RoundStarted, PlayerId::One, kInvalidUnitId, kInvalidUnitId,
                       {}, {}, explorationRound_, completion});
        }
        finishExplorationRun();
    }
}

bool GameEngine::shouldFinishExplorationRun() const {
    if (explorationRound_ >= explorationRoundLimit_) return true;
    ExplorationStats explorationStats = exploration_.stats();
    return explorationStats.objectivesTotal > 0 &&
           explorationStats.objectivesCleared >= explorationStats.objectivesTotal;
}

std::string GameEngine::explorationCompletionReason() const {
    ExplorationStats explorationStats = exploration_.stats();
    if (explorationStats.objectivesTotal > 0 &&
        explorationStats.objectivesCleared >= explorationStats.objectivesTotal) {
        return "Exploration complete: objectives cleared";
    }
    if (explorationRound_ >= explorationRoundLimit_) {
        return "Exploration complete: round limit reached";
    }
    return "";
}

int GameEngine::explorationObjectivesCleared() const {
    return exploration_.stats().objectivesCleared;
}

int GameEngine::explorationObjectivesTotal() const {
    return exploration_.stats().objectivesTotal;
}

int GameEngine::explorationBossesCleared() const {
    return exploration_.stats().bossesCleared;
}

int GameEngine::explorationEventsTriggered() const {
    return exploration_.stats().eventsTriggered;
}

int GameEngine::explorationTrapsTriggered() const {
    return exploration_.stats().trapsTriggered;
}

int GameEngine::randomGoldEventsClaimed() const {
    return exploration_.stats().randomGoldEventsClaimed;
}

void GameEngine::resetCombatantsForPreparation() {
    resetBoard(mapKind_);
    corpses_.clear();

    for (PlayerState& p : players_) {
        p.bench.clear();
        p.deployed.clear();
        p.recentBuys.clear();
    }

    for (Unit& u : units_) {
        bool neutralSporeServant = isNeutralSpawServant(u);
        UnitId summonedBy = kInvalidUnitId;
        double summonedRemaining = -1.0;
        if (neutralSporeServant) {
            for (const StatusEffect& status : u.statuses) {
                if (status.kind == StatusKind::Summoned && status.source != kInvalidUnitId) {
                    summonedBy = status.source;
                    summonedRemaining = status.remaining;
                    break;
                }
            }
        }

        u.shield = 0;
        u.target = kInvalidUnitId;
        u.retargetTimer = 0.0;
        u.moveProgress = 0.0;
        u.attackTimer = 0.0;
        u.abilityTimer = 0.0;
        if (!neutralSporeServant) u.lifespan = -1.0;
        u.firstStrikeReady = false;
        if (u.neutralBehavior == NeutralBehavior::PassiveGuardian && isNeutralMonsterType(u.spec.type)) {
            u.neutralReturningHome = board_.inBounds(u.homeCoord) && board_.inBounds(u.coord) &&
                                     u.coord != u.homeCoord;
        } else if (u.neutralBehavior != NeutralBehavior::HostileAmbusher && !neutralSporeServant) {
            u.neutralReturningHome = false;
        }
        u.neutralProvoked = u.neutralBehavior == NeutralBehavior::HostileAmbusher;
        if (u.neutralBehavior != NeutralBehavior::HostileAmbusher) u.provokedBy = kInvalidUnitId;
        u.specialCounter = 0;
        u.stuckTicks = 0;
        u.statuses.clear();
        if (neutralSporeServant) {
            double remaining = summonedRemaining > 0.0 ? summonedRemaining : std::max(0.1, u.lifespan);
            u.lifespan = remaining;
            u.statuses.push_back({StatusKind::Summoned, remaining, 0, summonedBy});
            u.neutralBehavior = NeutralBehavior::PassiveGuardian;
            u.neutralProvoked = true;
            u.neutralReturningHome = false;
        }

        if (isRoundTransientUnit(u.spec.type)) {
            u.alive = false;
            u.deployed = false;
            u.hp.clear();
            u.coord = {};
            u.lastCoord = {};
            u.homeCoord = {};
            u.neutralReturningHome = false;
            continue;
        }

        if (!u.alive || u.hp.empty()) {
            u.deployed = false;
            if (!isInternalUnit(u.spec.type)) {
                u.coord = {-1, -1};
                u.lastCoord = u.coord;
                u.homeCoord = u.coord;
                u.neutralReturningHome = false;
            }
            continue;
        }

        if (!isInternalUnit(u.spec.type) || neutralSporeServant || isNeutralMonsterType(u.spec.type)) {
            Coord stay = board_.inBounds(u.coord) ? u.coord : u.homeCoord;
            Coord leashOrigin = neutralSporeServant ? neutralLeashOrigin(u) : u.homeCoord;
            if (neutralSporeServant && manhattan(stay, leashOrigin) > kNeutralSummonGuardRadius) {
                stay = leashOrigin;
            }
            bool placed = board_.inBounds(stay) && placeUnit(u.id, stay);
            if (!placed && board_.inBounds(stay)) {
                int radius = neutralSporeServant ? kNeutralSummonGuardRadius
                             : (u.spec.type == UnitType::NeutralRedcap ? kRedcapAmbushRadius : 2);
                for (Coord candidate : cellsInRange(stay, radius)) {
                    if (neutralSporeServant &&
                        manhattan(candidate, leashOrigin) > kNeutralSummonGuardRadius) {
                        continue;
                    }
                    if (placeUnit(u.id, candidate)) {
                        stay = candidate;
                        placed = true;
                        break;
                    }
                }
            }

            if (placed) {
                u.coord = stay;
                u.lastCoord = stay;
                u.deployed = true;
                player(u.owner).deployed.push_back(u.id);
            } else {
                u.deployed = false;
                u.coord = {-1, -1};
                u.lastCoord = u.coord;
                if (!neutralSporeServant && !isNeutralMonsterType(u.spec.type)) {
                    u.homeCoord = u.coord;
                    u.neutralReturningHome = false;
                    player(u.owner).bench.push_back(u.id);
                }
            }
        } else {
            bool wasDeployed = u.deployed || board_.inBounds(u.homeCoord);
            Coord home = board_.inBounds(u.homeCoord) ? u.homeCoord : u.coord;
            if (u.spec.ability == AbilityKind::GuardianShield) {
                u.statuses.push_back({StatusKind::Taunt, 9999.0, 80, u.id});
            }

            if (wasDeployed && board_.inBounds(home) && isDeploymentCell(u.owner, home)) {
                u.coord = home;
                u.lastCoord = home;
                u.homeCoord = home;
                u.deployed = true;
                placeUnit(u.id, home);
                player(u.owner).deployed.push_back(u.id);
            } else {
                u.deployed = false;
                u.coord = {-1, -1};
                u.lastCoord = u.coord;
                u.homeCoord = u.coord;
                u.neutralReturningHome = false;
                player(u.owner).bench.push_back(u.id);
            }
        }
    }

    bool trapTerrainChanged = false;
    for (size_t index = 0; index < exploration_.objectives().size(); ++index) {
        const ExplorationObjectiveState* objective = exploration_.objective(index);
        if (!objective || objective->kind != ExplorationObjectiveKind::Trap ||
            !objective->triggered || objective->cleared) {
            continue;
        }
        bool hasLiveRedcap = false;
        for (UnitId id : objective->spawnedUnitIds) {
            if (id < 0 || id >= static_cast<int>(units_.size())) continue;
            const Unit& redcap = unit(id);
            if (redcap.alive && redcap.deployed && redcap.spec.type == UnitType::NeutralRedcap) {
                hasLiveRedcap = true;
                break;
            }
        }
        if (!hasLiveRedcap) {
            exploration_.markCleared(index);
            exploration_.clearTrapSpawns(index);
            trapTerrainChanged = true;
        }
    }
    if (trapTerrainChanged) applyExplorationObjectiveTerrain();
}

void GameEngine::ensureAiPlanner() {
    if (!aiPlanner_) loadAiPlanner();
}

void GameEngine::loadAiPlanner() {
    aiPolicyMetadata_ = AiPolicyMetadata{};
    aiPolicyMetadata_.format = kPolicyFormat;
    aiPolicyMetadata_.modelVersion = kPolicyModelVersion;
    aiPolicyMetadata_.difficulty = toString(config_.aiDifficulty);
    aiPolicyMetadata_.rulesFingerprint = rulesFingerprint();
    aiPolicyMetadata_.stateFeatureCount = kStateFeatureCount;
    aiPolicyMetadata_.actionFeatureCount = kActionFeatureCount;

    if (config_.aiDifficulty == AiDifficulty::Normal) {
        useScriptedNormalAiPlanner();
        return;
    }

    std::string path = joinPath(config_.aiPolicyDirectory, policyFileName(config_.aiDifficulty));
    aiPolicyMetadata_.path = path;

    std::string json;
    if (!readTextFile(path, json)) {
        useHeuristicAiPlanner("policy file missing: " + path);
        return;
    }

    auto format = jsonStringValue(json, "format");
    auto modelVersion = jsonStringValue(json, "modelVersion");
    auto difficulty = jsonStringValue(json, "difficulty");
    auto fingerprint = jsonStringValue(json, "rulesFingerprint");
    auto stateCount = jsonNumberValue(json, "stateFeatureCount");
    auto actionCount = jsonNumberValue(json, "actionFeatureCount");
    auto heuristicBlend = jsonNumberValue(json, "heuristicBlend");
    auto bias = jsonNumberValue(json, "bias");
    std::vector<double> weights = jsonNumberArray(json, "weights");

    if (format) aiPolicyMetadata_.format = *format;
    if (modelVersion) aiPolicyMetadata_.modelVersion = *modelVersion;
    if (difficulty) aiPolicyMetadata_.difficulty = *difficulty;
    if (fingerprint) aiPolicyMetadata_.rulesFingerprint = *fingerprint;
    if (stateCount) aiPolicyMetadata_.stateFeatureCount = static_cast<int>(*stateCount);
    if (actionCount) aiPolicyMetadata_.actionFeatureCount = static_cast<int>(*actionCount);
    if (heuristicBlend) aiPolicyMetadata_.heuristicBlend = *heuristicBlend;
    if (bias) aiPolicyMetadata_.bias = *bias;
    aiPolicyMetadata_.loaded = true;

    const int expectedWeights = kStateFeatureCount + kActionFeatureCount;
    std::string expectedDifficulty = toString(config_.aiDifficulty);
    if (aiPolicyMetadata_.format != kPolicyFormat) {
        useHeuristicAiPlanner("policy format mismatch: " + aiPolicyMetadata_.format);
        return;
    }
    if (aiPolicyMetadata_.modelVersion != kPolicyModelVersion) {
        useHeuristicAiPlanner("policy model mismatch: " + aiPolicyMetadata_.modelVersion);
        return;
    }
    if (lowerCopy(aiPolicyMetadata_.difficulty) != lowerCopy(expectedDifficulty)) {
        useHeuristicAiPlanner("policy difficulty mismatch: " + aiPolicyMetadata_.difficulty);
        return;
    }
    if (aiPolicyMetadata_.rulesFingerprint != rulesFingerprint()) {
        useHeuristicAiPlanner("policy rules fingerprint stale: " + aiPolicyMetadata_.rulesFingerprint);
        return;
    }
    if (aiPolicyMetadata_.stateFeatureCount != kStateFeatureCount ||
        aiPolicyMetadata_.actionFeatureCount != kActionFeatureCount) {
        useHeuristicAiPlanner("policy feature schema mismatch");
        return;
    }
    if (static_cast<int>(weights.size()) != expectedWeights) {
        useHeuristicAiPlanner("policy weight count mismatch");
        return;
    }

    aiPolicyMetadata_.valid = true;
    aiPolicyMetadata_.status = "loaded policy: " + path;
    aiPlanner_ = std::make_unique<PolicyAiPlanner>(aiPolicyMetadata_, std::move(weights));
    pushEvent({EventType::AiPolicyStatus, PlayerId::Two, kInvalidUnitId, kInvalidUnitId,
               {}, {}, 0, "AI policy " + toString(config_.aiDifficulty) + " loaded"});
}

void GameEngine::useScriptedNormalAiPlanner() {
    aiPolicyMetadata_.loaded = false;
    aiPolicyMetadata_.valid = true;
    aiPolicyMetadata_.path = "built-in";
    aiPolicyMetadata_.status = "built-in normal strategy";
    aiPlanner_ = std::make_unique<ScriptedNormalAiPlanner>();
    pushEvent({EventType::AiPolicyStatus, PlayerId::Two, kInvalidUnitId, kInvalidUnitId,
               {}, {}, 0, "AI Normal uses built-in strategy"});
}

void GameEngine::useHeuristicAiPlanner(const std::string& status) {
    aiPolicyMetadata_.valid = false;
    aiPolicyMetadata_.status = status;
    aiPlanner_ = std::make_unique<HeuristicAiPlanner>();
    pushEvent({EventType::AiPolicyStatus, PlayerId::Two, kInvalidUnitId, kInvalidUnitId,
               {}, {}, 0, "AI policy fallback: " + status});
}

void GameEngine::aiPrepare(PlayerId playerId) {
    ensureAiPlanner();
    std::vector<UnitId> movedThisPreparation;
    for (int step = 0; step < kMaxAiActionsPerPreparation && phase_ == Phase::Preparation; ++step) {
        std::vector<AiAction> actions = legalActions(playerId);
        actions.erase(std::remove_if(actions.begin(), actions.end(), [&](const AiAction& action) {
            if (action.kind == AiActionKind::ReturnToBench) return true;
            if (action.kind != AiActionKind::MoveDeployed) return false;
            return std::find(movedThisPreparation.begin(), movedThisPreparation.end(), action.unitId) !=
                   movedThisPreparation.end();
        }), actions.end());
        if (actions.empty()) break;
        std::optional<AiAction> action = aiPlanner_->chooseAction(*this, playerId, actions);
        if (!action) break;
        AiActionKind kind = action->kind;
        if (!applyAiAction(playerId, *action)) break;
        if (kind == AiActionKind::MoveDeployed) movedThisPreparation.push_back(action->unitId);
        if (kind == AiActionKind::Ready) return;
    }
    if (phase_ == Phase::Preparation && hasActiveCombatUnit(playerId)) {
        setReady(playerId, true);
    }
}

std::optional<AiAction> GameEngine::chooseScriptedNormalAction(
    PlayerId playerId,
    const std::vector<AiAction>& legalActions) const {
    if (legalActions.empty()) return std::nullopt;

    const PlayerState& self = player(playerId);
    const PlayerState& foe = player(opponent(playerId));

    auto findAction = [&](AiActionKind kind, UnitType type) -> const AiAction* {
        for (const AiAction& action : legalActions) {
            if (action.kind == kind && action.type == type) return &action;
        }
        return nullptr;
    };

    auto countCombatIds = [&](const std::vector<UnitId>& ids) {
        int count = 0;
        for (UnitId id : ids) {
            if (id < 0 || id >= static_cast<int>(units_.size())) continue;
            const Unit& u = unit(id);
            if (u.alive && u.owner == playerId && !isInternalUnit(u.spec.type)) ++count;
        }
        return count;
    };

    const int rosterCountNow = countCombatIds(self.bench) + countCombatIds(self.deployed);

    auto countOwned = [&](UnitType type) {
        int count = 0;
        auto countIds = [&](const std::vector<UnitId>& ids) {
            for (UnitId id : ids) {
                if (id < 0 || id >= static_cast<int>(units_.size())) continue;
                const Unit& u = unit(id);
                if (u.alive && u.owner == playerId && u.spec.type == type) ++count;
            }
        };
        countIds(self.bench);
        countIds(self.deployed);
        return count;
    };

    int enemyAir = 0;
    int enemySupport = 0;
    int enemyTanks = 0;
    int enemyMelee = 0;
    int enemyBackline = 0;
    for (UnitId id : foe.deployed) {
        if (id < 0 || id >= static_cast<int>(units_.size())) continue;
        const Unit& enemy = unit(id);
        if (!enemy.alive || isInternalUnitType(enemy.spec.type)) {
            continue;
        }
        if (enemy.spec.layer == UnitLayer::Air) ++enemyAir;
        if (enemy.spec.roleMask & kRoleSupport) ++enemySupport;
        if (enemy.spec.roleMask & kRoleTank) ++enemyTanks;
        if (enemy.spec.roleMask & kRoleMelee) ++enemyMelee;
        if (enemy.spec.roleMask & (kRoleSupport | kRoleRanged | kRoleAoe)) ++enemyBackline;
    }
    for (UnitType recent : foe.recentBuys) {
        const UnitSpec* spec = specFor(recent);
        if (!spec) continue;
        if (spec->layer == UnitLayer::Air) ++enemyAir;
        if (spec->roleMask & kRoleSupport) ++enemySupport;
        if (spec->roleMask & kRoleTank) ++enemyTanks;
        if (spec->roleMask & kRoleMelee) ++enemyMelee;
        if (spec->roleMask & (kRoleSupport | kRoleRanged | kRoleAoe)) ++enemyBackline;
    }

    auto targetCount = [&](UnitType type) {
        int roundBand = explorationRound_ + 1;

        if (round_ <= 1) {
            switch (type) {
                case UnitType::ShieldGuardian:
                case UnitType::Ranger:
                case UnitType::Cleric:
                    return 1;
                case UnitType::ImpSwarm:
                    return enemyAir > 0 ? 1 : 0;
                default:
                    return 0;
            }
        }
        switch (type) {
            case UnitType::ShieldGuardian:
            case UnitType::Ranger:
            case UnitType::Cleric:
                return 1;
            case UnitType::Skeleton:
                return 1;
            case UnitType::ImpSwarm:
                return enemyAir > 0 || roundBand >= 2 ? 1 : 0;
            case UnitType::RogueAssassin:
                return enemySupport > 0 || enemyBackline > 1 || roundBand >= 3 ? 1 : 0;
            case UnitType::Paladin:
            case UnitType::Evoker:
            case UnitType::Druid:
                return roundBand >= 2 ? 1 : 0;
            case UnitType::DragonWyrmling:
            case UnitType::Barbarian:
                return roundBand >= 3 ? 1 : 0;
            case UnitType::Necromancer:
            case UnitType::FireMephit:
                return roundBand >= 4 ? 1 : 0;
            case UnitType::GithyankiWarrior:
            case UnitType::GoblinSkirmisher:
                return roundBand >= 2 ? 1 : 0;
            default:
                return 0;
        }
    };

    std::vector<UnitType> buyPlan;
    if (round_ <= 1) {
        buyPlan.insert(buyPlan.end(), {
            UnitType::ShieldGuardian,
            UnitType::Ranger,
            UnitType::Cleric
        });
    }
    if (enemyAir > 0) {
        buyPlan.push_back(UnitType::Ranger);
        buyPlan.push_back(UnitType::ImpSwarm);
        buyPlan.push_back(UnitType::DragonWyrmling);
    }
    if (enemySupport > 0) buyPlan.push_back(UnitType::RogueAssassin);
    if (enemyTanks > 0) {
        buyPlan.push_back(UnitType::Evoker);
        buyPlan.push_back(UnitType::Paladin);
    }
    if (enemyMelee >= 2) {
        buyPlan.push_back(UnitType::ShieldGuardian);
        buyPlan.push_back(UnitType::Cleric);
    }

    const std::vector<UnitType> defaultPlan = {
        UnitType::ShieldGuardian,
        UnitType::Ranger,
        UnitType::Cleric,
        UnitType::Paladin,
        UnitType::Skeleton,
        UnitType::Evoker,
        UnitType::Druid,
        UnitType::ImpSwarm,
        UnitType::DragonWyrmling,
        UnitType::Barbarian,
        UnitType::RogueAssassin,
        UnitType::Necromancer,
        UnitType::FireMephit,
        UnitType::GithyankiWarrior,
        UnitType::GoblinSkirmisher
    };
    buyPlan.insert(buyPlan.end(), defaultPlan.begin(), defaultPlan.end());

    for (UnitType type : buyPlan) {
        if (countOwned(type) >= targetCount(type)) continue;
        if (const AiAction* buy = findAction(AiActionKind::Buy, type)) return *buy;
    }

    const AiAction* bestDeploy = nullptr;
    int bestDeployScore = std::numeric_limits<int>::min();
    auto deploymentScore = [&](const Unit& u, Coord coord) {
        int forward = playerId == PlayerId::One ? coord.x : (board_.width - 1 - coord.x);
        int desiredForward = 1;
        int desiredY = board_.height / 2;

        if (u.spec.roleMask & kRoleTank) desiredForward = 2;
        if (u.spec.roleMask & kRoleMelee) desiredForward = std::max(desiredForward, 2);
        if (u.spec.roleMask & kRoleRanged) desiredForward = 0;
        if (u.spec.roleMask & kRoleSupport) desiredForward = 0;
        if (u.spec.layer == UnitLayer::Air) desiredForward = 1;

        switch (u.spec.type) {
            case UnitType::ShieldGuardian:
            case UnitType::Barbarian:
            case UnitType::Paladin:
            case UnitType::GithyankiWarrior:
            case UnitType::Skeleton:
                desiredY = 3;
                break;
            case UnitType::GoblinSkirmisher:
                desiredY = 4;
                break;
            case UnitType::Ranger:
                desiredY = 2;
                break;
            case UnitType::Cleric:
            case UnitType::Druid:
            case UnitType::Necromancer:
                desiredY = 3;
                break;
            case UnitType::Evoker:
                desiredY = 4;
                break;
            case UnitType::RogueAssassin:
                desiredForward = 2;
                desiredY = coord.y < board_.height / 2 ? 0 : board_.height - 1;
                break;
            case UnitType::FireMephit:
            case UnitType::ImpSwarm:
            case UnitType::DragonWyrmling:
                desiredY = board_.height / 2;
                break;
            default:
                break;
        }

        int score = 200;
        score -= std::abs(forward - desiredForward) * 45;
        score -= std::abs(coord.y - desiredY) * 10;
        score += u.spec.threat / 2;
        score += aiDeploymentScore(playerId, u, coord);
        if (isEdgeWingExplorationStagingCoord(playerId, coord)) {
            bool prefersWing = u.spec.type == UnitType::RogueAssassin ||
                               u.spec.type == UnitType::GoblinSkirmisher ||
                               u.spec.layer == UnitLayer::Air;
            score += prefersWing ? 42 : -18;
        }
        if (rosterCountNow <= 4) {
            if (coord.y <= 1 || coord.y >= board_.height - 2) score += 10;
            if (std::abs(coord.y - board_.height / 2) <= 1) score += 8;
        }
        if (u.spec.type == UnitType::RogueAssassin &&
            (coord.y == 0 || coord.y == board_.height - 1)) {
            score += 20;
        }
        return score;
    };

    for (const AiAction& action : legalActions) {
        if ((action.kind != AiActionKind::Deploy && action.kind != AiActionKind::MoveDeployed) ||
            action.unitId < 0 || action.unitId >= static_cast<int>(units_.size())) {
            continue;
        }
        const Unit& u = unit(action.unitId);
        int score = deploymentScore(u, action.coord);
        if (action.kind == AiActionKind::MoveDeployed) {
            score -= 80;
            int currentScore = deploymentScore(u, u.coord);
            if (score <= currentScore + 18) continue;
        }
        if (!bestDeploy || score > bestDeployScore) {
            bestDeploy = &action;
            bestDeployScore = score;
        }
    }
    if (bestDeploy) return *bestDeploy;

    for (const AiAction& action : legalActions) {
        if (action.kind == AiActionKind::Ready) return action;
    }

    return chooseHeuristicAction(playerId, legalActions);
}

std::optional<AiAction> GameEngine::chooseHeuristicAction(
    PlayerId playerId,
    const std::vector<AiAction>& legalActions) const {
    if (legalActions.empty()) return std::nullopt;

    const AiAction* bestBuy = nullptr;
    const AiAction* bestDeploy = nullptr;
    const AiAction* bestUpgrade = nullptr;
    const AiAction* ready = nullptr;
    double bestBuyScore = -std::numeric_limits<double>::infinity();
    double bestDeployScore = -std::numeric_limits<double>::infinity();
    double bestUpgradeScore = -std::numeric_limits<double>::infinity();

    for (const AiAction& action : legalActions) {
        double score = heuristicActionScore(playerId, action);
        switch (action.kind) {
            case AiActionKind::Buy:
                if (!bestBuy || score > bestBuyScore) {
                    bestBuy = &action;
                    bestBuyScore = score;
                }
                break;
            case AiActionKind::Deploy:
                if (!bestDeploy || score > bestDeployScore) {
                    bestDeploy = &action;
                    bestDeployScore = score;
                }
                break;
            case AiActionKind::Upgrade:
                if (!bestUpgrade || score > bestUpgradeScore) {
                    bestUpgrade = &action;
                    bestUpgradeScore = score;
                }
                break;
            case AiActionKind::Ready:
                ready = &action;
                break;
            default:
                break;
        }
    }

    if (bestBuy) return *bestBuy;
    if (bestDeploy) return *bestDeploy;
    if (bestUpgrade && bestUpgradeScore > 8.0) return *bestUpgrade;
    if (ready) return *ready;
    if (bestUpgrade && !ready) return *bestUpgrade;
    return legalActions.front();
}

double GameEngine::heuristicActionScore(PlayerId playerId, const AiAction& action) const {
    switch (action.kind) {
        case AiActionKind::Buy: {
            const UnitSpec* spec = specFor(action.type);
            return spec ? aiPurchaseScore(playerId, *spec) : -1000.0;
        }
        case AiActionKind::Deploy:
        case AiActionKind::MoveDeployed: {
            if (action.unitId < 0 || action.unitId >= static_cast<int>(units_.size())) return -1000.0;
            const Unit& u = unit(action.unitId);
            double score = aiDeploymentScore(playerId, u, action.coord) + u.spec.threat * 0.5;
            if (action.kind == AiActionKind::MoveDeployed) score -= 120.0;
            return score;
        }
        case AiActionKind::Upgrade: {
            if (action.unitId < 0 || action.unitId >= static_cast<int>(units_.size())) return -1000.0;
            const Unit& u = unit(action.unitId);
            const PlayerState& self = player(playerId);
            int combatUnits = 0;
            auto countCombatUnits = [&](const std::vector<UnitId>& ids) {
                for (UnitId id : ids) {
                    if (id < 0 || id >= static_cast<int>(units_.size())) continue;
                    const Unit& owned = unit(id);
                    if (owned.alive && !isInternalUnit(owned.spec.type)) ++combatUnits;
                }
            };
            countCombatUnits(self.bench);
            countCombatUnits(self.deployed);

            int upgradeCost = effectiveUpgradeCost(playerId, u.spec);
            double score = u.spec.threat * 0.32 + u.spec.attack * 0.20 + u.spec.maxHp * 0.015 +
                           u.spec.attackBonus * 1.2 + u.spec.armorClass * 0.8;
            if (u.spec.ability == AbilityKind::BarbarianHeavySwing) score += u.spec.attack * 0.05;
            if (u.spec.roleMask & kRoleTank) score += 4.0;
            if (u.spec.roleMask & kRoleAoe) score += 5.0;
            if (u.spec.roleMask & kRoleSupport) score += 4.0;

            score -= upgradeCost * 2.4;
            int desiredCombatUnits = round_ <= 1 ? 4 : 5;
            if (combatUnits < desiredCombatUnits) {
                score -= (desiredCombatUnits - combatUnits) * 18.0;
            }

            int surplusAfterUpgrade = self.money - upgradeCost;
            if (surplusAfterUpgrade > 0) {
                score += std::min(24.0, surplusAfterUpgrade * 0.8);
            } else {
                score -= 8.0;
            }
            if (round_ <= 1 && u.spec.cost >= 18) score -= 30.0;
            return score;
        }
        case AiActionKind::ReturnToBench:
            return -500.0;
        case AiActionKind::Ready:
            return hasActiveCombatUnit(playerId) ? 5.0 : -1000.0;
    }
    return -1000.0;
}

double GameEngine::aiPurchaseScore(PlayerId playerId, const UnitSpec& spec) const {
    const PlayerState& self = player(playerId);
    const PlayerState& foe = player(opponent(playerId));
    const RunModifiers& modifiers = runModifiers_[playerIndex(playerId)];
    double attackScore = spec.attack * 0.8;
    if (spec.ability == AbilityKind::BarbarianHeavySwing) attackScore *= 1.08;
    if (spec.ability == AbilityKind::DragonBreath || spec.ability == AbilityKind::FrostNova ||
        spec.ability == AbilityKind::MephitDeathBurst || spec.ability == AbilityKind::EvokerMagicMissile) {
        attackScore += spec.attack * 0.25;
    }
    double score = spec.threat + attackScore + spec.range * 4.0 + spec.speed * 2.0 +
                   spec.attackBonus * 1.4 + spec.armorClass * 0.9 +
                   std::max(0, spec.spellSaveDc - 10) * 1.1 -
                   effectiveBuyCost(playerId, spec) * 2.8;
    score += modifiers.familyBias[static_cast<size_t>(familyIndexForUnitType(spec.type))] * 5.5;

    int enemyAir = 0;
    int enemyHealers = 0;
    int enemyDense = 0;
    for (UnitId id : foe.deployed) {
        if (id < 0 || id >= static_cast<int>(units_.size())) continue;
        const Unit& u = unit(id);
        if (!u.alive) continue;
        if (u.spec.layer == UnitLayer::Air) ++enemyAir;
        if (u.spec.ability == AbilityKind::ClericHeal) ++enemyHealers;
        if (u.coord.x >= 3 && u.coord.x <= 7) ++enemyDense;
    }

    if (enemyAir > 0 && spec.canAttackAir) score += 25 + enemyAir * 8;
    if (enemyHealers > 0 && (spec.roleMask & kRoleAssassin)) score += 35;
    if (enemyDense >= 3 && (spec.roleMask & kRoleAoe)) score += 30;
    if (spec.roleMask & kRoleTank) score += 8;
    if (spec.roleMask & kRoleSupport) score += 5;
    if (spec.roleMask & kRoleControl) score += 8;
    if (spec.roleMask & kRoleSummoner) score += 6;
    if (spec.roleMask & kRoleAoe) score += 5;
    if (spec.layer == UnitLayer::Air) score += 4;

    int ownedSame = 0;
    int friendlyCombat = 0;
    bool hasTank = false;
    bool hasRanged = false;
    bool hasSupport = false;
    bool hasAirAnswer = false;
    auto inspectOwned = [&](const std::vector<UnitId>& ids) {
        for (UnitId id : ids) {
            if (id < 0 || id >= static_cast<int>(units_.size())) continue;
            const Unit& u = unit(id);
            if (!u.alive || isInternalUnit(u.spec.type)) continue;
            ++friendlyCombat;
            if (u.spec.type == spec.type) ++ownedSame;
            hasTank = hasTank || (u.spec.roleMask & kRoleTank);
            hasRanged = hasRanged || (u.spec.roleMask & kRoleRanged);
            hasSupport = hasSupport || (u.spec.roleMask & kRoleSupport);
            hasAirAnswer = hasAirAnswer || u.spec.canAttackAir;
        }
    };
    inspectOwned(self.bench);
    inspectOwned(self.deployed);

    score -= ownedSame * 28.0;
    if (!hasTank && (spec.roleMask & kRoleTank)) score += 15.0;
    if (!hasRanged && (spec.roleMask & kRoleRanged)) score += 10.0;
    if (!hasSupport && friendlyCombat >= 2 && (spec.roleMask & kRoleSupport)) score += 12.0;
    if (!hasAirAnswer && spec.canAttackAir) score += 6.0;
    if (hasTank && (spec.roleMask & kRoleSupport)) score += 6.0;
    if (hasSupport && (spec.roleMask & kRoleTank)) score += 5.0;
    if (friendlyCombat >= 5 && spec.cost <= 6) score -= 10.0;

    int effectiveCost = effectiveBuyCost(playerId, spec);
    if (round_ <= 1 && effectiveCost >= 18) score -= 12.0;
    if (round_ <= 1 && self.money - effectiveCost <= 5) score -= 8.0;
    if (config_.aiDifficulty == AiDifficulty::Hard && round_ <= 1) {
        if (effectiveCost >= 18) score -= 25.0;
        if (effectiveCost >= 14) score -= 10.0;
        if (spec.roleMask & kRoleAssassin) score -= 25.0;
    }

    for (UnitType recent : foe.recentBuys) {
        const UnitSpec* recentSpec = specFor(recent);
        if (!recentSpec) continue;
        if (recentSpec->layer == UnitLayer::Air && spec.canAttackAir) score += 12;
        if ((recentSpec->roleMask & kRoleTank) && (spec.roleMask & kRoleAoe)) score += 6;
        if ((recentSpec->roleMask & kRoleSupport) && (spec.roleMask & kRoleAssassin)) score += 12;
    }

    std::uniform_int_distribution<int> jitter(-4, 4);
    return score + jitter(rng_);
}

int GameEngine::aiDeploymentScore(PlayerId playerId, const Unit& unit, Coord coord) const {
    int forward = playerId == PlayerId::One ? coord.x : (board_.width - 1 - coord.x);
    int centerPenalty = std::abs(coord.y - board_.height / 2) * 3;
    int score = -centerPenalty;

    if (unit.spec.roleMask & kRoleTank) score += forward * 15;
    if (unit.spec.roleMask & kRoleMelee) score += forward * 9;
    if (unit.spec.roleMask & kRoleRanged) score += (3 - forward) * 10;
    if (unit.spec.roleMask & kRoleSupport) score += (2 - forward) * 12;
    if (unit.spec.roleMask & kRoleAssassin) score += std::abs(coord.y - 3) * 8 + forward * 3;
    if (unit.spec.layer == UnitLayer::Air) score += 5 - centerPenalty;

    int bestTargetScore = std::numeric_limits<int>::min();
    for (const Unit& target : units_) {
        if (!target.alive || !target.deployed || !canAttack(unit, target)) continue;
        if (board_.blocked(coord) || board_.blocked(target.coord)) continue;
        int targetScore = target.spec.threat - manhattan(coord, target.coord) * 2;
        if (isNeutralMonsterType(target.spec.type)) targetScore += 45;
        bestTargetScore = std::max(bestTargetScore, targetScore);
    }
    if (bestTargetScore == std::numeric_limits<int>::min()) {
        if (std::abs(coord.y - board_.height / 2) > 2) score -= 160;
    } else {
        score += std::min(150, bestTargetScore);
    }

    return score;
}

void GameEngine::tickStatuses(Unit& u, double dt) {
    for (StatusEffect& status : u.statuses) status.remaining -= dt;
    u.statuses.erase(std::remove_if(u.statuses.begin(), u.statuses.end(), [](const StatusEffect& status) {
        return status.remaining <= 0.0;
    }), u.statuses.end());
    if (u.lifespan > 0.0) u.lifespan -= dt;
}

void GameEngine::tickAbilities(UnitId id, double dt) {
    Unit& u = unit(id);
    if (!u.alive || !u.deployed) return;
    if (u.spec.abilityCooldown <= 0.0) return;
    if (!canNeutralAct(u)) return;

    u.abilityTimer += dt;
    if (u.abilityTimer < u.spec.abilityCooldown) return;

    switch (u.spec.ability) {
        case AbilityKind::GuardianShield:
            addShield(id, u.spec.abilityValue, id);
            u.abilityTimer = 0.0;
            break;
        case AbilityKind::ClericHeal: {
            UnitId target = selectHealTarget(u);
            if (target != kInvalidUnitId) {
                applyHeal(target, u.spec.abilityValue, id);
                u.abilityTimer = 0.0;
            }
            break;
        }
        case AbilityKind::NecromancerSummon:
        case AbilityKind::DruidSummon: {
            if (!canSummonMore(id)) break;
            PlayerId owner = u.owner;
            Coord origin = u.coord;
            std::string summonerName = u.spec.name;
            AbilityKind ability = u.spec.ability;
            std::optional<Coord> spot = findSummonCell(owner, origin, UnitLayer::Land);
            if (spot) {
                UnitType summonType = ability == AbilityKind::NecromancerSummon
                                          ? UnitType::SkeletonByNecromancer
                                          : UnitType::Treant;
                UnitId summon = createUnit(owner, summonType);
                Unit& summoned = unit(summon);
                summoned.deployed = true;
                summoned.lifespan = summonType == UnitType::Treant ? 8.0 : kNecromancerSkeletonLifespan;
                for (StatusEffect& status : summoned.statuses) {
                    if (status.kind == StatusKind::Summoned) {
                        status.source = id;
                        status.remaining = summoned.lifespan;
                    }
                }
                if (!placeUnit(summon, *spot)) {
                    summoned.alive = false;
                    break;
                }
                summoned.homeCoord = *spot;
                player(owner).deployed.push_back(summon);
                pushEvent({EventType::Deployed, owner, summon, id, origin, *spot, 0,
                           summonerName + " summoned " + summoned.spec.name});
            }
            if (id >= 0 && id < static_cast<int>(units_.size())) unit(id).abilityTimer = 0.0;
            break;
        }
        case AbilityKind::AnimatingSpores: {
            if (!canSummonMore(id)) break;
            PlayerId owner = u.owner;
            Coord origin = u.coord;
            double lifespan = u.spec.abilityDuration > 0.0 ? u.spec.abilityDuration : 12.0;
            std::string casterName = eventUnitName(u);
            Coord guardOrigin = neutralLeashOrigin(u);
            std::optional<size_t> corpseIndex = selectCorpseForSpores(u, guardOrigin);
            if (!corpseIndex) break;

            CorpseState& corpse = corpses_[*corpseIndex];
            std::optional<Coord> spot = selectSporeSpawnCell(corpse.coord, guardOrigin);
            if (!spot) break;

            corpse.consumed = true;
            UnitType oldType = corpse.type;
            UnitId summon = createUnit(owner, UnitType::SporeServant);
            Unit& servant = unit(summon);
            servant.deployed = true;
            servant.lifespan = lifespan;
            servant.neutralBehavior = NeutralBehavior::PassiveGuardian;
            servant.neutralProvoked = true;
            servant.provokedBy = kInvalidUnitId;
            for (StatusEffect& status : servant.statuses) {
                if (status.kind == StatusKind::Summoned) {
                    status.source = id;
                    status.remaining = lifespan;
                }
            }
            if (!placeUnit(summon, *spot)) {
                servant.alive = false;
                corpse.consumed = false;
                break;
            }
            servant.homeCoord = guardOrigin;
            servant.lastCoord = *spot;
            servant.target = kInvalidUnitId;
            servant.retargetTimer = 0.0;
            player(owner).deployed.push_back(summon);
            pushEvent({EventType::Deployed, owner, summon, id, origin, *spot, 0,
                       casterName + " used Animating Spores on a " + toString(oldType) +
                           " corpse"});
            if (id >= 0 && id < static_cast<int>(units_.size())) unit(id).abilityTimer = 0.0;
            break;
        }
        default:
            break;
    }
}

void GameEngine::tickCombat(double dt) {
    combatTime_ += dt;

    std::vector<UnitId> ids;
    for (const PlayerState& p : players_) {
        for (UnitId id : p.deployed) {
            if (id >= 0 && id < static_cast<int>(units_.size()) && unit(id).alive) ids.push_back(id);
        }
    }

    for (UnitId id : ids) {
        if (!unit(id).alive) continue;
        double lifespanBefore = unit(id).lifespan;
        tickStatuses(unit(id), dt);
        if (lifespanBefore > 0.0 && unit(id).lifespan <= 0.0) killUnit(id, kInvalidUnitId);
    }

    for (UnitId id : ids) {
        if (!unit(id).alive) continue;
        triggerTamiaDominate(id);
    }

    for (UnitId id : ids) {
        if (!unit(id).alive) continue;
        tickAbilities(id, dt);
    }

    for (UnitId id : ids) {
        if (!unit(id).alive) continue;
        Unit& u = unit(id);
        if (!canNeutralAct(u)) continue;
        if (hasStatus(u, StatusKind::Stunned) || hasStatus(u, StatusKind::Prone)) continue;
        u.attackTimer += dt;
        u.retargetTimer -= dt;
        refreshTarget(u, shouldForceRetarget(u));

        if (u.target != kInvalidUnitId && u.attackTimer >= effectiveAttackCooldown(u) &&
            inAttackRange(u, unit(u.target))) {
            attack(id, u.target);
            if (id < static_cast<int>(units_.size()) && unit(id).alive) unit(id).attackTimer = 0.0;
        }
    }

    moveUnits(dt);
    clearDeadUnits();
    if (phase_ == Phase::Combat && shouldFinishExplorationRun()) {
        std::string completion = explorationCompletionReason();
        if (!completion.empty()) {
            pushEvent({EventType::RoundStarted, PlayerId::One, kInvalidUnitId, kInvalidUnitId,
                       {}, {}, explorationRound_, completion});
        }
        finishExplorationRun();
        return;
    }
    resolveVictory();

    if (phase_ == Phase::Combat && combatTime_ >= kExplorationCombatRoundCap) {
        startNextRound("combat time cap");
    }
}

void GameEngine::refreshTarget(Unit& u, bool force) {
    if (!force && u.retargetTimer > 0.0) return;
    u.target = selectTarget(u);
    u.retargetTimer = kRetargetInterval;
}

bool GameEngine::targetAllowedByAggro(const Unit& u, const Unit& candidate) const {
    if (u.spec.type == UnitType::NeutralRedcap) {
        Coord origin = board_.inBounds(u.homeCoord) ? u.homeCoord : u.coord;
        if (manhattan(candidate.coord, origin) > kRedcapAmbushRadius) return false;
    }
    if ((isNeutralGuardianUnit(u) || isNeutralSpawServant(u)) &&
        !withinNeutralLeash(u, candidate.coord)) {
        return false;
    }
    if (isNeutralLikeCombatant(u)) return true;
    if (isRoundTransientUnit(u.spec.type)) return false;
    return manhattan(u.coord, candidate.coord) <=
           std::max(kExplorationUnitAggroRadius, u.spec.range + 3);
}

GameEngine::TargetCandidate GameEngine::evaluateTargetCandidate(const Unit& u,
                                                                const Unit& candidate) const {
    TargetCandidate result;
    result.id = candidate.id;
    if (!candidate.alive || !candidate.deployed || !canAttack(u, candidate)) return result;
    if (!targetAllowedByAggro(u, candidate)) return result;

    result.distance = manhattan(u.coord, candidate.coord);
    result.inRange = inAttackRange(u, candidate);
    result.reachable = result.inRange || u.spec.layer == UnitLayer::Air;
    result.pathCost = result.inRange ? 0 : result.distance;
    if (!result.reachable) {
        int pathRange = u.spec.ability == AbilityKind::MinotaurCharge ? 1 : u.spec.range;
        PathResult path = findPathToAttackCell(u, candidate.coord, pathRange);
        result.reachable = path.found;
        if (path.found) {
            result.pathCost = path.steps.empty() ? result.distance
                                                 : static_cast<int>(path.steps.size()) - 1;
        }
    }
    if (!result.reachable) return result;

    result.attacksMe = candidate.target == u.id && canAttack(candidate, u);
    int maxHp = std::max(1, candidate.spec.maxHp * candidate.spec.unitCount);
    int hp = totalHp(candidate);
    int missingHp = std::max(0, maxHp - hp);

    result.score = candidate.spec.threat;
    result.score -= result.distance * 10.0;
    result.score -= result.pathCost * 10.0;
    if (result.inRange) result.score += 120.0;
    if (candidate.id == u.target) result.score += 34.0;
    if (result.attacksMe) {
        result.score += 180.0;
        result.forced = true;
    }
    if (hasStatus(candidate, StatusKind::Taunt) ||
        candidate.spec.ability == AbilityKind::GuardianShield) {
        result.score += result.distance <= 3 ? 220.0 : 90.0;
        result.forced = true;
    }
    if (candidate.spec.type == UnitType::NeutralRedcap &&
        candidate.neutralBehavior == NeutralBehavior::HostileAmbusher) {
        result.score += 150.0;
        result.forced = true;
    }

    result.score += std::min(36.0, missingHp * 0.12);
    if (hp < maxHp / 2) result.score += 24.0;

    if (u.spec.roleMask & kRoleAssassin) {
        if (candidate.spec.roleMask & (kRoleSupport | kRoleRanged | kRoleAoe)) result.score += 72.0;
        int backline = candidate.owner == PlayerId::One
                           ? (board_.width - 1 - candidate.coord.x)
                           : candidate.coord.x;
        result.score += backline * 4.5;
        result.score -= result.pathCost * 1.5;
    }
    if (u.spec.roleMask & kRoleAoe) {
        result.score += clusterScoreAround(u, candidate.coord) * 4.2 + 44.0;
    }
    if (u.spec.roleMask & kRoleControl) {
        if (candidate.spec.roleMask & (kRoleAoe | kRoleSupport | kRoleRanged)) result.score += 22.0;
    }
    if (u.spec.roleMask & kRoleRanged) {
        if (result.inRange) result.score += 44.0;
        if (result.distance <= 1) result.score -= 28.0;
        result.score += std::min(18, u.spec.range) * 1.5;
    }
    if (u.spec.range <= 1 || (u.spec.roleMask & kRoleMelee)) {
        result.score += 58.0 - result.distance * 7.0 - result.pathCost * 1.5;
    }
    if ((u.spec.canAttackAir || (u.spec.roleMask & kRoleRanged)) &&
        candidate.spec.layer == UnitLayer::Air) {
        result.score += 26.0;
    }

    return result;
}

bool GameEngine::shouldKeepCurrentTarget(const Unit& u, const TargetCandidate& current,
                                         const TargetCandidate& best) const {
    if (current.id == kInvalidUnitId || !current.reachable) return false;
    if (current.forced) return true;
    if (best.id == kInvalidUnitId) return true;
    if (best.forced && best.id != current.id) return false;
    if (current.inRange && !best.forced) return current.score + 36.0 >= best.score;
    int aggro = std::max(kExplorationUnitAggroRadius, u.spec.range + 3);
    if (!isNeutralLikeCombatant(u) && current.distance > aggro) return false;
    if (current.pathCost > aggro + 5 && best.pathCost <= 2) return false;
    return current.score + 48.0 >= best.score;
}

bool GameEngine::shouldForceRetarget(const Unit& u) const {
    if (u.target == kInvalidUnitId || u.target >= static_cast<int>(units_.size())) return true;
    const Unit& target = unit(u.target);
    if (!target.alive || !target.deployed || !canAttack(u, target)) return true;
    if (!targetAllowedByAggro(u, target)) return true;
    if (isRoundTransientUnit(u.spec.type) && !isNeutralSpawServant(u)) return true;
    TargetCandidate current = evaluateTargetCandidate(u, target);
    if (!current.reachable) return true;
    if (!current.inRange && hasImmediateAttackTarget(u)) return true;
    int aggro = std::max(kExplorationUnitAggroRadius, u.spec.range + 3);
    return !isNeutralLikeCombatant(u) && current.pathCost > aggro + 5;
}

UnitId GameEngine::selectTarget(const Unit& u) const {
    if (!u.alive || !u.deployed) return kInvalidUnitId;
    if (!canNeutralAct(u)) return kInvalidUnitId;
    if (u.neutralReturningHome) return kInvalidUnitId;
    if (u.spec.ability == AbilityKind::ClericHeal) {
        if (selectGlobalHealTarget(u) != kInvalidUnitId) return kInvalidUnitId;
        if (selectFollowAlly(u) != kInvalidUnitId) {
            UnitId closeEnemy = kInvalidUnitId;
            int bestDist = std::numeric_limits<int>::max();
            for (const Unit& candidate : units_) {
                if (!candidate.alive || !candidate.deployed || !inAttackRange(u, candidate)) continue;
                if (!targetAllowedByAggro(u, candidate)) continue;
                int dist = manhattan(u.coord, candidate.coord);
                if (dist < bestDist) {
                    bestDist = dist;
                    closeEnemy = candidate.id;
                }
            }
            return closeEnemy;
        }
    }

    if (u.neutralBehavior == NeutralBehavior::HostileAmbusher &&
        u.provokedBy >= 0 && u.provokedBy < static_cast<int>(units_.size())) {
        const Unit& triggered = unit(u.provokedBy);
        TargetCandidate triggeredCandidate = evaluateTargetCandidate(u, triggered);
        if (triggeredCandidate.reachable) {
            return triggered.id;
        }
    }

    if (isNeutralGuardianUnit(u) && u.provokedBy >= 0 && u.provokedBy < static_cast<int>(units_.size())) {
        const Unit& provoker = unit(u.provokedBy);
        TargetCandidate provokerCandidate = evaluateTargetCandidate(u, provoker);
        if (provokerCandidate.reachable) {
            return provoker.id;
        }
    }

    TargetCandidate best;
    TargetCandidate current;

    for (const Unit& candidate : units_) {
        TargetCandidate evaluated = evaluateTargetCandidate(u, candidate);
        if (!evaluated.reachable) continue;
        if (candidate.id == u.target) current = evaluated;
        if (evaluated.score > best.score) best = evaluated;
    }

    if (shouldKeepCurrentTarget(u, current, best)) return current.id;
    return best.id;
}

UnitId GameEngine::selectDominatePersonTarget(const Unit& caster) const {
    if (!caster.alive || !caster.deployed || caster.spec.type != UnitType::NeutralTamiaHolzt) {
        return kInvalidUnitId;
    }
    int range = std::max(1, caster.spec.abilityRange);
    UnitId best = kInvalidUnitId;
    double bestScore = -std::numeric_limits<double>::infinity();
    for (const Unit& candidate : units_) {
        if (!candidate.alive || !candidate.deployed) continue;
        if (isNeutralLikeCombatant(candidate)) continue;
        if (!isHumanoidUnitType(candidate.spec.type)) continue;
        if (manhattan(caster.coord, candidate.coord) > range) continue;
        double score = candidate.spec.threat - manhattan(caster.coord, candidate.coord) * 9.0;
        if (candidate.hp.size() < static_cast<size_t>(candidate.spec.unitCount)) score += 16.0;
        if (totalHp(candidate) < candidate.spec.maxHp * candidate.spec.unitCount / 2) score += 12.0;
        if (candidate.spec.roleMask & (kRoleSupport | kRoleRanged | kRoleAoe)) score += 22.0;
        if (score > bestScore) {
            bestScore = score;
            best = candidate.id;
        }
    }
    return best;
}

void GameEngine::triggerTamiaDominate(UnitId id) {
    if (id < 0 || id >= static_cast<int>(units_.size())) return;
    Unit& tamia = unit(id);
    if (!tamia.alive || !tamia.deployed || tamia.spec.type != UnitType::NeutralTamiaHolzt) return;
    if (!canNeutralAct(tamia) || tamia.oneShotAbilityUsed) return;

    UnitId targetId = selectDominatePersonTarget(tamia);
    if (targetId == kInvalidUnitId) return;

    tamia.oneShotAbilityUsed = true;
    pushEvent({EventType::StatusApplied, tamia.owner, id, targetId, tamia.coord, unit(targetId).coord, 0,
               eventUnitName(tamia) + " cast Dominate Person"});

    bool saved = savingThrowSucceeds(targetId, id, "Wis");
    if (saved) return;

    Unit& target = unit(targetId);
    const std::string targetName = eventUnitName(target);
    PlayerId originalOwner = target.owner;
    eraseValue(player(originalOwner).bench, targetId);
    eraseValue(player(originalOwner).deployed, targetId);
    target.owner = tamia.owner;
    target.neutralControlled = true;
    Unit& dominated = unit(targetId);
    dominated.target = kInvalidUnitId;
    dominated.retargetTimer = 0.0;
    dominated.neutralProvoked = false;
    dominated.provokedBy = kInvalidUnitId;
    if (dominated.deployed) {
        auto& neutralRoster = player(tamia.owner).deployed;
        if (std::find(neutralRoster.begin(), neutralRoster.end(), targetId) == neutralRoster.end()) {
            neutralRoster.push_back(targetId);
        }
    }
    for (Unit& other : units_) {
        if (!other.alive || !other.deployed) continue;
        if (other.target == targetId) {
            other.target = kInvalidUnitId;
            other.retargetTimer = 0.0;
        }
    }
    pushEvent({EventType::StatusApplied, tamia.owner, id, targetId, tamia.coord, dominated.coord, 0,
               targetName + " was dominated and turned hostile to both parties"});
}

UnitId GameEngine::selectHealTarget(const Unit& healer) const {
    UnitId best = kInvalidUnitId;
    double bestRatio = 1.01;
    for (UnitId id : player(healer.owner).deployed) {
        if (id < 0 || id >= static_cast<int>(units_.size())) continue;
        const Unit& ally = unit(id);
        if (!ally.alive || !ally.deployed || isInternalUnit(ally.spec.type)) continue;
        if (manhattan(healer.coord, ally.coord) > healer.spec.abilityRange) continue;

        int maxHp = ally.spec.maxHp * ally.spec.unitCount;
        if (maxHp <= 0) continue;
        double ratio = static_cast<double>(totalHp(ally)) / maxHp;
        if (ratio < bestRatio && ratio < 1.0) {
            bestRatio = ratio;
            best = id;
        }
    }
    return best;
}

UnitId GameEngine::selectGlobalHealTarget(const Unit& healer) const {
    UnitId best = kInvalidUnitId;
    double bestScore = 0.0;
    for (UnitId id : player(healer.owner).deployed) {
        if (id < 0 || id >= static_cast<int>(units_.size())) continue;
        const Unit& ally = unit(id);
        if (!ally.alive || !ally.deployed || isInternalUnit(ally.spec.type)) continue;

        int maxHp = ally.spec.maxHp * ally.spec.unitCount;
        if (maxHp <= 0) continue;
        int missing = maxHp - totalHp(ally);
        if (missing <= 0) continue;

        double missingRatio = static_cast<double>(missing) / maxHp;
        double score = missingRatio * 100.0 + missing * 0.2 - manhattan(healer.coord, ally.coord) * 0.35;
        if (ally.id == healer.id) score -= 15.0;
        if (score > bestScore) {
            bestScore = score;
            best = id;
        }
    }
    return best;
}

UnitId GameEngine::selectFollowAlly(const Unit& u) const {
    UnitId best = kInvalidUnitId;
    double bestScore = -std::numeric_limits<double>::infinity();
    for (UnitId id : player(u.owner).deployed) {
        if (id < 0 || id >= static_cast<int>(units_.size()) || id == u.id) continue;
        const Unit& ally = unit(id);
        if (!ally.alive || !ally.deployed || isInternalUnit(ally.spec.type)) continue;
        double score = ally.spec.threat + totalHp(ally) * 0.12 - manhattan(u.coord, ally.coord) * 2.0;
        if (ally.spec.roleMask & kRoleTank) score += 18.0;
        if (ally.spec.roleMask & kRoleMelee) score += 8.0;
        if (score > bestScore) {
            bestScore = score;
            best = id;
        }
    }
    return best;
}

std::optional<size_t> GameEngine::selectCorpseForSpores(const Unit& caster, Coord guardOrigin) const {
    size_t bestIndex = corpses_.size();
    int bestScore = std::numeric_limits<int>::min();
    int range = std::max(1, caster.spec.abilityRange);
    for (size_t i = 0; i < corpses_.size(); ++i) {
        const CorpseState& corpse = corpses_[i];
        if (corpse.consumed || corpse.round != round_) continue;
        if (!board_.inBounds(corpse.coord)) continue;
        if (manhattan(corpse.coord, guardOrigin) > kNeutralSummonGuardRadius) continue;
        int dist = manhattan(caster.coord, corpse.coord);
        if (dist > range) continue;
        int score = 100 - dist * 8;
        if (corpse.owner != caster.owner) score += 20;
        if (score > bestScore) {
            bestScore = score;
            bestIndex = i;
        }
    }
    if (bestIndex == corpses_.size()) return std::nullopt;
    return bestIndex;
}

std::optional<Coord> GameEngine::selectSporeSpawnCell(Coord corpseCoord, Coord guardOrigin) const {
    auto isLandable = [&](Coord coord) {
        if (!board_.inBounds(coord) || board_.blocked(coord)) return false;
        if (manhattan(coord, guardOrigin) > kNeutralSummonGuardRadius) return false;
        for (UnitId other : board_.occupants(coord, UnitLayer::Land)) {
            if (other < 0 || other >= static_cast<int>(units_.size())) continue;
            const Unit& u = unit(other);
            if (u.alive && u.deployed) return false;
        }
        return true;
    };

    if (isLandable(corpseCoord)) return corpseCoord;
    for (Coord coord : adjacentCells(corpseCoord)) {
        if (isLandable(coord)) return coord;
    }
    for (Coord coord : cellsInRange(corpseCoord, 2)) {
        if (isLandable(coord)) return coord;
    }
    return std::nullopt;
}

double GameEngine::clusterScoreAround(const Unit& attacker, Coord center) const {
    int radius = 1;
    int count = 0;
    int threat = 0;
    for (const Unit& enemy : units_) {
        if (!enemy.alive || !enemy.deployed || !canAttack(attacker, enemy)) continue;
        if (manhattan(center, enemy.coord) > radius) continue;
        ++count;
        threat += enemy.spec.threat;
    }
    if (count <= 1) return 0.0;
    return (count - 1) * 32.0 + threat * 0.15;
}

bool GameEngine::canAttack(const Unit& attacker, const Unit& target) const {
    if (isNeutralLikeCombatant(attacker)) {
        if (isNeutralLikeCombatant(target)) {
            return false;
        }
    }
    if (isNeutralSpawServant(attacker)) {
        if (!target.alive || attacker.id == target.id || isNeutralLikeCombatant(target) ||
            isInternalUnitType(target.spec.type)) {
            return false;
        }
    } else if (!isHostileCombatTarget(attacker, target)) {
        return false;
    }
    if (target.spec.layer == UnitLayer::Land && !attacker.spec.canAttackLand) return false;
    if (target.spec.layer == UnitLayer::Air && !attacker.spec.canAttackAir) return false;
    return true;
}

bool GameEngine::inAttackRange(const Unit& attacker, const Unit& target) const {
    if (!canAttack(attacker, target)) return false;
    if (attacker.spec.ability == AbilityKind::MinotaurCharge) {
        Coord step{0, 0};
        int dx = target.coord.x - attacker.coord.x;
        int dy = target.coord.y - attacker.coord.y;
        int distance = 0;
        if (dx != 0 && dy != 0) return false;
        if (dx != 0) {
            step.x = dx > 0 ? 1 : -1;
            distance = std::abs(dx);
        } else {
            step.y = dy > 0 ? 1 : -1;
            distance = std::abs(dy);
        }
        if (distance <= 0 || distance > std::max(1, attacker.spec.abilityRange)) return false;
        Coord current = attacker.coord;
        for (int i = 0; i < distance; ++i) {
            current = {current.x + step.x, current.y + step.y};
            if (!board_.inBounds(current) || board_.blocked(current)) return false;
        }
        return true;
    }
    return manhattan(attacker.coord, target.coord) <= attacker.spec.range;
}

double GameEngine::effectiveSpeed(const Unit& u) const {
    if (hasStatus(u, StatusKind::Stunned) || hasStatus(u, StatusKind::Prone)) return 0.0;
    double speed = u.spec.speed;
    if (hasStatus(u, StatusKind::Slow)) speed *= 0.6;
    if (hasStatus(u, StatusKind::Chilled)) speed *= 0.55;
    if (hasStatus(u, StatusKind::Frightened)) speed *= 0.45;
    return speed;
}

double GameEngine::effectiveAttackCooldown(const Unit& u) const {
    if (hasStatus(u, StatusKind::Stunned) || hasStatus(u, StatusKind::Prone)) {
        return std::numeric_limits<double>::infinity();
    }
    double cooldown = u.spec.attackCooldown;
    if (hasStatus(u, StatusKind::Slow)) cooldown /= 0.6;
    if (u.spec.ability == AbilityKind::BarbarianHeavySwing) {
        int maxHp = u.spec.maxHp * u.spec.unitCount;
        if (maxHp > 0) {
            double hpRatio = static_cast<double>(totalHp(u)) / maxHp;
            if (hpRatio <= 0.50) {
                cooldown *= 0.58;
            } else if (hpRatio <= 0.75) {
                cooldown *= 0.78;
            }
        }
    }
    return cooldown;
}

int GameEngine::effectiveArmorClass(const Unit& u) const {
    int armorClass = u.spec.armorClass;
    if (u.shield > 0) armorClass += 1;
    return std::max(1, armorClass);
}

int GameEngine::effectiveAttackBonus(const Unit& u) const {
    int attackBonus = u.spec.attackBonus;
    if (hasStatus(u, StatusKind::Poisoned)) attackBonus -= 2;
    if (hasStatus(u, StatusKind::Blinded)) attackBonus -= 4;
    if (hasStatus(u, StatusKind::Frightened)) attackBonus -= 3;
    if (hasStatus(u, StatusKind::Staggered)) attackBonus -= 3;
    return std::max(0, attackBonus);
}

bool GameEngine::hasStatus(const Unit& u, StatusKind kind) const {
    return std::any_of(u.statuses.begin(), u.statuses.end(), [kind](const StatusEffect& status) {
        return status.kind == kind && status.remaining > 0.0;
    });
}

bool GameEngine::canNeutralAct(const Unit& u) const {
    if (!isNeutralMonsterType(u.spec.type)) return true;
    if (u.neutralBehavior == NeutralBehavior::HostileAmbusher) return true;
    if (u.spec.type == UnitType::NeutralRedcap) return u.neutralProvoked;
    if (u.neutralReturningHome) return true;
    return u.neutralProvoked;
}

bool GameEngine::canActivateNeutralFrom(UnitId sourceId) const {
    if (sourceId < 0 || sourceId >= static_cast<int>(units_.size())) return false;
    const Unit& source = unit(sourceId);
    if (!source.alive) return false;
    if (isNeutralLikeCombatant(source)) return false;
    return true;
}

bool GameEngine::isNeutralGuardianUnit(const Unit& u) const {
    return isNeutralMonsterType(u.spec.type) &&
           u.neutralBehavior == NeutralBehavior::PassiveGuardian &&
           u.spec.type != UnitType::NeutralRedcap;
}

bool GameEngine::isNeutralSpawServant(const Unit& u) const {
    return u.spec.type == UnitType::SporeServant;
}

bool GameEngine::isNeutralLikeCombatant(const Unit& u) const {
    return isNeutralMonsterType(u.spec.type) || isNeutralSpawServant(u) || u.neutralControlled;
}

Coord GameEngine::neutralLeashOrigin(const Unit& u) const {
    if (u.spec.type == UnitType::SporeServant) {
        for (const StatusEffect& status : u.statuses) {
            if (status.kind != StatusKind::Summoned ||
                status.source < 0 || status.source >= static_cast<int>(units_.size())) {
                continue;
            }
            const Unit& source = unit(status.source);
            if (source.spec.type == UnitType::NeutralSovereignSpaw) {
                return board_.inBounds(source.homeCoord) ? source.homeCoord : source.coord;
            }
        }
    }
    return board_.inBounds(u.homeCoord) ? u.homeCoord : u.coord;
}

bool GameEngine::withinNeutralLeash(const Unit& u, Coord coord) const {
    Coord origin = neutralLeashOrigin(u);
    int radius = isNeutralSpawServant(u) ? kNeutralSummonGuardRadius : kNeutralGuardianLeashRadius;
    if (u.spec.type == UnitType::NeutralRedcap) radius = kRedcapAmbushRadius;
    return manhattan(coord, origin) <= radius;
}

void GameEngine::activateNeutral(UnitId neutralId, UnitId sourceId, const std::string& reason) {
    if (neutralId < 0 || neutralId >= static_cast<int>(units_.size())) return;
    Unit& neutral = unit(neutralId);
    if (!neutral.alive || !isNeutralMonsterType(neutral.spec.type)) return;
    if (neutral.neutralBehavior != NeutralBehavior::PassiveGuardian) return;
    if (!canActivateNeutralFrom(sourceId)) return;

    neutral.neutralReturningHome = false;
    if (!neutral.neutralProvoked) {
        neutral.neutralProvoked = true;
        neutral.provokedBy = sourceId;
        neutral.target = sourceId;
        neutral.retargetTimer = 0.0;
        std::string text = eventUnitName(neutral) + " awakens";
        if (!reason.empty()) text += " (" + reason + ")";
        pushEvent({EventType::StatusApplied, neutral.owner, sourceId, neutralId, {}, neutral.coord, 0,
                   std::move(text)});
    } else if (sourceId != kInvalidUnitId) {
        neutral.provokedBy = sourceId;
    }
}

void GameEngine::provokeNeutral(UnitId neutralId, UnitId sourceId) {
    activateNeutral(neutralId, sourceId, "provoked");
}

void GameEngine::convertUnitOwner(UnitId id, PlayerId newOwner) {
    if (id < 0 || id >= static_cast<int>(units_.size())) return;
    Unit& converted = unit(id);
    if (!converted.alive || converted.owner == newOwner) return;

    PlayerId oldOwner = converted.owner;
    eraseValue(player(oldOwner).bench, id);
    eraseValue(player(oldOwner).deployed, id);
    converted.owner = newOwner;
    converted.neutralProvoked = false;
    converted.neutralReturningHome = false;
    converted.provokedBy = kInvalidUnitId;
    converted.target = kInvalidUnitId;
    converted.retargetTimer = 0.0;
    converted.firstStrikeReady = false;

    if (converted.deployed) {
        auto& deployed = player(newOwner).deployed;
        if (std::find(deployed.begin(), deployed.end(), id) == deployed.end()) {
            deployed.push_back(id);
        }
    } else {
        auto& bench = player(newOwner).bench;
        if (std::find(bench.begin(), bench.end(), id) == bench.end()) {
            bench.push_back(id);
        }
    }

    for (Unit& other : units_) {
        if (!other.alive || !other.deployed) continue;
        if (other.target == id) {
            other.target = kInvalidUnitId;
            other.retargetTimer = 0.0;
        }
    }
}

int GameEngine::rollD20(int advantageScore) {
    std::uniform_int_distribution<int> die(1, 20);
    int first = die(rng_);
    if (advantageScore == 0) return first;

    int second = die(rng_);
    bool advantage = advantageScore > 0;
    return advantage ? std::max(first, second) : std::min(first, second);
}

int GameEngine::rollDice(int count, int sides) {
    if (count <= 0 || sides <= 0) return 0;
    std::uniform_int_distribution<int> die(1, sides);
    int total = 0;
    for (int i = 0; i < count; ++i) total += die(rng_);
    return total;
}

int GameEngine::rollDamageRoll(const DamageRoll& roll) {
    int total = std::max(0, roll.flatBonus);
    if (roll.diceCount > 0 && roll.diceSides > 0) {
        total += rollDice(roll.diceCount, roll.diceSides);
    }
    return total;
}

DamagePacket GameEngine::scaledDamagePacket(DamagePacket packet, int numerator, int denominator) const {
    if (denominator <= 0) denominator = 1;
    numerator = std::max(0, numerator);
    for (DamageRoll& roll : packet.rolls) {
        roll.diceCount = (roll.diceCount * numerator + denominator - 1) / denominator;
        roll.flatBonus = (roll.flatBonus * numerator + denominator - 1) / denominator;
    }
    packet.flatDamage = (packet.flatDamage * numerator + denominator - 1) / denominator;
    return packet;
}

bool GameEngine::savingThrowSucceeds(UnitId targetId, UnitId sourceId, const std::string& saveName,
                                     int advantageScore) {
    if (targetId < 0 || targetId >= static_cast<int>(units_.size())) return true;
    Unit& target = unit(targetId);
    if (!target.alive) return true;

    int dc = 10;
    std::string sourceName = "spell";
    if (sourceId >= 0 && sourceId < static_cast<int>(units_.size())) {
        const Unit& source = unit(sourceId);
        dc = source.spec.spellSaveDc;
        sourceName = eventUnitName(source);
    }

    int selectedRoll = rollD20(advantageScore);
    int bonus = target.spec.savingThrowBonus;
    int total = selectedRoll + bonus;
    bool success = selectedRoll == 20 || (selectedRoll != 1 && total >= dc);

    std::ostringstream text;
    text << eventUnitName(target) << (success ? " resisted " : " failed ")
         << saveName << " save vs DC " << dc
         << " against " << sourceName;
    pushEvent({EventType::StatusApplied, target.owner, sourceId, targetId, {}, target.coord, total,
               text.str()});
    return success;
}

bool GameEngine::knockbackUnit(UnitId targetId, Coord source, int distance, UnitId sourceId) {
    return resolveKnockbackLine(targetId, source, distance, sourceId).moved;
}

bool GameEngine::isPushableLandUnit(const Unit& u) const {
    if (!u.alive || !u.deployed || u.spec.layer != UnitLayer::Land) return false;
    switch (u.spec.type) {
        case UnitType::NeutralMindFlayer:
        case UnitType::NeutralWaterMyrmidon:
        case UnitType::NeutralPhaseSpiderMatriarch:
        case UnitType::NeutralRaphael:
        case UnitType::NeutralKethericThorm:
        case UnitType::NeutralMoonlightSliver:
        case UnitType::NeutralTamiaHolzt:
            return false;
        default:
            return true;
    }
}

bool GameEngine::isLandOccupiedByBlockingUnit(Coord coord) const {
    if (!board_.inBounds(coord) || board_.blocked(coord)) return true;
    for (UnitId id : board_.occupants(coord, UnitLayer::Land)) {
        if (id < 0 || id >= static_cast<int>(units_.size())) continue;
        const Unit& occupant = unit(id);
        if (occupant.alive && occupant.deployed) return true;
    }
    return false;
}

GameEngine::KnockbackResult GameEngine::resolveKnockbackLine(UnitId targetId,
                                                             Coord source,
                                                             int distance,
                                                             UnitId sourceId) {
    KnockbackResult result;
    if (targetId < 0 || targetId >= static_cast<int>(units_.size()) || distance <= 0) return result;
    Unit& target = unit(targetId);
    if (!isPushableLandUnit(target)) {
        result.blockedAtEnd = target.alive && target.deployed && target.spec.layer == UnitLayer::Land;
        return result;
    }

    Coord step{0, 0};
    int dx = target.coord.x - source.x;
    int dy = target.coord.y - source.y;
    if (std::abs(dx) >= std::abs(dy) && dx != 0) {
        step.x = dx > 0 ? 1 : -1;
    } else if (dy != 0) {
        step.y = dy > 0 ? 1 : -1;
    } else {
        step.x = target.owner == PlayerId::One ? -1 : 1;
    }

    const int maxChain = board_.width * board_.height;
    int appliedSteps = 0;
    for (int stepIndex = 0; stepIndex < distance; ++stepIndex) {
        std::vector<UnitId> chain;
        std::vector<Coord> starts;
        UnitId currentId = targetId;
        bool blocked = false;

        while (currentId != kInvalidUnitId && static_cast<int>(chain.size()) < maxChain) {
            const Unit& current = unit(currentId);
            if (!isPushableLandUnit(current) ||
                std::find(chain.begin(), chain.end(), currentId) != chain.end()) {
                blocked = true;
                break;
            }

            chain.push_back(currentId);
            starts.push_back(current.coord);
            Coord next{current.coord.x + step.x, current.coord.y + step.y};
            if (!board_.inBounds(next) || board_.blocked(next)) {
                blocked = true;
                break;
            }

            UnitId occupantId = kInvalidUnitId;
            for (UnitId id : board_.occupants(next, UnitLayer::Land)) {
                if (id < 0 || id >= static_cast<int>(units_.size())) continue;
                const Unit& occupant = unit(id);
                if (!occupant.alive || !occupant.deployed) continue;
                occupantId = id;
                break;
            }

            if (occupantId == kInvalidUnitId) break;
            if (!isPushableLandUnit(unit(occupantId))) {
                blocked = true;
                break;
            }
            currentId = occupantId;
        }

        if (blocked || chain.empty()) {
            result.blockedAtEnd = true;
            break;
        }

        std::vector<Coord> destinations;
        destinations.reserve(starts.size());
        for (Coord from : starts) destinations.push_back({from.x + step.x, from.y + step.y});

        for (UnitId id : chain) removeFromBoard(id);
        bool placed = true;
        for (size_t i = 0; i < chain.size(); ++i) {
            if (!placeUnit(chain[i], destinations[i])) {
                placed = false;
                for (size_t restore = 0; restore <= i; ++restore) removeFromBoard(chain[restore]);
                for (size_t restore = 0; restore < chain.size(); ++restore) placeUnit(chain[restore], starts[restore]);
                break;
            }
        }
        if (!placed) {
            result.blockedAtEnd = true;
            break;
        }

        ++appliedSteps;
        for (size_t i = 0; i < chain.size(); ++i) {
            if (starts[i] == destinations[i]) continue;
            Unit& moved = unit(chain[i]);
            moved.lastCoord = starts[i];
            moved.moveProgress = 0.0;
            moved.stuckTicks = 0;
            moved.target = kInvalidUnitId;
            moved.retargetTimer = 0.0;
            moved.neutralReturningHome = false;
            activateNeutral(chain[i], sourceId, "shoved");
            bool primary = chain[i] == targetId;
            result.moves.push_back({chain[i], starts[i], destinations[i], primary});
        }
    }

    if (appliedSteps <= 0) return result;

    std::vector<UnitId> announced;
    for (const KnockbackMove& move : result.moves) {
        if (std::find(announced.begin(), announced.end(), move.id) != announced.end()) continue;
        announced.push_back(move.id);
        const Unit& moved = unit(move.id);
        Coord finalTo = move.to;
        Coord firstFrom = move.from;
        bool primary = move.primary;
        for (const KnockbackMove& later : result.moves) {
            if (later.id != move.id) continue;
            finalTo = later.to;
            primary = primary || later.primary;
        }
        std::string sourceLabel;
        if (sourceId >= 0 && sourceId < static_cast<int>(units_.size())) {
            switch (unit(sourceId).spec.ability) {
                case AbilityKind::MinotaurCharge: sourceLabel = " by Charge"; break;
                case AbilityKind::OwlbearMultiattack: sourceLabel = " by Multiattack"; break;
                case AbilityKind::DiabolicChains: sourceLabel = " by Diabolic Chains"; break;
                case AbilityKind::KethericSmite: sourceLabel = " by Ketheric's Smite"; break;
                default: break;
            }
        }
        pushEvent({EventType::UnitMoved, moved.owner, sourceId, move.id, firstFrom, finalTo, appliedSteps,
                   eventUnitName(moved) + (primary ? " was knocked back" : " was shoved back") + sourceLabel});
    }
    result.moved = !result.moves.empty();
    return result;
}

GameEngine::KnockbackResult GameEngine::resolveRadialKnockback(Coord center,
                                                               int radius,
                                                               int distance,
                                                               UnitId sourceId) {
    KnockbackResult combined;
    if (radius <= 0 || distance <= 0) return combined;

    std::vector<UnitId> targets;
    const Unit* source = (sourceId >= 0 && sourceId < static_cast<int>(units_.size())) ? &unit(sourceId) : nullptr;
    for (const Unit& candidate : units_) {
        if (!isPushableLandUnit(candidate)) continue;
        if (candidate.coord == center || manhattan(candidate.coord, center) > radius) continue;
        if (source && !canAttack(*source, candidate)) continue;
        targets.push_back(candidate.id);
    }
    std::sort(targets.begin(), targets.end(), [this, center](UnitId a, UnitId b) {
        return manhattan(unit(a).coord, center) > manhattan(unit(b).coord, center);
    });

    for (UnitId id : targets) {
        if (id < 0 || id >= static_cast<int>(units_.size()) || !unit(id).alive || !unit(id).deployed) continue;
        KnockbackResult partial = resolveKnockbackLine(id, center, distance, sourceId);
        combined.moved = combined.moved || partial.moved;
        combined.blockedAtEnd = combined.blockedAtEnd || partial.blockedAtEnd;
        combined.moves.insert(combined.moves.end(), partial.moves.begin(), partial.moves.end());
    }
    return combined;
}

void GameEngine::addStatus(UnitId id, StatusEffect status) {
    if (id < 0 || id >= static_cast<int>(units_.size())) return;
    Unit& u = unit(id);
    switch (status.kind) {
        case StatusKind::Slow:
        case StatusKind::Stunned:
        case StatusKind::Prone:
        case StatusKind::Chilled:
        case StatusKind::Poisoned:
        case StatusKind::Blinded:
        case StatusKind::Frightened:
        case StatusKind::Staggered:
            activateNeutral(id, status.source, "afflicted");
            break;
        case StatusKind::Shield:
        case StatusKind::Taunt:
        case StatusKind::Summoned:
        case StatusKind::FirstStrike:
            break;
    }
    auto it = std::find_if(u.statuses.begin(), u.statuses.end(), [status](const StatusEffect& current) {
        return current.kind == status.kind;
    });
    if (it == u.statuses.end()) {
        u.statuses.push_back(status);
    } else {
        it->remaining = std::max(it->remaining, status.remaining);
        it->value = std::max(it->value, status.value);
        it->source = status.source;
    }
    pushEvent({EventType::StatusApplied, u.owner, status.source, id, {}, u.coord, status.value,
               eventUnitName(u) + " became " + statusName(status.kind)});
}

void GameEngine::applyWeaponStrikes(UnitId attackerId, UnitId targetId,
                                    const DamagePacket& strikeDamage, int strikeCount) {
    for (int i = 0; i < strikeCount; ++i) {
        if (targetId < 0 || targetId >= static_cast<int>(units_.size()) ||
            !unit(targetId).alive) {
            break;
        }
        applyDamage(targetId, strikeDamage, attackerId);
    }
}

bool GameEngine::resolveCounterspellReaction(UnitId attackerId, UnitId targetId) {
    Unit& attacker = unit(attackerId);
    Unit& target = unit(targetId);
    if (target.spec.type != UnitType::NeutralMindFlayer || target.counterspellUsed) return false;
    if (!isSpellLikeAbility(attacker.spec.ability) ||
        attacker.spec.ability == AbilityKind::Counterspell) {
        return false;
    }

    target.counterspellUsed = true;
    pushEvent({EventType::StatusApplied, target.owner, targetId, attackerId, target.coord,
               attacker.coord, 0,
               eventUnitName(target) + " cast Counterspell and stopped " +
                   eventUnitName(attacker)});
    return true;
}

bool GameEngine::tryResolveExtractBrain(UnitId attackerId, UnitId targetId) {
    Unit& attacker = unit(attackerId);
    if (attacker.spec.type != UnitType::NeutralMindFlayer || attacker.oneShotAbilityUsed) return false;

    UnitId extractTarget = kInvalidUnitId;
    double bestExtractScore = -std::numeric_limits<double>::infinity();
    for (const Unit& candidate : units_) {
        if (!candidate.alive || !candidate.deployed || !hasStatus(candidate, StatusKind::Stunned)) continue;
        if (!canAttack(attacker, candidate)) continue;
        if (manhattan(attacker.coord, candidate.coord) > 1) continue;
        double score = candidate.spec.threat + totalHp(candidate) * 0.35;
        if (candidate.id == targetId) score += 40.0;
        if (score > bestExtractScore) {
            bestExtractScore = score;
            extractTarget = candidate.id;
        }
    }
    if (extractTarget == kInvalidUnitId) return false;

    attacker.oneShotAbilityUsed = true;
    Coord victimCoord = unit(extractTarget).coord;
    pushEvent({EventType::UnitAttacked, attacker.owner, attackerId, extractTarget, attacker.coord,
               victimCoord, 0,
               eventUnitName(attacker) + " used Extract Brain on " +
                   eventUnitName(unit(extractTarget))});
    killUnit(extractTarget, attackerId);
    if (attackerId >= 0 && attackerId < static_cast<int>(units_.size()) && unit(attackerId).alive) {
        applyHeal(attackerId, rollDice(6, 6), attackerId);
    }
    return true;
}

void GameEngine::attack(UnitId attackerId, UnitId targetId) {
    if (attackerId < 0 || targetId < 0 || attackerId >= static_cast<int>(units_.size()) ||
        targetId >= static_cast<int>(units_.size())) {
        return;
    }
    Unit& attacker = unit(attackerId);
    Unit& target = unit(targetId);
    if (!attacker.alive || !target.alive) return;
    if (!canNeutralAct(attacker)) return;
    if (resolveCounterspellReaction(attackerId, targetId)) return;
    if (tryResolveExtractBrain(attackerId, targetId)) return;
    if (tryResolveActiveAbilityAttack(attackerId, targetId)) return;
    resolveWeaponAttack(attackerId, targetId);
}

bool GameEngine::tryResolveActiveAbilityAttack(UnitId attackerId, UnitId targetId) {
    Unit& attacker = unit(attackerId);
    Unit& target = unit(targetId);

    if (attacker.spec.ability == AbilityKind::DragonBreath) {
        pushEvent({EventType::UnitAttacked, attacker.owner, attackerId, targetId, attacker.coord,
                   target.coord, attacker.spec.attack,
                   eventUnitName(attacker) + " breathed fire through clustered enemies"});
        std::vector<UnitId> targets;
        for (const Unit& enemy : units_) {
            if (!enemy.alive || !enemy.deployed) continue;
            if (!canAttack(attacker, enemy) || manhattan(attacker.coord, enemy.coord) > attacker.spec.range) continue;
            targets.push_back(enemy.id);
        }
        for (UnitId id : targets) {
            bool saved = savingThrowSucceeds(id, attackerId, "Dex");
            applyDamage(id, saved ? std::max(1, attacker.spec.attack / 2) : attacker.spec.attack,
                        DamageType::Fire, attackerId);
        }
        return true;
    }

    if (attacker.spec.ability == AbilityKind::MephitDeathBurst) {
        pushEvent({EventType::UnitAttacked, attacker.owner, attackerId, targetId, attacker.coord,
                   target.coord, attacker.spec.attack,
                   eventUnitName(attacker) + " scattered cinders around the target"});
        std::vector<UnitId> targets;
        Coord center = target.coord;
        for (const Unit& enemy : units_) {
            if (!enemy.alive || !enemy.deployed) continue;
            if (!canAttack(attacker, enemy) || manhattan(center, enemy.coord) > 1) continue;
            targets.push_back(enemy.id);
        }
        for (UnitId id : targets) {
            bool saved = savingThrowSucceeds(id, attackerId, "Dex");
            applyDamage(id, saved ? std::max(1, attacker.spec.attack / 2) : attacker.spec.attack,
                        DamageType::Fire, attackerId);
        }
        return true;
    }

    if (attacker.spec.ability == AbilityKind::FrostNova) {
        pushEvent({EventType::UnitAttacked, attacker.owner, attackerId, targetId, attacker.coord,
                   target.coord, attacker.spec.attack,
                   eventUnitName(attacker) + " cast frost nova"});
        std::vector<UnitId> targets;
        Coord center = target.coord;
        for (const Unit& enemy : units_) {
            if (!enemy.alive || !enemy.deployed) continue;
            if (!canAttack(attacker, enemy) || manhattan(center, enemy.coord) > 1) continue;
            targets.push_back(enemy.id);
        }
        for (UnitId id : targets) {
            bool saved = savingThrowSucceeds(id, attackerId, "Con");
            applyDamage(id, saved ? std::max(1, attacker.spec.attack / 2) : attacker.spec.attack,
                        DamageType::Cold, attackerId);
            if (!saved && id < static_cast<int>(units_.size()) && unit(id).alive) {
                addStatus(id, {StatusKind::Slow, attacker.spec.abilityDuration, attacker.spec.abilityValue, attackerId});
            }
        }
        return true;
    }

    if (attacker.spec.ability == AbilityKind::SpectatorWoundingRay) {
        pushEvent({EventType::UnitAttacked, attacker.owner, attackerId, targetId, attacker.coord,
                   target.coord, attacker.spec.attack,
                   eventUnitName(attacker) + " cast Wounding Ray"});
        bool saved = savingThrowSucceeds(targetId, attackerId, "Con");
        applyDamage(targetId, saved ? std::max(1, attacker.spec.attack / 2) : attacker.spec.attack,
                    DamageType::Necrotic, attackerId);
        return true;
    }

    if (attacker.spec.ability == AbilityKind::MindBlast) {
        pushEvent({EventType::UnitAttacked, attacker.owner, attackerId, targetId, attacker.coord,
                   target.coord, attacker.spec.attack,
                   eventUnitName(attacker) + " unleashed Mind Blast"});
        std::vector<UnitId> targets;
        Coord center = target.coord;
        for (const Unit& enemy : units_) {
            if (!enemy.alive || !enemy.deployed) continue;
            if (!canAttack(attacker, enemy)) continue;
            if (manhattan(attacker.coord, enemy.coord) > attacker.spec.range) continue;
            if (enemy.id != targetId && manhattan(center, enemy.coord) > 2) continue;
            targets.push_back(enemy.id);
        }
        for (UnitId id : targets) {
            bool saved = savingThrowSucceeds(id, attackerId, "Int");
            applyDamage(id, saved ? std::max(1, attacker.spec.attack / 2) : attacker.spec.attack,
                        DamageType::Psychic, attackerId);
            if (!saved && id < static_cast<int>(units_.size()) && unit(id).alive) {
                addStatus(id, {StatusKind::Stunned,
                               std::max(0.8, attacker.spec.abilityDuration),
                               0,
                               attackerId});
            }
        }
        return true;
    }

    if (attacker.spec.ability == AbilityKind::DiabolicChains) {
        pushEvent({EventType::UnitAttacked, attacker.owner, attackerId, targetId, attacker.coord,
                   target.coord, attacker.spec.attack,
                   eventUnitName(attacker) + " lashed out with Diabolic Chains"});
        std::vector<UnitId> targets{targetId};
        for (const Unit& enemy : units_) {
            if (!enemy.alive || !enemy.deployed || enemy.id == targetId) continue;
            if (!canAttack(attacker, enemy)) continue;
            if (manhattan(attacker.coord, enemy.coord) <= attacker.spec.range &&
                manhattan(target.coord, enemy.coord) <= 2) {
                targets.push_back(enemy.id);
            }
            if (targets.size() >= 3) break;
        }
        for (UnitId id : targets) {
            bool saved = savingThrowSucceeds(id, attackerId, "Dex");
            DamagePacket chainDamage = abilityDamagePacketFor(attacker.spec, AbilityKind::DiabolicChains);
            applyDamage(id, saved ? scaledDamagePacket(chainDamage, 1, 2) : chainDamage, attackerId);
            if (!saved && id < static_cast<int>(units_.size()) && unit(id).alive) {
                addStatus(id, {StatusKind::Slow, std::max(0.8, attacker.spec.abilityDuration), 0, attackerId});
                if (unit(id).spec.layer == UnitLayer::Land) {
                    knockbackUnit(id, attacker.coord, 1, attackerId);
                }
            }
        }
        return true;
    }

    if (attacker.spec.ability == AbilityKind::SelunesIre) {
        pushEvent({EventType::UnitAttacked, attacker.owner, attackerId, targetId, attacker.coord,
                   target.coord, attacker.spec.attack + attacker.spec.abilityValue,
                   eventUnitName(attacker) + " invoked Selune's Ire"});
        bool saved = savingThrowSucceeds(targetId, attackerId, "Dex");
        int radiantDamage = attacker.spec.attack + std::max(0, attacker.spec.abilityValue);
        applyDamage(targetId, saved ? std::max(1, radiantDamage / 2) : radiantDamage,
                    DamageType::Radiant, attackerId);
        if (!saved && targetId < static_cast<int>(units_.size()) && unit(targetId).alive) {
            addStatus(targetId, {StatusKind::Blinded,
                                 std::max(0.8, attacker.spec.abilityDuration),
                                 0,
                                 attackerId});
        }
        return true;
    }

    if (attacker.spec.ability == AbilityKind::Blight) {
        int necroticDamage = isPlantLikeUnitType(target.spec.type) ? 64 : rollDice(8, 8);
        bool plantLike = isPlantLikeUnitType(target.spec.type);
        bool saved = savingThrowSucceeds(targetId, attackerId, "Con", plantLike ? -1 : 0);
        int finalDamage = saved ? std::max(1, necroticDamage / 2) : necroticDamage;
        pushEvent({EventType::UnitAttacked, attacker.owner, attackerId, targetId, attacker.coord,
                   target.coord, finalDamage,
                   eventUnitName(attacker) + " cast Blight"});
        applyDamage(targetId, finalDamage, DamageType::Necrotic, attackerId);
        return true;
    }

    if (attacker.spec.ability == AbilityKind::EvokerMagicMissile) {
        pushEvent({EventType::UnitAttacked, attacker.owner, attackerId, targetId, attacker.coord,
                   target.coord, attacker.spec.attack,
                   eventUnitName(attacker) + " cast Magic Missile barrage"});
        std::vector<UnitId> targets{targetId};
        Coord center = target.coord;
        for (const Unit& enemy : units_) {
            if (!enemy.alive || !enemy.deployed || enemy.id == targetId) continue;
            if (!canAttack(attacker, enemy)) continue;
            if (manhattan(center, enemy.coord) <= 1 && manhattan(attacker.coord, enemy.coord) <= attacker.spec.range) {
                targets.push_back(enemy.id);
            }
        }
        int missileDamage = std::max(1, attacker.spec.attack / 3);
        for (int i = 0; i < 3 && !targets.empty(); ++i) {
            UnitId id = targets[static_cast<size_t>(i) % targets.size()];
            if (id >= 0 && id < static_cast<int>(units_.size()) && unit(id).alive) {
                applyDamage(id, missileDamage, DamageType::Force, attackerId);
            }
        }
        return true;
    }

    if (attacker.spec.ability == AbilityKind::StrikeOfTheGuardian) {
        pushEvent({EventType::UnitAttacked, attacker.owner, attackerId, targetId, attacker.coord,
                   target.coord, attacker.spec.abilityValue,
                   eventUnitName(attacker) + " used Strike of the Guardian"});
        std::vector<UnitId> targets;
        Coord center = target.coord;
        for (const Unit& enemy : units_) {
            if (!enemy.alive || !enemy.deployed) continue;
            if (!canAttack(attacker, enemy)) continue;
            if (manhattan(attacker.coord, enemy.coord) > std::max(1, attacker.spec.abilityRange)) continue;
            if (manhattan(center, enemy.coord) > 1) continue;
            targets.push_back(enemy.id);
        }
        if (targets.empty()) targets.push_back(targetId);
        DamagePacket guardianStrike = abilityDamagePacketFor(attacker.spec, AbilityKind::StrikeOfTheGuardian);
        for (UnitId id : targets) {
            if (id < 0 || id >= static_cast<int>(units_.size()) || !unit(id).alive) continue;
            bool undead = isUndeadUnitType(unit(id).spec.type);
            bool saved = undead ? false : savingThrowSucceeds(id, attackerId, "Dex");
            applyDamage(id, saved ? scaledDamagePacket(guardianStrike, 1, 2) : guardianStrike, attackerId);
            if (undead && id < static_cast<int>(units_.size()) && unit(id).alive) {
                applyDamage(id,
                            DamagePacket{{DamageRoll{0, 0, 20, DamageType::Radiant}},
                                         0,
                                         "anti-undead radiance"},
                            attackerId);
            }
        }
        return true;
    }

    if (attacker.spec.ability == AbilityKind::MinotaurCharge) {
        int dx = target.coord.x - attacker.coord.x;
        int dy = target.coord.y - attacker.coord.y;
        Coord step{0, 0};
        if (std::abs(dx) >= std::abs(dy) && dx != 0) {
            step.x = dx > 0 ? 1 : -1;
        } else if (dy != 0) {
            step.y = dy > 0 ? 1 : -1;
        } else {
            step.x = attacker.owner == PlayerId::One ? 1 : -1;
        }

        std::vector<UnitId> targets;
        Coord current = attacker.coord;
        int chargeRange = std::max(1, attacker.spec.abilityRange);
        for (int i = 0; i < chargeRange; ++i) {
            current = {current.x + step.x, current.y + step.y};
            if (!board_.inBounds(current) || board_.blocked(current)) break;
            for (UnitId id : board_.occupants(current, UnitLayer::Land)) {
                if (id < 0 || id >= static_cast<int>(units_.size())) continue;
                const Unit& candidate = unit(id);
                if (!candidate.alive || !candidate.deployed || !canAttack(attacker, candidate)) continue;
                if (std::find(targets.begin(), targets.end(), id) == targets.end()) targets.push_back(id);
            }
        }
        if (std::find(targets.begin(), targets.end(), targetId) == targets.end() &&
            canAttack(attacker, target)) {
            targets.push_back(targetId);
        }

        pushEvent({EventType::UnitAttacked, attacker.owner, attackerId, targetId, attacker.coord,
                   target.coord, attacker.spec.abilityValue,
                   eventUnitName(attacker) + " used Charge"});
        DamagePacket charge = abilityDamagePacketFor(attacker.spec, AbilityKind::MinotaurCharge);
        for (UnitId id : targets) {
            if (id < 0 || id >= static_cast<int>(units_.size()) || !unit(id).alive) continue;
            bool saved = savingThrowSucceeds(id, attackerId, "Str");
            applyDamage(id, saved ? scaledDamagePacket(charge, 1, 2) : charge, attackerId);
            if (!saved && id < static_cast<int>(units_.size()) && unit(id).alive) {
                addStatus(id, {StatusKind::Prone, std::max(1.0, attacker.spec.abilityDuration), 0, attackerId});
                knockbackUnit(id, attacker.coord, 1, attackerId);
            }
        }
        return true;
    }

    return false;
}

void GameEngine::resolveWeaponAttack(UnitId attackerId, UnitId targetId) {
    Unit& attacker = unit(attackerId);
    Unit& target = unit(targetId);

    int baseStrikeCount = std::max(1, static_cast<int>(attacker.hp.size()));
    int damage = attacker.spec.attack;
    bool berserkerRage = attacker.spec.ability == AbilityKind::BarbarianHeavySwing;
    if (berserkerRage) ++attacker.specialCounter;
    bool hiemalStrike = attacker.spec.ability == AbilityKind::HiemalStrike;
    bool venomousBite = attacker.spec.ability == AbilityKind::VenomousBite;
    bool kethericSmite = attacker.spec.ability == AbilityKind::KethericSmite;
    bool staggeringSmite = attacker.spec.ability == AbilityKind::StaggeringSmite;
    bool electrifiedFlail = attacker.spec.ability == AbilityKind::ElectrifiedFlail;
    if (kethericSmite) ++attacker.specialCounter;

    int targetMaxHp = target.spec.maxHp * target.spec.unitCount;
    bool cruelSting = attacker.spec.ability == AbilityKind::KarnissCruelSting &&
                      targetMaxHp > 0 && totalHp(target) < targetMaxHp;
    bool owlbearMultiattack = attacker.spec.ability == AbilityKind::OwlbearMultiattack;
    bool frenziedStrike = berserkerRage && attacker.specialCounter % 3 == 0;
    int attackerMaxHp = attacker.spec.maxHp * attacker.spec.unitCount;
    bool bloodiedRage = berserkerRage && attackerMaxHp > 0 && totalHp(attacker) <= attackerMaxHp * 3 / 4;
    bool nearDeathRage = berserkerRage && attackerMaxHp > 0 && totalHp(attacker) <= attackerMaxHp / 2;

    int advantageScore = 0;
    if (attacker.firstStrikeReady) ++advantageScore;
    if (berserkerRage) ++advantageScore;
    if (nearDeathRage) ++advantageScore;
    if (cruelSting) ++advantageScore;
    if (hasStatus(target, StatusKind::Slow)) ++advantageScore;
    if (hasStatus(target, StatusKind::Chilled)) ++advantageScore;
    if (hasStatus(target, StatusKind::Prone) || hasStatus(target, StatusKind::Stunned)) ++advantageScore;
    if (hasStatus(attacker, StatusKind::Slow) || hasStatus(attacker, StatusKind::Chilled) ||
        hasStatus(attacker, StatusKind::Poisoned) || hasStatus(attacker, StatusKind::Blinded) ||
        hasStatus(attacker, StatusKind::Frightened)) {
        --advantageScore;
    }

    int selectedRoll = rollD20(advantageScore);
    int attackBonus = effectiveAttackBonus(attacker);
    int attackTotal = selectedRoll + attackBonus;
    int targetArmorClass = effectiveArmorClass(target);
    bool criticalHit = selectedRoll == 20;
    bool hit = criticalHit || (selectedRoll != 1 && attackTotal >= targetArmorClass);

    std::ostringstream attackText;
    (void)attackTotal;
    (void)targetArmorClass;
    attackText << eventUnitName(attacker) << (hit ? " hit " : " missed ")
               << eventUnitName(target) << " with an attack";
    if (criticalHit) attackText << " with a critical";
    if (cruelSting) attackText << " using Multiattack - Cruel Sting";
    if (owlbearMultiattack) attackText << " using Multiattack";
    if (frenziedStrike) attackText << " with Frenzied Strike";
    if (bloodiedRage) attackText << " while raging";
    if (hiemalStrike) attackText << " using Hiemal Strike";
    if (venomousBite) attackText << " using Venomous Bite";
    if (staggeringSmite) attackText << " using Staggering Smite";
    if (electrifiedFlail) attackText << " with Electrified Flail";
    if (kethericSmite) {
        attackText << (attacker.specialCounter % 2 == 0 ? " with Blinding Smite"
                                                        : " with Wrathful Smite");
    }

    pushEvent({EventType::UnitAttacked, attacker.owner, attackerId, targetId, attacker.coord,
               target.coord, hit ? damage : 0, attackText.str()});

    if (!hit) {
        if (attacker.firstStrikeReady) attacker.firstStrikeReady = false;
        return;
    }

    DamagePacket strikePacket = basicDamagePacketFor(attacker.spec);
    DamageType baseDamageType = basicDamageTypeFor(attacker.spec.type);

    if (criticalHit) {
        damage *= 2;
        strikePacket = scaledDamagePacket(strikePacket, 2);
    }

    if (attacker.spec.ability == AbilityKind::PaladinCharge && attacker.firstStrikeReady) {
        damage *= 2;
        strikePacket = scaledDamagePacket(strikePacket, 2);
        attacker.firstStrikeReady = false;
    }

    if (attacker.spec.ability == AbilityKind::GithyankiAstralRaid && attacker.firstStrikeReady) {
        damage += attacker.spec.abilityValue;
        strikePacket.rolls.push_back(DamageRoll{0, 0, attacker.spec.abilityValue, DamageType::Psychic});
        attacker.firstStrikeReady = false;
    }

    if (attacker.spec.ability == AbilityKind::RogueAmbush && attacker.firstStrikeReady) {
        damage += attacker.spec.abilityValue;
        strikePacket.rolls.push_back(DamageRoll{0, 0, attacker.spec.abilityValue, DamageType::Piercing});
        attacker.firstStrikeReady = false;
    }

    if (berserkerRage) {
        int rageBonus = nearDeathRage ? std::max(18, attacker.spec.attack / 2)
                                      : (bloodiedRage ? std::max(10, attacker.spec.attack / 4) : 0);
        damage += rageBonus;
        if (rageBonus > 0) strikePacket.rolls.push_back(DamageRoll{0, 0, rageBonus, baseDamageType});
    }

    if (hiemalStrike) {
        damage += attacker.spec.abilityValue;
        strikePacket.rolls.push_back(DamageRoll{0, 0, attacker.spec.abilityValue, DamageType::Cold});
    }

    if (venomousBite) {
        damage += attacker.spec.abilityValue;
        strikePacket.rolls.push_back(DamageRoll{0, 0, attacker.spec.abilityValue, DamageType::Poison});
    }

    bool kethericBlindingSmite = kethericSmite && attacker.specialCounter % 2 == 0;
    if (kethericSmite) {
        int smiteDamage = kethericBlindingSmite ? attacker.spec.abilityValue : attacker.spec.abilityValue * 2;
        damage += smiteDamage;
        strikePacket.rolls.push_back(DamageRoll{0, 0, smiteDamage, DamageType::Radiant});
    }

    if (staggeringSmite) {
        strikePacket = abilityDamagePacketFor(attacker.spec, AbilityKind::StaggeringSmite);
        if (criticalHit) strikePacket = scaledDamagePacket(strikePacket, 2);
    }

    if (electrifiedFlail) {
        strikePacket = abilityDamagePacketFor(attacker.spec, AbilityKind::ElectrifiedFlail);
        if (criticalHit) strikePacket = scaledDamagePacket(strikePacket, 2);
    }

    if (cruelSting) {
        applyWeaponStrikes(attackerId, targetId, scaledDamagePacket(strikePacket, 1, 2),
                           baseStrikeCount * 3);
        return;
    }

    if (owlbearMultiattack) {
        applyDamage(targetId, strikePacket, attackerId);
        if (targetId < static_cast<int>(units_.size()) && unit(targetId).alive) {
            applyDamage(targetId, DamagePacket{{damageRollForValue(std::max(1, (attacker.spec.attack * 2) / 3),
                                                                DamageType::Piercing)},
                                               0,
                                               "bite"},
                        attackerId);
            if (targetId < static_cast<int>(units_.size()) && unit(targetId).alive) {
                addStatus(targetId, {StatusKind::Prone, 1.1, 0, attackerId});
                knockbackUnit(targetId, attacker.coord, 1, attackerId);
            }
        }
        return;
    }

    applyWeaponStrikes(attackerId, targetId, strikePacket, baseStrikeCount);
    if (hiemalStrike && targetId < static_cast<int>(units_.size()) && unit(targetId).alive) {
        addStatus(targetId, {StatusKind::Chilled,
                             std::max(1.0, attacker.spec.abilityDuration),
                             attacker.spec.abilityValue,
                             attackerId});
    }
    if (venomousBite && targetId < static_cast<int>(units_.size()) && unit(targetId).alive) {
        bool saved = savingThrowSucceeds(targetId, attackerId, "Con");
        if (!saved && targetId < static_cast<int>(units_.size()) && unit(targetId).alive) {
            addStatus(targetId, {StatusKind::Poisoned,
                                 std::max(1.2, attacker.spec.abilityDuration),
                                 attacker.spec.abilityValue,
                                 attackerId});
        }
    }
    if (kethericBlindingSmite && targetId < static_cast<int>(units_.size()) && unit(targetId).alive) {
        bool saved = savingThrowSucceeds(targetId, attackerId, "Con");
        if (!saved && targetId < static_cast<int>(units_.size()) && unit(targetId).alive) {
            addStatus(targetId, {StatusKind::Blinded,
                                 std::max(1.2, attacker.spec.abilityDuration),
                                 attacker.spec.abilityValue,
                                 attackerId});
        }
    }
    if (kethericSmite && !kethericBlindingSmite) {
        resolveRadialKnockback(attacker.coord, 1, 1, attackerId);
        if (targetId < static_cast<int>(units_.size()) && unit(targetId).alive) {
            bool saved = savingThrowSucceeds(targetId, attackerId, "Wis");
            if (!saved && targetId < static_cast<int>(units_.size()) && unit(targetId).alive) {
                addStatus(targetId, {StatusKind::Frightened,
                                     std::max(1.2, attacker.spec.abilityDuration),
                                     attacker.spec.abilityValue,
                                     attackerId});
            }
        }
    }
    if (staggeringSmite && targetId < static_cast<int>(units_.size()) && unit(targetId).alive) {
        bool saved = savingThrowSucceeds(targetId, attackerId, "Wis");
        if (!saved && targetId < static_cast<int>(units_.size()) && unit(targetId).alive) {
            addStatus(targetId, {StatusKind::Staggered,
                                 std::max(1.2, attacker.spec.abilityDuration),
                                 attacker.spec.abilityValue,
                                 attackerId});
        }
    }
    if (electrifiedFlail && targetId < static_cast<int>(units_.size()) && unit(targetId).alive) {
        bool saved = savingThrowSucceeds(targetId, attackerId, "Con");
        if (!saved && targetId < static_cast<int>(units_.size()) && unit(targetId).alive) {
            addStatus(targetId, {StatusKind::Stunned,
                                 std::max(0.8, attacker.spec.abilityDuration),
                                 0,
                                 attackerId});
        }
    }
    if (frenziedStrike && targetId < static_cast<int>(units_.size()) && unit(targetId).alive) {
        applyDamage(targetId, std::max(18, damage / 2), baseDamageType, attackerId);
    }
}

void GameEngine::applyDamage(UnitId targetId, const DamagePacket& packet, UnitId sourceId) {
    if (targetId < 0 || targetId >= static_cast<int>(units_.size())) return;
    Unit& target = unit(targetId);
    if (!target.alive || target.hp.empty()) return;

    int amount = 0;
    std::vector<std::string> affinityNotes;
    std::vector<std::string> damageTypes;
    for (const DamageRoll& roll : packet.rolls) {
        int raw = rollDamageRoll(roll);
        if (raw <= 0) continue;
        DamageAffinity affinity = damageAffinity(target.spec.type, roll.type);
        int adjusted = raw;
        if (affinity == DamageAffinity::Immune) adjusted = 0;
        if (affinity == DamageAffinity::Resistant) adjusted = std::max(1, raw / 2);
        if (affinity == DamageAffinity::Vulnerable) adjusted = raw * 2;
        amount += adjusted;
        std::string typeName = toString(roll.type);
        if (std::find(damageTypes.begin(), damageTypes.end(), typeName) == damageTypes.end()) {
            damageTypes.push_back(typeName);
        }
        if (affinity != DamageAffinity::Normal) {
            std::string note = toString(affinity) + " to " + typeName;
            if (std::find(affinityNotes.begin(), affinityNotes.end(), note) == affinityNotes.end()) {
                affinityNotes.push_back(note);
            }
        }
    }
    if (packet.flatDamage > 0) {
        amount += packet.flatDamage;
        if (std::find(damageTypes.begin(), damageTypes.end(), "Force") == damageTypes.end()) {
            damageTypes.push_back("Force");
        }
    }

    if (amount <= 0) {
        std::string text = eventUnitName(target) + " took no damage";
        if (!affinityNotes.empty()) text += " (" + affinityNotes.front() + ")";
        pushEvent({EventType::DamageDealt, target.owner, sourceId, targetId, {}, target.coord, 0, text});
        return;
    }

    activateNeutral(targetId, sourceId, "struck");

    if (target.spec.ability == AbilityKind::BarbarianHeavySwing) {
        int maxHp = target.spec.maxHp * target.spec.unitCount;
        int beforeHp = totalHp(target);
        if (maxHp > 0) {
            double hpRatio = static_cast<double>(beforeHp) / maxHp;
            int reduction = hpRatio <= 0.50 ? amount / 3 : amount / 4;
            amount = std::max(1, amount - reduction);
            if (reduction > 0) {
                pushEvent({EventType::Shielded, target.owner, sourceId, targetId, {}, target.coord, reduction,
                           eventUnitName(target) + " reduced " + std::to_string(reduction) +
                               " damage with Rage"});
            }
        }
    }

    int absorbed = std::min(target.shield, amount);
    if (absorbed > 0) {
        target.shield -= absorbed;
        amount -= absorbed;
        pushEvent({EventType::Shielded, target.owner, sourceId, targetId, {}, target.coord, absorbed,
                   eventUnitName(target) + " shield absorbed damage"});
    }
    if (amount <= 0) return;

    auto hpIt = std::min_element(target.hp.begin(), target.hp.end());
    int index = static_cast<int>(std::distance(target.hp.begin(), hpIt));
    target.hp[index] -= amount;

    std::ostringstream typeText;
    for (size_t i = 0; i < damageTypes.size(); ++i) {
        if (i > 0) typeText << "/";
        typeText << damageTypes[i];
    }
    std::string text = eventUnitName(target) + " took " + std::to_string(amount) + " " +
                       (damageTypes.empty() ? std::string("damage") : typeText.str() + " damage");
    if (!affinityNotes.empty()) {
        text += " (";
        for (size_t i = 0; i < affinityNotes.size(); ++i) {
            if (i > 0) text += ", ";
            text += affinityNotes[i];
        }
        text += ")";
    }
    pushEvent({EventType::DamageDealt, target.owner, sourceId, targetId, {}, target.coord, amount,
               text});

    if (target.hp[index] <= 0) {
        target.hp.erase(target.hp.begin() + index);
    }
    if (target.hp.empty()) killUnit(targetId, sourceId);
}

void GameEngine::applyDamage(UnitId targetId, int amount, DamageType type, UnitId sourceId) {
    if (amount <= 0) return;
    applyDamage(targetId, DamagePacket{{DamageRoll{0, 0, amount, type}}, 0, "flat"}, sourceId);
}

void GameEngine::applyDamage(UnitId targetId, int amount, UnitId sourceId) {
    applyDamage(targetId, amount, DamageType::Force, sourceId);
}

void GameEngine::applyHeal(UnitId targetId, int amount, UnitId sourceId) {
    if (targetId < 0 || targetId >= static_cast<int>(units_.size()) || amount <= 0) return;
    Unit& target = unit(targetId);
    if (!target.alive || target.hp.empty()) return;

    auto it = std::min_element(target.hp.begin(), target.hp.end());
    int before = *it;
    *it = std::min(target.spec.maxHp, *it + amount);
    int healed = *it - before;
    if (healed > 0) {
        pushEvent({EventType::Healed, target.owner, sourceId, targetId, {}, target.coord, healed,
                   eventUnitName(target) + " healed " + std::to_string(healed)});
    }
}

void GameEngine::addShield(UnitId targetId, int amount, UnitId sourceId) {
    if (targetId < 0 || targetId >= static_cast<int>(units_.size()) || amount <= 0) return;
    Unit& target = unit(targetId);
    if (!target.alive) return;
    target.shield = std::min(120, target.shield + amount);
    pushEvent({EventType::Shielded, target.owner, sourceId, targetId, {}, target.coord, amount,
               eventUnitName(target) + " gained shield"});
}

void GameEngine::killUnit(UnitId id, UnitId sourceId) {
    if (id < 0 || id >= static_cast<int>(units_.size())) return;
    Unit& dead = unit(id);
    if (!dead.alive) return;

    Coord deathCoord = dead.coord;
    PlayerId owner = dead.owner;
    UnitType deadType = dead.spec.type;
    AbilityKind deathAbility = dead.spec.ability;
    bool deadIsNeutral = isNeutralMonsterType(dead.spec.type);
    dead.alive = false;
    dead.hp.clear();
    removeFromBoard(id);
    eraseValue(player(owner).bench, id);
    eraseValue(player(owner).deployed, id);
    dead.deployed = false;
    dead.target = kInvalidUnitId;
    dead.retargetTimer = 0.0;
    dead.neutralReturningHome = false;

    pushEvent({EventType::UnitDied, owner, sourceId, id, deathCoord, deathCoord, 0,
               eventUnitName(dead) + " died"});

    updateExplorationObjectiveForDeath(id, sourceId);

    if (!isRoundTransientUnit(deadType)) {
        corpses_.push_back({deathCoord, owner, deadType, round_, false});
    }

    if (sourceId >= 0 && sourceId < static_cast<int>(units_.size())) {
        Unit& killer = unit(sourceId);
        bool killerIsNeutral = isNeutralLikeCombatant(killer);
        if (killer.alive && !killerIsNeutral && (killer.owner != owner || deadIsNeutral)) {
            int bounty = killBountyFor(dead);
            if (bounty > 0) {
                PlayerState& killerPlayer = player(killer.owner);
                killerPlayer.money += bounty;
                std::string bountyText = deadIsNeutral
                                             ? eventUnitName(dead) + " dropped " +
                                                   std::to_string(bounty) + " gp bounty"
                                             : killerPlayer.name + " claimed " +
                                                   std::to_string(bounty) + " gp bounty";
                pushEvent({EventType::GoldGained, killer.owner, sourceId, id, {}, deathCoord, bounty,
                           std::move(bountyText)});
            }
        }
    }

    if (deathAbility == AbilityKind::MephitDeathBurst) {
        pushEvent({EventType::UnitAttacked, owner, id, kInvalidUnitId, deathCoord, deathCoord, 60,
                   eventUnitName(dead) + " exploded in a fire death burst"});
        std::vector<UnitId> targets;
        for (const Unit& enemy : units_) {
            if (!enemy.alive || !enemy.deployed) continue;
            bool enemyIsNeutral = isNeutralMonsterType(enemy.spec.type);
            if (deadIsNeutral && enemyIsNeutral) continue;
            if (!deadIsNeutral && !enemyIsNeutral && enemy.owner == owner) continue;
            if (enemy.spec.layer == UnitLayer::Land && manhattan(enemy.coord, deathCoord) <= 1) {
                targets.push_back(enemy.id);
            }
        }
        for (UnitId targetId : targets) {
            bool saved = savingThrowSucceeds(targetId, id, "Dex");
            applyDamage(targetId, saved ? 30 : 60, DamageType::Fire, id);
        }
    }
}

void GameEngine::clearDeadUnits() {
    for (PlayerState& p : players_) {
        p.bench.erase(std::remove_if(p.bench.begin(), p.bench.end(), [this](UnitId id) {
            return id < 0 || id >= static_cast<int>(units_.size()) || !unit(id).alive;
        }), p.bench.end());
        p.deployed.erase(std::remove_if(p.deployed.begin(), p.deployed.end(), [this](UnitId id) {
            return id < 0 || id >= static_cast<int>(units_.size()) ||
                   !unit(id).alive;
        }), p.deployed.end());
    }
}

void GameEngine::resolveVictory() {
    if (phase_ != Phase::Combat) return;

    if (!hasActiveCombatUnit(PlayerId::One) && !hasActiveCombatUnit(PlayerId::Two)) {
        startNextRound("no player/AI combat units remain");
        return;
    }
}

bool GameEngine::hasActiveCombatUnit(PlayerId playerId) const {
    for (UnitId id : player(playerId).deployed) {
        if (id < 0 || id >= static_cast<int>(units_.size())) continue;
        const Unit& u = unit(id);
        if (u.alive && u.deployed && !isInternalUnit(u.spec.type) && !u.neutralControlled) return true;
    }
    return false;
}

void GameEngine::moveUnits(double dt) {
    std::vector<UnitId> ids;
    for (const PlayerState& p : players_) {
        for (UnitId id : p.deployed) {
            if (id >= 0 && id < static_cast<int>(units_.size()) && unit(id).alive) ids.push_back(id);
        }
    }

    std::sort(ids.begin(), ids.end(), [this](UnitId lhs, UnitId rhs) {
        const Unit& a = unit(lhs);
        const Unit& b = unit(rhs);
        int aDepth = a.owner == PlayerId::One ? a.coord.x : (board_.width - 1 - a.coord.x);
        int bDepth = b.owner == PlayerId::One ? b.coord.x : (board_.width - 1 - b.coord.x);
        if (aDepth != bDepth) return aDepth > bDepth;
        if (a.spec.speed != b.spec.speed) return a.spec.speed > b.spec.speed;
        if (totalHp(a) != totalHp(b)) return totalHp(a) < totalHp(b);
        return lhs < rhs;
    });

    std::vector<Coord> reserved;
    for (UnitId id : ids) {
        Unit& u = unit(id);
        if (!u.alive || !u.deployed || u.spec.speed <= 0.0) continue;
        if (hasStatus(u, StatusKind::Stunned) || hasStatus(u, StatusKind::Prone)) continue;
        if (shouldForceRetarget(u)) {
            u.target = selectTarget(u);
            u.retargetTimer = kRetargetInterval;
        }
        if (!hasStatus(u, StatusKind::Frightened) &&
            u.target != kInvalidUnitId && u.target < static_cast<int>(units_.size()) &&
            unit(u.target).alive && inAttackRange(u, unit(u.target))) {
            continue;
        }

        u.moveProgress += effectiveSpeed(u) * dt;
        if (u.moveProgress < 1.0) continue;
        u.moveProgress -= 1.0;

        std::optional<Coord> next = chooseNextStep(id, reserved);
        if (!next || *next == u.coord) {
            ++u.stuckTicks;
            if (u.stuckTicks > 12) {
                u.target = kInvalidUnitId;
                u.retargetTimer = 0.0;
                u.moveProgress = 0.0;
                u.lastCoord = u.coord;
                u.stuckTicks = 0;
            }
            continue;
        }

        Coord from = u.coord;
        removeFromBoard(id);
        if (!placeUnit(id, *next)) {
            placeUnit(id, from);
            ++u.stuckTicks;
            if (u.stuckTicks > 8) {
                u.target = kInvalidUnitId;
                u.retargetTimer = 0.0;
                u.moveProgress = 0.0;
                u.lastCoord = u.coord;
                u.stuckTicks = 0;
            }
            continue;
        }

        if (u.spec.ability == AbilityKind::PaladinCharge) u.firstStrikeReady = true;
        u.stuckTicks = from == *next ? u.stuckTicks + 1 : 0;
        u.lastCoord = from;
        if (u.neutralReturningHome && board_.inBounds(u.homeCoord) && u.coord == u.homeCoord) {
            u.neutralReturningHome = false;
            u.neutralProvoked = false;
            u.provokedBy = kInvalidUnitId;
            u.target = kInvalidUnitId;
            u.retargetTimer = 0.0;
            pushEvent({EventType::UnitMoved, u.owner, id, kInvalidUnitId, from, *next, 0,
                       eventUnitName(u) + " returned to its guard post"});
        } else {
            pushEvent({EventType::UnitMoved, u.owner, id, kInvalidUnitId, from, *next, 0,
                       eventUnitName(u) + " moved"});
        }
        if (u.spec.layer == UnitLayer::Land) reserved.push_back(*next);
        if (canTriggerHiddenEvent(u)) {
            triggerHiddenEventAt(u.owner, *next, id);
            triggerTrapAt(u.owner, *next, id);
        }
    }
}

bool GameEngine::hasImmediateAttackTarget(const Unit& u) const {
    for (const Unit& candidate : units_) {
        if (!candidate.alive || !candidate.deployed) continue;
        if (!targetAllowedByAggro(u, candidate)) continue;
        if (inAttackRange(u, candidate)) return true;
    }
    return false;
}

std::optional<Coord> GameEngine::chooseLandStepToward(const Unit& u, Coord target, bool attackTarget,
                                                      const std::vector<Coord>& reserved,
                                                      int attackRange,
                                                      std::optional<Coord> leashCenter,
                                                      int leashRadius) const {
    PathResult directPath = attackTarget ? findPathToAttackCell(u, target, attackRange)
                                         : findPathToGoal(u, target);
    if (!directPath.found || directPath.steps.size() < 2) return std::nullopt;

    Coord best = directPath.steps[1];
    double bestScore = -std::numeric_limits<double>::infinity();
    auto reservedCell = [&](Coord coord) {
        return std::find(reserved.begin(), reserved.end(), coord) != reserved.end();
    };
    auto consider = [&](Coord candidate, bool direct) {
        if (reservedCell(candidate) || !passableForLand(u, candidate)) return;
        if (leashCenter && manhattan(candidate, *leashCenter) > leashRadius) return;
        Unit probe = u;
        probe.coord = candidate;
        PathResult path = attackTarget ? findPathToAttackCell(probe, target, attackRange)
                                       : findPathToGoal(probe, target);
        if (!path.found) return;
        int cost = path.steps.empty() ? manhattan(candidate, target)
                                      : static_cast<int>(path.steps.size()) - 1;
        int currentDistance = manhattan(u.coord, target);
        int candidateDistance = manhattan(candidate, target);
        double score = -cost * 20.0 + (currentDistance - candidateDistance) * 7.0;
        if (direct) score += 18.0;
        if (candidate == u.lastCoord) score -= u.stuckTicks >= 6 ? 18.0 : 75.0;
        if (u.spec.range > 1 && attackTarget && candidateDistance <= 1) score -= 45.0;
        if (u.spec.range > 1 && attackTarget && candidateDistance <= attackRange) {
            score += 28.0 - std::abs(candidateDistance - attackRange) * 5.0;
        }
        if (u.spec.range <= 1 || (u.spec.roleMask & kRoleMelee)) {
            score += candidateDistance <= 1 ? 18.0 : 0.0;
        }
        int forwardDepth = u.owner == PlayerId::One ? candidate.x : (board_.width - 1 - candidate.x);
        score += forwardDepth * 0.2;
        if (score > bestScore) {
            bestScore = score;
            best = candidate;
        }
    };

    consider(directPath.steps[1], true);
    for (Coord candidate : neighbors4(u.coord)) {
        consider(candidate, candidate == directPath.steps[1]);
    }

    if (bestScore == -std::numeric_limits<double>::infinity()) return std::nullopt;
    return best;
}

std::optional<Coord> GameEngine::chooseNextStep(UnitId id, const std::vector<Coord>& reserved) const {
    const Unit& u = unit(id);
    if (isNeutralMonsterType(u.spec.type) && !neutralCanLeaveHome(u)) return std::nullopt;
    if (isNeutralSpawServant(u)) return chooseNeutralSummonGuardStep(u, reserved);
    if (isNeutralGuardianUnit(u)) return chooseNeutralGuardianStep(u, reserved);

    Coord target = u.coord;
    bool hasUnitTarget = u.target != kInvalidUnitId && u.target < static_cast<int>(units_.size()) &&
                         unit(u.target).alive;
    if (u.spec.ability == AbilityKind::ClericHeal) {
        if (std::optional<Coord> supportStep = chooseSupportStep(u, reserved)) return supportStep;
        if (selectGlobalHealTarget(u) != kInvalidUnitId || selectFollowAlly(u) != kInvalidUnitId) {
            return std::nullopt;
        }
    }

    if (hasStatus(u, StatusKind::Frightened)) {
        UnitId threat = u.target;
        if (threat == kInvalidUnitId || threat >= static_cast<int>(units_.size()) || !unit(threat).alive) {
            threat = selectTarget(u);
        }
        if (threat != kInvalidUnitId) {
            Coord away = u.coord;
            int bestDistance = manhattan(u.coord, unit(threat).coord);
            for (Coord candidate : neighbors4(u.coord)) {
                if (std::find(reserved.begin(), reserved.end(), candidate) != reserved.end()) continue;
                if (!passableForLand(u, candidate)) continue;
                int distance = manhattan(candidate, unit(threat).coord);
                if (distance > bestDistance) {
                    bestDistance = distance;
                    away = candidate;
                }
            }
            if (away != u.coord) return away;
        }
    }

    if (u.spec.type == UnitType::NeutralRedcap && !hasUnitTarget) return std::nullopt;
    if (u.neutralControlled && !hasUnitTarget) return std::nullopt;

    std::optional<Coord> explorationGoal;
    if (!hasUnitTarget && !isNeutralLikeCombatant(u) && !isRoundTransientUnit(u.spec.type)) {
        explorationGoal = chooseExplorationGoal(u);
        if (explorationGoal) target = *explorationGoal;
    }

    if (hasUnitTarget) {
        target = unit(u.target).coord;
        if (inAttackRange(u, unit(u.target))) return std::nullopt;
    } else if (hasImmediateAttackTarget(u)) {
        return std::nullopt;
    }

    if (u.spec.layer == UnitLayer::Air) return chooseAirStep(u, target, reserved);

    int attackPathRange = u.spec.ability == AbilityKind::MinotaurCharge ? 1 : u.spec.range;
    std::optional<Coord> chosen = chooseLandStepToward(u, target, hasUnitTarget, reserved, attackPathRange);
    if (!chosen) return std::nullopt;
    Coord next = *chosen;
    if (!hasUnitTarget && explorationGoal && next == u.lastCoord) {
        Coord bestAlternate = next;
        int bestScore = std::numeric_limits<int>::min();
        for (Coord candidate : neighbors4(u.coord)) {
            if (candidate == u.lastCoord) continue;
            if (std::find(reserved.begin(), reserved.end(), candidate) != reserved.end()) continue;
            if (!passableForLand(u, candidate)) continue;
            Unit probe = u;
            probe.coord = candidate;
            PathResult alternatePath = findPathToGoal(probe, target);
            if (!alternatePath.found) continue;
            int score = -static_cast<int>(alternatePath.steps.size()) * 10;
            score += (manhattan(u.coord, target) - manhattan(candidate, target)) * 4;
            if (score > bestScore) {
                bestScore = score;
                bestAlternate = candidate;
            }
        }
        if (bestScore == std::numeric_limits<int>::min() && u.stuckTicks < 6) {
            return std::nullopt;
        }
        next = bestAlternate;
    }
    if (u.spec.type == UnitType::NeutralRedcap) {
        Coord origin = board_.inBounds(u.homeCoord) ? u.homeCoord : u.coord;
        if (manhattan(next, origin) > kRedcapAmbushRadius) return std::nullopt;
    }
    if (std::find(reserved.begin(), reserved.end(), next) != reserved.end()) return std::nullopt;
    if (!passableForLand(u, next)) return std::nullopt;
    return next;
}

bool GameEngine::neutralCanLeaveHome(const Unit& u) const {
    if (!isNeutralMonsterType(u.spec.type)) return true;
    if (u.spec.type == UnitType::NeutralRedcap) {
        Coord origin = board_.inBounds(u.homeCoord) ? u.homeCoord : u.coord;
        return manhattan(u.coord, origin) <= kRedcapAmbushRadius;
    }
    if (u.neutralBehavior == NeutralBehavior::HostileAmbusher) return true;
    if (!board_.inBounds(u.homeCoord)) return false;
    if (u.neutralReturningHome) return true;
    return u.neutralProvoked;
}

std::optional<Coord> GameEngine::chooseNeutralGuardianStep(const Unit& u,
                                                           const std::vector<Coord>& reserved) const {
    if (u.neutralReturningHome || !u.neutralProvoked) {
        Coord home = board_.inBounds(u.homeCoord) ? u.homeCoord : u.coord;
        if (u.coord == home) return std::nullopt;
        PathResult homePath = u.spec.layer == UnitLayer::Air ? PathResult{true, {u.coord, home}}
                                                             : findPathToGoal(u, home);
        if (!homePath.found || homePath.steps.size() < 2) return std::nullopt;
        Coord next = homePath.steps[1];
        if (std::find(reserved.begin(), reserved.end(), next) != reserved.end()) return std::nullopt;
        if (!withinNeutralLeash(u, next)) return std::nullopt;
        if (u.spec.layer == UnitLayer::Air) return chooseAirStep(u, home, reserved);
        if (!passableForLand(u, next)) return std::nullopt;
        return next;
    }
    if (u.target == kInvalidUnitId || u.target >= static_cast<int>(units_.size()) ||
        !unit(u.target).alive || !unit(u.target).deployed ||
        !withinNeutralLeash(u, unit(u.target).coord)) {
        return std::nullopt;
    }

    const Unit& target = unit(u.target);
    if (inAttackRange(u, target)) return std::nullopt;
    if (u.spec.layer == UnitLayer::Air) {
        std::optional<Coord> next = chooseAirStep(u, target.coord, reserved);
        if (next && withinNeutralLeash(u, *next)) return next;
        return std::nullopt;
    }
    Coord home = neutralLeashOrigin(u);
    std::optional<Coord> chosen =
        chooseLandStepToward(u, target.coord, true, reserved, u.spec.range,
                             home, kNeutralGuardianLeashRadius);
    if (!chosen) return std::nullopt;
    Coord next = *chosen;
    if (std::find(reserved.begin(), reserved.end(), next) != reserved.end()) return std::nullopt;
    if (!withinNeutralLeash(u, next)) return std::nullopt;
    if (!passableForLand(u, next)) return std::nullopt;
    return next;
}

std::optional<Coord> GameEngine::chooseNeutralSummonGuardStep(const Unit& u,
                                                              const std::vector<Coord>& reserved) const {
    Coord home = neutralLeashOrigin(u);
    if (u.target != kInvalidUnitId && u.target < static_cast<int>(units_.size()) &&
        unit(u.target).alive && unit(u.target).deployed && withinNeutralLeash(u, unit(u.target).coord)) {
        if (inAttackRange(u, unit(u.target))) return std::nullopt;
        std::optional<Coord> chosen =
            chooseLandStepToward(u, unit(u.target).coord, true, reserved, u.spec.range,
                                 home, kNeutralSummonGuardRadius);
        if (chosen) {
            Coord next = *chosen;
            if (manhattan(next, home) <= kNeutralSummonGuardRadius &&
                std::find(reserved.begin(), reserved.end(), next) == reserved.end() &&
                passableForLand(u, next)) {
                return next;
            }
        }
    }

    if (u.coord == home || manhattan(u.coord, home) <= 1) return std::nullopt;
    PathResult path = findPathToGoal(u, home);
    if (!path.found || path.steps.size() < 2) return std::nullopt;
    Coord next = path.steps[1];
    if (std::find(reserved.begin(), reserved.end(), next) != reserved.end()) return std::nullopt;
    if (!passableForLand(u, next)) return std::nullopt;
    return next;
}

std::optional<Coord> GameEngine::chooseExplorationGoal(const Unit& u) const {
    Coord best{};
    double bestScore = -std::numeric_limits<double>::infinity();

    auto consider = [&](Coord coord, const PathResult& path, double score) {
        if (!board_.inBounds(coord) || board_.blocked(coord)) return;
        if (!path.found) return;
        int pathCost = path.steps.empty() ? manhattan(u.coord, coord)
                                          : static_cast<int>(path.steps.size()) - 1;
        score -= pathCost * 5.0;
        if (path.steps.size() > 1 && path.steps[1] == u.lastCoord) score -= 45.0;
        if (score > bestScore) {
            bestScore = score;
            best = path.steps.empty() ? coord : path.steps.back();
        }
    };

    int maxHp = std::max(1, u.spec.maxHp * u.spec.unitCount);
    double hpRatio = std::clamp(static_cast<double>(totalHp(u)) / static_cast<double>(maxHp), 0.0, 1.0);
    auto onHomeFront = [&](Coord coord) {
        int center = board_.width / 2;
        return u.owner == PlayerId::One ? coord.x <= center : coord.x >= center;
    };
    bool homeFrontObjectiveAvailable = false;
    for (const ExplorationObjectiveState& objective : exploration_.objectives()) {
        if (objective.cleared) continue;
        if (objective.kind == ExplorationObjectiveKind::Trap && objective.triggered) continue;
        if (onHomeFront(objective.coord)) {
            homeFrontObjectiveAvailable = true;
            break;
        }
    }
    for (const ExplorationObjectiveState& objective : exploration_.objectives()) {
        if (objective.cleared) continue;
        if (objective.kind == ExplorationObjectiveKind::Trap && objective.triggered) continue;
        if (homeFrontObjectiveAvailable && !onHomeFront(objective.coord)) continue;

        double score = objective.rewardGold * 4.0 + objective.rewardQuality * 18.0;
        switch (objective.kind) {
            case ExplorationObjectiveKind::Camp:
                score += 100.0;
                break;
            case ExplorationObjectiveKind::Elite:
                score += 140.0 - (hpRatio < 0.55 ? 42.0 : 0.0);
                break;
            case ExplorationObjectiveKind::Boss:
                score += 185.0 - (hpRatio < 0.70 ? 80.0 : 0.0);
                break;
            case ExplorationObjectiveKind::Trap:
                score += 82.0 - (hpRatio < 0.50 ? 28.0 : 0.0);
                break;
        }

        if (objective.unitId != kInvalidUnitId) {
            if (objective.unitId < 0 || objective.unitId >= static_cast<int>(units_.size())) continue;
            const Unit& objectiveUnit = unit(objective.unitId);
            if (!objectiveUnit.alive || !objectiveUnit.deployed) continue;
            score += objectiveUnit.spec.threat * 0.45;
            if (objective.kind == ExplorationObjectiveKind::Boss) {
                score -= objectiveUnit.spec.threat * (hpRatio < 0.85 ? 0.45 : 0.18);
            }
            PathResult path = u.spec.layer == UnitLayer::Air
                                  ? PathResult{true, {u.coord, objectiveUnit.coord}}
                                  : findPathToAttackCell(u, objectiveUnit.coord, u.spec.range);
            if (onHomeFront(objective.coord)) score += 10.0;
            consider(objectiveUnit.coord, path, score);
            continue;
        }

        if (onHomeFront(objective.coord)) score += 10.0;
        PathResult path = u.spec.layer == UnitLayer::Air ? PathResult{true, {u.coord, objective.coord}}
                                                         : findPathToGoal(u, objective.coord);
        consider(objective.coord, path, score);
    }

    if (bestScore == -std::numeric_limits<double>::infinity()) return std::nullopt;
    return best;
}

std::optional<Coord> GameEngine::chooseSupportStep(const Unit& u, const std::vector<Coord>& reserved) const {
    (void)reserved;
    if (!u.alive || !u.deployed || u.spec.ability != AbilityKind::ClericHeal) return std::nullopt;

    UnitId healTarget = selectGlobalHealTarget(u);
    if (healTarget != kInvalidUnitId) {
        const Unit& target = unit(healTarget);
        if (manhattan(u.coord, target.coord) <= u.spec.abilityRange) return std::nullopt;

        PathResult path = findPathToAttackCell(u, target.coord, u.spec.abilityRange);
        if (!path.found || path.steps.size() < 2) return std::nullopt;
        Coord next = path.steps[1];
        if (!passableForLand(u, next)) return std::nullopt;
        return next;
    }

    std::optional<Coord> goal = chooseFollowAllyGoal(u);
    if (!goal || *goal == u.coord) return std::nullopt;

    PathResult path = findPathToGoal(u, *goal);
    if (!path.found || path.steps.size() < 2) return std::nullopt;
    Coord next = path.steps[1];
    if (!passableForLand(u, next)) return std::nullopt;
    return next;
}

std::optional<Coord> GameEngine::chooseFollowAllyGoal(const Unit& u) const {
    UnitId allyId = selectFollowAlly(u);
    if (allyId == kInvalidUnitId) return std::nullopt;

    const Unit& ally = unit(allyId);
    int behindDir = u.owner == PlayerId::One ? -1 : 1;
    int desiredX = ally.coord.x + behindDir;
    int followRange = std::max(1, std::min(3, u.spec.abilityRange));

    Coord best = u.coord;
    double bestScore = -std::numeric_limits<double>::infinity();
    for (Coord candidate : cellsInRange(ally.coord, followRange)) {
        if (candidate == ally.coord) continue;
        if (!board_.inBounds(candidate)) continue;
        if (candidate != u.coord && !passableForLand(u, candidate)) continue;

        bool behind = u.owner == PlayerId::One ? candidate.x <= ally.coord.x : candidate.x >= ally.coord.x;
        double score = 0.0;
        if (behind) score += 36.0;
        score -= std::abs(candidate.x - desiredX) * 9.0;
        score -= std::abs(candidate.y - ally.coord.y) * 6.0;
        score -= manhattan(u.coord, candidate) * 2.5;
        score -= std::abs(manhattan(candidate, ally.coord) - 2) * 3.0;
        if (candidate == u.coord) score += 12.0;

        if (score > bestScore) {
            bestScore = score;
            best = candidate;
        }
    }

    return best;
}

std::optional<Coord> GameEngine::chooseAirStep(const Unit& u, Coord target, const std::vector<Coord>& reserved) const {
    (void)reserved;
    std::vector<Coord> candidates = neighbors4(u.coord);
    Coord forward = forwardCoord(u.owner, u.coord);
    if (board_.inBounds(forward)) candidates.insert(candidates.begin(), forward);

    int currentDist = manhattan(u.coord, target);
    Coord best = u.coord;
    int bestScore = std::numeric_limits<int>::min();
    for (Coord candidate : candidates) {
        if (!board_.inBounds(candidate)) continue;
        int dist = manhattan(candidate, target);
        if (dist > currentDist) continue;
        int forwardBias = u.owner == PlayerId::One ? candidate.x : (board_.width - 1 - candidate.x);
        int score = (currentDist - dist) * 100 + forwardBias * 3 - std::abs(candidate.y - target.y) * 2;
        if (score > bestScore) {
            bestScore = score;
            best = candidate;
        }
    }
    if (best == u.coord) return std::nullopt;
    return best;
}

GameEngine::PathResult GameEngine::findPathToAttackCell(const Unit& u, Coord target, int range) const {
    if (manhattan(u.coord, target) <= range) return {true, {u.coord}};
    if (!board_.inBounds(target)) return {};

    const int n = board_.width * board_.height;
    std::vector<int> dist(n, std::numeric_limits<int>::max());
    std::vector<Coord> parent(n, Coord{-1, -1});
    auto index = [this](Coord coord) { return coord.y * board_.width + coord.x; };
    auto buildPath = [&](Coord goal) {
        std::vector<Coord> reversed;
        Coord step = goal;
        while (step != Coord{-1, -1}) {
            reversed.push_back(step);
            if (step == u.coord) break;
            step = parent[index(step)];
        }
        std::reverse(reversed.begin(), reversed.end());
        return reversed;
    };

    std::queue<Coord> q;
    dist[index(u.coord)] = 0;
    q.push(u.coord);

    while (!q.empty()) {
        Coord current = q.front();
        q.pop();
        if (current != u.coord && manhattan(current, target) <= range) {
            return {true, buildPath(current)};
        }

        for (Coord next : neighbors4(current)) {
            if (!passableForLand(u, next)) continue;
            int ni = index(next);
            if (dist[ni] != std::numeric_limits<int>::max()) continue;
            dist[ni] = dist[index(current)] + 1;
            parent[ni] = current;
            q.push(next);
        }
    }

    return {};
}

GameEngine::PathResult GameEngine::findPathToGoal(const Unit& u, Coord goal) const {
    if (!board_.inBounds(goal)) return {};
    if (u.coord == goal) return {true, {u.coord}};

    const int n = board_.width * board_.height;
    std::vector<int> dist(n, std::numeric_limits<int>::max());
    std::vector<Coord> parent(n, Coord{-1, -1});
    auto index = [this](Coord coord) { return coord.y * board_.width + coord.x; };

    std::queue<Coord> q;
    dist[index(u.coord)] = 0;
    q.push(u.coord);

    while (!q.empty()) {
        Coord current = q.front();
        q.pop();
        if (current == goal) break;

        for (Coord next : neighbors4(current)) {
            if (!board_.inBounds(next)) continue;
            if (next != goal && !passableForLand(u, next)) continue;
            if (next == goal && !passableForLand(u, next) && next != u.coord) continue;
            int ni = index(next);
            if (dist[ni] != std::numeric_limits<int>::max()) continue;
            dist[ni] = dist[index(current)] + 1;
            parent[ni] = current;
            q.push(next);
        }
    }

    if (dist[index(goal)] == std::numeric_limits<int>::max()) return {};

    std::vector<Coord> reversed;
    Coord step = goal;
    while (step != Coord{-1, -1}) {
        reversed.push_back(step);
        if (step == u.coord) break;
        step = parent[index(step)];
    }
    std::reverse(reversed.begin(), reversed.end());
    return {true, reversed};
}

bool GameEngine::passableForLand(const Unit& u, Coord coord) const {
    if (!board_.inBounds(coord)) return false;
    if (board_.blocked(coord)) return false;
    for (UnitId id : board_.occupants(coord, UnitLayer::Land)) {
        if (id < 0 || id >= static_cast<int>(units_.size())) continue;
        const Unit& occupant = unit(id);
        if (!occupant.alive || occupant.id == u.id) continue;
        if (occupant.deployed) {
            return false;
        }
    }
    return true;
}

std::vector<Coord> GameEngine::neighbors4(Coord coord) const {
    std::vector<Coord> result = {
        {coord.x + 1, coord.y},
        {coord.x - 1, coord.y},
        {coord.x, coord.y + 1},
        {coord.x, coord.y - 1},
    };
    result.erase(std::remove_if(result.begin(), result.end(), [this](Coord c) {
        return !board_.inBounds(c);
    }), result.end());
    return result;
}

std::vector<Coord> GameEngine::cellsInRange(Coord center, int range) const {
    std::vector<Coord> cells;
    for (int y = center.y - range; y <= center.y + range; ++y) {
        for (int x = center.x - range; x <= center.x + range; ++x) {
            Coord coord{x, y};
            if (board_.inBounds(coord) && manhattan(center, coord) <= range) cells.push_back(coord);
        }
    }
    return cells;
}

std::vector<Coord> GameEngine::adjacentCells(Coord center) const {
    std::vector<Coord> cells = neighbors4(center);
    cells.push_back(center);
    return cells;
}

Coord GameEngine::forwardCoord(PlayerId playerId, Coord coord) const {
    return playerId == PlayerId::One ? Coord{coord.x + 1, coord.y} : Coord{coord.x - 1, coord.y};
}

std::optional<Coord> GameEngine::findSummonCell(PlayerId owner, Coord origin, UnitLayer layer) const {
    (void)layer;
    auto isLandable = [&](Coord coord) -> bool {
        if (!board_.inBounds(coord)) return false;
        if (board_.blocked(coord)) return false;
        for (UnitId id : board_.occupants(coord, UnitLayer::Land)) {
            if (id < 0 || id >= static_cast<int>(units_.size())) continue;
            const Unit& u = unit(id);
            if (u.alive && u.deployed) {
                return false;
            }
        }
        return true;
    };

    std::vector<Coord> candidates;
    Coord forward = forwardCoord(owner, origin);
    if (board_.inBounds(forward)) candidates.push_back(forward);
    for (Coord coord : adjacentCells(origin)) {
        if (coord != forward) candidates.push_back(coord);
    }
    for (Coord coord : candidates) {
        if (isLandable(coord)) return coord;
    }
    return std::nullopt;
}

void GameEngine::performAssassinLeap(UnitId id) {
    if (id < 0 || id >= static_cast<int>(units_.size())) return;
    Unit& assassin = unit(id);
    if (!assassin.alive || assassin.spec.ability != AbilityKind::RogueAmbush) return;
    if (isNeutralMonsterType(assassin.spec.type)) return;

    UnitId target = kInvalidUnitId;
    int bestScore = std::numeric_limits<int>::min();
    for (const Unit& enemy : units_) {
        if (!enemy.alive || !enemy.deployed) continue;
        if (!canAttack(assassin, enemy)) continue;
        if (manhattan(assassin.coord, enemy.coord) > kRogueAmbushMaxRange) continue;
        if (!inAttackRange(assassin, enemy) &&
            !findPathToAttackCell(assassin, enemy.coord, assassin.spec.range).found) {
            continue;
        }
        int backline = enemy.owner == PlayerId::One ? (board_.width - 1 - enemy.coord.x) : enemy.coord.x;
        int score = enemy.spec.threat + backline * 8;
        if (enemy.spec.roleMask & (kRoleSupport | kRoleRanged | kRoleAoe)) score += 30;
        if (score > bestScore) {
            bestScore = score;
            target = enemy.id;
        }
    }
    if (target == kInvalidUnitId) return;

    Coord targetCoord = unit(target).coord;
    auto isLandable = [&](Coord coord) -> bool {
        if (!board_.inBounds(coord)) return false;
        if (board_.blocked(coord)) return false;
        if (coord == targetCoord) return false;  // adjacent, not on top of
        for (UnitLayer occupiedLayer : {UnitLayer::Land, UnitLayer::Air}) {
            for (UnitId other : board_.occupants(coord, occupiedLayer)) {
                if (other < 0 || other >= static_cast<int>(units_.size())) continue;
                const Unit& u = unit(other);
                if (u.alive && isNeutralMonsterType(u.spec.type)) {
                    return false;
                }
            }
        }
        return true;
    };

    // Pick the adjacent cell furthest forward into enemy territory among
    // valid landing spots. Falls back to the closest adjacent cell if no
    // forward candidate exists.
    Coord best{-1, -1};
    int bestForward = std::numeric_limits<int>::min();
    for (Coord coord : neighbors4(targetCoord)) {
        if (!isLandable(coord)) continue;
        int forwardDepth = assassin.owner == PlayerId::One
                               ? coord.x
                               : (board_.width - 1 - coord.x);
        if (forwardDepth > bestForward) {
            bestForward = forwardDepth;
            best = coord;
        }
    }
    if (best.x < 0) return;

    Coord from = assassin.coord;
    removeFromBoard(id);
    if (placeUnit(id, best)) {
        assassin.firstStrikeReady = true;
        pushEvent({EventType::UnitMoved, assassin.owner, id, target, from, best, 0,
                   eventUnitName(assassin) + " used Ambush on " + eventUnitName(unit(target))});
    } else {
        placeUnit(id, from);
    }
}

void GameEngine::performGithyankiAstralRaid(UnitId id) {
    if (id < 0 || id >= static_cast<int>(units_.size())) return;
    Unit& raider = unit(id);
    if (!raider.alive || !raider.deployed ||
        raider.spec.ability != AbilityKind::GithyankiAstralRaid) {
        return;
    }

    UnitId target = kInvalidUnitId;
    double bestTargetScore = -std::numeric_limits<double>::infinity();
    for (const Unit& enemy : units_) {
        if (!enemy.alive || !enemy.deployed) continue;
        if (!canAttack(raider, enemy)) continue;
        if (!inAttackRange(raider, enemy) &&
            !findPathToAttackCell(raider, enemy.coord, raider.spec.range).found) {
            continue;
        }
        int dist = manhattan(raider.coord, enemy.coord);
        double score = enemy.spec.threat - dist * 4.0;
        if (enemy.spec.roleMask & (kRoleSupport | kRoleRanged | kRoleAoe)) score += 20.0;
        if (totalHp(enemy) < enemy.spec.maxHp * enemy.spec.unitCount / 2) score += 8.0;
        if (score > bestTargetScore) {
            bestTargetScore = score;
            target = enemy.id;
        }
    }
    if (target == kInvalidUnitId) return;

    Coord targetCoord = unit(target).coord;
    Coord best = raider.coord;
    int currentDist = manhattan(raider.coord, targetCoord);
    double bestCellScore = -std::numeric_limits<double>::infinity();
    int blinkRange = std::max(1, raider.spec.abilityRange);
    for (Coord coord : cellsInRange(raider.coord, blinkRange)) {
        if (coord == raider.coord || coord == targetCoord) continue;
        if (!passableForLand(raider, coord)) continue;
        int dist = manhattan(coord, targetCoord);
        if (dist >= currentDist) continue;
        int forwardDepth = raider.owner == PlayerId::One ? coord.x : (board_.width - 1 - coord.x);
        double score = (currentDist - dist) * 100.0 + forwardDepth * 3.0 - std::abs(coord.y - targetCoord.y) * 6.0;
        if (score > bestCellScore) {
            bestCellScore = score;
            best = coord;
        }
    }
    if (best == raider.coord) return;

    Coord from = raider.coord;
    removeFromBoard(id);
    if (placeUnit(id, best)) {
        raider.firstStrikeReady = true;
        pushEvent({EventType::UnitMoved, raider.owner, id, target, from, best, raider.spec.abilityValue,
                   eventUnitName(raider) + " made an astral raid"});
    } else {
        placeUnit(id, from);
    }
}

std::string toString(PlayerId player) {
    return player == PlayerId::One ? "Player1" : "Player2";
}

std::string toString(AiDifficulty difficulty) {
    switch (difficulty) {
        case AiDifficulty::Normal: return "Normal";
        case AiDifficulty::Hard: return "Hard";
        case AiDifficulty::SuperHard: return "SuperHard";
    }
    return "Normal";
}

std::string toString(AiActionKind kind) {
    switch (kind) {
        case AiActionKind::Buy: return "Buy";
        case AiActionKind::Deploy: return "Deploy";
        case AiActionKind::MoveDeployed: return "MoveDeployed";
        case AiActionKind::ReturnToBench: return "ReturnToBench";
        case AiActionKind::Upgrade: return "Upgrade";
        case AiActionKind::Ready: return "Ready";
    }
    return "Ready";
}

std::string toString(Phase phase) {
    switch (phase) {
        case Phase::Preparation: return "Preparation";
        case Phase::Combat: return "Combat";
        case Phase::Finished: return "Finished";
    }
    return "Unknown";
}

std::string toString(UnitLayer layer) {
    return layer == UnitLayer::Land ? "Land" : "Air";
}

std::string toString(UnitType type) {
    switch (type) {
        case UnitType::Skeleton: return "Skeleton";
        case UnitType::SkeletonByNecromancer: return "SkeletonByNecromancer";
        case UnitType::GithyankiWarrior: return "GithyankiWarrior";
        case UnitType::Ranger: return "Ranger";
        case UnitType::Barbarian: return "Barbarian";
        case UnitType::Necromancer: return "Necromancer";
        case UnitType::FireMephit: return "FireMephit";
        case UnitType::ImpSwarm: return "ImpSwarm";
        case UnitType::GoblinSkirmisher: return "GoblinSkirmisher";
        case UnitType::Paladin: return "Paladin";
        case UnitType::DragonWyrmling: return "DragonWyrmling";
        case UnitType::NeutralSpectator: return "NeutralSpectator";
        case UnitType::NeutralOwlbear: return "NeutralOwlbear";
        case UnitType::NeutralMindFlayer: return "NeutralMindFlayer";
        case UnitType::NeutralSovereignSpaw: return "NeutralSovereignSpaw";
        case UnitType::NeutralKarniss: return "NeutralKarniss";
        case UnitType::NeutralRedcap: return "NeutralRedcap";
        case UnitType::NeutralWaterMyrmidon: return "NeutralWaterMyrmidon";
        case UnitType::NeutralPhaseSpiderMatriarch: return "NeutralPhaseSpiderMatriarch";
        case UnitType::NeutralRaphael: return "NeutralRaphael";
        case UnitType::NeutralKethericThorm: return "NeutralKethericThorm";
        case UnitType::NeutralMoonlightSliver: return "NeutralMoonlightSliver";
        case UnitType::NeutralGuardianOfFaith: return "NeutralGuardianOfFaith";
        case UnitType::NeutralMinotaur: return "NeutralMinotaur";
        case UnitType::NeutralDeathKnight: return "NeutralDeathKnight";
        case UnitType::NeutralAirMyrmidon: return "NeutralAirMyrmidon";
        case UnitType::NeutralTamiaHolzt: return "NeutralTamiaHolzt";
        case UnitType::ShieldGuardian: return "ShieldGuardian";
        case UnitType::Cleric: return "Cleric";
        case UnitType::Evoker: return "Evoker";
        case UnitType::RogueAssassin: return "RogueAssassin";
        case UnitType::Druid: return "Druid";
        case UnitType::Treant: return "Treant";
        case UnitType::SporeServant: return "SporeServant";
    }
    return "Unknown";
}

AiDifficulty aiDifficultyFromString(const std::string& text) {
    std::string normalized = lowerCopy(text);
    normalized.erase(std::remove(normalized.begin(), normalized.end(), ' '), normalized.end());
    normalized.erase(std::remove(normalized.begin(), normalized.end(), '-'), normalized.end());
    normalized.erase(std::remove(normalized.begin(), normalized.end(), '_'), normalized.end());
    if (normalized == "hard" || normalized == "2") return AiDifficulty::Hard;
    if (normalized == "superhard" || normalized == "nightmare" || normalized == "3" ||
        normalized == "4") {
        return AiDifficulty::SuperHard;
    }
    return AiDifficulty::Normal;
}

} // namespace autochess
