# ALIS 1.0.0 Release Plan

Anything that blows the scope gets cut -- no exceptions.

Aligns with [VISION.md](../../VISION.md) and [00_focus.md](00_focus.md).

---

## TOC

- [MUST HAVE](#must-have) -- collision, dialogue, inventory, interaction, rendering, packaging, character, UI
- [SHOULD HAVE](#should-have) -- visual, environment
- [Out of Scope](#out-of-scope)

---

## MUST HAVE

| # | Task | Source |
|---|------|--------|
| ~~1~~ | ~~Missing collision on fence near vehicle area~~ | ~~Environment~~ |
| ~~2~~ | ~~No collisions with UAZ - check collision for all objects~~ | ~~QA~~ |
| ~~3~~ | ~~Dormitory stairs - character passes through, missing collision~~ | ~~Environment~~ |
| ~~4~~ | ~~Grandpa says "let me open this door" after door already opened - trigger order broken~~ | ~~Dialogue~~ |
| ~~5~~ | ~~Rewrite grandpa sequence - "show water through peephole" -> lock sound -> "let me open"~~ | ~~Dialogue~~ |
| ~~6~~ | ~~Standing close to grandpa's door when giving water causes spin glitch until you move away~~ | ~~Dialogue~~ |
| ~~7~~ | ~~Hold E "Pick up" should only apply to loot containers, not consumables (water, cigarettes)~~ | ~~Inventory~~ |
| ~~8~~ | ~~No popup info about picked-up item when hovering while holding all items~~ | ~~Inventory~~ |
| ~~9~~ | ~~Luxury apartment sliding door moves forward instead of sideways - wrong axis~~ | ~~Interaction~~ |
| 10 | ToiletTable drawer does not open | Interaction |
| ~~11~~ | ~~Lumen striations/stripes visible when moving camera ([details](strafe_artifacts_fix.md))~~ | ~~Rendering~~ |
| ~~12~~ | ~~Implement poison gas cloud - environmental hazard via GAS ([details](implement_poison_gas_cloud.md))~~ | ~~Vitals~~ |
| ~~13~~ | ~~Shipping build crash - CCDIK empty array ACCESS_VIOLATION ([details](packaging_crash_hero_fix.md))~~ | ~~Packaging~~ |
| ~~14~~ | ~~Character no animation/mesh in packaged build - assets not cooked ([details](packaging_crash_hero_fix.md))~~ | ~~Packaging~~ |
| ~~15~~ | ~~Broken `BP_Hero` DefaultPawnClass paths in SinglePlayModeDefaults ([details](16.04.26_test_report.md))~~ | ~~Packaging~~ |
| ~~16~~ | ~~Camera behind neck - `relativeOffset` Z=73 needs adjustment ([details](packaging_crash_hero_fix.md))~~ | ~~Character~~ |
| ~~18~~ | ~~Inventory UI: raw definition names instead of display names ([details](inventory_ui_bugs.md))~~ | ~~Inventory~~ |
| ~~19~~ | ~~Inventory UI: icons disappear after drag or re-open ([details](inventory_ui_bugs.md))~~ | ~~Inventory~~ |
| ~~20~~ | ~~Inventory UI: large overlay text on screen ([details](inventory_ui_bugs.md))~~ | ~~Inventory~~ |
| 26 | RTX 3060-class / Medium / 1080p qualification - UNQUALIFIED: no physical adapter available | Runtime/Performance |

## SHOULD HAVE

| # | Task | Source |
|---|------|--------|
| 21 | Shop z-fighting on left wall + front wall transparent | Environment |
| 22 | Store trash shifted under stage, mouse selection broken | Environment |
| 23 | Tree flickering - delete or fix (disable Nanite on WPO mesh) | Foliage |
| 24 | Bush collapses, WPO not working with Nanite (Set WPO disable distance) | Foliage |
| ~~25~~ | ~~Luxury apartment sofa overlaps closet~~ | ~~Environment~~ |

---

## Out of Scope

Not 1.0.0. Don't start.

- Interaction: kitchen cabinets - attach doors to base axis coords
- Interaction: toilet/gas stove Y-axis rotation
- Interaction: remaining cabinets - split into openable mesh parts
- Landscape: fix road material rotation near buildings + spline directions
- Landscape: align terrain with shop buildings
- Landscape: change asphalt tiling, adjust gap and elevation
- Environment: school exterior - improve look
- Environment: garage - improve look
- Environment: downspouts - migrate to data asset driven approach
- Environment: Spline_BP/BP2/BP3 - migrate to parent JSON
- Environment: WoodenPalette_BP2 - migrate to JSON randomization
- Landscape: replace landscape layer, export/import, sort
- Interaction: replace legacy templates with current standards
- Interaction: extend rotation system to 3 axes (fragile code)
- Interaction: sliding/rotating doors/windows + atmospheric items
- Interaction: placement in tight spaces (car, truck, under wheel)
- Environment: add spline fence near Khrushchev building
- Environment: add objects - MetalFence, Border, Surface, TrashBin, Playground
- Vehicles: check refs for car colors, add noise variations
- Vehicles: add wheels + license plates from City Sample
- Vehicles: study City Sample car setup, replace + optimize materials
- Landscape: research terrain generation plugin (3D house prototype)
- Landscape: research road creation tools
- Landscape: research ground/soil/meadow/rock packs in Test_2
- Quest system, skill trees, progression
- Vehicles, base building
- Multiplayer
- Map expansion beyond City17 area
- Mod support, console ports
- Localization beyond EN/RU
