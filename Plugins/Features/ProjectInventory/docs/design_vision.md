# Inventory Design Vision

Game design document for ALIS inventory system - realistic survival inventory inspired by Escape from Tarkov, DayZ, and SCUM.

## Core Philosophy

**Realism over convenience.** Every item has physical properties (volume, mass) that affect how and where it can be carried. Players make meaningful choices about what to bring.

## Reference Units

| Unit | Value | Purpose |
|------|-------|---------|
| **Cell reference** | 5x5 cm | Authoring guideline for item sizing |
| **Depth unit** | abstract | Stack height constraint, not physical cm |

**Key rules:**
- Grid cell is an **abstract slot unit** for UX, not a physics promise
- Reference mapping: 1 cell ~ 5x5 cm (for authoring consistency only)
- Items occupy **whole cells only** - partial cell space is intentionally wasted
- Stacking is per-item (`MaxStack`, `UnitsPerDepthUnit`), **not derived from centimeters**

**Example sizing guidance:**
| Real-world item | Approx dimensions | Grid size |
|-----------------|-------------------|-----------|
| Coin/key | <5cm | 1x1 |
| Bandage/ammo box | ~5-10cm | 1x1 or 1x2 |
| Water bottle | ~8x20cm | 1x2 |
| Pistol | ~15x10cm | 2x2 |
| Rifle | ~25x100cm | 2x5 |

## Grid System (Tetris-Style)

Inspired by Escape from Tarkov, DayZ, Resident Evil 4.

### Cell-Based Storage
- All containers use a **uniform cell size** (same cell = same size everywhere)
- Items occupy rectangular areas (e.g., 1x1, 2x1, 2x3)
- Items can be **rotated 90 degrees** (R key) to fit different orientations
- Larger items cannot fit in small containers regardless of total capacity
- Items span multiple cells (true Tetris-style, not slot-based)

### Item Sizes (Examples)
| Item | Grid Size | Notes |
|------|-----------|-------|
| Bandage | 1x1 | Fits anywhere |
| Water bottle | 1x2 | Vertical, fits in pocket |
| Pistol | 2x2 | Small weapon |
| Rifle | 5x2 | Large, backpack only |

### Container Grid Sizes
| Container | Grid | Total Cells | Restrictions |
|-----------|------|-------------|--------------|
| Hand (x2) | 2x2 | 4 each | None; tall items overflow visually |
| Pants pocket (x2) | 2x2 | 4 each | Small items (max 2x2) |
| Jacket pocket (x2) | 2x3 | 6 each | Medium items (max 2x3) |
| Small backpack | 4x5 | 20 | None |
| Large backpack | 6x8 | 48 | None |

### Container Restrictions
- **Cell-based only**: Items must fit within container grid dimensions
- **No tag restrictions** (for now): No "liquids only" or "ammo only" pockets
- A 2x3 item CAN fit in a 4x2 pocket if rotated to 3x2
- Future: may add AllowedTags per container for specialized pouches

## Depth Stacking (KISS Voxel Model)

**Two-layer approach:** 2D grid for UX, constrained 3D depth for small items only.

### Why Not Full 3D Packing
- 3D bin packing is NP-hard - leads to unpredictable placement or forced autosort
- More realism -> more edge cases -> player frustration (Cataclysm: DDA pattern)
- 2D grid inventory is proven UX standard (Tarkov, RE4)

### The Rule: Depth Only for 1x1 Items
- Items with `gridW*gridH > 1` -> classic 2D placement only (no sharing cells)
- Items with `gridW=1 && gridH=1` -> depth constrains stack height in that cell
- **MVP: One stack per cell** (no multi-item-per-cell complexity)

This gives "3D realism" for small clutter (coins, ammo, meds, keys) without full 3D packing complexity.

### Container Properties
| Property | Description | Example |
|----------|-------------|---------|
| `gridW`, `gridH` | 2D grid dimensions | 4x5 |
| `cellDepthUnits` | Max stack height per cell | 4 (default 1) |
| `maxWeightKg` | Optional weight limit | 15.0 |
| `maxVolumeUnits` | Optional, can derive from gridxdepth | 80 |

### Item Properties
| Property | Description | Example |
|----------|-------------|---------|
| `gridW`, `gridH` | Footprint in cells | 1x1, 2x3 |
| `unitsPerDepthUnit` | Quantity per depth unit (1x1 only) | 20 (coins), 30 (ammo) |
| `maxStack` | Max quantity per stack | 60 (ammo), 1 (weapon) |
| `unitWeight`, `unitVolume` | Per-unit physical cost | 0.01kg, 0.5 |

### Placement Rules (MVP: One Stack Per Cell)
```
if item.gridW == 1 && item.gridH == 1:
    # Depth-limited stacks
    depthUsed = ceil(quantity / item.unitsPerDepthUnit)
    can_place = cell.is_empty && depthUsed <= container.cellDepthUnits
else:
    # Classic 2D occupancy only
    can_place = all_cells_free(x, y, item.gridW, item.gridH)
```

### Stacking vs Grid Placement
- **Grid placement**: Physical space occupation (Tetris-style)
- **Stacking**: Quantity abstraction for identical items
- A stack of 60 bullets is ONE item in ONE cell, consuming depth based on quantity
- `totalWeight = unitWeight x quantity`
- `totalVolume = unitVolume x quantity`
- `depthUsed = ceil(quantity / unitsPerDepthUnit)`

**Future v2:** Multi-item-per-cell would require new UI + validation - not MVP.

### UI Layers
- **Default view**: Tarkov-like - show item icons, stack counts
- **Advanced view** (optional): Per-cell depth meter for debugging/power users

## Weight System

### Soft Limits with Penalties
- **Current**: Hard cap at MaxWeight (pickup blocked when full) - safety limit
- **Target**: Add soft penalties above threshold (~80%) via GAS; keep hard cap as absolute safety
- **Overweight penalties** (planned) scale with excess weight:
  - Stamina drain increases
  - Movement speed decreases
  - Sprint duration reduced

### Dynamic Capacity (GAS Integration)
- Max comfortable carry weight affected by character state:
  - Healthy: 100% capacity
  - Fatigued: 80% capacity
  - Injured: 60% capacity
  - Exhausted: 40% capacity
- State.* tags from ProjectVitals drive these modifiers

### Weight Examples
| Item | Weight (kg) |
|------|-------------|
| Bandage | 0.05 |
| Water bottle (full) | 0.5 |
| Canned food | 0.4 |
| Pistol | 0.8 |
| Rifle | 3.5 |
| Backpack (empty) | 1.2 |

## Container Grants (Equipment-Based)

### Base State (Naked)
- **Hands only**: 2 slots for held items
- No pockets, no storage

### Equipment Grants
| Equipment | Grants | Grid Size |
|-----------|--------|-----------|
| Basic pants | Pockets1, Pockets2 | 2x2 each |
| Cargo pants | Pockets1, Pockets2 | 3x3 each |
| Light jacket | Pockets3, Pockets4 | 2x3 each |
| Tactical vest | Pockets3, Pockets4, MagPouch | 2x3, 2x3, 1x4 |
| Small backpack | Backpack | 4x5 |
| Large backpack | Backpack | 6x8 |

### Grant Rules
- Equipment defines containers via ObjectDefinition.Item.ContainerGrants
- Unequip blocked if granted container has items (with UI popup)
- Items must fit in granted container's grid dimensions
- **Backpack access**: Can access backpack contents without removing it (no "take off to access" mechanic)
- **No nesting**: Backpacks cannot be stored inside other backpacks (prevents recursion exploits)

### UI Container Layout Contract (SOT)

Inventory UI must use one generic container descriptor model for all granted containers.
Do not implement per-clothing or per-item widget paths.

Required descriptor fields:
- `ContainerId`
- `Label`
- `GridWidth`
- `GridHeight`
- `LayoutGroup` (TopCompact or BottomLarge)
- `OrderKey`

Layout behavior:
- Hands are always visible and separate from descriptor-driven storage.
- `TopCompact` containers are rendered to the right of hands.
- `BottomLarge` containers are rendered in the lower right storage area.
- Backpack is a `BottomLarge` container and is ordered first within that group.
- Pockets are `TopCompact` and ordered by canonical id: `Pockets1`, `Pockets2`, `Pockets3`, `Pockets4`.
- If no descriptors exist for a layout group, collapse that host (no empty placeholder panels).

Canonical progression examples:
- Naked -> hands only, no compact or large storage hosts visible.
- Pants equipped -> `Pockets1`, `Pockets2` appear in `TopCompact` to the right of hands.
- Jacket/vest equipped -> additional compact descriptors (for example `Pockets3`, `Pockets4`) appear after existing compact pockets by `OrderKey`.
- Backpack equipped -> `Backpack` descriptor appears in `BottomLarge` and renders below hands/compact row.

This keeps rendering logic universal:
- any equipped item can grant any grid size (2x2, 1x3, 4x5, 6x8, etc.)
- UI placement is data-driven by descriptor group/order, not by special-case code

## Hands (Separate System)

### Always Available
- 2 hands, always accessible
- Not affected by equipment
- Used for: carrying items, held weapon/tool

### Hand Grid (KISS Design)

Each hand is a **2x2 grid** representing the palm grip zone (~10cm x 10cm).

```
LEFT HAND (2x2)     RIGHT HAND (2x2)
+----+----+         +----+----+
|    |    |         |    |    |
+----+----+         +----+----+
|    |    |         |    |    |
+----+----+         +----+----+
```

**Grip rules:**
- Width constraint: 2 cells (~10cm) - realistic grip radius
- Height: items can overflow visually (holding tall objects like bottles, rifles)
- Any item CAN fit - even rifle (for carrying, not firing)

**Example - holding rifle (2x5):**
```
     [  ]  <- overflow (visual only)
     [  ]
+----+----+
| ## | ## | <- grip zone (2x2 interactive area)
+----+----+
| ## | ## |
+----+----+
     [  ]  <- overflow
```

**Implementation:**
- Hands = 2 containers with `gridW=2, gridH=2`
- **Special validation rule**: hands check `item.gridW <= 2` only, ignore height
- UI shows only grip zone; oversized items clip or show partial
- Items placed at grid position (0,0) in hand container

**Validation code (minimal addition):**
```cpp
bool CanPlaceInHand(const FItem& Item, const FContainer& Hand)
{
    // Hands only constrain width (grip), not height
    return Item.GridW <= Hand.GridW;
}
```

### Hand Slot Rules
- Quick-swap between hands (hotkey)
- Dropping from hands = immediate world drop
- Tall items overflow visually (rendered outside grid bounds)

## UI Interactions

### Drag and Drop
- Drag items between containers
- R key rotates during drag
- Invalid placement shows red outline
- Valid placement shows green outline

### Quick Actions
- Ctrl+Click: quick move to first available slot
- Shift+Click: split stack (quantity picker)
- Double-click: use/equip item

### Feedback
- Overweight indicator on HUD
- Popup text when action blocked (e.g., "Cannot unequip - pockets not empty")
- Capacity bar per container

## Design References

- [Escape from Tarkov](https://www.escapefromtarkov.com/) - Grid inventory, item rotation, weight penalties
- [DayZ](https://dayz.com/) - Clothing-based storage, realistic weight
- [SCUM](https://scumgame.com/) - Metabolism integration, detailed character stats
- [Resident Evil 4](https://www.residentevil.com/) - Tetris-style inventory management

## Research Sources

- [Games with Realistic Carry Capacity](https://gamerant.com/games-realistic-carry-capacity-equip-load-systems/)
- [Tarkov Item Rotation Guide](https://twinfinite.net/guides/escape-from-tarkov-rotate-items/)
- [Tarkov Inventory Optimization](https://www.gamedoggy.com/game-guides/escape-from-tarkov/how-to-optimize-your-inventory-management-in-escape-from-tarkov)
- [Jigsaw Inventory System (UE)](https://www.fab.com/listings/86a1fce7-2019-4096-954c-49071274706e)
