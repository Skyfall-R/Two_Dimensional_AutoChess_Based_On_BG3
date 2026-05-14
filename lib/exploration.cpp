#include <lib.hpp>

#include <algorithm>
#include <array>
#include <utility>

namespace autochess {

namespace {

constexpr int kRandomGoldMinCount = 1;
constexpr int kRandomGoldMaxCount = 3;
constexpr int kRandomGoldMinReward = 4;
constexpr int kRandomGoldMaxReward = 8;
constexpr int kHiddenHealMinCount = 1;
constexpr int kHiddenHealMaxCount = 2;
constexpr int kHiddenHealMinAmount = 18;
constexpr int kHiddenHealMaxAmount = 26;
constexpr int kExplorationTemplateCount = 2;
constexpr int kExplorationBaseObjectiveCount = 28;

struct ExplorationObjectiveTemplate {
    Coord coord;
    ExplorationObjectiveKind kind;
    UnitType type;
    int rewardGold;
    int rewardQuality;
};

constexpr std::array<ExplorationObjectiveTemplate, kExplorationBaseObjectiveCount> kExplorationObjectivesA = {
    ExplorationObjectiveTemplate{{6, 3}, ExplorationObjectiveKind::Camp, UnitType::NeutralSpectator, 8, 1},
    ExplorationObjectiveTemplate{{8, 15}, ExplorationObjectiveKind::Camp, UnitType::NeutralOwlbear, 8, 1},
    ExplorationObjectiveTemplate{{11, 4}, ExplorationObjectiveKind::Camp, UnitType::NeutralMinotaur, 8, 1},
    ExplorationObjectiveTemplate{{7, 12}, ExplorationObjectiveKind::Camp, UnitType::NeutralGuardianOfFaith, 8, 1},
    ExplorationObjectiveTemplate{{24, 3}, ExplorationObjectiveKind::Camp, UnitType::NeutralKarniss, 8, 1},
    ExplorationObjectiveTemplate{{26, 14}, ExplorationObjectiveKind::Camp, UnitType::NeutralSovereignSpaw, 8, 1},
    ExplorationObjectiveTemplate{{27, 6}, ExplorationObjectiveKind::Camp, UnitType::NeutralDeathKnight, 8, 1},
    ExplorationObjectiveTemplate{{24, 16}, ExplorationObjectiveKind::Camp, UnitType::NeutralAirMyrmidon, 8, 1},
    ExplorationObjectiveTemplate{{5, 8}, ExplorationObjectiveKind::Camp, UnitType::NeutralOwlbear, 8, 1},
    ExplorationObjectiveTemplate{{28, 8}, ExplorationObjectiveKind::Camp, UnitType::NeutralGuardianOfFaith, 8, 1},
    ExplorationObjectiveTemplate{{13, 5}, ExplorationObjectiveKind::Elite, UnitType::NeutralKarniss, 14, 2},
    ExplorationObjectiveTemplate{{12, 11}, ExplorationObjectiveKind::Elite, UnitType::NeutralSovereignSpaw, 14, 2},
    ExplorationObjectiveTemplate{{20, 6}, ExplorationObjectiveKind::Elite, UnitType::NeutralSpectator, 14, 2},
    ExplorationObjectiveTemplate{{21, 12}, ExplorationObjectiveKind::Elite, UnitType::NeutralMinotaur, 14, 2},
    ExplorationObjectiveTemplate{{17, 14}, ExplorationObjectiveKind::Elite, UnitType::NeutralDeathKnight, 16, 2},
    ExplorationObjectiveTemplate{{27, 12}, ExplorationObjectiveKind::Elite, UnitType::NeutralAirMyrmidon, 16, 2},
    ExplorationObjectiveTemplate{{16, 9}, ExplorationObjectiveKind::Boss, UnitType::NeutralMindFlayer, 24, 3},
    ExplorationObjectiveTemplate{{12, 7}, ExplorationObjectiveKind::Boss, UnitType::NeutralWaterMyrmidon, 24, 3},
    ExplorationObjectiveTemplate{{20, 9}, ExplorationObjectiveKind::Boss, UnitType::NeutralRaphael, 26, 3},
    ExplorationObjectiveTemplate{{15, 13}, ExplorationObjectiveKind::Boss, UnitType::NeutralKethericThorm, 26, 3},
    ExplorationObjectiveTemplate{{18, 5}, ExplorationObjectiveKind::Boss, UnitType::NeutralMoonlightSliver, 24, 3},
    ExplorationObjectiveTemplate{{11, 9}, ExplorationObjectiveKind::Boss, UnitType::NeutralPhaseSpiderMatriarch, 24, 3},
    ExplorationObjectiveTemplate{{22, 11}, ExplorationObjectiveKind::Boss, UnitType::NeutralTamiaHolzt, 28, 3},
    ExplorationObjectiveTemplate{{7, 5}, ExplorationObjectiveKind::Trap, UnitType::NeutralRedcap, 10, 1},
    ExplorationObjectiveTemplate{{25, 5}, ExplorationObjectiveKind::Trap, UnitType::NeutralRedcap, 10, 1},
    ExplorationObjectiveTemplate{{10, 13}, ExplorationObjectiveKind::Trap, UnitType::NeutralRedcap, 10, 1},
    ExplorationObjectiveTemplate{{23, 13}, ExplorationObjectiveKind::Trap, UnitType::NeutralRedcap, 10, 1},
    ExplorationObjectiveTemplate{{16, 6}, ExplorationObjectiveKind::Trap, UnitType::NeutralRedcap, 12, 2}
};

constexpr std::array<ExplorationObjectiveTemplate, kExplorationBaseObjectiveCount> kExplorationObjectivesB = {
    ExplorationObjectiveTemplate{{6, 4}, ExplorationObjectiveKind::Camp, UnitType::NeutralOwlbear, 8, 1},
    ExplorationObjectiveTemplate{{9, 14}, ExplorationObjectiveKind::Camp, UnitType::NeutralSpectator, 8, 1},
    ExplorationObjectiveTemplate{{10, 6}, ExplorationObjectiveKind::Camp, UnitType::NeutralGuardianOfFaith, 8, 1},
    ExplorationObjectiveTemplate{{7, 12}, ExplorationObjectiveKind::Camp, UnitType::NeutralMinotaur, 8, 1},
    ExplorationObjectiveTemplate{{24, 5}, ExplorationObjectiveKind::Camp, UnitType::NeutralSovereignSpaw, 8, 1},
    ExplorationObjectiveTemplate{{25, 13}, ExplorationObjectiveKind::Camp, UnitType::NeutralKarniss, 8, 1},
    ExplorationObjectiveTemplate{{27, 4}, ExplorationObjectiveKind::Camp, UnitType::NeutralDeathKnight, 8, 1},
    ExplorationObjectiveTemplate{{24, 16}, ExplorationObjectiveKind::Camp, UnitType::NeutralAirMyrmidon, 8, 1},
    ExplorationObjectiveTemplate{{9, 2}, ExplorationObjectiveKind::Camp, UnitType::NeutralMinotaur, 8, 1},
    ExplorationObjectiveTemplate{{23, 2}, ExplorationObjectiveKind::Camp, UnitType::NeutralSovereignSpaw, 8, 1},
    ExplorationObjectiveTemplate{{12, 14}, ExplorationObjectiveKind::Elite, UnitType::NeutralOwlbear, 14, 2},
    ExplorationObjectiveTemplate{{21, 6}, ExplorationObjectiveKind::Elite, UnitType::NeutralKarniss, 14, 2},
    ExplorationObjectiveTemplate{{15, 5}, ExplorationObjectiveKind::Elite, UnitType::NeutralGuardianOfFaith, 14, 2},
    ExplorationObjectiveTemplate{{18, 12}, ExplorationObjectiveKind::Elite, UnitType::NeutralSpectator, 14, 2},
    ExplorationObjectiveTemplate{{13, 16}, ExplorationObjectiveKind::Elite, UnitType::NeutralDeathKnight, 16, 2},
    ExplorationObjectiveTemplate{{23, 17}, ExplorationObjectiveKind::Elite, UnitType::NeutralAirMyrmidon, 16, 2},
    ExplorationObjectiveTemplate{{16, 9}, ExplorationObjectiveKind::Boss, UnitType::NeutralMindFlayer, 24, 3},
    ExplorationObjectiveTemplate{{12, 7}, ExplorationObjectiveKind::Boss, UnitType::NeutralWaterMyrmidon, 24, 3},
    ExplorationObjectiveTemplate{{20, 10}, ExplorationObjectiveKind::Boss, UnitType::NeutralRaphael, 26, 3},
    ExplorationObjectiveTemplate{{14, 11}, ExplorationObjectiveKind::Boss, UnitType::NeutralKethericThorm, 26, 3},
    ExplorationObjectiveTemplate{{18, 7}, ExplorationObjectiveKind::Boss, UnitType::NeutralMoonlightSliver, 24, 3},
    ExplorationObjectiveTemplate{{11, 10}, ExplorationObjectiveKind::Boss, UnitType::NeutralPhaseSpiderMatriarch, 24, 3},
    ExplorationObjectiveTemplate{{22, 8}, ExplorationObjectiveKind::Boss, UnitType::NeutralTamiaHolzt, 28, 3},
    ExplorationObjectiveTemplate{{11, 5}, ExplorationObjectiveKind::Trap, UnitType::NeutralRedcap, 10, 1},
    ExplorationObjectiveTemplate{{21, 4}, ExplorationObjectiveKind::Trap, UnitType::NeutralRedcap, 10, 1},
    ExplorationObjectiveTemplate{{9, 13}, ExplorationObjectiveKind::Trap, UnitType::NeutralRedcap, 10, 1},
    ExplorationObjectiveTemplate{{22, 12}, ExplorationObjectiveKind::Trap, UnitType::NeutralRedcap, 10, 1},
    ExplorationObjectiveTemplate{{16, 11}, ExplorationObjectiveKind::Trap, UnitType::NeutralRedcap, 12, 2}
};

int normalizedTemplateIndex(int templateIndex) {
    return ((templateIndex % kExplorationTemplateCount) + kExplorationTemplateCount) %
           kExplorationTemplateCount;
}

const std::array<ExplorationObjectiveTemplate, kExplorationBaseObjectiveCount>& objectiveTemplates(int templateIndex) {
    return normalizedTemplateIndex(templateIndex) == 1 ? kExplorationObjectivesB : kExplorationObjectivesA;
}

bool containsCoord(const std::vector<Coord>& coords, Coord coord) {
    return std::find(coords.begin(), coords.end(), coord) != coords.end();
}

bool tooCloseToExisting(const std::vector<HiddenExplorationEventState>& events, Coord coord) {
    return std::any_of(events.begin(), events.end(), [coord](const HiddenExplorationEventState& event) {
        return manhattan(event.coord, coord) <= 2;
    });
}

} // namespace

bool isCombatExplorationObjective(ExplorationObjectiveKind kind) {
    return kind == ExplorationObjectiveKind::Camp ||
           kind == ExplorationObjectiveKind::Elite ||
           kind == ExplorationObjectiveKind::Boss;
}

void ExplorationState::clear() {
    objectives_.clear();
    neutralCamps_.clear();
    traps_.clear();
    hiddenEvents_.clear();
}

void ExplorationState::resetObjectives(int templateIndex) {
    objectives_.clear();

    for (const ExplorationObjectiveTemplate& source : objectiveTemplates(templateIndex)) {
        ExplorationObjectiveState objective;
        objective.coord = source.coord;
        objective.kind = source.kind;
        objective.type = source.type;
        objective.owner = source.coord.x < kBoardWidth / 2 ? PlayerId::Two : PlayerId::One;
        objective.rewardGold = source.rewardGold;
        objective.rewardQuality = source.rewardQuality;

        objectives_.push_back(std::move(objective));
    }

    rebuildObjectiveIndexes();
}

void ExplorationState::rebuildObjectiveIndexes() {
    neutralCamps_.clear();
    traps_.clear();
    for (const ExplorationObjectiveState& objective : objectives_) {
        if (isCombatExplorationObjective(objective.kind)) {
            NeutralCampState camp;
            camp.coord = objective.coord;
            camp.type = objective.type;
            camp.owner = objective.owner;
            camp.unitId = objective.unitId;
            neutralCamps_.push_back(camp);
        } else if (objective.kind == ExplorationObjectiveKind::Trap) {
            traps_.push_back({objective.coord, objective.owner, objective.triggered});
        }
    }
}

void ExplorationState::resetHiddenEvents(std::vector<Coord> candidates, std::mt19937& rng) {
    hiddenEvents_.clear();
    if (candidates.empty()) return;

    std::shuffle(candidates.begin(), candidates.end(), rng);

    std::uniform_int_distribution<int> goldCountDist(kRandomGoldMinCount, kRandomGoldMaxCount);
    std::uniform_int_distribution<int> goldRewardDist(kRandomGoldMinReward, kRandomGoldMaxReward);
    std::uniform_int_distribution<int> healCountDist(kHiddenHealMinCount, kHiddenHealMaxCount);
    std::uniform_int_distribution<int> healAmountDist(kHiddenHealMinAmount, kHiddenHealMaxAmount);

    int goldCount = std::min(goldCountDist(rng), static_cast<int>(candidates.size()));
    int healCount = std::min(healCountDist(rng), std::max(0, static_cast<int>(candidates.size()) - goldCount));

    auto takeCandidate = [&](HiddenExplorationEventKind kind, int amount) {
        for (Coord coord : candidates) {
            if (std::any_of(hiddenEvents_.begin(), hiddenEvents_.end(),
                            [coord](const HiddenExplorationEventState& event) {
                                return event.coord == coord;
                            })) {
                continue;
            }
            if (tooCloseToExisting(hiddenEvents_, coord)) continue;
            HiddenExplorationEventState event;
            event.coord = coord;
            event.kind = kind;
            event.amount = amount;
            event.rewardGold = kind == HiddenExplorationEventKind::GoldCache ? amount : 0;
            hiddenEvents_.push_back(event);
            return true;
        }
        for (Coord coord : candidates) {
            if (std::any_of(hiddenEvents_.begin(), hiddenEvents_.end(),
                            [coord](const HiddenExplorationEventState& event) {
                                return event.coord == coord;
                            })) {
                continue;
            }
            HiddenExplorationEventState event;
            event.coord = coord;
            event.kind = kind;
            event.amount = amount;
            event.rewardGold = kind == HiddenExplorationEventKind::GoldCache ? amount : 0;
            hiddenEvents_.push_back(event);
            return true;
        }
        return false;
    };

    for (int i = 0; i < goldCount; ++i) {
        takeCandidate(HiddenExplorationEventKind::GoldCache, goldRewardDist(rng));
    }
    for (int i = 0; i < healCount; ++i) {
        takeCandidate(HiddenExplorationEventKind::HealingSpring, healAmountDist(rng));
    }
}

ExplorationStats ExplorationState::stats() const {
    ExplorationStats result;
    result.objectivesTotal = static_cast<int>(objectives_.size());

    for (const ExplorationObjectiveState& objective : objectives_) {
        if (objective.cleared) ++result.objectivesCleared;
        if (objective.kind == ExplorationObjectiveKind::Boss && objective.cleared) ++result.bossesCleared;
        if (objective.kind == ExplorationObjectiveKind::Trap && objective.triggered) ++result.trapsTriggered;
    }

    for (const HiddenExplorationEventState& event : hiddenEvents_) {
        if (event.claimed) {
            ++result.hiddenEventsClaimed;
            if (event.kind == HiddenExplorationEventKind::GoldCache) ++result.randomGoldEventsClaimed;
        }
        if (event.kind == HiddenExplorationEventKind::GoldCache) ++result.randomGoldEventsTotal;
    }

    return result;
}

std::vector<Coord> ExplorationState::randomGoldCoords() const {
    std::vector<Coord> coords;
    coords.reserve(hiddenEvents_.size());
    for (const HiddenExplorationEventState& event : hiddenEvents_) {
        if (event.kind == HiddenExplorationEventKind::GoldCache) coords.push_back(event.coord);
    }
    return coords;
}

std::vector<Coord> ExplorationState::hiddenHealingCoords() const {
    std::vector<Coord> coords;
    coords.reserve(hiddenEvents_.size());
    for (const HiddenExplorationEventState& event : hiddenEvents_) {
        if (event.kind == HiddenExplorationEventKind::HealingSpring) coords.push_back(event.coord);
    }
    return coords;
}

std::optional<HiddenExplorationEventState> ExplorationState::claimHiddenEvent(PlayerId player,
                                                                             Coord coord,
                                                                             UnitId unitId) {
    for (HiddenExplorationEventState& event : hiddenEvents_) {
        if (event.claimed || event.coord != coord) continue;
        event.claimed = true;
        event.claimedBy = player;
        event.claimedByUnit = unitId;
        return event;
    }
    return std::nullopt;
}

std::optional<RandomGoldEventState> ExplorationState::claimRandomGold(PlayerId player,
                                                                      Coord coord,
                                                                      UnitId unitId) {
    for (HiddenExplorationEventState& event : hiddenEvents_) {
        if (event.claimed || event.coord != coord || event.kind != HiddenExplorationEventKind::GoldCache) {
            continue;
        }
        event.claimed = true;
        event.claimedBy = player;
        event.claimedByUnit = unitId;
        return event;
    }
    return std::nullopt;
}

const std::vector<ExplorationObjectiveState>& ExplorationState::objectives() const {
    return objectives_;
}

const std::vector<NeutralCampState>& ExplorationState::neutralCamps() const {
    return neutralCamps_;
}

const std::vector<TrapState>& ExplorationState::traps() const {
    return traps_;
}

std::optional<size_t> ExplorationState::findTriggerableTrap(Coord coord) const {
    for (size_t i = 0; i < objectives_.size(); ++i) {
        const ExplorationObjectiveState& objective = objectives_[i];
        if (objective.kind == ExplorationObjectiveKind::Trap && !objective.cleared &&
            !objective.triggered && objective.coord == coord) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<size_t> ExplorationState::findObjectiveForUnit(UnitId unitId) const {
    if (unitId == kInvalidUnitId) return std::nullopt;
    for (size_t i = 0; i < objectives_.size(); ++i) {
        const ExplorationObjectiveState& objective = objectives_[i];
        if (objective.unitId == unitId) return i;
        if (std::find(objective.spawnedUnitIds.begin(), objective.spawnedUnitIds.end(), unitId) !=
            objective.spawnedUnitIds.end()) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<size_t> ExplorationState::findTrapForSpawn(Coord origin) const {
    for (size_t i = 0; i < objectives_.size(); ++i) {
        const ExplorationObjectiveState& objective = objectives_[i];
        if (objective.kind == ExplorationObjectiveKind::Trap && objective.coord == origin &&
            objective.triggered && !objective.cleared) {
            return i;
        }
    }
    return std::nullopt;
}

ExplorationObjectiveState* ExplorationState::objective(size_t index) {
    if (index >= objectives_.size()) return nullptr;
    return &objectives_[index];
}

const ExplorationObjectiveState* ExplorationState::objective(size_t index) const {
    if (index >= objectives_.size()) return nullptr;
    return &objectives_[index];
}

void ExplorationState::setObjectiveUnit(size_t index, UnitId unitId) {
    ExplorationObjectiveState* target = objective(index);
    if (!target) return;
    target->unitId = unitId;
    rebuildObjectiveIndexes();
}

void ExplorationState::markTriggered(size_t index) {
    ExplorationObjectiveState* target = objective(index);
    if (!target || target->cleared) return;
    target->triggered = true;
    target->revealed = true;

    if (target->kind == ExplorationObjectiveKind::Trap) {
        for (TrapState& trap : traps_) {
            if (trap.coord == target->coord) trap.triggered = true;
        }
    }
    rebuildObjectiveIndexes();
}

void ExplorationState::markCleared(size_t index) {
    ExplorationObjectiveState* target = objective(index);
    if (!target || target->cleared) return;
    target->cleared = true;
    target->revealed = true;

    if (target->kind == ExplorationObjectiveKind::Trap) {
        for (TrapState& trap : traps_) {
            if (trap.coord == target->coord) trap.triggered = true;
        }
    }
    rebuildObjectiveIndexes();
}

void ExplorationState::recordTrapSpawn(size_t index, UnitId unitId) {
    ExplorationObjectiveState* target = objective(index);
    if (!target || target->kind != ExplorationObjectiveKind::Trap) return;
    target->spawnedUnitIds.push_back(unitId);
    rebuildObjectiveIndexes();
}

void ExplorationState::clearTrapSpawns(size_t index) {
    ExplorationObjectiveState* target = objective(index);
    if (!target || target->kind != ExplorationObjectiveKind::Trap) return;
    target->spawnedUnitIds.clear();
    rebuildObjectiveIndexes();
}

} // namespace autochess
