# BG3 UI Assets

These are the active portrait and icon sources for the Tavern Tactics UI. Unit
icons are 512x512 close bust portraits without baked decorative frames; the GUI
draws the shared frame at render time so every token stays readable at board
scale.

Regenerate unit portraits with:

```powershell
python tools\make_bg3_portrait_tokens.py
```

Portrait mapping:

- `unit_arcane_evoker.png` -> `Gale_Model.png`
- `unit_githyanki_warrior.png` -> `Lae'zel_Model.png`
- `unit_berserker.png` -> `Karlach_Model.png`
- `unit_life_cleric.png` -> `Shadowheart_Model.png`
- `unit_shadow_rogue.png` -> `Astarion_Model.png`
- `unit_circle_druid.png` -> `Halsin_Model.png`
- `unit_paladin.png` -> `Minthara_Model.png`
- `unit_ranger.png` -> `Minsc_Model.png`
- `unit_necromancer.png` -> `Balthazar_Model.png`
- `unit_fire_mephit.png` -> `Magma_Mephit_Model.png`
- `unit_imp_swarm.png` -> `Imp_Model.png`
- `unit_goblin_ambusher.png` -> `Goblin_Brawler_Model.png`
- `unit_skeleton.png` -> `Skeleton_Model.png`
- `unit_neutral_spectator_bright.png` -> `Spectator_Art.png`
- `unit_neutral_owlbear.png` -> `Owlbear_Model.png`
- `unit_neutral_mind_flayer.png` -> `Mind_Flayer_Model.png`
- `unit_neutral_sovereign_spaw.png` -> `Spaw_Model.png`
- `unit_neutral_karniss.png` -> `Kar'niss_Model.png`
- `unit_neutral_redcap.png` -> `Redcap_Model.png`
- `unit_shield_guardian.png` -> `Animated_Armour_Model.png`
- `unit_guard_tower.png` -> `Steel_Watcher_Model.png`
- `unit_dragon_wyrmling.png` -> `Red_Dragon_Model.png`
- `unit_treant.png` -> `Wood_Woad_Model.png`
- `unit_neutral_water_myrmidon.png` -> `library/monsters/creatures/water_myrmidon.png`
- `unit_neutral_phase_spider_matriarch.png` -> `library/monsters/creatures/phase_spider_matriarch.png`
- `unit_neutral_raphael.png` -> `library/monsters/creatures/raphael.png`
- `unit_neutral_ketheric_thorm.png` -> `library/monsters/creatures/ketheric_thorm.png`
- `unit_neutral_air_myrmidon.png` -> `library/monsters/creatures/air_myrmidon.png`
- `unit_neutral_guardian_of_faith.png` -> `library/monsters/creatures/guardian_of_faith.png`
- `unit_neutral_minotaur.png` -> `library/monsters/creatures/minotaur.png`
- `unit_neutral_tamia_holzt.png` -> `library/monsters/creatures/tamia_holzt.png`
- `unit_neutral_moonlight_sliver.png` -> `library/monsters/creatures/moonlight_sliver.png`

Effect and ability icons:

- `arcane_burst.png` -> legacy spell burst texture; not used as a Magic Missile
  fallback for arbitrary abilities
- `melee_hit.png` -> `Reckless_Attack_Action_Icon.png`
- `ability_githyanki_astral_raid.png` -> `Astral_Knowledge_Icon.webp`
- `ability_barbarian_heavy_swing.png` -> `Frenzied_Strike_Icon.png`
- `ability_necromancer_summon.png` -> `Animate_Dead_Icon.png`
- `ability_mephit_death_burst.png` -> `Explosive_Explode_Icon.webp`
- `ability_paladin_charge.png` -> `Searing_Smite_Icon.png`
- `ability_dragon_breath.png` -> `Fire_Breath_(Cone)_Icon.png`
- `ability_guardian_shield.png` -> `Guardian_of_Faith_Icon.png`
- `ability_cleric_heal.png` -> `Healing_Word_Icon.png`
- `ability_frost_nova.png` -> `Ice_Storm_Icon.png`
- `ability_rogue_ambush.png` -> `Sneak_Attack_(Melee)_Icon.png`
- `ability_druid_summon.png` -> `Conjure_Animals_Icon.webp`
- `library/skills/spells/fireball.png` -> `Fireball_Icon.webp` from
  `https://bg3.wiki/wiki/Fireball`
- `library/skills/actions/electrified_flail.png` -> `Generic_Lightning_Icon.webp`
  from `https://bg3.wiki/wiki/Electrified_Flail`
- `sources/Magic_Missile_Icon.png` -> `Magic_Missile_Icon.webp` from
  `https://bg3.wiki/wiki/Magic_Missile`
- `ability_mephit_death_burst.png` -> `Generic_Explosion_Icon.webp` from
  `https://bg3.wiki/wiki/Death_Burst`
- `ability_dragon_breath.png` -> `Fire_Breath_Cone_Icon.webp` from
  `https://bg3.wiki/wiki/Fire_Breath_(Cone)`

Relic and expansion library:

- `relics/assigned/relic_*.png` are the stable icons used by the current relic
  draft and owned-relic UI.
- `library/relics/` caches extra Rings, Amulets, Headwear, and Handwear icons.
- `library/skills/` caches spell, action, and illithid-power icons for future
  units and abilities.
- `library/effects/` caches condition and effect icons for combat feedback.
- `library/monsters/` caches creature thumbnails for future encounter content.
- `library/quests/` caches quest and journal imagery for campaign expansion.

All source pages and direct file URLs were resolved from `https://bg3.wiki/`.
