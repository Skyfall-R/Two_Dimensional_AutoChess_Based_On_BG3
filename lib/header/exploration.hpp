#ifndef AUTOCHESS_EXPLORATION_HPP
#define AUTOCHESS_EXPLORATION_HPP

#include <cstddef>
#include <optional>
#include <random>
#include <vector>

namespace autochess {

struct ExplorationObjectiveState {
    Coord coord;
    ExplorationObjectiveKind kind = ExplorationObjectiveKind::Camp;
    UnitType type = UnitType::NeutralSpectator;
    PlayerId owner = PlayerId::One;
    UnitId unitId = kInvalidUnitId;
    std::vector<UnitId> spawnedUnitIds;
    bool cleared = false;
    bool revealed = false;
    bool triggered = false;
    int rewardGold = 0;
    int rewardQuality = 0;
};

struct NeutralCampState {
    Coord coord;
    UnitType type = UnitType::NeutralSpectator;
    PlayerId owner = PlayerId::One;
    UnitId unitId = kInvalidUnitId;
};

struct TrapState {
    Coord coord;
    PlayerId side = PlayerId::One;
    bool triggered = false;
};

struct HiddenExplorationEventState {
    Coord coord;
    HiddenExplorationEventKind kind = HiddenExplorationEventKind::GoldCache;
    int amount = 0;
    int rewardGold = 0;
    bool claimed = false;
    PlayerId claimedBy = PlayerId::One;
    UnitId claimedByUnit = kInvalidUnitId;
};

using RandomGoldEventState = HiddenExplorationEventState;

struct ExplorationStats {
    int objectivesCleared = 0;
    int objectivesTotal = 0;
    int bossesCleared = 0;
    int eventsTriggered = 0;
    int trapsTriggered = 0;
    int hiddenEventsClaimed = 0;
    int randomGoldEventsClaimed = 0;
    int randomGoldEventsTotal = 0;
};

bool isCombatExplorationObjective(ExplorationObjectiveKind kind);

class ExplorationState {
public:
    void clear();
    void resetObjectives(int templateIndex);
    void resetHiddenEvents(std::vector<Coord> candidates, std::mt19937& rng);
    void rebuildObjectiveIndexes();

    ExplorationStats stats() const;
    std::vector<Coord> randomGoldCoords() const;
    std::vector<Coord> hiddenHealingCoords() const;
    std::optional<HiddenExplorationEventState> claimHiddenEvent(PlayerId player, Coord coord, UnitId unitId);
    std::optional<RandomGoldEventState> claimRandomGold(PlayerId player, Coord coord, UnitId unitId);

    const std::vector<ExplorationObjectiveState>& objectives() const;
    const std::vector<NeutralCampState>& neutralCamps() const;
    const std::vector<TrapState>& traps() const;

    std::optional<size_t> findTriggerableTrap(Coord coord) const;
    std::optional<size_t> findObjectiveForUnit(UnitId unitId) const;
    std::optional<size_t> findTrapForSpawn(Coord origin) const;

    ExplorationObjectiveState* objective(size_t index);
    const ExplorationObjectiveState* objective(size_t index) const;
    void setObjectiveUnit(size_t index, UnitId unitId);
    void markTriggered(size_t index);
    void markCleared(size_t index);
    void recordTrapSpawn(size_t index, UnitId unitId);
    void clearTrapSpawns(size_t index);

private:
    std::vector<ExplorationObjectiveState> objectives_;
    std::vector<NeutralCampState> neutralCamps_;
    std::vector<TrapState> traps_;
    std::vector<HiddenExplorationEventState> hiddenEvents_;
};

} // namespace autochess

#endif
