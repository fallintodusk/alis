# UI Refactor Plan: Move Common Inventory UI Logic Into ProjectUI Framework

Status: Stage-1 implemented, validated, and hardened
Date: 2026-02-11
Owner: UI architecture refactor

## Goal

Refactor inventory UI so `ProjectInventoryUI` keeps only inventory-specific behavior, while reusable UI mechanics move to `ProjectUI` as common framework primitives.

Target outcome:
- Inventory classes become thin orchestration code.
- Shared mechanics (grid interaction, popup lifecycle, hover tooltip, widget binding, visual state) are implemented once in `ProjectUI`.
- New UIs (loot, crafting, vendor, equipment, stash, map overlays) reuse the same framework without copy-paste.

## Why This Refactor Is Needed

Current state is good compared to a monolith, but common logic is still locked in inventory-named helpers:
- `FInventoryGridHitDetector`
- `FInventoryGridVisualState`
- `FInventoryPanelDragDrop`
- `FInventoryPanelContextMenu`
- `FInventoryPanelTooltip`
- `FInventoryPanelBindings`
- `FInventoryPanelTextUpdater`
- `UInventoryGridCell`

These helpers are technically reusable, but their names/contracts are inventory-coupled, so other UI plugins will likely duplicate similar code instead of reusing them.

## Existing Foundation In ProjectUI (Already Strong)

Do not re-invent these:
- JSON layout system: `ProjectWidgetLayoutLoader`
- Widget base lifecycle: `UProjectUserWidget`, `UProjectActivatableWidget`
- Theme SOT: `UProjectUIThemeManager`, `UProjectUIThemeData`
- Definition registry/factory: `UProjectUIRegistrySubsystem`, `UProjectUIFactorySubsystem`
- Layer/input orchestration: `UProjectUILayerHostSubsystem`, `UUIExtensionSubsystem`, `UW_LayerStack`
- Diagnostics: `UProjectUIDebugSubsystem`, dump tooling

Refactor should extend these systems, not bypass them.

## SOC + SOT Architecture Boundaries

### Layer 1: Domain SOT (ProjectCore + feature plugin)
- Source of truth for inventory item rules and command semantics.
- Example fields: `bIsConsumable`, `EquipSlotTag`, `bCanBeDropped`, `Quantity`, `MaxStack`.
- Files: `IInventoryReadOnly.h`, `IInventoryCommands.h`, inventory feature code.

### Layer 2: ViewModel SOT (ProjectInventoryUI)
- Source of truth for UI-ready state.
- Converts domain data to presentation-friendly snapshots.
- Should also own action policy output (see Action Model section below).

### Layer 3: UI Mechanics SOT (ProjectUI)
- Owns generic interaction plumbing: hit tests, selection state machine, drag preview state, popup/tooltip lifecycle, widget lookup/binding helpers.
- No inventory tags, no inventory commands, no inventory structs.

### Layer 4: Screen-Specific Widgets (ProjectInventoryUI)
- Minimal glue code only:
  - route events from framework controllers to ViewModel commands
  - render inventory-specific visuals
  - no duplicated generic interaction logic

## What To Move Into ProjectUI

### A. Grid Interaction Primitives
Move and rename:
- `FInventoryGridHitDetector` -> `FProjectUIGridHitDetector`
- `FInventoryGridVisualState` -> `FProjectUIGridVisualState`
- `UInventoryGridCell` -> `UProjectGridCell` (or keep class and re-home if safe)

Design rules:
- Generic names and generic inputs only.
- No dependency on `InventoryViewModel` or `FInventoryEntryView`.
- Use callbacks/interfaces for "is cell enabled", "occupant id", "is valid drop".

### B. Drag/Drop Controller
Move and split:
- `FInventoryPanelDragDrop` -> `FProjectUIGridDragDropController`

Refactor contract:
- Input: geometry + pointer/drag event + adapter callbacks
- Output: `FProjectUIGridDragPreviewResult` (highlight cells, validity, target id)

Inventory plugin then injects only domain callbacks:
- can place item?
- what container id is this grid?
- request move/equip command

### C. Popup/Tooltip Lifecycle
Move:
- `FInventoryPanelContextMenu` -> `FProjectUIPopupPresenter`
- `FInventoryPanelTooltip` -> `FProjectUIHoverTooltipPresenter`

Keep generic:
- create/destroy widget in canvas
- click-catcher behavior
- viewport clamp and z-order policy
- optional cached item key to avoid redundant updates

Keep inventory-specific:
- tooltip content widget (`UW_ItemTooltip`)
- context action semantics (Use/Drop/Equip/Split)

### D. Widget Binding Helper
Evolve:
- `FInventoryPanelBindings` into generic binder utility in `ProjectUI`

Proposed API direction:
- strongly-typed `FindRequired<T>(Name)` and `FindOptional<T>(Name)`
- structured report for missing widgets
- one-time validation log + debug dump integration

This removes repeated `FindWidgetByNameTyped` boilerplate in each feature widget.

### E. Action Presentation Model (Critical)
Introduce generic action descriptor in `ProjectUI`:
- `FProjectUIActionDescriptor`
  - `ActionId` (FName)
  - `Label`
  - `bVisible`
  - `bEnabled`
  - `Priority`
  - optional icon id

Then inventory ViewModel exposes action descriptors for selected item.
UI widgets render descriptors, no duplicated "if consumable then hide equip" logic in multiple places.

## What Must Stay In ProjectInventoryUI

Do not move these to framework:
- equip slot tag map and body silhouette rules
- inventory-specific grid labels and item text format decisions
- inventory command routing (`RequestUseItem`, `RequestEquipItem`, etc.)
- inventory entry interpretation (`FInventoryEntryView`)
- inventory context menu widget skin/content

Framework owns mechanics. Inventory plugin owns meaning.

## Critical Guardrails (Do Not Break)

These rules are mandatory for current and future UI modules.

1. Generic mechanics go to `ProjectUI` only.
   - Examples: grid hit math, drag preview state, popup lifecycle, tooltip positioning, generic widget binding.
   - If logic is reusable by 2+ screens/plugins, it is not inventory-only code.

2. Feature plugins stay domain-focused.
   - `ProjectInventoryUI` can own inventory semantics and rendering only.
   - Do not add new cross-feature helpers in feature plugin folders.

3. Single action policy source.
   - Visibility/enabling for Use/Equip/Drop/Split must come from one ViewModel/domain policy output.
   - Widgets render policy state; they must not re-implement business rules.

4. Widget creation path is centralized.
   - Use `ProjectUIFactorySubsystem` and `ProjectUILayerHostSubsystem`.
   - Do not create ad-hoc top-level UI flows outside `ProjectUI`.

5. Layout binding is explicit and validated.
   - Required widgets must be validated at construct time.
   - Missing required bindings should fail fast in logs/tests, not degrade silently.

6. No game-entity logic inside widgets.
   - Widgets read ViewModel state and emit commands only.
   - Game object resolution stays in ViewModel/subsystems.

7. Container index SOT is storage-only.
   - `SelectedContainerIndex` and `SecondaryContainerIndex` must always be validated against filtered storage containers.
   - Hand containers (`Item.Container.Hands`, `Item.Container.LeftHand`, `Item.Container.RightHand`) must never drive storage grid dimensions.
   - Keep regression coverage for "hands + one storage" and "hands only" states.

8. Action capability mapping is explicit.
   - Map domain fields to action capabilities in one mapper function in the ViewModel.
   - Do not infer equip visibility with ad-hoc checks in widgets.
   - If new domain flags are added (example: `bCanUse`, `bCanEquip`), update mapper only.

9. Legacy helper classes are forbidden for new code.
   - Deleted helpers are hard-banned and must not be reintroduced:
     - `FInventoryPanelContextMenu`
     - `FInventoryPanelTooltip`
     - `FInventoryPanelBindings`
     - `FInventoryPanelDragDrop`
     - `FInventoryGridHitDetector`
     - `FInventoryGridVisualState`
     - `UInventoryGridCell`
   - New code must use `ProjectUI` equivalents only.

10. Presenter-owned widgets must keep ViewModel contract.
   - Any `UProjectUserWidget` created through presenters (`FProjectUIPopupPresenter`, `FProjectUIHoverTooltipPresenter`)
     must receive the same ViewModel as the owning screen when that widget depends on VM-driven diagnostics/state.
   - This prevents `NO_VIEWMODEL` regressions in dump-based inspection gates.

### Code Review Red Flags

- New utility class in a feature plugin that has no domain terms.
- Duplicated action visibility logic in multiple widgets.
- Direct `FindWidgetByName` boilerplate repeated across screens.
- New input/focus/popup code bypassing shared presenters.
- Widget code touching pawn/controller/component game logic.

### PR Checklist For UI Changes

- [ ] Is any new logic reusable outside this feature?
  - If yes, move to `ProjectUI` now.
- [ ] Is action policy defined in one place?
- [ ] Are required bindings validated and covered by a test/dump check?
- [ ] Does this keep `UProjectUserWidget`/layer host/factory flow intact?
- [ ] Did we avoid duplicate helper code already present in `ProjectUI`?

## Action Model Unification (Fix Current Drift)

Current action visibility/enabling is spread across:
- `W_ItemContextMenu::UpdateActionVisibility`
- `FInventoryPanelTextUpdater::UpdateCommandButtons`

This can drift over time.

Refactor target:
1. ViewModel computes one `FInventoryActionState` (or action descriptor array) for current selection.
2. Both context menu and button row consume the same state.
3. No duplicated visibility logic in widget layer.

### Implementation Status (2026-02-11)

Completed in code for `ProjectInventoryUI`:
- Removed public `FInventoryActionPolicy` model and old policy API from `UInventoryViewModel`.
- Added generic action descriptor mapping in `UInventoryViewModel` (`FProjectUIActionDescriptor`) as the only public action presentation model.
- `W_ItemContextMenu` now consumes ViewModel-provided action descriptors (no direct item-rule checks).
- `FInventoryPanelTextUpdater` now consumes the same descriptor state for detail buttons.
- `W_InventoryPanel` command handlers and context-menu handlers now gate actions through descriptor checks.
- Context menu is no longer shown when selected item has no enabled actions.
- Removed dead context-menu state (`CurrentQuantity`) and duplicate widget-level policy code.

Completed in code for descriptor-based command presentation:
- Added `FProjectUIActionDescriptor` in `ProjectUI` (`Presentation/ProjectUIActionDescriptor.h`).
- Migrated `W_ItemContextMenu` to consume descriptor arrays instead of policy booleans.
- Migrated `FInventoryPanelTextUpdater` detail command buttons to consume descriptor arrays.
- Migrated `W_InventoryPanel` command execution gates to descriptor checks (`IsActionEnabled`).

Completed in code for `ProjectUI` + inventory migration:
- Added generic popup lifecycle presenter: `FProjectUIPopupPresenter`.
- Added generic hover tooltip presenter: `FProjectUIHoverTooltipPresenter`.
- Added generic widget binding helper: `FProjectUIWidgetBinder`.
- Migrated `UW_InventoryPanel` to use ProjectUI presenters directly for context menu and tooltip lifecycle.
- Migrated `UW_InventoryPanel` widget lookup to `FProjectUIWidgetBinder` with required-widget validation logs.
- Migrated `UW_ItemContextMenu` and `UW_ItemTooltip` widget lookup to `FProjectUIWidgetBinder`.
- Removed inventory-specific lifecycle wrappers (legacy deleted):
  - `FInventoryPanelContextMenu`
  - `FInventoryPanelTooltip`
  - `FInventoryPanelBindings`
- Added stronger open guard: context menu only opens when at least one action is enabled.

Completed in code for framework reuse outside inventory:
- Migrated `UW_VitalsPanel` widget binding to `FProjectUIWidgetBinder`.
- Migrated `UW_VitalsHUD` widget binding to `FProjectUIWidgetBinder`.
- Migrated `UW_MainMenu` Settings content hosting to `FProjectUIPopupPresenter` (non-inventory reuse proof for Phase 6).

Completed in code for action capability contract finalization:
- Added explicit capability fields in `FInventoryEntryView`: `bCanUse`, `bCanEquip`.
- Added explicit producer marker field in `FInventoryEntryView`: `bActionCapsPopulated`.
- Updated inventory view producer mapping in `InventoryViewHelper` to populate explicit capability flags.
- Moved capability mapping into ViewModel SOT (`BuildActionCapabilityState`) with explicit flags first.
- Kept capability mapping SOT in one function: `BuildActionCapabilityState`.
- Removed legacy fallback path in `BuildActionCapabilityState`; action capability now uses explicit fields only.

Completed in code for dump-inspection stability:
- Presenter-owned widgets in inventory panel now receive ViewModel binding, eliminating `NO_VIEWMODEL`
  false failures in layout inspection.

Completed in code for stability and regression hardening:
- Fixed storage index normalization in `UInventoryViewModel::BuildContainerData` to use filtered storage containers (not raw container list with hands).
- Added explicit action capability mapper in `UInventoryViewModel` so Use/Equip/Drop/Split rules are derived from one capability mapping function.
- Added integration regression test `ProjectIntegrationTests.UI.Layout.InventoryHands.SingleStorageIndexing` using a mock `IInventoryReadOnly` source.
- Standardized inventory empty-cell sentinel to `UInventoryViewModel::EmptyCellInstanceId = INDEX_NONE` and removed remaining `0`-empty assumptions in viewmodel/grid code.
- Removed drag payload empty-cell adapter (`EmptyInstanceId`) after sentinel standardization; `FProjectUIGridDragDropController` now uses `INDEX_NONE` directly.

Completed in code for generic grid mechanics migration:
- Added `FProjectUIGridHitDetector` in `ProjectUI`.
- Added `FProjectUIGridVisualState` and themed color state model in `ProjectUI`.
- Added `FProjectUIGridDragDropController` with callback-driven domain hooks in `ProjectUI`.
- Migrated `UW_InventoryPanel` to use new ProjectUI grid mechanics types.
- Removed inventory-specific generic mechanics helpers (legacy deleted):
  - `FInventoryGridHitDetector`
  - `FInventoryGridVisualState`
  - `FInventoryPanelDragDrop`

Completed in code for generic grid-cell widget migration:
- Added generic `UProjectGridCell` in `ProjectUI`.
- Migrated inventory panel and grid builder to `UProjectGridCell`.
- Removed inventory-specific grid cell widget (legacy deleted):
  - `UInventoryGridCell`

Example descriptor rules:
- `Use` visible when `bCanUse`
- `Equip` visible when `bCanEquip` and not mutually excluded by current UX rule
- `Drop` visible if droppable
- `Split` visible if stackable and quantity > 1

Single source of truth = one descriptor builder function in ViewModel/domain adapter.

## Proposed New ProjectUI Building Blocks

Suggested folder structure:

```
Plugins/UI/ProjectUI/Source/ProjectUI/Public/
  Interaction/
    ProjectUIGridHitDetector.h
    ProjectUIGridDragDropController.h
    ProjectUISelectionState.h
  Presentation/
    ProjectUIGridVisualState.h
    ProjectUIActionDescriptor.h
    ProjectUIWidgetBinder.h
  Overlay/
    ProjectUIPopupPresenter.h
    ProjectUIHoverTooltipPresenter.h
  Widgets/
    ProjectGridCell.h
```

Private counterparts in matching `Private/` folders.

## Inventory Plugin After Refactor (Thin Class Target)

`UW_InventoryPanel` responsibilities should be:
- lifecycle (`NativeConstruct`, `NativeDestruct`)
- connect framework controllers to `InventoryViewModel`
- screen-specific mapping (which grid is primary/secondary/hands/equip)
- no low-level hit math, popup plumbing, drag preview algorithms

Helper classes left in inventory plugin should be only domain/presentation translators:
- inventory action descriptor adapter
- inventory-specific grid composition (if still unique)
- inventory tooltip/context view widgets

## Migration Plan (Safe Incremental)

### Phase 0 - Lock Behavior Baseline
- Snapshot current behavior with existing integration tests:
  - `ProjectIntegrationTests.UI.Layout.InventoryHands.DumpTree`
  - `ProjectIntegrationTests.UI.Layout.InventoryHands.MultiResolution`
- Capture dump artifacts for before/after diff.

### Phase 1 - Introduce ProjectUI Generic Types
- Add new generic classes in `ProjectUI` without removing inventory helpers.
- Keep wrappers in inventory plugin delegating to new classes.
- No behavior change expected.

### Phase 2 - Migrate Popup and Tooltip Lifecycle
- Replace inventory direct helper logic with `ProjectUIPopupPresenter` and `ProjectUIHoverTooltipPresenter`.
- Keep `UW_ItemContextMenu` and `UW_ItemTooltip` unchanged initially.

### Phase 3 - Migrate Grid Hit + Drag + Visual State
- Switch inventory panel to use new framework controllers.
- Keep existing public methods stable while internals swap.
- Add adapter layer for inventory-specific enablement/occupancy checks.

### Phase 4 - Unify Action Policy (SOT)
- Add action descriptor output in ViewModel.
- Update context menu and command bar to consume descriptor state.
- Remove duplicated visibility checks from widget code.

### Phase 5 - Cleanup and De-duplicate
- Delete or mark deprecated inventory-specific generic helpers.
- Keep compatibility aliases temporarily if needed.
- Update docs and architecture map.

### Phase 6 - Adopt In Next UI Feature
- Implement one non-inventory UI with new framework pieces to validate reuse.
- This is the proof that refactor actually removed duplication risk.

## Testing Strategy

### Existing tests to keep running
- Integration layout dump tests in `ProjectIntegrationTests`.
- UI debug dump command checks from `ProjectUI` docs.

### New tests to add
- Unit tests for generic grid hit detector edge cases.
- Unit tests for drag preview validity matrix (bounds, blocked cells, occupied cells).
- Unit tests for popup clamp logic and click-catcher behavior.
- Unit tests for action descriptor rendering state.

Added in `ProjectIntegrationTests`:
- `ProjectIntegrationTests.UI.Framework.WidgetBinder.RequiredValidation` (required widget lookup + missing tracking).
- `ProjectIntegrationTests.UI.Framework.GridDragDrop.FootprintValidation` (bounds/disabled/occupied/self-occupancy checks).
- `ProjectIntegrationTests.UI.Framework.ActionDescriptors.InventoryMapping` (consumable/equippable/stackable descriptor mapping).
- `ProjectIntegrationTests.UI.Framework.PopupAndTooltip.LifecycleAndClamp` (presenter lifecycle + viewport clamp).
- `ProjectIntegrationTests.UI.Framework.GridHitDetector.EdgeCases` (grid coordinate/index edge rules).
- `ProjectIntegrationTests.UI.Framework.GridVisualState.StateColorMapping` (visual flag precedence and color mapping).
- `ProjectIntegrationTests.UI.Framework.ActionDescriptors.ButtonRenderState` (descriptor -> button visibility/enabled rendering).
- `ProjectIntegrationTests.UI.Framework.MainMenu.SettingsPopupPresenterReuse` (Phase 6 non-inventory presenter reuse).

Completed in documentation and inspection flow:
- Added framework ownership and reuse contract in all UI plugin README docs.
- Added shared critical contract doc: `Plugins/UI/ProjectUI/docs/framework_consolidation.md`.
- Updated `docs/testing/agent_ue_inspection.md` with framework regression gate, consolidation checks, and menu/settings presenter reuse verification.

Mandatory gate for new UI modules:
- `ProjectIntegrationTests.UI.Framework.*` must pass.
- Module-specific layout dump + analyzer check must pass (no high/medium issues):
  - `scripts/ue/test/ui/check_inventory_layout.ps1` (or module equivalent).
- Smoke boot must pass before merge:
  - `scripts/ue/test/smoke/boot_test.bat`

### Regression checks
- left click select only, right click context menu
- drag start does not open context menu
- tooltip hidden while context menu visible
- no duplicate widgets on repeated show/hide
- no focus-related crashes from popup open/close path

## Risk Register and Mitigations

Risk: Breaking current inventory UX while extracting common code.
Mitigation: wrapper-first migration, behavior parity tests per phase.

Risk: Over-generalization that becomes hard to use.
Mitigation: start from concrete inventory use case, extract only proven common mechanics.

Risk: Framework depending on inventory domain types.
Mitigation: hard rule that `ProjectUI` cannot include inventory headers.

Risk: Input/focus regressions in context menu.
Mitigation: keep popup non-focusable by default, rely on mouse interaction and layer host input policy.

Risk: Performance regressions due extra abstraction.
Mitigation: keep controllers as lightweight structs/classes, avoid per-frame allocations, keep invalidation-friendly updates.

## Definition Of Done

Refactor is done when all are true:
- Inventory panel code is substantially thinner and domain-focused.
- Common mechanics exist only in `ProjectUI`.
- At least one additional UI feature reuses new framework components.
- Action visibility logic has one SOT location.
- Existing integration tests pass and no new UI regressions are introduced.
- Docs are updated in both `ProjectUI` and `ProjectInventoryUI`.

## Concrete Candidate Move Map

| Current class | Target class | Target plugin | Keep/Delete |
|---|---|---|---|
| FInventoryGridHitDetector | FProjectUIGridHitDetector | ProjectUI | Move |
| FInventoryGridVisualState | FProjectUIGridVisualState | ProjectUI | Move |
| FInventoryPanelDragDrop | FProjectUIGridDragDropController | ProjectUI | Move+Split |
| FInventoryPanelContextMenu | FProjectUIPopupPresenter | ProjectUI | Move |
| FInventoryPanelTooltip | FProjectUIHoverTooltipPresenter | ProjectUI | Move |
| FInventoryPanelBindings | FProjectUIWidgetBinder | ProjectUI | Generalize |
| UInventoryGridCell | UProjectGridCell | ProjectUI | Move/Re-home |
| FInventoryPanelTextUpdater | InventoryAction/Details presenter | ProjectInventoryUI | Keep (domain-facing) |
| FInventoryViewModelCellBuilder | Inventory-specific data transform | ProjectInventoryUI | Keep |
| FInventoryViewModelEquipSlotBuilder | Inventory-specific slot semantics | ProjectInventoryUI | Keep |

## Notes From External UE Guidance

Applied principles from Epic docs:
- Use event-driven updates over heavy per-frame property polling for UI.
- Keep UI layer centered on CommonUI activation/input routing patterns.
- Treat `GetCachedGeometry` carefully (frame-behind/stale risk) and guard usage.
- Keep UI composition modular through subsystem-based services.

## References

Internal references:
- `Plugins/UI/ProjectUI/README.md`
- `Plugins/UI/ProjectUI/docs/ui_layout.md`
- `Plugins/UI/ProjectUI/docs/ui_mvvm.md`
- `Plugins/UI/ProjectUI/docs/commonui_integration.md`
- `Plugins/UI/ProjectInventoryUI/docs/architecture.md`
- `Plugins/UI/ProjectInventoryUI/Source/ProjectInventoryUI/Private/Widgets/W_InventoryPanel.cpp`
- `Plugins/UI/ProjectInventoryUI/Source/ProjectInventoryUI/Private/Widgets/W_ItemContextMenu.cpp`
- `Plugins/UI/ProjectUI/Source/ProjectUI/Private/Interaction/ProjectUIGridDragDropController.cpp`
- `Plugins/UI/ProjectUI/Source/ProjectUI/Private/Interaction/ProjectUIGridHitDetector.cpp`
- `Plugins/UI/ProjectUI/Source/ProjectUI/Private/Presentation/ProjectUIGridVisualState.cpp`
- `Plugins/UI/ProjectUI/Source/ProjectUI/Private/Overlay/ProjectUIPopupPresenter.cpp`
- `Plugins/UI/ProjectUI/Source/ProjectUI/Private/Overlay/ProjectUIHoverTooltipPresenter.cpp`

External references:
- Unreal Engine UMG and Slate optimization guidelines: https://dev.epicgames.com/documentation/en-us/unreal-engine/optimization-guidelines-for-umg-in-unreal-engine
- Unreal Engine UWidget::GetCachedGeometry API notes: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/UMG/UWidget/GetCachedGeometry
- Unreal Engine CommonUI overview: https://dev.epicgames.com/documentation/en-us/unreal-engine/common-ui-plugin-for-advanced-user-interfaces-in-unreal-engine
- Unreal Engine UCommonActivatableWidget API: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/CommonUI/UCommonActivatableWidget
- Unreal Engine UCommonUIActionRouterBase API: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/CommonUI/UCommonUIActionRouterBase
- Unreal Engine MVVM overview: https://dev.epicgames.com/documentation/en-us/unreal-engine/umg-viewmodel-for-unreal-engine
- Unreal Engine UGameInstanceSubsystem API: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UGameInstanceSubsystem
