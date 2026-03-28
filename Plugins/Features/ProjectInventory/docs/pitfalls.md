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
