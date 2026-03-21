# ProjectInventory Architecture

Behavior SOT
- Inventory behavior and world-storage rules live only in `design_vision.md`.
- Track inventory code-vs-SOT gaps in
  `../../../../todo/current/implement_inventory_vision.md`.
- World-storage delivery note:
  `../../../../todo/done/improve_loot_places.md`.
- Do not restate inventory behavior here.

Purpose
- This doc is an implementation router only.
- Use it to find ownership boundaries, current integration seams, and key code
  files.

Current implementation surfaces
- static item data:
  `ObjectDefinition.Item` -> `IItemDataProvider` -> `FItemDataView`
- replicated runtime:
  `FInventoryEntry` and `FInventoryList`
- container config and validation:
  `FInventoryContainerConfig` plus ProjectInventory helper stack
- pickup integration:
  `IPickupSource`
- world-container session path:
  `IWorldContainerSessionSource`
- persistence:
  player-controlled pawn inventories only
- UI consumption:
  ProjectInventoryUI through read-only views and command interfaces

Cross-plugin boundaries
- ProjectInventory owns inventory runtime rules and authority checks.
- ProjectCore owns interfaces, tags, and cross-plugin contracts.
- ProjectInventoryUI owns presentation only.
- ProjectObject and ProjectObjectCapabilities own authored data and world
  capability providers.
- ProjectInteraction owns interaction tracing and dispatch.
- World storage keeps `ProjectObjectCapabilities` thin and avoids a direct
  dependency on ProjectInventory internals. See the SOT and the delivered
  world-storage note.

Key files
- `../Source/ProjectInventory/Public/Components/ProjectInventoryComponent.h`
- `../Source/ProjectInventory/Public/Types/InventoryContainerConfig.h`
- `../Source/ProjectInventory/Public/Inventory/InventoryTypes.h`
- `../Source/ProjectInventory/Private/Interaction/InventoryInteractionHandler.cpp`
- `../Source/ProjectInventory/Private/Helpers/InventoryGridPlacement.cpp`
- `../Source/ProjectInventory/Private/Helpers/InventoryLootHelper.cpp`
- `../Source/ProjectInventory/Public/Subsystems/ProjectContainerSessionSubsystem.h`
- `../../../Gameplay/ProjectObjectCapabilities/Source/ProjectObjectCapabilities/Public/LootContainer/LootContainerCapabilityComponent.h`
