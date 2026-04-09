# Inventory UI Golden Standard Refactor

Status: planned

Parent SOT:
- `../../Plugins/Features/ProjectInventory/docs/design_vision.md`

Related router:
- `implement_inventory_vision.md`

Framework references:
- `../../Plugins/UI/ProjectUI/README.md`
- `../../Plugins/UI/ProjectUI/docs/framework_consolidation.md`
- `../../Plugins/UI/ProjectUI/docs/ui_layout.md`
- `../../Plugins/UI/ProjectUI/docs/ui_mvvm.md`
- `../../Plugins/UI/ProjectInventoryUI/README.md`
- `../../Plugins/UI/ProjectInventoryUI/docs/architecture.md`
- `../../Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Interfaces/README.md`

---

## Problem

The current Shipping fix for the inventory panel is acceptable as a short-term
stabilization, but it is not the gold-standard architecture.

What is good today:
- The panel keeps the intended horizontal composition.
- Hands remain separate from storage.
- Nearby loot remains a world-container presentation, not a fake player
  storage container.
- The current implementation already reflects the intended high-level split
  into hands, compact storage, and large storage; the remaining problem is that
  this contract is implicit and tag-derived instead of explicit and shared.

What is not good enough:
- Layout width is currently decided by panel-local hardcoded numbers in
  `W_InventoryPanel.cpp`.
- Player storage presentation is not driven by one explicit container
  descriptor model.
- The ViewModel still infers pocket vs backpack presentation from container
  tags.
- The panel consumes multiple shapes:
  - pocket-specific arrays
  - large-container tab labels
  - special nearby section state

This means the implementation still relies on implicit UI semantics rather than
an explicit contract.

---

## Golden Standard

The target architecture is:

- Inventory domain publishes one explicit player-storage presentation contract.
- That contract lives in `ProjectCore`, not in `ProjectInventoryUI`.
- `ProjectInventory` owns how descriptors are built from grants/configuration.
- `ProjectInventoryUI` consumes descriptors only and renders generically.
- Hands remain separate from descriptor-driven storage, per SOT.
- Nearby loot remains a separate world-container presentation model, not mixed
  into player storage descriptors.
- Responsive shell sizing should be data-driven through reusable `ProjectUI`
  layout primitives, not hardcoded per-panel width constants.

The desired result is:

- no tag-name inference in the UI layer
- no pocket-specific vs large-storage-specific public ViewModel shapes
- no Shipping-only geometry heuristic in the panel
- one universal rendering path for granted storage containers

This is not a new framework direction. It aligns with existing `ProjectUI`
rules already documented in the repo:
- descriptor-driven rendering instead of named-container branching
- feature plugins own domain semantics
- reusable layout and interaction primitives belong in `ProjectUI`

---

## Current Architecture Gaps

### 1. Missing explicit presentation descriptor

Current public snapshots expose physical facts only:
- `FInventoryContainerGrantView`
- `FInventoryContainerConfig`
- `FInventoryContainerView`

They do not expose:
- display label
- presentation group
- order key

Because of that, the ViewModel rebuilds presentation semantics from tag names
instead of consuming an explicit contract.

### 2. ViewModel owns UI inference it should not own

`UInventoryViewModel::BuildContainerData()` currently:
- splits storage into pockets and large containers
- sorts pockets by canonical tags
- prioritizes backpack by tag
- feeds separate label arrays and grid dimensions

This is still data shaping, but it is not the right contract shape.

### 3. Panel is structurally generic in parts, but not at the container-host level

`UW_InventoryPanel` still has separate build paths for:
- pockets
- lower large-container tabs/grids
- nearby section

The panel is therefore generic at cell/grid level, but not at container
presentation level.

### 4. Missing responsive layout primitive in ProjectUI

The layout loader already supports canvas auto-size wrapping via
`*_AutoSizer`, but there is no reusable JSON-driven way to express:

- autosize content
- keep a minimum width
- clamp to viewport / maximum width

That missing primitive is why the panel ended up with local width overrides.

Important nuance:
- `ProjectUI` docs already describe `Clamp`/`SizeBox` constraints as the
  intended layout model.
- Current loader code does not yet expose a full generic min/max width policy
  for arbitrary JSON-built `SizeBox` wrappers.
- This phase should therefore align framework implementation with existing
  documented intent, not invent a brand-new layout concept.

### 5. Test coverage does not fully protect the target behavior

There is good coverage for:
- pocket ordering
- nearby loot dump creation
- general multi-resolution inventory dump

But there is no dedicated nearby-loot multi-resolution validation and no
assertion that the horizontal composition retains enough width for intended
layout at runtime.

---

## Recommended Contract Design

### Add explicit player-storage presentation descriptor in ProjectCore

Create a new UI-facing, domain-owned descriptor in `ProjectCore`.

Suggested shape:

```cpp
enum class EInventoryContainerPresentationGroup : uint8
{
    CompactStorage,
    LargeStorage
};

USTRUCT(BlueprintType)
struct PROJECTCORE_API FInventoryContainerPresentationDescriptor
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FGameplayTag ContainerId;

    UPROPERTY(BlueprintReadOnly)
    FText Label;

    UPROPERTY(BlueprintReadOnly)
    EInventoryContainerPresentationGroup PresentationGroup =
        EInventoryContainerPresentationGroup::LargeStorage;

    UPROPERTY(BlueprintReadOnly)
    int32 OrderKey = 0;
};
```

Why a separate descriptor instead of stuffing UI fields into
`FInventoryContainerView`:
- keeps physical container state separate from presentation state
- avoids turning every raw container snapshot into a UI policy object
- matches the SOT language directly

Join rule:
- UI joins `FInventoryContainerPresentationDescriptor` with
  `FInventoryContainerView` by `ContainerId`
- physical size and capacity remain owned by `FInventoryContainerView`
- presentation semantics remain owned by the descriptor

### Keep nearby loot outside the player-storage descriptor list

Nearby loot is a world-container interaction mode. It should continue using:
- `IWorldContainerSessionSource`
- `FInventoryContainerView`
- `FInventoryEntryView`
- explicit nearby label/stat state

It should not be inserted into player storage descriptors just to satisfy one
panel layout.

### Recommended ownership

- `ProjectCore`
  - owns descriptor type and presentation-group enum
- `ProjectInventory`
  - owns descriptor production from effective containers
- `ProjectInventoryUI`
  - owns generic rendering of descriptor groups
- `ProjectUI`
  - owns reusable responsive layout primitives

This keeps module boundaries clean.

Relevant existing framework guidance:
- `ProjectUI` already requires descriptor-driven rendering over named-container
  branching.
- `ProjectInventoryUI` already documents the current split into dedicated hands,
  compact storage, and large storage as an implementation detail.
- `ProjectCore` interface docs require stable abstractions and careful contract
  ownership.

---

## Recommended Runtime Flow

### Before

```text
Inventory source
  -> raw container views
  -> ViewModel infers pockets/backpack by tags
  -> ViewModel exposes split arrays
  -> panel builds separate hosts
  -> panel widens itself with hardcoded widths
```

### After

```text
Inventory source
  -> raw container views
  -> ProjectInventory builds presentation descriptors explicitly
  -> descriptors and container views are rebuilt and exposed from the same
     effective snapshot boundary
  -> ViewModel exposes one descriptor list for player storage
  -> panel joins descriptor + container snapshot by ContainerId
  -> panel renders CompactStorage and LargeStorage generically
  -> ProjectUI layout system handles responsive width constraints from data
```

---

## Phases

## Phase 1 - Add explicit contracts in ProjectCore

Goal:
- Introduce the missing presentation contract without changing behavior yet.

Tasks:
- [ ] Add `EInventoryContainerPresentationGroup` to `ProjectCore`
- [ ] Add `FInventoryContainerPresentationDescriptor` to `ProjectCore`
- [ ] Decide interface shape cleanly:
      extend `IInventoryReadOnly` if it remains the canonical UI snapshot
      contract, otherwise add a dedicated presentation read-only interface
- [ ] Add a read-only accessor for player-storage presentation descriptors on
      the selected interface shape
- [ ] Keep existing `GetContainersView()` unchanged for physical/container
      consumers
- [ ] Define how descriptor refresh/invalidation tracks container-view refreshes
- [ ] Ensure descriptor snapshots and container snapshots are rebuilt and
      exposed from the same effective inventory snapshot boundary

Candidate files:
- `Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Inventory/InventoryPresentationTypes.h`
- `Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Interfaces/IInventoryReadOnly.h`

Notes:
- Do not merge nearby loot into this contract.
- Keep descriptor naming semantic, not widget-oriented.
- Current default recommendation: extend `IInventoryReadOnly`, because it
  already exposes UI-facing snapshot views rather than raw internals.

## Phase 2 - Produce descriptors in ProjectInventory

Goal:
- Make inventory domain own presentation semantics explicitly.

Tasks:
- [ ] Build descriptors from effective player containers in
      `ProjectInventoryComponent`
- [ ] Assign labels explicitly instead of generating them only in the UI layer
- [ ] Assign `PresentationGroup` explicitly
- [ ] Assign `OrderKey` explicitly
- [ ] Preserve naked baseline = hands only
- [ ] Preserve backpack-first ordering inside `LargeStorage`
- [ ] Preserve canonical pocket ordering inside `CompactStorage`
- [ ] Cache descriptor snapshots with the same lifetime/update cadence as
      container snapshots and publish them from the same snapshot boundary

Candidate files:
- `Plugins/Features/ProjectInventory/Source/ProjectInventory/Public/Components/ProjectInventoryComponent.h`
- `Plugins/Features/ProjectInventory/Source/ProjectInventory/Private/Components/ProjectInventoryComponent.cpp`
- `Plugins/Features/ProjectInventory/Source/ProjectInventory/Private/Helpers/InventoryViewHelper.cpp`

Notes:
- Descriptor production should happen close to the inventory domain, not in the
  UI plugin.
- If labels remain derived from tags for now, do it here, not in UI code.

## Phase 3 - Simplify the ViewModel to one player-storage descriptor list

Goal:
- Reduce `UInventoryViewModel` to pass-through snapshot mapping plus local UI
  state.

Tasks:
- [ ] Add `PlayerStorageDescriptors` to the ViewModel
- [ ] Stop exposing pocket-specific presentation arrays as the main rendering
      contract
- [ ] Replace large-container label arrays with descriptor-driven selection
- [ ] Keep enough compatibility fields temporarily for staged migration if
      needed
- [ ] Keep nearby state separate
- [ ] Keep ViewModel logic focused on selection state, active container state,
      and nearby separation rather than reinterpreting descriptor semantics

Candidate files:
- `Plugins/UI/ProjectInventoryUI/Source/ProjectInventoryUI/Public/MVVM/InventoryViewModel.h`
- `Plugins/UI/ProjectInventoryUI/Source/ProjectInventoryUI/Private/MVVM/InventoryViewModel.cpp`

Notes:
- It is acceptable to keep transitional getters during migration, but the final
  goal is one descriptor-driven player-storage model.
- Descriptor ordering/grouping should be tested in `ProjectInventoryTests`,
  not redefined in ViewModel tests.

## Phase 4 - Refactor the panel to render descriptor groups generically

Goal:
- Make `UW_InventoryPanel` generic at the container-host level, not just cell
  level.

Tasks:
- [ ] Replace `RebuildPocketGrids()` special handling with descriptor-driven
      compact-container rendering
- [ ] Replace large-container tab assumptions with descriptor-driven
      bottom-large rendering
- [ ] Introduce one generic runtime record for descriptor-backed containers
- [ ] Join descriptor data with `FInventoryContainerView` by `ContainerId`
      instead of duplicating physical container dimensions in the descriptor
- [ ] Keep hands as separate fixed hosts
- [ ] Keep nearby section as separate world-container host
- [ ] Remove tag-based rendering assumptions from the panel

Candidate files:
- `Plugins/UI/ProjectInventoryUI/Source/ProjectInventoryUI/Public/Widgets/W_InventoryPanel.h`
- `Plugins/UI/ProjectInventoryUI/Source/ProjectInventoryUI/Private/Widgets/W_InventoryPanel.cpp`
- `Plugins/UI/ProjectInventoryUI/Source/ProjectInventoryUI/Private/Widgets/InventoryPanelGridBuilder.cpp`
- `Plugins/UI/ProjectInventoryUI/Data/InventoryPanel.json`

Notes:
- Do not create per-clothing widget paths.
- The panel should render descriptors by group and order only.

## Phase 5 - Move responsive shell sizing into ProjectUI primitives

Goal:
- Remove hardcoded width heuristics from inventory panel code.

Tasks:
- [ ] Audit the documented `Clamp`/`SizeBox` policy in `ProjectUI` docs against
      actual loader behavior and close the gap
- [ ] Extend the JSON layout pipeline to express reusable width constraints on
      size wrappers
- [ ] Support reusable `SizeBox` width policies such as min/max desired width
- [ ] Represent the inventory shell width policy in JSON/theme data where
      possible
- [ ] Delete panel-local width constants after migration

Candidate files:
- `Plugins/UI/ProjectUI/Source/ProjectUI/Private/Layout/ProjectWidgetLayoutLoader.cpp`
- `Plugins/UI/ProjectUI/Source/ProjectUI/Private/Layout/LayoutPropertyAppliers.cpp`
- `Plugins/UI/ProjectInventoryUI/Data/InventoryPanel.json`
- `Plugins/UI/ProjectInventoryUI/Source/ProjectInventoryUI/Private/Widgets/W_InventoryPanel.cpp`

Notes:
- Prefer a reusable primitive over a custom inventory-only widget.
- If a pure JSON solution is not enough, add a generic `ProjectUI` helper, not
  an inventory-specific geometry rule.

## Phase 6 - Strengthen tests around the real contract

Goal:
- Protect the intended architecture and the runtime layout.

Tasks:
- [ ] Add unit tests for descriptor production and ordering
- [ ] Add ViewModel tests for descriptor pass-through, selection state, and
      nearby separation
- [ ] Add nearby-loot multi-resolution UI test, not only hands/storage
- [ ] Add assertions for descriptor-driven group visibility and ordering
- [ ] Add `ProjectUI` width-policy tests for the reusable responsive layout
      primitive
- [ ] Add one inventory integration composition regression test that catches
      horizontal-compression regressions without overfitting to pixel output

Candidate files:
- `Plugins/Features/ProjectInventory/Source/ProjectInventoryTests/`
- `Plugins/Test/ProjectIntegrationTests/Source/ProjectIntegrationTests/Private/Integration/ProjectUIInventoryDumpTreeTest.cpp`
- `tools/agentic/ui/layout_report.py`

Notes:
- Prefer inventory-specific semantic assertions over overly generic geometry
  heuristics when possible.
- Semantic assertions are primary. Geometry assertions are secondary and should
  validate layout policy, not screenshot-exact output.
- Add `ProjectCoreTests` only if real contract logic or validation helpers end
  up living in `ProjectCore`.

## Phase 7 - Cleanup

Goal:
- Remove transitional code and freeze the new contract.

Tasks:
- [ ] Delete obsolete pocket/large-container presentation getters once no
      callers remain
- [ ] Remove panel-local responsive width constants
- [ ] Remove temporary compatibility branches
- [ ] Update docs to point to the descriptor-driven contract as implemented

---

## Explicit Non-Goals

- Do not fold nearby loot into the same descriptor list as player storage.
- Do not put rendering semantics into ad hoc widget names or JSON node names.
- Do not add per-item or per-clothing widget branches.
- Do not break the naked baseline = hands only.
- Do not move inventory orchestration into the UI plugin.

---

## Recommended Implementation Order

1. Add `ProjectCore` descriptor contract.
2. Produce descriptors in `ProjectInventory`.
3. Expose descriptor list in `UInventoryViewModel`.
4. Refactor `UW_InventoryPanel` to consume descriptors.
5. Add reusable `ProjectUI` width constraints.
6. Remove hardcoded panel width logic.
7. Expand tests and remove compatibility code.

This order keeps behavior stable while moving ownership in the right direction.

---

## Completion Criteria

This plan is complete when all of the following are true:

- Player storage presentation is driven by one explicit descriptor contract.
- `ProjectInventoryUI` no longer infers layout groups from container tags.
- `ProjectInventoryUI` joins presentation descriptors with raw container views
  by `ContainerId` rather than owning duplicate physical container fields.
- `UW_InventoryPanel` renders player storage generically by descriptor group and
  order.
- Nearby loot remains a separate world-container presentation path.
- Panel shell sizing is not controlled by inventory-specific hardcoded width
  constants.
- Tests fail if descriptor ordering/grouping, width-policy behavior, or final
  horizontal composition regresses.
