# ProjectInventory

Purpose
- Server-authoritative inventory feature for ALIS.
- Owns player-inventory runtime state, authority checks, equip/use/drop,
  pickup, and world-container transfer orchestration.
- UI is a consumer only.

Behavior SOT
- Inventory behavior and UI layout rules live only in `docs/design_vision.md`.
- Track code-vs-vision gaps in `../../../todo/current/implement_inventory_vision.md`.
- Do not duplicate inventory behavior rules in other docs.

Activation
- OnDemand feature registered via FFeatureRegistry.
- GameMode (ProjectSinglePlay / ProjectOnlinePlay) initializes the feature and attaches `UProjectInventoryComponent` to the Pawn.

Runtime scope
- Reads item and container grant data from `ObjectDefinition.Item` via `IItemDataProvider`.
- Uses `FPrimaryAssetId` for lightweight replication and lookups.
- Uses FastArray replication for inventory entries.
- Resolves pickup through `IPickupSource` and world-container transfer through
  `IWorldContainerSessionSource`.
- Persists player-controlled pawn inventories through ProjectSave.
- Applies use magnitudes and equip ability grants through ProjectGAS.

Current implementation notes
- Depth stacking and drag-time rotation from the behavior SOT are implemented;
  use the tracker above only as a completion router.
- World storage uses canonical `sections.storage` authoring and
  `IWorldContainerSessionSource` runtime flow.
- Nearby world containers now open inside the existing inventory screen with
  a distinct right-side panel, bidirectional move, and `Take All`.
- Search timing belongs to the world interaction capability, not inventory UI.
- Empty persistent world containers stay usable for store-back flows; they do
  not auto-collapse or auto-destroy on empty.

Current integration routes
- pickup:
  `IPickupSource` -> `TryAddItem` -> `Consume`
- world-container transfer:
  `IWorldContainerSessionSource` -> session open -> take/store/`Take All` ->
  inventory validation and move helpers -> world consume/store
- drop:
  `Server_DropItem` -> `IObjectSpawnService`

Key components
- `UProjectInventoryComponent`
- `FInventoryEntry` / `FInventoryList`
- `FInventoryInteractionHandler`
- `FInventoryGridPlacement`
- `FInventoryLootHelper`

Dependencies
- ProjectCore
- ProjectFeature
- ProjectGAS

Related docs
- Behavior SOT: `docs/design_vision.md`
- Gap tracker: `../../../todo/current/implement_inventory_vision.md`
- World-storage delivery note: `../../../todo/done/improve_loot_places.md`
- Implementation architecture: `docs/architecture.md`
- UI consumer: `Plugins/UI/ProjectInventoryUI/README.md`
- ProjectInteraction: `Plugins/Gameplay/ProjectInteraction/README.md`
- ProjectObject: `Plugins/Resources/ProjectObject/README.md`
