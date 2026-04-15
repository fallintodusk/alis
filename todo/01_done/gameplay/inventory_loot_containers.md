# Loot Containers System

Add loot container actors (loot boxes, bags, crates) to ProjectInventory plugin.

**Docs:** See [ProjectInventory architecture.md](../../Plugins/Features/ProjectInventory/docs/architecture.md#loot-container-projectinventory) for design details and integration patterns.

## Implementation Tasks

### Phase 1-3: Core Implementation [DONE]
- [x] Create `Loot/LootTypes.h` with `FLootEntry` struct
- [x] Add `CanFitItems()`, `AddItemsBatch()`, `GetCurrentWeight()` to inventory component
- [x] Create `AProjectLootContainerActor` with `TryLootAll()`, `IsInRange()` validation

### Phase 4: Testing
- [ ] Unit tests for CanFitItems (empty, partial stack, full, overweight)
- [ ] Smoke test: place container, interact, verify transfer + despawn
- [ ] Network test: client interaction -> server validation (2-client PIE)

## Files Created/Modified

**New:**
- `Plugins/Features/ProjectInventory/Source/ProjectInventory/Public/Loot/LootTypes.h`
- `Plugins/Features/ProjectInventory/Source/ProjectInventory/Public/Loot/ProjectLootContainerActor.h`
- `Plugins/Features/ProjectInventory/Source/ProjectInventory/Private/Loot/ProjectLootContainerActor.cpp`

**Modified:**
- `ProjectInventoryComponent.h/.cpp` - Added loot batch operations
- `ProjectInventory.Build.cs` - Added ProjectWorld dependency

## Future Extensions (Out of Scope)

- Loot tables with weighted random rolls
- Container UI for manual item selection
- Persistent containers (chests that don't despawn)
- Partial loot (take what fits)
