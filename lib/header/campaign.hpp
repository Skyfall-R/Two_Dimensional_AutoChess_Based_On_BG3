#ifndef AUTOCHESS_CAMPAIGN_HPP
#define AUTOCHESS_CAMPAIGN_HPP

#include <array>
#include <string>
#include <vector>

#include <lib.hpp>

namespace autochess {

struct LootEntry {
    std::string relicId;
    int weight = 0;
    std::string tag;
};

struct RelicSpec {
    std::string id;
    std::string name;
    RelicTier tier = RelicTier::Basic;
    std::string summary;
    std::vector<std::string> tags;
    int weight = 0;
    bool unique = false;
    bool stackable = true;
    RunModifiers modifiers;
};

struct DraftOffer {
    RelicSpec relic;
    int weight = 0;
    std::string reason;
};

struct NeutralEncounterSpec {
    std::string id;
    std::string name;
    std::string sourceMonster;
    std::string model;
    std::string mechanic;
    std::string counterHint;
    std::vector<std::string> tags;
    int pressure = 0;
};

struct RouteNode {
    int id = -1;
    int depth = 0;
    int lane = 0;
    Coord coord;
    Coord mirrorCoord;
    RouteNodeType type = RouteNodeType::Combat;
    NeutralFamily family = NeutralFamily::Swarm;
    int threatBudget = 0;
    int rewardGold = 0;
    int rewardQuality = 0;
    int relicDraftCount = 3;
    bool mirrored = true;
    bool boss = false;
    NeutralEncounterSpec encounter;
    std::string title;
    std::string subtitle;
    std::vector<std::string> riskTags;
    std::vector<std::string> rewardTags;
    std::vector<LootEntry> lootPool;
    std::vector<int> outgoing;
};

struct RouteMap {
    int width = 25;
    int height = 15;
    unsigned seed = 1;
    std::vector<RouteNode> nodes;
    std::vector<std::vector<int>> rows;
    int startNodeId = -1;
    int currentDepth = 0;
    int currentNodeId = -1;
};

struct BuildProfile {
    int totalUnits = 0;
    int totalThreat = 0;
    int tankCount = 0;
    int rangedCount = 0;
    int supportCount = 0;
    int controlCount = 0;
    int airCount = 0;
    int assassinCount = 0;
    int meleeCount = 0;
    int swarmCount = 0;
    int guardianCount = 0;
    int casterCount = 0;
    int artilleryCount = 0;
    int frontlinePressure = 0;
    int backlinePressure = 0;
    std::array<int, 5> familyCounts{};
};

RouteMap generateRouteMap(unsigned seed);
std::vector<int> routeChoicesForDepth(const RouteMap& map, int depth, int sourceNodeId);
int defaultRouteNodeForDepth(const RouteMap& map, int depth, int sourceNodeId);
BuildProfile analyzeBuild(const GameSnapshot& snapshot, PlayerId player);

const RelicSpec* relicById(const std::string& id);
std::vector<RelicSpec> relicCatalog();
std::vector<NeutralEncounterSpec> neutralEncounterCatalog(NeutralFamily family);
std::vector<RelicSpec> relicPoolForFamily(NeutralFamily family);
std::vector<DraftOffer> draftRelicsForNode(const RouteNode& node,
                                          const BuildProfile& playerBuild,
                                          const std::vector<std::string>& ownedRelics,
                                          unsigned seed,
                                          int choiceCount = 3);

RunModifiers runModifiersForRelics(const std::vector<std::string>& relicIds);
RunModifiers runModifiersForRelic(const RelicSpec& relic);
EncounterContext encounterContextForNode(const RouteNode& node);

std::string routeNodeTypeLabel(RouteNodeType type);
std::string neutralFamilyLabel(NeutralFamily family);
std::string relicTierLabel(RelicTier tier);
std::string routeNodePreviewTitle(const RouteNode& node);
std::string routeNodePreviewSubtitle(const RouteNode& node);
std::string routeNodeEncounterSummary(const RouteNode& node);
std::string routeNodeRiskSummary(const RouteNode& node);
std::string routeNodeRewardSummary(const RouteNode& node);
std::vector<std::string> routeNodeDropSummary(const RouteNode& node);
std::string relicSummaryLine(const RelicSpec& relic);

} // namespace autochess

#endif
