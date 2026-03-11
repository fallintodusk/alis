# ProjectInventory Architecture

Overview
- Server-authoritative, replicated inventory system using FastArray replication.
- Item data comes from ObjectDefinition.Item via IItemDataProvider.
- Pickup and loot use ProjectCore interfaces (IPickupSource, ILootSource).

Design Principles
- Data-driven: ObjectDefinition holds item identity and rules.
- Replicated: FFastArraySerializer for delta updates.
- Modular: Container grid is implemented in data/model; UI still evolves.
- GAS-native: UseItem applies SetByCaller magnitudes, EquipItem grants AbilitySets.
- Item-driven containers: equipment definitions grant container grids; slot grants are fallback only.

Layer Summary

Layer 1 - Item Data (static)
- Source: ObjectDefinition Item section (ProjectObject).
- Access: IItemDataProvider -> FItemDataView.
- Fields:
  - DisplayName, Description, IconPath, Tags
  - WeightPerUnit, VolumePerUnit (per-unit physical cost)
  - GridSize (W, H) - footprint in cells
  - MaxStack - max quantity per stack
  - UnitsPerDepthUnit (1x1 items) - quantity per depth unit for stacking
  - Magnitudes, EquipSlotTag, ContainerGrants
  - Container fields: CellDepthUnits (stack height limit per cell)
- Notes:
  - TotalWeight = WeightPerUnit × Quantity
  - TotalVolume = VolumePerUnit × Quantity
  - For 1x1 items: DepthUsed = ceil(Quantity / UnitsPerDepthUnit)
- See [design_vision.md#depth-stacking-kiss-voxel-model](design_vision.md#depth-stacking-kiss-voxel-model) for full depth model spec.

Layer 2 - Runtime Inventory (replicated)
- FInventoryEntry (FastArray item): InstanceId, ItemId, Quantity, ContainerId, GridPos, bRotated, InstanceData, OverrideMagnitudes.
- FInventoryList (FastArray container): delta replication, entry callbacks.

Layer 3 - Inventory Component (authority)
- UProjectInventoryComponent holds FInventoryList and exposes server RPCs.
- Enforces capacity (weight and volume).
- Uses ObjectDefinitionCache for async warmup.
- Validates move operations (Server_MoveItem) with overlap/stack/capacity checks.
- For 1x1 items: enforces DepthUsed <= container.CellDepthUnits (MVP: one stack per cell).
- Applies equipment container grants from item data (pockets, backpack) with unequip guard.
- Default slot grants: Legs -> Pockets1/Pockets2, Chest -> Pockets3/Pockets4, Back -> Backpack (used when item has no ContainerGrants).
- Standalone persistence: auto-load AutoSave on BeginPlay, auto-save on EndPlay (ProjectSaveSubsystem).
- Persistence applies to player-controlled pawn inventories only.

Rationale (inventory containers)
- Container capacity should be owned by the item definition (pants/backpack variants).
- Slot grants exist only to keep old content functional during migration.
- Hands-only default enforces "naked = hands only" gameplay rule.
- Unequip overflow policy: block unequip if any granted container is not empty.

Layer 4 - Interaction
- InventoryInteractionHandler subscribes to IInteractionService.
- Pickup: IPickupSource -> TryAddItem -> Consume.
- Loot: ILootSource -> CanFitItems -> AddItemsBatch -> ConsumeLootSource.

Drop
- Server_DropItem uses IObjectSpawnService to spawn world actor from ObjectDefinition, then removes items.

Planned Extensions
- Container UI: multiple grids + equip slots.
- Define final overflow policy for equipment removal (block vs auto-move vs spill).

Key Files
- Inventory component: Source/ProjectInventory/Public/Components/ProjectInventoryComponent.h
- Inventory list: Source/ProjectInventory/Public/Inventory/InventoryTypes.h
- Interaction handler: Source/ProjectInventory/Private/Interaction/InventoryInteractionHandler.cpp
- ObjectDefinition data: Plugins/Resources/ProjectObject/Source/ProjectObject/Public/Data/ObjectDefinition.h
- Item data view: Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Interfaces/IItemDataProvider.h

## Cross-Plugin Integration

### Feature Activation Chain
1. ProjectFeature defines FFeatureRegistry and FFeatureInitContext.
2. ProjectSinglePlay calls InitializeFeatures in HandleStartingNewPlayer (server) and passes Context.
3. ProjectInventory registers "Inventory" and attaches UProjectInventoryComponent to the Pawn at init time.

Refs:
- Plugins/Features/ProjectFeature/README.md
- Plugins/Gameplay/ProjectSinglePlay/README.md
- Source/ProjectInventory/Private/ProjectInventory.cpp

### Interaction Flow (Server-Authoritative)
1. ProjectInteraction re-traces on server and broadcasts IInteractionService::OnInteraction(Target, Instigator).
2. InventoryInteractionHandler handles IPickupSource and ILootSource.

Refs:
- Plugins/Gameplay/ProjectInteraction/README.md
- Source/ProjectInventory/Private/Interaction/InventoryInteractionHandler.cpp
- Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Interfaces/IPickupSource.h
- Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Interfaces/ILootSource.h

### Object Data and Capabilities
- ObjectDefinition holds Item and Loot sections (weight, volume, grid size, tags, ability set info).
- PickupCapability and LootContainerCapability expose IPickupSource and ILootSource.

Refs:
- Plugins/Resources/ProjectObject/Source/ProjectObject/Public/Data/ObjectDefinition.h
- Plugins/Gameplay/ProjectObjectCapabilities/Source/ProjectObjectCapabilities/Public/Pickup/PickupCapabilityComponent.h
- Plugins/Gameplay/ProjectObjectCapabilities/Source/ProjectObjectCapabilities/Public/LootContainer/LootContainerCapabilityComponent.h

### GAS Boundaries
- Inventory uses UProjectGASLibrary::ApplyMagnitudes for consumables, and AbilitySet grant on equip.
- ASC ownership lives in ProjectCharacter (not inventory).
- ProjectVitals owns State.* tags and tick logic (not inventory).
- Inventory sets State.Weight.* tags on ASC based on current weight ratio.
- VitalsComponent applies debuffs (speed, stamina regen) based on weight tags.

### Error Feedback (Multiplayer-Safe)
- `FOnInventoryError` delegate broadcasts error messages to UI.
- `BroadcastError()` routes messages: locally controlled → direct broadcast, server → Client RPC.
- `Client_InventoryError` RPC delivers messages to owning client on dedicated servers.
- Error types: "Too heavy", "No space", "Cannot unequip - pockets not empty".

Refs:
- Source/ProjectInventory/Private/Components/ProjectInventoryComponent.cpp
- Plugins/Gameplay/ProjectGAS/README.md
- Plugins/Gameplay/ProjectCharacter/README.md
- Plugins/Gameplay/ProjectVitals/README.md

## Tags Vocabulary (ProjectCore)

| Category | Examples | Purpose |
|----------|----------|---------|
| Item.Type.* | Item.Type.Consumable, Item.Type.Equipment | Item classification |
| Item.Survival.* | Item.Survival.Food, Item.Survival.Drink | Survival item subtypes |
| Item.EquipmentSlot.* | Item.EquipmentSlot.Head, Item.EquipmentSlot.Back | Equipment slot targeting |
| Item.Container.* | Item.Container.Hands, Item.Container.Backpack | Container identification |
| SetByCaller.* | SetByCaller.Condition, SetByCaller.Hydration | GAS magnitude tags |
| State.* | State.Hunger, State.Thirst | Vitals state (owned by ProjectVitals) |

Ref: Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/ProjectGameplayTags.h

## External References

- Epic: FFastArraySerializer API - https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/NetCore/FFastArraySerializer
- Epic: Lyra Inventory and Equipment - https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-inventory-and-equipment-in-unreal-engine
- Epic: Asset Manager (Primary Assets) - https://dev.epicgames.com/documentation/en-us/unreal-engine/asset-management-in-unreal-engine
- Epic: Data Assets - https://dev.epicgames.com/documentation/en-us/unreal-engine/data-assets-in-unreal-engine
- Forum: FFastArraySerializer removal replication - https://forums.unrealengine.com/t/ffastarrayserializer-fully-re-replicates-on-element-removal-instead-of-sending-only-the-delta/2655967
