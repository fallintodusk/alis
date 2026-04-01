# ALIS 1.0.0 Release Plan

Budget: **32 human-hours** total (includes 8h final test day).
Dev budget: **24h**. Test/fix budget: **8h**.
Anything that blows the budget gets cut -- no exceptions.

Aligns with [VISION.md](../../VISION.md) and [00_focus.md](00_focus.md).

---

## TOC

- [MUST HAVE](#must-have) -- ship blockers, fits in 24h dev
- [SHOULD HAVE](#should-have) -- include if ahead of schedule
- [NICE TO HAVE](#nice-to-have) -- post-1.0 unless free hours remain
- [Out of Scope](#out-of-scope)

---

## MUST HAVE

Without these, no release. Target: 24h dev total.

| # | Feature | Complexity | Hours (dev+debug+test) |
|---|---------|------------|------------------------|
| 1 | FP body visible, no camera clipping | High | 6 |
| 2 | Locomotion: walk, sprint, crouch, jump | Med | 2 |
| 3 | Vitals drain + affect movement + death | Med | 3 |
| 4 | Vitals HUD | Low | 1 |
| 5 | Pick up / drop items from world | Med | 3 |
| 6 | Inventory panel (grid, rotate, weight) | Med | 2 |
| 7 | Equip grants (pockets/backpack) | Med | 2 |
| 8 | Interaction prompt (look-at + use) | Low | 1 |
| 9 | City17 walkable, no holes/T-poses | Med | 2 |
| 10 | Lootable objects in environment | Low | 1 |
| 11 | Boot flow: menu -> game -> pause -> quit | Low | 0.5 |
| 12 | Settings (resolution, quality, audio) | Low | 0.5 |
| | **Subtotal** | | **24** |

---

## SHOULD HAVE

Worth it if MUST is done early. Not ship blockers.

| # | Feature | Complexity | Hours |
|---|---------|------------|-------|
| 13 | Micro-objective (reach shelter / find water) | Med | 3 |
| 14 | Ambient sound + footstep surfaces | Low | 2 |
| 15 | Day/night or weather (one variant) | Med | 2 |
| 16 | Item tooltips + weight warning | Low | 1 |
| 17 | Death screen with restart | Low | 1 |
| 18 | Doors open/close | Low | 1 |

---

## NICE TO HAVE

Cut without guilt. Post-1.0 patches.

| # | Feature | Complexity | Hours |
|---|---------|------------|-------|
| 19 | One NPC with dialogue | High | 4 |
| 20 | Melee combat (one weapon) | Med | 3 |
| 21 | Save/load | High | 4 |
| 22 | Crafting (one recipe) | Med | 2 |
| 23 | Interior spaces | Med | 3 |

---

## Out of Scope

Not 1.0.0. Don't start.

- Quest system, skill trees, progression
- Vehicles, base building
- Multiplayer
- Map expansion beyond City17 area
- Mod support, console ports
- Localization beyond EN/RU
