# Inventory UI Layout Improvement

UX redesign to match survival-game standards (Tarkov, DayZ, RE4).

**Status**: DONE

**Related**:
- [design_vision.md](../../Plugins/Features/ProjectInventory/docs/design_vision.md)
- [implement_inventory_vision.md](implement_inventory_vision.md)
- [agent_ue_inspection.md](../../docs/testing/agent_ue_inspection.md)

---

## Completed (2026-02-05)

| Task | Status | Notes |
|------|--------|-------|
| Two-column layout (Left: equip, Right: grids) | DONE | JSON restructured |
| Equipment panel body-position boxes | DONE | VerticalBox + HorizontalBox rows |
| Grid sizing with SizeBox wrappers | DONE | 280x280 min size |
| Container tabs | DONE | Already working |
| Bottom bar (selection info + controls) | DONE | Reorganized layout |
| Keyboard navigation | DONE | Arrow/Tab/Enter/R keys |
| Rotation state feedback | DONE | Shows On/Off text |
| Multi-resolution tests | PASS | 720p/1080p/1440p/Ultrawide |

**Verification**: `Widgets: 103  Issues: 0` at all resolutions

---

## Current Layout

```
+------------------+------------------------+
|   LEFT PANEL     |      RIGHT PANEL       |
|                  |                        |
|  Inventory       | [Tab1] [Tab2]          |
|  Weight/Volume   | +--------------------+ |
|                  | |                    | |
|  [HEAD] [ACCESS] | |   GRID (280x280)   | |
|  [L.H][CHEST][BK]| |                    | |
|       [R.H]      | +--------------------+ |
|      [LEGS]      |                        |
|      [FEET]      |                        |
|                  |                        |
| L.Hand | R.Hand  |                        |
| [2x2]  | [2x2]   |                        |
+------------------+------------------------+
| [Icon] Item Name / Stats                  |
+-------------------------------------------+

Context menu (on click):
+------------------+
| Item Name      X |
|------------------|
| Use              |
| Equip            |
| Drop             |
+------------------+
```

---

## Keyboard Controls

| Key | Action |
|-----|--------|
| Arrow keys | Navigate grid cells |
| Tab | Switch between primary/secondary grid |
| R | Toggle rotation mode |
| Enter | Use selected item |

---

## Future Improvements

### Hand Grid System (KISS)

**Concept**: Each hand = 2x2 grid (palm grip zone ~10cm x 10cm)

```
LEFT HAND (2x2)     RIGHT HAND (2x2)
+----+----+         +----+----+
|    |    |         |    |    |
+----+----+         +----+----+
|    |    |         |    |    |
+----+----+         +----+----+

Holding rifle (2x5) - item overflows visually:
     [  ]  <- overflow (not clickable)
     [  ]
+----+----+
| ## | ## | <- grip zone (interactive)
+----+----+
| ## | ## |
+----+----+
     [  ]  <- overflow
```

**Rules**:
- Hand = small container (2x2 cells = 10cm x 10cm)
- ANY item can fit (rifle for carrying, not firing)
- Tall items overflow visually - only grip zone is shown
- Reuses existing grid/container system

**Implementation**:
- [x] Data: Added `Item_Container_LeftHand` and `Item_Container_RightHand` tags
- [x] Validation: Added `bWidthOnlyValidation` property to `FInventoryContainerConfig`
  - Modified `IsRectWithinContainer()` to check width only when flag is set
  - Hand containers in default grants use `bWidthOnlyValidation = true`
  - **MaxCells = 1**: One item per hand (prevents multiple small items)
  - **Height clamp in overlap tests**: `DoesRectOverlap()` clamps effective height to container bounds
- [x] UI: Added `LeftHandGridHost` and `RightHandGridHost` in `InventoryPanel.json`
  - 72x72 SizeBox with `clip: ClipToBoundsWithoutIntersecting` for overflow
- [x] ViewModel: Added `LeftHandCellTexts`, `RightHandCellTexts`, instance ID getters
- [x] Visual: Clipping handles overflow (tall items show grip zone only)
- [x] Keyboard focus: `SetIsFocusable(true)` in constructor, `SetFocus()` on show

**Files modified**:
- `ProjectGameplayTags.h/cpp` - Added LeftHand/RightHand container tags
- `ProjectInventoryComponent.h/cpp` - `bWidthOnlyValidation`, `MaxCells=1`, height clamp
- `InventoryPanel.json` - HandsPanel with LeftHand/RightHand grids
- `InventoryPanelBindings.h/cpp` - LeftHandGridHost, RightHandGridHost
- `InventoryViewModel.h/cpp` - Hand grid cell texts and instance IDs
- `W_InventoryPanel.h/cpp` - Hand grid member pointers

**Minimal code**: one validator branch + occupancy clamp for hand containers.

### Code Quality

- [x] SOLID refactor W_InventoryPanel.cpp (994 -> 616 lines orchestrator)
  - `FInventoryPanelTextUpdater` (157 lines) - text/stats/command button updates
  - Existing helpers: GridBuilder, VisualState, HitDetector, DragDropHandler, PanelState
  - Main class is now thin orchestrator delegating to 6 helper classes

### Item Context Menu (TODO)

**Concept**: Click/right-click item -> floating context menu near cursor

```
+------------------+
| Item Name      X |
|------------------|
| Use              |  <- only if consumable
| Equip            |  <- only if equippable
| Drop             |  <- always
| Split (5)        |  <- only if stackable qty > 1
+------------------+
```

**Flow**:
- Click/right-click on item -> show context menu at cursor position
- Drag item -> no menu, just drag visual
- No selection -> clean UI

**Implementation DONE**:
- [x] Create `ItemContextMenu.json` layout
- [x] Create `W_ItemContextMenu.h/cpp` widget class
- [x] Wire to item cell click in `W_InventoryPanel::HandleCellClicked()`
- [x] Filter actions by item type (equippable/stackable/droppable)
- [x] Position menu near cursor (avoid screen edge clipping)
- [x] Close on: X button, ESC key
- [x] Split stack action (`RequestSplitStack` in IInventoryCommands/ViewModel/Component)
- [x] Click outside to close (invisible ClickCatcher button behind menu)

**Files to modify**:
- `Plugins/UI/ProjectInventoryUI/Data/ItemContextMenu.json` - layout
- `Plugins/UI/ProjectInventoryUI/Source/.../W_ItemContextMenu.h/cpp` - widget
- `W_InventoryPanel.cpp` - show context menu on click

### Visual Feedback (Polish)
- [x] Drag preview colors (valid=green, invalid=red) - existing in FInventoryGridVisualState
- [x] Drag preview animation/pulse for invalid zones
  - Added time-based pulse (2Hz, 0.5-1.0 alpha) in `FInventoryGridVisualState::ApplyToGrid()`
  - Invalid preview lerps between base and error color using `FPlatformTime::Seconds()`
- [x] Quantity indicator during drag
  - Creates `UTextBlock` as `DefaultDragVisual` in `NativeOnDragDetected()`
  - Shows "ItemName x{qty}" with theme's `TextPrimary` color

### Test Robustness (from code review)

**P1 - Prevents silent bugs:**
- [x] Assert all 8 equip slot tags are valid in dump test
  - Added `ValidateEquipSlotTags()` in `ProjectUIInventoryDumpTreeTest.cpp`
  - Fails test with error message if any tag is not registered

**P2 - Better agent debugging:**
- [x] Add screenshot artifact to layout dump test
  - Uses `FScreenshotRequest::RequestScreenshot()` in both DumpTree and MultiResolution tests
  - Outputs: `Dumps/Inventory_screenshot.png`, `Dumps/Inventory_{resolution}_screenshot.png`

**P3 - Script correctness:**
- [x] Fix ue_path.conf regex to accept both `=` and `:=`
  - Updated `scripts/ue/standalone/build.ps1` regex to `^([A-Z_]+)\s*:?=\s*(.+)$`
  - Also strips quotes from value

---

## Files Modified

| File | Changes |
|------|---------|
| `Data/InventoryPanel.json` | Two-column layout, SizeBox grid wrappers |
| `InventoryPanelBindings.h/cpp` | UVerticalBox for EquipSlotsHost |
| `InventoryPanelGridBuilder.h/cpp` | Body-position box layout for equip slots |
| `W_InventoryPanel.h` | UVerticalBox type for EquipSlotsHost |
| `W_InventoryPanel.cpp` | Keyboard navigation, improved rotation feedback |

---

## Verification Commands

```powershell
# Multi-resolution test
.\scripts\ue\test\unit\run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.UI.Layout.InventoryHands.MultiResolution" -Game -Map "/MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent"

# Analyze
python tools/agentic/ui/layout_report.py Saved/Dumps/Inventory_1080p.json
```
