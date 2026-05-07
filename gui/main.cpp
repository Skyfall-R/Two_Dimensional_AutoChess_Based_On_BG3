#include <lib.hpp>

#include <raylib.h>

#include <algorithm>
#include <array>
#include <sstream>
#include <string>
#include <vector>

using namespace autochess;

namespace {

constexpr int kScreenWidth = 1920;
constexpr int kScreenHeight = 1080;
constexpr float kCell = 88.0f;
constexpr float kBoardX = 44.0f;
constexpr float kBoardY = 128.0f;
constexpr float kSideX = 1050.0f;
constexpr float kShopY = 150.0f;
constexpr float kShopCardW = 820.0f;
constexpr float kShopCardH = 144.0f;
constexpr float kShopGap = 14.0f;
constexpr float kShopViewportH = 560.0f;

Font gFont{};
bool gCustomFont = false;

bool isShopUnit(const UnitSpec& spec) {
    return spec.cost > 0 && spec.type != UnitType::DefenseTower &&
           spec.type != UnitType::SkeletonByWitch && spec.type != UnitType::Treant;
}

Rectangle boardRect() {
    return {kBoardX, kBoardY, kCell * kBoardWidth, kCell * kBoardHeight};
}

Rectangle benchRect() {
    return {kBoardX, 812.0f, kCell * 10.0f + 30.0f, 194.0f};
}

Rectangle readyRect() {
    return {1650.0f, 38.0f, 220.0f, 62.0f};
}

Rectangle detailRect() {
    return {kSideX, 738.0f, kShopCardW, 226.0f};
}

Rectangle shopViewportRect() {
    return {kSideX, kShopY, kShopCardW, kShopViewportH};
}

Rectangle shopCardRect(int index, float scroll) {
    return {kSideX,
            kShopY + index * (kShopCardH + kShopGap) - scroll,
            kShopCardW,
            kShopCardH};
}

Rectangle benchSlotRect(int index) {
    Rectangle bench = benchRect();
    return {bench.x + 12.0f + index * (kCell + 2.0f), bench.y + 26.0f, kCell, kCell};
}

std::vector<UnitType> shopTypes(const GameEngine& engine) {
    std::vector<UnitType> types;
    for (const UnitSpec& spec : engine.shop()) {
        if (isShopUnit(spec)) types.push_back(spec.type);
    }
    return types;
}

int shopUnitCount(const GameEngine& engine) {
    return static_cast<int>(shopTypes(engine).size());
}

float shopContentHeight(const GameEngine& engine) {
    int count = shopUnitCount(engine);
    if (count <= 0) return 0.0f;
    return count * kShopCardH + (count - 1) * kShopGap;
}

float clampShopScroll(const GameEngine& engine, float scroll) {
    float maxScroll = std::max(0.0f, shopContentHeight(engine) - shopViewportRect().height);
    return std::max(0.0f, std::min(scroll, maxScroll));
}

const UnitSpec* shopSpecAtIndex(const GameEngine& engine, int index) {
    int card = 0;
    for (const UnitSpec& spec : engine.shop()) {
        if (!isShopUnit(spec)) continue;
        if (card == index) return &spec;
        ++card;
    }
    return nullptr;
}

const UnitView* findUnit(const GameSnapshot& snapshot, UnitId id) {
    for (const UnitView& unit : snapshot.units) {
        if (unit.id == id) return &unit;
    }
    return nullptr;
}

const UnitView* unitAtCell(const GameSnapshot& snapshot, Coord coord, bool ownOnly) {
    const UnitView* air = nullptr;
    const UnitView* land = nullptr;
    for (const UnitView& unit : snapshot.units) {
        if (!unit.alive || !unit.deployed || unit.coord != coord) continue;
        if (ownOnly && unit.owner != PlayerId::One) continue;
        if (unit.layer == UnitLayer::Air) air = &unit;
        if (unit.layer == UnitLayer::Land) land = &unit;
    }
    return air ? air : land;
}

bool mouseToCell(Vector2 mouse, Coord& coord) {
    Rectangle rect = boardRect();
    if (!CheckCollisionPointRec(mouse, rect)) return false;
    coord.x = static_cast<int>((mouse.x - rect.x) / kCell);
    coord.y = static_cast<int>((mouse.y - rect.y) / kCell);
    return coord.x >= 0 && coord.x < kBoardWidth && coord.y >= 0 && coord.y < kBoardHeight;
}

Rectangle cellRect(Coord coord) {
    return {kBoardX + coord.x * kCell, kBoardY + coord.y * kCell, kCell, kCell};
}

Color playerColor(PlayerId player) {
    return player == PlayerId::One ? Color{61, 127, 255, 255} : Color{230, 76, 76, 255};
}

Color unitFill(const UnitView& unit) {
    Color color = playerColor(unit.owner);
    if (unit.slowed) color = Color{80, 190, 230, 255};
    return color;
}

void drawText(const std::string& text, float x, float y, float size, Color color) {
    DrawTextEx(gFont, text.c_str(), {x, y}, size, 1.0f, color);
}

Vector2 measureText(const std::string& text, float size) {
    return MeasureTextEx(gFont, text.c_str(), size, 1.0f);
}

std::string fitText(const std::string& text, float maxWidth, float size) {
    if (measureText(text, size).x <= maxWidth) return text;
    std::string clipped = text;
    while (!clipped.empty() && measureText(clipped + "...", size).x > maxWidth) {
        clipped.pop_back();
    }
    return clipped.empty() ? text.substr(0, 1) : clipped + "...";
}

float drawWrappedText(const std::string& text, float x, float y, float maxWidth, float size, Color color) {
    std::istringstream words(text);
    std::string word;
    std::string line;
    const float lineHeight = size + 5.0f;
    while (words >> word) {
        std::string candidate = line.empty() ? word : line + " " + word;
        if (measureText(candidate, size).x <= maxWidth) {
            line = candidate;
            continue;
        }
        if (!line.empty()) {
            drawText(line, x, y, size, color);
            y += lineHeight;
        }
        line = word;
        if (measureText(line, size).x > maxWidth) {
            drawText(fitText(line, maxWidth, size), x, y, size, color);
            y += lineHeight;
            line.clear();
        }
    }
    if (!line.empty()) {
        drawText(line, x, y, size, color);
        y += lineHeight;
    }
    return y;
}

void drawTextCentered(const std::string& text, Rectangle rect, float size, Color color) {
    Vector2 measured = measureText(text, size);
    drawText(text, rect.x + (rect.width - measured.x) / 2.0f,
             rect.y + (rect.height - size) / 2.0f, size, color);
}

std::string targetText(const UnitSpec& spec) {
    if (spec.canAttackLand && spec.canAttackAir) return "Land/Air";
    if (spec.canAttackLand) return "Land";
    if (spec.canAttackAir) return "Air";
    return "None";
}

std::string abilityText(const UnitSpec& spec) {
    switch (spec.ability) {
        case AbilityKind::None:
            return "Ability: none. Straightforward stat unit.";
        case AbilityKind::PekkaHeavySwing:
            return "Ability: heavy swing. Skips every odd swing, then lands a very high damage hit.";
        case AbilityKind::WitchSummon:
            return "Ability: summons 2 skeleton units near herself every 1 second if a land tile is open. Summons are cleared after the round.";
        case AbilityKind::BalloonBomb:
            return "Ability: death bomb. On death, deals 60 damage to nearby enemy land units.";
        case AbilityKind::PrinceCharge:
            return "Ability: charge. After moving, the next attack deals double damage.";
        case AbilityKind::DragonBreath:
            return "Ability: breath attack. Hits every valid enemy in range instead of only the selected target.";
        case AbilityKind::ShieldGuard:
            return "Ability: taunt guard. Permanently taunts nearby target selection and gains 40 shield every 4 seconds, capped at 120.";
        case AbilityKind::ClericHeal:
            return "Ability: heal. Every 1.2 seconds, heals the most wounded ally within range 3 for 25 HP.";
        case AbilityKind::FrostNova:
            return "Ability: frost nova. Attacks splash around the target and slow affected enemies for 2 seconds.";
        case AbilityKind::BomberSplash:
            return "Ability: splash bomb. Attacks damage enemy land units adjacent to the target.";
        case AbilityKind::AssassinLeap:
            return "Ability: backline leap. At combat start, jumps next to a high-value non-tower enemy.";
        case AbilityKind::DruidSummon:
            return "Ability: treant summon. Every 5 seconds, summons a temporary Treant near himself.";
        case AbilityKind::LancerPierce:
            return "Ability: pierce. Damages the target and also hits the land unit behind it for half damage.";
        case AbilityKind::StormChain:
            return "Ability: storm chain. Hits the target, then chains 60 percent damage to up to 2 nearby enemies.";
    }
    return "Ability: unknown.";
}

std::string abilitySummary(const UnitSpec& spec) {
    switch (spec.ability) {
        case AbilityKind::None: return "No special ability.";
        case AbilityKind::PekkaHeavySwing: return "Heavy swing: every other swing deals huge damage.";
        case AbilityKind::WitchSummon: return "Summons temporary skeletons during combat.";
        case AbilityKind::BalloonBomb: return "Explodes on death, damaging nearby land units.";
        case AbilityKind::PrinceCharge: return "After moving, next hit deals double damage.";
        case AbilityKind::DragonBreath: return "Breath attack hits every enemy in range.";
        case AbilityKind::ShieldGuard: return "Taunts enemies and gains shield over time.";
        case AbilityKind::ClericHeal: return "Heals the most wounded nearby ally.";
        case AbilityKind::FrostNova: return "Splash attack slows affected enemies.";
        case AbilityKind::BomberSplash: return "Splash damage around the target.";
        case AbilityKind::AssassinLeap: return "Leaps to a valuable backline enemy at combat start.";
        case AbilityKind::DruidSummon: return "Summons temporary Treants during combat.";
        case AbilityKind::LancerPierce: return "Pierces the target and damages the unit behind.";
        case AbilityKind::StormChain: return "Chains damage from target to nearby enemies.";
    }
    return "Unknown ability.";
}

Font loadUiFont() {
    const std::array<const char*, 3> candidates = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/msyh.ttc"
    };
    for (const char* path : candidates) {
        if (!FileExists(path)) continue;
        Font font = LoadFontEx(path, 64, nullptr, 0);
        if (font.texture.id != 0) {
            SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
            gCustomFont = true;
            return font;
        }
    }
    gCustomFont = false;
    return GetFontDefault();
}

bool hasPlayerCombatUnit(const GameSnapshot& snapshot) {
    for (const UnitView& unit : snapshot.units) {
        if (unit.owner == PlayerId::One && unit.alive && unit.deployed &&
            unit.type != UnitType::DefenseTower) {
            return true;
        }
    }
    return false;
}

const UnitSpec* firstVisibleShopSpec(const GameEngine& engine, float shopScroll) {
    int index = static_cast<int>(shopScroll / (kShopCardH + kShopGap));
    return shopSpecAtIndex(engine, index);
}

const UnitSpec* shopSpecAtMouse(const GameEngine& engine, Vector2 mouse, float shopScroll) {
    if (!CheckCollisionPointRec(mouse, shopViewportRect())) return nullptr;
    for (int i = 0; i < shopUnitCount(engine); ++i) {
        Rectangle r = shopCardRect(i, shopScroll);
        if (r.y + r.height < shopViewportRect().y || r.y > shopViewportRect().y + shopViewportRect().height) continue;
        if (CheckCollisionPointRec(mouse, r)) return shopSpecAtIndex(engine, i);
    }
    return nullptr;
}

const UnitView* unitAtMouse(const GameSnapshot& snapshot, Vector2 mouse) {
    Coord coord;
    if (mouseToCell(mouse, coord)) return unitAtCell(snapshot, coord, false);

    for (int i = 0; i < static_cast<int>(snapshot.players[0].bench.size()); ++i) {
        Rectangle slot = benchSlotRect(i);
        if (CheckCollisionPointRec(mouse, slot)) return findUnit(snapshot, snapshot.players[0].bench[i]);
    }
    return nullptr;
}

void drawUnitDetails(const UnitSpec& spec, const UnitView* view) {
    Rectangle panel = detailRect();
    DrawRectangleRounded(panel, 0.04f, 8, Color{34, 40, 46, 255});
    DrawRectangleRoundedLines(panel, 0.04f, 8, 1.0f, Color{74, 86, 94, 255});

    float x = panel.x + 20.0f;
    float y = panel.y + 16.0f;
    std::string title = spec.name;
    if (view) title += TextFormat("  #%d", view->id);
    drawText(fitText(title, panel.width - 40.0f, 38.0f), x, y, 38.0f, RAYWHITE);
    y += 52.0f;

    drawText(TextFormat("Cost %d   Units x%d   HP %d each / %d total",
                        spec.cost, spec.unitCount, spec.maxHp, spec.maxHp * spec.unitCount),
             x, y, 26.0f, LIGHTGRAY);
    y += 38.0f;
    drawText(TextFormat("ATK %d   Range %d   Speed %.1f   %s   Targets %s",
                        spec.attack, spec.range, spec.speed, toString(spec.layer).c_str(),
                        targetText(spec).c_str()),
             x, y, 26.0f, LIGHTGRAY);
    y += 38.0f;

    if (view) {
        drawText(TextFormat("Current HP %d/%d   Shield %d   Owner %s",
                            view->totalHp, view->maxTotalHp, view->shield, toString(view->owner).c_str()),
                 x, y, 23.0f, Color{180, 210, 255, 255});
        y += 34.0f;
    }

    drawWrappedText(abilityText(spec), x, y, panel.width - 40.0f, 23.0f, Color{210, 216, 220, 255});
}

void drawUnit(const UnitView& unit, Rectangle rect, bool ghost = false) {
    Color fill = unitFill(unit);
    if (ghost) fill.a = 150;
    Vector2 center{rect.x + rect.width / 2.0f, rect.y + rect.height / 2.0f};

    if (unit.layer == UnitLayer::Air) {
        DrawCircleV(center, rect.width * 0.31f, fill);
        DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y),
                        rect.width * 0.31f, DARKBLUE);
    } else {
        Rectangle body{rect.x + 10.0f, rect.y + 10.0f, rect.width - 20.0f, rect.height - 20.0f};
        DrawRectangleRounded(body, 0.12f, 6, fill);
        DrawRectangleRoundedLines(body, 0.12f, 6, 1.0f, DARKGRAY);
    }

    drawTextCentered(unit.shortName, rect, 28.0f, WHITE);
    float hpRatio = unit.maxTotalHp > 0 ? static_cast<float>(unit.totalHp) / unit.maxTotalHp : 0.0f;
    Rectangle hpBack{rect.x + 8.0f, rect.y + rect.height - 12.0f, rect.width - 16.0f, 7.0f};
    DrawRectangleRec(hpBack, Color{40, 40, 40, 255});
    DrawRectangleRec({hpBack.x, hpBack.y, hpBack.width * hpRatio, hpBack.height}, GREEN);
    if (unit.shield > 0) {
        DrawRectangleRec({hpBack.x, hpBack.y - 8.0f, hpBack.width * std::min(1.0f, unit.shield / 120.0f), 5.0f},
                         SKYBLUE);
    }
}

void drawBoard(const GameEngine& engine, const GameSnapshot& snapshot, UnitId dragging) {
    Rectangle rect = boardRect();
    DrawRectangleRec(rect, Color{32, 38, 44, 255});

    Coord hover;
    bool hasHover = mouseToCell(GetMousePosition(), hover);
    const UnitView* draggingUnit = dragging != kInvalidUnitId ? findUnit(snapshot, dragging) : nullptr;

    for (int y = 0; y < snapshot.height; ++y) {
        for (int x = 0; x < snapshot.width; ++x) {
            Coord coord{x, y};
            Rectangle cell = cellRect(coord);
            Color fill = Color{48, 57, 64, 255};
            if (engine.isDeploymentCell(PlayerId::One, coord)) fill = Color{41, 70, 92, 255};
            if (engine.isDeploymentCell(PlayerId::Two, coord)) fill = Color{86, 48, 52, 255};
            DrawRectangleRec({cell.x + 1, cell.y + 1, cell.width - 2, cell.height - 2}, fill);
            DrawRectangleLinesEx(cell, 1.0f, Color{78, 88, 96, 255});

            if (hasHover && hover == coord && draggingUnit && snapshot.phase == Phase::Preparation) {
                bool legal = engine.canDeploy(PlayerId::One, coord, draggingUnit->layer);
                DrawRectangleLinesEx({cell.x + 3, cell.y + 3, cell.width - 6, cell.height - 6},
                                     3.0f, legal ? GREEN : RED);
            }
        }
    }

    for (const UnitView& unit : snapshot.units) {
        if (!unit.alive || !unit.deployed || unit.id == dragging) continue;
        Rectangle cell = cellRect(unit.coord);
        Rectangle inset{cell.x + 4.0f, cell.y + 4.0f, cell.width - 8.0f, cell.height - 8.0f};
        if (unit.layer == UnitLayer::Air) {
            inset.x += 14.0f;
            inset.y -= 2.0f;
            inset.width -= 12.0f;
            inset.height -= 12.0f;
        }
        drawUnit(unit, inset);
    }
}

void drawShop(GameEngine& engine, const GameSnapshot& snapshot, float shopScroll) {
    Rectangle viewport = shopViewportRect();
    drawText("Shop", kSideX, 104.0f, 38.0f, RAYWHITE);
    drawText("Mouse wheel to browse", kSideX + 104.0f, 120.0f, 22.0f, LIGHTGRAY);

    DrawRectangleRounded(viewport, 0.035f, 8, Color{29, 35, 40, 255});
    BeginScissorMode(static_cast<int>(viewport.x), static_cast<int>(viewport.y),
                     static_cast<int>(viewport.width), static_cast<int>(viewport.height));

    for (int card = 0; card < shopUnitCount(engine); ++card) {
        const UnitSpec* current = shopSpecAtIndex(engine, card);
        if (!current) continue;
        const UnitSpec& spec = *current;
        Rectangle r = shopCardRect(card, shopScroll);
        if (r.y + r.height < viewport.y || r.y > viewport.y + viewport.height) continue;

        bool hover = CheckCollisionPointRec(GetMousePosition(), r);
        Color fill = hover ? Color{68, 80, 88, 255} : Color{48, 56, 62, 255};
        if (snapshot.players[0].money < spec.cost) fill = Color{56, 45, 48, 255};
        DrawRectangleRounded(r, 0.06f, 6, fill);
        DrawRectangleRoundedLines(r, 0.06f, 6, 1.0f, Color{73, 86, 94, 255});

        drawText(std::to_string(card + 1), r.x + 18.0f, r.y + 19.0f, 26.0f, GOLD);
        drawText(fitText(spec.name, 480.0f, 34.0f), r.x + 72.0f, r.y + 14.0f, 34.0f, RAYWHITE);
        drawText(TextFormat("$%d", spec.cost), r.x + r.width - 82.0f, r.y + 17.0f, 32.0f, GOLD);
        drawText(TextFormat("x%d  HP %d  ATK %d  R%d",
                            spec.unitCount, spec.maxHp, spec.attack, spec.range),
                 r.x + 72.0f, r.y + 62.0f, 25.0f, LIGHTGRAY);
        drawText(toString(spec.layer), r.x + r.width - 154.0f, r.y + 62.0f, 25.0f, LIGHTGRAY);
        drawText(fitText(abilitySummary(spec), r.width - 112.0f, 22.0f),
                 r.x + 72.0f, r.y + 102.0f, 22.0f, Color{205, 212, 216, 255});
    }
    EndScissorMode();

    float contentHeight = shopContentHeight(engine);
    if (contentHeight > viewport.height) {
        float barHeight = std::max(36.0f, viewport.height * viewport.height / contentHeight);
        float maxScroll = contentHeight - viewport.height;
        float barY = viewport.y + (viewport.height - barHeight) * (shopScroll / maxScroll);
        Rectangle track{viewport.x + viewport.width - 8.0f, viewport.y + 4.0f, 4.0f, viewport.height - 8.0f};
        Rectangle thumb{track.x - 1.0f, barY, 6.0f, barHeight};
        DrawRectangleRounded(track, 1.0f, 4, Color{57, 66, 72, 255});
        DrawRectangleRounded(thumb, 1.0f, 4, Color{148, 164, 174, 255});
    }
}

void drawBench(const GameSnapshot& snapshot, UnitId dragging) {
    Rectangle area = benchRect();
    DrawRectangleRounded(area, 0.04f, 8, Color{34, 40, 46, 255});
    drawText("Bench", area.x, area.y - 42.0f, 32.0f, RAYWHITE);

    for (int i = 0; i < 10; ++i) {
        Rectangle slot = benchSlotRect(i);
        DrawRectangleRounded(slot, 0.1f, 6, Color{48, 56, 62, 255});
        DrawRectangleRoundedLines(slot, 0.1f, 6, 1.0f, Color{84, 94, 102, 255});
        if (i < static_cast<int>(snapshot.players[0].bench.size())) {
            UnitId id = snapshot.players[0].bench[i];
            if (id != dragging) {
                const UnitView* unit = findUnit(snapshot, id);
                if (unit) drawUnit(*unit, {slot.x + 4, slot.y + 4, slot.width - 8, slot.height - 8});
            }
        }
    }
}

void appendEvents(GameEngine& engine, std::vector<std::string>& log) {
    for (const Event& event : engine.consumeEvents()) {
        if (!event.text.empty()) log.push_back(event.text);
    }
    if (log.size() > 3) log.erase(log.begin(), log.end() - 3);
}

} // namespace

int main() {
    GameEngine engine(1234);
    engine.startNewGame(GameMode::SinglePlayerVsAi);

    SetTraceLogLevel(LOG_WARNING);
    InitWindow(kScreenWidth, kScreenHeight, "AutoChess 2D");
    SetTargetFPS(60);
    gFont = loadUiFont();

    UnitId dragging = kInvalidUnitId;
    float shopScroll = 0.0f;
    double accumulator = 0.0;
    std::vector<std::string> log;
    appendEvents(engine, log);

    while (!WindowShouldClose()) {
        float frameTime = GetFrameTime();
        GameSnapshot snapshot = engine.snapshot();

        if (snapshot.phase == Phase::Combat) {
            accumulator += frameTime;
            while (accumulator >= 1.0 / 30.0) {
                engine.tick(1.0 / 30.0);
                accumulator -= 1.0 / 30.0;
            }
        } else {
            accumulator = 0.0;
            engine.tick(frameTime);
        }
        appendEvents(engine, log);
        snapshot = engine.snapshot();

        Vector2 mouse = GetMousePosition();
        shopScroll = clampShopScroll(engine, shopScroll);
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f && CheckCollisionPointRec(mouse, shopViewportRect())) {
            shopScroll = clampShopScroll(engine, shopScroll - wheel * 72.0f);
        }

        const UnitView* focusedUnit = unitAtMouse(snapshot, mouse);
        const UnitSpec* focusedSpec = shopSpecAtMouse(engine, mouse, shopScroll);
        if (!focusedSpec && focusedUnit) focusedSpec = engine.specFor(focusedUnit->type);
        if (!focusedSpec) focusedSpec = firstVisibleShopSpec(engine, shopScroll);

        if (snapshot.phase == Phase::Preparation && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (CheckCollisionPointRec(mouse, readyRect())) {
                if (hasPlayerCombatUnit(snapshot)) {
                    engine.setReady(PlayerId::One, true);
                } else {
                    log.push_back("Deploy at least one unit first.");
                }
            } else {
                bool bought = false;
                if (const UnitSpec* spec = shopSpecAtMouse(engine, mouse, shopScroll)) {
                    if (!engine.buyUnit(PlayerId::One, spec->type)) log.push_back("Cannot buy that unit.");
                    bought = true;
                }
                if (!bought) {
                    for (int i = 0; i < static_cast<int>(snapshot.players[0].bench.size()); ++i) {
                        Rectangle slot = benchSlotRect(i);
                        if (CheckCollisionPointRec(mouse, slot)) {
                            dragging = snapshot.players[0].bench[i];
                            break;
                        }
                    }
                    if (dragging == kInvalidUnitId) {
                        Coord coord;
                        if (mouseToCell(mouse, coord)) {
                            const UnitView* unit = unitAtCell(snapshot, coord, true);
                            if (unit && unit->type != UnitType::DefenseTower) dragging = unit->id;
                        }
                    }
                }
            }
        }

        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && dragging != kInvalidUnitId) {
            Coord coord;
            if (mouseToCell(mouse, coord)) {
                if (!engine.deployUnit(PlayerId::One, dragging, coord)) log.push_back("Cannot deploy there.");
            } else if (CheckCollisionPointRec(mouse, benchRect())) {
                engine.returnToBench(PlayerId::One, dragging);
            }
            dragging = kInvalidUnitId;
        }
        if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) || IsKeyPressed(KEY_ESCAPE)) {
            dragging = kInvalidUnitId;
        }

        appendEvents(engine, log);
        snapshot = engine.snapshot();

        BeginDrawing();
        ClearBackground(Color{24, 28, 32, 255});

        drawText("AutoChess 2D", kBoardX, 38.0f, 44.0f, RAYWHITE);
        drawText(TextFormat("Round %d  %s", snapshot.round, toString(snapshot.phase).c_str()),
                 350.0f, 52.0f, 28.0f, LIGHTGRAY);
        drawText(TextFormat("Gold: %d", snapshot.players[0].money), 680.0f, 51.0f, 30.0f, GOLD);
        bool canStart = snapshot.phase == Phase::Preparation && hasPlayerCombatUnit(snapshot);
        DrawRectangleRounded(readyRect(), 0.12f, 8,
                             snapshot.phase == Phase::Preparation
                                 ? (canStart ? Color{61, 127, 255, 255} : Color{70, 74, 78, 255})
                                 : Color{70, 74, 78, 255});
        drawTextCentered(snapshot.phase == Phase::Preparation ? "Start" : "Fighting", readyRect(), 30.0f, WHITE);

        drawBoard(engine, snapshot, dragging);
        drawShop(engine, snapshot, shopScroll);
        drawBench(snapshot, dragging);
        if (focusedSpec) drawUnitDetails(*focusedSpec, focusedUnit);

        if (dragging != kInvalidUnitId) {
            const UnitView* unit = findUnit(snapshot, dragging);
            if (unit) {
                Rectangle ghost{mouse.x - 44.0f, mouse.y - 44.0f, 88.0f, 88.0f};
                drawUnit(*unit, ghost, true);
            }
        }

        drawText("Log", kSideX, 966.0f, 30.0f, RAYWHITE);
        for (int i = 0; i < static_cast<int>(log.size()); ++i) {
            drawText(fitText(log[i], kShopCardW, 22.0f), kSideX, 1002.0f + i * 26.0f, 22.0f, LIGHTGRAY);
        }

        if (snapshot.winner) {
            Rectangle overlay{0, 0, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)};
            DrawRectangleRec(overlay, Color{0, 0, 0, 170});
            std::string text = toString(*snapshot.winner) + " wins";
            drawTextCentered(text, {0, 320, static_cast<float>(kScreenWidth), 80}, 46.0f, RAYWHITE);
        }

        EndDrawing();
    }

    if (gCustomFont) UnloadFont(gFont);
    CloseWindow();
    return 0;
}
