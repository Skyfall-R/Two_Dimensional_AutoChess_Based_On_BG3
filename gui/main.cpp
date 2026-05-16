#include <lib.hpp>
#include <campaign.hpp>

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace autochess;

namespace {

constexpr int kScreenWidth = 2560;
constexpr int kScreenHeight = 1600;
constexpr float kCell = 56.0f;
constexpr float kBoardX = 18.0f;
constexpr float kBoardY = 104.0f;
constexpr float kSideX = 1936.0f;
constexpr float kSideW = 606.0f;
constexpr float kDetailY = 808.0f;
constexpr float kDetailH = 624.0f;
constexpr float kLogY = 1448.0f;
constexpr float kLogH = 134.0f;
constexpr float kFontScale = 1.10f;
constexpr float kFontSpacing = 0.0f;
constexpr int kUiFontAtlasSize = 96;
constexpr float kShopY = 142.0f;
constexpr float kShopCardW = kSideW;
constexpr float kShopCardH = 104.0f;
constexpr float kShopGap = 8.0f;
constexpr float kShopViewportH = 612.0f;
constexpr int kBattleLogVisibleLines = 3;
constexpr int kUnitTextureCount = static_cast<int>(UnitType::NeutralTamiaHolzt) + 1;
constexpr int kNeutralFamilyTextureCount = static_cast<int>(NeutralFamily::Artillery) + 1;
constexpr int kAbilityTextureCount = static_cast<int>(AbilityKind::BossFireball) + 1;

Font gFont{};
bool gCustomFont = false;
Font gBoldFont{};
bool gCustomBoldFont = false;
Texture2D gBannerTexture{};
bool gBannerLoaded = false;
std::array<Texture2D, kUnitTextureCount> gUnitTextures{};
std::array<Texture2D, kNeutralFamilyTextureCount> gNeutralFamilyTextures{};
std::array<Texture2D, kAbilityTextureCount> gAbilityTextures{};
std::unordered_map<std::string, Texture2D> gRelicTextures{};
Texture2D gFactionBlueSealTexture{};
Texture2D gFactionRedSealTexture{};
Texture2D gMeleeHitTexture{};
Vector2 gMousePosition{};
const std::filesystem::path kCacheDir = ".cache";
const std::filesystem::path kLatestEventLog = kCacheDir / "latest-gui-events.log";
const std::array<std::filesystem::path, 4> kUiAssetRoots = {
    std::filesystem::path("assets/ui/bg3"),
    std::filesystem::path("assets/ui/generated"),
    std::filesystem::path("assets/ui/deprecated_generated"),
    std::filesystem::path("assets/ui")
};
const Color kInk = Color{242, 232, 211, 255};
const Color kMutedInk = Color{194, 180, 151, 255};
const Color kParchmentInk = Color{39, 29, 22, 255};
const Color kParchmentMuted = Color{91, 71, 49, 255};
const Color kAccent = Color{204, 166, 92, 255};
const Color kAccentAlt = Color{108, 151, 145, 255};
const Color kPanel = Color{24, 18, 16, 255};
const Color kPanelDark = Color{10, 8, 8, 255};
const Color kBorder = Color{142, 105, 55, 255};
const Color kLeather = Color{34, 23, 19, 255};
const Color kLeatherDark = Color{13, 9, 8, 255};
const Color kParchment = Color{220, 202, 163, 255};
const Color kParchmentDark = Color{128, 92, 52, 255};
const Color kGold = Color{215, 176, 94, 255};
const Color kWine = Color{96, 43, 48, 255};
const Color kArcaneBlue = Color{78, 124, 139, 255};
const Color kPlayerBlue = Color{78, 142, 184, 255};
const Color kEnemyRed = Color{181, 78, 76, 255};

enum class CombatCueVisualKind {
    Melee,
    Arrow,
    Spell,
    Heal,
    Gold,
    Shield,
    Knockback,
    Leap,
    ReturnHome
};

struct CombatCue {
    EventType type = EventType::UnitAttacked;
    UnitId targetId = kInvalidUnitId;
    Coord from;
    Coord to;
    PlayerId player = PlayerId::One;
    float age = 0.0f;
    float ttl = 0.45f;
    int amount = 0;
    AbilityKind ability = AbilityKind::None;
    bool arcane = false;
    CombatCueVisualKind visual = CombatCueVisualKind::Melee;
    std::string text;
};

bool isKnockbackCueText(const std::string& text) {
    return text.find("knocked back") != std::string::npos ||
           text.find("shoved back") != std::string::npos ||
           text.find("repelled") != std::string::npos;
}

Color knockbackCueColor(const CombatCue& cue) {
    switch (cue.ability) {
        case AbilityKind::DiabolicChains:
        case AbilityKind::BossFireball:
            return Color{214, 82, 49, 255};
        case AbilityKind::KethericSmite:
            return Color{226, 197, 102, 255};
        case AbilityKind::OwlbearMultiattack:
            return Color{173, 118, 70, 255};
        case AbilityKind::MinotaurCharge:
            return Color{199, 111, 72, 255};
        default:
            return Color{223, 176, 94, 255};
    }
}

Color arcaneCueColor(const CombatCue& cue) {
    switch (cue.visual) {
        case CombatCueVisualKind::Heal:
            return Color{112, 219, 135, 255};
        case CombatCueVisualKind::Gold:
            return Color{239, 190, 83, 255};
        case CombatCueVisualKind::Shield:
            return Color{119, 198, 230, 255};
        default:
            break;
    }
    switch (cue.ability) {
        case AbilityKind::MephitDeathBurst:
        case AbilityKind::DragonBreath:
        case AbilityKind::BossFireball:
        case AbilityKind::DiabolicChains:
            return Color{235, 91, 58, 255};
        case AbilityKind::FrostNova:
        case AbilityKind::SpectatorWoundingRay:
        case AbilityKind::MindBlast:
            return Color{126, 185, 229, 255};
        case AbilityKind::SelunesIre:
        case AbilityKind::StrikeOfTheGuardian:
            return Color{236, 212, 118, 255};
        case AbilityKind::Blight:
        case AbilityKind::DominatePerson:
            return Color{154, 104, 202, 255};
        default:
            return Color{126, 167, 210, 255};
    }
}

struct DraftState {
    int depth = -1;
    std::vector<DraftOffer> offers;
    std::string fixedDropText;
};

struct DetailPanelState {
    float scroll = 0.0f;
    UnitId unitId = kInvalidUnitId;
    UnitType type = UnitType::Skeleton;
    bool shopMode = true;
};

std::filesystem::path resolveAssetPath(const std::filesystem::path& relative) {
    std::filesystem::path current = std::filesystem::current_path();
    for (int i = 0; i < 8; ++i) {
        std::filesystem::path candidate = current / relative;
        if (std::filesystem::exists(candidate)) return candidate;
        if (!current.has_parent_path()) break;
        current = current.parent_path();
    }
    return relative;
}

std::filesystem::path uiAssetPath(const char* filename) {
    for (const std::filesystem::path& root : kUiAssetRoots) {
        std::filesystem::path candidate = root / filename;
        if (std::filesystem::exists(resolveAssetPath(candidate))) return candidate;
    }
    return kUiAssetRoots.front() / filename;
}

std::optional<Rectangle> alphaContentBounds(const Image& image) {
    if (image.width <= 0 || image.height <= 0) return std::nullopt;

    Color* pixels = LoadImageColors(image);
    if (!pixels) return std::nullopt;

    int minX = image.width;
    int minY = image.height;
    int maxX = -1;
    int maxY = -1;
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            const Color& pixel = pixels[y * image.width + x];
            if (pixel.a <= 12) continue;
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
        }
    }
    UnloadImageColors(pixels);

    if (maxX < minX || maxY < minY) return std::nullopt;
    int pad = std::max(2, std::min(image.width, image.height) / 18);
    minX = std::max(0, minX - pad);
    minY = std::max(0, minY - pad);
    maxX = std::min(image.width - 1, maxX + pad);
    maxY = std::min(image.height - 1, maxY + pad);
    return Rectangle{static_cast<float>(minX),
                     static_cast<float>(minY),
                     static_cast<float>(maxX - minX + 1),
                     static_cast<float>(maxY - minY + 1)};
}

Texture2D loadTextureIfExists(const std::filesystem::path& relative, bool cropTransparent = false) {
    std::filesystem::path resolved = resolveAssetPath(relative);
    if (!std::filesystem::exists(resolved)) return {};
    Image image = LoadImage(resolved.string().c_str());
    if (image.data == nullptr) return {};
    if (cropTransparent) {
        if (std::optional<Rectangle> crop = alphaContentBounds(image)) {
            bool meaningfulCrop = crop->width < image.width * 0.92f || crop->height < image.height * 0.92f;
            if (meaningfulCrop) ImageCrop(&image, *crop);
        }
    }
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    if (texture.id != 0) SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    return texture;
}

void unloadTextureIfLoaded(Texture2D& texture) {
    if (texture.id != 0) UnloadTexture(texture);
    texture = Texture2D{};
}

Texture2D loadUiTexture(const char* filename, bool cropTransparent = false) {
    return loadTextureIfExists(uiAssetPath(filename), cropTransparent);
}

void loadUnitTexture(UnitType type, const char* filename) {
    int index = static_cast<int>(type);
    if (index < 0 || index >= static_cast<int>(gUnitTextures.size())) return;
    gUnitTextures[static_cast<size_t>(index)] = loadUiTexture(filename, true);
}

void loadNeutralFamilyTexture(NeutralFamily family, const char* filename) {
    int index = static_cast<int>(family);
    if (index < 0 || index >= static_cast<int>(gNeutralFamilyTextures.size())) return;
    gNeutralFamilyTextures[static_cast<size_t>(index)] = loadUiTexture(filename);
}

void loadAbilityTexture(AbilityKind ability, const char* filename) {
    int index = static_cast<int>(ability);
    if (index < 0 || index >= static_cast<int>(gAbilityTextures.size())) return;
    gAbilityTextures[static_cast<size_t>(index)] = loadUiTexture(filename, true);
}

void loadRelicTextures() {
    for (const RelicSpec& relic : relicCatalog()) {
        std::string filename = "relics/assigned/relic_" + relic.id + ".png";
        Texture2D texture = loadUiTexture(filename.c_str());
        if (texture.id != 0) gRelicTextures[relic.id] = texture;
    }
}

void loadUiTextures() {
    gBannerTexture = loadUiTexture("tavern_banner.png");
    gBannerLoaded = gBannerTexture.id != 0;
    gFactionBlueSealTexture = loadUiTexture("faction_blue_seal.png");
    gFactionRedSealTexture = loadUiTexture("faction_red_seal.png");
    gMeleeHitTexture = loadUiTexture("melee_hit.png");

    loadUnitTexture(UnitType::Skeleton, "unit_skeleton.png");
    loadUnitTexture(UnitType::SkeletonByNecromancer, "unit_skeleton.png");
    loadUnitTexture(UnitType::GithyankiWarrior, "unit_githyanki_warrior.png");
    loadUnitTexture(UnitType::Ranger, "unit_ranger.png");
    loadUnitTexture(UnitType::Barbarian, "unit_berserker.png");
    loadUnitTexture(UnitType::Necromancer, "unit_necromancer.png");
    loadUnitTexture(UnitType::FireMephit, "unit_fire_mephit.png");
    loadUnitTexture(UnitType::ImpSwarm, "unit_imp_swarm.png");
    loadUnitTexture(UnitType::GoblinSkirmisher, "unit_goblin_ambusher.png");
    loadUnitTexture(UnitType::Paladin, "unit_paladin.png");
    loadUnitTexture(UnitType::NeutralSpectator, "unit_neutral_spectator_bright.png");
    loadUnitTexture(UnitType::NeutralOwlbear, "unit_neutral_owlbear.png");
    loadUnitTexture(UnitType::NeutralMindFlayer, "unit_neutral_mind_flayer.png");
    loadUnitTexture(UnitType::NeutralSovereignSpaw, "unit_neutral_sovereign_spaw.png");
    loadUnitTexture(UnitType::NeutralKarniss, "unit_neutral_karniss.png");
    loadUnitTexture(UnitType::NeutralRedcap, "unit_neutral_redcap.png");
    loadUnitTexture(UnitType::NeutralWaterMyrmidon, "unit_neutral_water_myrmidon.png");
    loadUnitTexture(UnitType::NeutralPhaseSpiderMatriarch, "unit_neutral_phase_spider_matriarch.png");
    loadUnitTexture(UnitType::NeutralRaphael, "unit_neutral_raphael.png");
    loadUnitTexture(UnitType::NeutralKethericThorm, "unit_neutral_ketheric_thorm.png");
    loadUnitTexture(UnitType::NeutralMoonlightSliver, "unit_neutral_moonlight_sliver.png");
    loadUnitTexture(UnitType::NeutralGuardianOfFaith, "unit_neutral_guardian_of_faith.png");
    loadUnitTexture(UnitType::NeutralMinotaur, "unit_neutral_minotaur.png");
    loadUnitTexture(UnitType::NeutralDeathKnight, "library/monsters/creatures/death_knight.png");
    loadUnitTexture(UnitType::NeutralAirMyrmidon, "unit_neutral_air_myrmidon.png");
    loadUnitTexture(UnitType::NeutralTamiaHolzt, "unit_neutral_tamia_holzt.png");
    if (gUnitTextures[static_cast<size_t>(UnitType::NeutralSpectator)].id == 0) {
        gUnitTextures[static_cast<size_t>(UnitType::NeutralSpectator)] =
            loadUiTexture("unit_neutral_spectator.png");
    }
    if (gUnitTextures[static_cast<size_t>(UnitType::NeutralSpectator)].id == 0) {
        gUnitTextures[static_cast<size_t>(UnitType::NeutralSpectator)] =
            loadUiTexture("neutral_caster.png");
    }
    if (gUnitTextures[static_cast<size_t>(UnitType::NeutralOwlbear)].id == 0) {
        gUnitTextures[static_cast<size_t>(UnitType::NeutralOwlbear)] =
            loadUiTexture("neutral_guardian.png");
    }
    if (gUnitTextures[static_cast<size_t>(UnitType::NeutralMindFlayer)].id == 0) {
        gUnitTextures[static_cast<size_t>(UnitType::NeutralMindFlayer)] =
            loadUiTexture("neutral_caster.png");
    }
    if (gUnitTextures[static_cast<size_t>(UnitType::NeutralSovereignSpaw)].id == 0) {
        gUnitTextures[static_cast<size_t>(UnitType::NeutralSovereignSpaw)] =
            loadUiTexture("neutral_guardian.png");
    }
    if (gUnitTextures[static_cast<size_t>(UnitType::NeutralKarniss)].id == 0) {
        gUnitTextures[static_cast<size_t>(UnitType::NeutralKarniss)] =
            loadUiTexture("neutral_assassin.png");
    }
    if (gUnitTextures[static_cast<size_t>(UnitType::NeutralRedcap)].id == 0) {
        gUnitTextures[static_cast<size_t>(UnitType::NeutralRedcap)] =
            loadUiTexture("neutral_swarm.png");
    }
    if (gUnitTextures[static_cast<size_t>(UnitType::NeutralWaterMyrmidon)].id == 0) {
        gUnitTextures[static_cast<size_t>(UnitType::NeutralWaterMyrmidon)] =
            loadUiTexture("neutral_guardian.png");
    }
    if (gUnitTextures[static_cast<size_t>(UnitType::NeutralPhaseSpiderMatriarch)].id == 0) {
        gUnitTextures[static_cast<size_t>(UnitType::NeutralPhaseSpiderMatriarch)] =
            loadUiTexture("library/monsters/creatures/aggressive_crag_spider.png");
    }
    if (gUnitTextures[static_cast<size_t>(UnitType::NeutralPhaseSpiderMatriarch)].id == 0) {
        gUnitTextures[static_cast<size_t>(UnitType::NeutralPhaseSpiderMatriarch)] =
            loadUiTexture("neutral_assassin.png");
    }
    if (gUnitTextures[static_cast<size_t>(UnitType::NeutralRaphael)].id == 0) {
        gUnitTextures[static_cast<size_t>(UnitType::NeutralRaphael)] =
            loadUiTexture("library/quests/deal_with_the_devil.png");
    }
    if (gUnitTextures[static_cast<size_t>(UnitType::NeutralRaphael)].id == 0) {
        gUnitTextures[static_cast<size_t>(UnitType::NeutralRaphael)] =
            loadUiTexture("neutral_caster.png");
    }
    if (gUnitTextures[static_cast<size_t>(UnitType::NeutralKethericThorm)].id == 0) {
        gUnitTextures[static_cast<size_t>(UnitType::NeutralKethericThorm)] =
            loadUiTexture("library/quests/defeat_ketheric_thorm.png");
    }
    if (gUnitTextures[static_cast<size_t>(UnitType::NeutralKethericThorm)].id == 0) {
        gUnitTextures[static_cast<size_t>(UnitType::NeutralKethericThorm)] =
            loadUiTexture("neutral_guardian.png");
    }
    if (gUnitTextures[static_cast<size_t>(UnitType::NeutralGuardianOfFaith)].id == 0) {
        gUnitTextures[static_cast<size_t>(UnitType::NeutralGuardianOfFaith)] =
            loadUiTexture("ability_guardian_shield.png");
    }
    if (gUnitTextures[static_cast<size_t>(UnitType::NeutralMinotaur)].id == 0) {
        gUnitTextures[static_cast<size_t>(UnitType::NeutralMinotaur)] =
            loadUiTexture("neutral_guardian.png");
    }
    if (gUnitTextures[static_cast<size_t>(UnitType::NeutralDeathKnight)].id == 0) {
        gUnitTextures[static_cast<size_t>(UnitType::NeutralDeathKnight)] =
            loadUiTexture("unit_skeleton.png");
    }
    if (gUnitTextures[static_cast<size_t>(UnitType::NeutralAirMyrmidon)].id == 0) {
        gUnitTextures[static_cast<size_t>(UnitType::NeutralAirMyrmidon)] =
            loadUiTexture("library/monsters/creatures/air_elemental.png");
    }
    if (gUnitTextures[static_cast<size_t>(UnitType::NeutralTamiaHolzt)].id == 0) {
        gUnitTextures[static_cast<size_t>(UnitType::NeutralTamiaHolzt)] =
            loadUiTexture("neutral_caster.png");
    }
    loadUnitTexture(UnitType::ShieldGuardian, "unit_shield_guardian.png");
    loadUnitTexture(UnitType::Cleric, "unit_life_cleric.png");
    loadUnitTexture(UnitType::Evoker, "unit_arcane_evoker.png");
    loadUnitTexture(UnitType::RogueAssassin, "unit_shadow_rogue.png");
    loadUnitTexture(UnitType::Druid, "unit_circle_druid.png");
    loadUnitTexture(UnitType::Treant, "unit_treant.png");
    loadUnitTexture(UnitType::SporeServant, "unit_neutral_sovereign_spaw.png");
    loadUnitTexture(UnitType::DragonWyrmling, "unit_dragon_wyrmling.png");

    loadNeutralFamilyTexture(NeutralFamily::Swarm, "neutral_swarm.png");
    loadNeutralFamilyTexture(NeutralFamily::Guardian, "neutral_guardian.png");
    loadNeutralFamilyTexture(NeutralFamily::Caster, "neutral_caster.png");
    loadNeutralFamilyTexture(NeutralFamily::Assassin, "neutral_assassin.png");
    loadNeutralFamilyTexture(NeutralFamily::Artillery, "neutral_artillery.png");

    loadAbilityTexture(AbilityKind::GithyankiAstralRaid, "library/skills/actions/soulbreaker.png");
    loadAbilityTexture(AbilityKind::BarbarianHeavySwing, "ability_barbarian_heavy_swing.png");
    loadAbilityTexture(AbilityKind::NecromancerSummon, "ability_necromancer_summon.png");
    loadAbilityTexture(AbilityKind::MephitDeathBurst, "ability_mephit_death_burst.png");
    loadAbilityTexture(AbilityKind::PaladinCharge, "ability_paladin_charge.png");
    loadAbilityTexture(AbilityKind::DragonBreath, "ability_dragon_breath.png");
    loadAbilityTexture(AbilityKind::GuardianShield, "ability_guardian_shield.png");
    loadAbilityTexture(AbilityKind::ClericHeal, "ability_cleric_heal.png");
    loadAbilityTexture(AbilityKind::FrostNova, "ability_frost_nova.png");
    loadAbilityTexture(AbilityKind::RogueAmbush, "ability_rogue_ambush.png");
    loadAbilityTexture(AbilityKind::DruidSummon, "ability_druid_summon.png");
    loadAbilityTexture(AbilityKind::KarnissCruelSting, "library/skills/actions/venom_claws.png");
    loadAbilityTexture(AbilityKind::SpectatorWoundingRay, "library/skills/actions/generic_control.png");
    loadAbilityTexture(AbilityKind::MindBlast, "library/skills/illithid_powers/mind_blast.png");
    loadAbilityTexture(AbilityKind::Counterspell, "library/skills/spells/counterspell.png");
    loadAbilityTexture(AbilityKind::AnimatingSpores, "library/skills/actions/fungal_infestation.png");
    loadAbilityTexture(AbilityKind::OwlbearMultiattack, "library/skills/actions/owlbear_claws.png");
    loadAbilityTexture(AbilityKind::HiemalStrike, "library/skills/actions/hiemal_strike.png");
    loadAbilityTexture(AbilityKind::VenomousBite, "library/skills/actions/venomous_bite.png");
    loadAbilityTexture(AbilityKind::DiabolicChains, "library/skills/actions/diabolic_chains.png");
    loadAbilityTexture(AbilityKind::KethericSmite, "library/skills/actions/ketheric_smite.png");
    loadAbilityTexture(AbilityKind::SelunesIre, "library/skills/actions/selunes_ire.png");
    loadAbilityTexture(AbilityKind::EvokerMagicMissile, "sources/Magic_Missile_Icon.png");
    loadAbilityTexture(AbilityKind::DominatePerson, "library/skills/actions/dominate_person.png");
    loadAbilityTexture(AbilityKind::ExtractBrain, "library/skills/illithid_powers/extract_brain.png");
    loadAbilityTexture(AbilityKind::StrikeOfTheGuardian, "library/skills/actions/strike_of_the_guardian.png");
    loadAbilityTexture(AbilityKind::MinotaurCharge, "library/skills/actions/minotaur_charge.png");
    loadAbilityTexture(AbilityKind::Blight, "library/skills/actions/blight.png");
    loadAbilityTexture(AbilityKind::StaggeringSmite, "library/skills/actions/staggering_smite.png");
    loadAbilityTexture(AbilityKind::ElectrifiedFlail, "library/skills/actions/electrified_flail.png");
    loadAbilityTexture(AbilityKind::BossFireball, "library/skills/spells/fireball.png");
    loadRelicTextures();
}

void unloadUiTextures() {
    unloadTextureIfLoaded(gBannerTexture);
    gBannerLoaded = false;
    unloadTextureIfLoaded(gFactionBlueSealTexture);
    unloadTextureIfLoaded(gFactionRedSealTexture);
    unloadTextureIfLoaded(gMeleeHitTexture);
    for (Texture2D& texture : gUnitTextures) unloadTextureIfLoaded(texture);
    for (Texture2D& texture : gNeutralFamilyTextures) unloadTextureIfLoaded(texture);
    for (Texture2D& texture : gAbilityTextures) unloadTextureIfLoaded(texture);
    for (auto& entry : gRelicTextures) unloadTextureIfLoaded(entry.second);
    gRelicTextures.clear();
}

bool isShopUnit(const UnitSpec& spec) {
    return spec.cost > 0 && spec.type != UnitType::SkeletonByNecromancer && spec.type != UnitType::Treant &&
           spec.type != UnitType::SporeServant;
}

size_t shopFingerprint(const std::vector<UnitSpec>& shop) {
    size_t fingerprint = shop.size();
    for (const UnitSpec& spec : shop) {
        size_t value = static_cast<size_t>(spec.cost + 37);
        value ^= static_cast<size_t>(spec.threat + 101) << 8;
        value ^= static_cast<size_t>(spec.unitCount + 11) << 16;
        value ^= static_cast<size_t>(spec.type) << 24;
        fingerprint ^= value + 0x9e3779b9u + (fingerprint << 6) + (fingerprint >> 2);
    }
    return fingerprint;
}

const std::vector<const UnitSpec*>& orderedShopSpecs(const GameEngine& engine) {
    struct Cache {
        const std::vector<UnitSpec>* source = nullptr;
        size_t fingerprint = 0;
        std::vector<const UnitSpec*> specs;
    };
    static Cache cache;

    const std::vector<UnitSpec>& shop = engine.shop();
    size_t fingerprint = shopFingerprint(shop);
    if (cache.source != &shop || cache.fingerprint != fingerprint) {
        cache.source = &shop;
        cache.fingerprint = fingerprint;
        cache.specs.clear();
        cache.specs.reserve(shop.size());
        for (const UnitSpec& spec : shop) {
            if (isShopUnit(spec)) cache.specs.push_back(&spec);
        }
        std::stable_sort(cache.specs.begin(), cache.specs.end(), [](const UnitSpec* lhs, const UnitSpec* rhs) {
            if (lhs->cost != rhs->cost) return lhs->cost < rhs->cost;
            if (lhs->threat != rhs->threat) return lhs->threat > rhs->threat;
            return static_cast<int>(lhs->type) < static_cast<int>(rhs->type);
        });
    }
    return cache.specs;
}

Rectangle boardRect() {
    return {kBoardX, kBoardY, kCell * kBoardWidth, kCell * kBoardHeight};
}

Rectangle benchRect() {
    Rectangle board = boardRect();
    return {kBoardX, board.y + board.height + 12.0f, kCell * 10.0f + 42.0f, 126.0f};
}

Rectangle readyRect() {
    return {kSideX + 336.0f, 36.0f, kSideW - 336.0f, 56.0f};
}

Rectangle difficultyRect(int index) {
    return {kSideX + index * 98.0f, 36.0f, 92.0f, 56.0f};
}

Rectangle detailRect();
Rectangle explorationStatusRect();

Rectangle explorationRoundPanelRect(int index) {
    Rectangle panel = explorationStatusRect();
    constexpr float gap = 8.0f;
    constexpr float yOffset = 66.0f;
    float x = panel.x + 18.0f;
    float width = (panel.width - 36.0f - gap * 4.0f) / 5.0f;
    return {x + index * (width + gap), panel.y + yOffset, width, 34.0f};
}

int explorationRoundChoiceIndexAt(Vector2 point) {
    constexpr int choiceCount = 5;
    for (int i = 0; i < choiceCount; ++i) {
        Rectangle rect = explorationRoundPanelRect(i);
        Rectangle forgiving{rect.x - 6.0f, rect.y - 8.0f, rect.width + 12.0f, rect.height + 16.0f};
        if (CheckCollisionPointRec(point, forgiving)) return i;
    }

    Rectangle first = explorationRoundPanelRect(0);
    Rectangle last = explorationRoundPanelRect(choiceCount - 1);
    Rectangle row{first.x,
                  first.y - 18.0f,
                  last.x + last.width - first.x,
                  first.height + 36.0f};
    if (!CheckCollisionPointRec(point, row)) return -1;

    float bestDistance = 999999.0f;
    int best = -1;
    for (int i = 0; i < choiceCount; ++i) {
        Rectangle rect = explorationRoundPanelRect(i);
        float centerX = rect.x + rect.width * 0.5f;
        float distance = std::abs(point.x - centerX);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = i;
        }
    }
    return best;
}

Rectangle detailRect() {
    return {kSideX, kDetailY, kShopCardW, kDetailH};
}

Rectangle explorationStatusRect() {
    Rectangle panel = detailRect();
    constexpr float statusH = 104.0f;
    return {panel.x, panel.y + panel.height - statusH, panel.width, statusH};
}

Rectangle explorationUnitDetailRect() {
    Rectangle panel = detailRect();
    Rectangle status = explorationStatusRect();
    return {panel.x, panel.y, panel.width, status.y - panel.y - 10.0f};
}

Rectangle shopViewportRect() {
    return {kSideX, kShopY, kShopCardW, kShopViewportH};
}

Rectangle relicPanelRect() {
    Rectangle bench = benchRect();
    float y = bench.y + bench.height + 12.0f;
    float h = std::max(0.0f, static_cast<float>(kScreenHeight) - y - kBoardX);
    Rectangle board = boardRect();
    return {kBoardX, y, board.width, h};
}

Rectangle battleLogRect() {
    return {kSideX, kLogY, kSideW, kLogH};
}

Rectangle shopCardRect(int index, float scroll) {
    return {kSideX,
            kShopY + index * (kShopCardH + kShopGap) - scroll,
            kShopCardW,
            kShopCardH};
}

Rectangle benchSlotRect(int index, int slotCount = 10) {
    Rectangle bench = benchRect();
    slotCount = std::max(1, slotCount);
    constexpr float gap = 2.0f;
    float slotW = std::min(kCell, (bench.width - 28.0f - gap * static_cast<float>(slotCount - 1)) /
                                      static_cast<float>(slotCount));
    return {bench.x + 14.0f + index * (slotW + gap), bench.y + 34.0f, slotW, kCell};
}

Rectangle canvasDestRect() {
    float windowW = static_cast<float>(GetScreenWidth());
    float windowH = static_cast<float>(GetScreenHeight());
    float scale = std::min(windowW / static_cast<float>(kScreenWidth),
                           windowH / static_cast<float>(kScreenHeight));
    scale = std::max(0.01f, scale);
    float width = static_cast<float>(kScreenWidth) * scale;
    float height = static_cast<float>(kScreenHeight) * scale;
    return {(windowW - width) * 0.5f, (windowH - height) * 0.5f, width, height};
}

Vector2 screenToCanvasMouse(Vector2 screenMouse) {
    Rectangle dest = canvasDestRect();
    float scale = dest.width / static_cast<float>(kScreenWidth);
    if (scale <= 0.0f) return {};
    return {(screenMouse.x - dest.x) / scale, (screenMouse.y - dest.y) / scale};
}

std::vector<UnitType> shopTypes(const GameEngine& engine) {
    std::vector<UnitType> types;
    const std::vector<const UnitSpec*>& specs = orderedShopSpecs(engine);
    types.reserve(specs.size());
    for (const UnitSpec* spec : specs) {
        types.push_back(spec->type);
    }
    return types;
}

int shopUnitCount(const GameEngine& engine) {
    return static_cast<int>(orderedShopSpecs(engine).size());
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
    const std::vector<const UnitSpec*>& specs = orderedShopSpecs(engine);
    if (index < 0 || index >= static_cast<int>(specs.size())) return nullptr;
    return specs[static_cast<size_t>(index)];
}

const UnitView* findUnit(const GameSnapshot& snapshot, UnitId id) {
    for (const UnitView& unit : snapshot.units) {
        if (unit.id == id) return &unit;
    }
    return nullptr;
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

bool coordOnBoard(Coord coord) {
    return coord.x >= 0 && coord.x < kBoardWidth && coord.y >= 0 && coord.y < kBoardHeight;
}

TerrainKind terrainAt(const GameSnapshot& snapshot, Coord coord) {
    if (coord.x < 0 || coord.x >= snapshot.width || coord.y < 0 || coord.y >= snapshot.height) {
        return TerrainKind::Wall;
    }
    size_t index = static_cast<size_t>(coord.y * snapshot.width + coord.x);
    if (index >= snapshot.terrain.size()) return TerrainKind::Open;
    return snapshot.terrain[index];
}

Color terrainColor(TerrainKind terrain, Coord coord, const GameSnapshot& snapshot) {
    switch (terrain) {
        case TerrainKind::Wall:
            return Color{18, 17, 18, 255};
        case TerrainKind::SideRoad:
            return Color{50, 46, 42, 255};
        case TerrainKind::NeutralCamp:
            return Color{58, 52, 45, 255};
        case TerrainKind::Trap:
            return Color{65, 48, 43, 255};
        case TerrainKind::BossSite:
            return Color{70, 56, 46, 255};
        case TerrainKind::ClearedObjective:
            return Color{43, 40, 37, 255};
        case TerrainKind::ClearedBoss:
            return Color{48, 43, 40, 255};
        case TerrainKind::Open:
        default:
            return Color{40, 37, 34, 255};
    }
}

NeutralFamily campFamilyForCell(Coord coord) {
    if (coord.y <= 4) return NeutralFamily::Caster;
    return NeutralFamily::Guardian;
}

Vector2 cellCenter(Coord coord) {
    Rectangle rect = cellRect(coord);
    return {rect.x + rect.width / 2.0f, rect.y + rect.height / 2.0f};
}

Vector2 knockbackDrawOffset(UnitId unitId, const std::vector<CombatCue>& cues) {
    for (auto it = cues.rbegin(); it != cues.rend(); ++it) {
        const CombatCue& cue = *it;
        if (cue.type != EventType::UnitMoved || cue.targetId != unitId || !isKnockbackCueText(cue.text)) continue;
        if (!coordOnBoard(cue.from) || !coordOnBoard(cue.to)) continue;
        float raw = cue.ttl > 0.0f ? std::clamp(cue.age / cue.ttl, 0.0f, 1.0f) : 1.0f;
        float slide = 1.0f - std::pow(1.0f - raw, 3.0f);
        Vector2 from = cellCenter(cue.from);
        Vector2 to = cellCenter(cue.to);
        return {(from.x + (to.x - from.x) * slide) - to.x,
                (from.y + (to.y - from.y) * slide) - to.y};
    }
    return {0.0f, 0.0f};
}

Rectangle stackedUnitRect(Rectangle cell, int index, int count, UnitLayer layer = UnitLayer::Land) {
    if (count <= 1) {
        if (layer == UnitLayer::Air) {
            float size = cell.width * 0.62f;
            return {cell.x + (cell.width - size) * 0.5f,
                    cell.y + cell.height * 0.08f,
                    size,
                    size};
        }
        float pad = 2.0f;
        return {cell.x + pad, cell.y + pad, cell.width - pad * 2.0f, cell.height - pad * 2.0f};
    }

    float size = layer == UnitLayer::Air ? cell.width * 0.46f : cell.width * 0.74f;
    float centerX = cell.x + cell.width * 0.5f;
    float centerY = layer == UnitLayer::Air ? cell.y + cell.height * 0.28f : cell.y + cell.height * 0.60f;
    static constexpr std::array<Vector2, 6> offsets = {
        Vector2{0.0f, 0.0f},
        Vector2{-0.22f, -0.05f},
        Vector2{0.22f, -0.05f},
        Vector2{-0.13f, 0.18f},
        Vector2{0.13f, 0.18f},
        Vector2{0.0f, -0.22f}
    };
    Vector2 offset = offsets[static_cast<size_t>(std::min(index, static_cast<int>(offsets.size()) - 1))];
    if (index >= static_cast<int>(offsets.size())) {
        offset.x += ((index % 3) - 1) * 0.10f;
        offset.y += 0.08f * (index / 3);
    }
    return {centerX + offset.x * cell.width - size * 0.5f,
            centerY + offset.y * cell.height - size * 0.5f,
            size,
            size};
}

const UnitView* unitAtBoardMouse(const GameSnapshot& snapshot, Vector2 mouse, bool ownOnly) {
    Coord coord;
    if (!mouseToCell(mouse, coord)) return nullptr;

    std::vector<const UnitView*> stack;
    for (const UnitView& unit : snapshot.units) {
        if (!unit.alive || !unit.deployed || unit.coord != coord) continue;
        if (ownOnly && (unit.owner != PlayerId::One || isInternalUnit(unit.type))) continue;
        stack.push_back(&unit);
    }
    if (stack.empty()) return nullptr;

    std::stable_sort(stack.begin(), stack.end(), [](const UnitView* a, const UnitView* b) {
        if (a->layer != b->layer) return a->layer == UnitLayer::Land;
        return a->id < b->id;
    });
    std::vector<const UnitView*> landUnits;
    std::vector<const UnitView*> airUnits;
    for (const UnitView* unit : stack) {
        if (unit->layer == UnitLayer::Air) airUnits.push_back(unit);
        else landUnits.push_back(unit);
    }

    Rectangle cell = cellRect(coord);
    for (int i = static_cast<int>(airUnits.size()) - 1; i >= 0; --i) {
        Rectangle slot = stackedUnitRect(cell, i, static_cast<int>(airUnits.size()), UnitLayer::Air);
        if (CheckCollisionPointRec(mouse, slot)) return airUnits[static_cast<size_t>(i)];
    }
    for (int i = static_cast<int>(landUnits.size()) - 1; i >= 0; --i) {
        Rectangle slot = stackedUnitRect(cell, i, static_cast<int>(landUnits.size()), UnitLayer::Land);
        if (CheckCollisionPointRec(mouse, slot)) return landUnits[static_cast<size_t>(i)];
    }
    if (!airUnits.empty()) return airUnits.back();
    return landUnits.empty() ? nullptr : landUnits.back();
}

Color playerColor(PlayerId player) {
    return player == PlayerId::One ? Color{82, 143, 184, 255} : Color{183, 76, 78, 255};
}

Color unitFill(const UnitView& unit) {
    Color color = playerColor(unit.owner);
    if (isNeutralMonster(unit.type) || unit.neutralControlled) {
        color = Color{188, 169, 119, 255};
    }
    return color;
}

int colorLuminance(Color color) {
    return (static_cast<int>(color.r) * 299 + static_cast<int>(color.g) * 587 +
            static_cast<int>(color.b) * 114) /
           1000;
}

Color mixColor(Color a, Color b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    auto lerp = [t](unsigned char x, unsigned char y) {
        return static_cast<unsigned char>(std::round(static_cast<float>(x) +
                                                     (static_cast<float>(y) - static_cast<float>(x)) * t));
    };
    return Color{lerp(a.r, b.r), lerp(a.g, b.g), lerp(a.b, b.b), lerp(a.a, b.a)};
}

void drawTextMaterial(Font font,
                      const std::string& text,
                      float x,
                      float y,
                      float size,
                      Color color,
                      bool strong) {
    if (text.empty()) return;
    float pixelSize = size * kFontScale;
    bool lightText = colorLuminance(color) > 136;
    if (size >= 13.0f) {
        const unsigned char edgeAlpha = static_cast<unsigned char>(strong ? 112 : 62);
        const unsigned char shadeAlpha = static_cast<unsigned char>(strong ? 168 : 92);
        if (lightText) {
            if (strong || size >= 24.0f) {
                const std::array<Vector2, 4> edge = {
                    Vector2{-0.65f, 0.0f}, Vector2{0.65f, 0.0f},
                    Vector2{0.0f, -0.65f}, Vector2{0.0f, 0.65f}
                };
                for (Vector2 offset : edge) {
                    DrawTextEx(font,
                               text.c_str(),
                               {x + offset.x, y + offset.y},
                               pixelSize,
                               kFontSpacing,
                               Color{18, 11, 8, edgeAlpha});
                }
            }
            DrawTextEx(font, text.c_str(), {x + 1.15f, y + 1.45f}, pixelSize, kFontSpacing,
                       Color{0, 0, 0, shadeAlpha});
            DrawTextEx(font, text.c_str(), {x, y - 0.48f}, pixelSize, kFontSpacing,
                       Color{255, 242, 198, static_cast<unsigned char>(strong ? 72 : 36)});
        } else {
            if (strong || size >= 22.0f) {
                DrawTextEx(font, text.c_str(), {x + 0.75f, y + 1.0f}, pixelSize, kFontSpacing,
                           Color{255, 232, 177, static_cast<unsigned char>(strong ? 84 : 36)});
                const std::array<Vector2, 2> recess = {
                    Vector2{-0.45f, -0.45f}, Vector2{0.35f, -0.25f}
                };
                for (Vector2 offset : recess) {
                    DrawTextEx(font,
                               text.c_str(),
                               {x + offset.x, y + offset.y},
                               pixelSize,
                               kFontSpacing,
                               Color{20, 12, 8, static_cast<unsigned char>(strong ? 82 : 48)});
                }
            }
        }
    }
    DrawTextEx(font, text.c_str(), {x, y}, pixelSize, kFontSpacing, color);
}

void drawText(const std::string& text, float x, float y, float size, Color color) {
    drawTextMaterial(gFont, text, x, y, size, color, false);
}

Vector2 measureText(const std::string& text, float size) {
    return MeasureTextEx(gFont, text.c_str(), size * kFontScale, kFontSpacing);
}

Vector2 measureTextStrong(const std::string& text, float size) {
    Font font = gCustomBoldFont ? gBoldFont : gFont;
    return MeasureTextEx(font, text.c_str(), size * kFontScale, kFontSpacing);
}

template <typename MeasureFn>
std::string fitTextMeasured(const std::string& text, float maxWidth, float size, MeasureFn measure) {
    if (text.empty() || maxWidth <= 0.0f) return "";
    if (measure(text, size).x <= maxWidth) return text;
    constexpr const char* kEllipsis = "...";
    if (measure(kEllipsis, size).x > maxWidth) return "";

    size_t low = 0;
    size_t high = text.size();
    while (low < high) {
        size_t mid = low + (high - low + 1) / 2;
        std::string candidate = text.substr(0, mid) + kEllipsis;
        if (measure(candidate, size).x <= maxWidth) {
            low = mid;
        } else {
            high = mid - 1;
        }
    }
    return text.substr(0, low) + kEllipsis;
}

std::string fitText(const std::string& text, float maxWidth, float size) {
    return fitTextMeasured(text, maxWidth, size, measureText);
}

std::string fitTextStrong(const std::string& text, float maxWidth, float size) {
    return fitTextMeasured(text, maxWidth, size, measureTextStrong);
}

float drawWrappedText(const std::string& text, float x, float y, float maxWidth, float size, Color color) {
    std::istringstream words(text);
    std::string word;
    std::string line;
    const float lineHeight = size * kFontScale + 5.0f;
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

float drawWrappedTextLimited(const std::string& text,
                             float x,
                             float y,
                             float maxWidth,
                             float size,
                             Color color,
                             float bottomY,
                             int maxLines = 0) {
    if (text.empty() || maxWidth <= 0.0f || y >= bottomY) return y;

    const float lineHeight = size * kFontScale + 5.0f;
    int lines = 0;
    bool clipped = false;
    auto drawLine = [&](const std::string& line) {
        if (line.empty()) return true;
        if ((maxLines > 0 && lines >= maxLines) || y + lineHeight > bottomY) {
            clipped = true;
            return false;
        }
        std::string out = line;
        if (maxLines > 0 && lines == maxLines - 1) {
            out = fitText(line, maxWidth, size);
        }
        drawText(fitText(out, maxWidth, size), x, y, size, color);
        y += lineHeight;
        ++lines;
        return true;
    };

    std::istringstream words(text);
    std::string word;
    std::string line;
    while (words >> word) {
        std::string candidate = line.empty() ? word : line + " " + word;
        if (measureText(candidate, size).x <= maxWidth) {
            line = candidate;
            continue;
        }
        if (!drawLine(line)) break;
        line = word;
        if (measureText(line, size).x > maxWidth) {
            if (!drawLine(line)) break;
            line.clear();
        }
    }
    if (!clipped && !line.empty()) drawLine(line);
    return y;
}

std::vector<std::string> wrapTextLines(const std::string& text,
                                       float maxWidth,
                                       float size,
                                       int maxLines = 0) {
    std::vector<std::string> lines;
    if (text.empty() || maxWidth <= 0.0f) return lines;

    std::istringstream words(text);
    std::string word;
    std::string line;
    auto pushLine = [&](std::string value) {
        if (value.empty()) return false;
        if (maxLines > 0 && static_cast<int>(lines.size()) >= maxLines) return false;
        if (maxLines > 0 && static_cast<int>(lines.size()) == maxLines - 1) {
            value = fitText(value, maxWidth, size);
        }
        lines.push_back(std::move(value));
        return true;
    };

    while (words >> word) {
        std::string candidate = line.empty() ? word : line + " " + word;
        if (measureText(candidate, size).x <= maxWidth) {
            line = candidate;
            continue;
        }
        if (!line.empty() && !pushLine(line)) return lines;
        line = word;
        if (measureText(line, size).x > maxWidth) {
            if (!pushLine(fitText(line, maxWidth, size))) return lines;
            line.clear();
        }
    }
    if (!line.empty()) pushLine(line);
    return lines;
}

void drawTextCentered(const std::string& text, Rectangle rect, float size, Color color) {
    Vector2 measured = measureText(text, size);
    drawText(text, rect.x + (rect.width - measured.x) / 2.0f,
             rect.y + (rect.height - size * kFontScale) / 2.0f, size, color);
}

void drawTextShadowed(const std::string& text, float x, float y, float size, Color color) {
    Font font = (gCustomBoldFont && gBoldFont.texture.id != 0) ? gBoldFont : gFont;
    DrawTextEx(font, text.c_str(), {x + 2.2f, y + 3.0f}, size * kFontScale, kFontSpacing,
               Color{0, 0, 0, 185});
    drawTextMaterial(font, text, x, y, size, color, true);
}

void drawTextStrong(const std::string& text, float x, float y, float size, Color color) {
    Font font = (gCustomBoldFont && gBoldFont.texture.id != 0) ? gBoldFont : gFont;
    if (gCustomBoldFont && gBoldFont.texture.id != 0) {
        drawTextMaterial(font, text, x, y, size, color, true);
        return;
    }
    drawTextMaterial(font, text, x + 0.6f, y, size, color, true);
    drawTextMaterial(font, text, x, y, size, color, true);
}

void drawTextCenteredStrong(const std::string& text, Rectangle rect, float size, Color color) {
    Vector2 measured = measureTextStrong(text, size);
    drawTextStrong(text,
                   rect.x + (rect.width - measured.x) / 2.0f,
                   rect.y + (rect.height - size * kFontScale) / 2.0f,
                   size,
                   color);
}

void drawTextRight(const std::string& text, float right, float y, float size, Color color) {
    Vector2 measured = measureText(text, size);
    drawText(text, right - measured.x, y, size, color);
}

void drawTextureAspectFit(const Texture2D& texture, Rectangle rect, Color tint = WHITE) {
    if (texture.id == 0 || rect.width <= 0.0f || rect.height <= 0.0f) return;
    float scale = std::min(rect.width / static_cast<float>(texture.width),
                           rect.height / static_cast<float>(texture.height));
    float width = texture.width * scale;
    float height = texture.height * scale;
    Rectangle dest{rect.x + (rect.width - width) * 0.5f,
                   rect.y + (rect.height - height) * 0.5f,
                   width,
                   height};
    DrawTexturePro(texture,
                   {0.0f, 0.0f, static_cast<float>(texture.width), static_cast<float>(texture.height)},
                   dest,
                   {0.0f, 0.0f},
                   0.0f,
                   tint);
}

const Texture2D* unitTexture(UnitType type) {
    int index = static_cast<int>(type);
    if (index < 0 || index >= static_cast<int>(gUnitTextures.size())) return nullptr;
    const Texture2D& texture = gUnitTextures[static_cast<size_t>(index)];
    return texture.id != 0 ? &texture : nullptr;
}

const Texture2D* neutralFamilyTexture(NeutralFamily family) {
    int index = static_cast<int>(family);
    if (index < 0 || index >= static_cast<int>(gNeutralFamilyTextures.size())) return nullptr;
    const Texture2D& texture = gNeutralFamilyTextures[static_cast<size_t>(index)];
    return texture.id != 0 ? &texture : nullptr;
}

const Texture2D* factionSealTexture(PlayerId owner) {
    const Texture2D& texture = owner == PlayerId::One ? gFactionBlueSealTexture : gFactionRedSealTexture;
    return texture.id != 0 ? &texture : nullptr;
}

const Texture2D* abilityTexture(AbilityKind ability) {
    int index = static_cast<int>(ability);
    if (index < 0 || index >= static_cast<int>(gAbilityTextures.size())) return nullptr;
    const Texture2D& texture = gAbilityTextures[static_cast<size_t>(index)];
    return texture.id != 0 ? &texture : nullptr;
}

const Texture2D* relicTexture(const RelicSpec& relic) {
    auto it = gRelicTextures.find(relic.id);
    if (it == gRelicTextures.end() || it->second.id == 0) return nullptr;
    return &it->second;
}

std::optional<AbilityKind> abilityFromEvent(const Event& event) {
    const std::string& text = event.text;
    if (text.find("astral raid") != std::string::npos) return AbilityKind::GithyankiAstralRaid;
    if (text.find("Frenzied Strike") != std::string::npos || text.find("Rage") != std::string::npos ||
        text.find("raging") != std::string::npos || text.find("reckless swing") != std::string::npos ||
        text.find("reckless") != std::string::npos) {
        return AbilityKind::BarbarianHeavySwing;
    }
    if (text.find("summoned Treant") != std::string::npos || text.find("Treant") != std::string::npos) {
        return AbilityKind::DruidSummon;
    }
    if (text.find("summoned Skeleton") != std::string::npos || text.find("Skeleton") != std::string::npos) {
        return AbilityKind::NecromancerSummon;
    }
    if (text.find("fire breath") != std::string::npos || text.find("breathed fire") != std::string::npos ||
        text.find("breath weapon") != std::string::npos) {
        return AbilityKind::DragonBreath;
    }
    if (text.find("cinders") != std::string::npos || text.find("death burst") != std::string::npos) {
        return AbilityKind::MephitDeathBurst;
    }
    if (text.find("Fireball") != std::string::npos) return AbilityKind::BossFireball;
    if (text.find("cast frost nova") != std::string::npos || text.find("slow") != std::string::npos) {
        return AbilityKind::FrostNova;
    }
    if (text.find("Cruel Sting") != std::string::npos) return AbilityKind::KarnissCruelSting;
    if (text.find("Wounding Ray") != std::string::npos) return AbilityKind::SpectatorWoundingRay;
    if (text.find("Extract Brain") != std::string::npos) return AbilityKind::ExtractBrain;
    if (text.find("Mind Blast") != std::string::npos || text.find("Stunned") != std::string::npos) {
        return AbilityKind::MindBlast;
    }
    if (text.find("Hiemal Strike") != std::string::npos || text.find("Chilled") != std::string::npos) {
        return AbilityKind::HiemalStrike;
    }
    if (text.find("Venomous Bite") != std::string::npos || text.find("Poisoned") != std::string::npos) {
        return AbilityKind::VenomousBite;
    }
    if (text.find("Diabolic Chains") != std::string::npos) return AbilityKind::DiabolicChains;
    if (text.find("Strike of the Guardian") != std::string::npos ||
        text.find("anti-undead radiance") != std::string::npos) {
        return AbilityKind::StrikeOfTheGuardian;
    }
    if (text.find("used Charge") != std::string::npos || text.find("by Charge") != std::string::npos) {
        return AbilityKind::MinotaurCharge;
    }
    if (text.find("Staggering Smite") != std::string::npos ||
        text.find("Staggered") != std::string::npos) {
        return AbilityKind::StaggeringSmite;
    }
    if (text.find("Electrified Flail") != std::string::npos) return AbilityKind::ElectrifiedFlail;
    if (text.find("Selune's Ire") != std::string::npos || text.find("Sacred Flame") != std::string::npos) {
        return AbilityKind::SelunesIre;
    }
    if (text.find("Dominate Person") != std::string::npos ||
        text.find("dominated") != std::string::npos) {
        return AbilityKind::DominatePerson;
    }
    if (text.find("Blight") != std::string::npos) return AbilityKind::Blight;
    if (text.find("Wrathful Smite") != std::string::npos ||
        text.find("Blinding Smite") != std::string::npos ||
        text.find("Ketheric's Smite") != std::string::npos ||
        text.find("Frightened") != std::string::npos ||
        text.find("Blinded") != std::string::npos) {
        return AbilityKind::KethericSmite;
    }
    if (text.find("Counterspell") != std::string::npos) return AbilityKind::Counterspell;
    if (text.find("Animating Spores") != std::string::npos || text.find("Spore Servant") != std::string::npos) {
        return AbilityKind::AnimatingSpores;
    }
    if (text.find("Magic Missile") != std::string::npos) return AbilityKind::EvokerMagicMissile;
    if (text.find("Multiattack") != std::string::npos || text.find("Prone") != std::string::npos) {
        return AbilityKind::OwlbearMultiattack;
    }
    if (text.find("shield absorbed damage") != std::string::npos || text.find("gained shield") != std::string::npos) {
        return AbilityKind::GuardianShield;
    }
    if (text.find("healed") != std::string::npos) return AbilityKind::ClericHeal;
    if (text.find("leaped to the backline") != std::string::npos || text.find("ambush") != std::string::npos) {
        return AbilityKind::RogueAmbush;
    }
    if (text.find("charge") != std::string::npos) return AbilityKind::PaladinCharge;
    return std::nullopt;
}

void drawSceneBackground() {
    Rectangle screen{0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)};
    if (gBannerLoaded) {
        float scale = std::max(screen.width / static_cast<float>(gBannerTexture.width),
                               screen.height / static_cast<float>(gBannerTexture.height));
        float sourceW = screen.width / scale;
        float sourceH = screen.height / scale;
        Rectangle source{(static_cast<float>(gBannerTexture.width) - sourceW) * 0.5f,
                         (static_cast<float>(gBannerTexture.height) - sourceH) * 0.5f,
                         sourceW,
                         sourceH};
        DrawTexturePro(gBannerTexture,
                       source,
                       screen,
                       {0.0f, 0.0f},
                       0.0f,
                       WHITE);
        DrawRectangleGradientV(0, 0, kScreenWidth, kScreenHeight,
                               Color{0, 0, 0, 72}, Color{0, 0, 0, 182});
        DrawRectangleGradientH(0, 0, kScreenWidth, kScreenHeight,
                               Color{25, 16, 14, 118}, Color{4, 5, 7, 92});
        DrawRectangleRec(screen, Color{18, 12, 12, 86});
    } else {
        DrawRectangleGradientV(0, 0, kScreenWidth, kScreenHeight,
                               Color{26, 18, 15, 255}, Color{8, 7, 7, 255});
    }
}

Color fade(Color color, unsigned char alpha) {
    color.a = alpha;
    return color;
}

void drawCornerBolts(Rectangle rect, Color color) {
    const std::array<Vector2, 4> corners = {
        Vector2{rect.x + 10.0f, rect.y + 10.0f},
        Vector2{rect.x + rect.width - 10.0f, rect.y + 10.0f},
        Vector2{rect.x + 10.0f, rect.y + rect.height - 10.0f},
        Vector2{rect.x + rect.width - 10.0f, rect.y + rect.height - 10.0f}
    };
    for (Vector2 corner : corners) {
        DrawCircleV(corner, 3.0f, color);
        DrawCircleV(corner, 1.2f, Color{32, 24, 34, color.a});
    }
}

float grainRand(int seed, int index) {
    unsigned int value = static_cast<unsigned int>(seed) * 374761393u +
                         static_cast<unsigned int>(index) * 668265263u;
    value = (value ^ (value >> 13)) * 1274126177u;
    value ^= value >> 16;
    return static_cast<float>(value & 0xffffu) / 65535.0f;
}

void drawMaterialGrain(Rectangle rect, Color light, Color dark, int seed, int strokes) {
    if (rect.width <= 4.0f || rect.height <= 4.0f || strokes <= 0) return;
    for (int i = 0; i < strokes; ++i) {
        float rx = grainRand(seed + 17, i * 4 + 0);
        float ry = grainRand(seed + 17, i * 4 + 1);
        float rr = grainRand(seed + 17, i * 4 + 2);
        float rs = grainRand(seed + 17, i * 4 + 3);
        float x = rect.x + 8.0f + rx * std::max(1.0f, rect.width - 16.0f);
        float y = rect.y + 8.0f + ry * std::max(1.0f, rect.height - 16.0f);
        float len = 18.0f + rr * std::min(rect.width * 0.36f, 95.0f);
        float slope = (rs - 0.5f) * 5.0f;
        float x2 = std::clamp(x + len, rect.x + 4.0f, rect.x + rect.width - 4.0f);
        float y2 = std::clamp(y + slope, rect.y + 4.0f, rect.y + rect.height - 4.0f);
        Color line = (i % 3 == 0) ? light : dark;
        line.a = static_cast<unsigned char>(std::max(14, std::min(70, static_cast<int>(line.a))));
        DrawLineEx({x, y}, {x2, y2}, 1.0f, line);
    }
}

void drawParchmentPanel(Rectangle rect, Color fill = kParchment) {
    DrawRectangleRounded({rect.x + 4.0f, rect.y + 6.0f, rect.width, rect.height}, 0.035f, 8, Color{0, 0, 0, 84});
    DrawRectangleRounded(rect, 0.035f, 8, fill);
    DrawRectangleGradientV(static_cast<int>(rect.x + 2.0f),
                           static_cast<int>(rect.y + 2.0f),
                           static_cast<int>(std::max(1.0f, rect.width - 4.0f)),
                           static_cast<int>(std::max(1.0f, rect.height - 4.0f)),
                           Color{255, 243, 207, 26},
                           Color{88, 55, 31, 30});
    DrawRectangleRoundedLines(rect, 0.035f, 8, 1.75f, mixColor(kParchmentDark, kWine, 0.18f));
    DrawRectangleLinesEx({rect.x + 7.0f, rect.y + 7.0f, rect.width - 14.0f, rect.height - 14.0f},
                         1.0f, Color{116, 86, 54, 88});
    DrawRectangleLinesEx({rect.x + 11.0f, rect.y + 11.0f, rect.width - 22.0f, rect.height - 22.0f},
                         1.0f, Color{246, 224, 178, 34});
    DrawLineEx({rect.x + 14.0f, rect.y + 16.0f}, {rect.x + rect.width - 14.0f, rect.y + 11.0f},
               1.0f, Color{245, 232, 197, 28});
    DrawLineEx({rect.x + 18.0f, rect.y + rect.height - 14.0f},
               {rect.x + rect.width - 18.0f, rect.y + rect.height - 20.0f},
               1.0f, Color{70, 52, 37, 28});
    drawCornerBolts(rect, Color{151, 111, 58, 128});
}

void drawPanelFrame(Rectangle rect, Color fill = kPanel) {
    DrawRectangleRounded({rect.x + 5.0f, rect.y + 7.0f, rect.width, rect.height}, 0.035f, 8,
                         Color{0, 0, 0, 118});
    DrawRectangleRounded(rect, 0.035f, 8, fill);
    DrawRectangleGradientV(static_cast<int>(rect.x + 2.0f),
                           static_cast<int>(rect.y + 2.0f),
                           static_cast<int>(std::max(1.0f, rect.width - 4.0f)),
                           static_cast<int>(std::max(1.0f, rect.height - 4.0f)),
                           Color{74, 47, 34, 28},
                           Color{0, 0, 0, 46});
    drawMaterialGrain({rect.x + 3.0f, rect.y + 3.0f, rect.width - 6.0f, rect.height - 6.0f},
                      Color{119, 75, 44, 28},
                      Color{0, 0, 0, 36},
                      static_cast<int>(rect.x * 7.0f + rect.y * 11.0f),
                      static_cast<int>(std::clamp(rect.height / 18.0f, 7.0f, 26.0f)));
    DrawRectangleRoundedLines(rect, 0.035f, 8, 1.85f, mixColor(kBorder, kGold, 0.20f));
    DrawRectangleLinesEx({rect.x + 5.0f, rect.y + 5.0f, rect.width - 10.0f, rect.height - 10.0f},
                         1.0f, Color{198, 156, 82, 84});
    DrawRectangleLinesEx({rect.x + 10.0f, rect.y + 10.0f, rect.width - 20.0f, rect.height - 20.0f},
                         1.0f, Color{42, 24, 16, 125});
    DrawLineEx({rect.x + 18.0f, rect.y + 1.0f}, {rect.x + rect.width - 18.0f, rect.y + 1.0f},
               1.0f, Color{228, 199, 135, 68});
    DrawLineEx({rect.x + 18.0f, rect.y + rect.height - 2.0f},
               {rect.x + rect.width - 18.0f, rect.y + rect.height - 2.0f},
               1.0f, Color{0, 0, 0, 130});
    drawCornerBolts(rect, Color{181, 139, 72, 145});
}

Color iconAccent(UnitType type) {
    switch (type) {
        case UnitType::ShieldGuardian: return Color{105, 143, 160, 255};
        case UnitType::Cleric: return Color{184, 178, 160, 255};
        case UnitType::Evoker: return Color{128, 119, 177, 255};
        case UnitType::RogueAssassin: return Color{151, 94, 116, 255};
        case UnitType::Druid: return Color{109, 145, 102, 255};
        case UnitType::Treant: return Color{83, 127, 82, 255};
        case UnitType::Necromancer: return Color{115, 97, 139, 255};
        case UnitType::FireMephit: return Color{180, 93, 65, 255};
        case UnitType::ImpSwarm: return Color{142, 143, 161, 255};
        case UnitType::GoblinSkirmisher: return Color{158, 132, 80, 255};
        case UnitType::Paladin: return Color{185, 171, 132, 255};
        case UnitType::DragonWyrmling: return Color{177, 112, 66, 255};
        case UnitType::NeutralSpectator: return Color{133, 118, 164, 255};
        case UnitType::NeutralOwlbear: return Color{132, 105, 75, 255};
        case UnitType::NeutralMindFlayer: return Color{127, 112, 170, 255};
        case UnitType::NeutralSovereignSpaw: return Color{113, 141, 96, 255};
        case UnitType::SporeServant: return Color{96, 132, 88, 255};
        case UnitType::NeutralKarniss: return Color{162, 112, 76, 255};
        case UnitType::NeutralRedcap: return Color{157, 65, 70, 255};
        case UnitType::NeutralWaterMyrmidon: return Color{84, 149, 166, 255};
        case UnitType::NeutralPhaseSpiderMatriarch: return Color{159, 134, 90, 255};
        case UnitType::NeutralRaphael: return Color{174, 76, 52, 255};
        case UnitType::NeutralKethericThorm: return Color{178, 157, 75, 255};
        case UnitType::NeutralMoonlightSliver: return Color{190, 178, 115, 255};
        case UnitType::NeutralGuardianOfFaith: return Color{207, 185, 98, 255};
        case UnitType::NeutralMinotaur: return Color{161, 101, 72, 255};
        case UnitType::NeutralDeathKnight: return Color{161, 103, 161, 255};
        case UnitType::NeutralAirMyrmidon: return Color{102, 154, 210, 255};
        case UnitType::NeutralTamiaHolzt: return Color{138, 104, 158, 255};
        case UnitType::Barbarian: return Color{163, 83, 65, 255};
        case UnitType::GithyankiWarrior: return Color{98, 151, 157, 255};
        case UnitType::Ranger: return Color{103, 145, 98, 255};
        case UnitType::Skeleton:
        case UnitType::SkeletonByNecromancer:
        default:
            return Color{178, 178, 171, 255};
    }
}

std::string abilitySchoolLabel(AbilityKind ability) {
    switch (ability) {
        case AbilityKind::GithyankiAstralRaid: return "Psionic";
        case AbilityKind::BarbarianHeavySwing: return "Martial";
        case AbilityKind::NecromancerSummon: return "Necromancy";
        case AbilityKind::MephitDeathBurst: return "Elemental";
        case AbilityKind::PaladinCharge: return "Divine";
        case AbilityKind::DragonBreath: return "Draconic";
        case AbilityKind::GuardianShield: return "Divine";
        case AbilityKind::ClericHeal: return "Divine";
        case AbilityKind::FrostNova: return "Arcane";
        case AbilityKind::RogueAmbush: return "Shadow";
        case AbilityKind::DruidSummon: return "Primal";
        case AbilityKind::KarnissCruelSting: return "Martial";
        case AbilityKind::SpectatorWoundingRay: return "Necrotic";
        case AbilityKind::MindBlast: return "Psionic";
        case AbilityKind::Counterspell: return "Arcane";
        case AbilityKind::AnimatingSpores: return "Fungal";
        case AbilityKind::OwlbearMultiattack: return "Martial";
        case AbilityKind::HiemalStrike: return "Elemental";
        case AbilityKind::VenomousBite: return "Poison";
        case AbilityKind::DiabolicChains: return "Infernal";
        case AbilityKind::KethericSmite: return "Divine";
        case AbilityKind::SelunesIre: return "Divine";
        case AbilityKind::EvokerMagicMissile: return "Arcane";
        case AbilityKind::DominatePerson: return "Enchantment";
        case AbilityKind::ExtractBrain: return "Illithid";
        case AbilityKind::StrikeOfTheGuardian: return "Radiant";
        case AbilityKind::MinotaurCharge: return "Martial";
        case AbilityKind::Blight: return "Necromancy";
        case AbilityKind::StaggeringSmite: return "Necromancy";
        case AbilityKind::ElectrifiedFlail: return "Elemental";
        case AbilityKind::BossFireball: return "Evocation";
        case AbilityKind::None:
        default:
            return "Martial";
    }
}

Color abilitySchoolColor(AbilityKind ability) {
    switch (ability) {
        case AbilityKind::GithyankiAstralRaid: return Color{186, 134, 72, 255};
        case AbilityKind::BarbarianHeavySwing: return Color{158, 93, 63, 255};
        case AbilityKind::NecromancerSummon: return Color{127, 100, 151, 255};
        case AbilityKind::MephitDeathBurst: return Color{181, 110, 70, 255};
        case AbilityKind::PaladinCharge: return Color{199, 166, 91, 255};
        case AbilityKind::DragonBreath: return Color{177, 84, 58, 255};
        case AbilityKind::GuardianShield: return Color{116, 154, 172, 255};
        case AbilityKind::ClericHeal: return Color{200, 183, 126, 255};
        case AbilityKind::FrostNova: return Color{95, 151, 174, 255};
        case AbilityKind::RogueAmbush: return Color{143, 101, 131, 255};
        case AbilityKind::DruidSummon: return Color{111, 148, 93, 255};
        case AbilityKind::KarnissCruelSting: return Color{174, 112, 72, 255};
        case AbilityKind::SpectatorWoundingRay: return Color{105, 154, 131, 255};
        case AbilityKind::MindBlast: return Color{132, 103, 171, 255};
        case AbilityKind::Counterspell: return Color{122, 141, 185, 255};
        case AbilityKind::AnimatingSpores: return Color{103, 147, 92, 255};
        case AbilityKind::OwlbearMultiattack: return Color{163, 120, 80, 255};
        case AbilityKind::HiemalStrike: return Color{82, 157, 176, 255};
        case AbilityKind::VenomousBite: return Color{99, 155, 75, 255};
        case AbilityKind::DiabolicChains: return Color{186, 80, 49, 255};
        case AbilityKind::KethericSmite: return Color{195, 169, 76, 255};
        case AbilityKind::SelunesIre: return Color{203, 183, 93, 255};
        case AbilityKind::EvokerMagicMissile: return Color{126, 119, 181, 255};
        case AbilityKind::DominatePerson: return Color{148, 99, 171, 255};
        case AbilityKind::ExtractBrain: return Color{178, 96, 78, 255};
        case AbilityKind::StrikeOfTheGuardian: return Color{207, 184, 96, 255};
        case AbilityKind::MinotaurCharge: return Color{174, 104, 70, 255};
        case AbilityKind::Blight: return Color{98, 143, 88, 255};
        case AbilityKind::StaggeringSmite: return Color{189, 99, 178, 255};
        case AbilityKind::ElectrifiedFlail: return Color{85, 150, 231, 255};
        case AbilityKind::BossFireball: return Color{214, 82, 49, 255};
        case AbilityKind::None:
        default:
            return Color{154, 137, 105, 255};
    }
}

void drawTinySpark(Vector2 c, float size, Color color) {
    DrawLineEx({c.x - size, c.y}, {c.x + size, c.y}, 1.5f, color);
    DrawLineEx({c.x, c.y - size}, {c.x, c.y + size}, 1.5f, color);
    DrawLineEx({c.x - size * 0.7f, c.y - size * 0.7f}, {c.x + size * 0.7f, c.y + size * 0.7f}, 1.0f, color);
    DrawLineEx({c.x - size * 0.7f, c.y + size * 0.7f}, {c.x + size * 0.7f, c.y - size * 0.7f}, 1.0f, color);
}

void drawRoundedRuneRing(Rectangle rect, Color accent) {
    DrawRectangleRounded(rect, 0.24f, 6, Color{35, 23, 23, 255});
    DrawRectangleRoundedLines(rect, 0.24f, 6, 1.5f, accent);
    DrawRectangleRounded({rect.x + 2.0f, rect.y + 2.0f, rect.width - 4.0f, rect.height - 4.0f},
                         0.22f, 6, Color{220, 203, 163, 255});
    DrawRectangleRoundedLines({rect.x + 2.0f, rect.y + 2.0f, rect.width - 4.0f, rect.height - 4.0f},
                              0.22f, 6, 1.0f, Color{102, 69, 38, 180});
}

void drawAbilitySigil(Rectangle rect, AbilityKind ability, bool withLabel = false) {
    Color accent = abilitySchoolColor(ability);
    Rectangle shadow{rect.x + 2.0f, rect.y + 3.0f, rect.width, rect.height};
    DrawRectangleRounded(shadow, 0.26f, 6, Color{0, 0, 0, 90});
    drawRoundedRuneRing(rect, accent);

    Rectangle core{rect.x + rect.width * 0.10f, rect.y + rect.height * 0.10f,
                   rect.width * 0.80f, rect.height * 0.80f};
    DrawRectangleRounded(core, 0.20f, 6, Color{28, 20, 17, 255});
    DrawRectangleRoundedLines(core, 0.20f, 6, 1.0f, Color{115, 82, 45, 190});
    if (const Texture2D* texture = abilityTexture(ability)) {
        Rectangle icon{rect.x + rect.width * 0.06f, rect.y + rect.height * 0.06f,
                       rect.width * 0.88f, rect.height * 0.88f};
        drawTextureAspectFit(*texture, icon, WHITE);
    } else {
        DrawRectangleRoundedLines(core, 0.20f, 6, 2.0f, Color{180, 132, 76, 150});
        drawTextCentered(ability == AbilityKind::None ? "ATK" : "ICON",
                         core,
                         std::max(12.0f, rect.height * 0.18f),
                         Color{210, 184, 130, 210});
        if (ability != AbilityKind::None) {
            TraceLog(LOG_WARNING, "Missing BG3 ability icon for ability id %d", static_cast<int>(ability));
        }
    }

    DrawLineEx({rect.x + rect.width * 0.18f, rect.y + rect.height * 0.12f},
               {rect.x + rect.width * 0.82f, rect.y + rect.height * 0.12f},
               1.0f, Color{255, 244, 210, 90});
    DrawLineEx({rect.x + rect.width * 0.16f, rect.y + rect.height * 0.86f},
               {rect.x + rect.width * 0.84f, rect.y + rect.height * 0.86f},
               1.0f, Color{89, 52, 31, 90});

    if (withLabel) {
        Rectangle label{rect.x - 22.0f, rect.y + rect.height + 2.0f, rect.width + 44.0f, 24.0f};
        DrawRectangleRounded(label, 0.18f, 6, Color{48, 31, 23, 232});
        DrawRectangleRoundedLines(label, 0.18f, 6, 1.0f, accent);
        drawTextCentered(abilitySchoolLabel(ability), label, 15.0f, kInk);
    }
}

std::string stripMechanicPrefix(const std::string& text) {
    constexpr const char* prefix = "Mechanic: ";
    constexpr size_t prefixLen = 10;
    if (text.rfind(prefix, 0) == 0) return text.substr(prefixLen);
    return text;
}

void drawCross(Vector2 center, float size, Color color, float thickness = 4.0f) {
    DrawLineEx({center.x - size, center.y}, {center.x + size, center.y}, thickness, color);
    DrawLineEx({center.x, center.y - size}, {center.x, center.y + size}, thickness, color);
}

Color factionPrimary(PlayerId owner) {
    return owner == PlayerId::One ? Color{48, 166, 245, 255} : Color{236, 65, 82, 255};
}

Color factionTrim(PlayerId owner) {
    return owner == PlayerId::One ? Color{218, 236, 255, 255} : Color{255, 207, 145, 255};
}

void drawFactionBands(Rectangle rect, PlayerId owner, bool ghost) {
    Color primary = factionPrimary(owner);
    Color trim = factionTrim(owner);
    if (ghost) {
        primary.a = 130;
        trim.a = 130;
    }
    DrawRectangleRounded({rect.x + 1.0f, rect.y + 1.0f, rect.width - 2.0f, rect.height - 2.0f},
                         0.14f, 6, Color{primary.r, primary.g, primary.b, static_cast<unsigned char>(ghost ? 35 : 58)});
    if (const Texture2D* seal = factionSealTexture(owner)) {
        Rectangle sealRect{rect.x + rect.width * 0.18f, rect.y + rect.height * 0.14f,
                           rect.width * 0.64f, rect.height * 0.64f};
        unsigned char alpha = static_cast<unsigned char>(ghost ? 52 : 78);
        drawTextureAspectFit(*seal, sealRect, Color{255, 255, 255, alpha});
    }
    DrawRectangleRec({rect.x + 3.0f, rect.y + 3.0f, rect.width - 6.0f, 7.0f}, primary);
    DrawRectangleRec({rect.x + 4.0f, rect.y + rect.height - 11.0f, rect.width - 8.0f, 5.0f}, trim);
    if (owner == PlayerId::One) {
        DrawRectangleRec({rect.x + 2.0f, rect.y + 4.0f, 6.0f, rect.height - 8.0f}, primary);
        DrawTriangle({rect.x + 8.0f, rect.y + 10.0f}, {rect.x + 26.0f, rect.y + 10.0f},
                     {rect.x + 8.0f, rect.y + 28.0f}, trim);
    } else {
        DrawRectangleRec({rect.x + rect.width - 8.0f, rect.y + 4.0f, 6.0f, rect.height - 8.0f}, primary);
        DrawTriangle({rect.x + rect.width - 8.0f, rect.y + 10.0f},
                     {rect.x + rect.width - 26.0f, rect.y + 10.0f},
                     {rect.x + rect.width - 8.0f, rect.y + 28.0f}, trim);
    }
}

void drawFactionOutline(Rectangle rect, PlayerId owner, bool ghost) {
    Color primary = factionPrimary(owner);
    Color trim = factionTrim(owner);
    if (ghost) {
        primary.a = 160;
        trim.a = 150;
    }
    DrawRectangleRoundedLines({rect.x + 1.0f, rect.y + 1.0f, rect.width - 2.0f, rect.height - 2.0f},
                              0.14f, 6, 3.0f, primary);
    DrawRectangleRoundedLines({rect.x + 5.0f, rect.y + 5.0f, rect.width - 10.0f, rect.height - 10.0f},
                              0.12f, 6, 1.0f, trim);
}

void drawFactionNameplate(Rectangle rect, PlayerId owner, bool ghost) {
    Color primary = factionPrimary(owner);
    if (ghost) {
        primary.a = 150;
    }
    float notch = std::max(10.0f, rect.width * 0.17f);
    if (owner == PlayerId::One) {
        DrawTriangle({rect.x + 8.0f, rect.y + 8.0f},
                     {rect.x + 8.0f + notch, rect.y + 8.0f},
                     {rect.x + 8.0f, rect.y + 8.0f + notch},
                     primary);
    } else {
        DrawTriangle({rect.x + rect.width - 8.0f, rect.y + 8.0f},
                     {rect.x + rect.width - 8.0f - notch, rect.y + 8.0f},
                     {rect.x + rect.width - 8.0f, rect.y + 8.0f + notch},
                     primary);
    }
}

void drawIconFrame(Rectangle rect, Color accent) {
    Rectangle shadow{rect.x + 2.0f, rect.y + 3.0f, rect.width, rect.height};
    DrawRectangleRounded(shadow, 0.16f, 8, Color{0, 0, 0, 90});
    DrawRectangleRounded(rect, 0.16f, 8, Color{20, 15, 14, 255});
    DrawRectangleRoundedLines(rect, 0.16f, 8, 2.0f, Color{219, 174, 86, 255});

    Rectangle inner{rect.x + 2.0f, rect.y + 2.0f, rect.width - 4.0f, rect.height - 4.0f};
    DrawRectangleRounded(inner, 0.14f, 8, Color{34, 24, 20, 255});
    DrawRectangleRoundedLines(inner, 0.14f, 8, 1.0f, Color{104, 76, 44, 255});
    DrawRectangleRec({rect.x + 4.0f, rect.y + 4.0f, rect.width - 8.0f, 1.0f}, Color{255, 242, 196, 60});
    DrawRectangleRec({rect.x + 4.0f, rect.y + rect.height - 5.0f, rect.width - 8.0f, 1.0f}, Color{0, 0, 0, 95});
    DrawCircleV({rect.x + rect.width * 0.20f, rect.y + rect.height * 0.20f}, 1.6f, Color{255, 244, 207, 55});
    DrawCircleV({rect.x + rect.width * 0.80f, rect.y + rect.height * 0.80f}, 1.6f, Color{0, 0, 0, 55});
    DrawLineEx({rect.x + rect.width * 0.14f, rect.y + 1.0f}, {rect.x + rect.width * 0.86f, rect.y + 1.0f},
               1.0f, Color{accent.r, accent.g, accent.b, 48});
}

void drawUnitGlyph(UnitType type, Rectangle rect, Color base) {
    if (rect.width < 10.0f || rect.height < 10.0f) {
        Vector2 c{rect.x + rect.width / 2.0f, rect.y + rect.height / 2.0f};
        DrawCircleV(c, std::max(2.0f, std::min(rect.width, rect.height) * 0.28f), base);
        return;
    }
    drawIconFrame(rect, base);
    if (const Texture2D* texture = unitTexture(type)) {
        float inset = std::max(1.0f, std::min(rect.width, rect.height) * 0.025f);
        Rectangle imageRect{rect.x + inset, rect.y + inset, rect.width - inset * 2.0f, rect.height - inset * 2.0f};
        DrawRectangleRounded(imageRect, 0.10f, 6, Color{8, 7, 8, 210});
        drawTextureAspectFit(*texture, imageRect);
        DrawRectangleRoundedLines(imageRect,
                                  0.10f,
                                  6,
                                  1.0f,
                                  Color{255, 224, 142, 90});
        return;
    }

    Vector2 c{rect.x + rect.width / 2.0f, rect.y + rect.height / 2.0f};
    float w = rect.width;
    float h = rect.height;
    Color dark = Color{21, 23, 28, 255};
    Color light = Color{231, 234, 238, 255};
    Color accent = base;

    switch (type) {
        case UnitType::ShieldGuardian:
        case UnitType::Paladin: {
            DrawRectangleRounded({rect.x + w * 0.32f, rect.y + h * 0.20f, w * 0.36f, h * 0.46f}, 0.20f, 4,
                                 Color{133, 152, 175, 255});
            DrawRectangleRoundedLines({rect.x + w * 0.32f, rect.y + h * 0.20f, w * 0.36f, h * 0.46f},
                                      0.20f, 4, 2.0f, dark);
            drawCross({c.x, c.y - h * 0.01f}, w * 0.08f, light, 3.0f);
            DrawLineEx({c.x, rect.y + h * 0.20f}, {c.x, rect.y + h * 0.62f}, 3.0f, dark);
            break;
        }
        case UnitType::Cleric: {
            DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), w * 0.22f, Color{190, 193, 198, 255});
            drawCross(c, w * 0.11f, accent, 5.0f);
            DrawLineEx({c.x - w * 0.18f, c.y}, {c.x - w * 0.12f, c.y}, 2.0f, Color{213, 199, 144, 255});
            DrawLineEx({c.x + w * 0.12f, c.y}, {c.x + w * 0.18f, c.y}, 2.0f, Color{213, 199, 144, 255});
            DrawLineEx({c.x, c.y - h * 0.18f}, {c.x, c.y - h * 0.12f}, 2.0f, Color{213, 199, 144, 255});
            DrawLineEx({c.x, c.y + h * 0.12f}, {c.x, c.y + h * 0.18f}, 2.0f, Color{213, 199, 144, 255});
            break;
        }
        case UnitType::Evoker: {
            DrawCircleV(c, w * 0.16f, Color{108, 90, 176, 255});
            DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), w * 0.16f, dark);
            DrawLineEx({c.x - w * 0.18f, c.y}, {c.x - w * 0.04f, c.y}, 2.0f, light);
            DrawLineEx({c.x + w * 0.04f, c.y}, {c.x + w * 0.18f, c.y}, 2.0f, light);
            DrawLineEx({c.x, c.y - h * 0.18f}, {c.x, c.y - h * 0.04f}, 2.0f, light);
            DrawLineEx({c.x, c.y + h * 0.04f}, {c.x, c.y + h * 0.18f}, 2.0f, light);
            break;
        }
        case UnitType::RogueAssassin: {
            DrawLineEx({c.x - w * 0.06f, c.y + h * 0.18f}, {c.x + w * 0.18f, c.y - h * 0.18f}, 4.0f,
                       Color{84, 58, 84, 255});
            DrawTriangle({c.x + w * 0.08f, c.y - h * 0.16f}, {c.x + w * 0.18f, c.y - h * 0.10f},
                         {c.x + w * 0.02f, c.y - h * 0.02f}, light);
            break;
        }
        case UnitType::Druid: {
            DrawTriangle({c.x, rect.y + h * 0.18f}, {rect.x + w * 0.40f, rect.y + h * 0.58f},
                         {rect.x + w * 0.60f, rect.y + h * 0.58f}, Color{103, 150, 99, 255});
            DrawLineEx({c.x, rect.y + h * 0.18f}, {c.x, rect.y + h * 0.72f}, 3.0f, dark);
            DrawLineEx({c.x - w * 0.12f, c.y + h * 0.04f}, {c.x - w * 0.24f, c.y + h * 0.16f}, 2.0f, light);
            DrawLineEx({c.x + w * 0.12f, c.y + h * 0.04f}, {c.x + w * 0.24f, c.y + h * 0.16f}, 2.0f, light);
            break;
        }
        case UnitType::Treant: {
            DrawCircleV({c.x, rect.y + h * 0.36f}, w * 0.18f, Color{101, 150, 98, 255});
            DrawLineEx({c.x, rect.y + h * 0.44f}, {c.x, rect.y + h * 0.80f}, 4.0f, dark);
            DrawLineEx({c.x - w * 0.14f, rect.y + h * 0.48f}, {c.x, rect.y + h * 0.30f}, 3.0f, light);
            DrawLineEx({c.x + w * 0.14f, rect.y + h * 0.48f}, {c.x, rect.y + h * 0.30f}, 3.0f, light);
            break;
        }
        case UnitType::Necromancer: {
            DrawTriangle({c.x, rect.y + h * 0.18f}, {rect.x + w * 0.28f, rect.y + h * 0.60f},
                         {rect.x + w * 0.72f, rect.y + h * 0.60f}, Color{89, 70, 118, 255});
            DrawCircleV({c.x, c.y - h * 0.02f}, w * 0.10f, light);
            DrawCircleV({c.x - w * 0.03f, c.y - h * 0.03f}, w * 0.015f, dark);
            DrawCircleV({c.x + w * 0.03f, c.y - h * 0.03f}, w * 0.015f, dark);
            DrawLineEx({c.x - w * 0.12f, c.y + h * 0.18f}, {c.x + w * 0.12f, c.y + h * 0.18f}, 3.0f, dark);
            break;
        }
        case UnitType::FireMephit: {
            DrawTriangle({c.x, rect.y + h * 0.16f}, {rect.x + w * 0.34f, rect.y + h * 0.60f},
                         {rect.x + w * 0.66f, rect.y + h * 0.60f}, Color{220, 120, 72, 255});
            DrawTriangle({c.x, rect.y + h * 0.26f}, {rect.x + w * 0.44f, rect.y + h * 0.62f},
                         {rect.x + w * 0.56f, rect.y + h * 0.62f}, light);
            break;
        }
        case UnitType::ImpSwarm: {
            for (int i = 0; i < 3; ++i) {
                float x = rect.x + w * (0.28f + i * 0.18f);
                DrawCircleV({x, c.y}, w * 0.06f, Color{199, 201, 209, 255});
                DrawTriangle({x, c.y - h * 0.10f}, {x - w * 0.02f, c.y - h * 0.02f},
                             {x + w * 0.02f, c.y - h * 0.02f}, accent);
            }
            break;
        }
        case UnitType::GoblinSkirmisher: {
            DrawLineEx({c.x - w * 0.04f, c.y + h * 0.18f}, {c.x + w * 0.18f, c.y - h * 0.18f}, 4.0f,
                       Color{135, 110, 60, 255});
            DrawTriangle({c.x + w * 0.18f, c.y - h * 0.18f}, {c.x + w * 0.28f, c.y - h * 0.12f},
                         {c.x + w * 0.12f, c.y - h * 0.04f}, light);
            break;
        }
        case UnitType::Ranger: {
            DrawLineEx({c.x - w * 0.10f, c.y - h * 0.16f}, {c.x - w * 0.10f, c.y + h * 0.16f}, 2.5f, light);
            DrawLineEx({c.x - w * 0.10f, c.y - h * 0.16f}, {c.x + w * 0.10f, c.y}, 2.5f, accent);
            DrawLineEx({c.x + w * 0.10f, c.y}, {c.x - w * 0.10f, c.y + h * 0.16f}, 2.5f, light);
            DrawTriangle({c.x + w * 0.12f, c.y}, {c.x + w * 0.22f, c.y - h * 0.04f},
                         {c.x + w * 0.18f, c.y + h * 0.02f}, light);
            break;
        }
        case UnitType::Barbarian: {
            DrawLineEx({c.x - w * 0.04f, c.y + h * 0.20f}, {c.x + w * 0.04f, c.y - h * 0.20f}, 4.0f, dark);
            DrawTriangle({c.x + w * 0.04f, c.y - h * 0.16f}, {c.x + w * 0.22f, c.y - h * 0.06f},
                         {c.x + w * 0.06f, c.y + h * 0.00f}, light);
            break;
        }
        case UnitType::DragonWyrmling: {
            DrawTriangle({c.x, rect.y + h * 0.18f}, {rect.x + w * 0.24f, rect.y + h * 0.56f},
                         {rect.x + w * 0.76f, rect.y + h * 0.56f}, Color{199, 109, 63, 255});
            DrawTriangle({c.x, rect.y + h * 0.26f}, {rect.x + w * 0.34f, rect.y + h * 0.48f},
                         {rect.x + w * 0.66f, rect.y + h * 0.48f}, dark);
            DrawLineEx({c.x - w * 0.12f, c.y - h * 0.02f}, {c.x - w * 0.22f, c.y - h * 0.08f}, 2.0f, light);
            DrawLineEx({c.x + w * 0.12f, c.y - h * 0.02f}, {c.x + w * 0.22f, c.y - h * 0.08f}, 2.0f, light);
            break;
        }
        case UnitType::NeutralSpectator: {
            DrawCircleV(c, w * 0.20f, Color{158, 132, 199, 255});
            DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), w * 0.20f, dark);
            DrawCircleV(c, w * 0.08f, light);
            DrawCircleV({c.x + w * 0.02f, c.y - h * 0.02f}, w * 0.03f, dark);
            DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), w * 0.28f, accent);
            DrawLineEx({c.x - w * 0.24f, c.y}, {c.x - w * 0.30f, c.y}, 2.0f, accent);
            DrawLineEx({c.x + w * 0.24f, c.y}, {c.x + w * 0.30f, c.y}, 2.0f, accent);
            DrawLineEx({c.x, c.y - h * 0.24f}, {c.x, c.y - h * 0.30f}, 2.0f, accent);
            DrawLineEx({c.x, c.y + h * 0.24f}, {c.x, c.y + h * 0.30f}, 2.0f, accent);
            break;
        }
        case UnitType::NeutralOwlbear: {
            DrawCircleV({c.x, c.y - h * 0.05f}, w * 0.18f, Color{156, 122, 82, 255});
            DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y - h * 0.05f), w * 0.18f, dark);
            DrawTriangle({c.x - w * 0.08f, c.y - h * 0.02f}, {c.x + w * 0.08f, c.y - h * 0.02f},
                         {c.x, c.y + h * 0.10f}, light);
            DrawLineEx({c.x - w * 0.16f, c.y + h * 0.10f}, {c.x - w * 0.28f, c.y + h * 0.22f}, 3.0f, accent);
            DrawLineEx({c.x + w * 0.16f, c.y + h * 0.10f}, {c.x + w * 0.28f, c.y + h * 0.22f}, 3.0f, accent);
            DrawLineEx({c.x - w * 0.06f, c.y + h * 0.18f}, {c.x - w * 0.12f, c.y + h * 0.30f}, 3.0f, dark);
            DrawLineEx({c.x + w * 0.06f, c.y + h * 0.18f}, {c.x + w * 0.12f, c.y + h * 0.30f}, 3.0f, dark);
            break;
        }
        case UnitType::NeutralMindFlayer: {
            DrawCircleV({c.x, c.y - h * 0.07f}, w * 0.16f, Color{142, 116, 190, 255});
            DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y - h * 0.07f), w * 0.16f, dark);
            for (float dx : {-0.14f, -0.05f, 0.05f, 0.14f}) {
                DrawLineEx({c.x + w * dx, c.y + h * 0.02f},
                           {c.x + w * (dx * 1.4f), c.y + h * 0.28f},
                           2.0f, accent);
            }
            break;
        }
        case UnitType::NeutralSovereignSpaw:
        case UnitType::SporeServant: {
            DrawCircleV(c, w * 0.18f, Color{116, 163, 103, 255});
            DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), w * 0.18f, dark);
            DrawLineEx({c.x, c.y - h * 0.24f}, {c.x, c.y + h * 0.20f}, 3.0f, accent);
            DrawCircleV({c.x - w * 0.14f, c.y - h * 0.20f}, w * 0.08f, light);
            DrawCircleV({c.x + w * 0.14f, c.y - h * 0.16f}, w * 0.07f, light);
            break;
        }
        case UnitType::NeutralKarniss: {
            DrawCircleV({c.x, c.y - h * 0.13f}, w * 0.10f, Color{198, 151, 108, 255});
            for (float dx : {-0.20f, -0.10f, 0.10f, 0.20f}) {
                DrawLineEx({c.x, c.y}, {c.x + w * dx, c.y + h * 0.25f}, 3.0f, accent);
            }
            DrawLineEx({c.x, c.y - h * 0.02f}, {c.x, c.y + h * 0.18f}, 4.0f, dark);
            break;
        }
        case UnitType::NeutralRedcap: {
            DrawTriangle({c.x, c.y - h * 0.22f}, {c.x - w * 0.18f, c.y + h * 0.10f},
                         {c.x + w * 0.18f, c.y + h * 0.10f}, Color{204, 74, 78, 255});
            DrawCircleV({c.x, c.y + h * 0.06f}, w * 0.12f, accent);
            DrawLineEx({c.x - w * 0.12f, c.y + h * 0.20f}, {c.x + w * 0.12f, c.y + h * 0.20f},
                       3.0f, dark);
            break;
        }
        case UnitType::NeutralWaterMyrmidon: {
            DrawLineEx({c.x - w * 0.20f, c.y + h * 0.20f}, {c.x + w * 0.12f, c.y - h * 0.22f},
                       4.0f, Color{201, 231, 238, 255});
            for (float dx : {-0.08f, 0.0f, 0.08f}) {
                DrawLineEx({c.x + w * 0.12f, c.y - h * 0.22f},
                           {c.x + w * (0.12f + dx), c.y - h * 0.34f},
                           2.0f, accent);
            }
            DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), w * 0.24f, accent);
            break;
        }
        case UnitType::NeutralPhaseSpiderMatriarch: {
            DrawCircleV(c, w * 0.15f, Color{184, 143, 94, 255});
            DrawCircleV({c.x, c.y - h * 0.15f}, w * 0.10f, light);
            for (float side : {-1.0f, 1.0f}) {
                for (float yy : {-0.12f, 0.0f, 0.12f}) {
                    DrawLineEx({c.x + side * w * 0.08f, c.y + h * yy},
                               {c.x + side * w * 0.30f, c.y + h * (yy + 0.08f)},
                               2.0f, accent);
                }
            }
            break;
        }
        case UnitType::NeutralRaphael: {
            DrawCircleV({c.x, c.y - h * 0.04f}, w * 0.15f, Color{174, 55, 45, 255});
            DrawTriangle({c.x - w * 0.10f, c.y - h * 0.18f}, {c.x - w * 0.24f, c.y - h * 0.30f},
                         {c.x - w * 0.12f, c.y - h * 0.06f}, accent);
            DrawTriangle({c.x + w * 0.10f, c.y - h * 0.18f}, {c.x + w * 0.24f, c.y - h * 0.30f},
                         {c.x + w * 0.12f, c.y - h * 0.06f}, accent);
            for (float dy : {-0.10f, 0.06f, 0.20f}) {
                DrawLineEx({c.x - w * 0.26f, c.y + h * dy},
                           {c.x + w * 0.24f, c.y + h * (dy + 0.05f)}, 2.5f, light);
            }
            break;
        }
        case UnitType::NeutralKethericThorm: {
            DrawRectangleRounded({c.x - w * 0.15f, c.y - h * 0.18f, w * 0.30f, h * 0.38f},
                                 0.08f, 4, Color{93, 87, 80, 255});
            DrawRectangleRoundedLines({c.x - w * 0.15f, c.y - h * 0.18f, w * 0.30f, h * 0.38f},
                                      0.08f, 4, 2.0f, accent);
            DrawLineEx({c.x + w * 0.12f, c.y - h * 0.22f}, {c.x - w * 0.12f, c.y + h * 0.20f},
                       4.0f, light);
            drawTinySpark({c.x - w * 0.14f, c.y - h * 0.12f}, std::max(2.0f, w * 0.07f), accent);
            break;
        }
        case UnitType::NeutralGuardianOfFaith: {
            DrawRectangleRounded({c.x - w * 0.14f, c.y - h * 0.22f, w * 0.28f, h * 0.44f},
                                 0.08f, 4, Color{206, 190, 130, 255});
            DrawRectangleRoundedLines({c.x - w * 0.14f, c.y - h * 0.22f, w * 0.28f, h * 0.44f},
                                      0.08f, 4, 2.0f, dark);
            DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), w * 0.28f, accent);
            drawTinySpark(c, std::max(3.0f, w * 0.10f), light);
            break;
        }
        case UnitType::NeutralMinotaur: {
            DrawCircleV({c.x, c.y - h * 0.06f}, w * 0.16f, Color{153, 94, 69, 255});
            DrawLineEx({c.x - w * 0.10f, c.y - h * 0.15f}, {c.x - w * 0.30f, c.y - h * 0.26f},
                       3.0f, light);
            DrawLineEx({c.x + w * 0.10f, c.y - h * 0.15f}, {c.x + w * 0.30f, c.y - h * 0.26f},
                       3.0f, light);
            DrawLineEx({c.x - w * 0.04f, c.y + h * 0.08f}, {c.x + w * 0.18f, c.y + h * 0.26f},
                       4.0f, accent);
            break;
        }
        case UnitType::NeutralDeathKnight: {
            DrawCircleV({c.x, c.y - h * 0.08f}, w * 0.13f, Color{206, 190, 210, 255});
            DrawRectangleRounded({c.x - w * 0.12f, c.y + h * 0.02f, w * 0.24f, h * 0.28f},
                                 0.06f, 4, Color{58, 47, 65, 255});
            DrawLineEx({c.x - w * 0.23f, c.y + h * 0.20f}, {c.x + w * 0.18f, c.y - h * 0.22f},
                       4.0f, accent);
            drawTinySpark({c.x + w * 0.20f, c.y - h * 0.22f}, std::max(2.0f, w * 0.07f), light);
            break;
        }
        case UnitType::NeutralAirMyrmidon: {
            DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), w * 0.24f, accent);
            DrawLineEx({c.x - w * 0.20f, c.y - h * 0.20f}, {c.x + w * 0.04f, c.y - h * 0.02f},
                       3.0f, light);
            DrawLineEx({c.x + w * 0.04f, c.y - h * 0.02f}, {c.x - w * 0.06f, c.y + h * 0.02f},
                       3.0f, accent);
            DrawLineEx({c.x - w * 0.06f, c.y + h * 0.02f}, {c.x + w * 0.22f, c.y + h * 0.22f},
                       3.0f, light);
            break;
        }
        case UnitType::GithyankiWarrior: {
            DrawLineEx({c.x - w * 0.08f, c.y + h * 0.18f}, {c.x + w * 0.10f, c.y - h * 0.18f}, 4.0f,
                       Color{216, 220, 226, 255});
            DrawTriangle({c.x + w * 0.08f, c.y - h * 0.16f}, {c.x + w * 0.20f, c.y - h * 0.08f},
                         {c.x + w * 0.02f, c.y - h * 0.02f}, light);
            DrawLineEx({c.x - w * 0.04f, c.y + h * 0.10f}, {c.x + w * 0.16f, c.y - h * 0.08f}, 2.0f, accent);
            break;
        }
        case UnitType::Skeleton:
        case UnitType::SkeletonByNecromancer:
        default: {
            DrawCircleV({c.x, c.y - h * 0.04f}, w * 0.13f, light);
            DrawCircleV({c.x - w * 0.04f, c.y - h * 0.05f}, w * 0.02f, dark);
            DrawCircleV({c.x + w * 0.04f, c.y - h * 0.05f}, w * 0.02f, dark);
            DrawLineEx({c.x - w * 0.10f, c.y + h * 0.12f}, {c.x + w * 0.10f, c.y + h * 0.12f}, 3.0f, dark);
            DrawLineEx({c.x - w * 0.16f, c.y + h * 0.22f}, {c.x + w * 0.16f, c.y + h * 0.22f}, 3.0f, accent);
            break;
        }
    }
}

std::string targetText(const UnitSpec& spec) {
    if (spec.canAttackLand && spec.canAttackAir) return "Land/Air";
    if (spec.canAttackLand) return "Land";
    if (spec.canAttackAir) return "Air";
    return "None";
}

std::string secondsLabel(double seconds) {
    if (seconds <= 0.0) return "Passive";
    return TextFormat("%.1fs", seconds);
}

std::string perHitDamageSummary(const UnitSpec& spec) {
    return damageFormula(basicDamagePacketFor(spec));
}

std::string wikiDamageLine(const UnitSpec& spec) {
    DamagePacket packet = basicDamagePacketFor(spec);
    if (spec.unitCount <= 1) return damageFormula(packet);

    int bodies = std::max(1, spec.unitCount);
    return TextFormat("%s x%d", damageFormula(packet).c_str(), bodies);
}

std::string wikiDamageFormulaLine(const UnitSpec& spec) {
    DamagePacket packet = basicDamagePacketFor(spec);
    if (spec.unitCount <= 1) return damageFormula(packet);
    return TextFormat("%s x%d", damageFormula(packet).c_str(), std::max(1, spec.unitCount));
}

std::string trimCopy(std::string text) {
    auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
    while (!text.empty() && isSpace(static_cast<unsigned char>(text.front()))) text.erase(text.begin());
    while (!text.empty() && isSpace(static_cast<unsigned char>(text.back()))) text.pop_back();
    return text;
}

struct DamageTextParts {
    std::string rangeLine;
    std::string formulaLine;
};

DamageTextParts splitDamageText(const std::string& line) {
    constexpr const char* marker = "    ";
    size_t split = line.find(marker);
    if (line.rfind("Damage:", 0) != 0 || split == std::string::npos) {
        return {line, ""};
    }
    return {trimCopy(line.substr(0, split)), trimCopy(line.substr(split))};
}

std::string unitProfileLine(const UnitSpec& spec,
                            bool neutral,
                            bool neutralActivated,
                            bool neutralReturningHome = false) {
    if (neutral) {
        if (spec.type == UnitType::NeutralRedcap) return "Ambusher | Active";
        std::string state = neutralReturningHome ? "Returning" : (neutralActivated ? "Activated" : "Dormant");
        if (spec.range >= 4 && spec.canAttackAir) return "Neutral ranged threat | " + state;
        if (spec.roleMask != 0 && spec.canAttackAir) return "Neutral controller | " + state;
        if (spec.range <= 1) return "Neutral melee guardian | " + state;
        return "Neutral guardian | " + state;
    }
    switch (spec.ability) {
        case AbilityKind::BarbarianHeavySwing:
            return "Frontline bruiser; grows more dangerous while wounded.";
        case AbilityKind::PaladinCharge:
            return "Armored striker; turns movement into a decisive first blow.";
        case AbilityKind::EvokerMagicMissile:
            return "Fragile artillery; reliable force damage into clusters.";
        case AbilityKind::NecromancerSummon:
        case AbilityKind::DruidSummon:
            return "Summoner; trades time and space for extra bodies.";
        case AbilityKind::ClericHeal:
            return "Backline support; keeps damaged allies alive.";
        case AbilityKind::GuardianShield:
            return "Defensive anchor; taunts and rebuilds shields.";
        case AbilityKind::BossFireball:
            return "Boss controller; punishes flying attackers with Fireball.";
        default:
            break;
    }
    if (spec.range >= 3) return "Ranged damage dealer; wants a protected firing lane.";
    if (spec.unitCount > 1) return "Squad unit; loses damage as bodies fall.";
    return "Melee combatant; positioning decides its value.";
}

std::string abilityDamageLine(const UnitSpec& spec, AbilityKind ability) {
    switch (ability) {
        case AbilityKind::None:
            return wikiDamageLine(spec);
        case AbilityKind::BarbarianHeavySwing:
            return damageFormula(abilityDamagePacketFor(spec, ability));
        case AbilityKind::NecromancerSummon:
            return "Summon: 2-body Raised Skeleton unit";
        case AbilityKind::GuardianShield:
            return TextFormat("Shield: +%d, capped at 120", spec.abilityValue);
        case AbilityKind::ClericHeal:
            return TextFormat("Healing: %d HP", spec.abilityValue);
        case AbilityKind::DruidSummon:
            return "Summon: 1 temporary Treant";
        case AbilityKind::Counterspell:
            return "Reaction: cancels one spell";
        case AbilityKind::AnimatingSpores:
            return "Raise: 1 corpse into a Spore Servant";
        case AbilityKind::DominatePerson:
            return "Control: no damage";
        case AbilityKind::ExtractBrain:
            return damageFormula(abilityDamagePacketFor(spec, AbilityKind::ExtractBrain));
        case AbilityKind::GithyankiAstralRaid:
        case AbilityKind::MephitDeathBurst:
        case AbilityKind::BossFireball:
        case AbilityKind::PaladinCharge:
        case AbilityKind::DragonBreath:
        case AbilityKind::FrostNova:
        case AbilityKind::RogueAmbush:
        case AbilityKind::KarnissCruelSting:
        case AbilityKind::SpectatorWoundingRay:
        case AbilityKind::MindBlast:
        case AbilityKind::OwlbearMultiattack:
        case AbilityKind::HiemalStrike:
        case AbilityKind::VenomousBite:
        case AbilityKind::DiabolicChains:
        case AbilityKind::KethericSmite:
        case AbilityKind::SelunesIre:
        case AbilityKind::EvokerMagicMissile:
        case AbilityKind::StrikeOfTheGuardian:
        case AbilityKind::MinotaurCharge:
        case AbilityKind::Blight:
        case AbilityKind::StaggeringSmite:
        case AbilityKind::ElectrifiedFlail:
            return damageFormula(abilityDamagePacketFor(spec, ability));
    }
    return damageFormula(abilityDamagePacketFor(spec, ability));
}

struct AbilityDetail {
    std::string title;
    std::string headline;
    std::string formula;
    std::string body;
    std::string save;
    std::string recharge;
    std::string range;
};

UnitSpec abilityDisplaySpec(UnitSpec spec, AbilityKind ability) {
    spec.ability = ability;
    return spec;
}

std::vector<AbilityKind> displayedAbilitiesFor(const UnitSpec& spec) {
    if (spec.type == UnitType::NeutralTamiaHolzt) {
        return {AbilityKind::DominatePerson, AbilityKind::Blight};
    }
    if (spec.type == UnitType::NeutralMindFlayer) {
        return {AbilityKind::MindBlast, AbilityKind::Counterspell, AbilityKind::ExtractBrain};
    }
    return {spec.ability};
}

AbilityDetail abilityDetailFor(UnitSpec spec, AbilityKind ability) {
    spec.ability = ability;
    AbilityDetail detail;
    detail.title = "Basic Attack";
    detail.headline = abilityDamageLine(spec, AbilityKind::None);
    detail.formula = wikiDamageFormulaLine(spec);
    detail.body = TextFormat("Hit %+d vs AC. %d living model%s can strike.",
                             spec.attackBonus,
                             std::max(1, spec.unitCount),
                             spec.unitCount == 1 ? "" : "s");
    detail.save = TextFormat("Attack Roll %+d", spec.attackBonus);
    detail.recharge = TextFormat("Attack %.1fs", spec.attackCooldown);
    detail.range = TextFormat("Range %d; Targets %s", spec.range, targetText(spec).c_str());

    switch (spec.ability) {
        case AbilityKind::None:
            return detail;
        case AbilityKind::GithyankiAstralRaid:
            detail.title = "Astral Raid";
            detail.headline = abilityDamageLine(spec, AbilityKind::GithyankiAstralRaid);
            detail.formula = TextFormat("Blink %d; opening hit has advantage",
                                        std::max(1, spec.abilityRange));
            detail.body = "Opens by blinking onto a high-value target.";
            detail.save = TextFormat("Attack Roll %+d", spec.attackBonus);
            detail.recharge = "Once per combat";
            detail.range = TextFormat("Blink %d; melee follow-up", std::max(1, spec.abilityRange));
            return detail;
        case AbilityKind::BarbarianHeavySwing:
            detail.title = "Berserker Rage";
            detail.headline = abilityDamageLine(spec, AbilityKind::BarbarianHeavySwing);
            detail.formula = "Advantage while raging; resists incoming damage";
            detail.body = "Wounded rage: lower HP means faster, harder swings. Every third hit adds Frenzied Strike.";
            detail.save = "No Save";
            detail.recharge = "Always on";
            detail.range = TextFormat("Melee; attack %.1fs base", spec.attackCooldown);
            return detail;
        case AbilityKind::NecromancerSummon:
            detail.title = "Raise Dead";
            detail.headline = abilityDamageLine(spec, AbilityKind::NecromancerSummon);
            detail.formula = TextFormat("Recharge %.1fs; cap 2 plus relics; lasts 8s",
                                        spec.abilityCooldown);
            detail.body = "Raises a temporary two-body skeleton unit nearby.";
            detail.save = "No Save";
            detail.recharge = TextFormat("Recharge %.1fs", spec.abilityCooldown);
            detail.range = TextFormat("Summon near caster; range %d attack", spec.range);
            return detail;
        case AbilityKind::MephitDeathBurst:
            detail.title = "Cinder Burst";
            detail.headline = abilityDamageLine(spec, AbilityKind::MephitDeathBurst);
            detail.formula = "Splash attack; death burst hits adjacent enemies";
            detail.body = "Fire splashes near the target. On death, adjacent land enemies are caught in the burst.";
            detail.save = TextFormat("DEX Save DC %d halves", spec.spellSaveDc);
            detail.recharge = TextFormat("Attack %.1fs; death trigger", spec.attackCooldown);
            detail.range = TextFormat("Range %d; splash radius 1", spec.range);
            return detail;
        case AbilityKind::BossFireball:
            detail.title = "Fireball";
            detail.headline = abilityDamageLine(spec, AbilityKind::BossFireball);
            detail.formula = "Anti-air boss spell; burst radius 1";
            detail.body = "Activated bosses punish flying units from long range, but the blast can also catch nearby ground units.";
            detail.save = TextFormat("DEX Save DC %d halves", spec.spellSaveDc);
            detail.recharge = "Boss reaction spell";
            detail.range = "Range 5; burst radius 1";
            return detail;
        case AbilityKind::PaladinCharge:
            detail.title = "Divine Charge";
            detail.headline = abilityDamageLine(spec, AbilityKind::PaladinCharge);
            detail.formula = "After moving, next hit has advantage and double damage";
            detail.body = "A short charge turns positioning into a heavy opening smite.";
            detail.save = TextFormat("Attack Roll %+d", spec.attackBonus);
            detail.recharge = "Refreshes after movement";
            detail.range = "Melee";
            return detail;
        case AbilityKind::DragonBreath:
            detail.title = "Fire Breath";
            detail.headline = abilityDamageLine(spec, AbilityKind::DragonBreath);
            detail.formula = "Area breath attack";
            detail.body = "Breathes fire over every valid enemy in range.";
            detail.save = TextFormat("DEX Save DC %d halves", spec.spellSaveDc);
            detail.recharge = TextFormat("Attack %.1fs", spec.attackCooldown);
            detail.range = TextFormat("Range %d; targets %s", spec.range, targetText(spec).c_str());
            return detail;
        case AbilityKind::GuardianShield:
            detail.title = "Arcane Ward";
            detail.headline = abilityDamageLine(spec, AbilityKind::GuardianShield);
            detail.formula = TextFormat("Taunt aura; shield refreshes every %.1fs",
                                        spec.abilityCooldown);
            detail.body = "Draws attacks away from allies and rebuilds a protective ward.";
            detail.save = "No Save";
            detail.recharge = TextFormat("Recharge %.1fs", spec.abilityCooldown);
            detail.range = "Self; taunt priority";
            return detail;
        case AbilityKind::ClericHeal:
            detail.title = "Healing Word";
            detail.headline = abilityDamageLine(spec, AbilityKind::ClericHeal);
            detail.formula = TextFormat("Recharge %.1fs; heals range %d",
                                        spec.abilityCooldown,
                                        spec.abilityRange);
            detail.body = "Restores the most wounded nearby ally.";
            detail.save = "No Save";
            detail.recharge = TextFormat("Recharge %.1fs", spec.abilityCooldown);
            detail.range = TextFormat("Heal range %d; attack range %d", spec.abilityRange, spec.range);
            return detail;
        case AbilityKind::FrostNova:
            detail.title = "Frost Nova";
            detail.headline = abilityDamageLine(spec, AbilityKind::FrostNova);
            detail.formula = "Burst radius 1; failed save Slowed";
            detail.body = "Cold burst; failed saves are Slowed.";
            detail.save = TextFormat("CON Save DC %d halves", spec.spellSaveDc);
            detail.recharge = TextFormat("Slow: %.1fs", spec.abilityDuration);
            detail.range = TextFormat("Range %d; burst radius 1", spec.range);
            return detail;
        case AbilityKind::RogueAmbush:
            detail.title = "Backline Ambush";
            detail.headline = abilityDamageLine(spec, AbilityKind::RogueAmbush);
            detail.formula = "Combat start: leap beside a priority target";
            detail.body = "Advantage opener with sneak attack damage.";
            detail.save = TextFormat("Attack Roll %+d", spec.attackBonus);
            detail.recharge = "Once per combat";
            detail.range = "Teleport adjacent; melee";
            return detail;
        case AbilityKind::DruidSummon:
            detail.title = "Awaken Treant";
            detail.headline = abilityDamageLine(spec, AbilityKind::DruidSummon);
            detail.formula = TextFormat("Recharge %.1fs; cap 1 plus relics; lasts 8s",
                                        spec.abilityCooldown);
            detail.body = "Summons a temporary Treant to block land movement.";
            detail.save = "No Save";
            detail.recharge = TextFormat("Recharge %.1fs", spec.abilityCooldown);
            detail.range = TextFormat("Summon near caster; range %d attack", spec.range);
            return detail;
        case AbilityKind::KarnissCruelSting:
            detail.title = "Multiattack - Cruel Sting";
            detail.headline = abilityDamageLine(spec, AbilityKind::KarnissCruelSting);
            detail.formula = TextFormat("Wounded target: %d sting sequences",
                                        std::max(1, spec.unitCount) * 3);
            detail.body = "Full-health targets take one hit. Wounded targets suffer repeated stings.";
            detail.save = TextFormat("Attack Roll %+d", spec.attackBonus);
            detail.recharge = TextFormat("Attack %.1fs", spec.attackCooldown);
            detail.range = "Melee";
            return detail;
        case AbilityKind::SpectatorWoundingRay:
            detail.title = "Wounding Ray";
            detail.headline = abilityDamageLine(spec, AbilityKind::SpectatorWoundingRay);
            detail.formula = "Focused necrotic ray";
            detail.body = "A focused necrotic ray against one creature.";
            detail.save = TextFormat("CON Save DC %d halves", spec.spellSaveDc);
            detail.recharge = TextFormat("Attack %.1fs", spec.attackCooldown);
            detail.range = TextFormat("Range %d; targets %s", spec.range, targetText(spec).c_str());
            return detail;
        case AbilityKind::MindBlast:
            detail.title = "Mind Blast";
            detail.headline = abilityDamageLine(spec, AbilityKind::MindBlast);
            detail.formula = "Psychic burst; fail: Stunned";
            detail.body = "Psychic cone around the target. Stunned victims can be finished by Extract Brain.";
            detail.save = TextFormat("INT Save DC %d halves", spec.spellSaveDc);
            detail.recharge = TextFormat("Stun: %.1fs", std::max(0.8, spec.abilityDuration));
            detail.range = TextFormat("Range %d; burst radius 2", spec.range);
            return detail;
        case AbilityKind::Counterspell:
            detail.title = "Counterspell";
            detail.headline = abilityDamageLine(spec, AbilityKind::Counterspell);
            detail.formula = "Passive reaction to the first hostile spell";
            detail.body = "Once per game, cancels spell damage and its status effect.";
            detail.save = "No Save";
            detail.recharge = "Once per game";
            detail.range = "Reaction";
            return detail;
        case AbilityKind::ExtractBrain:
            detail.title = "Extract Brain";
            detail.headline = abilityDamageLine(spec, AbilityKind::ExtractBrain);
            detail.formula = "Adjacent Stunned target: instant kill";
            detail.body = "Once per lifetime, executes a stunned creature and heals 6d6.";
            detail.save = "No Save if Stunned";
            detail.recharge = "Once per lifetime";
            detail.range = "Melee; Stunned target only";
            return detail;
        case AbilityKind::AnimatingSpores:
            detail.title = "Animating Spores";
            detail.headline = abilityDamageLine(spec, AbilityKind::AnimatingSpores);
            detail.formula = TextFormat("Corpse in range %d; cap 2; lasts %.0fs",
                                        spec.abilityRange,
                                        spec.abilityDuration > 0.0 ? spec.abilityDuration : 12.0);
            detail.body = "Raises one nearby corpse as a temporary Spore Servant. Neutral Spaw ignores player relics.";
            detail.save = "No Save";
            detail.recharge = TextFormat("Recharge %.1fs", spec.abilityCooldown);
            detail.range = TextFormat("Corpse search range %d", spec.abilityRange);
            return detail;
        case AbilityKind::OwlbearMultiattack:
            detail.title = "Multiattack";
            detail.headline = abilityDamageLine(spec, AbilityKind::OwlbearMultiattack);
            detail.formula = "Multiattack; surviving target is shoved and Prone";
            detail.body = "Claw, bite, then a body-check. Land units behind it are shoved in a domino line.";
            detail.save = TextFormat("Attack Roll %+d", spec.attackBonus);
            detail.recharge = TextFormat("Attack %.1fs", spec.attackCooldown);
            detail.range = "Melee";
            return detail;
        case AbilityKind::HiemalStrike:
            detail.title = "Hiemal Strike";
            detail.headline = abilityDamageLine(spec, AbilityKind::HiemalStrike);
            detail.formula = TextFormat("Hit applies Chilled for %.1fs",
                                        std::max(1.0, spec.abilityDuration));
            detail.body = "Cold strike; Chilled targets move at 55% speed.";
            detail.save = TextFormat("Attack Roll %+d", spec.attackBonus);
            detail.recharge = TextFormat("Attack %.1fs", spec.attackCooldown);
            detail.range = "Melee";
            return detail;
        case AbilityKind::VenomousBite:
            detail.title = "Venomous Bite";
            detail.headline = abilityDamageLine(spec, AbilityKind::VenomousBite);
            detail.formula = "Bite, then venom";
            detail.body = "Heavy bite followed by venom. Poisoned targets lose attack accuracy.";
            detail.save = TextFormat("CON Save DC %d avoids Poisoned", spec.spellSaveDc);
            detail.recharge = TextFormat("Poisoned: %.1fs", std::max(1.2, spec.abilityDuration));
            detail.range = "Melee";
            return detail;
        case AbilityKind::DiabolicChains:
            detail.title = "Diabolic Chains";
            detail.headline = abilityDamageLine(spec, AbilityKind::DiabolicChains);
            detail.formula = "Up to 3 chained targets; failed save shoves";
            detail.body = "Infernal chains lash a cluster. Failed saves take the full hit, Slow, and shove land units backward.";
            detail.save = TextFormat("DEX Save DC %d halves and resists shove", spec.spellSaveDc);
            detail.recharge = TextFormat("Slow: %.1fs", std::max(0.8, spec.abilityDuration));
            detail.range = TextFormat("Range %d; chain radius 2", spec.range);
            return detail;
        case AbilityKind::KethericSmite:
            detail.title = "Ketheric's Smites";
            detail.headline = abilityDamageLine(spec, AbilityKind::KethericSmite);
            detail.formula = TextFormat("Alternates Wrathful shockwave and Blinding Smite; DC %d",
                                        spec.spellSaveDc);
            detail.body = "Wrathful Smite repels adjacent land enemies, then can Frighten. Blinding Smite can Blind.";
            detail.save = TextFormat("WIS avoids Frightened; CON avoids Blind, DC %d", spec.spellSaveDc);
            detail.recharge = TextFormat("Control: %.1fs", std::max(1.2, spec.abilityDuration));
            detail.range = "Melee";
            return detail;
        case AbilityKind::SelunesIre:
            detail.title = "Selune's Ire";
            detail.headline = abilityDamageLine(spec, AbilityKind::SelunesIre);
            detail.formula = "Radiant retaliation; fail: Blinded";
            detail.body = "Radiant retaliation that blinds careless attackers.";
            detail.save = TextFormat("DEX Save DC %d halves", spec.spellSaveDc);
            detail.recharge = TextFormat("Blind: %.1fs", std::max(0.8, spec.abilityDuration));
            detail.range = TextFormat("Range %d", spec.range);
            return detail;
        case AbilityKind::EvokerMagicMissile:
            detail.title = "Magic Missile Barrage";
            detail.headline = abilityDamageLine(spec, AbilityKind::EvokerMagicMissile);
            detail.formula = "Auto-hit missiles into a small cluster";
            detail.body = "Reliable force barrage; fragile caster.";
            detail.save = "No Save";
            detail.recharge = TextFormat("Attack %.1fs", spec.attackCooldown);
            detail.range = TextFormat("Range %d; cluster radius 1", spec.range);
            return detail;
        case AbilityKind::DominatePerson:
            detail.title = "Dominate Person";
            detail.headline = abilityDamageLine(spec, AbilityKind::DominatePerson);
            detail.formula = TextFormat("Once; WIS Save DC %d negates",
                                        spec.spellSaveDc);
            detail.body = "After provoked, converts one humanoid soldier on a failed save.";
            detail.save = TextFormat("WIS Save DC %d negates", spec.spellSaveDc);
            detail.recharge = "Once per combat";
            detail.range = TextFormat("Range %d; humanoid only", std::max(1, spec.abilityRange));
            return detail;
        case AbilityKind::StrikeOfTheGuardian:
            detail.title = "Strike of the Guardian";
            detail.headline = abilityDamageLine(spec, AbilityKind::StrikeOfTheGuardian);
            detail.formula = "Burst radius 1; radiant strike";
            detail.body = "Radiant sentinel strikes the marked area. Undead cannot halve the hit and take extra holy damage.";
            detail.save = TextFormat("DEX Save DC %d halves; undead punished", spec.spellSaveDc);
            detail.recharge = TextFormat("Attack %.1fs", spec.attackCooldown);
            detail.range = TextFormat("Range %d; burst radius 1", std::max(1, spec.abilityRange));
            return detail;
        case AbilityKind::MinotaurCharge:
            detail.title = "Charge";
            detail.headline = abilityDamageLine(spec, AbilityKind::MinotaurCharge);
            detail.formula = TextFormat("Line %d; fail: pushed and Prone",
                                        std::max(1, spec.abilityRange));
            detail.body = "Charges through a straight lane. Failed saves are pushed back; packed land units shove like dominoes.";
            detail.save = TextFormat("STR Save DC %d halves and avoids Prone", spec.spellSaveDc);
            detail.recharge = "Every attack";
            detail.range = TextFormat("Line %d; melee start", std::max(1, spec.abilityRange));
            return detail;
        case AbilityKind::Blight:
            detail.title = "Blight";
            detail.headline = abilityDamageLine(spec, AbilityKind::Blight);
            detail.formula = "8d8 Necrotic; plant-like targets suffer";
            detail.body = "Life-draining burst. Plant-like summons are especially vulnerable.";
            detail.save = TextFormat("CON Save DC %d halves", spec.spellSaveDc);
            detail.recharge = TextFormat("Attack %.1fs", spec.attackCooldown);
            detail.range = TextFormat("Range %d; single target", spec.range);
            return detail;
        case AbilityKind::StaggeringSmite:
            detail.title = "Staggering Smite";
            detail.headline = abilityDamageLine(spec, AbilityKind::StaggeringSmite);
            detail.formula = "Weapon hit; fail: Staggered";
            detail.body = "Weapon hit plus psychic pressure. Failed saves lose accuracy.";
            detail.save = TextFormat("WIS Save DC %d avoids Staggered", spec.spellSaveDc);
            detail.recharge = TextFormat("Attack %.1fs", spec.attackCooldown);
            detail.range = "Melee";
            return detail;
        case AbilityKind::ElectrifiedFlail:
            detail.title = "Electrified Flail";
            detail.headline = abilityDamageLine(spec, AbilityKind::ElectrifiedFlail);
            detail.formula = "Bludgeoning plus lightning; fail: Stunned";
            detail.body = "Bludgeoning impact followed by lightning. Failed saves are briefly Stunned.";
            detail.save = TextFormat("CON Save DC %d avoids Stunned", spec.spellSaveDc);
            detail.recharge = TextFormat("Attack %.1fs", spec.attackCooldown);
            detail.range = "Melee";
            return detail;
    }
    return detail;
}

AbilityDetail abilityDetail(const UnitSpec& spec) {
    return abilityDetailFor(spec, spec.ability);
}

std::string abilityText(const UnitSpec& spec) {
    switch (spec.ability) {
        case AbilityKind::None:
            return "Ability: none. Straightforward stat unit.";
        case AbilityKind::GithyankiAstralRaid:
            return "Ability: astral raid. At combat start, blinks up to 2 cells toward a high-value enemy, then the next attack has advantage and deals +15 psychic damage.";
        case AbilityKind::BarbarianHeavySwing:
            return "Ability: Berserker Rage. Reduces incoming damage, attacks faster when wounded, and every third attack adds a Frenzied Strike. Below half HP, damage and advantage spike again.";
        case AbilityKind::NecromancerSummon:
            return "Ability: raise dead. Summons temporary skeletons nearby if a land tile is open, up to 2 active summons per necromancer plus relic bonuses.";
        case AbilityKind::MephitDeathBurst:
            return "Ability: cinder burst. Attacks splash fire around the target; on death, nearby enemy land units make a Dex save or take 60 damage.";
        case AbilityKind::BossFireball:
            return "Ability: Fireball. Activated bosses target flying attackers at range 5; enemies around the impact make Dex saves, success halves damage.";
        case AbilityKind::PaladinCharge:
            return "Ability: divine charge. After moving, the next attack has advantage and deals double damage.";
        case AbilityKind::DragonBreath:
            return "Ability: fire breath. The dragon burns all enemies in range; each target makes a Dex save, success halves damage.";
        case AbilityKind::GuardianShield:
            return "Ability: arcane ward. Permanently taunts nearby target selection and gains 40 shield every 4 seconds, capped at 120.";
        case AbilityKind::ClericHeal:
            return "Ability: heal. Every 1.2 seconds, heals the most wounded ally within range 3 for 25 HP.";
        case AbilityKind::FrostNova:
            return "Ability: frost nova. Enemies near the target make a Con save; failures take full damage and are slowed for 2 seconds.";
        case AbilityKind::RogueAmbush:
            return "Ability: backline ambush. At combat start, jumps next to a high-value high-value enemy; opening attack has advantage and sneak attack damage.";
        case AbilityKind::DruidSummon:
            return "Ability: treant summon. Every 5 seconds, summons a temporary Treant nearby, up to 1 active summon per druid plus relic bonuses.";
        case AbilityKind::KarnissCruelSting:
            return "Ability: Multiattack - Cruel Sting. Kar'niss makes a three-part sting attack against wounded targets; targets at full HP take a normal hit.";
        case AbilityKind::SpectatorWoundingRay:
            return "Ability: Wounding Ray. Single-target necrotic ray; the victim makes a Con save, success halves damage.";
        case AbilityKind::MindBlast:
            return "Ability: Mind Blast. Wide psychic blast around the target; failed Int saves are stunned. Also has Counterspell once per game and Extract Brain once per lifetime.";
        case AbilityKind::Counterspell:
            return "Ability: Counterspell. Stops the first incoming spell-like attack once per game.";
        case AbilityKind::ExtractBrain:
            return "Ability: Extract Brain. Once per lifetime, instantly kills an adjacent Stunned creature and heals the mind flayer for 6d6.";
        case AbilityKind::AnimatingSpores:
            return "Ability: Animating Spores. Every 3 seconds, turns one nearby corpse into a temporary Spore Servant if a tile is open, up to 2 active servants. Neutral Spaw ignores player relic bonuses.";
        case AbilityKind::OwlbearMultiattack:
            return "Ability: Multiattack. Claw plus bite; surviving land targets are shoved back and knocked Prone.";
        case AbilityKind::HiemalStrike:
            return "Ability: Hiemal Strike. A frost-rimed trident hit adds cold damage and Chills the target, sharply slowing movement.";
        case AbilityKind::VenomousBite:
            return "Ability: Venomous Bite. Heavy bite damage followed by a Con save; failures become Poisoned and lose attack accuracy.";
        case AbilityKind::DiabolicChains:
            return "Ability: Diabolic Chains. Raphael lashes up to three clustered enemies; failed Dex saves take full fire damage, Slow, and are shoved back.";
        case AbilityKind::KethericSmite:
            return "Ability: Ketheric's Warhammer. Alternates Wrathful Smite, which repels nearby land enemies and can Frighten, with Blinding Smite.";
        case AbilityKind::SelunesIre:
            return "Ability: Selune's Ire. Radiant retaliation that can Blind on a failed Dex save.";
        case AbilityKind::EvokerMagicMissile:
            return "Ability: Magic Missile barrage. Glass-cannon auto-hit missiles split across clustered enemies.";
        case AbilityKind::DominatePerson:
            return "Ability: Dominate Person. Passive once per combat; failed Wis save converts one humanoid soldier into Tamia's ally.";
        case AbilityKind::StrikeOfTheGuardian:
            return "Ability: Strike of the Guardian. Area radiant strike for 20 radiant + 20 radiant; Dex save halves, but undead take the full strike plus extra holy damage.";
        case AbilityKind::MinotaurCharge:
            return "Ability: Charge. Line attack for 4d8 + 4 piercing; failed Str saves trigger domino pushback and Prone.";
        case AbilityKind::Blight:
            return "Ability: Blight. Deals 8d8 necrotic damage; a Con save halves it, and plant-like targets are hit at maximum force.";
        case AbilityKind::StaggeringSmite:
            return "Ability: Staggering Smite. Weapon damage plus 4d6 psychic; failed Wis saves become Staggered and miss more often.";
        case AbilityKind::ElectrifiedFlail:
            return "Ability: Electrified Flail. 1d8 + 7 bludgeoning plus 1d8 + 1d10 lightning; failed Con saves are Stunned.";
    }
    return "Ability: unknown.";
}

std::string abilitySummary(const UnitSpec& spec) {
    switch (spec.ability) {
        case AbilityKind::None: return "No special ability.";
        case AbilityKind::GithyankiAstralRaid: return "Astral raid: opens with a short blink and empowered strike.";
        case AbilityKind::BarbarianHeavySwing: return "Berserker Rage: damage reduction, wound-scaling speed, Frenzied Strike.";
        case AbilityKind::NecromancerSummon: return "Raises temporary skeletons, capped per necromancer.";
        case AbilityKind::MephitDeathBurst: return "Cinder splash attacks; death burst forces nearby Dex saves.";
        case AbilityKind::BossFireball: return "Boss Fireball: long-range anti-air blast; Dex save halves.";
        case AbilityKind::PaladinCharge: return "After moving, next hit has advantage and double damage.";
        case AbilityKind::DragonBreath: return "Fire breath hits all enemies in range; Dex save halves.";
        case AbilityKind::GuardianShield: return "Taunts enemies and gains shield over time.";
        case AbilityKind::ClericHeal: return "Heals the most wounded nearby ally.";
        case AbilityKind::FrostNova: return "Frost nova forces Con saves and slows failures.";
        case AbilityKind::RogueAmbush: return "Backline leap, advantage opener, sneak attack.";
        case AbilityKind::DruidSummon: return "Summons temporary Treants, capped per druid.";
        case AbilityKind::KarnissCruelSting: return "Multiattack - Cruel Sting: triple strike against wounded targets.";
        case AbilityKind::SpectatorWoundingRay: return "Wounding Ray: necrotic ray, Con save halves.";
        case AbilityKind::MindBlast: return "Mind Blast stuns in radius 2; Counterspell once per game; Extract Brain executes stunned.";
        case AbilityKind::Counterspell: return "Stops the first incoming spell-like attack once per game.";
        case AbilityKind::ExtractBrain: return "Executes an adjacent Stunned target once per lifetime.";
        case AbilityKind::AnimatingSpores: return "Turns nearby corpses into capped Spore Servants.";
        case AbilityKind::OwlbearMultiattack: return "Multiattack: claw, bite, shove, and brief Prone.";
        case AbilityKind::HiemalStrike: return "Hiemal Strike: cold burst and Chilled slow.";
        case AbilityKind::VenomousBite: return "Venomous Bite: poison save after a heavy bite.";
        case AbilityKind::DiabolicChains: return "Diabolic Chains: clustered fire lash, Slow, and shove on failed Dex save.";
        case AbilityKind::KethericSmite: return "Ketheric's smites alternate shockwave Frightened pressure and Blind.";
        case AbilityKind::SelunesIre: return "Selune's Ire: radiant retaliation that can blind on a failed Dex save.";
        case AbilityKind::EvokerMagicMissile: return "Glass cannon: auto-hit missiles into clusters.";
        case AbilityKind::DominatePerson: return "Dominate Person: one failed Wis save can convert a humanoid soldier.";
        case AbilityKind::StrikeOfTheGuardian: return "Radiant area strike; undead take extra holy damage.";
        case AbilityKind::MinotaurCharge: return "Line charge: piercing damage, domino knockback, and Prone on failed Str save.";
        case AbilityKind::Blight: return "Blight: 8d8 necrotic damage, Con save halves.";
        case AbilityKind::StaggeringSmite: return "Staggering Smite: weapon hit plus 4d6 psychic and Staggered on failed Wis save.";
        case AbilityKind::ElectrifiedFlail: return "Electrified Flail: bludgeoning plus lightning, with Stunned on failed Con save.";
    }
    return "Unknown ability.";
}

std::vector<int> uiFontCodepoints() {
    std::vector<int> codepoints;
    codepoints.reserve(95);
    for (int cp = 32; cp <= 126; ++cp) codepoints.push_back(cp);
    return codepoints;
}

Font loadUiFont() {
    const std::vector<const char*> candidates = {
        "assets/ui/fonts/QuadraatPro-Regular.otf",
        "assets/ui/fonts/QuadraatPro-Regular.ttf",
        "assets/ui/fonts/Quadraat Pro Regular.otf",
        "assets/ui/fonts/Quadraat Pro Regular.ttf",
        "assets/fonts/QuadraatPro-Regular.otf",
        "assets/fonts/Quadraat Pro Regular.otf",
        "C:/Windows/Fonts/QuadraatPro-Regular.otf",
        "C:/Windows/Fonts/Quadraat Pro Regular.otf",
        "C:/Windows/Fonts/constan.ttf",
        "C:/Windows/Fonts/GARA.TTF",
        "C:/Windows/Fonts/georgia.ttf",
        "C:/Windows/Fonts/cambria.ttc",
        "C:/Windows/Fonts/BOOKOS.TTF",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/verdana.ttf",
        "C:/Windows/Fonts/calibri.ttf",
        "C:/Windows/Fonts/tahoma.ttf"
    };
    std::vector<int> codepoints = uiFontCodepoints();
    for (const char* path : candidates) {
        if (!FileExists(path)) continue;
        Font font = LoadFontEx(path, kUiFontAtlasSize, codepoints.data(), static_cast<int>(codepoints.size()));
        if (font.texture.id != 0) {
            SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
            gCustomFont = true;
            return font;
        }
    }
    gCustomFont = false;
    return GetFontDefault();
}

Font loadBoldUiFont() {
    const std::vector<const char*> candidates = {
        "assets/ui/fonts/QuadraatPro-Bold.otf",
        "assets/ui/fonts/QuadraatPro-Bold.ttf",
        "assets/ui/fonts/Quadraat Pro Bold.otf",
        "assets/ui/fonts/Quadraat Pro Bold.ttf",
        "assets/ui/fonts/QuadraatPro-Regular.otf",
        "assets/ui/fonts/Quadraat Pro Regular.otf",
        "assets/fonts/QuadraatPro-Bold.otf",
        "assets/fonts/Quadraat Pro Bold.otf",
        "C:/Windows/Fonts/QuadraatPro-Bold.otf",
        "C:/Windows/Fonts/Quadraat Pro Bold.otf",
        "C:/Windows/Fonts/constanb.ttf",
        "C:/Windows/Fonts/GARABD.TTF",
        "C:/Windows/Fonts/georgiab.ttf",
        "C:/Windows/Fonts/cambriab.ttf",
        "C:/Windows/Fonts/BOOKOSB.TTF",
        "C:/Windows/Fonts/segoeuib.ttf",
        "C:/Windows/Fonts/arialbd.ttf",
        "C:/Windows/Fonts/verdanab.ttf",
        "C:/Windows/Fonts/seguibl.ttf",
        "C:/Windows/Fonts/tahomabd.ttf"
    };
    std::vector<int> codepoints = uiFontCodepoints();
    for (const char* path : candidates) {
        if (!FileExists(path)) continue;
        Font font = LoadFontEx(path, kUiFontAtlasSize, codepoints.data(), static_cast<int>(codepoints.size()));
        if (font.texture.id != 0) {
            SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
            gCustomBoldFont = true;
            return font;
        }
    }
    gCustomBoldFont = false;
    return gFont.texture.id != 0 ? gFont : GetFontDefault();
}

bool hasPlayerCombatUnit(const GameSnapshot& snapshot) {
    for (const UnitView& unit : snapshot.units) {
        if (unit.owner == PlayerId::One && unit.alive && unit.deployed &&
            !isInternalUnit(unit.type)) {
            return true;
        }
    }
    return false;
}

std::string difficultyLabel(AiDifficulty difficulty) {
    return difficulty == AiDifficulty::SuperHard ? "Super" : toString(difficulty);
}

void drawDifficultySelector(const GameEngine& engine, const GameSnapshot& snapshot) {
    const std::array<AiDifficulty, 3> difficulties = {
        AiDifficulty::Normal,
        AiDifficulty::Hard,
        AiDifficulty::SuperHard
    };
    for (int i = 0; i < static_cast<int>(difficulties.size()); ++i) {
        Rectangle rect = difficultyRect(i);
        bool selected = engine.aiDifficulty() == difficulties[i];
        bool disabled = snapshot.phase != Phase::Preparation;
        Color fill = selected ? Color{71, 49, 34, 255} : Color{31, 25, 22, 255};
        if (disabled && !selected) fill = Color{22, 20, 20, 255};
        DrawRectangleRounded(rect, 0.08f, 8, fill);
        DrawRectangleRoundedLines(rect, 0.08f, 8, 1.0f,
                                  selected ? kGold : Color{116, 83, 47, 255});
        drawTextCentered(fitText(difficultyLabel(difficulties[i]), rect.width - 8.0f, 18.0f),
                         rect,
                         18.0f,
                         disabled && !selected ? GRAY : kInk);
    }
}

void drawTopStatusHeader(const GameSnapshot& snapshot) {
    Rectangle board = boardRect();
    Rectangle titlePlate{kBoardX + 18.0f, 16.0f, board.width - 36.0f, 62.0f};
    DrawRectangleGradientH(static_cast<int>(titlePlate.x),
                           static_cast<int>(titlePlate.y),
                           static_cast<int>(titlePlate.width),
                           static_cast<int>(titlePlate.height),
                           Color{8, 6, 5, 236},
                           Color{8, 6, 5, 132});
    const std::string title = "Exploration Run";
    drawTextShadowed(title, kBoardX + 30.0f, 22.0f, 42.0f, kInk);

    std::vector<std::pair<std::string, std::string>> metrics;
    metrics.push_back({"Round", std::to_string(snapshot.round)});
    metrics.push_back({"Phase", toString(snapshot.phase)});
    metrics.push_back({"Explore", TextFormat("%d/%d", snapshot.explorationRound, snapshot.explorationRoundLimit)});
    metrics.push_back({"Score", TextFormat("%d/%d", snapshot.explorationScores[0], snapshot.explorationScores[1])});
    metrics.push_back({"Gold", std::to_string(snapshot.players[0].money)});

    constexpr float labelSize = 20.0f;
    constexpr float valueSize = 30.0f;
    constexpr float gap = 42.0f;
    std::vector<float> widths;
    widths.reserve(metrics.size());
    float totalWidth = 0.0f;
    for (const auto& metric : metrics) {
        float labelWidth = measureText(metric.first, labelSize).x;
        float valueWidth = measureTextStrong(metric.second, valueSize).x;
        float width = std::max(labelWidth, valueWidth);
        widths.push_back(width);
        totalWidth += width;
    }
    if (!metrics.empty()) totalWidth += gap * static_cast<float>(metrics.size() - 1);

    float titleWidth = measureTextStrong(title, 42.0f).x;
    float minX = kBoardX + 30.0f + titleWidth + 72.0f;
    float rightEdge = board.x + board.width - 36.0f;
    float x = std::max(minX, rightEdge - totalWidth);

    for (int i = 0; i < static_cast<int>(metrics.size()); ++i) {
        bool gold = metrics[static_cast<size_t>(i)].first == "Gold";
        if (i > 0) {
            DrawLineEx({x - gap * 0.5f, 26.0f},
                       {x - gap * 0.5f, 62.0f},
                       1.0f,
                       Color{113, 84, 48, 120});
        }
        drawText(metrics[static_cast<size_t>(i)].first,
                 x,
                 14.0f,
                 labelSize,
                 gold ? Color{218, 181, 102, 245} : kMutedInk);
        drawTextStrong(metrics[static_cast<size_t>(i)].second,
                       x,
                       38.0f,
                       valueSize,
                       gold ? kGold : kInk);
        x += widths[static_cast<size_t>(i)] + gap;
    }
}

void drawRunInfoPanel(const GameSnapshot& snapshot, bool roundChoicesDismissed) {
    Rectangle panel = explorationStatusRect();
    drawParchmentPanel(panel, kParchment);

    bool complete = snapshot.explorationObjectivesTotal > 0 &&
                    snapshot.explorationObjectivesCleared >= snapshot.explorationObjectivesTotal;
    drawTextStrong("Dungeon Run", panel.x + 18.0f, panel.y + 12.0f, 28.0f, kParchmentInk);
    drawTextStrong(fitTextStrong(complete ? "Complete" : TextFormat("Remaining %d", snapshot.explorationRoundsRemaining),
                                 220.0f,
                                 24.0f),
                   panel.x + panel.width - 230.0f, panel.y + 14.0f, 24.0f,
                   complete ? kWine : kParchmentInk);
    std::string stats = TextFormat("Objectives %d/%d   Boss %d   Trap %d   Secrets %d   Score %d/%d",
                                   snapshot.explorationObjectivesCleared,
                                   snapshot.explorationObjectivesTotal,
                                   snapshot.bossesCleared,
                                   snapshot.trapsTriggered,
                                   snapshot.hiddenEventsClaimed,
                                   snapshot.explorationScores[0],
                                   snapshot.explorationScores[1]);
    drawText(fitText(stats, panel.width - 36.0f, 19.0f),
             panel.x + 18.0f,
             panel.y + 45.0f,
             19.0f,
             kParchmentMuted);

    const std::array<int, 5> choices = {2, 4, 6, 8, 10};
    bool enabled = snapshot.phase == Phase::Preparation && snapshot.explorationRound == 0 &&
                   !snapshot.explorationRoundLimitLocked;
    bool showChoices = enabled && !roundChoicesDismissed;
    if (showChoices) {
        for (int i = 0; i < static_cast<int>(choices.size()); ++i) {
            Rectangle rect = explorationRoundPanelRect(i);
            bool selected = snapshot.explorationRoundLimit == choices[static_cast<size_t>(i)];
            Color fill = selected ? Color{78, 56, 38, 255} : Color{55, 43, 33, 235};
            if (!enabled && !selected) fill = Color{88, 78, 63, 170};
            DrawRectangleRounded(rect, 0.10f, 8, fill);
            DrawRectangleRoundedLines(rect, 0.10f, 8, selected ? 2.4f : 1.4f,
                                      selected ? kGold : Color{142, 102, 59, 230});
            drawTextCenteredStrong(std::to_string(choices[static_cast<size_t>(i)]),
                                   rect,
                                   22.0f,
                                   enabled || selected ? kInk : Color{92, 75, 57, 255});
        }
    }

    if (!showChoices) {
        Rectangle progress{panel.x + 18.0f, panel.y + 72.0f, panel.width - 36.0f, 28.0f};
        DrawRectangleRounded(progress, 0.12f, 6, Color{61, 48, 37, 205});
        float ratio = complete
                          ? 1.0f
                          : (snapshot.explorationRoundLimit > 0
                                 ? std::clamp(static_cast<float>(snapshot.explorationRound) /
                                                  static_cast<float>(snapshot.explorationRoundLimit),
                                              0.0f,
                                              1.0f)
                                 : 0.0f);
        DrawRectangleRounded({progress.x + 3.0f, progress.y + 3.0f,
                              (progress.width - 6.0f) * ratio, progress.height - 6.0f},
                             0.10f, 6, Color{86, 127, 101, 220});
        DrawRectangleRoundedLines(progress, 0.12f, 6, 1.2f, Color{105, 82, 54, 210});
        drawTextCenteredStrong(complete ? "Run complete"
                                        : TextFormat("%d / %d Rounds",
                                                     snapshot.explorationRound,
                                                     snapshot.explorationRoundLimit),
                               progress, 18.0f, kInk);
    }
}

Color relicFamilyColor(NeutralFamily family) {
    switch (family) {
        case NeutralFamily::Swarm: return Color{110, 206, 172, 255};
        case NeutralFamily::Guardian: return Color{112, 161, 220, 255};
        case NeutralFamily::Caster: return Color{171, 124, 229, 255};
        case NeutralFamily::Assassin: return Color{219, 99, 129, 255};
        case NeutralFamily::Artillery: return Color{232, 165, 86, 255};
    }
    return kAccent;
}

Color relicTierColor(RelicTier tier) {
    switch (tier) {
        case RelicTier::Basic: return Color{112, 206, 172, 255};
        case RelicTier::Build: return Color{113, 179, 145, 255};
        case RelicTier::Transform: return Color{226, 175, 78, 255};
        case RelicTier::Unique: return Color{225, 190, 106, 255};
    }
    return kAccent;
}

const Texture2D* relicFamilyTexture(NeutralFamily family) {
    if (const Texture2D* texture = neutralFamilyTexture(family)) return texture;
    switch (family) {
        case NeutralFamily::Swarm: return unitTexture(UnitType::ImpSwarm);
        case NeutralFamily::Guardian: return unitTexture(UnitType::NeutralOwlbear);
        case NeutralFamily::Caster: return unitTexture(UnitType::NeutralSpectator);
        case NeutralFamily::Assassin: return unitTexture(UnitType::RogueAssassin);
        case NeutralFamily::Artillery: return unitTexture(UnitType::DragonWyrmling);
    }
    return nullptr;
}

NeutralFamily relicFamilyHint(const RelicSpec& relic) {
    auto hasTag = [&](const char* tag) {
        return std::find(relic.tags.begin(), relic.tags.end(), tag) != relic.tags.end();
    };
    if (relic.modifiers.familyBias[0] > 0 || hasTag("swarm")) return NeutralFamily::Swarm;
    if (relic.modifiers.familyBias[1] > 0 || hasTag("guardian")) return NeutralFamily::Guardian;
    if (relic.modifiers.familyBias[2] > 0 || hasTag("caster")) return NeutralFamily::Caster;
    if (relic.modifiers.familyBias[3] > 0 || hasTag("assassin")) return NeutralFamily::Assassin;
    if (relic.modifiers.familyBias[4] > 0 || hasTag("artillery")) return NeutralFamily::Artillery;
    return NeutralFamily::Swarm;
}

void drawRelicIcon(const RelicSpec& relic, Rectangle rect) {
    DrawRectangleRounded(rect, 0.18f, 8, Color{58, 38, 30, 220});
    DrawRectangleRoundedLines(rect, 0.18f, 8, 1.4f, relicFamilyColor(relicFamilyHint(relic)));
    Rectangle inner{rect.x + rect.width * 0.10f,
                    rect.y + rect.height * 0.10f,
                    rect.width * 0.80f,
                    rect.height * 0.80f};
    if (const Texture2D* texture = relicTexture(relic)) {
        drawTextureAspectFit(*texture, inner, WHITE);
    } else if (const Texture2D* texture = relicFamilyTexture(relicFamilyHint(relic))) {
        drawTextureAspectFit(*texture, inner, WHITE);
    } else {
        drawTextCentered(relic.name.substr(0, 1), inner, inner.height * 0.50f, kInk);
    }
}

int draftVisibleStart(int scrollIndex, int count) {
    if (count <= 2) return 0;
    return std::clamp(scrollIndex, 0, count - 2);
}

Rectangle relicInfoCardRect(Rectangle panel, int visibleIndex, int visibleCount) {
    float gap = 14.0f;
    float cardW = (panel.width - 28.0f - gap * (visibleCount - 1)) / std::max(1, visibleCount);
    float cardH = std::max(360.0f, panel.height - 152.0f);
    return {panel.x + 14.0f + visibleIndex * (cardW + gap), panel.y + 72.0f, cardW, cardH};
}

Rectangle draftScrollButtonRect(Rectangle panel, bool right) {
    float size = 34.0f;
    float rightX = panel.x + panel.width - 18.0f - size;
    float leftX = rightX - 8.0f - size;
    return {right ? rightX : leftX,
            panel.y + 15.0f,
            size,
            size};
}

Rectangle draftScrollPageRect(Rectangle panel) {
    Rectangle left = draftScrollButtonRect(panel, false);
    return {left.x - 100.0f, panel.y + 17.0f, 88.0f, 28.0f};
}

void drawRelicCard(const DraftOffer& offer, Rectangle rect, bool selected) {
    Color fill = selected ? Color{230, 209, 158, 255} : Color{204, 183, 136, 255};
    drawParchmentPanel(rect, fill);
    DrawRectangleRec({rect.x + 4.0f, rect.y + 4.0f, 10.0f, rect.height - 8.0f},
                     relicTierColor(offer.relic.tier));
    Rectangle chip{rect.x + 14.0f, rect.y + 12.0f, rect.width - 28.0f, 38.0f};
    DrawRectangleRounded(chip, 0.18f, 6, Color{70, 45, 29, 210});
    DrawRectangleRoundedLines(chip, 0.18f, 6, 1.5f, kGold);
    drawTextCentered(fitText(offer.relic.name, chip.width - 16.0f, 25.0f), chip, 25.0f, kInk);

    float iconSize = std::min(116.0f, rect.width - 66.0f);
    iconSize = std::max(82.0f, iconSize);
    Rectangle icon{rect.x + (rect.width - iconSize) * 0.5f, rect.y + 64.0f, iconSize, iconSize};
    drawRelicIcon(offer.relic, icon);

    drawTextCentered(relicTierLabel(offer.relic.tier),
                     {rect.x + 18.0f, rect.y + 190.0f, rect.width - 36.0f, 28.0f},
                     22.0f, relicFamilyColor(relicFamilyHint(offer.relic)));

    Rectangle reasonChip{rect.x + 18.0f, rect.y + 226.0f, rect.width - 36.0f, 34.0f};
    DrawRectangleRounded(reasonChip, 0.18f, 6, Color{84, 58, 35, 220});
    DrawRectangleRoundedLines(reasonChip, 0.18f, 6, 1.0f, Color{230, 208, 153, 255});
    drawTextCentered(fitText(offer.reason, reasonChip.width - 18.0f, 20.0f), reasonChip, 20.0f, kInk);

    drawWrappedTextLimited(offer.relic.summary,
                           rect.x + 18.0f,
                           rect.y + 278.0f,
                           rect.width - 36.0f,
                           21.0f,
                           kParchmentMuted,
                           rect.y + rect.height - 18.0f,
                           6);
}

void drawRelicDraftPanel(const DraftState& draft,
                         const RunModifiers& modifiers,
                         int draftScrollIndex = 0) {
    Rectangle panel = detailRect();
    drawPanelFrame(panel, Color{29, 21, 19, 255});
    drawTextStrong("Relic Reward", panel.x + 14.0f, panel.y + 12.0f, 26.0f, kInk);
    if (!draft.fixedDropText.empty()) {
        drawText(fitText(draft.fixedDropText, panel.width - 220.0f, 18.0f),
                 panel.x + 14.0f, panel.y + 44.0f, 18.0f, kGold);
    }
    int offerCount = static_cast<int>(draft.offers.size());
    int visibleCount = std::min(2, offerCount);
    int start = draftVisibleStart(draftScrollIndex, offerCount);
    BeginScissorMode(static_cast<int>(panel.x + 10.0f),
                     static_cast<int>(panel.y + 66.0f),
                     static_cast<int>(panel.width - 20.0f),
                     static_cast<int>(panel.height - 76.0f));
    for (int i = 0; i < visibleCount; ++i) {
        int offerIndex = start + i;
        if (offerIndex < 0 || offerIndex >= offerCount) continue;
        Rectangle card = relicInfoCardRect(panel, i, visibleCount);
        drawRelicCard(draft.offers[static_cast<size_t>(offerIndex)], card, false);
    }
    EndScissorMode();

    Rectangle footerBand{panel.x + 12.0f,
                         panel.y + panel.height - 64.0f,
                         panel.width - 24.0f,
                         52.0f};
    DrawRectangleRounded(footerBand, 0.08f, 6, Color{46, 34, 28, 214});
    DrawRectangleRoundedLines(footerBand, 0.08f, 6, 1.0f, Color{123, 90, 52, 200});
    DrawLineEx({footerBand.x + 10.0f, footerBand.y},
               {footerBand.x + footerBand.width - 10.0f, footerBand.y},
               1.0f,
               Color{145, 107, 58, 110});

    float footerX = footerBand.x + 10.0f;
    drawText(fitText(TextFormat("Relics %d   Cost -%d   Income +%d   Roster +%d",
                                static_cast<int>(modifiers.relicIds.size()),
                                modifiers.costDiscount,
                                modifiers.roundIncomeBonus,
                                modifiers.benchBonus),
                     footerBand.width - 20.0f,
                     16.0f),
             footerX,
             footerBand.y + 7.0f,
             16.0f,
             kAccentAlt);
    drawText(fitText(TextFormat("Interest +%d   Bonus clear gold +%d   Draft +%d   Summon cap +%d",
                                modifiers.interestBonus,
                                modifiers.bonusGoldOnClear,
                                modifiers.extraRelicChoices,
                                modifiers.summonLimitBonus),
                     footerBand.width - 20.0f,
                     16.0f),
             footerX,
             footerBand.y + 25.0f,
             16.0f,
             kMutedInk);

    if (offerCount > visibleCount) {
        Rectangle left = draftScrollButtonRect(panel, false);
        Rectangle right = draftScrollButtonRect(panel, true);
        for (Rectangle button : {left, right}) {
            DrawRectangleRounded(button, 0.16f, 6, Color{54, 37, 29, 240});
            DrawRectangleRoundedLines(button, 0.16f, 6, 1.2f, kGold);
        }
        drawTextCentered("<", left, 24.0f, kInk);
        drawTextCentered(">", right, 24.0f, kInk);
        std::string page = TextFormat("%d-%d / %d", start + 1, std::min(start + visibleCount, offerCount), offerCount);
        Rectangle pageSlot = draftScrollPageRect(panel);
        DrawRectangleRounded(pageSlot, 0.12f, 5, Color{42, 31, 26, 172});
        DrawRectangleRoundedLines(pageSlot, 0.12f, 5, 1.0f, Color{118, 85, 49, 165});
        drawTextCentered(fitText(page, pageSlot.width - 10.0f, 16.0f), pageSlot, 16.0f, kMutedInk);
    }
}

int draftOfferAtMouse(const DraftState& draft, Vector2 mouse, Rectangle panel, int scrollIndex) {
    int count = static_cast<int>(draft.offers.size());
    int visibleCount = std::min(2, count);
    int start = draftVisibleStart(scrollIndex, count);
    for (int i = 0; i < visibleCount; ++i) {
        if (CheckCollisionPointRec(mouse, relicInfoCardRect(panel, i, visibleCount))) return start + i;
    }
    return -1;
}

void applyRelicChoice(const DraftOffer& offer,
                      std::vector<std::string>& relicIds,
                      RunModifiers& modifiers,
                      GameEngine& engine) {
    if (std::find(relicIds.begin(), relicIds.end(), offer.relic.id) == relicIds.end() ||
        offer.relic.stackable) {
        relicIds.push_back(offer.relic.id);
        modifiers = runModifiersForRelics(relicIds);
        engine.setRunModifiers(PlayerId::One, modifiers);
    }
}

bool neutralTypeDropsRelic(UnitType type) {
    return isNeutralMonster(type) && relicDropEligibleForObjective(ExplorationObjectiveKind::Camp, type);
}

std::optional<ExplorationObjectiveKind> objectiveKindFromClearText(const std::string& text) {
    if (text.find("Camp cleared") != std::string::npos) return ExplorationObjectiveKind::Camp;
    if (text.find("Elite cleared") != std::string::npos) return ExplorationObjectiveKind::Elite;
    if (text.find("Boss defeated") != std::string::npos) return ExplorationObjectiveKind::Boss;
    return std::nullopt;
}

DraftState createRandomNeutralDraft(const std::string& sourceName,
                                    const std::vector<std::string>& relicIds,
                                    int extraChoices,
                                    unsigned seed) {
    DraftState draft;
    draft.fixedDropText = sourceName + " dropped a random relic";

    std::vector<RelicSpec> candidates;
    for (const RelicSpec& relic : relicCatalog()) {
        bool owned = std::find(relicIds.begin(), relicIds.end(), relic.id) != relicIds.end();
        if ((relic.unique || !relic.stackable) && owned) continue;
        candidates.push_back(relic);
    }
    std::mt19937 rng(seed);
    std::shuffle(candidates.begin(), candidates.end(), rng);
    int count = std::min(3 + std::max(0, extraChoices), static_cast<int>(candidates.size()));
    for (int i = 0; i < count; ++i) {
        draft.offers.push_back({candidates[static_cast<size_t>(i)], 1, "random drop"});
    }
    return draft;
}

void promoteNextDraft(std::optional<DraftState>& pendingDraft, std::vector<DraftState>& queuedDrafts) {
    if (pendingDraft || queuedDrafts.empty()) return;
    pendingDraft = std::move(queuedDrafts.front());
    queuedDrafts.erase(queuedDrafts.begin());
}

void queueNeutralRelicDrops(const std::vector<Event>& events,
                            const std::vector<std::string>& relicIds,
                            const RunModifiers& modifiers,
                            unsigned& seed,
                            std::vector<std::string>& log,
                            std::optional<DraftState>& pendingDraft,
                            std::vector<DraftState>& queuedDrafts) {
    std::vector<UnitId> queuedTargets;
    for (const Event& event : events) {
        if (event.type != EventType::GoldGained || event.player != PlayerId::One ||
            event.target == kInvalidUnitId || event.amount <= 0) {
            continue;
        }
        std::optional<ExplorationObjectiveKind> objectiveKind = objectiveKindFromClearText(event.text);
        if (!objectiveKind) continue;
        if (std::find(queuedTargets.begin(), queuedTargets.end(), event.target) != queuedTargets.end()) continue;
        queuedTargets.push_back(event.target);

        std::string sourceName = event.text;
        size_t colon = sourceName.find(':');
        if (colon != std::string::npos) sourceName = sourceName.substr(0, colon);
        unsigned dropSeed = seed++ ^ static_cast<unsigned>(event.target * 97);
        if (!relicDropsForObjectiveClear(*objectiveKind, UnitType::Skeleton, dropSeed)) {
            log.push_back(sourceName + ": no relic found");
            continue;
        }
        DraftState draft = createRandomNeutralDraft(sourceName, relicIds, modifiers.extraRelicChoices,
                                                    seed++ ^ static_cast<unsigned>(event.target * 193));
        if (draft.offers.empty()) continue;
        queuedDrafts.push_back(std::move(draft));
    }
    promoteNextDraft(pendingDraft, queuedDrafts);
}

const UnitSpec* shopSpecAtMouse(const GameEngine& engine, Vector2 mouse, float shopScroll) {
    if (!CheckCollisionPointRec(mouse, shopViewportRect())) return nullptr;
    const std::vector<const UnitSpec*>& specs = orderedShopSpecs(engine);
    for (int i = 0; i < static_cast<int>(specs.size()); ++i) {
        Rectangle r = shopCardRect(i, shopScroll);
        if (r.y + r.height < shopViewportRect().y || r.y > shopViewportRect().y + shopViewportRect().height) continue;
        if (CheckCollisionPointRec(mouse, r)) return specs[static_cast<size_t>(i)];
    }
    return nullptr;
}

const UnitView* unitAtMouse(const GameSnapshot& snapshot, Vector2 mouse) {
    if (const UnitView* boardUnit = unitAtBoardMouse(snapshot, mouse, false)) return boardUnit;

    int visibleSlots = std::max(10, static_cast<int>(snapshot.players[0].bench.size()));
    for (int i = 0; i < static_cast<int>(snapshot.players[0].bench.size()); ++i) {
        Rectangle slot = benchSlotRect(i, visibleSlots);
        if (CheckCollisionPointRec(mouse, slot)) return findUnit(snapshot, snapshot.players[0].bench[i]);
    }
    return nullptr;
}

struct DetailSelection {
    const UnitSpec* spec = nullptr;
    const UnitView* unit = nullptr;
};

DetailSelection resolveDetailSelection(const GameEngine& engine,
                                       const GameSnapshot& snapshot,
                                       Vector2 mouse,
                                       float shopScroll,
                                       UnitId pinnedUnit) {
    if (const UnitSpec* shopSpec = shopSpecAtMouse(engine, mouse, shopScroll)) {
        return {shopSpec, nullptr};
    }

    if (const UnitView* hoveredUnit = unitAtMouse(snapshot, mouse)) {
        const UnitSpec* spec = engine.specFor(hoveredUnit->type);
        return {spec ? spec : shopSpecAtIndex(engine, 0), hoveredUnit};
    }

    if (pinnedUnit != kInvalidUnitId) {
        if (const UnitView* pinned = findUnit(snapshot, pinnedUnit); pinned && pinned->alive) {
            const UnitSpec* spec = engine.specFor(pinned->type);
            return {spec ? spec : shopSpecAtIndex(engine, 0), pinned};
        }
    }

    for (UnitId id : snapshot.players[0].deployed) {
        const UnitView* unit = findUnit(snapshot, id);
        if (unit && unit->alive && !isInternalUnit(unit->type)) {
            const UnitSpec* spec = engine.specFor(unit->type);
            return {spec ? spec : shopSpecAtIndex(engine, 0), unit};
        }
    }
    for (UnitId id : snapshot.players[0].bench) {
        const UnitView* unit = findUnit(snapshot, id);
        if (unit && unit->alive) {
            const UnitSpec* spec = engine.specFor(unit->type);
            return {spec ? spec : shopSpecAtIndex(engine, 0), unit};
        }
    }

    return {shopSpecAtIndex(engine, 0), nullptr};
}

void drawAbilityDetailsInline(const UnitSpec& spec,
                              AbilityKind ability,
                              float x,
                              float y,
                              float width,
                              float bottomY) {
    if (bottomY - y < 64.0f) return;

    AbilityDetail detail = abilityDetailFor(spec, ability);
    Color accent = abilitySchoolColor(ability);
    Color title = kParchmentInk;
    Color muted = Color{94, 69, 48, 255};
    Color bodyText = Color{52, 38, 29, 255};
    Color formulaText = Color{88, 48, 32, 255};
    float available = bottomY - y;
    bool compact = available < 220.0f;

    DrawLineEx({x, y}, {x + width, y}, 1.0f, Color{132, 101, 64, 125});
    y += compact ? 10.0f : 12.0f;

    float iconSize = compact ? 46.0f : 56.0f;
    float titleSize = compact ? 23.0f : 28.0f;
    float metaTopSize = compact ? 15.0f : 17.0f;
    Rectangle icon{x, y, iconSize, iconSize};
    drawAbilitySigil(icon, ability);
    drawTextStrong(fitTextStrong(detail.title, width - iconSize - 14.0f, titleSize),
                   x + iconSize + 12.0f, y, titleSize, title);
    drawText(fitText(abilitySchoolLabel(ability) + "; " + detail.range,
                     width - iconSize - 14.0f, metaTopSize),
             x + iconSize + 12.0f, y + (compact ? 27.0f : 35.0f), metaTopSize, muted);
    y += iconSize + (compact ? 12.0f : 16.0f);

    DrawLineEx({x, y - 4.0f}, {x + width, y - 4.0f}, 2.0f, Color{accent.r, accent.g, accent.b, 125});

    const float footerH = compact ? 46.0f : 52.0f;
    const float footerY = bottomY - footerH;
    const float textBottom = footerY - 8.0f;
    const float headlineSize = compact ? 20.0f : 22.0f;
    const float formulaSize = compact ? 17.0f : 19.0f;
    const float bodySize = compact ? 17.0f : 19.0f;
    const float lineGap = compact ? 5.0f : 7.0f;

    if (!detail.headline.empty() && y < textBottom) {
        DamageTextParts damageText = splitDamageText(detail.headline);
        std::string primaryLine = damageText.formulaLine.empty() ? damageText.rangeLine : damageText.formulaLine;
        drawTextStrong(fitTextStrong(primaryLine, width, headlineSize), x, y, headlineSize, bodyText);
        y += headlineSize * kFontScale + lineGap;
        if (!damageText.formulaLine.empty() && damageText.rangeLine != primaryLine && y < textBottom) {
            y = drawWrappedTextLimited(damageText.rangeLine, x, y, width, formulaSize, formulaText,
                                       textBottom, compact ? 1 : 2) + lineGap;
        }
    }

    std::string formula = stripMechanicPrefix(detail.formula);
    if (!formula.empty() && formula != detail.headline && y < textBottom) {
        int formulaLines = compact ? 1 : 1;
        y = drawWrappedTextLimited(formula, x, y, width, formulaSize, formulaText,
                                   textBottom, formulaLines) + lineGap;
    }

    if (!detail.save.empty() && y < textBottom) {
        drawText(fitText(detail.save, width, formulaSize), x, y, formulaSize, formulaText);
        y += formulaSize * kFontScale + lineGap;
    }

    if (!detail.body.empty() && y < textBottom) {
        float lineHeight = bodySize * kFontScale + 5.0f;
        int bodyLines = std::max(1, static_cast<int>((textBottom - y) / lineHeight));
        bodyLines = std::min(bodyLines, compact ? 2 : 3);
        drawWrappedTextLimited(detail.body, x, y, width, bodySize, bodyText, textBottom, bodyLines);
    }

    if (footerY > y + 2.0f) {
        DrawLineEx({x, footerY - 4.0f}, {x + width, footerY - 4.0f}, 1.0f,
                   Color{132, 101, 64, 105});
        float footerSize = compact ? 15.0f : 16.0f;
        drawText(fitText(detail.recharge, width, footerSize),
                 x, footerY + 2.0f, footerSize, muted);
        drawText(fitText(detail.range, width, footerSize),
                 x, footerY + 23.0f, footerSize, muted);
    }
}

void drawAbilityCompactList(const UnitSpec& spec,
                            const std::vector<AbilityKind>& abilities,
                            float x,
                            float y,
                            float width,
                            float bottomY) {
    if (abilities.empty() || bottomY - y < 52.0f) return;

    DrawLineEx({x, y}, {x + width, y}, 1.0f, Color{132, 101, 64, 125});
    y += 10.0f;

    float gap = 7.0f;
    float rowH = (bottomY - y - gap * static_cast<float>(abilities.size() - 1)) /
                 static_cast<float>(abilities.size());
    rowH = std::max(48.0f, std::min(76.0f, rowH));

    for (AbilityKind ability : abilities) {
        if (y + 42.0f > bottomY) break;

        AbilityDetail detail = abilityDetailFor(spec, ability);
        float iconSize = std::min(42.0f, std::max(30.0f, rowH - 12.0f));
        Rectangle icon{x, y + 3.0f, iconSize, iconSize};
        drawAbilitySigil(icon, ability);

        float textX = x + iconSize + 12.0f;
        float textW = width - iconSize - 12.0f;
        drawTextStrong(fitTextStrong(detail.title, textW, 22.0f), textX, y, 22.0f, kParchmentInk);

        DamageTextParts damageText = splitDamageText(detail.headline);
        std::string line = damageText.rangeLine;
        if (!damageText.formulaLine.empty()) {
            line += " | " + damageText.formulaLine;
        }
        std::string mechanic = stripMechanicPrefix(detail.formula);
        if (!mechanic.empty() && mechanic != detail.headline) {
            line += line.empty() ? mechanic : " | " + mechanic;
        }
        if (!detail.save.empty() && detail.save != "No Save") {
            line += line.empty() ? detail.save : " | " + detail.save;
        }
        if (line.empty()) line = detail.body;
        drawText(fitText(line, textW, 17.0f), textX, y + 27.0f, 17.0f,
                 Color{76, 47, 32, 255});

        if (rowH >= 68.0f && !detail.body.empty()) {
            drawText(fitText(detail.body, textW, 16.0f), textX, y + 49.0f, 16.0f,
                     Color{94, 69, 48, 255});
        }
        y += rowH + gap;
    }
}

void drawAbilityDetailsInline(const UnitSpec& spec, float x, float y, float width, float bottomY) {
    std::vector<AbilityKind> abilities = displayedAbilitiesFor(spec);
    if (abilities.size() <= 1) {
        drawAbilityDetailsInline(spec, spec.ability, x, y, width, bottomY);
        return;
    }

    float gap = 10.0f;
    float sectionH = (bottomY - y - gap * static_cast<float>(abilities.size() - 1)) /
                     static_cast<float>(abilities.size());
    if (sectionH < 118.0f) {
        drawAbilityCompactList(spec, abilities, x, y, width, bottomY);
        return;
    }
    for (size_t i = 0; i < abilities.size(); ++i) {
        float top = y + static_cast<float>(i) * (sectionH + gap);
        drawAbilityDetailsInline(spec, abilities[i], x, top, width, top + sectionH);
    }
}

struct DetailStat {
    std::string label;
    std::string value;
};

float detailLineHeight(float size) {
    return size * kFontScale + 5.0f;
}

bool detailVisible(Rectangle viewport, float y, float h) {
    return y + h >= viewport.y - 12.0f && y <= viewport.y + viewport.height + 12.0f;
}

void drawDetailStatChip(Rectangle rect,
                        const std::string& label,
                        const std::string& value,
                        bool prominent = false) {
    Color fill = prominent ? Color{86, 60, 38, 92} : Color{93, 71, 48, 54};
    Color line = prominent ? Color{150, 104, 57, 175} : Color{126, 93, 55, 120};
    DrawRectangleRounded(rect, 0.10f, 5, fill);
    DrawRectangleRoundedLines(rect, 0.10f, 5, 1.0f, line);
    drawText(fitText(label, rect.width - 12.0f, 13.0f),
             rect.x + 7.0f, rect.y + 5.0f, 13.0f, Color{98, 71, 48, 255});
    float valueSize = prominent ? 19.0f : 18.0f;
    while (valueSize > 14.0f && measureTextStrong(value, valueSize).x > rect.width - 12.0f) {
        valueSize -= 1.0f;
    }
    drawTextStrong(fitTextStrong(value, rect.width - 12.0f, valueSize),
                   rect.x + 7.0f, rect.y + 20.0f, valueSize, kParchmentInk);
}

std::string trimDetailText(const std::string& text) {
    auto first = std::find_if_not(text.begin(), text.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    if (first == text.end()) return "";
    auto last = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();
    return std::string(first, last);
}

std::string numberAfter(const std::string& text, const std::string& marker) {
    size_t pos = text.find(marker);
    if (pos == std::string::npos) return "";
    pos += marker.size();
    while (pos < text.size() && !std::isdigit(static_cast<unsigned char>(text[pos]))) ++pos;
    size_t start = pos;
    while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos]))) ++pos;
    return start < pos ? text.substr(start, pos - start) : "";
}

std::string compactCheckText(const std::string& text) {
    std::string value = trimDetailText(text);
    if (value.find("No Save") != std::string::npos) return "No Save";
    constexpr const char* kAttackRoll = "Attack Roll ";
    if (value.rfind(kAttackRoll, 0) == 0) {
        return "Attack " + value.substr(std::string(kAttackRoll).size());
    }

    std::string dc = numberAfter(value, "DC ");
    if (!dc.empty() && value.find("WIS avoids") != std::string::npos &&
        value.find("CON avoids") != std::string::npos) {
        return "WIS/CON DC " + dc;
    }

    size_t savePos = value.find(" Save DC ");
    if (savePos != std::string::npos && !dc.empty()) {
        std::string ability = trimDetailText(value.substr(0, savePos));
        size_t semi = ability.find_last_of(';');
        if (semi != std::string::npos) ability = trimDetailText(ability.substr(semi + 1));
        size_t space = ability.find_last_of(' ');
        if (space != std::string::npos) ability = trimDetailText(ability.substr(space + 1));
        return ability + " DC " + dc;
    }

    return value;
}

std::string compactTimingText(const std::string& text) {
    std::string value = trimDetailText(text);
    if (value == "Refreshes after movement") return "After move";
    if (value == "Once per combat" || value == "Once per lifetime" || value == "Once per game") return "Once";
    if (value == "Every attack") return "Every hit";
    if (value == "Always on") return "Always";
    if (value == "Boss reaction spell") return "Reaction";
    if (value.rfind("Recharge ", 0) == 0) return "Rech " + value.substr(9);
    if (value.rfind("Poisoned:", 0) == 0) return "Poison" + value.substr(9);
    return value;
}

std::string compactReachText(const std::string& text) {
    std::string value = trimDetailText(text);
    if (value.empty()) return value;
    if (value.rfind("Melee; Stunned", 0) == 0) return "Melee Stunned";
    if (value.rfind("Melee", 0) == 0) return "Melee";
    if (value.rfind("Self", 0) == 0) return "Self";
    if (value.rfind("Reaction", 0) == 0) return "Reaction";
    if (value.rfind("Teleport", 0) == 0) return "Teleport";

    std::string line = numberAfter(value, "Line ");
    if (!line.empty()) return "Line " + line;
    std::string heal = numberAfter(value, "Heal range ");
    if (!heal.empty()) return "Heal " + heal;
    std::string corpse = numberAfter(value, "Corpse search range ");
    if (!corpse.empty()) return "Corpse " + corpse;
    std::string summon = numberAfter(value, "range ");
    if (value.find("Summon near caster") != std::string::npos && !summon.empty()) {
        return "Summon " + summon;
    }

    std::string range = numberAfter(value, "Range ");
    if (!range.empty()) {
        std::string prefix = "R" + range;
        std::string burst = numberAfter(value, "burst radius ");
        if (!burst.empty()) return prefix + " Burst" + burst;
        std::string chain = numberAfter(value, "chain radius ");
        if (!chain.empty()) return prefix + " Chain" + chain;
        std::string cluster = numberAfter(value, "cluster radius ");
        if (!cluster.empty()) return prefix + " Cluster" + cluster;
        if (value.find("humanoid only") != std::string::npos) return prefix + " Humanoid";
        size_t targets = value.find("targets ");
        if (targets == std::string::npos) targets = value.find("Targets ");
        if (targets != std::string::npos) {
            std::string target = trimDetailText(value.substr(targets + 8));
            return target.empty() ? prefix : prefix + " " + target;
        }
        return prefix;
    }

    return value;
}

class DetailCursor {
public:
    DetailCursor(Rectangle viewport, float scroll, bool draw)
        : viewport_(viewport),
          x_(viewport.x + 16.0f),
          width_(viewport.width - 42.0f),
          y_(viewport.y + 14.0f - scroll),
          startY_(viewport.y + 14.0f - scroll),
          draw_(draw) {}

    float contentHeight() const {
        return std::max(0.0f, y_ - startY_ + 18.0f);
    }

    void sectionTitle(const std::string& title) {
        float h = 30.0f;
        if (draw_ && detailVisible(viewport_, y_, h)) {
            drawTextStrong(title, x_, y_, 21.0f, kParchmentInk);
            DrawLineEx({x_ + 122.0f, y_ + 15.0f},
                       {x_ + width_, y_ + 15.0f},
                       1.0f,
                       Color{132, 101, 64, 120});
        }
        y_ += h;
    }

    void divider(float gap = 10.0f) {
        if (draw_ && detailVisible(viewport_, y_, 8.0f)) {
            DrawLineEx({x_, y_}, {x_ + width_, y_}, 1.0f, Color{132, 101, 64, 90});
        }
        y_ += gap;
    }

    void wrapped(const std::string& text,
                 float size,
                 Color color,
                 int maxLines = 0,
                 bool strong = false) {
        std::vector<std::string> lines = wrapTextLines(text, width_, size, maxLines);
        float h = detailLineHeight(size);
        for (const std::string& line : lines) {
            if (draw_ && detailVisible(viewport_, y_, h)) {
                if (strong) {
                    drawTextStrong(fitTextStrong(line, width_, size), x_, y_, size, color);
                } else {
                    drawText(fitText(line, width_, size), x_, y_, size, color);
                }
            }
            y_ += h;
        }
    }

    void statGrid(const std::vector<DetailStat>& stats, int columns = 3) {
        if (stats.empty()) return;
        columns = std::max(1, std::min(columns, 3));
        const float gap = 8.0f;
        const float rowH = 42.0f;
        float colW = (width_ - gap * static_cast<float>(columns - 1)) / static_cast<float>(columns);
        for (size_t i = 0; i < stats.size(); i += static_cast<size_t>(columns)) {
            for (int col = 0; col < columns; ++col) {
                size_t index = i + static_cast<size_t>(col);
                if (index >= stats.size()) break;
                Rectangle rect{x_ + static_cast<float>(col) * (colW + gap), y_, colW, rowH};
                if (draw_ && detailVisible(viewport_, rect.y, rect.height)) {
                    drawDetailStatChip(rect, stats[index].label, stats[index].value);
                }
            }
            y_ += rowH + 8.0f;
        }
    }

    void abilityBlock(const UnitSpec& spec, AbilityKind ability) {
        AbilityDetail detail = abilityDetailFor(spec, ability);
        Color accent = abilitySchoolColor(ability);
        divider(12.0f);

        float iconSize = 42.0f;
        Rectangle icon{x_, y_ + 2.0f, iconSize, iconSize};
        float textX = x_ + iconSize + 12.0f;
        float textW = width_ - iconSize - 12.0f;
        if (draw_ && detailVisible(viewport_, y_, 48.0f)) {
            drawAbilitySigil(icon, ability);
            drawTextStrong(fitTextStrong(detail.title, textW, 22.0f),
                           textX, y_, 22.0f, kParchmentInk);
            drawText(fitText(abilitySchoolLabel(ability), textW, 15.0f),
                     textX, y_ + 27.0f, 15.0f, Color{98, 71, 48, 255});
            DrawLineEx({textX, y_ + 47.0f}, {x_ + width_, y_ + 47.0f}, 2.0f,
                       Color{accent.r, accent.g, accent.b, 125});
        }
        y_ += 56.0f;

        DamageTextParts damageText = splitDamageText(detail.headline);
        std::string headline = damageText.formulaLine.empty()
                                   ? damageText.rangeLine
                                   : damageText.formulaLine;
        if (!headline.empty()) wrapped(headline, 19.0f, Color{52, 38, 29, 255}, 0, true);
        if (!damageText.formulaLine.empty() && damageText.rangeLine != headline) {
            wrapped(damageText.rangeLine, 16.0f, Color{88, 48, 32, 255});
        }

        std::string mechanic = stripMechanicPrefix(detail.formula);
        if (!mechanic.empty() && mechanic != detail.headline) {
            wrapped(mechanic, 16.0f, Color{88, 48, 32, 255});
        }
        statGrid({{"Check", compactCheckText(detail.save.empty() ? "No Save" : detail.save)},
                  {"Timing", compactTimingText(detail.recharge)},
                  {"Reach", compactReachText(detail.range)}}, 3);
        if (!detail.body.empty()) wrapped(detail.body, 16.0f, Color{64, 46, 34, 255});
        y_ += 8.0f;
    }

private:
    Rectangle viewport_;
    float x_ = 0.0f;
    float width_ = 0.0f;
    float y_ = 0.0f;
    float startY_ = 0.0f;
    bool draw_ = false;
};

std::vector<DetailStat> abilityScoreStats(const AbilityScores& scores) {
    return {{"STR", std::to_string(scores.strength)},
            {"DEX", std::to_string(scores.dexterity)},
            {"CON", std::to_string(scores.constitution)},
            {"INT", std::to_string(scores.intelligence)},
            {"WIS", std::to_string(scores.wisdom)},
            {"CHA", std::to_string(scores.charisma)}};
}

std::vector<AbilityKind> detailAbilityListFor(const UnitSpec& spec) {
    std::vector<AbilityKind> abilities{AbilityKind::None};
    for (AbilityKind ability : displayedAbilitiesFor(spec)) {
        if (ability != AbilityKind::None &&
            std::find(abilities.begin(), abilities.end(), ability) == abilities.end()) {
            abilities.push_back(ability);
        }
    }
    return abilities;
}

float detailMaxScroll(float contentHeight, Rectangle viewport) {
    return std::max(0.0f, contentHeight - viewport.height);
}

void drawDetailScrollbar(Rectangle viewport, float contentHeight, float scroll) {
    float maxScroll = detailMaxScroll(contentHeight, viewport);
    if (maxScroll <= 1.0f) return;
    float trackH = viewport.height - 10.0f;
    float barHeight = std::max(36.0f, trackH * viewport.height / std::max(contentHeight, viewport.height));
    float barY = viewport.y + 5.0f + (trackH - barHeight) * (scroll / maxScroll);
    Rectangle track{viewport.x + viewport.width - 8.0f, viewport.y + 5.0f, 4.0f, trackH};
    Rectangle thumb{track.x - 1.0f, barY, 6.0f, barHeight};
    DrawRectangleRounded(track, 1.0f, 4, Color{120, 92, 58, 82});
    DrawRectangleRounded(thumb, 1.0f, 4, Color{130, 91, 48, 205});
}

void syncDetailPanelState(DetailPanelState& state, const UnitSpec* spec, const UnitView* view) {
    if (!spec) return;
    UnitId unitId = view ? view->id : kInvalidUnitId;
    UnitType type = spec->type;
    bool shopMode = view == nullptr;
    if (state.unitId != unitId || state.type != type || state.shopMode != shopMode) {
        state.scroll = 0.0f;
        state.unitId = unitId;
        state.type = type;
        state.shopMode = shopMode;
    }
}

void drawUnitDetailHeader(const UnitSpec& spec, const UnitView* view, Rectangle panel) {
    std::vector<AbilityKind> headerAbilities = displayedAbilitiesFor(spec);

    int units = view ? view->units : spec.unitCount;
    int maxUnits = view ? view->maxUnits : spec.unitCount;
    int totalHp = view ? view->totalHp : spec.maxHp * spec.unitCount;
    int maxTotalHp = view ? view->maxTotalHp : spec.maxHp * spec.unitCount;
    int range = view ? view->range : spec.range;
    int armorClass = view ? view->armorClass : spec.armorClass;
    int attackBonus = view ? view->attackBonus : spec.attackBonus;

    Rectangle iconArea{panel.x + 16.0f, panel.y + 17.0f, 66.0f, 66.0f};
    drawUnitGlyph(spec.type, iconArea, iconAccent(spec.type));
    drawAbilitySigil({panel.x + 29.0f, panel.y + 94.0f, 42.0f, 42.0f}, spec.ability);

    Rectangle schoolChip{panel.x + panel.width - 156.0f, panel.y + 13.0f, 134.0f, 32.0f};
    DrawRectangleRounded(schoolChip, 0.14f, 6, Color{67, 49, 37, 232});
    DrawRectangleRoundedLines(schoolChip, 0.14f, 6, 1.4f, abilitySchoolColor(spec.ability));
    std::string chipLabel = headerAbilities.size() > 1
                                ? TextFormat("%d Skills", static_cast<int>(headerAbilities.size()))
                                : abilitySchoolLabel(spec.ability);
    drawTextCentered(fitText(chipLabel, schoolChip.width - 14.0f, 16.0f),
                     schoolChip, 16.0f, kInk);

    float x = panel.x + 104.0f;
    float contentW = panel.width - 126.0f;
    float titleW = schoolChip.x - x - 10.0f;
    std::string title = spec.name;
    if (view) title += TextFormat("  #%d", view->id);
    drawTextStrong(fitTextStrong(title, titleW, 25.0f), x, panel.y + 15.0f, 25.0f, kParchmentInk);

    Rectangle hpBar{x, panel.y + 58.0f, contentW, 26.0f};
    DrawRectangleRounded(hpBar, 0.12f, 6, Color{75, 49, 34, 205});
    float hpRatio = maxTotalHp > 0 ? std::clamp(static_cast<float>(totalHp) /
                                                    static_cast<float>(maxTotalHp),
                                                0.0f,
                                                1.0f)
                                   : 0.0f;
    Rectangle hpFill{hpBar.x + 3.0f, hpBar.y + 3.0f,
                     (hpBar.width - 6.0f) * hpRatio, hpBar.height - 6.0f};
    DrawRectangleRounded(hpFill, 0.10f, 6,
                         hpRatio > 0.45f ? Color{58, 135, 103, 230}
                                          : Color{181, 77, 65, 230});
    DrawRectangleRoundedLines(hpBar, 0.12f, 6, 1.2f, Color{118, 82, 45, 230});
    drawTextCentered(TextFormat("HP %d/%d   Models %d/%d",
                                totalHp,
                                maxTotalHp,
                                units,
                                maxUnits),
                     hpBar, 17.0f, kInk);

    float chipY = panel.y + 94.0f;
    float gap = 8.0f;
    float chipW = (contentW - gap * 2.0f) / 3.0f;
    drawDetailStatChip({x, chipY, chipW, 38.0f}, "AC", std::to_string(armorClass), true);
    drawDetailStatChip({x + chipW + gap, chipY, chipW, 38.0f},
                       "Hit", TextFormat("%+d", attackBonus), true);
    drawDetailStatChip({x + (chipW + gap) * 2.0f, chipY, chipW, 38.0f},
                       "Range", std::to_string(range), true);

    DrawLineEx({panel.x + 14.0f, panel.y + 140.0f},
               {panel.x + panel.width - 14.0f, panel.y + 140.0f},
               1.0f,
               Color{132, 101, 64, 120});

}

float drawUnitDetailBody(const UnitSpec& spec,
                         const UnitView* view,
                         Rectangle viewport,
                         float scroll,
                         bool draw) {
    DetailCursor cursor(viewport, scroll, draw);

    const UnitProfile& profileData = view ? view->profile : spec.profile;
    std::string dossierLine = view ? view->profileSummary : profileSummary(spec.profile);
    int cost = view ? view->cost : spec.cost;
    int units = view ? view->units : spec.unitCount;
    int maxUnits = view ? view->maxUnits : spec.unitCount;
    int maxTotalHp = view ? view->maxTotalHp : spec.maxHp * spec.unitCount;
    int hpEach = maxUnits > 0 ? maxTotalHp / maxUnits : spec.maxHp;
    int attack = view ? view->attack : spec.attack;
    int range = view ? view->range : spec.range;
    int armorClass = view ? view->armorClass : spec.armorClass;
    int attackBonus = view ? view->attackBonus : spec.attackBonus;
    int savingThrowBonus = view ? view->savingThrowBonus : spec.savingThrowBonus;
    int spellSaveDc = view ? view->spellSaveDc : spec.spellSaveDc;
    UnitLayer layer = view ? view->layer : spec.layer;
    bool neutral = view ? (isNeutralMonster(view->type) || view->neutralControlled)
                        : isNeutralMonster(spec.type);

    UnitSpec damageSpec = spec;
    damageSpec.attack = attack;
    damageSpec.unitCount = std::max(1, units);
    DamagePacket damage = basicDamagePacketFor(damageSpec);

    cursor.sectionTitle("Profile");
    if (!dossierLine.empty()) cursor.wrapped(dossierLine, 17.0f, Color{88, 59, 40, 255});
    std::string profileLine = unitProfileLine(spec,
                                              neutral,
                                              view ? view->neutralActivated : false,
                                              view ? view->neutralReturningHome : false);
    if (!neutral && cost > 0) profileLine = TextFormat("Cost %d. ", cost) + profileLine;
    cursor.wrapped(profileLine, 16.0f, kParchmentMuted);
    cursor.statGrid({{"Layer", toString(layer)},
                     {"School", abilitySchoolLabel(spec.ability)},
                     {"Targets", targetText(spec)}}, 3);

    cursor.sectionTitle("Combat");
    std::string formulaLine = damageFormula(damage);
    if (damageSpec.unitCount > 1) formulaLine += TextFormat(" x%d", damageSpec.unitCount);
    cursor.wrapped(formulaLine, 20.0f, kParchmentInk, 0, true);
    std::string damageLine = "Damage " + damageRange(damage);
    if (damageSpec.unitCount > 1) {
        damageLine += TextFormat(" each. Volley %d-%d.",
                                 damagePacketMin(damage) * damageSpec.unitCount,
                                 damagePacketMax(damage) * damageSpec.unitCount);
    }
    cursor.wrapped(damageLine, 16.0f, Color{88, 48, 32, 255});
    cursor.statGrid({{"Attack", std::to_string(attack)},
                     {"Hit", TextFormat("%+d", attackBonus)},
                     {"AC", std::to_string(armorClass)},
                     {"Save", TextFormat("%+d", savingThrowBonus)},
                     {"DC", std::to_string(spellSaveDc)},
                     {"Range", std::to_string(range)},
                     {"Move", TextFormat("%.1f", spec.speed)},
                     {"Models", TextFormat("%d/%d", units, maxUnits)},
                     {"HP each", std::to_string(hpEach)}}, 3);

    cursor.sectionTitle("Attributes");
    cursor.statGrid(abilityScoreStats(profileData.abilityScores), 3);

    cursor.sectionTitle("Defenses");
    cursor.wrapped(damageAffinitySummary(spec.type), 16.0f, Color{79, 61, 43, 255});
    if (view && view->shield > 0) {
        cursor.statGrid({{"Shield", std::to_string(view->shield)}}, 3);
    }
    if (view && view->hp.size() > 1) {
        std::string hpLine = "Member HP ";
        for (size_t i = 0; i < view->hp.size(); ++i) {
            if (i > 0) hpLine += " | ";
            hpLine += std::to_string(view->hp[i]) + "/" + std::to_string(hpEach);
        }
        cursor.wrapped(hpLine, 16.0f, Color{79, 61, 43, 255});
    }

    cursor.sectionTitle("Abilities");
    for (AbilityKind ability : detailAbilityListFor(spec)) {
        cursor.abilityBlock(spec, ability);
    }

    return cursor.contentHeight();
}

float drawUnitDetails(const UnitSpec& spec,
                      const UnitView* view,
                      Rectangle panel,
                      float scroll) {
    drawParchmentPanel(panel, kParchment);
    constexpr float headerH = 150.0f;
    drawUnitDetailHeader(spec, view, panel);

    Rectangle viewport{panel.x + 10.0f,
                       panel.y + headerH,
                       panel.width - 20.0f,
                       panel.height - headerH - 12.0f};
    float contentHeight = drawUnitDetailBody(spec, view, viewport, 0.0f, false);
    float maxScroll = detailMaxScroll(contentHeight, viewport);
    float drawScroll = std::clamp(scroll, 0.0f, maxScroll);

    BeginScissorMode(static_cast<int>(viewport.x),
                     static_cast<int>(viewport.y),
                     static_cast<int>(viewport.width),
                     static_cast<int>(viewport.height));
    drawUnitDetailBody(spec, view, viewport, drawScroll, true);
    EndScissorMode();

    if (drawScroll > 1.0f) {
        DrawRectangleGradientV(static_cast<int>(viewport.x),
                               static_cast<int>(viewport.y),
                               static_cast<int>(viewport.width - 12.0f),
                               18,
                               Color{220, 202, 163, 230},
                               Color{220, 202, 163, 0});
    }
    if (maxScroll - drawScroll > 1.0f) {
        DrawRectangleGradientV(static_cast<int>(viewport.x),
                               static_cast<int>(viewport.y + viewport.height - 18.0f),
                               static_cast<int>(viewport.width - 12.0f),
                               18,
                               Color{220, 202, 163, 0},
                               Color{220, 202, 163, 230});
    }
    drawDetailScrollbar(viewport, contentHeight, drawScroll);
    return maxScroll;
}

void drawRelicsPanel(const std::vector<std::string>& relicIds, const RunModifiers& modifiers) {
    Rectangle panel = relicPanelRect();
    drawPanelFrame(panel, Color{31, 22, 23, 245});
    drawTextStrong("Relics", panel.x + 14.0f, panel.y + 10.0f, 28.0f, kInk);
    drawText(TextFormat("%d owned", static_cast<int>(relicIds.size())),
             panel.x + panel.width - 142.0f, panel.y + 14.0f, 20.0f, kMutedInk);

    if (relicIds.empty()) {
        Rectangle empty{panel.x + 14.0f, panel.y + 50.0f, panel.width - 28.0f, 52.0f};
        DrawRectangleRounded(empty, 0.12f, 6, Color{47, 32, 29, 230});
        DrawRectangleRoundedLines(empty, 0.12f, 6, 1.0f, Color{126, 91, 52, 190});
        drawTextCentered("No relics yet", empty, 20.0f, kMutedInk);
    } else {
        int shown = std::min<int>(8, relicIds.size());
        int offset = static_cast<int>(relicIds.size()) - shown;
        int columns = panel.width >= 1200.0f ? std::min(4, shown) : 2;
        columns = std::max(1, std::min(columns, shown));
        int rows = (shown + columns - 1) / columns;
        float cardGapX = panel.width >= 1200.0f ? 10.0f : 8.0f;
        float cardGapY = 10.0f;
        float cardW = (panel.width - 28.0f - cardGapX * static_cast<float>(columns - 1)) /
                      static_cast<float>(columns);
        float availableH = std::max(82.0f, panel.height - 96.0f);
        float cardH = std::clamp((availableH - cardGapY * static_cast<float>(rows - 1)) /
                                     static_cast<float>(rows),
                                 64.0f,
                                 92.0f);
        for (int i = 0; i < shown; ++i) {
            int relicIndex = offset + i;
            const RelicSpec* relic = relicById(relicIds[static_cast<size_t>(relicIndex)]);
            int col = i % columns;
            int row = i / columns;
            Rectangle card{panel.x + 14.0f + col * (cardW + cardGapX),
                           panel.y + 48.0f + row * (cardH + cardGapY),
                           cardW,
                           cardH};
            DrawRectangleRounded(card, 0.08f, 6, Color{68, 44, 30, 230});
            DrawRectangleRoundedLines(card, 0.08f, 6, 1.0f,
                                      relic ? relicTierColor(relic->tier)
                                             : kGold);
            if (relic) {
                float iconSize = std::min(48.0f, std::max(34.0f, card.height - 14.0f));
                Rectangle icon{card.x + 8.0f, card.y + 8.0f, iconSize, iconSize};
                drawRelicIcon(*relic, icon);
                float textX = icon.x + icon.width + 10.0f;
                float textW = card.x + card.width - textX - 10.0f;
                drawTextStrong(fitTextStrong(relic->name, textW, 21.0f), textX, card.y + 8.0f, 21.0f, kInk);
                drawWrappedTextLimited(relic->summary, textX, card.y + 36.0f, textW, 18.0f,
                                       kMutedInk, card.y + card.height - 8.0f, card.height > 78.0f ? 2 : 1);
            } else {
                drawTextCentered(relicIds[static_cast<size_t>(relicIndex)], card, 20.0f, kInk);
            }
        }
    }

    drawText(TextFormat("Discount %d  Income +%d  Interest +%d  Roster +%d  Draft +%d  Summon +%d",
                        modifiers.costDiscount,
                        modifiers.roundIncomeBonus,
                        modifiers.interestBonus,
                        modifiers.benchBonus,
                        modifiers.extraRelicChoices,
                        modifiers.summonLimitBonus),
             panel.x + 14.0f, panel.y + panel.height - 36.0f, 20.0f, kMutedInk);
}

void drawBattleLogPanel(const std::vector<std::string>& log) {
    Rectangle panel = battleLogRect();
    drawPanelFrame(panel, Color{31, 21, 22, 245});
    drawTextStrong("Log", panel.x + 14.0f, panel.y + 10.0f, 24.0f, kInk);
    float y = panel.y + 42.0f;
    int start = std::max(0, static_cast<int>(log.size()) - kBattleLogVisibleLines);
    for (int i = start; i < static_cast<int>(log.size()); ++i) {
        drawText(fitText(log[static_cast<size_t>(i)], panel.width - 28.0f, 17.0f),
                 panel.x + 14.0f, y, 17.0f, kMutedInk);
        y += 19.0f;
    }
}

void drawUnit(const UnitView& unit, Rectangle rect, bool ghost = false) {
    Color fill = unitFill(unit);
    if (ghost) fill.a = 150;
    Vector2 center{rect.x + rect.width / 2.0f, rect.y + rect.height / 2.0f};
    if (rect.width < 12.0f || rect.height < 12.0f) {
        float radius = std::max(2.0f, std::min(rect.width, rect.height) * 0.35f);
        DrawCircleV(center, radius, fill);
        DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y), radius,
                        Color{31, 47, 75, fill.a});
        return;
    }
    float pad = std::max(1.0f, rect.width * 0.035f);
    Color accent = iconAccent(unit.type);
    bool neutral = isNeutralMonster(unit.type) || unit.neutralControlled;
    if (neutral) {
        DrawRectangleRounded(rect, 0.10f, 6, Color{43, 34, 31, static_cast<unsigned char>(ghost ? 150 : 230)});
        DrawRectangleRoundedLines(rect, 0.10f, 6, 1.6f,
                                  Color{186, 147, 84, static_cast<unsigned char>(ghost ? 150 : 235)});
    } else {
        drawFactionBands(rect, unit.owner, ghost);
    }

    if (unit.layer == UnitLayer::Air) {
        DrawCircleV(center, rect.width * 0.31f, fill);
        DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y),
                        rect.width * 0.31f, Color{31, 47, 75, 255});
    } else {
        Rectangle body{rect.x + pad, rect.y + pad, rect.width - pad * 2.0f, rect.height - pad * 2.0f};
        DrawRectangleRounded(body, 0.12f, 6, fill);
        DrawRectangleRoundedLines(body, 0.12f, 6, 1.5f,
                                  neutral ? Color{216, 171, 92, static_cast<unsigned char>(ghost ? 160 : 235)}
                                          : (ghost ? Color{44, 49, 57, 190} : factionTrim(unit.owner)));
    }

    Rectangle iconRect{rect.x + pad, rect.y + pad,
                       rect.width - pad * 2.0f, rect.height - pad * 2.6f};
    drawUnitGlyph(unit.type, iconRect, accent);
    if (unit.slowed) {
        Color slow{118, 215, 238, static_cast<unsigned char>(ghost ? 120 : 210)};
        DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y),
                        rect.width * 0.41f, slow);
        DrawLineEx({center.x - rect.width * 0.20f, center.y},
                   {center.x + rect.width * 0.20f, center.y},
                   2.0f, slow);
    }
    float hpRatio = unit.maxTotalHp > 0 ? static_cast<float>(unit.totalHp) / unit.maxTotalHp : 0.0f;
    hpRatio = std::clamp(hpRatio, 0.0f, 1.0f);
    float hpHeight = rect.width < 48.0f ? 4.0f : 7.0f;
    Rectangle hpBack{rect.x + pad, rect.y + rect.height - pad, rect.width - pad * 2.0f, hpHeight};
    DrawRectangleRec(hpBack, Color{34, 25, 22, 255});
    DrawRectangleRec({hpBack.x, hpBack.y, hpBack.width * hpRatio, hpBack.height},
                     hpRatio > 0.35f ? Color{84, 176, 100, 255} : Color{202, 72, 62, 255});
    if (unit.shield > 0) {
        DrawRectangleRec({hpBack.x, hpBack.y - hpHeight - 2.0f,
                          hpBack.width * std::min(1.0f, unit.shield / 120.0f), hpHeight},
                         neutral ? Color{196, 159, 92, 255}
                                 : (unit.owner == PlayerId::One ? Color{98, 184, 220, 255}
                                                                 : Color{218, 135, 155, 255}));
    }
    if (neutral) {
        Color neutralLine = unit.neutralReturningHome
                                ? Color{116, 184, 214, static_cast<unsigned char>(ghost ? 150 : 255)}
                            : unit.neutralActivated
                                ? Color{236, 100, 64, static_cast<unsigned char>(ghost ? 160 : 255)}
                                : Color{222, 176, 93, static_cast<unsigned char>(ghost ? 145 : 245)};
        DrawRectangleRoundedLines(rect, 0.10f, 6,
                                  ghost ? 1.5f : ((unit.neutralActivated || unit.neutralReturningHome) ? 3.0f : 2.4f),
                                  neutralLine);
        if (unit.neutralActivated || unit.neutralReturningHome) {
            DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y),
                            rect.width * 0.47f, Color{243, 174, 91, static_cast<unsigned char>(ghost ? 130 : 230)});
        }
        Rectangle tag{rect.x + rect.width - pad - rect.width * 0.24f,
                      rect.y + pad,
                      rect.width * 0.20f,
                      rect.width * 0.18f};
        DrawRectangleRounded(tag, 0.25f, 4, Color{53, 38, 31, static_cast<unsigned char>(ghost ? 130 : 235)});
        drawTextCentered(unit.neutralReturningHome ? "R" : (unit.neutralActivated ? "A" : "N"),
                         tag,
                         std::max(10.0f, rect.width * 0.16f),
                         unit.neutralReturningHome
                             ? Color{160, 220, 240, static_cast<unsigned char>(ghost ? 160 : 255)}
                         : unit.neutralActivated
                             ? Color{255, 180, 116, static_cast<unsigned char>(ghost ? 160 : 255)}
                             : Color{235, 204, 136, static_cast<unsigned char>(ghost ? 150 : 255)});
    } else {
        drawFactionOutline(rect, unit.owner, ghost);
        drawFactionNameplate(rect, unit.owner, ghost);
    }
}

bool cueUsesArcaneTexture(const CombatCue& cue) {
    return cue.visual == CombatCueVisualKind::Spell ||
           cue.visual == CombatCueVisualKind::Heal ||
           cue.visual == CombatCueVisualKind::Gold ||
           cue.visual == CombatCueVisualKind::Shield ||
           cue.arcane ||
           cue.type == EventType::Shielded || cue.type == EventType::Healed ||
           cue.type == EventType::StatusApplied || cue.type == EventType::GoldGained;
}

void drawCombatCues(const std::vector<CombatCue>& cues) {
    for (const CombatCue& cue : cues) {
        if (!coordOnBoard(cue.to)) continue;
        float t = cue.ttl > 0.0f ? std::clamp(cue.age / cue.ttl, 0.0f, 1.0f) : 1.0f;
        float alpha = 1.0f - t;
        Vector2 to = cellCenter(cue.to);
        Color faction = factionPrimary(cue.player);
        faction.a = static_cast<unsigned char>(std::clamp(alpha * 210.0f, 0.0f, 210.0f));

        if (cue.type == EventType::UnitAttacked && coordOnBoard(cue.from) && cue.arcane) {
            Vector2 from = cellCenter(cue.from);
            Color trail = faction;
            trail.a = static_cast<unsigned char>(std::clamp(alpha * 150.0f, 0.0f, 150.0f));
            DrawLineEx(from, to, 2.5f, trail);
        }

        if (cue.type == EventType::UnitMoved && isKnockbackCueText(cue.text)) {
            Vector2 from = coordOnBoard(cue.from) ? cellCenter(cue.from) : to;
            Color shock = knockbackCueColor(cue);
            shock.a = static_cast<unsigned char>(std::clamp(alpha * 220.0f, 0.0f, 220.0f));
            DrawLineEx(from, to, 5.0f, Color{42, 28, 21, static_cast<unsigned char>(std::clamp(alpha * 150.0f, 0.0f, 150.0f))});
            DrawLineEx(from, to, 2.5f, shock);
            DrawCircleLines(static_cast<int>(to.x), static_cast<int>(to.y), 20.0f + 16.0f * t, shock);
            if (cue.ability == AbilityKind::KethericSmite && coordOnBoard(cue.from)) {
                DrawCircleLines(static_cast<int>(from.x), static_cast<int>(from.y),
                                28.0f + 24.0f * t,
                                Color{shock.r, shock.g, shock.b,
                                      static_cast<unsigned char>(std::clamp(alpha * 165.0f, 0.0f, 165.0f))});
            }
            drawTinySpark(to, 10.0f + 8.0f * t, shock);
            continue;
        }

        if (cue.visual == CombatCueVisualKind::ReturnHome && coordOnBoard(cue.from)) {
            Vector2 from = cellCenter(cue.from);
            Color returnColor{116, 184, 214, static_cast<unsigned char>(std::clamp(alpha * 210.0f, 0.0f, 210.0f))};
            DrawLineEx(from, to, 3.5f, returnColor);
            DrawCircleLines(static_cast<int>(to.x), static_cast<int>(to.y), 16.0f + 12.0f * t, returnColor);
            continue;
        }

        if (cue.visual == CombatCueVisualKind::Leap && coordOnBoard(cue.from)) {
            Vector2 from = cellCenter(cue.from);
            Color shadow{52, 32, 58, static_cast<unsigned char>(std::clamp(alpha * 210.0f, 0.0f, 210.0f))};
            DrawLineEx(from, to, 5.0f, Color{12, 9, 12, static_cast<unsigned char>(std::clamp(alpha * 155.0f, 0.0f, 155.0f))});
            DrawLineEx(from, to, 2.0f, shadow);
            DrawCircleLines(static_cast<int>(to.x), static_cast<int>(to.y), 18.0f + 18.0f * t, shadow);
            continue;
        }

        if (cue.visual == CombatCueVisualKind::Arrow && coordOnBoard(cue.from)) {
            Vector2 from = cellCenter(cue.from);
            Vector2 dir{to.x - from.x, to.y - from.y};
            float len = std::max(1.0f, std::sqrt(dir.x * dir.x + dir.y * dir.y));
            dir.x /= len;
            dir.y /= len;
            Vector2 perp{-dir.y, dir.x};
            float progress = std::clamp(t * 1.25f, 0.0f, 1.0f);
            Vector2 head{from.x + (to.x - from.x) * progress,
                         from.y + (to.y - from.y) * progress};
            float arrowLen = std::clamp(len * 0.24f, 24.0f, 46.0f);
            Vector2 tail{head.x - dir.x * arrowLen, head.y - dir.y * arrowLen};
            Color shadow{25, 18, 14, static_cast<unsigned char>(std::clamp(alpha * 92.0f, 0.0f, 92.0f))};
            Color copper{174, 122, 61, static_cast<unsigned char>(std::clamp(alpha * 138.0f, 0.0f, 138.0f))};
            Color gold{232, 184, 92, static_cast<unsigned char>(std::clamp(alpha * 188.0f, 0.0f, 188.0f))};
            Color pale{255, 228, 158, static_cast<unsigned char>(std::clamp(alpha * 210.0f, 0.0f, 210.0f))};

            DrawLineEx(tail, head, 7.0f, shadow);
            DrawLineEx(tail, head, 3.5f, copper);

            Vector2 headLeft{head.x - dir.x * 11.0f + perp.x * 7.0f,
                             head.y - dir.y * 11.0f + perp.y * 7.0f};
            Vector2 headRight{head.x - dir.x * 11.0f - perp.x * 7.0f,
                              head.y - dir.y * 11.0f - perp.y * 7.0f};
            Vector2 notch{head.x - dir.x * 7.0f, head.y - dir.y * 7.0f};
            DrawTriangle(head, headLeft, notch, gold);
            DrawTriangle(head, notch, headRight, gold);
            DrawLineEx(tail, {head.x - dir.x * 8.0f, head.y - dir.y * 8.0f}, 2.0f, pale);
            DrawLineEx({tail.x - perp.x * 5.0f, tail.y - perp.y * 5.0f},
                       {tail.x + perp.x * 5.0f, tail.y + perp.y * 5.0f},
                       2.0f,
                       gold);
            DrawCircleLines(static_cast<int>(head.x), static_cast<int>(head.y),
                            8.0f + 8.0f * t, Color{gold.r, gold.g, gold.b,
                                                    static_cast<unsigned char>(gold.a * 0.55f)});
            continue;
        }

        const Texture2D* texture = cue.ability != AbilityKind::None ? abilityTexture(cue.ability) : nullptr;
        bool useArcaneFallback = !texture && cueUsesArcaneTexture(cue);
        if (!texture && !useArcaneFallback) {
            texture = &gMeleeHitTexture;
        }
        bool hasTexture = texture && texture->id != 0;
        float size = (cueUsesArcaneTexture(cue) ? 92.0f : 76.0f) + 26.0f * t;
        Rectangle rect{to.x - size / 2.0f, to.y - size / 2.0f, size, size};
        Color tint{255, 255, 255, static_cast<unsigned char>(std::clamp(alpha * 235.0f, 0.0f, 235.0f))};
        if (hasTexture) {
            drawTextureAspectFit(*texture, rect, tint);
        } else if (useArcaneFallback) {
            Color glow = arcaneCueColor(cue);
            glow.a = static_cast<unsigned char>(std::clamp(alpha * 210.0f, 0.0f, 210.0f));
            Color soft{glow.r, glow.g, glow.b,
                       static_cast<unsigned char>(std::clamp(alpha * 70.0f, 0.0f, 70.0f))};
            DrawCircleV(to, size * 0.24f + 12.0f * t, soft);
            DrawCircleLines(static_cast<int>(to.x), static_cast<int>(to.y), size * 0.30f, glow);
            DrawCircleLines(static_cast<int>(to.x), static_cast<int>(to.y), size * 0.17f + 10.0f * t, glow);
            drawTinySpark(to, size * 0.18f, glow);
        } else {
            DrawCircleLines(static_cast<int>(to.x), static_cast<int>(to.y), size * 0.32f, faction);
            drawTinySpark(to, size * 0.18f, faction);
        }

        if (cue.amount > 0 && (cue.type == EventType::DamageDealt || cue.type == EventType::Shielded ||
                               cue.type == EventType::Healed || cue.type == EventType::UnitAttacked ||
                               cue.type == EventType::GoldGained)) {
            Color text = cue.type == EventType::Healed ? Color{124, 222, 135, 255}
                         : cue.type == EventType::GoldGained ? Color{239, 190, 83, 255}
                         : cue.type == EventType::Shielded ? Color{119, 198, 230, 255}
                                                          : Color{242, 110, 87, 255};
            text.a = static_cast<unsigned char>(std::clamp(alpha * 255.0f, 0.0f, 255.0f));
            std::string prefix = cue.type == EventType::Healed ? "+" : "-";
            if (cue.type == EventType::Shielded) prefix = "";
            if (cue.type == EventType::GoldGained) prefix = "+";
            drawTextShadowed(prefix + std::to_string(cue.amount),
                             to.x - 18.0f,
                             to.y - 48.0f - 20.0f * t,
                             23.0f,
                             text);
        }
    }
}

void drawBoard(const GameEngine& engine,
               const GameSnapshot& snapshot,
               UnitId dragging,
               const std::vector<CombatCue>& combatCues) {
    Rectangle rect = boardRect();
    drawPanelFrame({rect.x - 14.0f, rect.y - 14.0f, rect.width + 28.0f, rect.height + 28.0f}, kLeatherDark);
    DrawRectangleRec(rect, Color{28, 25, 22, 255});

    Coord hover;
    bool hasHover = mouseToCell(gMousePosition, hover);
    const UnitView* draggingUnit = dragging != kInvalidUnitId ? findUnit(snapshot, dragging) : nullptr;
    std::vector<std::vector<const UnitView*>> unitStacks(
        static_cast<size_t>(std::max(0, snapshot.width * snapshot.height)));
    for (const UnitView& unit : snapshot.units) {
        if (!unit.alive || !unit.deployed || unit.id == dragging || !coordOnBoard(unit.coord)) continue;
        size_t index = static_cast<size_t>(unit.coord.y * snapshot.width + unit.coord.x);
        if (index < unitStacks.size()) unitStacks[index].push_back(&unit);
    }

    for (int y = 0; y < snapshot.height; ++y) {
        for (int x = 0; x < snapshot.width; ++x) {
            Coord coord{x, y};
            Rectangle cell = cellRect(coord);
            TerrainKind terrain = terrainAt(snapshot, coord);
            bool deployOne = engine.isDeploymentCell(PlayerId::One, coord);
            bool deployTwo = engine.isDeploymentCell(PlayerId::Two, coord);
            bool deployCell = deployOne || deployTwo;
            bool preparation = snapshot.phase == Phase::Preparation;
            Color fill = terrainColor(terrain, coord, snapshot);
            if ((x + y) % 2 == 1 && terrain != TerrainKind::Wall) {
                fill.r = static_cast<unsigned char>(std::max(0, fill.r - 4));
                fill.g = static_cast<unsigned char>(std::max(0, fill.g - 4));
                fill.b = static_cast<unsigned char>(std::max(0, fill.b - 4));
            }
            DrawRectangleRec({cell.x + 1, cell.y + 1, cell.width - 2, cell.height - 2}, fill);
            DrawRectangleLinesEx(cell, 1.0f,
                                 terrain == TerrainKind::Wall ? Color{16, 14, 15, 180}
                                                              : Color{106, 85, 61, 88});
            if (deployCell) {
                Color deployFill = deployOne
                                       ? Color{148, 112, 61, static_cast<unsigned char>(preparation ? 54 : 22)}
                                       : Color{134, 96, 56, static_cast<unsigned char>(preparation ? 48 : 20)};
                DrawRectangleRec({cell.x + 2.0f, cell.y + 2.0f, cell.width - 4.0f, cell.height - 4.0f},
                                 deployFill);
            }
            if (terrain == TerrainKind::Wall) {
                DrawRectangleRec({cell.x + 8.0f, cell.y + 8.0f, cell.width - 16.0f, cell.height - 16.0f},
                                 Color{18, 16, 17, 126});
                DrawLineEx({cell.x + 9.0f, cell.y + cell.height - 10.0f},
                           {cell.x + cell.width - 8.0f, cell.y + 10.0f},
                           1.0f, Color{58, 49, 42, 72});
            } else if (terrain == TerrainKind::SideRoad) {
                DrawCircleLines(static_cast<int>(cell.x + cell.width * 0.5f),
                                static_cast<int>(cell.y + cell.height * 0.5f),
                                cell.width * 0.20f, Color{170, 140, 102, 52});
            } else if (terrain == TerrainKind::NeutralCamp) {
                NeutralFamily family = campFamilyForCell(coord);
                DrawCircleV({cell.x + cell.width / 2.0f, cell.y + cell.height / 2.0f},
                            cell.width * 0.36f, Color{19, 14, 11, 150});
                DrawCircleLines(static_cast<int>(cell.x + cell.width / 2.0f),
                                static_cast<int>(cell.y + cell.height / 2.0f),
                                cell.width * 0.36f,
                                family == NeutralFamily::Swarm
                                    ? Color{156, 151, 118, 138}
                                    : family == NeutralFamily::Guardian
                                          ? Color{138, 151, 160, 138}
                                          : family == NeutralFamily::Caster
                                                ? Color{156, 142, 170, 138}
                                                : family == NeutralFamily::Assassin
                                                      ? Color{168, 128, 137, 138}
                                                      : Color{176, 150, 111, 138});
                Rectangle iconRect{cell.x + 8.0f, cell.y + 8.0f, cell.width - 16.0f, cell.height - 16.0f};
                if (const Texture2D* texture = relicFamilyTexture(family)) {
                    drawTextureAspectFit(*texture, iconRect, WHITE);
                } else {
                    drawTextCentered(neutralFamilyLabel(family).substr(0, 1), iconRect, 22.0f, kInk);
                }
            } else if (terrain == TerrainKind::BossSite) {
                Vector2 c{cell.x + cell.width * 0.5f, cell.y + cell.height * 0.5f};
                DrawCircleV(c, cell.width * 0.39f, Color{33, 18, 14, 166});
                DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y),
                                cell.width * 0.38f, Color{216, 176, 110, 195});
                DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y),
                                cell.width * 0.25f, Color{141, 96, 70, 178});
                DrawTriangle({c.x, c.y - cell.height * 0.20f},
                             {c.x - cell.width * 0.18f, c.y + cell.height * 0.16f},
                             {c.x + cell.width * 0.18f, c.y + cell.height * 0.16f},
                             Color{224, 182, 112, 212});
                DrawCircleV(c, cell.width * 0.08f, Color{41, 24, 19, 225});
            } else if (terrain == TerrainKind::Trap) {
                Vector2 c{cell.x + cell.width * 0.5f, cell.y + cell.height * 0.5f};
                DrawCircleV(c, cell.width * 0.28f, Color{62, 33, 25, 126});
                DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y),
                                cell.width * 0.28f, Color{181, 142, 93, 150});
                DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y),
                                cell.width * 0.16f, Color{121, 83, 63, 170});
                DrawLineEx({c.x - cell.width * 0.16f, c.y - cell.height * 0.06f},
                           {c.x + cell.width * 0.16f, c.y + cell.height * 0.06f},
                           2.2f, Color{217, 175, 107, 160});
                DrawLineEx({c.x - cell.width * 0.12f, c.y + cell.height * 0.12f},
                           {c.x + cell.width * 0.12f, c.y - cell.height * 0.12f},
                           1.8f, Color{124, 92, 68, 160});
            } else if (terrain == TerrainKind::ClearedObjective ||
                       terrain == TerrainKind::ClearedBoss) {
                Vector2 c{cell.x + cell.width * 0.5f, cell.y + cell.height * 0.52f};
                Color stone = terrain == TerrainKind::ClearedBoss
                                  ? Color{88, 78, 72, 226}
                                  : Color{80, 76, 70, 216};
                Rectangle grave{cell.x + cell.width * 0.28f, cell.y + cell.height * 0.18f,
                                cell.width * 0.44f, cell.height * 0.58f};
                DrawRectangleRounded(grave, 0.34f, 10, stone);
                DrawRectangleRoundedLines(grave, 0.34f, 10, 1.5f, Color{194, 168, 122, 138});
                DrawCircleV({c.x - cell.width * 0.08f, c.y - cell.height * 0.04f},
                            cell.width * 0.035f, Color{24, 22, 21, 210});
                DrawCircleV({c.x + cell.width * 0.08f, c.y - cell.height * 0.04f},
                            cell.width * 0.035f, Color{24, 22, 21, 210});
                DrawLineEx({c.x - cell.width * 0.09f, c.y + cell.height * 0.10f},
                           {c.x + cell.width * 0.09f, c.y + cell.height * 0.10f},
                           2.0f, Color{24, 22, 21, 210});
            }
            if (deployCell) {
                auto adjacentDeploy = [&](Coord adjacent) {
                    return engine.isDeploymentCell(PlayerId::One, adjacent) ||
                           engine.isDeploymentCell(PlayerId::Two, adjacent);
                };
                Color edge = deployOne
                                 ? Color{222, 178, 99, static_cast<unsigned char>(preparation ? 142 : 74)}
                                 : Color{204, 149, 87, static_cast<unsigned char>(preparation ? 124 : 66)};
                float edgeWidth = preparation ? 2.2f : 1.2f;
                if (!adjacentDeploy({coord.x - 1, coord.y})) {
                    DrawLineEx({cell.x + 1.5f, cell.y + 3.0f},
                               {cell.x + 1.5f, cell.y + cell.height - 3.0f}, edgeWidth, edge);
                }
                if (!adjacentDeploy({coord.x + 1, coord.y})) {
                    DrawLineEx({cell.x + cell.width - 1.5f, cell.y + 3.0f},
                               {cell.x + cell.width - 1.5f, cell.y + cell.height - 3.0f}, edgeWidth, edge);
                }
                if (!adjacentDeploy({coord.x, coord.y - 1})) {
                    DrawLineEx({cell.x + 3.0f, cell.y + 1.5f},
                               {cell.x + cell.width - 3.0f, cell.y + 1.5f}, edgeWidth, edge);
                }
                if (!adjacentDeploy({coord.x, coord.y + 1})) {
                    DrawLineEx({cell.x + 3.0f, cell.y + cell.height - 1.5f},
                               {cell.x + cell.width - 3.0f, cell.y + cell.height - 1.5f}, edgeWidth, edge);
                }

                if (((coord.x + coord.y) % 2) == 0) {
                    Color rune = Color{225, 178, 95, static_cast<unsigned char>(preparation ? 108 : 52)};
                    float inset = preparation ? 9.0f : 11.0f;
                    float mark = preparation ? 9.0f : 6.0f;
                    DrawLineEx({cell.x + inset, cell.y + inset},
                               {cell.x + inset + mark, cell.y + inset}, 1.2f, rune);
                    DrawLineEx({cell.x + inset, cell.y + inset},
                               {cell.x + inset, cell.y + inset + mark}, 1.2f, rune);
                    DrawLineEx({cell.x + cell.width - inset, cell.y + cell.height - inset},
                               {cell.x + cell.width - inset - mark, cell.y + cell.height - inset}, 1.2f, rune);
                    DrawLineEx({cell.x + cell.width - inset, cell.y + cell.height - inset},
                               {cell.x + cell.width - inset, cell.y + cell.height - inset - mark}, 1.2f, rune);
                }
            }
            if ((x == 0 || x == snapshot.width - 1) && y == snapshot.height / 2) {
                DrawCircleV({cell.x + cell.width / 2.0f, cell.y + cell.height / 2.0f},
                            25.0f, Color{18, 13, 10, 78});
                DrawCircleLines(static_cast<int>(cell.x + cell.width / 2.0f),
                                static_cast<int>(cell.y + cell.height / 2.0f),
                                24.0f, Color{187, 154, 96, 128});
            }

            if (hasHover && hover == coord && draggingUnit && snapshot.phase == Phase::Preparation) {
                bool legal = engine.canDeploy(PlayerId::One, coord, draggingUnit->layer);
                DrawRectangleRec({cell.x + 4, cell.y + 4, cell.width - 8, cell.height - 8},
                                 legal ? Color{208, 166, 91, 46} : Color{128, 61, 49, 36});
                DrawRectangleLinesEx({cell.x + 3, cell.y + 3, cell.width - 6, cell.height - 6},
                                     3.0f, legal ? Color{186, 152, 92, 235} : Color{142, 83, 63, 235});
            }
        }
    }

    for (int y = 0; y < snapshot.height; ++y) {
        for (int x = 0; x < snapshot.width; ++x) {
            Coord coord{x, y};
            size_t index = static_cast<size_t>(coord.y * snapshot.width + coord.x);
            if (index >= unitStacks.size()) continue;
            std::vector<const UnitView*> stack = unitStacks[index];
            if (stack.empty()) continue;

            std::stable_sort(stack.begin(), stack.end(), [](const UnitView* a, const UnitView* b) {
                if (a->layer != b->layer) return a->layer == UnitLayer::Land;
                return a->id < b->id;
            });

            Rectangle cell = cellRect(coord);
            std::vector<const UnitView*> landUnits;
            std::vector<const UnitView*> airUnits;
            for (const UnitView* unit : stack) {
                if (unit->layer == UnitLayer::Air) airUnits.push_back(unit);
                else landUnits.push_back(unit);
            }

            for (int i = 0; i < static_cast<int>(landUnits.size()); ++i) {
                Rectangle inset = stackedUnitRect(cell, i, static_cast<int>(landUnits.size()), UnitLayer::Land);
                Vector2 slide = knockbackDrawOffset(landUnits[static_cast<size_t>(i)]->id, combatCues);
                inset.x += slide.x;
                inset.y += slide.y;
                drawUnit(*landUnits[static_cast<size_t>(i)], inset);
            }
            for (int i = 0; i < static_cast<int>(airUnits.size()); ++i) {
                Rectangle inset = stackedUnitRect(cell, i, static_cast<int>(airUnits.size()), UnitLayer::Air);
                Vector2 slide = knockbackDrawOffset(airUnits[static_cast<size_t>(i)]->id, combatCues);
                inset.x += slide.x;
                inset.y += slide.y;
                Vector2 shadow{inset.x + inset.width * 0.5f, cell.y + cell.height * 0.78f};
                DrawEllipse(static_cast<int>(shadow.x),
                            static_cast<int>(shadow.y),
                            inset.width * 0.28f,
                            inset.height * 0.08f,
                            Color{8, 6, 5, 95});
                drawUnit(*airUnits[static_cast<size_t>(i)], inset);
            }
        }
    }
    drawCombatCues(combatCues);
}

void drawShop(GameEngine& engine, const GameSnapshot& snapshot, float shopScroll) {
    Rectangle viewport = shopViewportRect();
    const std::vector<const UnitSpec*>& specs = orderedShopSpecs(engine);
    drawTextStrong("Unit Shop", kSideX, kShopY - 31.0f, 27.0f, kInk);

    drawPanelFrame(viewport, Color{28, 20, 20, 255});
    BeginScissorMode(static_cast<int>(viewport.x), static_cast<int>(viewport.y),
                     static_cast<int>(viewport.width), static_cast<int>(viewport.height));

    for (int card = 0; card < static_cast<int>(specs.size()); ++card) {
        const UnitSpec& spec = *specs[static_cast<size_t>(card)];
        Rectangle r = shopCardRect(card, shopScroll);
        if (r.y + r.height < viewport.y || r.y > viewport.y + viewport.height) continue;

        bool hover = CheckCollisionPointRec(gMousePosition, r);
        int cost = engine.effectiveBuyCost(PlayerId::One, spec);
        bool affordable = snapshot.players[0].money >= cost;
        Color fill = hover ? Color{221, 203, 165, 255} : Color{201, 181, 143, 255};
        if (!affordable) fill = Color{128, 112, 101, 255};
        drawParchmentPanel(r, fill);
        DrawRectangleRec({r.x + 4.0f, r.y + 4.0f, 10.0f, r.height - 8.0f},
                         affordable ? Color{82, 45, 46, 255} : Color{64, 57, 54, 255});

        Rectangle iconArea{r.x + 14.0f, r.y + 10.0f, 60.0f, 60.0f};
        drawUnitGlyph(spec.type, iconArea, iconAccent(spec.type));
        drawText(std::to_string(card + 1), r.x + 19.0f, r.y + 13.0f, 18.0f, kGold);
        drawTextStrong(fitTextStrong(spec.name, r.width - 246.0f, 24.0f), r.x + 88.0f, r.y + 10.0f,
                       24.0f, kParchmentInk);
        Rectangle schoolChip{r.x + r.width - 232.0f, r.y + 42.0f, 168.0f, 34.0f};
        DrawRectangleRounded(schoolChip, 0.18f, 6, Color{66, 49, 36, 218});
        DrawRectangleRoundedLines(schoolChip, 0.18f, 6, 1.2f, abilitySchoolColor(spec.ability));
        drawTextCenteredStrong(fitTextStrong(abilitySchoolLabel(spec.ability), schoolChip.width - 18.0f, 18.0f),
                               schoolChip, 18.0f, kInk);
        Rectangle costSeal{r.x + r.width - 60.0f, r.y + 10.0f, 48.0f, 32.0f};
        DrawRectangleRounded(costSeal, 0.16f, 8, Color{44, 33, 27, 230});
        DrawRectangleRoundedLines(costSeal, 0.16f, 8, 1.5f, kGold);
        drawTextCentered(TextFormat("%d", cost), costSeal, 22.0f, affordable ? kGold : Color{165, 139, 118, 255});
        std::string stats = TextFormat("x%d  HP %d  %s  R%d",
                                        spec.unitCount,
                                        spec.maxHp,
                                        perHitDamageSummary(spec).c_str(),
                                        spec.range);
        float statWidth = std::max(180.0f, schoolChip.x - (r.x + 88.0f) - 8.0f);
        drawText(fitText(stats, statWidth, 18.0f),
                 r.x + 88.0f, r.y + 50.0f, 18.0f, kParchmentMuted);
    }
    EndScissorMode();

    float contentHeight = specs.empty()
                              ? 0.0f
                              : static_cast<float>(specs.size()) * kShopCardH +
                                    static_cast<float>(specs.size() - 1) * kShopGap;
    if (contentHeight > viewport.height) {
        float barHeight = std::max(36.0f, viewport.height * viewport.height / contentHeight);
        float maxScroll = contentHeight - viewport.height;
        float barY = viewport.y + (viewport.height - barHeight) * (shopScroll / maxScroll);
        Rectangle track{viewport.x + viewport.width - 8.0f, viewport.y + 4.0f, 4.0f, viewport.height - 8.0f};
        Rectangle thumb{track.x - 1.0f, barY, 6.0f, barHeight};
        DrawRectangleRounded(track, 1.0f, 4, Color{35, 42, 52, 255});
        DrawRectangleRounded(thumb, 1.0f, 4, kAccent);
    }
}

void drawBench(const GameSnapshot& snapshot, UnitId dragging, int rosterLimit) {
    Rectangle area = benchRect();
    drawPanelFrame(area, Color{42, 25, 21, 255});
    int deployedRoster = 0;
    for (UnitId id : snapshot.players[0].deployed) {
        const UnitView* unit = findUnit(snapshot, id);
        if (unit && unit->alive && !isInternalUnit(unit->type) && unit->cost > 0) ++deployedRoster;
    }
    int benchCount = static_cast<int>(snapshot.players[0].bench.size());
    int rosterCount = benchCount + deployedRoster;
    int visibleSlots = std::max(10, rosterLimit);
    drawTextStrong("Bench", area.x + 14.0f, area.y + 5.0f, 25.0f, kInk);
    drawText(TextFormat("Roster %d/%d   Bench %d + Deployed %d",
                        rosterCount, rosterLimit, benchCount, deployedRoster),
             area.x + 102.0f, area.y + 10.0f, 17.0f, kMutedInk);

    for (int i = 0; i < visibleSlots; ++i) {
        Rectangle slot = benchSlotRect(i, visibleSlots);
        DrawRectangleRounded(slot, 0.08f, 6, Color{23, 17, 18, 255});
        DrawRectangleRoundedLines(slot, 0.08f, 6, 1.5f, Color{143, 94, 48, 220});
        DrawLineEx({slot.x + 8.0f, slot.y + 8.0f}, {slot.x + slot.width - 8.0f, slot.y + 8.0f},
                   1.0f, Color{245, 200, 109, 70});
        if (i < benchCount) {
            UnitId id = snapshot.players[0].bench[i];
            if (id != dragging) {
                const UnitView* unit = findUnit(snapshot, id);
                if (unit) drawUnit(*unit, {slot.x + 2, slot.y + 2, slot.width - 4, slot.height - 4});
            }
        }
    }
}

void drawHoverTooltip(const UnitSpec* spec,
                      const UnitView* view,
                      Vector2 mouse,
                      std::optional<int> effectiveCost = std::nullopt) {
    if (!spec) return;
    if (mouse.x >= kSideX - 18.0f) return;

    std::vector<std::string> lines;
    lines.push_back(view ? view->name : spec->name);
    lines.push_back(TextFormat("Type %s   Layer %s", toString(spec->type).c_str(), toString(spec->layer).c_str()));
    std::string dossierLine = view ? view->profileSummary : profileSummary(spec->profile);
    if (!dossierLine.empty()) lines.push_back(dossierLine);
    std::vector<AbilityKind> abilities = displayedAbilitiesFor(*spec);
    if (abilities.size() > 1) {
        std::string skillLine = "Skills ";
        for (size_t i = 0; i < abilities.size(); ++i) {
            if (i > 0) skillLine += " | ";
            skillLine += abilityDetailFor(*spec, abilities[i]).title;
        }
        lines.push_back(skillLine);
    } else {
        lines.push_back(TextFormat("School %s", abilitySchoolLabel(spec->ability).c_str()));
    }
    if (view) {
        UnitSpec damageSpec = *spec;
        damageSpec.attack = view->attack;
        damageSpec.unitCount = std::max(1, view->units);
        DamagePacket damage = basicDamagePacketFor(damageSpec);
        lines.push_back(TextFormat("HP %d/%d   Shield %d   AC %d",
                                   view->totalHp, view->maxTotalHp, view->shield, view->armorClass));
        lines.push_back("Damage " + damageRange(damage) + "   " + damageFormula(damage));
        lines.push_back(damageAffinitySummary(view->type));
        lines.push_back(TextFormat("Hit +%d   Rng %d   Save +%d   DC %d",
                                   view->attackBonus, view->range,
                                   view->savingThrowBonus, view->spellSaveDc));
    } else {
        int cost = effectiveCost.value_or(spec->cost);
        DamagePacket damage = basicDamagePacketFor(*spec);
        lines.push_back(TextFormat("Cost %d   HP %d x%d   AC %d",
                                   cost, spec->maxHp, spec->unitCount, spec->armorClass));
        lines.push_back("Damage " + damageRange(damage) + "   " + damageFormula(damage));
        lines.push_back(damageAffinitySummary(spec->type));
        lines.push_back(TextFormat("Hit +%d   Rng %d   Save +%d   DC %d",
                                   spec->attackBonus, spec->range,
                                   spec->savingThrowBonus, spec->spellSaveDc));
    }
    lines.push_back(abilitySummary(*spec));

    float width = 340.0f;
    for (const std::string& line : lines) {
        float lineWidth = measureText(line, 20.0f).x + 30.0f;
        width = std::max(width, std::min(500.0f, lineWidth));
    }
    width = std::clamp(width, 340.0f, 500.0f);

    std::vector<std::string> drawLines;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i == 0) {
            drawLines.push_back(fitTextStrong(lines[i], width - 28.0f, 24.0f));
            continue;
        }
        int maxLines = 2;
        if (i == lines.size() - 1 || lines[i].size() > 72) maxLines = 3;
        std::vector<std::string> wrapped = wrapTextLines(lines[i], width - 28.0f, 20.0f, maxLines);
        for (const std::string& wrappedLine : wrapped) {
            drawLines.push_back(wrappedLine);
        }
    }

    float height = 18.0f;
    height += 29.0f;
    height += 10.0f;
    int bodyCount = std::max(0, static_cast<int>(drawLines.size()) - 1);
    height += static_cast<float>(bodyCount) * (20.0f * kFontScale + 5.0f);
    height += 12.0f;
    float x = mouse.x + 20.0f;
    float y = mouse.y - height - 18.0f;
    if (x + width > kScreenWidth - 24.0f) x = mouse.x - width - 20.0f;
    if (y < 16.0f) y = mouse.y + 20.0f;
    if (y + height > kScreenHeight - 16.0f) y = std::max(16.0f, kScreenHeight - height - 16.0f);

    Rectangle panel{x, y, width, height};
    drawParchmentPanel(panel, Color{221, 199, 151, 245});
    DrawLineEx({panel.x + 12.0f, panel.y + 36.0f}, {panel.x + panel.width - 12.0f, panel.y + 36.0f}, 1.0f,
               Color{99, 61, 32, 150});

    float textY = panel.y + 14.0f;
    if (!drawLines.empty()) {
        drawTextStrong(drawLines.front(), panel.x + 14.0f, textY, 24.0f, kParchmentInk);
        textY += 39.0f;
    }
    for (size_t i = 1; i < drawLines.size(); ++i) {
        drawText(fitText(drawLines[i], panel.width - 28.0f, 20.0f),
                 panel.x + 14.0f,
                 textY,
                 20.0f,
                 kParchmentMuted);
        textY += 20.0f * kFontScale + 5.0f;
    }
}

bool terrainTooltipLines(TerrainKind terrain, std::vector<std::string>& lines) {
    switch (terrain) {
        case TerrainKind::NeutralCamp:
            lines = {"Neutral Camp", "Guarded fight. Clear it for gold and relic chances."};
            return true;
        case TerrainKind::BossSite:
            lines = {"Boss Site", "Major neutral threat. Bosses stay passive until provoked."};
            return true;
        case TerrainKind::Trap:
            lines = {"Redcap Ambush", "Triggered trap. Redcaps remain until killed."};
            return true;
        case TerrainKind::ClearedBoss:
            lines = {"Defeated Boss", "Skull marker: this boss has already been cleared."};
            return true;
        case TerrainKind::ClearedObjective:
            lines = {"Cleared Objective", "Skull marker: this objective has already paid out."};
            return true;
        default:
            return false;
    }
}

void drawTerrainTooltip(const GameSnapshot& snapshot, Vector2 mouse) {
    Coord coord;
    if (!mouseToCell(mouse, coord)) return;

    std::vector<std::string> lines;
    if (!terrainTooltipLines(terrainAt(snapshot, coord), lines)) return;

    float width = 300.0f;
    for (const std::string& line : lines) {
        width = std::max(width, std::min(470.0f, measureText(line, 22.0f).x + 30.0f));
    }
    width = std::clamp(width, 300.0f, 470.0f);
    float lineHeight = 21.0f * kFontScale + 5.0f;
    int bodyLines = std::max(1, static_cast<int>(std::ceil(measureText(lines[1], 21.0f).x /
                                                            std::max(1.0f, width - 28.0f))));
    bodyLines = std::clamp(bodyLines, 1, 3);
    float height = 58.0f + static_cast<float>(bodyLines) * lineHeight;
    float x = mouse.x + 20.0f;
    float y = mouse.y - height - 18.0f;
    if (x + width > kScreenWidth - 24.0f) x = mouse.x - width - 20.0f;
    if (y < 16.0f) y = mouse.y + 20.0f;

    Rectangle panel{x, y, width, height};
    drawParchmentPanel(panel, Color{221, 199, 151, 246});
    drawTextStrong(lines[0], panel.x + 14.0f, panel.y + 12.0f, 24.0f, kParchmentInk);
    DrawLineEx({panel.x + 12.0f, panel.y + 40.0f},
               {panel.x + panel.width - 12.0f, panel.y + 40.0f},
               1.0f,
               Color{99, 61, 32, 150});
    drawWrappedTextLimited(lines[1],
                           panel.x + 14.0f,
                           panel.y + 48.0f,
                           panel.width - 28.0f,
                           21.0f,
                           kParchmentMuted,
                           panel.y + panel.height - 10.0f,
                           bodyLines);
}

bool eventHasBoardCue(const Event& event) {
    switch (event.type) {
        case EventType::UnitMoved:
            return event.text.find("astral raid") != std::string::npos ||
                   event.text.find("leaped to the backline") != std::string::npos ||
                   event.text.find("used Ambush") != std::string::npos ||
                   event.text.find("returned to its guard post") != std::string::npos ||
                   isKnockbackCueText(event.text);
        case EventType::UnitAttacked:
        case EventType::DamageDealt:
        case EventType::Healed:
        case EventType::Shielded:
        case EventType::StatusApplied:
        case EventType::UnitDied:
        case EventType::GoldGained:
            return coordOnBoard(event.to);
        default:
            return false;
    }
}

bool eventLooksArcane(const Event& event) {
    if (std::optional<AbilityKind> ability = abilityFromEvent(event)) {
        switch (*ability) {
            case AbilityKind::NecromancerSummon:
            case AbilityKind::MephitDeathBurst:
            case AbilityKind::DragonBreath:
            case AbilityKind::ClericHeal:
            case AbilityKind::FrostNova:
            case AbilityKind::DruidSummon:
            case AbilityKind::SpectatorWoundingRay:
            case AbilityKind::MindBlast:
            case AbilityKind::Counterspell:
            case AbilityKind::AnimatingSpores:
            case AbilityKind::BossFireball:
            case AbilityKind::DiabolicChains:
            case AbilityKind::SelunesIre:
            case AbilityKind::EvokerMagicMissile:
            case AbilityKind::DominatePerson:
            case AbilityKind::StrikeOfTheGuardian:
            case AbilityKind::Blight:
                return true;
            case AbilityKind::None:
            case AbilityKind::GithyankiAstralRaid:
            case AbilityKind::BarbarianHeavySwing:
            case AbilityKind::PaladinCharge:
            case AbilityKind::GuardianShield:
            case AbilityKind::RogueAmbush:
            case AbilityKind::KarnissCruelSting:
            case AbilityKind::OwlbearMultiattack:
            case AbilityKind::HiemalStrike:
            case AbilityKind::VenomousBite:
            case AbilityKind::KethericSmite:
            case AbilityKind::ExtractBrain:
            case AbilityKind::MinotaurCharge:
            case AbilityKind::StaggeringSmite:
            case AbilityKind::ElectrifiedFlail:
                return false;
        }
    }
    return event.type == EventType::GoldGained ||
           event.text.find("cast") != std::string::npos ||
           event.text.find("breath") != std::string::npos ||
           event.text.find("shield") != std::string::npos ||
           event.text.find("healed") != std::string::npos ||
           event.text.find("Status") != std::string::npos ||
           event.text.find("astral") != std::string::npos ||
           event.text.find("leaped") != std::string::npos;
}

CombatCueVisualKind visualKindFromEvent(const Event& event) {
    if (event.type == EventType::Healed) return CombatCueVisualKind::Heal;
    if (event.type == EventType::GoldGained) return CombatCueVisualKind::Gold;
    if (event.type == EventType::Shielded) return CombatCueVisualKind::Shield;
    if (event.type == EventType::UnitMoved && isKnockbackCueText(event.text)) return CombatCueVisualKind::Knockback;
    if (event.type == EventType::UnitMoved && event.text.find("returned to its guard post") != std::string::npos) {
        return CombatCueVisualKind::ReturnHome;
    }
    if (event.text.find("used Ambush") != std::string::npos ||
        event.text.find("astral raid") != std::string::npos ||
        event.text.find("leaped") != std::string::npos) {
        return CombatCueVisualKind::Leap;
    }
    if (event.type == EventType::UnitAttacked &&
        (event.text.find("Elven Ranger") != std::string::npos ||
         event.text.find("Ranger") != std::string::npos ||
         event.text.find("arrow") != std::string::npos)) {
        return CombatCueVisualKind::Arrow;
    }
    if (eventLooksArcane(event)) return CombatCueVisualKind::Spell;
    return CombatCueVisualKind::Melee;
}

void pushCombatCue(const Event& event, std::vector<CombatCue>* combatCues) {
    if (!combatCues || !eventHasBoardCue(event)) return;
    std::optional<AbilityKind> ability = abilityFromEvent(event);
    CombatCueVisualKind visual = visualKindFromEvent(event);
    bool arcane = visual == CombatCueVisualKind::Spell ||
                  visual == CombatCueVisualKind::Heal ||
                  visual == CombatCueVisualKind::Gold ||
                  visual == CombatCueVisualKind::Shield ||
                  (eventLooksArcane(event) && visual != CombatCueVisualKind::Arrow);
    if (event.type == EventType::UnitAttacked && visual == CombatCueVisualKind::Melee && !ability.has_value()) return;
    if (event.type == EventType::DamageDealt || event.type == EventType::UnitDied) return;
    CombatCue cue;
    cue.type = event.type;
    cue.targetId = event.target;
    cue.from = event.from;
    cue.to = event.to;
    cue.player = event.player;
    cue.amount = event.amount;
    cue.ability = ability.value_or(AbilityKind::None);
    cue.arcane = arcane;
    cue.visual = visual;
    cue.text = event.text;
    cue.ttl = cueUsesArcaneTexture(cue) ? 0.55f : 0.38f;
    if (cue.visual == CombatCueVisualKind::Arrow || cue.visual == CombatCueVisualKind::Leap ||
        cue.visual == CombatCueVisualKind::ReturnHome) {
        cue.ttl = 0.42f;
    }
    if (event.type == EventType::UnitMoved && isKnockbackCueText(event.text)) cue.ttl = 0.48f;
    combatCues->push_back(cue);
    if (combatCues->size() > 28) {
        combatCues->erase(combatCues->begin(), combatCues->end() - 28);
    }
}

void updateCombatCues(std::vector<CombatCue>& combatCues, float frameTime) {
    for (CombatCue& cue : combatCues) cue.age += frameTime;
    combatCues.erase(std::remove_if(combatCues.begin(), combatCues.end(),
                                    [](const CombatCue& cue) { return cue.age >= cue.ttl; }),
                     combatCues.end());
}

bool shouldShowInUiLog(const Event& event) {
    if (event.text.empty()) return false;
    switch (event.type) {
        case EventType::AiPolicyStatus:
        case EventType::DamageDealt:
            return false;
        case EventType::UnitMoved:
            return abilityFromEvent(event).has_value() ||
                   event.text.find("used Ambush") != std::string::npos ||
                   event.text.find("returned to its guard post") != std::string::npos;
        case EventType::UnitAttacked:
            return abilityFromEvent(event).has_value() ||
                   event.text.find("critical") != std::string::npos;
        case EventType::StatusApplied:
            return event.text.find("Redcap ambush") != std::string::npos ||
                   event.text.find("Counterspell") != std::string::npos ||
                   event.text.find("awakens") != std::string::npos ||
                   event.text.find("Stunned") != std::string::npos ||
                   event.text.find("Prone") != std::string::npos;
        case EventType::Shielded:
            return event.text.find("Rage") != std::string::npos;
        case EventType::Healed:
        case EventType::UnitDied:
        case EventType::GoldGained:
        case EventType::Upgraded:
        case EventType::RoundStarted:
        case EventType::Victory:
            return true;
        case EventType::Bought:
        case EventType::Deployed:
        case EventType::CombatStarted:
            return false;
    }
    return false;
}

void appendEvents(GameEngine& engine,
                  std::vector<std::string>& log,
                  std::ofstream* logFile = nullptr,
                  std::vector<CombatCue>* combatCues = nullptr,
                  std::vector<Event>* consumedEvents = nullptr) {
    bool wroteLogFile = false;
    for (const Event& event : engine.consumeEvents()) {
        if (consumedEvents) consumedEvents->push_back(event);
        if (event.text.empty()) continue;
        pushCombatCue(event, combatCues);
        if (shouldShowInUiLog(event)) log.push_back(event.text);
        if (logFile && *logFile) {
            *logFile << event.text << "\n";
            wroteLogFile = true;
        }
    }
    if (wroteLogFile && logFile && *logFile) logFile->flush();
    if (log.size() > static_cast<size_t>(kBattleLogVisibleLines)) {
        log.erase(log.begin(), log.end() - kBattleLogVisibleLines);
    }
}

std::ofstream openFreshEventLog() {
    std::error_code ec;
    std::filesystem::create_directories(kCacheDir, ec);
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(kCacheDir, ec)) {
        if (ec || !entry.is_regular_file()) continue;
        std::string name = entry.path().filename().string();
        bool isGuiEventLog = name == kLatestEventLog.filename().string() ||
                             (name.rfind("gui-events-", 0) == 0 && entry.path().extension() == ".log");
        if (isGuiEventLog) std::filesystem::remove(entry.path(), ec);
    }
    return std::ofstream(kLatestEventLog, std::ios::trunc);
}

} // namespace

int main() {
    std::random_device device;
    unsigned sessionSeed = device() ^
                           static_cast<unsigned>(
                               std::chrono::high_resolution_clock::now().time_since_epoch().count());
    GameEngine engine(sessionSeed);
    GameConfig config;
    config.mode = GameMode::SinglePlayerVsAi;
    config.aiDifficulty = AiDifficulty::Normal;
    engine.startNewGame(config);

    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "Tavern Tactics");
    int monitor = GetCurrentMonitor();
    SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
    SetWindowPosition(0, 0);
    SetWindowState(FLAG_FULLSCREEN_MODE);
    SetTargetFPS(60);
    RenderTexture2D canvas = LoadRenderTexture(kScreenWidth, kScreenHeight);
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_BILINEAR);
    gFont = loadUiFont();
    gBoldFont = loadBoldUiFont();
    loadUiTextures();

    std::vector<std::string> playerRelics;
    RunModifiers playerModifiers;
    std::optional<DraftState> pendingDraft;
    std::vector<DraftState> queuedNeutralDrafts;
    int draftScrollIndex = 0;
    unsigned neutralDraftSeed = sessionSeed ^ 0x6d2b79f5u;
    engine.setRunModifiers(PlayerId::One, playerModifiers);
    engine.setRunModifiers(PlayerId::Two, RunModifiers{});

    UnitId dragging = kInvalidUnitId;
    UnitId inspectedUnit = kInvalidUnitId;
    float shopScroll = 0.0f;
    DetailPanelState detailPanel;
    bool explorationRoundChoicesDismissed = false;
    double accumulator = 0.0;
    std::vector<std::string> log;
    std::vector<CombatCue> combatCues;
    std::ofstream eventLog = openFreshEventLog();
    appendEvents(engine, log, &eventLog, &combatCues);

    while (!WindowShouldClose()) {
        float frameTime = std::min(GetFrameTime(), 0.12f);
        updateCombatCues(combatCues, frameTime);
        GameSnapshot snapshot = engine.snapshot();

        bool draftPausesCombat = pendingDraft.has_value() && snapshot.phase == Phase::Combat;
        if (snapshot.phase == Phase::Combat && !draftPausesCombat) {
            accumulator += frameTime;
            constexpr double kFixedTick = 1.0 / 30.0;
            int steps = 0;
            while (accumulator >= kFixedTick && steps < 4) {
                engine.tick(kFixedTick);
                accumulator -= kFixedTick;
                ++steps;
            }
            if (steps >= 4 && accumulator >= kFixedTick) accumulator = 0.0;
        } else {
            accumulator = 0.0;
            if (!draftPausesCombat) engine.tick(frameTime);
        }
        std::vector<Event> consumedEvents;
        appendEvents(engine, log, &eventLog, &combatCues, &consumedEvents);
        snapshot = engine.snapshot();
        bool hadDraftBeforeQueue = pendingDraft.has_value();
        queueNeutralRelicDrops(consumedEvents, playerRelics, playerModifiers, neutralDraftSeed, log,
                               pendingDraft, queuedNeutralDrafts);
        if (!hadDraftBeforeQueue && pendingDraft) draftScrollIndex = 0;

        Vector2 screenMouse = GetMousePosition();
        gMousePosition = screenToCanvasMouse(screenMouse);
        Vector2 mouse = gMousePosition;
        Rectangle activeDraftPanel = detailRect();
        const UnitView* hoveredUnit = unitAtMouse(snapshot, mouse);
        const UnitSpec* hoveredShopSpec = shopSpecAtMouse(engine, mouse, shopScroll);
        if (hoveredUnit) {
            inspectedUnit = hoveredUnit->id;
        } else if (hoveredShopSpec) {
            inspectedUnit = kInvalidUnitId;
        }
        shopScroll = clampShopScroll(engine, shopScroll);
        DetailSelection wheelDetail = resolveDetailSelection(engine, snapshot, mouse, shopScroll, inspectedUnit);
        syncDetailPanelState(detailPanel,
                             wheelDetail.spec ? wheelDetail.spec : shopSpecAtIndex(engine, 0),
                             wheelDetail.unit);
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            if (pendingDraft && CheckCollisionPointRec(mouse, activeDraftPanel)) {
                int count = static_cast<int>(pendingDraft->offers.size());
                draftScrollIndex = draftVisibleStart(draftScrollIndex + (wheel < 0.0f ? 1 : -1), count);
            } else if (!pendingDraft && CheckCollisionPointRec(mouse, explorationUnitDetailRect())) {
                detailPanel.scroll = std::max(0.0f, detailPanel.scroll - wheel * 72.0f);
            } else if (CheckCollisionPointRec(mouse, shopViewportRect())) {
                shopScroll = clampShopScroll(engine, shopScroll - wheel * 72.0f);
            }
        }

        bool handledMouseDown = false;
        if (pendingDraft && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (CheckCollisionPointRec(mouse, draftScrollButtonRect(activeDraftPanel, false))) {
                draftScrollIndex = draftVisibleStart(draftScrollIndex - 1,
                                                     static_cast<int>(pendingDraft->offers.size()));
                handledMouseDown = true;
            } else if (CheckCollisionPointRec(mouse, draftScrollButtonRect(activeDraftPanel, true))) {
                draftScrollIndex = draftVisibleStart(draftScrollIndex + 1,
                                                     static_cast<int>(pendingDraft->offers.size()));
                handledMouseDown = true;
            }
            int offerIndex = handledMouseDown ? -1
                                              : draftOfferAtMouse(*pendingDraft,
                                                                  mouse,
                                                                  activeDraftPanel,
                                                                  draftScrollIndex);
            if (offerIndex >= 0 && offerIndex < static_cast<int>(pendingDraft->offers.size())) {
                applyRelicChoice(pendingDraft->offers[static_cast<size_t>(offerIndex)],
                                 playerRelics, playerModifiers, engine);
                log.push_back("Selected relic: " + pendingDraft->offers[static_cast<size_t>(offerIndex)].relic.name);
                pendingDraft.reset();
                promoteNextDraft(pendingDraft, queuedNeutralDrafts);
                draftScrollIndex = 0;
                appendEvents(engine, log, &eventLog, &combatCues);
                handledMouseDown = true;
            }
        }
        if (!handledMouseDown && snapshot.phase == Phase::Preparation && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            bool handledDifficulty = false;
            bool handledExplorationRounds = false;
            if (!pendingDraft) {
                const std::array<AiDifficulty, 3> difficulties = {
                    AiDifficulty::Normal,
                    AiDifficulty::Hard,
                    AiDifficulty::SuperHard
                };
                for (int i = 0; i < static_cast<int>(difficulties.size()); ++i) {
                    if (CheckCollisionPointRec(mouse, difficultyRect(i))) {
                        engine.setAiDifficulty(difficulties[i]);
                        handledDifficulty = true;
                        break;
                    }
                }
                if (snapshot.explorationRound == 0 && !snapshot.explorationRoundLimitLocked) {
                    const std::array<int, 5> explorationChoices = {2, 4, 6, 8, 10};
                    int choiceIndex = explorationRoundChoiceIndexAt(mouse);
                    if (choiceIndex < 0) choiceIndex = explorationRoundChoiceIndexAt(screenMouse);
                    if (choiceIndex >= 0 && choiceIndex < static_cast<int>(explorationChoices.size())) {
                        int chosenRounds = explorationChoices[static_cast<size_t>(choiceIndex)];
                        engine.setExplorationRoundLimit(chosenRounds);
                        engine.lockExplorationRoundLimit();
                        explorationRoundChoicesDismissed = true;
                        log.push_back(TextFormat("Exploration rounds locked: %d", chosenRounds));
                        handledExplorationRounds = true;
                    }
                }
            }

            if (handledDifficulty || handledExplorationRounds) {
                appendEvents(engine, log, &eventLog, &combatCues);
            } else if (!pendingDraft && CheckCollisionPointRec(mouse, readyRect())) {
                if (hasPlayerCombatUnit(snapshot)) {
                    if (!snapshot.explorationRoundLimitLocked) engine.lockExplorationRoundLimit();
                    explorationRoundChoicesDismissed = true;
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
                        Rectangle slot = benchSlotRect(i, std::max(10, engine.benchLimit(PlayerId::One)));
                        if (CheckCollisionPointRec(mouse, slot)) {
                            dragging = snapshot.players[0].bench[i];
                            break;
                        }
                    }
                    if (dragging == kInvalidUnitId) {
                        const UnitView* unit = unitAtBoardMouse(snapshot, mouse, true);
                        if (unit) dragging = unit->id;
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

        appendEvents(engine, log, &eventLog, &combatCues);
        snapshot = engine.snapshot();
        const UnitView* postHoverUnit = unitAtMouse(snapshot, mouse);
        const UnitSpec* postHoverShopSpec = shopSpecAtMouse(engine, mouse, shopScroll);
        if (postHoverUnit) {
            inspectedUnit = postHoverUnit->id;
        } else if (postHoverShopSpec) {
            inspectedUnit = kInvalidUnitId;
        }
        DetailSelection detail = resolveDetailSelection(engine, snapshot, mouse, shopScroll, inspectedUnit);
        const UnitView* detailUnit = detail.unit;
        const UnitSpec* detailSpec = detail.spec ? detail.spec : shopSpecAtIndex(engine, 0);
        syncDetailPanelState(detailPanel, detailSpec, detailUnit);

        BeginTextureMode(canvas);
        ClearBackground(Color{13, 10, 9, 255});
        drawSceneBackground();

        drawDifficultySelector(engine, snapshot);
        bool canStart = snapshot.phase == Phase::Preparation && hasPlayerCombatUnit(snapshot);
        bool canUsePrimary = pendingDraft.has_value() ||
                             (snapshot.phase == Phase::Preparation && canStart);
        std::string primaryAction = pendingDraft
                                        ? "Pick Relic"
                                        : (snapshot.phase == Phase::Preparation
                                               ? (canStart ? "Ready" : "Deploy")
                                               : "Fighting");
        DrawRectangleRounded(readyRect(), 0.12f, 8,
                             snapshot.phase == Phase::Preparation
                                  ? (canUsePrimary ? Color{88, 49, 31, 255} : Color{43, 33, 31, 255})
                                  : Color{44, 28, 26, 255});
        DrawRectangleRoundedLines(readyRect(), 0.12f, 8, 1.5f,
                                  canUsePrimary ? kGold : Color{92, 72, 48, 255});
        drawTextCenteredStrong(primaryAction, readyRect(), 30.0f, kInk);

        drawBoard(engine, snapshot, dragging, combatCues);
        drawRelicsPanel(playerRelics, playerModifiers);
        drawShop(engine, snapshot, shopScroll);
        if (pendingDraft) {
            drawRelicDraftPanel(*pendingDraft, playerModifiers, draftScrollIndex);
        } else {
            float maxDetailScroll = drawUnitDetails(detailSpec ? *detailSpec : engine.shop().front(),
                                                    detailUnit,
                                                    explorationUnitDetailRect(),
                                                    detailPanel.scroll);
            detailPanel.scroll = std::clamp(detailPanel.scroll, 0.0f, maxDetailScroll);
            drawRunInfoPanel(snapshot, explorationRoundChoicesDismissed);
        }
        drawBench(snapshot, dragging, engine.benchLimit(PlayerId::One));

        if (dragging != kInvalidUnitId) {
            const UnitView* unit = findUnit(snapshot, dragging);
            if (unit) {
                Rectangle ghost{mouse.x - kCell * 0.5f, mouse.y - kCell * 0.5f, kCell, kCell};
                drawUnit(*unit, ghost, true);
            }
        }

        drawBattleLogPanel(log);
        if (!postHoverUnit && dragging == kInvalidUnitId) {
            drawTerrainTooltip(snapshot, mouse);
        }
        drawTopStatusHeader(snapshot);

        if (snapshot.winner) {
            Rectangle overlay{0, 0, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)};
            DrawRectangleRec(overlay, Color{0, 0, 0, 170});
            std::string text = toString(*snapshot.winner) + " wins";
            drawTextCentered(text, {0, 320, static_cast<float>(kScreenWidth), 80}, 46.0f, kInk);
        }

        EndTextureMode();

        BeginDrawing();
        ClearBackground(Color{8, 7, 7, 255});
        Rectangle canvasDest = canvasDestRect();
        DrawTexturePro(canvas.texture,
                       {0.0f, 0.0f, static_cast<float>(canvas.texture.width),
                        -static_cast<float>(canvas.texture.height)},
                       canvasDest,
                       {0.0f, 0.0f},
                       0.0f,
                       WHITE);
        EndDrawing();
    }

    unloadUiTextures();
    UnloadRenderTexture(canvas);
    if (gCustomBoldFont) UnloadFont(gBoldFont);
    if (gCustomFont) UnloadFont(gFont);
    CloseWindow();
    return 0;
}
