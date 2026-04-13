# ALIS 1.0.0 Release Plan

Budget: **32 human-hours** total (includes 8h final test day).
Dev budget: **24h**. Test/fix budget: **8h**.
Anything that blows the budget gets cut -- no exceptions.

Aligns with [VISION.md](../../VISION.md) and [00_focus.md](00_focus.md).

---

## TOC

- [MUST HAVE](#must-have) -- collision, dialogue, inventory, interaction, rendering (14.5h)
- [SHOULD HAVE](#should-have) -- visual, environment (3h)
- [Out of Scope](#out-of-scope)

---

## MUST HAVE

| # | Task | Source | Hours |
|---|------|--------|-------|
| ~~1~~ | ~~Missing collision on fence near vehicle area~~ | ~~Environment~~ | ~~0.5~~ |
| ~~2~~ | ~~No collisions with UAZ — check collision for all objects~~ | ~~QA~~ | ~~0.5~~ |
| 3 | Dormitory stairs — character passes through, missing collision | Environment | 0.5 |
| 4 | Grandpa says "let me open this door" after door already opened — trigger order broken | Dialogue | 2 |
| 5 | Rewrite grandpa sequence — "show water through peephole" → lock sound → "let me open" | Dialogue | 1.5 |
| 6 | Standing close to grandpa's door when giving water causes spin glitch until you move away | Dialogue | 1 |
| 7 | Hold E "Pick up" should only apply to loot containers, not consumables (water, cigarettes) | Inventory | 2 |
| 8 | No popup info about picked-up item when hovering while holding all items | Inventory | 2 |
| 9 | Grandpa's toilet door opens into player's face — fix open direction | Interaction | 1 |
| 10 | ToiletTable drawer does not open | Interaction | 1 |
| 11 | Lumen striations/stripes visible when moving camera | Rendering | 2 |
| | **Subtotal** | | **14.5** |

## SHOULD HAVE

| # | Task | Source | Hours |
|---|------|--------|-------|
| 12 | Shop z-fighting on left wall + front wall transparent | Environment | 0.5 |
| 13 | Store trash shifted under stage, mouse selection broken | Environment | 0.5 |
| 14 | Tree flickering — delete or fix (disable Nanite on WPO mesh) | Foliage | 0.5 |
| 15 | Bush collapses, WPO not working with Nanite (disable Nanite or set WPO disable distance) | Foliage | 1 |
| 16 | Luxury apartment sofa overlaps closet | Environment | 0.5 |
| | **Subtotal** | | **3** |

**Total: ~17.5h**

---

## Out of Scope

Not 1.0.0. Don't start.

- Interaction: kitchen cabinets — attach doors to base axis coords (1.5h)
- Interaction: toilet/gas stove Y-axis rotation (1h)
- Interaction: remaining cabinets — split into openable mesh parts (2h)
- Landscape: fix road material rotation near buildings + spline directions (1h)
- Landscape: align terrain with shop buildings (1h)
- Landscape: change asphalt tiling, adjust gap and elevation (0.5h)
- Environment: school exterior — improve look (1.5h)
- Environment: garage — improve look (1h)
- Environment: downspouts — migrate to data asset driven approach (2h)
- Environment: Spline_BP/BP2/BP3 — migrate to parent JSON (1.5h)
- Environment: WoodenPalette_BP2 — migrate to JSON randomization (0.5h)
- Landscape: replace landscape layer, export/import, sort (1.5h)
- Interaction: replace legacy templates with current standards (1h)
- Interaction: extend rotation system to 3 axes (fragile code) (2h)
- Interaction: sliding/rotating doors/windows + atmospheric items (2h)
- Interaction: placement in tight spaces (car, truck, under wheel) (1.5h)
- Environment: add spline fence near Khrushchev building (0.5h)
- Environment: add objects — MetalFence, Border, Surface, TrashBin, Playground (2h)
- Vehicles: check refs for car colors, add noise variations (1h)
- Vehicles: add wheels + license plates from City Sample (0.5h)
- Vehicles: study City Sample car setup, replace + optimize materials (2h)
- Landscape: research terrain generation plugin (3D house prototype) (1h)
- Landscape: research road creation tools (1h)
- Landscape: research ground/soil/meadow/rock packs in Test_2 (0.5h)
- Quest system, skill trees, progression
- Vehicles, base building
- Multiplayer
- Map expansion beyond City17 area
- Mod support, console ports
- Localization beyond EN/RU
