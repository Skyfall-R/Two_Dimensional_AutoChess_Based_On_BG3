#include <lib.hpp>

#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using namespace autochess;

namespace {

bool isShopUnit(const UnitSpec& spec) {
    return spec.cost > 0 && spec.type != UnitType::DefenseTower &&
           spec.type != UnitType::SkeletonByWitch && spec.type != UnitType::Treant;
}

const UnitView* findUnit(const GameSnapshot& snapshot, UnitId id) {
    for (const UnitView& unit : snapshot.units) {
        if (unit.id == id) return &unit;
    }
    return nullptr;
}

void printEvents(GameEngine& engine) {
    for (const Event& event : engine.consumeEvents()) {
        if (!event.text.empty()) std::cout << "  " << event.text << "\n";
    }
}

void printBoard(const GameSnapshot& snapshot) {
    std::cout << "\nRound " << snapshot.round << " | " << toString(snapshot.phase)
              << " | P1 $" << snapshot.players[0].money
              << " | P2 $" << snapshot.players[1].money << "\n";
    std::cout << "Deployment: Player1 x=0..2, Player2 x=8..10\n\n";

    for (int y = 0; y < snapshot.height; ++y) {
        for (int x = 0; x < snapshot.width; ++x) {
            std::string label = ".";
            for (const UnitView& unit : snapshot.units) {
                if (!unit.alive || !unit.deployed || unit.coord.x != x || unit.coord.y != y) continue;
                char owner = unit.owner == PlayerId::One ? '1' : '2';
                label = std::string(1, owner) + unit.shortName.substr(0, 1);
                if (unit.layer == UnitLayer::Air) label += "^";
                break;
            }
            std::cout << std::setw(4) << std::left << label;
        }
        std::cout << "\n";
    }

    std::cout << "\nBench:\n";
    for (const PlayerView& player : snapshot.players) {
        std::cout << "  " << player.name << ": ";
        for (UnitId id : player.bench) {
            const UnitView* unit = findUnit(snapshot, id);
            if (unit) std::cout << "[" << id << ":" << unit->name << "] ";
        }
        std::cout << "\n";
    }

    std::cout << "\nDeployed units:\n";
    for (const UnitView& unit : snapshot.units) {
        if (!unit.alive || !unit.deployed) continue;
        std::cout << "  #" << unit.id << " " << unit.name << " "
                  << (unit.owner == PlayerId::One ? "P1" : "P2")
                  << " (" << unit.coord.x << "," << unit.coord.y << ") "
                  << unit.units << "u HP " << unit.totalHp << "/" << unit.maxTotalHp;
        if (unit.shield > 0) std::cout << " shield " << unit.shield;
        if (unit.slowed) std::cout << " slowed";
        std::cout << "\n";
    }
}

void printShop(const GameEngine& engine) {
    int index = 1;
    std::cout << "\nShop:\n";
    for (const UnitSpec& spec : engine.shop()) {
        if (!isShopUnit(spec)) continue;
        std::cout << "  " << index << ". " << std::setw(15) << std::left << spec.name
                  << " cost " << spec.cost
                  << " atk " << spec.attack
                  << " range " << spec.range
                  << " speed " << spec.speed
                  << " " << toString(spec.layer) << "\n";
        ++index;
    }
}

std::vector<UnitType> shopTypes(const GameEngine& engine) {
    std::vector<UnitType> types;
    for (const UnitSpec& spec : engine.shop()) {
        if (isShopUnit(spec)) types.push_back(spec.type);
    }
    return types;
}

void runCombat(GameEngine& engine) {
    std::cout << "\nCombat running...\n";
    int printed = 0;
    while (engine.snapshot().phase == Phase::Combat && printed < 1800) {
        engine.tick(1.0 / 30.0);
        ++printed;
        if (printed % 30 == 0) {
            printEvents(engine);
        } else {
            engine.consumeEvents();
        }
    }
    printEvents(engine);
}

void autoDeployPlayerOne(GameEngine& engine) {
    std::vector<UnitType> picks = {
        UnitType::ShieldGuard,
        UnitType::Archer,
        UnitType::Cleric,
        UnitType::Bomber
    };
    std::vector<Coord> spots = {{2, 3}, {1, 2}, {0, 3}, {1, 4}};
    for (UnitType type : picks) engine.buyUnit(PlayerId::One, type);
    GameSnapshot snapshot = engine.snapshot();
    for (size_t i = 0; i < snapshot.players[0].bench.size() && i < spots.size(); ++i) {
        engine.deployUnit(PlayerId::One, snapshot.players[0].bench[i], spots[i]);
    }
}

} // namespace

int main() {
    std::cout << "AutoChess 2D Debug CLI\n";
    std::cout << "1. Single Player vs AI\n2. Two Player\nInput mode: ";

    std::string modeText;
    std::cin >> modeText;

    GameEngine engine(42);
    engine.startNewGame(modeText == "2" ? GameMode::TwoPlayer : GameMode::SinglePlayerVsAi);
    printEvents(engine);

    while (engine.snapshot().phase != Phase::Finished) {
        GameSnapshot snapshot = engine.snapshot();
        if (snapshot.phase == Phase::Combat) {
            runCombat(engine);
            continue;
        }

        printBoard(snapshot);
        printShop(engine);
        std::cout << "\nCommands:\n"
                  << "  b <shop_index>        buy unit\n"
                  << "  d <unit_id> <x> <y>   deploy or move unit\n"
                  << "  u <unit_id>           upgrade unit\n"
                  << "  auto                  buy/deploy a sample P1 army\n"
                  << "  ready                 start combat\n"
                  << "  q                     quit\n> ";

        std::string command;
        std::cin >> command;
        if (command == "q") break;
        if (command == "auto") {
            autoDeployPlayerOne(engine);
            printEvents(engine);
            continue;
        }
        if (command == "ready") {
            engine.setReady(PlayerId::One, true);
            if (modeText == "2") engine.setReady(PlayerId::Two, true);
            printEvents(engine);
            continue;
        }
        if (command == "b") {
            int index = 0;
            std::cin >> index;
            std::vector<UnitType> types = shopTypes(engine);
            if (index >= 1 && index <= static_cast<int>(types.size()) &&
                engine.buyUnit(PlayerId::One, types[index - 1])) {
                std::cout << "Bought.\n";
            } else {
                std::cout << "Cannot buy that unit.\n";
            }
            printEvents(engine);
            continue;
        }
        if (command == "d") {
            UnitId id;
            Coord coord;
            std::cin >> id >> coord.x >> coord.y;
            if (!engine.deployUnit(PlayerId::One, id, coord)) {
                std::cout << "Cannot deploy or move that unit there.\n";
            }
            printEvents(engine);
            continue;
        }
        if (command == "u") {
            UnitId id;
            std::cin >> id;
            if (!engine.upgradeUnit(PlayerId::One, id)) {
                std::cout << "Cannot upgrade that unit.\n";
            }
            printEvents(engine);
            continue;
        }

        std::cout << "Unknown command.\n";
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    GameSnapshot finalSnapshot = engine.snapshot();
    if (finalSnapshot.winner) {
        std::cout << "\nWinner: " << toString(*finalSnapshot.winner) << "\n";
    }
    return 0;
}
