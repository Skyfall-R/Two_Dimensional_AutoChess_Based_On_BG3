#include <campaign.hpp>

#include <algorithm>
#include <cmath>
#include <random>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace autochess {

namespace {

int familyIndex(NeutralFamily family) {
    return static_cast<int>(family);
}

std::string familyTag(NeutralFamily family) {
    switch (family) {
        case NeutralFamily::Swarm: return "swarm";
        case NeutralFamily::Guardian: return "guardian";
        case NeutralFamily::Caster: return "caster";
        case NeutralFamily::Assassin: return "assassin";
        case NeutralFamily::Artillery: return "artillery";
    }
    return "swarm";
}

bool hasTag(const RelicSpec& relic, const std::string& tag) {
    return std::find(relic.tags.begin(), relic.tags.end(), tag) != relic.tags.end();
}

RunModifiers makeModifiers(int costDiscount = 0,
                           int upgradeDiscount = 0,
                           int roundIncomeBonus = 0,
                           int interestBonus = 0,
                           int benchBonus = 0,
                           int extraRelicChoices = 0,
                           int bonusGoldOnClear = 0,
                           std::array<int, 5> familyBias = {},
                           int summonLimitBonus = 0) {
    RunModifiers modifiers;
    modifiers.costDiscount = costDiscount;
    modifiers.upgradeDiscount = upgradeDiscount;
    modifiers.roundIncomeBonus = roundIncomeBonus;
    modifiers.interestBonus = interestBonus;
    modifiers.benchBonus = benchBonus;
    modifiers.extraRelicChoices = extraRelicChoices;
    modifiers.bonusGoldOnClear = bonusGoldOnClear;
    modifiers.summonLimitBonus = summonLimitBonus;
    modifiers.familyBias = familyBias;
    return modifiers;
}

RelicSpec relic(std::string id,
                std::string name,
                RelicTier tier,
                std::string summary,
                std::vector<std::string> tags,
                int weight,
                RunModifiers modifiers,
                bool unique = false,
                bool stackable = true) {
    RelicSpec spec;
    spec.id = std::move(id);
    spec.name = std::move(name);
    spec.tier = tier;
    spec.summary = std::move(summary);
    spec.tags = std::move(tags);
    spec.weight = weight;
    spec.unique = unique;
    spec.stackable = stackable;
    spec.modifiers = std::move(modifiers);
    return spec;
}

const std::vector<RelicSpec>& catalogRef() {
    static const std::vector<RelicSpec> catalog = {
        relic("coin_shard",
              "Coin Shard",
              RelicTier::Basic,
              "Round income +1.",
              {"economy", "basic"},
              36,
              makeModifiers(0, 0, 1)),
        relic("tarnished_ledger",
              "Tarnished Ledger",
              RelicTier::Basic,
              "Interest cap +1.",
              {"economy", "interest", "basic"},
              28,
              makeModifiers(0, 0, 0, 1)),
        relic("field_roster",
              "Field Roster",
              RelicTier::Basic,
              "Roster capacity +1.",
              {"shop", "tempo", "basic"},
              30,
              makeModifiers(0, 0, 0, 0, 1)),
        relic("supply_mark",
              "Supply Mark",
              RelicTier::Basic,
              "Buy costs -1, minimum 1.",
              {"shop", "tempo", "basic"},
              26,
              makeModifiers(1)),
        relic("scouts_chalk",
              "Scout's Chalk",
              RelicTier::Basic,
              "Hidden event detection range +1.",
              {"event", "scout", "basic", "choice"},
              24,
              [] {
                  RunModifiers modifiers;
                  modifiers.hiddenEventRevealBonus = 1;
                  return modifiers;
              }()),
        relic("snarewire_charm",
              "Snarewire Charm",
              RelicTier::Basic,
              "Trap checks +2.",
              {"trap", "basic"},
              24,
              [] {
                  RunModifiers modifiers;
                  modifiers.trapCheckBonus = 2;
                  return modifiers;
              }()),

        relic("brood_sigil",
              "Brood Sigil",
              RelicTier::Build,
              "Swarm drops appear more often. Roster capacity +1.",
              {"swarm", "build", "wide"},
              34,
              makeModifiers(0, 0, 0, 0, 1, 0, 0, {2, 0, 0, 0, 0})),
        relic("bulwark_keystone",
              "Bulwark Keystone",
              RelicTier::Build,
              "Guardian drops appear more often. Roster capacity +1.",
              {"guardian", "build", "frontline"},
              32,
              makeModifiers(0, 0, 0, 0, 1, 0, 0, {0, 2, 0, 0, 0})),
        relic("runic_lens",
              "Runic Lens",
              RelicTier::Build,
              "Caster drops appear more often. Income +1.",
              {"caster", "build", "control"},
              32,
              makeModifiers(0, 0, 1, 0, 0, 0, 0, {0, 0, 2, 0, 0})),
        relic("veiled_mark",
              "Veiled Mark",
              RelicTier::Build,
              "Assassin drops appear more often. Buy costs -1.",
              {"assassin", "build", "backline"},
              30,
              makeModifiers(1, 0, 0, 0, 0, 0, 0, {0, 0, 0, 2, 0})),
        relic("siege_lens",
              "Siege Lens",
              RelicTier::Build,
              "Artillery drops appear more often. Interest cap +1.",
              {"artillery", "build", "range"},
              30,
              makeModifiers(0, 0, 0, 1, 0, 0, 0, {0, 0, 0, 0, 2})),
        relic("finders_pouch",
              "Finder's Pouch",
              RelicTier::Basic,
              "Hidden event payouts +1.",
              {"event", "economy", "basic"},
              22,
              [] {
                  RunModifiers modifiers;
                  modifiers.hiddenEventPayoutBonus = 1;
                  return modifiers;
              }()),
        relic("bramble_brooch",
              "Bramble Brooch",
              RelicTier::Build,
              "Swarm drops appear more often. Hidden event payouts +2.",
              {"swarm", "event", "build"},
              20,
              [] {
                  RunModifiers modifiers = makeModifiers(0, 0, 0, 0, 0, 0, 0, {1, 0, 0, 0, 0});
                  modifiers.hiddenEventPayoutBonus = 2;
                  return modifiers;
              }()),
        relic("wardplate_rivet",
              "Wardplate Rivet",
              RelicTier::Build,
              "Guardian drops appear more often. Start combat with +4 shield.",
              {"guardian", "shield", "build"},
              20,
              [] {
                  RunModifiers modifiers = makeModifiers(0, 0, 0, 0, 0, 0, 0, {0, 1, 0, 0, 0});
                  modifiers.roundStartShield = 4;
                  return modifiers;
              }()),
        relic("spellglass_charm",
              "Spellglass Charm",
              RelicTier::Build,
              "Caster drops appear more often. Event healing and shielding +3.",
              {"caster", "event", "build"},
              20,
              [] {
                  RunModifiers modifiers = makeModifiers(0, 0, 0, 0, 0, 0, 0, {0, 0, 1, 0, 0});
                  modifiers.eventHealBonus = 3;
                  return modifiers;
              }()),
        relic("backdoor_token",
              "Backdoor Token",
              RelicTier::Build,
              "Assassin drops appear more often. Trap checks +2.",
              {"assassin", "trap", "build"},
              20,
              [] {
                  RunModifiers modifiers = makeModifiers(0, 0, 0, 0, 0, 0, 0, {0, 0, 0, 1, 0});
                  modifiers.trapCheckBonus = 2;
                  return modifiers;
              }()),
        relic("ballista_gauge",
              "Ballista Gauge",
              RelicTier::Build,
              "Artillery drops appear more often. Hidden event detection range +1.",
              {"artillery", "event", "build"},
              20,
              [] {
                  RunModifiers modifiers = makeModifiers(0, 0, 0, 0, 0, 0, 0, {0, 0, 0, 0, 1});
                  modifiers.hiddenEventRevealBonus = 1;
                  return modifiers;
              }()),
        relic("oath_thread",
              "Oath Thread",
              RelicTier::Build,
              "Guardian drops appear more often. Event healing and shielding +2.",
              {"guardian", "event", "build"},
              18,
              [] {
                  RunModifiers modifiers = makeModifiers(0, 0, 0, 0, 0, 0, 0, {0, 1, 0, 0, 0});
                  modifiers.eventHealBonus = 2;
                  return modifiers;
              }()),

        relic("mirror_contract",
              "Mirror Contract",
              RelicTier::Transform,
              "Relic drafts show one extra choice.",
              {"choice", "draft", "transform"},
              22,
              makeModifiers(0, 0, 0, 0, 0, 1),
              false,
              false),
        relic("blood_tithe",
              "Blood Tithe",
              RelicTier::Transform,
              "Elite and boss clears pay +2 gold.",
              {"boss", "economy", "transform"},
              22,
              makeModifiers(0, 0, 0, 0, 0, 0, 2),
              false,
              false),
        relic("chimeric_plate",
              "Chimeric Plate",
              RelicTier::Transform,
              "Guardian and Swarm drops appear more often. Roster capacity +1.",
              {"guardian", "swarm", "hybrid", "transform"},
              20,
              makeModifiers(0, 0, 0, 0, 1, 0, 0, {1, 1, 0, 0, 0}),
              false,
              false),
        relic("signal_caul",
              "Signal Caul",
              RelicTier::Transform,
              "Caster and Artillery drops appear more often. Interest cap +1.",
              {"caster", "artillery", "hybrid", "transform"},
              20,
              makeModifiers(0, 0, 0, 1, 0, 0, 0, {0, 0, 1, 0, 1}),
              false,
              false),
        relic("pilgrims_basin",
              "Pilgrim's Basin",
              RelicTier::Transform,
              "Hidden healing and waystone effects +6.",
              {"event", "heal", "transform"},
              16,
              [] {
                  RunModifiers modifiers;
                  modifiers.eventHealBonus = 6;
                  return modifiers;
              }(),
              false,
              false),
        relic("cartographers_needle",
              "Cartographer's Needle",
              RelicTier::Transform,
              "Hidden event detection range +1. Hidden event payouts +2.",
              {"event", "choice", "transform"},
              16,
              [] {
                  RunModifiers modifiers;
                  modifiers.hiddenEventRevealBonus = 1;
                  modifiers.hiddenEventPayoutBonus = 2;
                  return modifiers;
              }(),
              false,
              false),
        relic("trapwrights_nail",
              "Trapwright's Nail",
              RelicTier::Transform,
              "Trap checks +4. Successful trap disarms pay +4 gold.",
              {"trap", "economy", "transform"},
              16,
              [] {
                  RunModifiers modifiers;
                  modifiers.trapCheckBonus = 4;
                  modifiers.trapDisarmGoldBonus = 4;
                  return modifiers;
              }(),
              false,
              false),
        relic("grave_dividend",
              "Grave Dividend",
              RelicTier::Transform,
              "Cursed idols pay +6 and deal less damage.",
              {"event", "economy", "transform"},
              14,
              [] {
                  RunModifiers modifiers;
                  modifiers.cursedIdolPayoutBonus = 6;
                  modifiers.cursedIdolDamageReductionPercent = 25;
                  return modifiers;
              }(),
              false,
              false),
        relic("banner_cord",
              "Banner Cord",
              RelicTier::Transform,
              "Rally banners last longer. Start combat with +3 shield.",
              {"event", "shield", "transform"},
              14,
              [] {
                  RunModifiers modifiers;
                  modifiers.rallyDurationBonus = 3;
                  modifiers.roundStartShield = 3;
                  return modifiers;
              }(),
              false,
              false),

        relic("crown_of_mirrors",
              "Crown of Mirrors",
              RelicTier::Unique,
              "Extra relic choice. Buy costs -1.",
              {"choice", "economy", "unique"},
              12,
              makeModifiers(1, 0, 0, 0, 0, 1),
              true,
              false),
        relic("black_sun_vault",
              "Black Sun Vault",
              RelicTier::Unique,
              "Boss and elite clears pay +4 gold.",
              {"boss", "economy", "unique"},
              10,
              makeModifiers(0, 0, 0, 0, 0, 0, 4),
              true,
              false),
        relic("adaptive_signet",
              "Adaptive Signet",
              RelicTier::Unique,
              "Buy costs -1. Relic drafts show one extra choice.",
              {"counter", "shop", "unique"},
              10,
              makeModifiers(1, 0, 0, 0, 0, 1),
              true,
              false),
        relic("underdark_beacon",
              "Underdark Beacon",
              RelicTier::Unique,
              "Hidden event detection range +2. Adds one hidden event.",
              {"event", "choice", "unique"},
              8,
              [] {
                  RunModifiers modifiers;
                  modifiers.hiddenEventRevealBonus = 2;
                  modifiers.hiddenEventCountBonus = 1;
                  return modifiers;
              }(),
              true,
              false),
        relic("nine_hells_writ",
              "Nine-Hells Writ",
              RelicTier::Unique,
              "Elite, boss, and cursed idol payouts +5. Start combat shield -2.",
              {"boss", "event", "economy", "unique"},
              8,
              [] {
                  RunModifiers modifiers = makeModifiers(0, 0, 0, 0, 0, 0, 5);
                  modifiers.cursedIdolPayoutBonus = 5;
                  modifiers.roundStartShield = -2;
                  return modifiers;
              }(),
              true,
              false),
        relic("dawnward_aegis",
              "Dawnward Aegis",
              RelicTier::Unique,
              "Start combat with +10 shield. Healing springs cleanse poison and chill.",
              {"guardian", "heal", "shield", "unique"},
              8,
              [] {
                  RunModifiers modifiers;
                  modifiers.roundStartShield = 10;
                  modifiers.healingSpringCleanses = true;
                  return modifiers;
              }(),
              true,
              false),
        relic("smuggler_kings_seal",
              "Smuggler King's Seal",
              RelicTier::Unique,
              "Smuggler caches always pass their check. Hidden event payouts +4.",
              {"assassin", "event", "economy", "unique"},
              8,
              [] {
                  RunModifiers modifiers;
                  modifiers.smugglerAlwaysSucceeds = true;
                  modifiers.hiddenEventPayoutBonus = 4;
                  return modifiers;
              }(),
              true,
              false),
        relic("vault_compass",
              "Vault Compass",
              RelicTier::Unique,
              "Relic drafts show one extra choice. Hidden event payouts +3.",
              {"choice", "event", "economy", "unique"},
              8,
              [] {
                  RunModifiers modifiers = makeModifiers(0, 0, 0, 0, 0, 1);
                  modifiers.hiddenEventPayoutBonus = 3;
                  return modifiers;
              }(),
              true,
              false),
        relic("quartermaster_badge",
              "Quartermaster Badge",
              RelicTier::Basic,
              "Roster capacity +2.",
              {"shop", "tempo", "basic"},
              24,
              makeModifiers(0, 0, 0, 0, 2)),
        relic("moonlit_purse",
              "Moonlit Purse",
              RelicTier::Basic,
              "Round income +2.",
              {"economy", "basic"},
              22,
              makeModifiers(0, 0, 2)),
        relic("oracle_lens",
              "Oracle Lens",
              RelicTier::Transform,
              "Relic drafts show two extra choices.",
              {"choice", "transform"},
              14,
              makeModifiers(0, 0, 0, 0, 0, 2),
              false,
              false),
        relic("ashen_contract",
              "Ashen Contract",
              RelicTier::Build,
              "Boss and elite clears pay +3 gold. Caster drops appear more often.",
              {"boss", "caster", "build"},
              18,
              makeModifiers(0, 0, 0, 0, 0, 0, 3, {0, 0, 1, 0, 0})),
        relic("bone_censer",
              "Bone Censer",
              RelicTier::Build,
              "Each summoner may keep one extra summoned unit alive.",
              {"summon", "caster", "build"},
              18,
              makeModifiers(0, 0, 0, 0, 0, 0, 0, {0, 0, 1, 0, 0}, 1)),
        relic("piercer_pin",
              "Piercer Pin",
              RelicTier::Build,
              "Assassin and Artillery drops appear more often. Buy costs -1.",
              {"assassin", "artillery", "build", "damage"},
              18,
              makeModifiers(1, 0, 0, 0, 0, 0, 0, {0, 0, 0, 1, 1})),
        relic("camp_standard",
              "Camp Standard",
              RelicTier::Unique,
              "Roster capacity +3. Round income +1.",
              {"shop", "economy", "unique"},
              8,
              makeModifiers(0, 0, 1, 0, 3),
              true,
              false),
    };
    return catalog;
}

int familyForUnit(UnitType type) {
    switch (type) {
        case UnitType::Skeleton:
        case UnitType::SkeletonByNecromancer:
        case UnitType::GoblinSkirmisher:
        case UnitType::ImpSwarm:
            return familyIndex(NeutralFamily::Swarm);
        case UnitType::Barbarian:
        case UnitType::Paladin:
        case UnitType::ShieldGuardian:
        case UnitType::NeutralOwlbear:
        case UnitType::NeutralSovereignSpaw:
            return familyIndex(NeutralFamily::Guardian);
        case UnitType::Necromancer:
        case UnitType::Cleric:
        case UnitType::Evoker:
        case UnitType::Druid:
        case UnitType::Treant:
        case UnitType::SporeServant:
        case UnitType::NeutralSpectator:
        case UnitType::NeutralMindFlayer:
            return familyIndex(NeutralFamily::Caster);
        case UnitType::GithyankiWarrior:
        case UnitType::RogueAssassin:
        case UnitType::NeutralKarniss:
        case UnitType::NeutralRedcap:
            return familyIndex(NeutralFamily::Assassin);
        case UnitType::Ranger:
        case UnitType::FireMephit:
        case UnitType::DragonWyrmling:
            return familyIndex(NeutralFamily::Artillery);
    }
    return familyIndex(NeutralFamily::Swarm);
}

std::string dominantFamilyTag(const BuildProfile& profile) {
    int bestIndex = 0;
    for (int i = 1; i < static_cast<int>(profile.familyCounts.size()); ++i) {
        if (profile.familyCounts[static_cast<size_t>(i)] > profile.familyCounts[static_cast<size_t>(bestIndex)]) {
            bestIndex = i;
        }
    }
    return familyTag(static_cast<NeutralFamily>(bestIndex));
}

} // namespace

BuildProfile analyzeBuild(const GameSnapshot& snapshot, PlayerId player) {
    BuildProfile profile;
    for (const UnitView& unit : snapshot.units) {
        if (unit.owner != player || !unit.alive || isInternalUnit(unit.type)) continue;
        ++profile.totalUnits;
        profile.totalThreat += unit.attack + unit.armorClass + unit.maxTotalHp / 25;
        int family = familyForUnit(unit.type);
        profile.familyCounts[static_cast<size_t>(family)] += 1;
        if (family == familyIndex(NeutralFamily::Swarm)) ++profile.swarmCount;
        if (family == familyIndex(NeutralFamily::Guardian)) ++profile.guardianCount;
        if (family == familyIndex(NeutralFamily::Caster)) ++profile.casterCount;
        if (family == familyIndex(NeutralFamily::Artillery)) ++profile.artilleryCount;

        if (unit.layer == UnitLayer::Air) ++profile.airCount;
        if (unit.range >= 3) ++profile.rangedCount;
        if (unit.armorClass >= 17 || unit.maxTotalHp >= 160) ++profile.tankCount;
        if (unit.ability == AbilityKind::ClericHeal || unit.ability == AbilityKind::DruidSummon ||
            unit.ability == AbilityKind::AnimatingSpores) {
            ++profile.supportCount;
        }
        if (unit.ability == AbilityKind::FrostNova || unit.ability == AbilityKind::DragonBreath ||
            unit.ability == AbilityKind::MindBlast || unit.ability == AbilityKind::SpectatorWoundingRay ||
            unit.ability == AbilityKind::EvokerMagicMissile || unit.ability == AbilityKind::BossFireball) {
            ++profile.controlCount;
        }
        if (unit.ability == AbilityKind::RogueAmbush || unit.type == UnitType::GithyankiWarrior) {
            ++profile.assassinCount;
            ++profile.familyCounts[familyIndex(NeutralFamily::Assassin)];
        }
        if (unit.range <= 1) ++profile.meleeCount;
        if (unit.units > 1) ++profile.swarmCount;

        int forward = player == PlayerId::One ? unit.coord.x : (snapshot.width - 1 - unit.coord.x);
        if (forward >= 2) profile.frontlinePressure += unit.attack + unit.armorClass;
        if (unit.range >= 3 || unit.ability == AbilityKind::ClericHeal) {
            profile.backlinePressure += unit.attack + unit.spellSaveDc;
        }
    }
    return profile;
}

const RelicSpec* relicById(const std::string& id) {
    const auto& catalog = catalogRef();
    auto it = std::find_if(catalog.begin(), catalog.end(), [&](const RelicSpec& relicSpec) {
        return relicSpec.id == id;
    });
    return it == catalog.end() ? nullptr : &*it;
}

std::vector<RelicSpec> relicCatalog() {
    return catalogRef();
}

std::vector<RelicSpec> relicPoolForFamily(NeutralFamily family) {
    std::string tag = familyTag(family);
    std::vector<RelicSpec> pool;
    for (const RelicSpec& relicSpec : catalogRef()) {
        if (hasTag(relicSpec, tag) || hasTag(relicSpec, "basic") ||
            hasTag(relicSpec, "economy") || hasTag(relicSpec, "choice")) {
            pool.push_back(relicSpec);
        }
    }
    return pool;
}

RunModifiers runModifiersForRelic(const RelicSpec& relicSpec) {
    RunModifiers modifiers = relicSpec.modifiers;
    modifiers.relicIds.push_back(relicSpec.id);
    return modifiers;
}

RunModifiers runModifiersForRelics(const std::vector<std::string>& relicIds) {
    RunModifiers total;
    std::unordered_set<std::string> seenUnique;
    for (const std::string& id : relicIds) {
        const RelicSpec* relicSpec = relicById(id);
        if (!relicSpec) continue;
        if ((relicSpec->unique || !relicSpec->stackable) && seenUnique.count(id) > 0) continue;
        if (relicSpec->unique || !relicSpec->stackable) seenUnique.insert(id);
        RunModifiers current = runModifiersForRelic(*relicSpec);
        total.costDiscount += current.costDiscount;
        total.upgradeDiscount += current.upgradeDiscount;
        total.roundIncomeBonus += current.roundIncomeBonus;
        total.interestBonus += current.interestBonus;
        total.benchBonus += current.benchBonus;
        total.extraRelicChoices += current.extraRelicChoices;
        total.bonusGoldOnClear += current.bonusGoldOnClear;
        total.summonLimitBonus += current.summonLimitBonus;
        total.hiddenEventRevealBonus += current.hiddenEventRevealBonus;
        total.hiddenEventPayoutBonus += current.hiddenEventPayoutBonus;
        total.trapCheckBonus += current.trapCheckBonus;
        total.roundStartShield += current.roundStartShield;
        total.eventHealBonus += current.eventHealBonus;
        total.hiddenEventCountBonus += current.hiddenEventCountBonus;
        total.trapDisarmGoldBonus += current.trapDisarmGoldBonus;
        total.cursedIdolPayoutBonus += current.cursedIdolPayoutBonus;
        total.cursedIdolDamageReductionPercent += current.cursedIdolDamageReductionPercent;
        total.rallyDurationBonus += current.rallyDurationBonus;
        total.smugglerAlwaysSucceeds = total.smugglerAlwaysSucceeds || current.smugglerAlwaysSucceeds;
        total.healingSpringCleanses = total.healingSpringCleanses || current.healingSpringCleanses;
        for (size_t i = 0; i < total.familyBias.size(); ++i) {
            total.familyBias[i] += current.familyBias[i];
        }
        total.relicIds.push_back(id);
    }
    return total;
}

bool relicDropEligibleForObjective(ExplorationObjectiveKind kind, UnitType type) {
    if (type == UnitType::NeutralRedcap) return false;
    return kind == ExplorationObjectiveKind::Camp ||
           kind == ExplorationObjectiveKind::Elite ||
           kind == ExplorationObjectiveKind::Boss;
}

bool relicDropsForObjectiveClear(ExplorationObjectiveKind kind, UnitType type, unsigned seed) {
    if (!relicDropEligibleForObjective(kind, type)) return false;
    if (kind == ExplorationObjectiveKind::Elite || kind == ExplorationObjectiveKind::Boss) return true;
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> coin(0, 1);
    return coin(rng) == 1;
}

std::string neutralFamilyLabel(NeutralFamily family) {
    switch (family) {
        case NeutralFamily::Swarm: return "Swarm";
        case NeutralFamily::Guardian: return "Guardian";
        case NeutralFamily::Caster: return "Caster";
        case NeutralFamily::Assassin: return "Assassin";
        case NeutralFamily::Artillery: return "Artillery";
    }
    return "Swarm";
}

std::string relicTierLabel(RelicTier tier) {
    switch (tier) {
        case RelicTier::Basic: return "Basic";
        case RelicTier::Build: return "Build";
        case RelicTier::Transform: return "Transform";
        case RelicTier::Unique: return "Unique";
    }
    return "Basic";
}

std::string relicSummaryLine(const RelicSpec& relicSpec) {
    std::ostringstream out;
    out << relicSpec.name << " [" << relicTierLabel(relicSpec.tier) << "] "
        << relicSpec.summary;
    return out.str();
}

} // namespace autochess
