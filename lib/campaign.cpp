#include <campaign.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <random>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace autochess {

namespace {

constexpr int kRouteWidth = 25;
constexpr int kRouteHeight = 15;
constexpr int kLaneCount = 3;

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

std::string join(const std::vector<std::string>& values, const char* separator) {
    std::ostringstream out;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) out << separator;
        out << values[i];
    }
    return out.str();
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
              "Future nodes pay +1 gold.",
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

        relic("mirror_contract",
              "Mirror Contract",
              RelicTier::Transform,
              "Relic drafts show one extra choice.",
              {"choice", "route", "transform"},
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
              "Future nodes pay +2 gold.",
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
              "Roster capacity +3. Future nodes pay +1 gold.",
              {"shop", "economy", "unique"},
              8,
              makeModifiers(0, 0, 1, 0, 3),
              true,
              false),
    };
    return catalog;
}

NeutralEncounterSpec encounterSpec(std::string id,
                                   std::string name,
                                   std::string sourceMonster,
                                   std::string model,
                                   std::string mechanic,
                                   std::string counterHint,
                                   std::vector<std::string> tags,
                                   int pressure) {
    NeutralEncounterSpec spec;
    spec.id = std::move(id);
    spec.name = std::move(name);
    spec.sourceMonster = std::move(sourceMonster);
    spec.model = std::move(model);
    spec.mechanic = std::move(mechanic);
    spec.counterHint = std::move(counterHint);
    spec.tags = std::move(tags);
    spec.pressure = pressure;
    return spec;
}

const std::vector<NeutralEncounterSpec>& encounterCatalogRef(NeutralFamily family) {
    static const std::array<std::vector<NeutralEncounterSpec>, 5> catalogs = {
        std::vector<NeutralEncounterSpec>{
            encounterSpec("goblin_warband",
                          "Goblin Warband",
                          "Goblin",
                          "small humanoid pack",
                          "Floods side lanes with expendable bodies and cheap focus fire.",
                          "AOE, durable frontline, early ranged cleanup",
                          {"wide-board", "focus-fire"},
                          2),
            encounterSpec("gnoll_hunter_pack",
                          "Gnoll Hunter Pack",
                          "Gnoll",
                          "blood-crazed raiders",
                          "Snowballs after a takedown and punishes isolated frontliners.",
                          "Tanks with support, slows, disciplined focus",
                          {"snowball", "isolation-punish"},
                          5),
            encounterSpec("phase_spider_brood",
                          "Phase Spider Brood",
                          "Phase Spider",
                          "teleporting spiderlings",
                          "Phase hops and hatchlings collapse onto exposed backlines.",
                          "AOE, anti-summon pressure, protected carries",
                          {"teleport", "backline-collapse"},
                          7),
        },
        std::vector<NeutralEncounterSpec>{
            encounterSpec("owlbear_matriarch",
                          "Owlbear Matriarch",
                          "Owlbear",
                          "single heavy bruiser",
                          "Leap and cleave pressure punish clumped melee boards.",
                          "Kite tools, slows, armor shred",
                          {"bruiser", "cleave"},
                          6),
            encounterSpec("bulette_burrower",
                          "Bulette Burrower",
                          "Bulette",
                          "burrowing charger",
                          "Burst engages from fog and threatens front-to-back knockups.",
                          "Spread formation, taunts, anti-charge tanks",
                          {"charge", "knockup"},
                          8),
            encounterSpec("steel_watcher",
                          "Steel Watcher",
                          "Steel Watcher",
                          "armored construct",
                          "High armor and a low-health failsafe punish slow clears.",
                          "Control, ranged focus, avoid overtime",
                          {"armor-check", "failsafe"},
                          10),
        },
        std::vector<NeutralEncounterSpec>{
            encounterSpec("spectator_raycaster",
                          "Spectator Raycaster",
                          "Spectator",
                          "beholder-kin eye monster",
                          "Eye rays punish stacked carries and greedy support clusters.",
                          "Spread, summons as bait, fast control",
                          {"eye-rays", "cluster-punish"},
                          9),
            encounterSpec("mind_flayer_psion",
                          "Mind Flayer Psion",
                          "Mind Flayer",
                          "illithid controller",
                          "Charm and stun checks target the carry line before damage lands.",
                          "Assassins, burst, high-save units",
                          {"control-check", "carry-threat"},
                          10),
            encounterSpec("hag_coven_trickster",
                          "Hag Coven Trickster",
                          "Hag",
                          "illusion caster",
                          "Decoys and curses turn tempo into a resource puzzle.",
                          "Reveal with pressure, sustained damage, flexible bench",
                          {"illusion", "tempo-curse"},
                          7),
        },
        std::vector<NeutralEncounterSpec>{
            encounterSpec("githyanki_raider",
                          "Githyanki Raider",
                          "Githyanki",
                          "astral skirmisher",
                          "Astral jumps force quick repositioning around ranged units.",
                          "Peel tanks, taunt, hard control",
                          {"dive", "reposition-check"},
                          6),
            encounterSpec("meazel_nightstalker",
                          "Meazel Nightstalker",
                          "Meazel",
                          "shadow ambusher",
                          "Pulls isolated units into kill zones before the main clash.",
                          "Keep formation tight, punish backline dives",
                          {"pull", "ambush"},
                          8),
            encounterSpec("vampire_spawn",
                          "Vampire Spawn",
                          "Vampire Spawn",
                          "life-steal duelist",
                          "Sustains through weak focus and resets if it finds a kill.",
                          "Burst, anti-heal pressure, layered frontline",
                          {"lifesteal", "reset-threat"},
                          9),
        },
        std::vector<NeutralEncounterSpec>{
            encounterSpec("hook_horror_screecher",
                          "Hook Horror Screecher",
                          "Hook Horror",
                          "sonic cave predator",
                          "Shrieks break clustered supports before the melee line arrives.",
                          "Split supports, silence, fast engage",
                          {"sonic-shriek", "support-punish"},
                          6),
            encounterSpec("merregon_legionnaire",
                          "Merregon Legionnaire",
                          "Merregon",
                          "hell-forged volley line",
                          "Long-range volleys punish slow and greedy economy boards.",
                          "Assassin flank, fire-resistant tanks, quick tempo",
                          {"volley", "range-check"},
                          8),
            encounterSpec("sahuagin_tidehunter",
                          "Sahuagin Tidehunter",
                          "Sahuagin",
                          "net-and-spear hunter",
                          "Nets pin carries while spears punish light and airborne units.",
                          "Cleanse or control, sturdier carries, anti-net spacing",
                          {"nets", "anti-air"},
                          7),
        },
    };
    return catalogs[static_cast<size_t>(family)];
}

bool routeTypeHasMonster(RouteNodeType type) {
    return type != RouteNodeType::Shop && type != RouteNodeType::Event;
}

NeutralEncounterSpec encounterFor(NeutralFamily family,
                                  RouteNodeType type,
                                  int depth,
                                  int lane,
                                  std::mt19937& rng) {
    if (!routeTypeHasMonster(type)) return {};
    const std::vector<NeutralEncounterSpec>& catalog = encounterCatalogRef(family);
    if (catalog.empty()) return {};
    int typeBias = type == RouteNodeType::Boss ? 2 : (type == RouteNodeType::Elite ? 1 : 0);
    int roll = std::uniform_int_distribution<int>(0, static_cast<int>(catalog.size()) - 1)(rng);
    int index = (depth + lane * 2 + typeBias + roll) % static_cast<int>(catalog.size());
    return catalog[static_cast<size_t>(index)];
}

void appendUnique(std::vector<std::string>& tags, const std::vector<std::string>& extra) {
    for (const std::string& tag : extra) {
        if (std::find(tags.begin(), tags.end(), tag) == tags.end()) tags.push_back(tag);
    }
}

NeutralFamily familyFromRoll(std::mt19937& rng, int depth, int lane) {
    std::array<NeutralFamily, 5> families = {
        NeutralFamily::Swarm,
        NeutralFamily::Guardian,
        NeutralFamily::Caster,
        NeutralFamily::Assassin,
        NeutralFamily::Artillery
    };
    int base = (depth * 2 + lane * 3) % static_cast<int>(families.size());
    std::uniform_int_distribution<int> drift(0, static_cast<int>(families.size()) - 1);
    return families[(base + drift(rng)) % families.size()];
}

std::vector<std::array<int, kLaneCount>> routeColumnsForMap(std::mt19937& rng) {
    std::vector<std::array<int, kLaneCount>> rows;
    rows.reserve(kRouteHeight);
    int left = std::uniform_int_distribution<int>(3, 7)(rng);
    for (int depth = 0; depth < kRouteHeight; ++depth) {
        int center = kRouteWidth / 2;
        left = std::clamp(left, 2, center - 3);
        int right = kRouteWidth - 1 - left;
        rows.push_back({left, center, right});

        int drift = std::uniform_int_distribution<int>(-2, 2)(rng);
        if (depth == 3 || depth == 8 || depth == 12) {
            drift += left < 6 ? 2 : -2;
        }
        left = std::clamp(left + drift, 2, center - 3);
    }
    return rows;
}

RouteNodeType mirroredTypeForDepth(int depth, int lane, std::mt19937& rng) {
    if (depth >= kRouteHeight - 1) return RouteNodeType::Boss;
    if (lane == 1) {
        if (depth == 2 || depth == 8 || depth == 13) return RouteNodeType::Shop;
        if (depth == 5 || depth == 10) return RouteNodeType::Event;
        if (depth == 12) return RouteNodeType::Elite;
    }

    int roll = std::uniform_int_distribution<int>(0, 99)(rng);
    if (depth <= 2) {
        if (roll < 50) return RouteNodeType::Combat;
        if (roll < 86) return RouteNodeType::Neutral;
        return RouteNodeType::Event;
    }
    if (depth <= 7) {
        if (roll < 34) return RouteNodeType::Combat;
        if (roll < 68) return RouteNodeType::Neutral;
        if (roll < 84) return RouteNodeType::Elite;
        if (roll < 93) return RouteNodeType::Shop;
        return RouteNodeType::Event;
    }
    if (depth <= 12) {
        if (roll < 24) return RouteNodeType::Combat;
        if (roll < 58) return RouteNodeType::Neutral;
        if (roll < 82) return RouteNodeType::Elite;
        if (roll < 91) return RouteNodeType::Shop;
        return RouteNodeType::Event;
    }
    return roll < 65 ? RouteNodeType::Elite : RouteNodeType::Neutral;
}

std::vector<int> routeChoicesForDepthImpl(const RouteMap& map, int depth, int sourceNodeId) {
    std::vector<int> choices;
    if (depth < 0 || depth >= static_cast<int>(map.rows.size())) return choices;

    if (depth == 0) {
        if (map.startNodeId >= 0) choices.push_back(map.startNodeId);
        return choices;
    }
    if (sourceNodeId < 0 || sourceNodeId >= static_cast<int>(map.nodes.size())) {
        return map.rows[static_cast<size_t>(depth)];
    }

    const RouteNode& source = map.nodes[static_cast<size_t>(sourceNodeId)];
    for (int nextId : source.outgoing) {
        if (nextId < 0 || nextId >= static_cast<int>(map.nodes.size())) continue;
        const RouteNode& next = map.nodes[static_cast<size_t>(nextId)];
        if (next.depth == depth) choices.push_back(nextId);
    }

    if (choices.empty()) {
        choices = map.rows[static_cast<size_t>(depth)];
    }
    return choices;
}

int defaultRouteNodeForDepthImpl(const RouteMap& map, int depth, int sourceNodeId) {
    std::vector<int> choices = routeChoicesForDepthImpl(map, depth, sourceNodeId);
    if (choices.empty()) return -1;
    if (depth == 0) {
        return choices.front();
    }
    if (sourceNodeId < 0 || sourceNodeId >= static_cast<int>(map.nodes.size())) {
        for (int choice : choices) {
            const RouteNode& node = map.nodes[static_cast<size_t>(choice)];
            if (node.lane == 1) return choice;
        }
        return choices.front();
    }

    const RouteNode& source = map.nodes[static_cast<size_t>(sourceNodeId)];
    int bestChoice = choices.front();
    int bestDistance = std::numeric_limits<int>::max();
    for (int choice : choices) {
        const RouteNode& node = map.nodes[static_cast<size_t>(choice)];
        int distance = std::abs(node.lane - source.lane);
        if (distance < bestDistance || (distance == bestDistance && node.lane == source.lane)) {
            bestChoice = choice;
            bestDistance = distance;
        }
    }
    return bestChoice;
}

int typeThreatBonus(RouteNodeType type) {
    switch (type) {
        case RouteNodeType::Combat: return 6;
        case RouteNodeType::Neutral: return 12;
        case RouteNodeType::Elite: return 28;
        case RouteNodeType::Shop: return -8;
        case RouteNodeType::Event: return 0;
        case RouteNodeType::Boss: return 62;
    }
    return 0;
}

int typeRewardBonus(RouteNodeType type) {
    switch (type) {
        case RouteNodeType::Combat: return 4;
        case RouteNodeType::Neutral: return 8;
        case RouteNodeType::Elite: return 18;
        case RouteNodeType::Shop: return 14;
        case RouteNodeType::Event: return 6;
        case RouteNodeType::Boss: return 32;
    }
    return 0;
}

std::vector<std::string> riskTagsFor(RouteNodeType type, NeutralFamily family, int depth) {
    std::vector<std::string> tags;
    if (depth <= 3) tags.push_back("single-mechanic");
    else if (depth <= 9) tags.push_back("dual-mechanic");
    else tags.push_back("path-pressure");

    switch (type) {
        case RouteNodeType::Combat:
            tags.push_back("ai-counter");
            break;
        case RouteNodeType::Neutral:
            tags.push_back(familyTag(family) + "-family");
            tags.push_back("loot-telegraphed");
            break;
        case RouteNodeType::Elite:
            tags.push_back("tempo-tax");
            tags.push_back("terrain-punish");
            break;
        case RouteNodeType::Shop:
            tags.push_back("safe-low-threat");
            break;
        case RouteNodeType::Event:
            tags.push_back("choice-cost");
            break;
        case RouteNodeType::Boss:
            tags.push_back("double-pressure");
            tags.push_back("mirror-check");
            break;
    }
    return tags;
}

std::vector<std::string> rewardTagsFor(RouteNodeType type, NeutralFamily family, int quality) {
    std::vector<std::string> tags;
    tags.push_back(familyTag(family) + "-pool");
    tags.push_back("q" + std::to_string(quality));
    switch (type) {
        case RouteNodeType::Combat:
            tags.push_back("stable-gold");
            break;
        case RouteNodeType::Neutral:
            tags.push_back("3-choice-relic");
            break;
        case RouteNodeType::Elite:
            tags.push_back("fixed-drop-plus-choice");
            break;
        case RouteNodeType::Shop:
            tags.push_back("economy-pivot");
            break;
        case RouteNodeType::Event:
            tags.push_back("choice-tradeoff");
            break;
        case RouteNodeType::Boss:
            tags.push_back("unique-weight");
            break;
    }
    return tags;
}

std::vector<LootEntry> lootPoolFor(NeutralFamily family, RouteNodeType type, int quality) {
    std::vector<LootEntry> pool;
    std::string tag = familyTag(family);
    for (const RelicSpec& relicSpec : catalogRef()) {
        int weight = relicSpec.weight;
        if (hasTag(relicSpec, tag)) weight += 26;
        if (hasTag(relicSpec, "basic") && quality <= 1) weight += 10;
        if (hasTag(relicSpec, "build") && quality >= 2) weight += 12;
        if (hasTag(relicSpec, "transform") && quality >= 3) weight += 14;
        if (hasTag(relicSpec, "unique")) weight += type == RouteNodeType::Boss ? 42 : -20;
        if ((type == RouteNodeType::Shop || type == RouteNodeType::Event) &&
            (hasTag(relicSpec, "economy") || hasTag(relicSpec, "choice") || hasTag(relicSpec, "shop"))) {
            weight += 18;
        }
        if (type == RouteNodeType::Elite && (hasTag(relicSpec, "transform") || hasTag(relicSpec, "build"))) {
            weight += 12;
        }
        if (weight <= 0) continue;
        pool.push_back({relicSpec.id, weight, tag});
    }
    std::stable_sort(pool.begin(), pool.end(), [](const LootEntry& lhs, const LootEntry& rhs) {
        if (lhs.weight != rhs.weight) return lhs.weight > rhs.weight;
        return lhs.relicId < rhs.relicId;
    });
    return pool;
}

std::string titleFor(RouteNodeType type, NeutralFamily family, const NeutralEncounterSpec& encounter) {
    if (type == RouteNodeType::Boss) {
        return encounter.name.empty() ? "Boss: Mirror Sovereign" : "Boss: " + encounter.name;
    }
    if (!encounter.name.empty()) return encounter.name + " " + routeNodeTypeLabel(type);
    return neutralFamilyLabel(family) + " " + routeNodeTypeLabel(type);
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
        case UnitType::DefenseTower:
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

bool ownedRelic(const std::vector<std::string>& ids, const std::string& id) {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

} // namespace

RouteMap generateRouteMap(unsigned seed) {
    RouteMap map;
    map.width = kRouteWidth;
    map.height = kRouteHeight;
    map.seed = seed;
    map.rows.resize(kRouteHeight);
    std::mt19937 rng(seed);
    std::vector<std::array<int, kLaneCount>> routeColumns = routeColumnsForMap(rng);

    for (int depth = 0; depth < kRouteHeight; ++depth) {
        RouteNodeType mirroredSideType = mirroredTypeForDepth(depth, 0, rng);
        NeutralFamily mirroredSideFamily = familyFromRoll(rng, depth, 0);
        NeutralEncounterSpec mirroredSideEncounter =
            encounterFor(mirroredSideFamily, mirroredSideType, depth, 0, rng);
        for (int lane = 0; lane < kLaneCount; ++lane) {
            RouteNode node;
            node.id = static_cast<int>(map.nodes.size());
            node.depth = depth;
            node.lane = lane;
            node.coord = {routeColumns[static_cast<size_t>(depth)][static_cast<size_t>(lane)], depth};
            node.mirrorCoord = {kRouteWidth - 1 - node.coord.x, depth};
            node.mirrored = lane != 1;
            if (lane == 1) {
                node.type = mirroredTypeForDepth(depth, lane, rng);
                node.family = familyFromRoll(rng, depth, lane);
                node.encounter = encounterFor(node.family, node.type, depth, lane, rng);
            } else {
                node.type = mirroredSideType;
                node.family = mirroredSideFamily;
                node.encounter = mirroredSideEncounter;
            }
            node.boss = node.type == RouteNodeType::Boss;

            int baseThreat = 18 + depth * 6;
            int baseReward = 6 + depth * 2;
            node.threatBudget = std::max(0, baseThreat + typeThreatBonus(node.type) + node.encounter.pressure);
            node.rewardGold = std::max(0, baseReward + typeRewardBonus(node.type));
            node.rewardQuality = std::min(4, 1 + depth / 4 +
                                                  (node.type == RouteNodeType::Elite ? 1 : 0) +
                                                  (node.type == RouteNodeType::Boss ? 2 : 0));
            node.relicDraftCount = node.type == RouteNodeType::Shop || node.type == RouteNodeType::Event ? 2 : 3;
            node.riskTags = riskTagsFor(node.type, node.family, depth);
            appendUnique(node.riskTags, node.encounter.tags);
            node.rewardTags = rewardTagsFor(node.type, node.family, node.rewardQuality);
            node.lootPool = lootPoolFor(node.family, node.type, node.rewardQuality);
            node.title = titleFor(node.type, node.family, node.encounter);
            node.subtitle = routeNodePreviewSubtitle(node);

            map.rows[static_cast<size_t>(depth)].push_back(node.id);
            map.nodes.push_back(std::move(node));
        }
    }

    for (int depth = 0; depth < kRouteHeight - 1; ++depth) {
        std::vector<int> nextIncoming(map.rows[static_cast<size_t>(depth + 1)].size(), 0);
        for (int nodeId : map.rows[static_cast<size_t>(depth)]) {
            RouteNode& node = map.nodes[static_cast<size_t>(nodeId)];
            std::vector<int> nextIds;
            for (int nextId : map.rows[static_cast<size_t>(depth + 1)]) {
                const RouteNode& next = map.nodes[static_cast<size_t>(nextId)];
                if (std::abs(next.lane - node.lane) <= 1) nextIds.push_back(nextId);
            }
            std::stable_sort(nextIds.begin(), nextIds.end(), [&](int lhs, int rhs) {
                const RouteNode& lhsNode = map.nodes[static_cast<size_t>(lhs)];
                const RouteNode& rhsNode = map.nodes[static_cast<size_t>(rhs)];
                int lhsDistance = std::abs(lhsNode.coord.x - node.coord.x) + std::abs(lhsNode.lane - node.lane) * 2;
                int rhsDistance = std::abs(rhsNode.coord.x - node.coord.x) + std::abs(rhsNode.lane - node.lane) * 2;
                if (lhsDistance != rhsDistance) return lhsDistance < rhsDistance;
                return lhsNode.lane < rhsNode.lane;
            });

            int edgeCount = depth <= 2 ? 2 : (std::uniform_int_distribution<int>(0, 99)(rng) < 58 ? 2 : 1);
            edgeCount = std::min(edgeCount, static_cast<int>(nextIds.size()));
            for (int i = 0; i < edgeCount; ++i) {
                node.outgoing.push_back(nextIds[static_cast<size_t>(i)]);
                auto rowIt = std::find(map.rows[static_cast<size_t>(depth + 1)].begin(),
                                       map.rows[static_cast<size_t>(depth + 1)].end(),
                                       nextIds[static_cast<size_t>(i)]);
                if (rowIt != map.rows[static_cast<size_t>(depth + 1)].end()) {
                    ++nextIncoming[static_cast<size_t>(std::distance(map.rows[static_cast<size_t>(depth + 1)].begin(), rowIt))];
                }
            }
        }

        for (size_t nextIndex = 0; nextIndex < nextIncoming.size(); ++nextIndex) {
            if (nextIncoming[nextIndex] > 0) continue;
            int nextId = map.rows[static_cast<size_t>(depth + 1)][nextIndex];
            const RouteNode& next = map.nodes[static_cast<size_t>(nextId)];
            int bestSource = map.rows[static_cast<size_t>(depth)].front();
            int bestDistance = std::numeric_limits<int>::max();
            for (int sourceId : map.rows[static_cast<size_t>(depth)]) {
                const RouteNode& source = map.nodes[static_cast<size_t>(sourceId)];
                if (std::abs(source.lane - next.lane) > 1) continue;
                int distance = std::abs(source.coord.x - next.coord.x) + std::abs(source.lane - next.lane) * 2;
                if (distance < bestDistance) {
                    bestSource = sourceId;
                    bestDistance = distance;
                }
            }
            RouteNode& source = map.nodes[static_cast<size_t>(bestSource)];
            if (std::find(source.outgoing.begin(), source.outgoing.end(), nextId) == source.outgoing.end()) {
                source.outgoing.push_back(nextId);
            }
        }
    }

    map.startNodeId = map.rows.empty() || map.rows[0].size() < 2 ? -1 : map.rows[0][1];
    map.currentNodeId = map.startNodeId;
    map.currentDepth = 0;
    return map;
}

std::vector<int> routeChoicesForDepth(const RouteMap& map, int depth, int sourceNodeId) {
    return routeChoicesForDepthImpl(map, depth, sourceNodeId);
}

int defaultRouteNodeForDepth(const RouteMap& map, int depth, int sourceNodeId) {
    return defaultRouteNodeForDepthImpl(map, depth, sourceNodeId);
}

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
            unit.ability == AbilityKind::EvokerMagicMissile) {
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

std::vector<NeutralEncounterSpec> neutralEncounterCatalog(NeutralFamily family) {
    return encounterCatalogRef(family);
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

std::vector<DraftOffer> draftRelicsForNode(const RouteNode& node,
                                          const BuildProfile& playerBuild,
                                          const std::vector<std::string>& ownedRelics,
                                          unsigned seed,
                                          int choiceCount) {
    std::vector<DraftOffer> candidates;
    std::string dominant = dominantFamilyTag(playerBuild);
    for (const LootEntry& entry : node.lootPool) {
        const RelicSpec* spec = relicById(entry.relicId);
        if (!spec) continue;
        if ((spec->unique || !spec->stackable) && ownedRelic(ownedRelics, spec->id)) continue;

        int weight = entry.weight;
        if (hasTag(*spec, dominant)) weight += 12;
        if (node.rewardQuality >= 3 && spec->tier == RelicTier::Transform) weight += 12;
        if (node.type == RouteNodeType::Boss && spec->tier == RelicTier::Unique) weight += 40;
        if (playerBuild.totalUnits == 0 && hasTag(*spec, "economy")) weight += 10;
        if (weight <= 0) continue;
        candidates.push_back({*spec, weight, entry.tag + " pool"});
    }

    std::vector<DraftOffer> result;
    if (candidates.empty()) return result;
    std::mt19937 rng(seed ^ (static_cast<unsigned>(node.id) * 2654435761u));
    choiceCount = std::max(1, std::min(choiceCount, static_cast<int>(candidates.size())));
    for (int i = 0; i < choiceCount; ++i) {
        int total = 0;
        for (const DraftOffer& offer : candidates) total += std::max(1, offer.weight);
        std::uniform_int_distribution<int> pickDist(1, total);
        int pick = pickDist(rng);
        int cursor = 0;
        int chosen = 0;
        for (int j = 0; j < static_cast<int>(candidates.size()); ++j) {
            cursor += std::max(1, candidates[static_cast<size_t>(j)].weight);
            if (pick <= cursor) {
                chosen = j;
                break;
            }
        }
        result.push_back(candidates[static_cast<size_t>(chosen)]);
        candidates.erase(candidates.begin() + chosen);
    }
    return result;
}

EncounterContext encounterContextForNode(const RouteNode& node) {
    EncounterContext context;
    context.active = true;
    context.nodeType = node.type;
    context.family = node.family;
    context.depth = node.depth;
    context.threatBudget = node.threatBudget;
    context.rewardGold = node.rewardGold;
    context.rewardQuality = node.rewardQuality;
    context.boss = node.boss;
    context.riskTags = node.riskTags;
    context.rewardTags = node.rewardTags;
    return context;
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
        for (size_t i = 0; i < total.familyBias.size(); ++i) {
            total.familyBias[i] += current.familyBias[i];
        }
        total.relicIds.push_back(id);
    }
    return total;
}

std::string routeNodeTypeLabel(RouteNodeType type) {
    switch (type) {
        case RouteNodeType::Combat: return "Battle";
        case RouteNodeType::Neutral: return "Neutral";
        case RouteNodeType::Elite: return "Elite";
        case RouteNodeType::Shop: return "Shop";
        case RouteNodeType::Event: return "Event";
        case RouteNodeType::Boss: return "Boss";
    }
    return "Battle";
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

std::string routeNodePreviewTitle(const RouteNode& node) {
    return node.title.empty() ? titleFor(node.type, node.family, node.encounter) : node.title;
}

std::string routeNodePreviewSubtitle(const RouteNode& node) {
    if (!node.subtitle.empty()) return node.subtitle;
    if (!node.encounter.name.empty()) {
        switch (node.type) {
            case RouteNodeType::Combat:
                return "AI skirmish modeled after " + node.encounter.sourceMonster + ": " +
                       node.encounter.mechanic;
            case RouteNodeType::Neutral:
                return node.encounter.sourceMonster + " encounter: " + node.encounter.mechanic;
            case RouteNodeType::Elite:
                return "Elite " + node.encounter.model + " with a higher threat budget: " +
                       node.encounter.mechanic;
            case RouteNodeType::Boss:
                return "Boss pressure based on " + node.encounter.sourceMonster + ": " +
                       node.encounter.mechanic;
            case RouteNodeType::Shop:
            case RouteNodeType::Event:
                break;
        }
    }
    switch (node.type) {
        case RouteNodeType::Combat:
            return "Mirror-fair AI battle; risk is readable and driven by counter-build pressure.";
        case RouteNodeType::Neutral:
            return "Family encounter with visible loot weights and a 3-choice relic draft.";
        case RouteNodeType::Elite:
            return "Higher threat budget, tempo tax, and a stronger fixed-drop supplement.";
        case RouteNodeType::Shop:
            return "Low-threat economy pivot; good when the current build needs money or bench space.";
        case RouteNodeType::Event:
            return "Choice tradeoff with visible reward tags but unstable opportunity cost.";
        case RouteNodeType::Boss:
            return "Final pressure node; unique relic odds rise and the AI doubles down on counters.";
    }
    return "";
}

std::string routeNodeEncounterSummary(const RouteNode& node) {
    if (node.encounter.name.empty()) return "";
    std::ostringstream out;
    out << node.encounter.name << " | " << node.encounter.model
        << " | counter: " << node.encounter.counterHint;
    return out.str();
}

std::string routeNodeRiskSummary(const RouteNode& node) {
    std::vector<std::string> tags = node.riskTags;
    if (tags.size() > 4) {
        int hidden = static_cast<int>(tags.size()) - 4;
        tags.resize(4);
        tags.push_back("+" + std::to_string(hidden));
    }
    std::ostringstream out;
    out << "Threat " << node.threatBudget << " | " << join(tags, ", ");
    return out.str();
}

std::string routeNodeRewardSummary(const RouteNode& node) {
    std::vector<std::string> tags = node.rewardTags;
    if (tags.size() > 4) {
        int hidden = static_cast<int>(tags.size()) - 4;
        tags.resize(4);
        tags.push_back("+" + std::to_string(hidden));
    }
    std::ostringstream out;
    out << "+" << node.rewardGold << "g"
        << " | tier " << node.rewardQuality
        << " | " << node.relicDraftCount << "-choice"
        << " | " << join(tags, ", ");
    return out.str();
}

std::vector<std::string> routeNodeDropSummary(const RouteNode& node) {
    std::vector<std::string> result;
    const int limit = std::min(5, static_cast<int>(node.lootPool.size()));
    for (int i = 0; i < limit; ++i) {
        const LootEntry& entry = node.lootPool[static_cast<size_t>(i)];
        const RelicSpec* relicSpec = relicById(entry.relicId);
        if (!relicSpec) continue;
        std::ostringstream line;
        line << entry.tag << " | " << relicSpec->name
             << " (" << relicTierLabel(relicSpec->tier) << ")";
        result.push_back(line.str());
    }
    return result;
}

std::string relicSummaryLine(const RelicSpec& relicSpec) {
    std::ostringstream out;
    out << relicSpec.name << " [" << relicTierLabel(relicSpec.tier) << "] "
        << relicSpec.summary;
    return out.str();
}

} // namespace autochess
