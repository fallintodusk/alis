# Inventory - Stacks Not Shown + Icon Disappears on Drag-Drop

**Status:** Resolved (ready to archive to `01_done/` after user review)
**Priority:** Major
**Date:** 2026-04-17 (initial fix); 2026-04-20 (follow-up round); 2026-04-22 (triage)
**Build:** ALIS_20260417_173648 (Shipping, Win64)
**Log:** `<local-app-data>\Alis\Saved\Logs\Alis.log`
**Related:** [inventory_ui_bugs.md](inventory_ui_bugs.md) (previous round, marked Resolved)

## Current state (triage 2026-04-22, autonomous watcher tick)

Both documented rounds (2026-04-17 approved plan + 2026-04-20 whole-stack
/ tooltip / backpack stacking follow-up) are landed and validated. The
`Symptoms`, `Proven from Log`, `Hypotheses`, `Likely Cause Ranking`,
`Recommended Actions`, `Engine-Backed Fix Directions`, and
`Code Research Checklist` sections BELOW were written during the
initial investigation and are retained as **reference material / history**
- they are NOT an open work queue. The follow-up round addressed the
visible P0/P1 items; any remaining items from those sections that are
still relevant should be promoted into a fresh, focused todo rather than
re-read from this archive.

No new work taken this tick. Doc is ready for `todo/01_done/inventory/`
archival on user review.

---

## Approved Plan

- Make `UObjectDefinitionCache` the single inventory-side resolver and residency owner.
- Final architecture correction: make the cache owned by one GameInstance subsystem, not by inventory components.
- Remove inventory runtime `AssetManager` reach-through and resolve item data only through the cache.
- Replace the single-text inventory cell contract with a visual DTO that can render icon/text plus quantity at the same time.
- Treat split-to-source overlap as a local cancel/no-op instead of warning spam.
- Make `HideDefinition` idempotent when auto-visibility hides a definition before its widget is active.
- Add diagnostics so automation dumps show resolved display payloads and cache state instead of only widget trees.
- Re-run data validation, targeted inventory/UI automation, and smoke coverage after the refactor.

## Resolution Summary

- Root cause 1: object definition residency was not owned consistently. Warmed/on-demand loads could fall out of the cache, and some manually created inventory components had no cache at all. The old direct `AssetManager` fallback masked this in some paths and made failures intermittent.
- Root cause 2: inventory UI cells only had one text payload, so icon glyphs and stack quantities competed for the same slot. This made stacked items unreadable and made drag-drop visuals fragile.
- Root cause 3: expected control-flow cases were logged as warnings: hide-before-show in `ProjectUILayerHostSubsystem`, and split drags that landed back on the source footprint.
- Root cause 4: diagnostics were too shallow. Widget tree dumps existed, but they did not show resolved inventory entry payloads or cache residency state.
- Architecture review found and fixed one second-pass issue: component-local fallback cache creation could have made per-component caches permanent. The final version binds components to a single GameInstance-owned cache subsystem.

## Implemented

- `UObjectDefinitionCache` now keeps long-lived `ResolvedDefinitions` and `ResidentHandles`, exposes load-state diagnostics, and promotes both warmup and on-demand loads into the same residency tables.
- `UProjectObjectDefinitionCacheSubsystem` now owns exactly one `UObjectDefinitionCache` per GameInstance.
- `UProjectInventoryComponent` now binds to the GameInstance cache subsystem, resolves item data only through that cache, logs explicit resolve/reject states, and no longer creates component-local caches or depends on runtime `AssetManager` reach-through.
- `ResolveItemDataView` is read-oriented. It observes Loaded/Loading/Missing state and does not start async loads from gameplay getters; warmup/load orchestration stays at bootstrap/session boundaries.
- Inventory entry views now always set capability flags explicitly, including deterministic unavailable states when definition data is not ready.
- `ProjectInventoryUI` now uses `FInventoryCellVisualState` as the only cell presentation path so anchor cells can show icon/text and quantity overlays together while spill cells stay visually quiet.
- Legacy cell text arrays and compatibility setters were removed instead of maintained as a parallel path.
- Inventory grid cells now render an overlay-based presentation owned by inventory UI: centered primary text/icon plus bottom-right quantity badge.
- Split drags onto the source footprint are cancelled locally in the panel instead of sent to the server as noisy invalid moves.
- UI and backend overlap checks now share `FInventoryGridGeometry` from ProjectSharedTypes instead of duplicated rectangle math or ProjectCore helper creep.
- `ProjectUILayerHostSubsystem::HideDefinition` now returns quietly when a definition exists but no active widget entry has been created yet.
- Inventory dump automation now writes entry-level diagnostics and definition-cache diagnostics alongside the widget tree dump.

## Validation

- `python .\scripts\ue\check\data\validate_all.py`
- `.\scripts\ue\standalone\build.ps1`
- `.\scripts\ue\test\unit\run_cpp_tests_safe.ps1 -TestFilter "ProjectInventory.Cache"`
- `.\scripts\ue\test\unit\run_cpp_tests_safe.ps1 -TestFilter "ProjectInventory.CacheSubsystem"`
- `.\scripts\ue\test\unit\run_cpp_tests_safe.ps1 -TestFilter "ProjectInventory.Component.Cache.ResolveMissingDoesNotStartLoad"`
- `.\scripts\ue\test\unit\run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.Inventory.Cache" -Map "/MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent"`
- `.\scripts\ue\test\unit\run_cpp_tests_safe.ps1 -TestFilter "ProjectInventory.MoveHelper.SelfOverlapDetection"`
- `.\scripts\ue\test\unit\run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.UI.Framework.Inventory.CellVisualState"`
- `.\scripts\ue\test\unit\run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.UI.Dialogue.LayerHost.HideDefinitionIdempotent" -Map "/MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent"`
- `.\scripts\ue\test\ui\check_inventory_layout.ps1 -Scenario Hands`
- `.\scripts\ue\test\ui\check_inventory_layout.ps1 -Scenario NearbyLoot`
- `.\scripts\ue\test\ui\check_inventory_layout.ps1 -Scenario Naked`
- `.\scripts\ue\test\unit\run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.InventoryLootPlaces.UI.ViewModelNearbyContainerSession"`
- `.\scripts\ue\test\unit\run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.InventoryLootPlaces.UI.HandsAllowMultipleSmallItemsInSameHand"`
- `.\scripts\ue\test\unit\run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.InventoryLootPlaces.UI.TakeNearbyFallsBackToAlternateHand"`
- `.\scripts\ue\test\smoke\boot_test.bat`

## Follow-up: Whole-Stack Drag, Anchored Tooltip, Backpack Stacking

Date: 2026-04-20

Additional root causes:
- Normal drag started from `SelectedQuantity = 1`, so stacked items moved as a
  one-item split unless the quantity state had been changed elsewhere.
- ProjectUI grid drop validation rejected every non-self occupied cell before
  InventoryUI could preview same-item stack drops. Hand stacking worked through
  another path, while backpack grid stacking was blocked by the generic UI
  occupancy check.
- Split failures had backend reasons, but the UI did not reliably surface a
  clear user-facing error when the quantity was invalid or the target was
  blocked.
- Hover tooltips followed the cursor and could cover the item/drop area.

Implemented:
- `InventoryPanelState` now syncs new selections to whole-stack quantity by
  default, while preserving an explicit quantity override for the same selected
  entry.
- `NativeOnDragDetected` and context-menu drop flows use the whole entry
  quantity unless a split/quantity override is active.
- `FProjectUIGridDragDropController` now has rule-based validation/preview/drop
  overloads. The default rule remains strict; InventoryUI supplies its own
  occupied-cell allowance for stack previews.
- `FInventoryUIDropStackPolicy` owns InventoryUI stack-preview rules from
  resolved entry views: same item, 1x1 source and target, stackable target, and
  enough remaining stack capacity.
- `UW_InventoryPanel` now routes local drop/split failures through inventory
  toast errors and keeps backend authority for final move rejection.
- `FProjectUIHoverTooltipPresenter::PositionAtAnchor` positions hover tooltips
  above the hovered item's top-center cell with viewport clamping.
- Expected user-denied inventory move/store paths now log exact reasons at
  verbose/log level instead of warnings. Warnings are reserved for likely
  bootstrap, data, or service bugs.
- Missing `IconCode` now renders text fallback without warning noise.

Additional validation:
- `python .\scripts\ue\check\data\validate_all.py`
- `.\scripts\ue\standalone\build.ps1`
- `.\scripts\ue\test\unit\run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.UI.Framework.Inventory" -NoRHI -TimeoutSeconds 180`
- `.\scripts\ue\test\unit\run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.UI.Framework.GridDragDrop.FootprintValidation" -NoRHI -TimeoutSeconds 180`
- `.\scripts\ue\test\unit\run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.UI.Framework.PopupAndTooltip.LifecycleAndClamp" -NoRHI -TimeoutSeconds 180`
- `.\scripts\ue\test\unit\run_cpp_tests_safe.ps1 -TestFilter "ProjectInventory" -NoRHI -TimeoutSeconds 240`
- `.\scripts\ue\test\unit\run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.InventoryLootPlaces.Inventory.RequestMoveItemRejectsDepthOverflowOverlap" -TimeoutSeconds 240`
- `.\scripts\ue\test\unit\run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.InventoryLootPlaces.UI" -TimeoutSeconds 360`
- `.\scripts\ue\test\smoke\boot_test.bat`
- Targeted log scan after the final inventory UI integration run found no
  inventory warnings/errors, no `Grid resolve FAILED`, no missing-definition
  warnings, no stack-reject warning, and no cell-builder `IconCode` warning.

## Symptoms

### S1 - Stack counts not visible in UI

Backpack shows Cigarette x17 in one cell but UI doesn't display the stack count overlay. Backend correctly tracks stacks (log confirms `Stacked 1 x Cigarette (Total: 17)`).

### S2 - Icon disappears after drag-drop

When dragging an item from backpack to hand grid, the icon vanishes. The item is placed correctly in the data model (ViewModel reports correct entries), but the visual widget loses its icon.

---

## Proven from Log

### Backend stacking works correctly

Items are stacked properly in the inventory component:

```
(line 8405) LogProjectInventory: Stacked 1 x Cigarette (Total: 2)
...
(line 9808) LogInventoryVM: Entry: ObjectDefinition:Cigarette x17 in Item.Container.Backpack at (0,0)
```

### Drag-drop moves items correctly in data

```
(line 10614) LogInventoryPanel: NativeOnDrop: InstanceId=2 From=Item.Container.Backpack Pos=(0,0) Qty=1 Rot=0
(line 10615) LogInventoryPanel: NativeOnDrop -> MoveItem to hand Item.Container.LeftHand at (0,0)
(line 10622) LogInventoryVM: Entry: ObjectDefinition:Cigarette x1 in Item.Container.LeftHand at (0,0)
```

Data move appears correct in the observed sequence. ViewModel reflects the moved item.

### Stacking on occupied hand cells works in data

Dropping onto an already-occupied hand cell correctly stacks:

```
(line 12481) LogInventoryVM: Entry: ObjectDefinition:Cigarette x2 in Item.Container.RightHand at (1,1)
...
(line 13426) LogInventoryVM: Entry: ObjectDefinition:Cigarette x3 in Item.Container.LeftHand at (0,0)
```

### Item data lookups start failing after some gameplay

```
(line 14737) LogProjectInventory: Warning: GetItemDataView: Missing item data for ObjectDefinition:Cigarette (not loaded)
(line 14738) LogProjectInventory: Warning: Server_DropItem: Item cannot be dropped
```

After dropping one cigarette to world (line 14458), subsequent drops fail with "Missing item data".

```
(line 15604) LogProjectInventory: Warning: Internal_MoveItem: Missing item data
(line 15605) LogProjectInventory: Warning: GetItemDataView: Missing item data for ObjectDefinition:Backpack (not loaded)
```

Eventually Backpack definition also becomes unavailable.

### ActionCaps warning after data loss

```
(line 15621) LogInventoryVM: Warning: BuildActionCapabilityState: entry producer did not set bActionCapsPopulated; capabilities default to explicit values only.
```

This appears only after the "Missing item data" errors - is consistent with a cascade into capability resolution.

### Cigarette has no IconCode - text fallback

```
(line 15647) LogTemp: Warning: [CellBuilder] Item 'ObjectDefinition:Cigarette' has no IconCode - using text fallback
```

CellBuilder cannot resolve icon for Cigarette. This is content debt (no `IconCode` field set), but it is also a competing explanation for part of the "icon disappears" symptom - cells may be rendering invisible/empty text fallback instead of an icon glyph.

### WaterBottle item data fails repeatedly, then succeeds later

```
(line 30913) LogProjectInventory: Warning: GetItemDataView: Missing item data for ObjectDefinition:WaterBottle (not loaded)
(line 30914) LogProjectInventory: Warning: Internal_AddItem: Missing item data for ObjectDefinition:WaterBottle
... (11 failures total between lines 30913-31301)
(line 32515) LogProjectInventory: Internal_AddItem: ObjectDefinition:WaterBottle x1 (size: 1x2, weight: 0.50) -> allowed: 1
(line 32953) LogProjectInventory: [HandleAction] inventory.consume: Removing 1x 'WaterBottle' (InstanceId=13, Context='give_water')
```

WaterBottle fails 11 times then later succeeds. This suggests an intermittent load/order/cache problem, not a permanently missing asset. The data eventually resolves.

### Split overlap and drop failures

```
(line 22434) LogProjectInventory: Warning: Internal_MoveItem: Target overlaps source when splitting
(line 23212) LogProjectInventory: Warning: Internal_MoveItem: Target overlaps source when splitting
```

Split drag-drop attempts fail when target cell overlaps the source.

### DialoguePanel HideDefinition registration issue

```
(line 32650) LogProjectUILayerHost: Warning: HideDefinition - Entry not found for Id=ProjectDialogueUI.DialoguePanel
(line 32653) LogProjectUILayerHost: Warning: HideDefinition - Entry not found for Id=ProjectDialogueUI.DialoguePanel
(line 32656) LogProjectUILayerHost: Warning: HideDefinition - Entry not found for Id=ProjectDialogueUI.DialoguePanel
```

Panel widget is not registered in the layer host when hide is called (registration/order issue).

---

## Hypotheses (need code verification)

| Hypothesis | Why plausible | What to verify |
|------------|--------------|----------------|
| ObjectDefinition cache refs get GC'd | Data works initially then fails after ~60s (`gc.TimeBetweenPurgingPendingKillObjects=61.1`) | Check if cache holds `TStrongObjectPtr` or `TWeakObjectPtr` in ObjectDefinitionCache.cpp |
| Missing `IconCode` explains part of icon disappearance | CellBuilder falls back to text when no icon code | Check if text fallback renders invisible (wrong font size/color) |
| Grid cell widget has no stack count text block | VM reports correct x17, but player sees no number | Inspect cell widget blueprint/C++ for quantity binding |
| Cell icon brush not re-applied after RefreshFromInventory | Data is correct post-drop, but cell visual stale | Trace refresh path from VM -> cell widget -> icon brush |

---

## Likely Cause Ranking

| # | Issue | Certainty | Impact | Fix Effort |
|---|-------|-----------|--------|------------|
| 1 | Item data lookups fail after gameplay | **Proven** | **Critical** - items un-droppable, un-movable | Harden cache refs |
| 2 | No IconCode on Cigarette | **Proven** | **High** - text fallback, may explain icon issue | Add IconCode to ObjectDefinition |
| 3 | WaterBottle intermittent data failure | **Proven** | **High** - item broken during loot sequences | Fix cache/load ordering |
| 4 | Stack count not displayed in UI | **Observed** | **High** - stacks invisible to player | Verify cell widget binding |
| 5 | Cell icon not refreshed after drag-drop | **Hypothesis** | **High** - icons vanish | Verify refresh path |

---

## Recommended Actions

### P0 - Data loss

1. **Harden ObjectDefinitionCache** - verify whether it holds hard references (`TStrongObjectPtr` or `AddToRoot`) or soft/weak refs. If soft, switch to persistent handles via `FStreamableManager` so loaded ObjectDefinition assets survive GC.

2. **Investigate WaterBottle load ordering** - the item fails 11 times then succeeds later. Check:
   - Whether the first lookup triggers a scan that hasn't completed yet
   - Whether the asset is in a chunk that loads later
   - Whether the cache recovers via retry or via a separate scan path

### P1 - UI rendering

3. **Add IconCode to Cigarette** (and audit all items) - `[CellBuilder]` falls back to text when `IconCode` is empty. This may be the primary visual explanation for "missing icons."

4. **Verify stack count display** - check if the cell widget has a text block bound to entry quantity. If not, add one visible when count > 1.

5. **Verify icon refresh after NativeOnDrop** - trace the path from `RefreshFromInventory` -> cell widget -> icon brush. Confirm cells re-resolve the icon code from the (now loaded) ObjectDefinition.

### P2 - Edge cases

6. **Fix split-to-same-cell overlap** - `Internal_MoveItem: Target overlaps source when splitting` fires at lines 22434, 23212. Split target validation should exclude the source cell.

7. **Validate drop rules client-side** - `Server_DropItem: Item cannot be dropped` fires repeatedly. Add pre-validation before the request.

8. **Fix DialoguePanel HideDefinition** - `HideDefinition - Entry not found for Id=ProjectDialogueUI.DialoguePanel` fires 3x at line 32650+. Panel is not registered in the layer host when hide fires.

9. **Add defensive logging** - log when a cell widget sets/clears its icon brush, so icon disappearance can be traced to a specific call.

---

## Engine-Backed Fix Directions

### Item Data Residency - explicit Asset Manager ownership

UE's asset loading docs: `FStreamableManager` provides managed handles that stay active until explicitly released. `RequestAsyncLoad` avoids game thread stalls; `RequestSyncLoad` can stall for seconds. Asset Manager recommends primary asset registration up front and bundle-based loading/unloading.

**Standard:**
- `ObjectDefinition` / item data must use explicit residency ownership.
- Use Asset Manager registration for discovery.
- Use `FStreamableManager` / Asset Manager load handles for residency.
- Keep managed handles alive for the entire gameplay/UI scope that needs the data.

**Project research:**
- Inspect `ObjectDefinitionCache` and determine whether it stores:
  - only IDs / soft paths,
  - weak refs,
  - strong refs,
  - or managed `FStreamableHandle`s.
- Search for any synchronous fallback (`RequestSyncLoad`, `TryLoad`, `LoadSynchronous`) in inventory and UI code.
- Verify whether WaterBottle failure happens before async completion, after handle release, or after a bundle/state change.

**Design rule:** A cache should not simultaneously be: (1) a registry, (2) a loader, (3) a GC workaround, (4) a UI data source. Split those roles.

Ref: [Asset Management](https://dev.epicgames.com/documentation/unreal-engine/asset-management-in-unreal-engine), [FStreamableManager::RequestAsyncLoad](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/FStreamableManager/RequestAsyncLoad)

### Plugin-Owned Asset Scan Ownership

Epic exposes `UGameFeatureData::GetPrimaryAssetTypesToScan()` for defining where primary assets should be scanned in the plugin hierarchy.

**Standard:**
If item definitions live in plugins or game-feature style modules, their scan roots should be declared by the owning module, not discovered indirectly by inventory code.

**Project research:**
- Verify whether `ObjectDefinition`, `LootProfileDefinition`, `ProjectAbilitySet`, and related data types come from one root AssetManager config or multiple plugin roots.
- If modular, verify scan ownership in each owning plugin / feature data asset.

**Design rule:** The module that owns the content type owns its scan declaration. Inventory should consume registered content, not invent scan policy.

Ref: [UGameFeatureData::GetPrimaryAssetTypesToScan](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/GameFeatures/UGameFeatureData/GetPrimaryAssetTypesToScan/1)

### Content Validation in CI

UE 5.7 Data Validation supports custom validators, can be run from command line as `UnrealEditor-Cmd.exe <PROJECT>.uproject -run=DataValidation`, and validates assets programmatically.

**Standard:**
Add project validators for:
- required `IconCode` on all item ObjectDefinitions,
- required dimensions / weight / stackability fields,
- droppable / consumable policy coherence,
- valid soft references (no missing packages),
- primary asset registration completeness for mandatory gameplay asset classes.

Run validation in CI with the DataValidation commandlet.

**Project research:**
- Add validators for ObjectDefinitions, item display data, ability sets, and UI definitions.
- Fail CI on invalid production content, not at runtime.

**Design rule:** Content correctness is a build-time contract, not a runtime hope.

Ref: [Data Validation](https://dev.epicgames.com/documentation/unreal-engine/data-validation-in-unreal-engine)

### UI Contract - consume resolved display state, not raw item assets

**Standard:**
The cell widget should consume a resolved display DTO / VM row:
- icon brush or fallback text,
- stack count text,
- visibility flags,
- action capability state.

It should not load or resolve ObjectDefinitions during paint or refresh.

**Project research:**
- Trace `RefreshFromInventory` -> VM row build -> cell visual apply.
- Verify whether cells are rebuilding from stable VM data or re-reading item assets on refresh.

**Design rule:** UI rendering must be pure and side-effect free. Asset loading belongs to a data service, not to widgets. Widgets must never be the owner of asset residency.

---

## Code Research Checklist

```text
1. ObjectDefinitionCache lifetime:
   - who owns handles
   - when handles are released
   - whether cache stores weak refs only
   - whether UI/inventory accesses data before async completion

2. Search repo for sync load red flags:
   RequestSyncLoad, LoadSynchronous, TryLoad, GetPrimaryAssetData,
   LoadPrimaryAsset, LoadPrimaryAssets

3. Plugin scan ownership:
   - UGameFeatureData::GetPrimaryAssetTypesToScan() for each content plugin
   - AssetManager config for ObjectDefinition, LootProfileDefinition, ProjectAbilitySet

4. UI refresh path:
   RefreshFromInventory -> VM row build -> cell widget -> icon brush
   - does cell re-query ObjectDefinition or consume resolved VM data?

5. Content validation:
   - audit all ObjectDefinitions for IconCode field
   - audit soft references for missing packages
   - prototype DataValidation commandlet run
```

---

## Key Log Lines

| Line | Category | Detail |
|------|----------|--------|
| 8405 | Stacking works | `Stacked 1 x Cigarette (Total: 2)` |
| 9808 | Stack in VM | `Cigarette x17 in Backpack at (0,0)` |
| 10614-10615 | Drag-drop works | `NativeOnDrop -> MoveItem to hand` |
| 14737 | Data loss start | `Missing item data for Cigarette (not loaded)` |
| 14738 | Drop blocked | `Server_DropItem: Item cannot be dropped` |
| 15604-15605 | Cascade | `Missing item data` for both Cigarette and Backpack |
| 15621 | ActionCaps degraded | `entry producer did not set bActionCapsPopulated` |
| 15647 | No IconCode | `[CellBuilder] Cigarette has no IconCode - using text fallback` |
| 22434, 23212 | Split overlap | `Target overlaps source when splitting` x2 |
| 30913-31301 | WaterBottle fails | `Missing item data for WaterBottle` x11 |
| 32515 | WaterBottle succeeds | `Internal_AddItem: WaterBottle x1 -> allowed: 1` |
| 32650-32656 | DialoguePanel hide | `HideDefinition - Entry not found` x3 |
