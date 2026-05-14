#!/usr/bin/env python3
"""Build readable BG3-style unit portrait tokens.

The GUI already draws a unified frame around unit icons, so these files should
stay focused on the portrait itself: close bust framing, strong silhouette, and
no baked decorative border.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Literal

from PIL import Image, ImageDraw, ImageEnhance, ImageFilter


ROOT = Path("assets/ui/bg3")
SIZE = 512
Mode = Literal["opaque", "transparent"]


@dataclass(frozen=True)
class PortraitConfig:
    output: str
    source: str
    crop: tuple[int, int, int, int] | None = None
    mode: Mode = "transparent"
    bg: tuple[int, int, int] = (44, 36, 32)
    width_fraction: float = 0.98
    top: int = 0
    x_offset: int = 0
    contrast: float = 1.08
    sharpness: float = 1.16


CONFIGS: tuple[PortraitConfig, ...] = (
    PortraitConfig("unit_neutral_water_myrmidon.png",
                   "library/monsters/creatures/water_myrmidon.png",
                   mode="opaque", contrast=1.12, sharpness=1.18),
    PortraitConfig("unit_neutral_phase_spider_matriarch.png",
                   "library/monsters/creatures/phase_spider_matriarch.png",
                   mode="opaque", contrast=1.12, sharpness=1.20),
    PortraitConfig("unit_neutral_raphael.png",
                   "library/monsters/creatures/raphael.png",
                   mode="opaque", contrast=1.13, sharpness=1.20),
    PortraitConfig("unit_neutral_ketheric_thorm.png",
                   "library/monsters/creatures/ketheric_thorm.png",
                   mode="opaque", contrast=1.12, sharpness=1.18),
    PortraitConfig("unit_neutral_air_myrmidon.png",
                   "library/monsters/creatures/air_myrmidon.png",
                   mode="opaque", contrast=1.12, sharpness=1.18),
    PortraitConfig("unit_neutral_karniss.png",
                   "sources/Kar'niss_Model.png",
                   crop=(300, 0, 650, 520), bg=(58, 30, 18),
                   width_fraction=1.00),
    PortraitConfig("unit_neutral_mind_flayer.png",
                   "sources/Mind_Flayer_Model.png",
                   crop=(0, 0, 320, 500), bg=(35, 25, 58),
                   width_fraction=1.05),
    PortraitConfig("unit_neutral_guardian_of_faith.png",
                   "library/monsters/creatures/guardian_of_faith.png",
                   crop=(0, 0, 264, 335), bg=(78, 60, 18),
                   width_fraction=1.14),
    PortraitConfig("unit_neutral_minotaur.png",
                   "library/monsters/creatures/minotaur.png",
                   crop=(8, 0, 384, 430), bg=(62, 39, 24),
                   width_fraction=1.04),
    PortraitConfig("unit_neutral_tamia_holzt.png",
                   "library/monsters/creatures/tamia_holzt.png",
                   crop=(35, 40, 455, 700), bg=(44, 28, 54),
                   width_fraction=0.98),
    PortraitConfig("unit_neutral_moonlight_sliver.png",
                   "library/monsters/creatures/moonlight_sliver.png",
                   crop=(190, 0, 765, 710), bg=(66, 62, 55),
                   width_fraction=1.14),
    PortraitConfig("unit_neutral_spectator_bright.png",
                   "sources/Spectator_Art.png",
                   crop=(115, 55, 1245, 1125), bg=(38, 26, 58),
                   width_fraction=1.04),
    PortraitConfig("unit_neutral_owlbear.png",
                   "sources/Owlbear_Model.png",
                   crop=(0, 0, 529, 520), bg=(60, 38, 22),
                   width_fraction=1.08),
    PortraitConfig("unit_neutral_sovereign_spaw.png",
                   "sources/Spaw_Model.png",
                   crop=(0, 0, 353, 500), bg=(35, 50, 36),
                   width_fraction=1.04),
    PortraitConfig("unit_neutral_redcap.png",
                   "sources/Redcap_Model.png",
                   crop=(0, 0, 386, 510), bg=(70, 30, 18),
                   width_fraction=1.02),
    PortraitConfig("unit_ranger.png",
                   "sources/Minsc_Model.png",
                   crop=(0, 0, 530, 760), bg=(46, 36, 22),
                   width_fraction=0.96),
    PortraitConfig("unit_arcane_evoker.png",
                   "sources/Gale_Model.png",
                   crop=(0, 0, 494, 760), bg=(28, 42, 65),
                   width_fraction=0.96),
    PortraitConfig("unit_githyanki_warrior.png",
                   "sources/Lae'zel_Model.png",
                   crop=(0, 0, 492, 760), bg=(44, 50, 36),
                   width_fraction=0.96),
    PortraitConfig("unit_paladin.png",
                   "sources/Minthara_Model.png",
                   crop=(0, 0, 499, 760), bg=(45, 38, 35),
                   width_fraction=0.96),
    PortraitConfig("unit_berserker.png",
                   "sources/Karlach_Model.png",
                   crop=(0, 0, 490, 760), bg=(62, 30, 18),
                   width_fraction=0.96),
    PortraitConfig("unit_life_cleric.png",
                   "sources/Shadowheart_Model.png",
                   crop=(0, 0, 396, 760), bg=(70, 62, 46),
                   width_fraction=0.88),
    PortraitConfig("unit_shadow_rogue.png",
                   "sources/Astarion_Model.png",
                   crop=(0, 0, 542, 760), bg=(42, 38, 44),
                   width_fraction=0.96),
    PortraitConfig("unit_circle_druid.png",
                   "sources/Halsin_Model.png",
                   crop=(0, 0, 300, 410), bg=(36, 58, 36),
                   width_fraction=0.92),
    PortraitConfig("unit_necromancer.png",
                   "sources/Balthazar_Model.png",
                   crop=(20, 0, 670, 850), bg=(42, 36, 54),
                   width_fraction=0.98),
    PortraitConfig("unit_fire_mephit.png",
                   "sources/Magma_Mephit_Model.png",
                   crop=(130, 0, 980, 650), bg=(78, 38, 20),
                   width_fraction=1.06),
    PortraitConfig("unit_imp_swarm.png",
                   "sources/Imp_Model.png",
                   crop=(20, 0, 620, 570), bg=(75, 32, 24),
                   width_fraction=1.02),
    PortraitConfig("unit_goblin_ambusher.png",
                   "sources/Goblin_Brawler_Model.png",
                   crop=(35, 10, 700, 790), bg=(42, 58, 28),
                   width_fraction=1.02),
    PortraitConfig("unit_skeleton.png",
                   "sources/Skeleton_Model.png",
                   crop=(0, 0, 585, 760), bg=(54, 52, 62),
                   width_fraction=0.96),
    PortraitConfig("unit_shield_guardian.png",
                   "sources/Animated_Armour_Model.png",
                   crop=(0, 0, 622, 780), bg=(58, 58, 62),
                   width_fraction=1.02),
    PortraitConfig("unit_guard_tower.png",
                   "sources/Steel_Watcher_Model.png",
                   crop=(0, 0, 547, 760), bg=(46, 58, 62),
                   width_fraction=1.02),
    PortraitConfig("unit_dragon_wyrmling.png",
                   "sources/Red_Dragon_Model.png",
                   crop=(70, 0, 1070, 660), bg=(70, 22, 16),
                   width_fraction=1.04),
    PortraitConfig("unit_treant.png",
                   "sources/Wood_Woad_Model.png",
                   crop=(0, 0, 476, 560), bg=(35, 62, 36),
                   width_fraction=1.02),
)


def alpha_bbox(image: Image.Image) -> tuple[int, int, int, int]:
    return image.getchannel("A").getbbox() or (0, 0, image.width, image.height)


def composite_clipped(dst: Image.Image, src: Image.Image, x: int, y: int) -> None:
    sx = max(0, -x)
    sy = max(0, -y)
    dx = max(0, x)
    dy = max(0, y)
    width = min(src.width - sx, dst.width - dx)
    height = min(src.height - sy, dst.height - dy)
    if width <= 0 or height <= 0:
        return
    part = src.crop((sx, sy, sx + width, sy + height))
    dst.alpha_composite(part, (dx, dy))


def portrait_background(rgb: tuple[int, int, int]) -> Image.Image:
    image = Image.new("RGBA", (SIZE, SIZE), (12, 10, 10, 255))
    glow = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(glow)
    for radius, alpha in ((255, 70), (215, 48), (170, 32), (120, 20)):
        draw.ellipse((SIZE // 2 - radius,
                      SIZE // 2 - radius - 35,
                      SIZE // 2 + radius,
                      SIZE // 2 + radius - 35),
                     fill=(rgb[0], rgb[1], rgb[2], alpha))
    image.alpha_composite(glow)

    vignette = Image.new("L", (SIZE, SIZE), 0)
    mask_draw = ImageDraw.Draw(vignette)
    mask_draw.ellipse((-95, -70, SIZE + 95, SIZE + 120), fill=225)
    vignette = vignette.filter(ImageFilter.GaussianBlur(42))
    dark = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 150))
    return Image.composite(image, dark, vignette)


def cover_square(image: Image.Image) -> Image.Image:
    scale = max(SIZE / image.width, SIZE / image.height)
    resized = image.resize((round(image.width * scale), round(image.height * scale)),
                           Image.Resampling.LANCZOS)
    left = (resized.width - SIZE) // 2
    top = (resized.height - SIZE) // 2
    return resized.crop((left, top, left + SIZE, top + SIZE))


def render_portrait(config: PortraitConfig) -> Image.Image:
    source = Image.open(ROOT / config.source).convert("RGBA")
    if config.crop:
        source = source.crop(config.crop)

    if config.mode == "opaque":
        portrait = cover_square(source)
        portrait = ImageEnhance.Contrast(portrait).enhance(config.contrast)
        return ImageEnhance.Sharpness(portrait).enhance(config.sharpness)

    x0, y0, x1, y1 = alpha_bbox(source)
    pad = max(2, min(source.width, source.height) // 40)
    subject = source.crop((max(0, x0 - pad),
                           max(0, y0 - pad),
                           min(source.width, x1 + pad),
                           min(source.height, y1 + pad)))
    scale = (SIZE * config.width_fraction) / subject.width
    subject = subject.resize((max(1, round(subject.width * scale)),
                              max(1, round(subject.height * scale))),
                             Image.Resampling.LANCZOS)
    subject = ImageEnhance.Contrast(subject).enhance(config.contrast)
    subject = ImageEnhance.Sharpness(subject).enhance(config.sharpness)

    portrait = portrait_background(config.bg)
    shadow = Image.new("RGBA", subject.size, (0, 0, 0, 0))
    shadow.putalpha(subject.getchannel("A").filter(ImageFilter.GaussianBlur(11)))

    x = round((SIZE - subject.width) / 2 + config.x_offset)
    y = config.top
    composite_clipped(portrait, shadow, x + 10, y + 15)
    composite_clipped(portrait, subject, x, y)
    return portrait


def build_contact_sheet(paths: list[Path]) -> Path:
    thumb = 148
    pad = 14
    label_height = 38
    cols = 6
    items: list[tuple[str, Path]] = [
        ("death ref", ROOT / "library/monsters/creatures/death_knight.png")
    ]
    items.extend((path.stem.replace("unit_neutral_", "n_").replace("unit_", "")[:22], path)
                 for path in paths)
    rows = (len(items) + cols - 1) // cols
    sheet = Image.new("RGBA",
                      (pad + cols * (thumb + pad), pad + rows * (thumb + label_height + pad)),
                      (16, 13, 12, 255))
    draw = ImageDraw.Draw(sheet)
    for index, (label, path) in enumerate(items):
        image = Image.open(path).convert("RGBA")
        image.thumbnail((thumb, thumb), Image.Resampling.LANCZOS)
        x = pad + (index % cols) * (thumb + pad)
        y = pad + (index // cols) * (thumb + label_height + pad)
        sheet.alpha_composite(image, (x + (thumb - image.width) // 2,
                                      y + (thumb - image.height) // 2))
        draw.text((x, y + thumb + 3), label, fill=(235, 225, 205, 255))
    output = Path("tmp/bg3_portrait_tokens_sheet.png")
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    return output


def main() -> int:
    written: list[Path] = []
    for config in CONFIGS:
        output = ROOT / config.output
        output.parent.mkdir(parents=True, exist_ok=True)
        render_portrait(config).save(output, "PNG", optimize=True)
        written.append(output)
        print(output)
    print("contact_sheet", build_contact_sheet(written))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
