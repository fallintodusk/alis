# ProjectInventoryUI Architecture

Behavior SOT
- Inventory behavior and the container layout contract live only in
  `../../../Features/ProjectInventory/docs/design_vision.md`.
- This doc covers UI implementation split and framework ownership only.
- Do not restate inventory behavior here.

## Overview

Inventory UI follows SOLID principles with single-responsibility components and structured observability.

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

**Refactoring Summary:** Before: 2 monolithic files (1698 + 750 = 2448 lines). After: 7 focused files (~2227 lines total).

## SOLID Principles Applied

| Principle | Implementation |
|-----------|----------------|
| **SRP** | Each class has ONE responsibility |
| **OCP** | Visual states, slot positions are data-driven |
| **LSP** | N/A for current structure |
| **ISP** | Clean interfaces between components |
| **DIP** | Panel depends on abstractions (GridBuilder, DragDropHandler) |

## Observability

- UE_LOG statements at: construct, visibility changes, property changes, grid rebuilds, click handlers, drag/drop
- Log category: `LogInventoryPanel`
- Verbose level for frequent events (mouse move, cell clicks)
- Log level for user actions (equip, drop, use)

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
 Private/Widgets/
   InventoryPanelGridBuilder.cpp   (~268 lines)
   InventoryPanelState.cpp         (~28 lines)
   W_InventoryPanel.cpp            (~788 lines)
   W_ItemTooltip.cpp
   W_ItemContextMenu.cpp
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
- Keep generic interaction mechanics in ProjectUI (grid math, drag/drop controller, popup/tooltip lifecycle, widget binder).
- Empty cell sentinel SOT is `UInventoryViewModel::EmptyCellInstanceId = INDEX_NONE`.
- Action capability SOT is explicit producer mapping (`bCanUse`, `bCanEquip`, `bActionCapsPopulated`) consumed by `BuildActionCapabilityState`.
- Do not reintroduce removed inventory-generic helpers (`FInventoryPanelDragDrop`, `FInventoryGridHitDetector`, `FInventoryGridVisualState`, `UInventoryGridCell`).
- Reference: `Plugins/UI/ProjectUI/docs/framework_consolidation.md`.

## External References

- Epic: Creating Drag and Drop UI (UMG) - https://dev.epicgames.com/documentation/en-us/unreal-engine/creating-drag-and-drop-ui-in-unreal-engine
- Epic: CommonUI plugin docs - https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/CommonUI
