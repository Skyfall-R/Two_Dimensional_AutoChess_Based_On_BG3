#include <lib.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <sstream>

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

} // namespace

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

GameEngine::GameEngine(unsigned seed) : specs_(makeSpecs()), rng_(seed) {}

void GameEngine::startNewGame(GameMode mode) {
    mode_ = mode;
    phase_ = Phase::Preparation;
    winner_.reset();
    round_ = 1;
    time_ = 0.0;
    combatTime_ = 0.0;
    nextUnitId_ = 0;
    units_.clear();
    events_.clear();
    resetBoard();

    players_[0] = PlayerState{PlayerId::One, "Player1", false, 10, false, {}, {}, {}};
    players_[1] = PlayerState{PlayerId::Two, mode == GameMode::SinglePlayerVsAi ? "AI" : "Player2",
                              mode == GameMode::SinglePlayerVsAi, 7, false, {}, {}, {}};

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
    for (PlayerState& p : players_) {
        p.ready = false;
        p.money += 5;
    }
    pushEvent({EventType::RoundStarted, PlayerId::One, kInvalidUnitId, kInvalidUnitId,
               {}, {}, round_, "Round " + std::to_string(round_) + " started - board cleared"});
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
            u.alive = true;
            u.hp.assign(u.spec.unitCount, u.spec.maxHp);
            Coord home = board_.inBounds(u.homeCoord) ? u.homeCoord : u.coord;
            u.deployed = true;
            u.coord = home;
            u.lastCoord = home;
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

void GameEngine::aiPrepare(PlayerId playerId) {
    aiBuy(playerId);
    aiDeploy(playerId);
    player(playerId).ready = true;
}

void GameEngine::aiBuy(PlayerId playerId) {
    PlayerState& p = player(playerId);
    for (int purchase = 0; purchase < 8 && p.bench.size() < 10; ++purchase) {
        const UnitSpec* best = nullptr;
        double bestScore = -std::numeric_limits<double>::infinity();
        for (const UnitSpec& spec : specs_) {
            if (spec.cost <= 0 || isInternalUnit(spec.type) || spec.cost > p.money) continue;
            double score = aiPurchaseScore(playerId, spec);
            if (score > bestScore) {
                bestScore = score;
                best = &spec;
            }
        }
        if (!best) break;
        if (!buyUnit(playerId, best->type)) break;
    }
}

void GameEngine::aiDeploy(PlayerId playerId) {
    PlayerState& p = player(playerId);
    std::vector<UnitId> pending = p.bench;
    std::sort(pending.begin(), pending.end(), [this](UnitId lhs, UnitId rhs) {
        return unit(lhs).spec.threat > unit(rhs).spec.threat;
    });

    for (UnitId id : pending) {
        if (id < 0 || id >= static_cast<int>(units_.size())) continue;
        const Unit& u = unit(id);
        int bestScore = std::numeric_limits<int>::min();
        Coord bestCoord{-1, -1};
        for (int y = 0; y < board_.height; ++y) {
            for (int x = 0; x < board_.width; ++x) {
                Coord coord{x, y};
                if (!canDeploy(playerId, coord, u.spec.layer)) continue;
                int score = aiDeploymentScore(playerId, u, coord);
                if (score > bestScore) {
                    bestScore = score;
                    bestCoord = coord;
                }
            }
        }
        if (bestCoord.x >= 0) deployUnit(playerId, id, bestCoord);
    }
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

} // namespace autochess
