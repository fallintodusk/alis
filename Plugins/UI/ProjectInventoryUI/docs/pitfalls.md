# ProjectInventoryUI - Common Pitfalls

Bugs and gotchas encountered during inventory UI development.

## Investigation Patterns (read this first)

Patterns extracted from bugs that took >1h to find. If your symptom matches
one, jump straight to the playbook below.

### Pattern A: "First time fails, after one X works"

If a flow is broken on first attempt and works after some unrelated
interaction, **something between the two attempts is changing widget /
subsystem state**. The unrelated interaction is doing the work that
the first-attempt code path forgot to do.

**Playbook:**
1. Identify the state delta between attempt #1 and attempt #2. Usual
   suspects: a VM `OnPropertyChanged` fire, a delayed
   `RefreshFromViewModel`, a layer-host stamp, a focus change.
2. Find what runs on the second path and ask "could this run earlier?"
3. The fix is usually either: (a) make the missing run happen at
   construction / first frame, or (b) make the widget self-enforce
   the contract so it doesn't depend on the second path.

**Real example:** "First drag from nearby container to inventory cancels;
after one in-container drop, container -> inventory works." The
in-container drop fired a VM property change which ran
`UpdateVisibilityFromViewModel` and flipped the nearby panel root from
`Visible` -> `SelfHitTestInvisible`. First-attempt code path never got
that flip. (See "Nearby Panel Root Visible During First Frame" below.)

### Pattern B: "Log shows a state the code can't produce"

If a log line shows a value that no code path in the file writes, the
value came from outside the file. **Stop debugging the file and grep
for who else writes that field.**

**Playbook:**
1. Confirm the log is honest: does the log actually print the live
   state (`GetVisibility()` / `IsEnabled()` / etc.), or a cached snapshot?
2. List every writer of the field (grep for `Set<Field>`, `<Field> =`,
   reflection setters via `FProperty::SetValue_InContainer`).
3. Eliminate writers in the owning class. Whatever's left is the
   culprit - usually a UMG default, a factory stamp, a layer-host hook,
   a Blueprint default, or a parent-class reflection re-apply.
4. Fix at the closest layer to the widget: a self-enforcing override
   beats hunting an upstream setter.

**Real example:** `RootVis=Visible` in a log emitted by a widget whose
own `UpdateVisibilityFromViewModel` only writes `SelfHitTestInvisible`
or `Collapsed`. The `Visible` came from UMG's default + factory's
post-construct stamp.

### Pattern C: "Two siblings, both `size_policy: Fill`, both `Visible`"

If two widgets share a layer in `ui_definitions.json` with `size_policy:
Fill` and the same `priority`, and one is `ESlateVisibility::Visible`,
that one absorbs Slate's leaf-first hit-test for the entire viewport.
Its sibling's child wrappers will never receive `NativeOnDragOver`,
`NativeOnMouseButtonDown`, etc.

**Playbook:**
1. List Fill-anchored siblings on the same layer (`grep size_policy Data/ui_definitions.json`).
2. For each sibling, confirm: "is this widget's root supposed to be
   hit-test-active across the whole viewport?" Almost always the
   answer is no for overlay widgets - they want
   `SelfHitTestInvisible` so children intercept their own bounds and
   the empty Fill area passes through to siblings.
3. Either coerce visibility (per-widget self-enforcement, the fix used
   in this codebase), or split overlays onto a different layer with a
   `fill_nohittest` size policy (long-term).

### Pattern D: "Diag logs that print events, not state"

Slate drag/drop bugs are routing bugs. Routing bugs are state bugs.
**Diag logs that show only event types without widget state are not
debuggable.** Always print:
- the widget's own visibility (`GetVisibility()`)
- enabled flag
- screen position
- the originating drag op identity

**Anti-example:** `LogNearbyContainerPanel: NativeOnDragCancelled.` Useless.
**Good example:** `LogNearbyContainerPanel: NativeOnDragCancelled: Screen=(797,379) RootVis=Visible Enabled=1 Op=...`. The state field is the
smoking gun.

If a recurring bug class doesn't have state in its diag logs, add it
*before* you start debugging - the existing log format is the cheapest
information channel you have.

### Pattern E: "UMG default visibility wins races"

UMG's default `ESlateVisibility::Visible` is the value any widget
property re-apply (factory stamping, BP CDO defaults, designer-time
DefaultVisibility, `SynchronizeProperties` reflowing reflected props
post-AddToViewport) will reset to. If your widget's contract requires
NOT-Visible, **a one-shot `SetVisibility(...)` in `NativeConstruct` is
not enough**. Override `SetVisibility` and / or
`SynchronizeProperties` to enforce the contract at every entrypoint.

This is especially load-bearing for overlay widgets with
`size_policy: Fill`, where a Visible root means "absorb every drag /
click in the viewport" - see Pattern C.

### Pattern F: "It started working after I added a log"

Heisenbug. The log is forcing a recompile, a frame pump, a refresh
cycle, or a layout pass that the original code path missed. Don't
declare victory - find what the log triggered and inline that work
into the production path.

### Pattern G: "Function already has a body" + cascading C7595 in unity build

If a fresh build prints `error C2084: function ... already has a body`
for a helper inside an `anonymous-namespace`, and `note: see previous
definition of '` anonymous-namespace `'::<same-helper>'` points at a
**different .cpp file** in the same module, you have an unintended
unity-TU collision. Anonymous namespaces guarantee internal linkage at
the **TU** level, not the **file** level - and UE's adaptive unity
build concatenates several .cpp files into one TU
(`Module.<Name>.<N>.cpp`). When two of those concatenated files each
define `IsInventoryDragDiagEnabled` / `DescribeObject` / etc. in
their own anonymous namespace, the merged TU sees two definitions of
the same symbol and refuses to compile.

A second error (`C7595: TCheckedFormatStringPrivate ... call to
immediate function is not a constant expression` + "not enough
arguments provided to format string") often follows. It's a
**cascading failure** - the duplicate-definition error makes the
compiler reject the call expression that uses the helper, so the
arg-count check in `UE_LOG`'s consteval format-string sanitizer sees
the argument list as malformed. Fix the symbol collision and the
format-string error disappears with it.

**Playbook:**
1. Identify the colliding helper names (the C2084 notes name them).
2. Pick ONE TU as the "owner" of the helper. Either:
   - Rename the helpers in the **other** TU (prefix with the file's
     concept, e.g. `NearbyPanel_IsInventoryDragDiagEnabled`); OR
   - Wrap the helpers in a uniquely-**named** namespace per file
     (`namespace InventoryDragDropOperationLocal { ... }`) **AND
     fully qualify every call site** (e.g.
     `InventoryDragDropOperationLocal::IsInventoryDragDiagEnabled()`).
3. Do **NOT** add `using namespace YourLocal;` at file or function
   scope. In the unity TU, an unqualified call to a helper that also
   exists in a sibling file's anonymous namespace becomes ambiguous.
   `InventoryPanelSurfaceRegistry.cpp` is the model: each call site
   is fully namespace-qualified.
4. Do NOT promote the helpers to a header just to share them - that
   blows up include hygiene and replaces a 5-line problem with a
   30-file rebuild every time the helper changes.

**Real example (2026-04-25):** Three sibling .cpps in
ProjectInventoryUI (`InventoryDragDropOperation.cpp`,
`InventoryPanelSurfaceRegistry.cpp`, `W_NearbyContainerPanel.cpp`)
each defined `IsInventoryDragDiagEnabled` in their own anonymous
namespace, copy-pasted from `InventoryUIDragHostSubsystem.cpp`. The
adaptive unity build concatenated them, multiple definitions
collided, and the failure cascaded into a misleading C7595
format-string error. Fixes:
- `InventoryDragDropOperation.cpp` and
  `InventoryPanelSurfaceRegistry.cpp` -> wrap helpers in named
  namespaces (`InventoryDragDropOperationLocal`,
  `InventoryPanelSurfaceRegistryLocal`) and fully qualify all call
  sites at the call point.
- `W_NearbyContainerPanel.cpp` -> simply rename the helper to
  `NearbyPanel_IsInventoryDragDiagEnabled` (only 4 call sites,
  rename was the smaller diff).

**Avoidance rule for new files:** when copying a helper out of one
.cpp into a sibling, default to a **named** namespace, not anonymous,
and fully qualify the call sites. Reserve anonymous for one-off
helpers that don't share names with anything else in the module. The
cost of a 30-character qualifier is nothing; the cost of a
unity-build collision is a confusing build failure that reads like a
format-string bug.

---

## Map Default Overwrites Valid Slot

**Symptom:** Wrong icon on equip slot (e.g. backpack slot shows footprint).

**Root cause:** `GetEquipSlotGridPos` default returned valid position `(1,2)` that collided with Back slot. Unmapped Accessory slot (processed after Back) overwrote it with footprint fallback.

**Fix:** Default to `(-1,-1)` for unmapped slots - bounds check filters them out.

**File:** `InventoryPanelGridBuilder.cpp` - `GetEquipSlotGridPos()`

## Hands Container Ignores IconCode

**Symptom:** Item in hand shows text name ("Backpack") instead of icon glyph.

**Root cause:** `BuildHandCellVisuals` generic Hands fallback called `BuildEntryLabel()` directly, skipping `Entry.IconCode`. The `CellBuilder::Build()` path (used by LeftHand/RightHand containers) checks IconCode, but the generic Hands container path did not.

**Fix:** Check `Entry.IconCode` first, fall back to `BuildEntryLabel` only when empty.

**File:** `InventoryViewModel.cpp` - `BuildHandCellVisuals()`

## Wrong Font Codepoint in JSON

**Symptom:** Item icon renders as wrong glyph (e.g. backpack shows as footprint).

**Root cause:** `game-icons.ttf` has glyph build errors. Knapsack (U+F831 GID 2096) has wrong outline data. The JSON referenced knapsack instead of backpack.

**Fix:** Verify icon name in `game-icons.css`, use correct name. Backpack = U+F12A (not knapsack U+F831).

**Reference:** `Plugins/UI/ProjectUI/Content/Slate/Fonts/README.txt` KNOWN ISSUES section.

## UE_LOG with PUA Icon Chars Triggers Slate Glyph Warnings

See [ProjectUI pitfalls.md #7](../../ProjectUI/docs/pitfalls.md#7-ue_log-with-puaicon-characters-triggers-slate-glyph-warnings-critical) for full explanation.

**Fixed in:** `InventoryViewModel.cpp` - `BuildHandCellVisuals()`, `W_InventoryPanel.cpp` - `RebuildHandGrids()`

## static const TMap Not Refreshed After Hot Reload

**Symptom:** Icon codepoint change in C++ has no effect after rebuild.

**Root cause:** `static const TMap` initializes once per process. Hot reload loads new code but does not reinitialize static data in already-loaded translation units.

**Fix:** Full editor restart required after changing codepoints in static maps.

**File:** `InventoryPanelGridBuilder.cpp` - `GetEquipSlotIcon()`

## bFromNearbyContainer Uses Stale Cache

**Symptom:** After taking item from nearby container to hand, dragging it back to the container is rejected as "rearrange within nearby not supported."

**Root cause:** `DragOp->bFromNearbyContainer` was set via `IsNearbyEntryInstanceId()` which checks `CachedNearbyEntries`. After take, the InstanceId persists in the cache until the next refresh cycle.

**Fix:** Use `Entry.ContainerId == ProjectTags::Item_Container_WorldStorage` instead - the entry's own ContainerId is authoritative.

**File:** `W_InventoryPanel.cpp` - `NativeOnDragDetected()`

## SetContent Re-parenting Breaks Icon Rendering

**Symptom:** After storing item into nearby container, the icon doesn't appear in the container grid until the next drag interaction.

**Root cause:** `RefreshFromViewModel_Implementation` fires once per VIEWMODEL_PROPERTY change (~30 times per refresh). Each call to `RebuildGrids()` called `ActiveSecondaryGridHost->SetContent(GridPanelSecondary)`, re-parenting the grid widget in Slate. This invalidated STextBlock rendering state, causing SetText to not visually apply.

**Fix:** Guard with `if (Host->GetContent() != Widget)` before calling `SetContent`.

**File:** `W_InventoryPanel.cpp` - `RebuildGrids()`

## Global Inventory VM Can Reopen as Empty Shell

**Symptom:** Nearby loot search opens the inventory screen, but the player side
shows no silhouette, no equip slots, no storage grids, and the nearby panel can
appear as an empty shell.

**Root cause:** The shared global `InventoryViewModel` can lose its live pawn
inventory source across hot reload, reopen, or session-driven panel open paths,
while `W_InventoryPanel` keeps stale runtime host content instead of rebuilding
its grid and hand widgets.

**Fix:** Re-resolve the inventory source from the original initialization
context before showing the panel or binding nearby world storage, and rebuild
runtime panel hosts when content is missing on reopen or theme/layout reload.

**Files:**
- `InventoryViewModel.cpp` - source rebind before show/session open
- `W_InventoryPanel.cpp` - runtime host reset and guarded grid rebuild

## Production Trigger Spawned Only Main Panel; Nearby Sibling Was Never Asked to Exist

**Symptom:** Approaching a real loot container opens the inventory main panel
but the nearby loot panel is completely absent on screen. The dump-tree test
"passed" because it manually called `LayerHost->ShowDefinition` for both
widgets — production only spawned one.

**Root cause:** `ASinglePlayController::HandleInventoryViewModelPropertyChanged`
called `LayerHost->ShowDefinition("...InventoryPanel")` directly and never
spawned the nearby sibling. After the decouple, the nearby widget was
registered in `ui_definitions.json` but had no production trigger asking for
it. The two widgets MUST come up as a unit.

**Fix:** All inventory UI visibility toggles MUST go through
`InventoryUIVisibilityCoordinator::SetInventoryUIVisible(LayerHost, bool)`.
The helper toggles BOTH definitions atomically. Header explicitly forbids
direct `ShowDefinition` calls for either definition id.

**Files:**
- `Source/ProjectInventoryUI/Public/UI/InventoryUIVisibilityCoordinator.h` - the contract
- `SinglePlayController.cpp` - production caller routes through coordinator

**Regression test:** `ProjectIntegrationTests.UI.Framework.Inventory.VisibilityCoordinatorSpawnsBothPanels`
calls `SetInventoryUIVisible(LayerHost, true)` once and asserts both widgets
appear via `GetAllWidgetsOfClass`. **Sabotage-verified**: commenting out the
second `ShowDefinition` in the coordinator makes the test fail with a
specific "nearby container panel MUST also exist" message. Mirror this
pattern for any future spawn-pair: extract a coordinator helper, write the
test, sabotage-verify it.

## Surface OccupantAllowedChecker Reached for Global Drag State and Got Null

**Symptom:** After equipping a backpack, every empty cell in the new backpack
grid showed "unavailable cell" (or silently rejected drops) on a real drag.
Same payload visited cells that were unambiguously empty.

**Root cause:** `RegisterPlayerGridSurfaces` set the surface's
`OccupantAllowedChecker` lambda to call
`UWidgetBlueprintLibrary::GetDragDroppingContent()` to fetch the active
DragOp at validation time. In some lifecycle edges (preview frames after
re-registration on tab/surface churn) that returned null, and
`IsDropOccupantAllowed` then returned false BEFORE its empty-cell
short-circuit (see next entry). Net effect: every empty cell rejected.

**Fix two-part:**
1. **Self-contained payload.** `FProjectUIGridDragPayload` (ProjectUI)
   extended with `Quantity` and `SourceSurfaceTag`. Surface lambdas now
   read everything they need from `Payload`. No global Slate-state lookup.
2. **Anonymous-namespace helper.** `IsPayloadAllowedOnOccupant(WeakVM,
   Payload, OccupantId, bSecondary, [Cell])` in `W_InventoryPanel.cpp`
   replaces the lambda's reach to global state.

**Rule:** Surface callbacks (`EnabledChecker` / `OccupantChecker` /
`OccupantAllowedChecker`) and any lambda registered at setup time MUST NOT
read mutable global state at call time. Plumb data through the parameter
struct.

**Files:**
- `Plugins/UI/ProjectUI/Source/ProjectUI/Public/Interaction/ProjectUIGridDragDropController.h` - extended payload contract
- `W_InventoryPanel.cpp` - `IsPayloadAllowedOnOccupant`, `RegisterPlayerGridSurfaces`
- `W_NearbyContainerPanel.cpp` - `NativeOnDragOver` / `NativeOnDrop` populate full payload

**Regression test:** `ProjectIntegrationTests.UI.Framework.Inventory.BackpackEmptyCellDropAccepts`
spawns the live panel with an empty backpack, waits for Slate paint, calls
`ResolveDropTargetOverSurfaces` at an empty cell, and asserts it succeeds
even with a degenerate payload.

## IsDropOccupantAllowed Empty-Cell Short-Circuit Ran AFTER Null Guard

**Symptom:** A single null DragOp at validation time made every cell on the
grid reject the drop, even cells that were unambiguously empty.

**Root cause:** Original guard order:
```cpp
if (!DragOp) return false;                                      // ← runs first
if (OccupantId == EmptyCellInstanceId || OccupantId == INDEX_NONE) return true;
```
A null DragOp short-circuited before the empty-cell rule had a chance.

**Fix:** Empty-cell short-circuit MUST run first. Domain-identity wins
before resource-availability checks:
```cpp
if (OccupantId == EmptyCellInstanceId || OccupantId == INDEX_NONE) return true;
if (!DragOp) return false;
```

**Rule:** Validator guard order — (1) domain identity short-circuits, (2)
resource preconditions, (3) domain rules. Pair with a property-style test
that asserts `f(empty_input) == identity` even with degenerate auxiliary
state.

**File:** `W_InventoryPanel.cpp` - `IsDropOccupantAllowed`

## Drag-Start Used Drop-Validation API and Refused Occupied Cells

**Symptom:** Picking up an item from a nearby loot cell silently failed.
The grid registered a press but no drag operation started.

**Root cause:** `UW_NearbyContainerPanel::NativeOnDragDetected` called
`ResolveDropTargetOverSurfaces(...)` with a probe payload
(`InstanceId=INDEX_NONE`, `ItemSize=(1,1)`) to identify the pressed cell.
That API runs full footprint validation including occupancy rules — and a
drag source is by definition an occupied cell, so the probe got rejected
every time.

**Fix:** Two distinct controller APIs, do not conflate:
- `ResolveSurfaceCellAtScreenPos(ScreenPos, OutTag, OutCol, OutRow)` — pure
  source-side hit test. No payload, no occupancy. Use at drag-start.
- `ResolveDropTargetOverSurfaces(ScreenPos, Payload, ...)` — hit test PLUS
  footprint validation. Use at drop-resolve.

`NativeOnDragDetected` now uses the source API.

**File:** `W_NearbyContainerPanel.cpp` - `NativeOnDragDetected`

**Regression test:** `ProjectIntegrationTests.UI.Framework.Inventory.NearbyDragStartWorksOnOccupiedCell`
spawns the nearby widget with the loot fixture (Cigarette at (0,0),
InstanceId=1001), waits for paint, and asserts the source-hit API succeeds
on the occupied cell. Also asserts the drop-validation API rejects the
same cell with a probe payload (regression guard against re-conflation).

## Sibling Widget Root with anchor=Fill Steals Drag/Drop From Main Panel

**Symptom:** Opening a loot container makes ALL drag/drop in the main
inventory break. TakeAll button still works (inside nearby widget).
Inventory-only (open via 'I') works. The combination is what breaks.

**Root cause:** `NearbyContainerPanel.json` declares `RootCanvas` with
`anchor: "Fill"` (covers the entire viewport). When `UpdateVisibilityFromViewModel`
set the user-widget root to `Visible`, the full-screen canvas hit-tested
ahead of the sibling `W_InventoryPanel`, so `NativeOnDragOver` /
`NativeOnDrop` on the nearby widget fired for every drag — even those
intended for the main panel grids. The nearby widget either consumed the
event (returned `true`) or returned `false`, and Slate does NOT bubble
sibling-to-sibling for layer-host overlay widgets — so the main panel
never saw the event.

**Fix is two-part - either alone is insufficient:**

1. **`SelfHitTestInvisible` on the user-widget root** when shown (not
   `Visible`). The user-widget root is then skipped during hit-testing,
   while interactive children remain Visible and intercept clicks within
   their own bounds. Empty canvas areas pass through to the sibling.

2. **Per-cell `SetGridMouseDownHandler` binding** in `NativeConstruct`
   via `FInventoryPanelGridBuilder::SetCellMouseDownHandler`. This makes
   each cell catch its own click directly. Without this, the only click
   pathway was the user-widget-level `NativeOnMouseButtonDown` - which
   `SelfHitTestInvisible` skips. Net effect of fix-1-alone: clicking
   nearby cells produced NO logs and drag-from-nearby was dead.

**Rule (general):** Any layer-host widget that needs `SelfHitTestInvisible`
to coexist with siblings MUST also bind cell/control handlers AT THE
CELL/CONTROL level (not at the user-widget level). User-widget level
handlers (`NativeOnMouseButtonDown`, `NativeOnDragDetected`) only fire
when something hit-tests *the user widget* - and `SelfHitTestInvisible`
prevents exactly that.

**Files:**
- `W_NearbyContainerPanel.cpp` - `UpdateVisibilityFromViewModel`, `NativeConstruct` SetCellMouseDownHandler, `HandleNearbyCellMouseDown`

**Regression tests (both required - first alone is theater):**
- `ProjectIntegrationTests.UI.Framework.Inventory.NearbyPanelRootIsHitTransparent` - asserts `GetVisibility() == SelfHitTestInvisible` after binding nearby session. Sabotage-verified.
- `ProjectIntegrationTests.UI.Framework.Inventory.NearbyCellsHaveMouseDownHandler` - constructs the widget with a real loot fixture and asserts EVERY built `UProjectGridCell` has `IsGridMouseDownHandlerBound() == true`. Sabotage-verified: removing the `SetCellMouseDownHandler` call in `NativeConstruct` makes the test fail with "to be 20, but it was 0" - the exact symptom of clicks doing nothing in the user's session.

## Pre-Bound ViewModel Skipped Initial Hand Surface Registration

**Symptom:** After splitting nearby loot into a sibling widget, drag starts
from the nearby panel but cross-panel drops never resolve over the main
inventory hands. Drag diagnostics show only `Item.Container.WorldStorage`
in the controller order.

**Root cause:** `UW_InventoryPanel::NativeConstruct` could call
`RefreshFromViewModel()` before `SurfaceRegistry` existed. That built the
hand/player grids, but the `RegisterHands()` / `RegisterPlayerGrids()` /
`RegisterPockets()` forwarders were guarded on `SurfaceRegistry.IsValid()`
and therefore no-op'ed. Once the registry was created later in the same
construct path, nothing backfilled the already-built surfaces.

**Fix:** After creating and binding `SurfaceRegistry`, immediately
re-register the current player-grid, hand, and pocket surfaces when a
ViewModel is already bound.

**File:** `W_InventoryPanel.cpp` - `NativeConstruct`

**Regression test:** `ProjectIntegrationTests.UI.Framework.Inventory.SurfaceRegistration.PreboundViewModelRegistersHandSurfaces`
pre-binds the ViewModel before `AddToViewport()` and asserts LeftHand and
RightHand surfaces still appear in the shared drag host after construct.

## Cell Wrapper Lost To Screen-Space Re-Resolve

**Symptom:** On first open, drag from nearby loot into hands or player
inventory can fail even though the correct cell wrapper received the drag
event. A nearby-to-nearby move "wakes up" the UI and later drags start
working.

**Root cause:** `UW_InventoryCellDropTarget` forwarded drag-over/drop by
screen position only. After Slate already routed the event to a specific
cell wrapper, the drag host asked the shared controller to resolve the
target again across all registered surfaces. If a sibling surface still
had stale or overlapping cached geometry, the controller could pick that
other surface instead of the wrapper that actually won hit-testing.

**Fix:** Treat the per-cell wrapper as the authoritative semantic target.
`UW_InventoryCellDropTarget` now calls
`UInventoryUIDragHostSubsystem::UpdatePreviewAtTarget` /
`CompleteDropAtTarget`, and the controller validates the nominated
surface/cell directly instead of re-picking by screen position.

**Files:**
- `W_InventoryCellDropTarget.cpp`
- `InventoryUIDragHostSubsystem.cpp`
- `ProjectUIGridDragDropController.cpp`

**Regression test:** `ProjectIntegrationTests.UI.Framework.Inventory.CellDropTarget.DirectTargetWinsOverScreenPosSurface`
registers a higher-priority world surface, sends a drag event to a
LeftHand cell wrapper, and uses a screen point that belongs to the world
surface. Preview must still resolve to LeftHand because Slate already
routed the event to that wrapper.

## Nearby Panel Root Visible During First Frame

**Symptom:** First drag from a freshly-opened world container into the
main inventory cancels without any inventory cell ever firing
`NativeOnDragOver`. After ANY successful drop within the same nearby
container (container cell -> container cell), subsequent container ->
inventory drags work.

Live repro log (`inv.drag.diag 1`):

```
LogInventoryCellDropTarget: NativeOnDragOver Surface=Item.Container.WorldStorage Cell=0 ...
LogInventoryCellDropTarget: NativeOnDragLeave Surface=Item.Container.WorldStorage Cell=0 ...
LogNearbyContainerPanel: NativeOnDragCancelled: Screen=(...) RootVis=Visible
```

The `RootVis=Visible` is the smoking gun -
`UpdateVisibilityFromViewModel` only ever writes `SelfHitTestInvisible`
or `Collapsed`, so the `Visible` had to come from somewhere else.

**Root cause:** The UI factory / layer host stamped UMG's default
`Visible` onto the root during the window between `NativeConstruct`
(which sets `Collapsed`) and the first VM `OnPropertyChanged` (which
would drive `UpdateVisibilityFromViewModel`). Because the panel has
`size_policy: Fill` and a Canvas root, a `Visible` root absorbed drag
events meant for the sibling `W_InventoryPanel` cell wrappers. The
first completed drop inside the nearby container fired a VM property
change, which ran `UpdateVisibilityFromViewModel` and flipped the root
to `SelfHitTestInvisible`. From that point forward, events routed
correctly - hence "works after one in-container drop."

**Fix:** Harden the widget so its root can never be `Visible`:
- `UW_NearbyContainerPanel::SetVisibility` coerces `Visible` input to
  `SelfHitTestInvisible` at the one public entrypoint.
- `UW_NearbyContainerPanel::SynchronizeProperties` re-asserts the
  contract after UMG re-applies reflected properties (e.g. after
  AddToViewport stamping).
- A single `NormalizeNearbyRootVisibility` helper in the cpp keeps the
  rule as one SOT.

`NativeConstruct` still sets `Collapsed` (correct idle state) and
`UpdateVisibilityFromViewModel` still picks `SelfHitTestInvisible` vs
`Collapsed` based on VM state. The override is defense-in-depth.

**Files:**
- `Public/Widgets/W_NearbyContainerPanel.h`
- `Private/Widgets/W_NearbyContainerPanel.cpp`

**Regression tests:**
- `ProjectIntegrationTests.UI.Framework.Inventory.CellDropTarget.NearbyPanelRootCoercedAwayFromVisible`
  pins the visibility contract: `SetVisibility(Visible)` must coerce to
  `SelfHitTestInvisible`, and `SynchronizeProperties` must re-assert
  after any external stamping.
- `ProjectIntegrationTests.UI.Framework.Inventory.CellDropTarget.NearbyFirstDragToInventoryWorks`
  reproduces the real user-visible flow: build a painted nearby panel
  + main inventory fixture, stamp `Visible` on the nearby root pre-drag
  (simulates the factory race), then route a drag-over / drop to an
  inventory cell wrapper. Must emit `PreviewUpdated` / `DropResolved` /
  `Routed` / `VMInvoked` / `Completed`, proving the visibility fix
  unblocks Slate's routing to sibling cells.

**Follow-up (out of scope here):** if the upstream setter turns out to
be `UProjectUILayerHostSubsystem` / `UProjectUIFactorySubsystem`
stamping `Visible` for every `size_policy: Fill` widget, introduce a
`fill_nohittest` size-policy variant so all Fill-filling overlays
default to `SelfHitTestInvisible` without per-widget overrides.
