# Inventory Pitfalls

Verified traps discovered during development. Check before writing inventory code or tests.

## TryAddItem returns QUANTITY, not InstanceId

`UProjectInventoryComponent::TryAddItem(ObjectId, Qty)` returns `int32` = quantity actually added.
It does NOT return an InstanceId. If the item stacks onto an existing entry, no new InstanceId is created.

```cpp
// Wrong:
const uint32 Id = Inventory->TryAddItem(ItemId, 1);
ViewModel->RequestStoreItemInNearbyContainerAt(static_cast<int32>(Id), ...);

// Right:
Inventory->TryAddItem(ItemId, 1);
FInventoryEntry Entry;
Inventory->FindEntryByItemId(ItemId, Entry);
ViewModel->RequestStoreItemInNearbyContainerAt(Entry.InstanceId, ...);
```

## Live loot containers generate seed entries at spawn

JSON definitions with a `lootProfileId` populate RuntimeEntries during `SpawnFromDefinition`.
Tests that take-all then store-back must account for items already in inventory from the take-all phase.
Always resolve InstanceId AFTER all inventory mutations, never assume sequential IDs.

## World-container store path (extraction before commit)

```
ViewModel::RequestStoreItemInNearbyContainerAt
  -> ProjectInventoryComponent::StoreInventoryEntryInWorldContainerResolved
    1. Build CandidateEntry (GridPos from caller, or -1,-1 for auto)
    2. CanStoreContainerEntries (simulated validation)
    3. CaptureInventoryStateSnapshot
    4. TryExtractContainerTransferEntry (REMOVES item from inventory)
    5. StoreContainerEntries on world container
       a. Snapshot RuntimeEntries
       b. TryApplyStoreTransfer
       c. BuildRuntimeEntryViews (post-commit validation)
       d. Rollback on failure
    6. On failure: RestoreInventoryStateSnapshot + RefreshFromInventory
```

Key: extraction happens BEFORE store commit. Rollback covers both sides.

## UI pitfalls affecting inventory

See `Plugins/UI/ProjectInventoryUI/docs/pitfalls.md` for UI traps:
- bFromNearbyContainer must use Entry.ContainerId, not ViewModel cache
- Guard SetContent to prevent redundant re-parenting (icon rendering)

## Effective max stack is container-contextual

Helpers must use `GetEffectiveMaxStack(Container, ItemData)` callback, never `ItemData.MaxStack` directly.
The callback applies `CellDepthUnits` from the container to cap stacking.
Both `FInventoryAddHelper` and `FInventoryLootHelper` assert the callback is bound at runtime.

## Internal_AddItem on Missing definition (FIXED - deferred-pickup pattern)

**Status:** FIXED. `FInventoryAddOutcome` is wired. `Internal_AddItem`
returns `Outcome.bDeferred = true` on `Missing`/`Loading`, and the
interaction handler drives `ObjectDefinitionCache::RequestLoad` + retries.

**Prior symptom (now impossible):** Picking up an item with an unloaded
definition used to silently fail with:
```
LogProjectInventory: Warning: GetItemDataView: ObjectDefinition:Crowbar -> Missing
LogProjectInventory: Warning: Internal_AddItem: Item data unavailable for ObjectDefinition:Crowbar (Missing)
```

**Fix shape:**
- `Internal_AddItem` returns a detailed `FInventoryAddOutcome` with
  `bDeferred` / `Fail` enum instead of swallowing Missing as `0`.
- On `Missing` or `Loading`, sets `Outcome.bDeferred = true` and logs
  `"Internal_AddItem: deferred (<state>) for <id> - handler will RequestLoad"`
  at Log level (not Warning).
- Terminal failures (`InvalidProvider`, `InvalidData`, `InvalidRequest`)
  set `Outcome.Fail` and broadcast a user-facing error via
  `BroadcastError`.
- `FInventoryInteractionHandler` queues deferred intents and re-enters
  the add path when the cache resolve callback fires.

**Canonical pattern:** See `docs/agents/canonical.md` section 9
"Deferred-pickup + loud-error pattern" for reuse in other cross-async-load
races. Regression coverage: `InventoryDeferredPickupIntegrationTest.cpp`
(7 tests including deterministic-cache race reproductions).

**Files:**
- `Source/ProjectInventory/Private/Components/ProjectInventoryComponent.cpp` - `Internal_AddItem`, `TryAddItemDetailed`
- `Source/ProjectInventory/Private/Interaction/InventoryInteractionHandler.cpp` - deferred-intent queue, cache callback re-entry

## Session leak between integration tests

`InventoryLootPlaces` tests sharing a PIE world leak `ProjectContainerSessionSubsystem` state.
Tests that open sessions and destroy the PlayerController prevent subsequent tests from opening new sessions.

Symptoms: "A container session is already active for this player" or `CreateInventoryLootPlacesPlayerInventory` returns null.

```powershell
# Stable batch (no session coupling):
-TestFilter "ProjectIntegrationTests.InventoryLootPlaces.Inventory+...World+...Content"

# Must run isolated (session-dependent):
-TestFilter "ProjectIntegrationTests.InventoryLootPlaces.UI.ViewModelStoreFailureRefreshesInventoryState"
-TestFilter "ProjectIntegrationTests.InventoryLootPlaces.UI.FullOpenEmptyContainerStaysOpenAndAcceptsStore"
```

## Drop From Equip Slot Leaves Granted Container In UI

**Symptom:** Player drags an equipped backpack off the silhouette and
releases on no valid drop target. The backpack item drops to the world
correctly, but the 6x6 backpack grid stays visible in the inventory UI.
Cells the equipped item added do not disappear after the drop.

Repro log:
```
LogProjectInventory: Dropped 1 x ObjectDefinition:Backpack at X=...
LogInventoryVM: RefreshFromInventory: 0 entries, 3 containers
   Container: Item.Container.LeftHand (2x2)
   Container: Item.Container.RightHand (2x2)
   Container: Item.Container.Backpack (6x6)   <-- still 6x6 after drop
```

**Root cause:** `GetEffectiveContainers` in
`ProjectInventoryComponent_Containers.cpp:101-134` derives the granted
containers (e.g. `Item.Container.Backpack` 6x6) by iterating
`EquippedItems` and reading each equipped item's
`ItemData.ContainerGrants` (or the slot-level fallback). The grant is
implicit: present iff `EquippedItems[Slot]` is present.

`Internal_UnequipItem` (line 1645) correctly does
`EquippedItems.Remove(EquipSlot)` so the grant disappears on the normal
unequip path. But `Server_DropItem` (in
`ProjectInventoryComponent_Mutation.cpp`) only called
`Internal_RemoveItem` and **never touched `EquippedItems`**. The item
was removed from the inventory but the equip slot map entry stayed,
keeping the granted container alive forever.

**Fix:** New helper `Internal_RevokeEquipSlotForRemoval(InstanceId,
ItemData)` in `ProjectInventoryComponent_Mutation.cpp`. Mirrors
`Internal_UnequipItem`'s storage-empty validation but skips the
rehome-to-hand step (the caller is destroying the item). Pure cleanup:
GAS abilities released via `UProjectAbilitySet::TakeFromAbilitySystem`,
`EquippedItems.Remove(Slot)`. `Server_DropItem` calls it BEFORE the
spawn, so a non-empty backpack drop is blocked atomically with a
player-facing error ("Cannot drop - empty the equipped item's storage
first"), matching the unequip UX.

**Files:**
- `Public/Components/ProjectInventoryComponent.h` - declaration
- `Private/Components/ProjectInventoryComponent_Mutation.cpp` - helper
  body + call from `Server_DropItem_Implementation`

**Why no automated test was added:** the bug-relevant code path needs a
fully-equipped item (real `ObjectDefinition` data + ASC) to exercise
the storage-empty / GAS-cleanup branches. A unit test of the
not-equipped early-out would catch nothing meaningful. Manual repro is
30 seconds (equip backpack, drop from silhouette, observe cells
disappear). Future Phase 5 Layer A "equip-grant lifecycle" integration
test (per `docs/agents/canonical.md` section 7) should add the equip ->
drop -> verify-revoke flow alongside equip -> unequip.

**Investigation pattern (reusable):** when a UI shows stale state after
a successful backend mutation, check whether the backend's *derived*
state (in this case `GetEffectiveContainers` reading `EquippedItems`)
has a corresponding *source* update on the mutation path. The drop
path updated `Inventory.Entries` but not `EquippedItems`; derived
state is only as fresh as its sources.
