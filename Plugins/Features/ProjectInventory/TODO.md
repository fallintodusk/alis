# ProjectInventory TODO

Status (2026-01-29)
- DONE: Feature registration, component attach, FastArray replication, GAS use/equip, pickup and loot via IPickupSource/ILootSource, drop spawn via IObjectSpawnService.
- DONE: Item data view includes Description, IconPath, Volume, GridSize. Weight and volume enforced on add path.
- DONE: Containers + grid placement + Server_MoveItem validation.
- DONE: Catalog warmup uses ObjectCatalog.
- DONE: Inventory UI drag/drop, container tabs, equip slots list, command bar.
- DONE: Stack split/merge rules, container grants, persistence shape and stub wiring.

---

## Phase 0 - Feature registration (DONE)
- Register "Inventory" feature in FFeatureRegistry.
- Attach UProjectInventoryComponent during GameMode feature init.

## Phase 1 - Core data and replication (DONE)
- FInventoryEntry + FInventoryList using FFastArraySerializer.
- InstanceId for stable identity (not array index).

## Phase 2 - GAS integration (DONE)
- UseItem -> ApplyMagnitudes (SetByCaller).
- EquipItem -> AbilitySet grant and revoke.

## Phase 3 - Interaction and world pickup (DONE)
- InventoryInteractionHandler subscribes to IInteractionService.
- IPickupSource -> TryAddItem -> Consume.
- ILootSource -> CanFitItems -> AddItemsBatch -> ConsumeLootSource.

## Phase 4 - ObjectDefinition data source (DONE)
- Item data read via IItemDataProvider (ObjectDefinition Item section).
- ObjectDefinitionCache for async warmup.

## Phase 5 - Capacity rules (DONE)
- Enforce MaxWeight and MaxVolume on add path and CanFitItems.
- CurrentWeight and CurrentVolume query APIs.

## Phase 6 - Containers and grid (DONE)
- DONE: ContainerId on entries.
- DONE: GridPos placement rules (with rotation option).
- DONE: Container grants on equip (block unequip if container not empty).

## Phase 7 - UI (DONE)
- ProjectInventoryUI grid with drag/drop and container tabs (DONE).
- Equip slots list and command bar (DONE).
- Inventory input action (I key) and UI layer routing (DONE).

## Phase 8 - RPC validation (DONE)
- Server_MoveItem(InstanceId, FromContainer, FromPos, ToContainer, ToPos, Quantity).
- Validate overlap, stacking, weight/volume, ownership, and bounds.

## Phase 9 - Catalog warmup update (DONE)
- Warmup ObjectDefinitionCache from ObjectCatalog.

## Phase 10 - Persistence (DONE)
- Define serialization shape for SaveGame or backend profile (DONE).
- Add ProjectSave PluginBinaryData stub + inventory serialize/deserialize helpers (DONE).

---

## Validation checklist
- OwnerComponent set on server and client (PostInitProperties + OnRep).
- Never RPC component pointers (use Actor + ContainerId).
- Drop spawns correct pickup quantity.
