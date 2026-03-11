# ProjectInventory

Purpose
- Server-authoritative inventory feature for ALIS.
- Owns item storage, stacking, equip/use, and drop logic.
- Handles pickup and loot via ProjectCore interfaces.

Activation
- OnDemand feature registered via FFeatureRegistry.
- GameMode (ProjectSinglePlay / ProjectOnlinePlay) initializes the feature and attaches UProjectInventoryComponent to the Pawn.

Item Data Source
- Item data is read from ObjectDefinition.Item via IItemDataProvider.
- Inventory uses FPrimaryAssetId for lightweight replication and lookups.
- Optional warmup: ObjectCatalog (ProjectCore) lists ObjectDefinition IDs to preload.
- Consumable magnitudes follow ProjectGAS units: Hydration in liters (L), Calories in kilocalories (kcal).

Containers and Equipment Grants
- Default container is Item.Container.Hands (2 cells) for "naked" characters.
- Equipment can grant additional containers via ObjectDefinition.Item.ContainerGrants.
- Fallback grants can be configured per equip slot (EquipSlotContainerGrants).
- Common container tags: Item.Container.Hands, Item.Container.Pockets1-4, Item.Container.Backpack.
- Default slot grants (when item has no ContainerGrants):
  - Legs -> Pockets1 + Pockets2 (pants)
  - Chest -> Pockets3 + Pockets4 (jacket)
  - Back -> Backpack
- Container grants are data-driven (ObjectDefinition), not GAS-driven.
- EquipAbilitySetPath is optional; items can equip to grant containers only.
- Pocket size is driven by ContainerGrants. Defaults are small (2x2).
  - Currently enforced: GridSize (cell fit), weight limits.
  - Future (data may exist, not enforced yet): MaxVolume, AllowedTags.
- Unequip overflow policy: block unequip if any granted container is not empty.

Why this system (item-driven grants + slot fallback)
- Items define their own container expansion, so different pants/backpacks can grant different sizes and rules.
- This keeps inventory logic SOLID: inventory owns rules, items own capacity data, UI only consumes views.
- Slot fallback is a temporary safety net for legacy content during migration.
- Hands-only default keeps naked characters limited to what they can carry.

Key Components
- UProjectInventoryComponent
  - Stores inventory list (FastArray), exposes server RPCs, enforces capacity.
  - Implements IInventoryReadOnly for UI consumers.
- FInventoryEntry / FInventoryList
  - Replicated container entries with InstanceId.
- FInventoryInteractionHandler
  - Subscribes to IInteractionService, routes pickup and loot interactions.

Pickup and Loot Flow
- Player presses E -> ProjectInteraction server re-trace -> OnInteraction(Target, Instigator)
- InventoryInteractionHandler:
  - If IPickupSource: TryAddItem(ObjectId, Quantity) then Consume.
  - If ILootSource: CanFitItems -> AddItemsBatch -> ConsumeLootSource.

Drop Flow
- Server_DropItem spawns a world actor via IObjectSpawnService and then removes items.
- Spawn service is resolved through ProjectCore service locator (no hard dependency on ProjectObject).

Persistence (ProjectSave)
- Inventory data serializes to FInventorySaveData.
- Use SaveToSaveSubsystem / LoadFromSaveSubsystem to store inventory into ProjectSaveSubsystem plugin data.
- Default save key is "ProjectInventory.Inventory" when SaveKey is None.
- Standalone flow: inventory auto-loads AutoSave on BeginPlay and auto-saves on EndPlay.
- Only player-controlled pawn inventories participate in local save.

Action Receiver (IProjectActionReceiver)
- `inventory.consume:<ObjectId>` -- removes exact ObjectId.
- `inventory.consume:<ObjectId>*` -- removes first matching ObjectId prefix family (for variants like `WaterBottleBig` when action uses `WaterBottle*`).
- Routes through server RPC for proper authority.
- Used by dialogue system to consume items as part of scripted sequences.

Non-responsibilities
- UI presentation (implemented by ProjectInventoryUI; UI is a consumer only).
- ASC creation or ownership (owned by ProjectCharacter).
- Vitals tick or state tags (owned by ProjectVitals).

Dependencies
- ProjectCore (interfaces, service locator, tags)
- ProjectFeature (feature registration)
- ProjectGAS (ApplyMagnitudes, AbilitySet)

Related Docs
- Design vision: docs/design_vision.md (game design intent)
- Agent implementation tracker: ../../../todo/current/implement_inventory_vision.md
- Technical architecture: docs/architecture.md
- ProjectCore interfaces: Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Interfaces/
- ProjectInteraction: Plugins/Gameplay/ProjectInteraction/README.md
- ProjectObject data source: Plugins/Resources/ProjectObject/README.md

Strict path for agents
- For inventory behavior decisions, `docs/design_vision.md` is the source of truth.
- If implementation diverges from vision, align code to vision or document an explicit exception.
- UI contract must stay descriptor-driven:
  - expose generic container descriptors (id, label, size, group, order)
  - avoid per-container UI branching for pants/jacket/backpack logic
