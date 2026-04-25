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
- definition resolution and residency:
  `UProjectObjectDefinitionCacheSubsystem` -> `UObjectDefinitionCache`
  -> explicit Loaded/Loading/Missing state
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
- ProjectInventory owns item-definition resolution, asset residency handles,
  action capability defaults, and move/drop reject reasons.
- `UProjectObjectDefinitionCacheSubsystem` is the single runtime owner of the
  object-definition cache for one game instance. Inventory components bind to
  it and must not create per-component caches.
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
- `../Source/ProjectInventory/Public/Subsystems/ProjectObjectDefinitionCacheSubsystem.h`
- `../Source/ProjectInventory/Public/Services/ObjectDefinitionCache.h`
- `../Source/ProjectInventory/Public/Types/InventoryContainerConfig.h`
- `../Source/ProjectInventory/Public/Inventory/InventoryTypes.h`
- `../Source/ProjectInventory/Private/Interaction/InventoryInteractionHandler.cpp`
- `../Source/ProjectInventory/Private/Helpers/InventoryGridPlacement.cpp`
- `../Source/ProjectInventory/Private/Helpers/InventoryLootHelper.cpp`
- `../Source/ProjectInventory/Public/Subsystems/ProjectContainerSessionSubsystem.h`
- `../../../Gameplay/ProjectObjectCapabilities/Source/ProjectObjectCapabilities/Public/LootContainer/LootContainerCapabilityComponent.h`

Runtime data resolution pattern
- Inventory runtime code must not reach through directly to `UAssetManager`
  for gameplay item data. Resolve object definitions through
  `UProjectObjectDefinitionCacheSubsystem::GetCache()`.
- The subsystem owns the cache. The cache owns resolved definition pointers,
  resident load handles, in-flight pending loads, and definition diagnostics.
- `UProjectInventoryComponent` may bind to the subsystem in lifecycle hooks, but
  it must not create cache objects. If no game instance subsystem is available,
  it reports Missing/Loading through the explicit resolver state and fails soft.
- Warmup/load orchestration belongs at feature/session/bootstrap boundaries.
  Gameplay getters are read-oriented and must not start async loads on first
  touch.
- Callers must handle explicit resolver states instead of treating null data as
  ambiguous. Use Loaded for usable data, Loading for an in-flight request, and
  Missing for a deterministic miss.
- If item data is unavailable, inventory entry views still emit deterministic
  safe defaults and mark action capabilities as populated but unavailable.
- Move/drop/use paths should log one explicit reject or resolve reason. Avoid
  repeated generic warnings that hide the first failure.
- Split or drag operations that target the same occupied source footprint are
  expected local cancels/no-ops. The server still rejects real invalid moves
  such as out-of-bounds targets, true overlap, or invalid containers.

Diagnostics contract
- Tests and debug dumps should capture both gameplay and presentation inputs:
  item id, instance id, container id, grid position, quantity, resolved display
  payload, resolver state, and definition-cache diagnostics.
- Widget dumps alone are not enough for inventory regressions. A useful failure
  dump must show whether the problem is definition residency, entry projection,
  visual-state construction, or widget rendering.
