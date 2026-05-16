#ifndef AUTOCHESS_CAMPAIGN_HPP
#define AUTOCHESS_CAMPAIGN_HPP

#include <array>
#include <string>
#include <vector>

#include <lib.hpp>

namespace autochess {

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

BuildProfile analyzeBuild(const GameSnapshot& snapshot, PlayerId player);

const RelicSpec* relicById(const std::string& id);
std::vector<RelicSpec> relicCatalog();
std::vector<RelicSpec> relicPoolForFamily(NeutralFamily family);

RunModifiers runModifiersForRelics(const std::vector<std::string>& relicIds);
RunModifiers runModifiersForRelic(const RelicSpec& relic);
bool relicDropEligibleForObjective(ExplorationObjectiveKind kind, UnitType type);
bool relicDropsForObjectiveClear(ExplorationObjectiveKind kind, UnitType type, unsigned seed);

std::string neutralFamilyLabel(NeutralFamily family);
std::string relicTierLabel(RelicTier tier);
std::string relicSummaryLine(const RelicSpec& relic);

} // namespace autochess

#endif
