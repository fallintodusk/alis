# ProjectInventoryUI Architecture

> See also: [docs/agents/canonical.md](../../../../docs/agents/canonical.md) for agent/dev quick reference.

Behavior SOT
- Inventory behavior and the container layout contract live only in
  `../../../Features/ProjectInventory/docs/design_vision.md`.
- This doc covers UI implementation split and framework ownership only.
- Do not restate inventory behavior here.

## Overview

Inventory UI follows SOLID principles with single-responsibility components and structured observability.

## Widget Decomposition

Two widgets, both registered under `UI.Layer.Menu`, both binding the same
`UInventoryViewModel` (scope: PerPlayer, vm_creation: Global).

| Widget | Anchoring | Visibility rule |
|---|---|---|
| `UW_InventoryPanel` | Center | `VM.bPanelVisible` |
| `UW_NearbyContainerPanel` | Right-center | `VM.bPanelVisible && VM.bHasNearbyContainer`; else Collapsed |

Rules:
- Nearby surface MUST NOT live inside the main panel's layout tree. The
  two widgets are independent layer-host registrations - opening a loot
  session does not reflow the main panel.
- Both widgets read cell/padding geometry from `FInventoryUISettings::Get()`
  (see Cell Size Contract below). Neither widget's layout JSON carries
  `settings.cellSize`.
- Both widgets share the grid builder class `FInventoryPanelGridBuilder`
  but each constructs its own instance (per-widget mouse-down handler
  via `SetCellMouseDownHandler`).
- When nearby is inactive, the widget's root is `Collapsed` - no
  reserved layout footprint.

## Surface Identity and Drag Host

Drag/drop resolution is tag-indexed through
`FProjectUIGridDragDropController` (ProjectUI, generic). Surface identity
uses the existing global tag taxonomy in `ProjectGameplayTags.h`:
`Item.Container.Hands` (parent), `LeftHand`, `RightHand`,
`Item.Container.Pockets` (parent), `Pockets1..4`, `Backpack`,
`WorldStorage`.

- ProjectUI stays domain-agnostic: it accepts an `FGameplayTag` per
  registered grid and does not know what inventory semantics any tag
  carries.
- ProjectInventoryUI supplies tags via `FProjectUIGridSurface` when
  registering with `UInventoryUIDragHostSubsystem` (ULocalPlayerSubsystem
  owning one `FProjectUIGridDragDropController` per local player).
- Priorities come from
  `Plugins/UI/ProjectInventoryUI/Public/Interaction/InventoryUISurfacePriority.h`
  (`PlayerStorage=0`, `NearbyWorldStorage=10`, `ModalOverlay=100`).
  **Widgets must not pass raw integers** - pick a constant from that
  header, or add a new one there.
- Surfaces are stable-sorted on register by priority descending; ties
  preserve registration order. `FProjectUIGridDragDropController::GetSurfaceTagsInPriorityOrder()`
  is the diagnostic accessor used by the regression test
  `ProjectIntegrationTests.UI.Framework.GridDragDrop.SurfacePriorityOrdering`.

Lifecycle:
- Widgets call `UInventoryUIDragHostSubsystem::RegisterSurface(...)` on
  construct / rebuild, `UnregisterSurface(Tag)` on destruct. The
  subsystem outlives any single widget, so there is no cross-widget
  lookup and no "register on next tick" hack.

Cross-widget drag:
- Both widgets register their grids with the shared subsystem controller
  (tag-based). UMG dispatches `NativeOnDragOver`/`NativeOnDrop` to
  whichever widget is under the cursor. Each widget queries the shared
  controller for preview and drop resolution, then routes through
  `FInventoryDropRouter::Route`. Source widget and target widget never
  reference each other directly - the subsystem + tag API + router
  carry the handoff.
- Drag-FROM the nearby widget is wired via
  `UW_NearbyContainerPanel::NativeOnDragDetected`, which reads the
  pressed cell through the subsystem controller and builds a
  `UInventoryDragDropOperation` with `FromContainer =
  Item.Container.WorldStorage`. Dropping back onto the world grid, or
  onto any player-side grid/hand/pocket, routes through the router.

Two distinct controller APIs, do not conflate them:
- `ResolveSurfaceCellAtScreenPos(ScreenPos, OutTag, OutCol, OutRow)` -
  pure source-side hit test for "which cell is under the cursor?". No
  payload, no occupancy validation. Use at drag-start.
- `ResolveDropTargetOverSurfaces(ScreenPos, Payload, ...)` - hit test
  PLUS footprint validation (bounds, enabled, occupancy rule). Use at
  drop-resolve.

Mixing them means drag-start refuses to pick up occupied cells - which
is every drag source. Regression: `ResolveSourceIgnoresOccupancy` test.

## Drop Command Routing

`FInventoryDropRouter::Route(VM, FInventoryDragContext, FInventoryDropTarget)`
is the single SOT that maps a resolved drag/drop pair to the correct VM
method.

**Current state (Slice 6e complete):**

- `W_NearbyContainerPanel::NativeOnDrop` routes world-target drops
  through the router.
- `W_InventoryPanel::NativeOnDrop` routes grid drops through the router.
  Hand and pocket drops also route through the router after their
  specialized inline validation (hand slot availability, pocket cell
  enabled/occupied toasts) has passed.
- Equip-slot drops stay inline (`VM.RequestEquipItem`) because equip
  slots are not container grids and the router fail-closes on non
  `Item.Container.*` target tags.
- Both widgets share a single `FProjectUIGridDragDropController` owned
  by `UInventoryUIDragHostSubsystem` (per-player); no widget holds a
  local controller any more.

Routing (tag-match, parent tags catch children):
- Target `MatchesTag(Item.Container.WorldStorage)` and source is player-side
  -> `VM.RequestStoreItemInNearbyContainerAt(...)`.
- Source `MatchesTag(Item.Container.WorldStorage)` and target is player-side
  -> `VM.RequestTakeNearbyItemToContainer(...)`.
- Both player-side (hands, pockets, backpack, ...)
  -> `VM.RequestMoveItem(...)`.
- World -> world is not a supported flow and returns false.

Fail-closed: both source and target tags MUST match `Item.Container`
parent or the router returns false without touching the VM. This keeps a
future non-inventory surface accidentally registered with the drag host
from silently being mapped to `RequestMoveItem`. Tests:
- `ProjectIntegrationTests.UI.Framework.Inventory.DropRouterDispatchesByTag`
  (dispatch + fail-closed semantics via spy VM).
- `ProjectIntegrationTests.UI.Framework.Inventory.MultiSurfaceDragResolvesCorrectTag`
  (three-surface registration + priority overlap).
- `ProjectIntegrationTests.UI.Framework.GridDragDrop.PlayerGridTagReRegistersOnTabChange`
  (tab switch tag churn).
- `ProjectIntegrationTests.UI.Framework.Inventory.NearbyPanelIsIndependentlyAnchored`
  (main panel width stable across nearby session open/close).

## Data Flow Contract

ProjectInventoryUI consumes resolved inventory read models. It does not resolve
object definitions, hold asset residency handles, or infer item semantics from
assets.

Canonical flow:
```
ProjectInventory rules/cache
  -> FInventoryContainerView and FInventoryEntryView
  -> UInventoryViewModel
  -> FInventoryCellVisualState arrays
  -> W_InventoryPanel and InventoryPanelGridBuilder
  -> generic ProjectUI grid cells/widgets
```

The view model owns presentation projection. Widgets render already-built
visual state and route user intent back through view-model command methods.
Visual DTO arrays are the only cell presentation path. Do not keep parallel
legacy text arrays or compatibility setters.

## Component Responsibilities

| Component | Lines | Responsibility |
|-----------|-------|----------------|
| W_InventoryPanel.cpp | ~788 | Orchestrator - lifecycle, callbacks, input routing |
| InventoryPanelGridBuilder.cpp | ~268 | Grid/tab/slot UI construction |
| ProjectUIGridDragDropController.cpp | ~220 | Shared drag preview and drop resolution (ProjectUI) |
| InventoryPanelState.cpp | ~28 | Selection/hover/quantity state |
| ProjectUIGridVisualState.cpp | ~170 | Shared cell color state machine (ProjectUI) |
| ProjectUIGridHitDetector.cpp | ~170 | Shared hit detection for grids/slots (ProjectUI) |
| InventoryViewModel.cpp | ~697 | Data binding and queries |
| InventoryViewModelCellBuilder.cpp | varies | Entry views -> cell visual DTOs |

**Refactoring Summary:** Before: 2 monolithic files (1698 + 750 = 2448 lines). After: 7 focused files (~2227 lines total).

## SOLID Principles Applied

| Principle | Implementation |
|-----------|----------------|
| **SRP** | Each class has ONE responsibility |
| **OCP** | Visual states, slot positions are data-driven |
| **LSP** | N/A for current structure |
| **ISP** | Clean interfaces between components |
| **DIP** | Panel depends on abstractions (GridBuilder, shared drag subsystem + router) |

## Observability

- UE_LOG statements at: construct, visibility changes, property changes, grid rebuilds, click handlers, drag/drop
- Log category: `LogInventoryPanel`
- Verbose level for frequent events (mouse move, cell clicks)
- Log level for user actions (equip, drop, use)
- Dump automation should include cell visual arrays and resolved display
  payloads, not only widget tree geometry.

## Cell Visual State Contract

Inventory cells use `FInventoryCellVisualState` instead of a single text value.
This keeps stack count, icon/text, and occupancy separate.

Fields and meaning:
- `InstanceId`: stable runtime entry id, or `EmptyCellInstanceId`.
- `PrimaryText`: icon glyph when available, otherwise fallback display text.
- `QuantityText`: stack count text for quantity badges.
- `bUseIconFont`: render `PrimaryText` with the icon font.
- `bShowQuantity`: show the quantity badge overlay.
- `bIsAnchorCell`: true only for the top-left cell of a multi-cell item.

Rendering rules:
- Anchor cells render the primary layer and optional quantity badge.
- Spill cells keep occupancy through `InstanceId` but do not duplicate primary
  text or quantity badges.
- Widgets must not choose icons by loading object definitions. They render the
  visual DTO supplied by the view model.
- Do not add legacy text-array mirrors. Cell presentation state should have one
  writable shape: the visual DTO supplied by the view model.

## Cell Size Contract

Every cell in the inventory panel (hand grids, pocket grids, backpack primary,
secondary storage, nearby loot, equip slots) renders at the same visible pixel
size. Sizing drift across grids is a bug.

Single SOT:
- `Plugins/UI/ProjectInventoryUI/Data/InventoryUISettings.json` (currently
  `{cellSize: 64, gridSlotLineWidth: 1, cellInnerPadding: 4, hostOuterPadding: 4}`).
- Loaded once per process via `FInventoryUISettings::Get()`; falls back
  to in-code defaults with a warning log if the file is missing or
  malformed. Every inventory-facing widget reads from the same loader -
  no per-widget layout JSON carries `cellSize`.

Runtime flow:
- `W_InventoryPanel::NativeConstruct` reads `FInventoryUISettings::Get().CellSize`
  into `CachedCellSize` and forwards it to `FInventoryPanelGridBuilder::SetCellSize`.
- `W_NearbyContainerPanel::NativeConstruct` reads the same settings
  helper so both widgets render cells at identical size without any
  cross-file dependency.
- `UW_InventoryPanel::ApplyGridHostSize(Host, W, H)` computes the host pixel
  size via `FInventoryPanelGridBuilder::ComputeGridHostPixelSize(W, H, CachedCellSize)`
  and applies `SetWidthOverride` / `SetHeightOverride`.
- `RebuildGrids` calls it for primary, secondary, and nearby hosts.
- `RebuildHandGrids` calls it for both hand hosts at `(2, 2)`.
- Pocket grids auto-size from cell count and inherit the same per-cell
  `CellSize` through `BuildGrid`.

Formula (owned by `FInventoryPanelGridBuilder`):
- `CellPitch = CellSize + 2 * GridSlotLineWidth`
- `HostPixelSize(W, H) = FIntPoint(CellPitch * W, CellPitch * H) + 2 * GridHostOuterPadding`
- `GetCellFrameOverhead() = 2 * (GridSlotLineWidth + GridCellInnerPadding)`,
  used by `UpdateGridVisuals` so icon/text font size stays proportional.

Rules:
- Do not add fixed `size` entries for grid host SizeBoxes in the JSON; the
  runtime drives those overrides.
- Do not hardcode pixel constants (`140`, `280`, `320`, etc.) inside panel code.
- When adding a new grid surface, bind its SizeBox as `USizeBox*` and size it
  through `ApplyGridHostSize`.
- Regression guard: `ProjectIntegrationTests.UI.Framework.Inventory.GridSizing.*`.

## Drag, Stack, and Tooltip Presentation

- Default drag starts with the whole stack quantity. The panel preserves an
  explicit quantity override only while the same entry remains selected.
- Partial-stack movement is an explicit split flow. Invalid split quantities
  and blocked drops surface as toast messages instead of silent failures.
- Occupied-cell stack preview uses `FInventoryUIDropStackPolicy`. It accepts
  same-item 1x1 entries only when the target visual entry has enough remaining
  stack capacity. ProjectInventory still owns the authoritative move result.
- `ProjectUI` grid drag/drop accepts a feature-supplied occupancy predicate.
  InventoryUI supplies stack-preview meaning through that predicate; ProjectUI
  does not know what an inventory stack is.
- Hover tooltips are anchored above the item's top-center cell through
  `FProjectUIHoverTooltipPresenter::PositionAtAnchor`. Cursor-follow fallback
  is used only when the anchor cannot be resolved.
- Missing `IconCode` is a valid fallback display state. The cell builder renders
  text fallback and logs only at verbose level.

## DRY Patterns

| Pattern | Solution |
|---------|----------|
| BuildGrid/BuildSecondaryGrid | GridBuilder.BuildGrid() |
| BuildContainerTabs duplication | GridBuilder.BuildContainerTabs() |
| Hit detection lambdas | HitDetector.ResolveDualGridHit() |
| Cell visual logic | VisualState.ApplyToGrid() |
| Drag preview logic | DragDropHandler.UpdatePreview() |

## Additional Widgets

| Widget | Responsibility |
|--------|----------------|
| W_ItemTooltip | Item details (name, description, weight/volume, durability bar, ammo, modifiers) |

- Layout driven by `Data/ItemTooltipLayout.json`
- Durability bar only shown when < 100%

## Toast Notifications

Toast system lives in ProjectUI (shared across features):
- `UProjectToastSubsystem` (GameInstanceSubsystem) - queue management, auto-dismiss timer
- `UW_ToastNotification` - fade in/out animation, icon by type
- Panel calls `ShowToast()` on inventory errors via ViewModel subscription

## File Structure

```
Source/ProjectInventoryUI/
 Public/Widgets/
   InventoryPanelGridBuilder.h     (~69 lines)
   InventoryPanelState.h           (~108 lines)
   W_ItemTooltip.h
   W_ItemContextMenu.h
 Public/Policies/
   InventoryUIDropStackPolicy.h
 Private/Widgets/
   InventoryPanelGridBuilder.cpp   (~268 lines)
   InventoryPanelState.cpp         (~28 lines)
   W_InventoryPanel.cpp            (~788 lines)
   W_ItemTooltip.cpp
   W_ItemContextMenu.cpp
 Private/Policies/
   InventoryUIDropStackPolicy.cpp
 Private/MVVM/
   InventoryViewModel.cpp          (~697 lines)
 Data/
   ItemTooltipLayout.json
```

Shared UI framework dependencies (ProjectUI):
- `Source/ProjectUI/Public/Interaction/ProjectUIGridHitDetector.h`
- `Source/ProjectUI/Public/Interaction/ProjectUIGridDragDropController.h`
- `Source/ProjectUI/Public/Presentation/ProjectUIGridVisualState.h`
- `Source/ProjectUI/Public/Overlay/ProjectUIPopupPresenter.h`
- `Source/ProjectUI/Public/Overlay/ProjectUIHoverTooltipPresenter.h`

## Critical Ownership Guardrails

- Keep inventory-specific meaning in this plugin (entry semantics, command routing, equip slot interpretation).
- Keep inventory-specific preview policy in this plugin, such as same-item
  stack preview checks from resolved entry views.
- Keep generic interaction mechanics in ProjectUI (grid math, drag/drop controller, popup/tooltip lifecycle, widget binder).
- Empty cell sentinel SOT is `UInventoryViewModel::EmptyCellInstanceId = INDEX_NONE`.
- Action capability SOT is explicit producer mapping (`bCanUse`, `bCanEquip`, `bActionCapsPopulated`) consumed by `BuildActionCapabilityState`.
- Cell presentation SOT is `FInventoryCellVisualState`; a single text payload is
  insufficient for stacked/icon-backed items.
- Shared grid geometry such as rectangle overlap belongs in ProjectSharedTypes
  so UI and gameplay cannot diverge without putting data/helper drift into
  ProjectCore.
- Do not reintroduce removed inventory-generic helpers (`FInventoryPanelDragDrop`, `FInventoryGridHitDetector`, `FInventoryGridVisualState`, `UInventoryGridCell`).
- Reference: `Plugins/UI/ProjectUI/docs/framework_consolidation.md`.

## External References

- Epic: Creating Drag and Drop UI (UMG) - https://dev.epicgames.com/documentation/en-us/unreal-engine/creating-drag-and-drop-ui-in-unreal-engine
- Epic: CommonUI plugin docs - https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/CommonUI
