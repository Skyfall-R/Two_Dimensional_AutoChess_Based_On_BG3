#!/usr/bin/env python3
"""Fetch BG3 wiki visual assets into the local UI library.

The script keeps a broad future-facing cache under assets/ui/bg3/library and
also creates stable assigned relic icons at assets/ui/bg3/relics/assigned.
"""

from __future__ import annotations

import argparse
import json
import re
import time
from dataclasses import dataclass
from io import BytesIO
from pathlib import Path
from typing import Callable
from urllib.parse import quote, unquote, urljoin

import requests
from bs4 import BeautifulSoup
from PIL import Image


BASE_URL = "https://bg3.wiki"
API_URL = f"{BASE_URL}/w/api.php"
USER_AGENT = "AutoChessAssetCollector/1.0"


@dataclass
class Asset:
    category: str
    title: str
    source_page: str
    source_url: str
    local_file: str


def slugify(text: str, max_len: int = 96) -> str:
    text = unquote(text)
    text = re.sub(r"\.(png|webp|jpg|jpeg)(\.webp)?$", "", text, flags=re.IGNORECASE)
    text = re.sub(r"^\d+px-", "", text)
    text = re.sub(r"[^A-Za-z0-9]+", "_", text).strip("_").lower()
    if not text:
        text = "asset"
    return text[:max_len].rstrip("_")


def clean_title_from_src(src: str, alt: str) -> str:
    if alt and alt.strip():
        return alt.strip()
    name = unquote(src.split("/")[-1])
    name = re.sub(r"^\d+px-", "", name)
    name = re.sub(r"\.(png|webp|jpg|jpeg)(\.webp)?$", "", name, flags=re.IGNORECASE)
    return name.replace("_", " ").strip()


def page_url(page: str) -> str:
    return f"{BASE_URL}/wiki/{quote(page.replace(' ', '_'))}"


def image_url(src: str) -> str:
    url = urljoin(BASE_URL, src)
    marker = "/w/images/thumb/"
    if marker not in url:
        return url
    prefix, rest = url.split(marker, 1)
    parts = rest.split("/")
    if len(parts) < 4:
        return url
    original_name = parts[-2]
    return prefix + "/w/images/" + "/".join(parts[:2] + [original_name])


def image_name(src: str) -> str:
    return unquote(src.split("/")[-1])


def request_get(session: requests.Session, url: str, **kwargs) -> requests.Response:
    response = session.get(url, timeout=30, **kwargs)
    response.raise_for_status()
    return response


def save_image(session: requests.Session,
               url: str,
               destination: Path,
               square_size: int | None = None,
               max_side: int | None = None,
               overwrite: bool = True) -> bool:
    if not overwrite and destination.exists() and destination.stat().st_size > 0:
        return False
    response = request_get(session, url)
    image = Image.open(BytesIO(response.content)).convert("RGBA")

    if square_size:
        bbox = image.getbbox()
        if bbox:
            image = image.crop(bbox)
        target = max(1, int(square_size * 0.86))
        scale = min(target / image.width, target / image.height)
        image = image.resize((max(1, int(image.width * scale)),
                              max(1, int(image.height * scale))),
                             Image.Resampling.LANCZOS)
        canvas = Image.new("RGBA", (square_size, square_size), (0, 0, 0, 0))
        canvas.alpha_composite(image, ((square_size - image.width) // 2,
                                      (square_size - image.height) // 2))
        image = canvas
    elif max_side:
        image.thumbnail((max_side, max_side), Image.Resampling.LANCZOS)

    destination.parent.mkdir(parents=True, exist_ok=True)
    image.save(destination, "PNG", optimize=True)
    return True


def collect_page_images(session: requests.Session,
                        page: str,
                        category: str,
                        out_dir: Path,
                        predicate: Callable[[str], bool],
                        limit: int,
                        square_size: int | None,
                        max_side: int | None,
                        metadata: list[Asset]) -> list[Path]:
    url = page_url(page)
    html = request_get(session, url).text
    soup = BeautifulSoup(html, "html.parser")
    seen: set[str] = set()
    saved: list[Path] = []

    for img in soup.find_all("img"):
        src = img.get("src") or img.get("data-src") or ""
        if "/w/images/" not in src:
            continue
        name = image_name(src)
        if name in seen or not predicate(name):
            continue
        seen.add(name)

        title = clean_title_from_src(src, img.get("alt") or "")
        local = out_dir / f"{slugify(title or name)}.png"
        try:
            save_image(session, image_url(src), local, square_size=square_size, max_side=max_side)
        except Exception as exc:
            print(f"skip {category}: {title}: {exc}")
            continue
        metadata.append(Asset(category, title, url, image_url(src), str(local.as_posix())))
        saved.append(local)
        if len(saved) >= limit:
            break
    return saved


def category_members(session: requests.Session, category: str, limit: int) -> list[str]:
    titles: list[str] = []
    params = {
        "action": "query",
        "format": "json",
        "list": "categorymembers",
        "cmtitle": category,
        "cmlimit": "50",
    }
    while len(titles) < limit:
        data = request_get(session, API_URL, params=params).json()
        members = data.get("query", {}).get("categorymembers", [])
        titles.extend(member["title"] for member in members if not member["title"].startswith("Category:"))
        if "continue" not in data:
            break
        params.update(data["continue"])
        time.sleep(0.1)
    return titles[:limit]


def collect_page_thumbnails(session: requests.Session,
                            titles: list[str],
                            category: str,
                            out_dir: Path,
                            limit: int,
                            thumb_size: int,
                            metadata: list[Asset]) -> list[Path]:
    saved: list[Path] = []
    seen_urls: set[str] = set()
    for i in range(0, len(titles), 25):
        batch = titles[i:i + 25]
        params = {
            "action": "query",
            "format": "json",
            "titles": "|".join(batch),
            "prop": "pageimages",
            "pithumbsize": str(thumb_size),
        }
        data = request_get(session, API_URL, params=params).json()
        pages = data.get("query", {}).get("pages", {})
        for page in pages.values():
            title = page.get("title", "")
            source = page.get("thumbnail", {}).get("source", "")
            if not source or source in seen_urls:
                continue
            if any(token in source.lower() for token in ("icon", "20px-", "25px-", "map")):
                continue
            seen_urls.add(source)
            local = out_dir / f"{slugify(title)}.png"
            try:
                save_image(session, image_url(source), local)
            except Exception as exc:
                print(f"skip {category}: {title}: {exc}")
                continue
            metadata.append(Asset(category, title, page_url(title), source, str(local.as_posix())))
            saved.append(local)
            if len(saved) >= limit:
                return saved
        time.sleep(0.1)
    return saved


def indexed_choice(paths: list[Path], index: int) -> Path:
    if not paths:
        raise ValueError("cannot choose from an empty asset list")
    return paths[index % len(paths)]


def write_assigned_relics(paths_by_group: dict[str, list[Path]], assigned_dir: Path) -> None:
    assignments = {
        "coin_shard": ("rings", 5),
        "tarnished_ledger": ("amulets", 1),
        "field_roster": ("handwear", 3),
        "supply_mark": ("rings", 2),
        "brood_sigil": ("rings", 28),
        "bulwark_keystone": ("headwear", 10),
        "runic_lens": ("headwear", 4),
        "veiled_mark": ("handwear", 12),
        "siege_lens": ("amulets", 18),
        "mirror_contract": ("amulets", 8),
        "blood_tithe": ("rings", 34),
        "chimeric_plate": ("handwear", 20),
        "signal_caul": ("headwear", 22),
        "crown_of_mirrors": ("headwear", 30),
        "black_sun_vault": ("amulets", 28),
        "adaptive_signet": ("rings", 42),
    }
    assigned_dir.mkdir(parents=True, exist_ok=True)
    for relic_id, (group, index) in assignments.items():
        source = indexed_choice(paths_by_group.get(group, []), index)
        destination = assigned_dir / f"relic_{relic_id}.png"
        image = Image.open(source).convert("RGBA")
        image.save(destination, "PNG", optimize=True)


def write_exact_boss_assets(session: requests.Session,
                            library: Path,
                            metadata: list[Asset]) -> None:
    """Stable assets for named Stage1 bosses and their assigned skill icons."""
    exact_assets = [
        ("monsters/creatures", "Water Myrmidon", "Water_Myrmidon",
         "https://bg3.wiki/w/images/8/8b/Portrait_Water_Myrmidon.png",
         "water_myrmidon.png", None, None),
        ("monsters/creatures", "Phase Spider Matriarch", "Phase_Spider_Matriarch",
         "https://bg3.wiki/w/images/6/68/Portrait_Phase_Spider_Matriarch.png",
         "phase_spider_matriarch.png", None, None),
        ("monsters/creatures", "Raphael", "Raphael",
         "https://bg3.wiki/w/images/3/39/Portrait_Raphael.png",
         "raphael.png", None, None),
        ("monsters/creatures", "Ketheric Thorm", "Ketheric_Thorm",
         "https://bg3.wiki/w/images/f/f2/Portrait_Ketheric_Thorm.png",
         "ketheric_thorm.png", None, None),
        ("monsters/creatures", "Moonlight Sliver", "Moonlight_Sliver",
         "https://bg3.wiki/w/images/a/ab/Moonlight_Sliver_Model.png",
         "moonlight_sliver.png", None, None),
        ("monsters/creatures", "Tamia Holzt", "Tamia_Holzt",
         "https://bg3.wiki/w/images/4/4f/Black_Gauntlet_Tamia_Holzt_Model.png",
         "tamia_holzt.png", None, None),
        ("monsters/creatures", "Death Knight", "Death_Knight",
         "https://bg3.wiki/w/images/5/57/Portrait_Death_Knight.png",
         "death_knight.png", None, None),
        ("monsters/creatures", "Air Myrmidon", "Air_Myrmidon",
         "https://bg3.wiki/w/images/f/f4/Portrait_Air_Myrmidon.png",
         "air_myrmidon.png", None, None),
        ("skills/actions", "Hiemal Strike", "Hiemal_Strike",
         "https://bg3.wiki/w/images/6/64/Winter%27s_Breath.webp",
         "hiemal_strike.png", None, None),
        ("skills/actions", "Venomous Bite", "Venomous_Bite",
         "https://bg3.wiki/w/images/d/d5/Venomous_Bite_Icon.webp",
         "venomous_bite.png", None, None),
        ("skills/actions", "Venom Claws", "Venom_Claws",
         "https://bg3.wiki/w/images/3/34/Generic_Physical_Icon.webp",
         "venom_claws.png", None, None),
        ("skills/actions", "Owlbear Claws", "Claws",
         "https://bg3.wiki/w/images/6/6c/Claws_Bear_Icon.webp",
         "owlbear_claws.png", None, None),
        ("skills/actions", "Diabolic Chains", "Diabolic_Chains",
         "https://bg3.wiki/w/images/7/76/Scorching_Ray.webp",
         "diabolic_chains.png", None, None),
        ("skills/actions", "Divine Smite", "Divine_Smite",
         "https://bg3.wiki/w/images/d/d5/Divine_Smite.webp",
         "divine_smite.png", None, None),
        ("skills/actions", "Ketheric Smite", "Divine_Smite",
         "https://bg3.wiki/w/images/4/4b/Divine_Smite_Icon.webp",
         "ketheric_smite.png", None, None),
        ("skills/actions", "Selune's Ire", "Sel%C3%BBne's_Ire",
         "https://bg3.wiki/w/images/1/16/Sacred_Flame_Icon.webp",
         "selunes_ire.png", None, None),
        ("skills/actions", "Dominate Person", "Dominate_Person",
         "https://bg3.wiki/w/images/1/1d/Dominate_Person_Icon.webp",
         "dominate_person.png", None, None),
        ("skills/actions", "Blight", "Blight",
         "https://bg3.wiki/w/images/e/e0/Blight_Icon.webp",
         "blight.png", None, None),
        ("skills/actions", "Blinding Smite", "Blinding_Smite",
         "https://bg3.wiki/w/images/7/7a/Blinding_Smite.webp",
         "blinding_smite.png", None, None),
        ("skills/actions", "Staggering Smite", "Staggering_Smite",
         "https://bg3.wiki/w/images/8/82/Staggering_Smite.webp",
         "staggering_smite.png", None, None),
        ("skills/actions", "Electrified Flail", "Electrified_Flail",
         "https://bg3.wiki/w/images/a/a8/Generic_Lightning.webp",
         "electrified_flail.png", None, None),
    ]
    for category, title, page, source, filename, square_size, max_side in exact_assets:
        destination = library / category / filename
        save_image(session, source, destination, square_size=square_size, max_side=max_side)
        metadata.append(Asset(category, title, page_url(page), source, str(destination.as_posix())))

    divine = Image.open(library / "skills" / "actions" / "divine_smite.png").convert("RGBA")
    blinding = Image.open(library / "skills" / "actions" / "blinding_smite.png").convert("RGBA")
    canvas = Image.new("RGBA", (96, 96), (0, 0, 0, 0))
    canvas.alpha_composite(divine.resize((62, 62), Image.Resampling.LANCZOS), (3, 17))
    canvas.alpha_composite(blinding.resize((62, 62), Image.Resampling.LANCZOS), (31, 17))
    for y in range(16, 80):
        for x in range(45, 51):
            canvas.putpixel((x, y), (255, 235, 150, 80))
    canvas.save(library / "skills" / "actions" / "ketheric_smite_pair.png", "PNG", optimize=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default="assets/ui/bg3", help="BG3 asset root")
    args = parser.parse_args()

    root = Path(args.out)
    library = root / "library"
    relic_library = library / "relics"
    metadata: list[Asset] = []
    session = requests.Session()
    session.headers.update({"User-Agent": USER_AGENT})

    equipment_pages = {
        "rings": ("Rings", lambda name: "Ring_" in name and "_Icon" in name),
        "amulets": ("Amulets", lambda name: ("Amulet" in name or "Necklace" in name or
                                             "Talisman" in name or "Locket" in name or
                                             "Collar" in name) and "_Icon" in name),
        "headwear": ("Headwear", lambda name: "_Unfaded_Icon" in name and
                     not any(skip in name for skip in ("Condition", "Action", "Spell"))),
        "handwear": ("Handwear", lambda name: ("Gloves" in name or "Bracers" in name) and
                     "_Icon" in name),
    }

    paths_by_group: dict[str, list[Path]] = {}
    for group, (page, predicate) in equipment_pages.items():
        paths_by_group[group] = collect_page_images(
            session,
            page,
            f"relics/{group}",
            relic_library / group,
            predicate,
            limit=80,
            square_size=None,
            max_side=None,
            metadata=metadata,
        )

    skill_predicate = lambda name: "_Icon" in name and "_Unfaded_Icon" not in name
    for page in ("Spells", "Actions", "Illithid powers"):
        try:
            collect_page_images(session, page, f"skills/{slugify(page)}", library / "skills" / slugify(page),
                                skill_predicate, limit=90, square_size=None, max_side=None,
                                metadata=metadata)
        except Exception as exc:
            print(f"skip page {page}: {exc}")

    collect_page_images(session, "Conditions", "effects/conditions", library / "effects" / "conditions",
                        lambda name: "Condition_Icon" in name or "Generic_" in name,
                        limit=120, square_size=None, max_side=None, metadata=metadata)

    monster_titles: list[str] = []
    for category in ("Category:Creatures", "Category:Aberrations", "Category:Beasts",
                     "Category:Fiends", "Category:Monstrosities", "Category:Undead"):
        monster_titles.extend(category_members(session, category, 45))
    monster_titles = list(dict.fromkeys(monster_titles))
    collect_page_thumbnails(session, monster_titles, "monsters/creatures",
                            library / "monsters" / "creatures",
                            limit=100, thumb_size=256, metadata=metadata)

    quest_titles = category_members(session, "Category:Quests", 90)
    collect_page_thumbnails(session, quest_titles, "quests", library / "quests",
                            limit=70, thumb_size=256, metadata=metadata)
    collect_page_images(session, "Quests", "quests/list_images", library / "quests" / "list_images",
                        lambda name: ("Quest" in name or name.startswith("250px-")) and
                        "Ico_knownSpells" not in name,
                        limit=70, square_size=None, max_side=256, metadata=metadata)

    write_exact_boss_assets(session, library, metadata)
    write_assigned_relics(paths_by_group, root / "relics" / "assigned")

    metadata_file = library / "metadata.json"
    metadata_file.parent.mkdir(parents=True, exist_ok=True)
    metadata_file.write_text(json.dumps([asset.__dict__ for asset in metadata], indent=2),
                             encoding="utf-8")

    readme = library / "README.md"
    readme.write_text(
        "# BG3 Wiki Asset Library\n\n"
        "Generated from bg3.wiki for local prototype UI references.\n\n"
        "- `relics/`: original-size equipment icons from Rings, Amulets, Headwear, and Handwear.\n"
        "- `skills/`: original-size spell, action, and illithid-power icons.\n"
        "- `effects/`: original-size condition and effect icons.\n"
        "- `monsters/`: original-size creature images for future encounter content.\n"
        "- `monsters/creatures/{water_myrmidon,phase_spider_matriarch,raphael,ketheric_thorm,moonlight_sliver}.png`: stable Stage1 boss portraits.\n"
        "- `monsters/creatures/{death_knight,air_myrmidon}.png`: stable Stage1 camp/elite portraits.\n"
        "- `skills/actions/{hiemal_strike,venomous_bite,venom_claws,owlbear_claws,diabolic_chains,ketheric_smite,selunes_ire,staggering_smite,electrified_flail}.png`: stable Stage1 skill icons.\n"
        "- `quests/`: quest and journal imagery for future campaign content.\n"
        "- `../relics/assigned/`: stable icons consumed by the current relic UI.\n\n"
        "Source metadata is in `metadata.json`.\n",
        encoding="utf-8",
    )
    print(f"Saved {len(metadata)} assets under {library}")
    print("Assigned current relic icons under", root / "relics" / "assigned")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
