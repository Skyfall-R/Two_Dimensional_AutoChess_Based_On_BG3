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
constexpr int kStartingGold = 12;
constexpr int kBaseRoundIncome = 6;
constexpr int kMaxRoundIncomeGrowth = 6;
constexpr int kInterestGoldStep = 10;
constexpr int kMaxInterestIncome = 3;
constexpr int kBoardFeaturePlanes = 6;
constexpr int kGlobalStateFeatureCount = 16;
constexpr int kStateFeatureCount = kGlobalStateFeatureCount + kBoardWidth * kBoardHeight * kBoardFeaturePlanes;
constexpr int kActionFeatureCount = 20;
constexpr int kMaxAiActionsPerPreparation = 64;
constexpr const char* kPolicyFormat = "autochess_policy_v1";
constexpr const char* kPolicyModelVersion = "linear-v1";

int playerIndex(PlayerId player) {
    return player == PlayerId::One ? 0 : 1;
}

bool isInternalUnit(UnitType type) {
    return type == UnitType::DefenseTower || type == UnitType::SkeletonByWitch ||
           type == UnitType::Treant;
}

bool isRoundTransientUnit(UnitType type) {
    return type == UnitType::SkeletonByWitch || type == UnitType::Treant;
}

int sign(int value) {
    if (value > 0) return 1;
    if (value < 0) return -1;
    return 0;
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

int roundIncomeFor(const PlayerState& player, int round) {
    int growth = std::min(round, kMaxRoundIncomeGrowth);
    int interest = std::min(kMaxInterestIncome, player.money / kInterestGoldStep);
    return kBaseRoundIncome + growth + interest;
}

std::vector<UnitSpec> makeSpecs() {
    return {
        {UnitType::Skeleton, "Skeleton", "Sk", 1, 3, 15, 10, 1, 3.0, 0.8,
         UnitLayer::Land, true, false, 12, kRoleMelee, AbilityKind::None},
        {UnitType::SkeletonByWitch, "Skeleton by Witch", "Ws", 0, 2, 16, 8, 1, 3.0, 0.8,
         UnitLayer::Land, true, false, 8, kRoleMelee, AbilityKind::None},
        {UnitType::Knight, "Knight", "Kn", 3, 1, 120, 18, 1, 2.0, 0.8,
         UnitLayer::Land, true, false, 25, kRoleTank | kRoleMelee, AbilityKind::None},
        {UnitType::Archer, "Archer", "Ar", 3, 2, 40, 15, 3, 3.0, 0.8,
         UnitLayer::Land, true, true, 32, kRoleRanged, AbilityKind::None},
        {UnitType::Pekka, "P_E_K_K_A", "Pk", 6, 1, 320, 100, 1, 1.0, 0.8,
         UnitLayer::Land, true, false, 60, kRoleTank | kRoleMelee, AbilityKind::PekkaHeavySwing},
        {UnitType::Witch, "Witch", "Wi", 4, 1, 80, 20, 2, 2.0, 0.8,
         UnitLayer::Land, true, true, 42, kRoleRanged | kRoleSummoner, AbilityKind::WitchSummon,
         1.0, 0, 1, 0.0},
        {UnitType::Balloon, "Balloon", "Ba", 4, 1, 150, 30, 1, 2.0, 0.9,
         UnitLayer::Air, true, false, 45, kRoleAir | kRoleAoe, AbilityKind::BalloonBomb},
        {UnitType::Minions, "Minions", "Mn", 3, 3, 20, 15, 3, 3.0, 0.8,
         UnitLayer::Air, true, true, 34, kRoleAir | kRoleRanged, AbilityKind::None},
        {UnitType::Goblin, "Goblin", "Go", 2, 3, 30, 12, 1, 4.0, 0.7,
         UnitLayer::Land, true, false, 22, kRoleMelee, AbilityKind::None},
        {UnitType::Prince, "Prince", "Pr", 5, 1, 200, 35, 1, 3.0, 0.8,
         UnitLayer::Land, true, false, 48, kRoleMelee, AbilityKind::PrinceCharge},
        {UnitType::BabyDragon, "Baby Dragon", "Dr", 4, 1, 150, 15, 3, 3.0, 0.9,
         UnitLayer::Air, true, true, 46, kRoleAir | kRoleAoe, AbilityKind::DragonBreath},
        {UnitType::DefenseTower, "Defense Tower", "Tw", 0, 1, 800, 30, 3, 0.0, 0.8,
         UnitLayer::Land, true, true, 80, kRoleRanged, AbilityKind::None},
        {UnitType::ShieldGuard, "ShieldGuard", "Sh", 4, 1, 180, 12, 1, 1.0, 0.9,
         UnitLayer::Land, true, false, 55, kRoleTank, AbilityKind::ShieldGuard,
         4.0, 40, 2, 0.0},
        {UnitType::Cleric, "Cleric", "Cl", 4, 1, 75, 8, 3, 2.0, 0.9,
         UnitLayer::Land, true, true, 44, kRoleSupport | kRoleRanged, AbilityKind::ClericHeal,
         1.2, 25, 3, 0.0},
        {UnitType::FrostMage, "FrostMage", "Fr", 5, 1, 70, 14, 3, 2.0, 0.9,
         UnitLayer::Land, true, true, 50, kRoleControl | kRoleRanged | kRoleAoe,
         AbilityKind::FrostNova, 0.0, 40, 1, 2.0},
        {UnitType::Bomber, "Bomber", "Bo", 3, 1, 60, 25, 2, 2.0, 1.0,
         UnitLayer::Land, true, false, 38, kRoleAoe | kRoleRanged, AbilityKind::BomberSplash},
        {UnitType::ShadowAssassin, "ShadowAssassin", "Sa", 4, 1, 90, 30, 1, 4.0, 0.7,
         UnitLayer::Land, true, false, 52, kRoleAssassin | kRoleMelee, AbilityKind::AssassinLeap},
        {UnitType::Druid, "Druid", "Du", 4, 1, 85, 12, 2, 2.0, 0.9,
         UnitLayer::Land, true, false, 36, kRoleSummoner | kRoleSupport, AbilityKind::DruidSummon,
         5.0, 0, 1, 0.0},
        {UnitType::Treant, "Treant", "Tr", 0, 1, 60, 8, 1, 1.0, 0.9,
         UnitLayer::Land, true, false, 10, kRoleMelee | kRoleTank, AbilityKind::None},
        {UnitType::Lancer, "Lancer", "La", 3, 1, 100, 22, 1, 3.0, 0.8,
         UnitLayer::Land, true, false, 34, kRoleMelee, AbilityKind::LancerPierce},
        {UnitType::StormSpirit, "StormSpirit", "St", 5, 2, 55, 18, 3, 4.0, 0.8,
         UnitLayer::Air, true, true, 54, kRoleAir | kRoleRanged | kRoleAoe,
         AbilityKind::StormChain}
    };
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
        case AiDifficulty::Normal: return "normal.policy.json";
        case AiDifficulty::Hard: return "hard.policy.json";
        case AiDifficulty::SuperHard: return "superhard.policy.json";
    }
    return "normal.policy.json";
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

} // namespace

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

UnitId Board::occupant(Coord coord, UnitLayer layer) const {
    if (!inBounds(coord)) return kInvalidUnitId;
    const Cell& cell = at(coord);
    return layer == UnitLayer::Land ? cell.land : cell.air;
}

void Board::setOccupant(Coord coord, UnitLayer layer, UnitId id) {
    if (!inBounds(coord)) return;
    Cell& cell = at(coord);
    if (layer == UnitLayer::Land) {
        cell.land = id;
    } else {
        cell.air = id;
    }
}

GameEngine::GameEngine(unsigned seed) : specs_(makeSpecs()), rng_(seed) {
    config_.mode = mode_;
    aiPolicyMetadata_.format = kPolicyFormat;
    aiPolicyMetadata_.modelVersion = kPolicyModelVersion;
    aiPolicyMetadata_.difficulty = toString(config_.aiDifficulty);
    aiPolicyMetadata_.stateFeatureCount = kStateFeatureCount;
    aiPolicyMetadata_.actionFeatureCount = kActionFeatureCount;
}

void GameEngine::startNewGame(GameMode mode) {
    GameConfig config = config_;
    config.mode = mode;
    startNewGame(config);
}

void GameEngine::startNewGame(const GameConfig& config) {
    config_ = config;
    mode_ = config.mode;
    phase_ = Phase::Preparation;
    winner_.reset();
    round_ = 1;
    time_ = 0.0;
    combatTime_ = 0.0;
    nextUnitId_ = 0;
    units_.clear();
    events_.clear();
    resetBoard();

    players_[0] = PlayerState{PlayerId::One, "Player1", false, kStartingGold, false, {}, {}, {}};
    players_[1] = PlayerState{PlayerId::Two, mode_ == GameMode::SinglePlayerVsAi ? "AI" : "Player2",
                              mode_ == GameMode::SinglePlayerVsAi, kStartingGold, false, {}, {}, {}};
    loadAiPlanner();

    const std::array<Coord, 4> towerCoords = {Coord{1, 2}, Coord{1, 4}, Coord{9, 2}, Coord{9, 4}};
    for (int i = 0; i < 4; ++i) {
        PlayerId owner = i < 2 ? PlayerId::One : PlayerId::Two;
        UnitId tower = createUnit(owner, UnitType::DefenseTower);
        placeUnit(tower, towerCoords[i]);
        unit(tower).deployed = true;
        unit(tower).homeCoord = towerCoords[i];
        player(owner).deployed.push_back(tower);
    }

    pushEvent({EventType::RoundStarted, PlayerId::One, kInvalidUnitId, kInvalidUnitId,
               {}, {}, round_, "Round 1 started"});
}

bool GameEngine::buyUnit(PlayerId playerId, UnitType type) {
    if (phase_ != Phase::Preparation) return false;
    const UnitSpec* spec = specFor(type);
    if (!spec || spec->cost <= 0 || isInternalUnit(type)) return false;

    PlayerState& p = player(playerId);
    if (p.money < spec->cost || p.bench.size() >= 10) return false;

    UnitId id = createUnit(playerId, type);
    p.money -= spec->cost;
    p.bench.push_back(id);
    addRecentBuy(p, type);

    pushEvent({EventType::Bought, playerId, id, kInvalidUnitId, {}, {}, spec->cost,
               p.name + " bought " + spec->name});
    return true;
}

bool GameEngine::deployUnit(PlayerId playerId, UnitId unitId, Coord coord) {
    if (phase_ != Phase::Preparation || unitId < 0 || unitId >= static_cast<int>(units_.size())) return false;
    Unit& u = unit(unitId);
    if (!u.alive || u.owner != playerId) return false;
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
    if (!u.alive || !u.deployed || u.owner != playerId) return false;
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
    if (!u.alive || !u.deployed || u.owner != playerId || u.spec.type == UnitType::DefenseTower) return false;
    if (p.bench.size() >= 10) return false;

    Coord from = u.coord;
    removeFromBoard(unitId);
    u.deployed = false;
    u.homeCoord = {};
    eraseValue(p.deployed, unitId);
    p.bench.push_back(unitId);

    pushEvent({EventType::Deployed, playerId, unitId, kInvalidUnitId, from, {}, 0,
               u.spec.name + " returned to bench"});
    return true;
}

bool GameEngine::upgradeUnit(PlayerId playerId, UnitId unitId) {
    if (phase_ != Phase::Preparation || unitId < 0 || unitId >= static_cast<int>(units_.size())) return false;
    Unit& u = unit(unitId);
    PlayerState& p = player(playerId);
    if (!u.alive || u.owner != playerId || u.upgraded || isInternalUnit(u.spec.type)) return false;
    if (p.money < u.spec.cost) return false;

    p.money -= u.spec.cost;
    u.upgraded = true;
    u.spec.attack += 10;
    u.spec.maxHp += 10;
    u.spec.name += "+";
    for (int& hp : u.hp) hp = u.spec.maxHp;

    pushEvent({EventType::Upgraded, playerId, unitId, kInvalidUnitId, {}, {}, u.spec.cost,
               p.name + " upgraded " + u.spec.name});
    return true;
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
    snapshot.phase = phase_;
    snapshot.winner = winner_;

    for (int i = 0; i < 2; ++i) {
        const PlayerState& p = players_[i];
        snapshot.players[i] = PlayerView{p.id, p.name, p.isAi, p.money, p.ready, p.bench, p.deployed};
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
        view.deployed = u.deployed;
        view.alive = u.alive;
        view.upgraded = u.upgraded;
        view.units = static_cast<int>(u.hp.size());
        view.maxUnits = u.spec.unitCount;
        view.totalHp = totalHp(u);
        view.maxTotalHp = u.spec.maxHp * u.spec.unitCount;
        view.shield = u.shield;
        view.attack = u.spec.attack;
        view.range = u.spec.range;
        view.cost = u.spec.cost;
        view.slowed = hasStatus(u, StatusKind::Slow);
        view.taunting = hasStatus(u, StatusKind::Taunt) || u.spec.ability == AbilityKind::ShieldGuard;
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

bool GameEngine::canDeploy(PlayerId playerId, Coord coord, UnitLayer layer) const {
    if (!isDeploymentCell(playerId, coord)) return false;
    return board_.occupant(coord, layer) == kInvalidUnitId;
}

bool GameEngine::isDeploymentCell(PlayerId playerId, Coord coord) const {
    if (!board_.inBounds(coord)) return false;
    if (playerId == PlayerId::One) return coord.x >= 0 && coord.x <= 2;
    return coord.x >= 8 && coord.x <= 10;
}

Coord GameEngine::baseCoord(PlayerId playerId) const {
    return playerId == PlayerId::One ? Coord{0, 3} : Coord{10, 3};
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
            "global:round,time,phase,economy,bench,deployed,tower_hp,ready,combat,board_size",
            "board:friend_land_threat,enemy_land_threat,friend_air_threat,enemy_air_threat,friend_hp,enemy_hp"
        },
        {
            "kind:buy,deploy,move,return,upgrade,ready",
            "unit:cost,count,hp,attack,range,speed,threat,layer,targets_air",
            "coord:normalized_x,normalized_y,forward_depth,unit_flags"
        }
    };
}

std::string GameEngine::rulesFingerprint() const {
    uint64_t hash = 14695981039346656037ull;
    hashAppend(hash, "autochess-rules-v1");
    hashAppend(hash, std::to_string(kBoardWidth));
    hashAppend(hash, std::to_string(kBoardHeight));
    hashAppend(hash, std::to_string(kStartingGold));
    hashAppend(hash, std::to_string(kBaseRoundIncome));
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
    }
    return hashToHex(hash);
}

std::vector<AiAction> GameEngine::legalActions(PlayerId playerId) const {
    std::vector<AiAction> actions;
    if (phase_ != Phase::Preparation) return actions;

    const PlayerState& p = player(playerId);
    if (p.bench.size() < 10) {
        for (const UnitSpec& spec : specs_) {
            if (spec.cost <= 0 || isInternalUnit(spec.type) || spec.cost > p.money) continue;
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
        if (!u.alive || !u.deployed || u.owner != playerId || u.spec.type == UnitType::DefenseTower) continue;
        for (int y = 0; y < board_.height; ++y) {
            for (int x = 0; x < board_.width; ++x) {
                Coord coord{x, y};
                if (coord != u.coord && canDeploy(playerId, coord, u.spec.layer)) {
                    actions.push_back({AiActionKind::MoveDeployed, u.spec.type, id, coord});
                }
            }
        }
        if (p.bench.size() < 10) actions.push_back({AiActionKind::ReturnToBench, u.spec.type, id, {}});
        if (!u.upgraded && u.spec.cost > 0 && p.money >= u.spec.cost) {
            actions.push_back({AiActionKind::Upgrade, u.spec.type, id, {}});
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
    auto towerHp = [this](PlayerId owner) {
        int total = 0;
        int maxTotal = 0;
        for (const Unit& u : units_) {
            if (u.owner == owner && u.alive && u.spec.type == UnitType::DefenseTower) {
                total += totalHp(u);
                maxTotal += u.spec.maxHp * u.spec.unitCount;
            }
        }
        return maxTotal > 0 ? static_cast<double>(total) / maxTotal : 0.0;
    };

    features.push_back(std::min(1.0, round_ / 20.0));
    features.push_back(std::min(1.0, time_ / 300.0));
    features.push_back(phase_ == Phase::Preparation ? 1.0 : 0.0);
    features.push_back(std::min(1.0, self.money / 50.0));
    features.push_back(std::min(1.0, foe.money / 50.0));
    features.push_back(std::min(1.0, self.bench.size() / 10.0));
    features.push_back(std::min(1.0, foe.bench.size() / 10.0));
    features.push_back(std::min(1.0, self.deployed.size() / 20.0));
    features.push_back(std::min(1.0, foe.deployed.size() / 20.0));
    features.push_back(towerHp(playerId));
    features.push_back(towerHp(foeId));
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
                UnitId id = board_.occupant(coord, layer);
                if (id == kInvalidUnitId || id >= static_cast<int>(units_.size())) continue;
                const Unit& u = unit(id);
                if (!u.alive) continue;
                bool friendly = u.owner == playerId;
                double threat = std::min(1.0, u.spec.threat / 100.0);
                double hpRatio = u.spec.maxHp * u.spec.unitCount > 0
                                     ? static_cast<double>(totalHp(u)) / (u.spec.maxHp * u.spec.unitCount)
                                     : 0.0;
                if (friendly && layer == UnitLayer::Land) planes[0] = threat;
                if (!friendly && layer == UnitLayer::Land) planes[1] = threat;
                if (friendly && layer == UnitLayer::Air) planes[2] = threat;
                if (!friendly && layer == UnitLayer::Air) planes[3] = threat;
                if (friendly) planes[4] = std::max(planes[4], hpRatio);
                if (!friendly) planes[5] = std::max(planes[5], hpRatio);
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
        constexpr double kMaxUnitType = static_cast<double>(static_cast<int>(UnitType::StormSpirit));
        features[6] = kMaxUnitType > 0.0 ? static_cast<int>(spec->type) / kMaxUnitType : 0.0;
        features[7] = std::min(1.0, spec->cost / 10.0);
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

void GameEngine::resetBoard() {
    board_ = Board();
}

UnitId GameEngine::createUnit(PlayerId owner, UnitType type) {
    const UnitSpec* spec = specFor(type);
    Unit unit;
    unit.id = nextUnitId_++;
    unit.spec = spec ? *spec : UnitSpec{};
    unit.owner = owner;
    unit.hp.assign(unit.spec.unitCount, unit.spec.maxHp);
    unit.lastCoord = unit.coord;
    unit.homeCoord = unit.coord;
    unit.attackTimer = 0.0;
    unit.abilityTimer = 0.0;

    if (type == UnitType::Treant) {
        unit.lifespan = 8.0;
        unit.statuses.push_back({StatusKind::Summoned, 8.0, 0, kInvalidUnitId});
    } else if (type == UnitType::SkeletonByWitch) {
        unit.statuses.push_back({StatusKind::Summoned, 999.0, 0, kInvalidUnitId});
    }
    if (unit.spec.ability == AbilityKind::ShieldGuard) {
        unit.statuses.push_back({StatusKind::Taunt, 9999.0, 80, unit.id});
    }

    units_.push_back(unit);
    return unit.id;
}

bool GameEngine::placeUnit(UnitId id, Coord coord) {
    Unit& u = unit(id);
    if (!board_.inBounds(coord)) return false;
    if (board_.occupant(coord, u.spec.layer) != kInvalidUnitId) return false;
    u.coord = coord;
    board_.setOccupant(coord, u.spec.layer, id);
    return true;
}

void GameEngine::removeFromBoard(UnitId id) {
    if (id < 0 || id >= static_cast<int>(units_.size())) return;
    Unit& u = unit(id);
    if (board_.inBounds(u.coord) && board_.occupant(u.coord, u.spec.layer) == id) {
        board_.setOccupant(u.coord, u.spec.layer, kInvalidUnitId);
    }
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
        if (!isRoundTransientUnit(u.spec.type)) u.homeCoord = u.coord;
        if (u.spec.ability == AbilityKind::AssassinLeap) {
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

void GameEngine::startNextRound() {
    resetCombatantsForPreparation();
    phase_ = Phase::Preparation;
    ++round_;
    combatTime_ = 0.0;

    std::array<int, 2> incomes{};
    for (PlayerState& p : players_) {
        p.ready = false;
        int income = roundIncomeFor(p, round_);
        p.money += income;
        incomes[playerIndex(p.id)] = income;
    }
    pushEvent({EventType::RoundStarted, PlayerId::One, kInvalidUnitId, kInvalidUnitId,
               {}, {}, incomes[0],
               "Round " + std::to_string(round_) + " started - tower damage persists, income +" +
                   std::to_string(incomes[0]) + "/+" + std::to_string(incomes[1])});
}

void GameEngine::resetCombatantsForPreparation() {
    resetBoard();

    for (PlayerState& p : players_) {
        p.bench.clear();
        p.deployed.clear();
        p.recentBuys.clear();
    }

    for (Unit& u : units_) {
        u.shield = 0;
        u.target = kInvalidUnitId;
        u.retargetTimer = 0.0;
        u.moveProgress = 0.0;
        u.attackTimer = 0.0;
        u.abilityTimer = 0.0;
        u.lifespan = -1.0;
        u.firstStrikeReady = false;
        u.specialCounter = 0;
        u.stuckTicks = 0;
        u.statuses.clear();

        if (u.spec.type == UnitType::DefenseTower) {
            Coord home = board_.inBounds(u.homeCoord) ? u.homeCoord : u.coord;
            u.coord = home;
            u.lastCoord = home;
            if (!u.alive || u.hp.empty()) {
                u.deployed = false;
                continue;
            }
            u.deployed = true;
            placeUnit(u.id, home);
            player(u.owner).deployed.push_back(u.id);
        } else {
            u.alive = false;
            u.deployed = false;
            u.hp.clear();
            u.coord = {};
            u.lastCoord = {};
            u.homeCoord = {};
        }
    }
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

void GameEngine::useHeuristicAiPlanner(const std::string& status) {
    aiPolicyMetadata_.valid = false;
    aiPolicyMetadata_.status = status;
    aiPlanner_ = std::make_unique<HeuristicAiPlanner>();
    pushEvent({EventType::AiPolicyStatus, PlayerId::Two, kInvalidUnitId, kInvalidUnitId,
               {}, {}, 0, "AI policy fallback: " + status});
}

void GameEngine::aiPrepare(PlayerId playerId) {
    ensureAiPlanner();
    for (int step = 0; step < kMaxAiActionsPerPreparation && phase_ == Phase::Preparation; ++step) {
        std::vector<AiAction> actions = legalActions(playerId);
        if (actions.empty()) break;
        std::optional<AiAction> action = aiPlanner_->chooseAction(*this, playerId, actions);
        if (!action) break;
        AiActionKind kind = action->kind;
        if (!applyAiAction(playerId, *action)) break;
        if (kind == AiActionKind::Ready) return;
    }
    if (phase_ == Phase::Preparation && hasActiveCombatUnit(playerId)) {
        setReady(playerId, true);
    }
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
    if (bestUpgrade && player(playerId).bench.empty()) return *bestUpgrade;
    if (bestDeploy) return *bestDeploy;
    if (ready) return *ready;
    if (bestUpgrade) return *bestUpgrade;
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
            return aiDeploymentScore(playerId, u, action.coord) + u.spec.threat * 0.5;
        }
        case AiActionKind::Upgrade: {
            if (action.unitId < 0 || action.unitId >= static_cast<int>(units_.size())) return -1000.0;
            const Unit& u = unit(action.unitId);
            return u.spec.threat + u.spec.attack * 0.4 + u.spec.maxHp * 0.05;
        }
        case AiActionKind::ReturnToBench:
            return -500.0;
        case AiActionKind::Ready:
            return -50.0;
    }
    return -1000.0;
}

double GameEngine::aiPurchaseScore(PlayerId playerId, const UnitSpec& spec) const {
    const PlayerState& foe = player(opponent(playerId));
    double score = spec.threat + spec.attack * 0.8 + spec.range * 4.0 + spec.speed * 2.0;

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

    u.abilityTimer += dt;
    if (u.abilityTimer < u.spec.abilityCooldown) return;

    switch (u.spec.ability) {
        case AbilityKind::ShieldGuard:
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
        case AbilityKind::WitchSummon:
        case AbilityKind::DruidSummon: {
            PlayerId owner = u.owner;
            Coord origin = u.coord;
            std::string summonerName = u.spec.name;
            AbilityKind ability = u.spec.ability;
            std::optional<Coord> spot = findSummonCell(owner, origin, UnitLayer::Land);
            if (spot) {
                UnitType summonType = ability == AbilityKind::WitchSummon
                                          ? UnitType::SkeletonByWitch
                                          : UnitType::Treant;
                UnitId summon = createUnit(owner, summonType);
                Unit& summoned = unit(summon);
                summoned.deployed = true;
                summoned.lifespan = summonType == UnitType::Treant ? 8.0 : -1.0;
                placeUnit(summon, *spot);
                summoned.homeCoord = *spot;
                player(owner).deployed.push_back(summon);
                pushEvent({EventType::Deployed, owner, summon, id, origin, *spot, 0,
                           summonerName + " summoned " + summoned.spec.name});
            }
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
        tickAbilities(id, dt);
    }

    for (UnitId id : ids) {
        if (!unit(id).alive) continue;
        Unit& u = unit(id);
        u.attackTimer += dt;
        u.retargetTimer -= dt;
        bool force = u.target == kInvalidUnitId ||
                     u.target >= static_cast<int>(units_.size()) ||
                     !unit(u.target).alive;
        refreshTarget(u, force);

        if (u.target != kInvalidUnitId && u.attackTimer >= effectiveAttackCooldown(u) &&
            inAttackRange(u, unit(u.target))) {
            attack(id, u.target);
            if (id < static_cast<int>(units_.size()) && unit(id).alive) unit(id).attackTimer = 0.0;
        }
    }

    moveUnits(dt);
    clearDeadUnits();
    resolveVictory();

    if (phase_ == Phase::Combat && combatTime_ >= 45.0) {
        startNextRound();
    }
}

void GameEngine::refreshTarget(Unit& u, bool force) {
    if (!force && u.retargetTimer > 0.0) return;
    u.target = selectTarget(u);
    u.retargetTimer = 0.2;
}

UnitId GameEngine::selectTarget(const Unit& u) const {
    if (!u.alive || !u.deployed) return kInvalidUnitId;
    if (u.spec.ability == AbilityKind::ClericHeal) {
        if (selectGlobalHealTarget(u) != kInvalidUnitId) return kInvalidUnitId;
        if (selectFollowAlly(u) != kInvalidUnitId) {
            UnitId closeEnemy = kInvalidUnitId;
            int bestDist = std::numeric_limits<int>::max();
            for (UnitId candidateId : player(opponent(u.owner)).deployed) {
                if (candidateId < 0 || candidateId >= static_cast<int>(units_.size())) continue;
                const Unit& candidate = unit(candidateId);
                if (!candidate.alive || !candidate.deployed || !inAttackRange(u, candidate)) continue;
                int dist = manhattan(u.coord, candidate.coord);
                if (dist < bestDist) {
                    bestDist = dist;
                    closeEnemy = candidateId;
                }
            }
            return closeEnemy;
        }
    }

    UnitId best = kInvalidUnitId;
    double bestScore = -std::numeric_limits<double>::infinity();
    PlayerId foe = opponent(u.owner);

    for (UnitId candidateId : player(foe).deployed) {
        if (candidateId < 0 || candidateId >= static_cast<int>(units_.size())) continue;
        const Unit& candidate = unit(candidateId);
        if (!candidate.alive || !candidate.deployed || !canAttack(u, candidate)) continue;

        int dist = manhattan(u.coord, candidate.coord);
        double score = candidate.spec.threat - dist * 8.0;
        if (candidate.hp.size() < static_cast<size_t>(candidate.spec.unitCount)) score += 10.0;
        if (totalHp(candidate) < candidate.spec.maxHp * candidate.spec.unitCount / 2) score += 8.0;
        if (hasStatus(candidate, StatusKind::Taunt) || candidate.spec.ability == AbilityKind::ShieldGuard) {
            score += dist <= 3 ? 80.0 : 25.0;
        }
        if (u.spec.roleMask & kRoleAssassin) {
            if (candidate.spec.roleMask & (kRoleSupport | kRoleRanged | kRoleAoe)) score += 60.0;
            int backline = candidate.owner == PlayerId::One
                               ? (board_.width - 1 - candidate.coord.x)
                               : candidate.coord.x;
            score += backline * 4.0;
        }
        if (u.spec.roleMask & kRoleAoe) {
            score += clusterScoreAround(u, candidate.coord);
        }
        if (u.spec.roleMask & kRoleRanged) {
            if (dist <= u.spec.range) score += 20.0;
        }
        if (u.spec.range <= 1 || (u.spec.roleMask & kRoleMelee)) {
            score += 60.0 - dist * 6.0;
        }
        if (candidate.spec.type == UnitType::DefenseTower && dist > u.spec.range) {
            score -= 35.0;
        }
        if (u.spec.layer == UnitLayer::Land && !inAttackRange(u, candidate)) {
            if (!findPathToAttackCell(u, candidate.coord, u.spec.range).found) score -= 1000.0;
        }
        if (score > bestScore) {
            bestScore = score;
            best = candidateId;
        }
    }

    return best;
}

UnitId GameEngine::selectHealTarget(const Unit& healer) const {
    UnitId best = kInvalidUnitId;
    double bestRatio = 1.01;
    for (UnitId id : player(healer.owner).deployed) {
        if (id < 0 || id >= static_cast<int>(units_.size())) continue;
        const Unit& ally = unit(id);
        if (!ally.alive || !ally.deployed || ally.spec.type == UnitType::DefenseTower) continue;
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
        if (!ally.alive || !ally.deployed || ally.spec.type == UnitType::DefenseTower) continue;

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
        if (!ally.alive || !ally.deployed || ally.spec.type == UnitType::DefenseTower) continue;
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

double GameEngine::clusterScoreAround(const Unit& attacker, Coord center) const {
    int radius = attacker.spec.ability == AbilityKind::StormChain ? 2 : 1;
    int count = 0;
    int threat = 0;
    for (UnitId id : player(opponent(attacker.owner)).deployed) {
        if (id < 0 || id >= static_cast<int>(units_.size())) continue;
        const Unit& enemy = unit(id);
        if (!enemy.alive || !enemy.deployed || !canAttack(attacker, enemy)) continue;
        if (attacker.spec.ability == AbilityKind::BomberSplash && enemy.spec.layer != UnitLayer::Land) continue;
        if (manhattan(center, enemy.coord) > radius) continue;
        ++count;
        threat += enemy.spec.threat;
    }
    if (count <= 1) return 0.0;
    return (count - 1) * 32.0 + threat * 0.15;
}

bool GameEngine::canAttack(const Unit& attacker, const Unit& target) const {
    if (!attacker.alive || !target.alive || attacker.owner == target.owner) return false;
    if (target.spec.layer == UnitLayer::Land && !attacker.spec.canAttackLand) return false;
    if (target.spec.layer == UnitLayer::Air && !attacker.spec.canAttackAir) return false;
    return true;
}

bool GameEngine::inAttackRange(const Unit& attacker, const Unit& target) const {
    return canAttack(attacker, target) && manhattan(attacker.coord, target.coord) <= attacker.spec.range;
}

double GameEngine::effectiveSpeed(const Unit& u) const {
    double speed = u.spec.speed;
    if (hasStatus(u, StatusKind::Slow)) speed *= 0.6;
    return speed;
}

double GameEngine::effectiveAttackCooldown(const Unit& u) const {
    double cooldown = u.spec.attackCooldown;
    if (hasStatus(u, StatusKind::Slow)) cooldown /= 0.6;
    return cooldown;
}

bool GameEngine::hasStatus(const Unit& u, StatusKind kind) const {
    return std::any_of(u.statuses.begin(), u.statuses.end(), [kind](const StatusEffect& status) {
        return status.kind == kind && status.remaining > 0.0;
    });
}

void GameEngine::addStatus(UnitId id, StatusEffect status) {
    if (id < 0 || id >= static_cast<int>(units_.size())) return;
    Unit& u = unit(id);
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
               "Status applied to " + eventUnitName(u)});
}

void GameEngine::attack(UnitId attackerId, UnitId targetId) {
    if (attackerId < 0 || targetId < 0 || attackerId >= static_cast<int>(units_.size()) ||
        targetId >= static_cast<int>(units_.size())) {
        return;
    }
    Unit& attacker = unit(attackerId);
    if (!attacker.alive || !unit(targetId).alive) return;

    pushEvent({EventType::UnitAttacked, attacker.owner, attackerId, targetId, attacker.coord,
               unit(targetId).coord, attacker.spec.attack, eventUnitName(attacker) + " attacked"});

    int damage = attacker.spec.attack * std::max(1, static_cast<int>(attacker.hp.size()));

    if (attacker.spec.ability == AbilityKind::PekkaHeavySwing) {
        ++attacker.specialCounter;
        if (attacker.specialCounter % 2 == 1) return;
    }

    if (attacker.spec.ability == AbilityKind::PrinceCharge && attacker.firstStrikeReady) {
        damage *= 2;
        attacker.firstStrikeReady = false;
    }

    if (attacker.spec.ability == AbilityKind::DragonBreath) {
        std::vector<UnitId> targets;
        for (UnitId id : player(opponent(attacker.owner)).deployed) {
            if (id >= 0 && id < static_cast<int>(units_.size()) && unit(id).alive &&
                canAttack(attacker, unit(id)) && manhattan(attacker.coord, unit(id).coord) <= attacker.spec.range) {
                targets.push_back(id);
            }
        }
        for (UnitId id : targets) applyDamage(id, attacker.spec.attack, attackerId);
        return;
    }

    if (attacker.spec.ability == AbilityKind::BomberSplash ||
        attacker.spec.ability == AbilityKind::FrostNova) {
        std::vector<UnitId> targets;
        Coord center = unit(targetId).coord;
        for (UnitId id : player(opponent(attacker.owner)).deployed) {
            if (id >= 0 && id < static_cast<int>(units_.size()) && unit(id).alive &&
                canAttack(attacker, unit(id)) && manhattan(center, unit(id).coord) <= 1) {
                if (attacker.spec.ability == AbilityKind::BomberSplash && unit(id).spec.layer != UnitLayer::Land) continue;
                targets.push_back(id);
            }
        }
        for (UnitId id : targets) {
            applyDamage(id, attacker.spec.attack, attackerId);
            if (attacker.spec.ability == AbilityKind::FrostNova && id < static_cast<int>(units_.size()) && unit(id).alive) {
                addStatus(id, {StatusKind::Slow, attacker.spec.abilityDuration, attacker.spec.abilityValue, attackerId});
            }
        }
        return;
    }

    if (attacker.spec.ability == AbilityKind::StormChain) {
        applyDamage(targetId, attacker.spec.attack, attackerId);
        std::vector<std::pair<int, UnitId>> nearby;
        Coord center = unit(targetId).coord;
        for (UnitId id : player(opponent(attacker.owner)).deployed) {
            if (id == targetId || id < 0 || id >= static_cast<int>(units_.size())) continue;
            if (!unit(id).alive || !canAttack(attacker, unit(id))) continue;
            int dist = manhattan(center, unit(id).coord);
            if (dist <= 2) nearby.push_back({dist, id});
        }
        std::sort(nearby.begin(), nearby.end());
        for (int i = 0; i < static_cast<int>(nearby.size()) && i < 2; ++i) {
            applyDamage(nearby[i].second, static_cast<int>(attacker.spec.attack * 0.6), attackerId);
        }
        return;
    }

    if (attacker.spec.ability == AbilityKind::LancerPierce) {
        Coord targetCoord = unit(targetId).coord;
        applyDamage(targetId, damage, attackerId);
        Coord behind{targetCoord.x + sign(targetCoord.x - attacker.coord.x),
                     targetCoord.y + sign(targetCoord.y - attacker.coord.y)};
        if (board_.inBounds(behind)) {
            UnitId pierced = board_.occupant(behind, UnitLayer::Land);
            if (pierced != kInvalidUnitId && unit(pierced).alive && unit(pierced).owner != attacker.owner) {
                applyDamage(pierced, damage / 2, attackerId);
            }
        }
        return;
    }

    applyDamage(targetId, damage, attackerId);
}

void GameEngine::applyDamage(UnitId targetId, int amount, UnitId sourceId) {
    if (targetId < 0 || targetId >= static_cast<int>(units_.size()) || amount <= 0) return;
    Unit& target = unit(targetId);
    if (!target.alive || target.hp.empty()) return;

    int absorbed = std::min(target.shield, amount);
    if (absorbed > 0) {
        target.shield -= absorbed;
        amount -= absorbed;
        pushEvent({EventType::Shielded, target.owner, sourceId, targetId, {}, target.coord, absorbed,
                   eventUnitName(target) + " shield absorbed damage"});
    }
    if (amount <= 0) return;

    std::uniform_int_distribution<int> pick(0, static_cast<int>(target.hp.size()) - 1);
    int index = pick(rng_);
    target.hp[index] -= amount;

    pushEvent({EventType::DamageDealt, target.owner, sourceId, targetId, {}, target.coord, amount,
               eventUnitName(target) + " took " + std::to_string(amount) + " damage"});

    if (target.hp[index] <= 0) {
        target.hp.erase(target.hp.begin() + index);
    }
    if (target.hp.empty()) killUnit(targetId, sourceId);
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
    AbilityKind deathAbility = dead.spec.ability;
    dead.alive = false;
    dead.hp.clear();
    removeFromBoard(id);
    eraseValue(player(owner).bench, id);
    if (isRoundTransientUnit(dead.spec.type)) {
        dead.deployed = false;
        eraseValue(player(owner).deployed, id);
    }

    pushEvent({EventType::UnitDied, owner, sourceId, id, deathCoord, deathCoord, 0,
               eventUnitName(dead) + " died"});

    if (deathAbility == AbilityKind::BalloonBomb) {
        std::vector<UnitId> targets;
        for (UnitId enemyId : player(opponent(owner)).deployed) {
            if (enemyId < 0 || enemyId >= static_cast<int>(units_.size())) continue;
            const Unit& enemy = unit(enemyId);
            if (enemy.alive && enemy.spec.layer == UnitLayer::Land && manhattan(enemy.coord, deathCoord) <= 1) {
                targets.push_back(enemyId);
            }
        }
        for (UnitId targetId : targets) applyDamage(targetId, 60, id);
    }
}

void GameEngine::clearDeadUnits() {
    for (PlayerState& p : players_) {
        p.bench.erase(std::remove_if(p.bench.begin(), p.bench.end(), [this](UnitId id) {
            return id < 0 || id >= static_cast<int>(units_.size()) || !unit(id).alive;
        }), p.bench.end());
        p.deployed.erase(std::remove_if(p.deployed.begin(), p.deployed.end(), [this](UnitId id) {
            return id < 0 || id >= static_cast<int>(units_.size()) ||
                   (!unit(id).alive && isRoundTransientUnit(unit(id).spec.type));
        }), p.deployed.end());
    }
}

void GameEngine::resolveVictory() {
    if (phase_ != Phase::Combat) return;
    for (const Unit& u : units_) {
        if (!u.alive || !u.deployed || u.spec.type == UnitType::DefenseTower) continue;
        if (u.coord == baseCoord(opponent(u.owner))) {
            finishCombat(u.owner);
            return;
        }
    }

    for (PlayerId p : {PlayerId::One, PlayerId::Two}) {
        bool towerAlive = false;
        for (UnitId id : player(p).deployed) {
            if (id >= 0 && id < static_cast<int>(units_.size()) && unit(id).alive &&
                unit(id).spec.type == UnitType::DefenseTower) {
                towerAlive = true;
                break;
            }
        }
        if (!towerAlive) {
            finishCombat(opponent(p));
            return;
        }
    }

    if (!hasActiveCombatUnit(PlayerId::One) && !hasActiveCombatUnit(PlayerId::Two)) {
        startNextRound();
    }
}

bool GameEngine::hasActiveCombatUnit(PlayerId playerId) const {
    for (UnitId id : player(playerId).deployed) {
        if (id < 0 || id >= static_cast<int>(units_.size())) continue;
        const Unit& u = unit(id);
        if (u.alive && u.deployed && u.spec.type != UnitType::DefenseTower) return true;
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
        if (a.spec.speed != b.spec.speed) return a.spec.speed < b.spec.speed;
        if (totalHp(a) != totalHp(b)) return totalHp(a) < totalHp(b);
        return lhs < rhs;
    });

    std::vector<Coord> reserved;
    for (UnitId id : ids) {
        Unit& u = unit(id);
        if (!u.alive || !u.deployed || u.spec.speed <= 0.0 || u.spec.type == UnitType::DefenseTower) continue;
        if (u.target != kInvalidUnitId && u.target < static_cast<int>(units_.size()) &&
            unit(u.target).alive && inAttackRange(u, unit(u.target))) {
            continue;
        }

        u.moveProgress += effectiveSpeed(u) * dt;
        if (u.moveProgress < 1.0) continue;
        u.moveProgress -= 1.0;

        std::optional<Coord> next = chooseNextStep(id, reserved);
        if (!next || *next == u.coord) {
            ++u.stuckTicks;
            if (u.stuckTicks > 30) {
                u.target = kInvalidUnitId;
                u.retargetTimer = 0.0;
                u.stuckTicks = 0;
            }
            continue;
        }

        Coord from = u.coord;
        removeFromBoard(id);
        if (!placeUnit(id, *next)) {
            placeUnit(id, from);
            ++u.stuckTicks;
            continue;
        }

        if (u.spec.ability == AbilityKind::PrinceCharge) u.firstStrikeReady = true;
        u.stuckTicks = from == *next ? u.stuckTicks + 1 : 0;
        u.lastCoord = from;
        reserved.push_back(*next);
        pushEvent({EventType::UnitMoved, u.owner, id, kInvalidUnitId, from, *next, 0,
                   eventUnitName(u) + " moved"});
    }
}

std::optional<Coord> GameEngine::chooseNextStep(UnitId id, const std::vector<Coord>& reserved) const {
    const Unit& u = unit(id);
    Coord target = baseCoord(opponent(u.owner));
    bool hasUnitTarget = u.target != kInvalidUnitId && u.target < static_cast<int>(units_.size()) &&
                         unit(u.target).alive;

    if (u.spec.ability == AbilityKind::ClericHeal) {
        if (std::optional<Coord> supportStep = chooseSupportStep(u, reserved)) return supportStep;
        if (selectGlobalHealTarget(u) != kInvalidUnitId || selectFollowAlly(u) != kInvalidUnitId) {
            return std::nullopt;
        }
    }

    if (hasUnitTarget) {
        target = unit(u.target).coord;
        if (inAttackRange(u, unit(u.target))) return std::nullopt;
    }

    if (u.spec.layer == UnitLayer::Air) return chooseAirStep(u, target, reserved);

    PathResult path = hasUnitTarget ? findPathToAttackCell(u, target, u.spec.range)
                                    : findPathToGoal(u, target);
    if (!path.found || path.steps.size() < 2) return std::nullopt;
    Coord next = path.steps[1];
    if (std::find(reserved.begin(), reserved.end(), next) != reserved.end()) return std::nullopt;
    if (!passableForLand(u, next)) return std::nullopt;
    return next;
}

std::optional<Coord> GameEngine::chooseSupportStep(const Unit& u, const std::vector<Coord>& reserved) const {
    if (!u.alive || !u.deployed || u.spec.ability != AbilityKind::ClericHeal) return std::nullopt;

    UnitId healTarget = selectGlobalHealTarget(u);
    if (healTarget != kInvalidUnitId) {
        const Unit& target = unit(healTarget);
        if (manhattan(u.coord, target.coord) <= u.spec.abilityRange) return std::nullopt;

        PathResult path = findPathToAttackCell(u, target.coord, u.spec.abilityRange);
        if (!path.found || path.steps.size() < 2) return std::nullopt;
        Coord next = path.steps[1];
        if (std::find(reserved.begin(), reserved.end(), next) != reserved.end()) return std::nullopt;
        if (!passableForLand(u, next)) return std::nullopt;
        return next;
    }

    std::optional<Coord> goal = chooseFollowAllyGoal(u);
    if (!goal || *goal == u.coord) return std::nullopt;

    PathResult path = findPathToGoal(u, *goal);
    if (!path.found || path.steps.size() < 2) return std::nullopt;
    Coord next = path.steps[1];
    if (std::find(reserved.begin(), reserved.end(), next) != reserved.end()) return std::nullopt;
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
    std::vector<Coord> candidates = neighbors4(u.coord);
    Coord forward = forwardCoord(u.owner, u.coord);
    if (board_.inBounds(forward)) candidates.insert(candidates.begin(), forward);

    int currentDist = manhattan(u.coord, target);
    Coord best = u.coord;
    int bestScore = std::numeric_limits<int>::min();
    for (Coord candidate : candidates) {
        if (!board_.inBounds(candidate)) continue;
        if (board_.occupant(candidate, UnitLayer::Air) != kInvalidUnitId) continue;
        if (std::find(reserved.begin(), reserved.end(), candidate) != reserved.end()) continue;
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

    std::vector<Coord> goals;
    for (Coord coord : cellsInRange(target, range)) {
        if (coord == target) continue;
        if (!board_.inBounds(coord)) continue;
        if (coord == u.coord || passableForLand(u, coord)) goals.push_back(coord);
    }

    PathResult best;
    for (Coord goal : goals) {
        PathResult path = findPathToGoal(u, goal);
        if (path.found && (!best.found || path.steps.size() < best.steps.size())) best = path;
    }
    return best;
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
    if (coord == u.coord) return true;
    return board_.occupant(coord, UnitLayer::Land) == kInvalidUnitId;
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
    std::vector<Coord> candidates;
    Coord forward = forwardCoord(owner, origin);
    if (board_.inBounds(forward)) candidates.push_back(forward);
    for (Coord coord : adjacentCells(origin)) {
        if (coord != forward) candidates.push_back(coord);
    }

    for (Coord coord : candidates) {
        if (board_.inBounds(coord) && board_.occupant(coord, layer) == kInvalidUnitId) return coord;
    }
    return std::nullopt;
}

void GameEngine::performAssassinLeap(UnitId id) {
    if (id < 0 || id >= static_cast<int>(units_.size())) return;
    Unit& assassin = unit(id);
    if (!assassin.alive || assassin.spec.ability != AbilityKind::AssassinLeap) return;

    UnitId target = kInvalidUnitId;
    int bestScore = std::numeric_limits<int>::min();
    for (UnitId enemyId : player(opponent(assassin.owner)).deployed) {
        if (enemyId < 0 || enemyId >= static_cast<int>(units_.size())) continue;
        const Unit& enemy = unit(enemyId);
        if (!enemy.alive || enemy.spec.type == UnitType::DefenseTower) continue;
        int backline = enemy.owner == PlayerId::One ? (board_.width - 1 - enemy.coord.x) : enemy.coord.x;
        int score = enemy.spec.threat + backline * 8;
        if (enemy.spec.roleMask & (kRoleSupport | kRoleRanged | kRoleAoe)) score += 30;
        if (score > bestScore) {
            bestScore = score;
            target = enemyId;
        }
    }
    if (target == kInvalidUnitId) return;

    Coord best{-1, -1};
    int bestDist = std::numeric_limits<int>::max();
    for (Coord coord : adjacentCells(unit(target).coord)) {
        if (!board_.inBounds(coord)) continue;
        if (board_.occupant(coord, assassin.spec.layer) != kInvalidUnitId) continue;
        int dist = manhattan(coord, unit(target).coord);
        if (dist < bestDist) {
            bestDist = dist;
            best = coord;
        }
    }
    if (best.x < 0) return;

    Coord from = assassin.coord;
    removeFromBoard(id);
    if (placeUnit(id, best)) {
        assassin.firstStrikeReady = true;
        pushEvent({EventType::UnitMoved, assassin.owner, id, target, from, best, 0,
                   eventUnitName(assassin) + " leaped to the backline"});
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
        case UnitType::SkeletonByWitch: return "SkeletonByWitch";
        case UnitType::Knight: return "Knight";
        case UnitType::Archer: return "Archer";
        case UnitType::Pekka: return "Pekka";
        case UnitType::Witch: return "Witch";
        case UnitType::Balloon: return "Balloon";
        case UnitType::Minions: return "Minions";
        case UnitType::Goblin: return "Goblin";
        case UnitType::Prince: return "Prince";
        case UnitType::BabyDragon: return "BabyDragon";
        case UnitType::DefenseTower: return "DefenseTower";
        case UnitType::ShieldGuard: return "ShieldGuard";
        case UnitType::Cleric: return "Cleric";
        case UnitType::FrostMage: return "FrostMage";
        case UnitType::Bomber: return "Bomber";
        case UnitType::ShadowAssassin: return "ShadowAssassin";
        case UnitType::Druid: return "Druid";
        case UnitType::Treant: return "Treant";
        case UnitType::Lancer: return "Lancer";
        case UnitType::StormSpirit: return "StormSpirit";
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
