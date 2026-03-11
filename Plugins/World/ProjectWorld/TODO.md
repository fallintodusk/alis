# ProjectWorld TODO

## Immediate (Minimal First Step)

- [ ] Implement `UProjectWorldLoader`:
  - `LoadTile(WorldName, TileIndex, OutData)` -> parses JSON into `FWorldTileData`
  - World plugins register their data paths
- [ ] Implement `IWorldSpatialQuery` interface:
  - `QueryPOIsInRadius(Location, Radius, OutPOIs)`
  - `QueryBuildingsInBounds(Bounds, OutBuildings)`
  - `GetZoneAtLocation(Location, OutZone)`

## Types

- [ ] Define `FWorldTileId` struct (WorldName + TileIndex)
- [ ] Define `FWorldPosition` struct (tile-relative position in meters)
- [ ] Define `FWorldPOI` struct (id, type, location, encounter slots)
- [ ] Define `FWorldBuilding` struct (id, preset, transform, streaming priority)
- [ ] Define `FWorldZone` struct (id, polygon, PCG params)
- [ ] Define `FWorldTileData` container (buildings, POIs, zones, events, streaming hints)
- [ ] Define `EStreamingPriority` enum (Critical, Normal, Low)

## World Data Loader

- [ ] Implement JSON parser for tile data format
- [ ] World plugin registration API: `RegisterWorldDataPath(WorldName, BasePath)`
- [ ] Path resolution: `WorldName + TileIndex -> file path`
- [ ] Error handling: clear messages for missing/invalid files

## Spatial Query Service

- [ ] Implement `UProjectWorldQuerySubsystem` (GameInstance subsystem)
- [ ] Build spatial index on tile load (grid hash or R-tree)
- [ ] Register with `FProjectServiceLocator::Register<IWorldSpatialQuery>()`
- [ ] Cache tile data in memory for fast lookups

## Event Service

- [ ] Implement `IWorldEventService` interface:
  - `SetActiveEvent(EventName)`
  - `GetActiveEvent() -> FName`
  - `GetLayersForEvent(EventName) -> TArray<FName>`
- [ ] Implement `UProjectWorldEventSubsystem`
- [ ] Data Layer toggle logic via UE's Data Layer system
- [ ] Broadcast delegate when event changes

## Validation

- [ ] Implement `UProjectWorldValidator`:
  - Check unique IDs
  - Check valid asset references
  - Check polygon winding
  - Check required fields
- [ ] Return detailed error messages
- [ ] Commandlet wrapper: `UE-Cmd -run=ValidateWorldData -World=<name>`

## Incremental Build Helpers

- [ ] Implement `UProjectWorldDiff`:
  - `ComputeDiff(OldData, NewData) -> FWorldDataDiff`
  - `FWorldDataDiff` contains Added, Modified, Removed entries
- [ ] Diff by ID: same ID = compare fields, missing ID = removed, new ID = added

## Hot Reload (Editor Only)

- [ ] Implement file watcher for world data files
- [ ] On change: reload tile data, notify listeners
- [ ] Broadcast delegate: `OnTileDataReloaded(WorldName, TileIndex)`
- [ ] Editor module only (not in runtime builds)

## Shared Resources Loading

- [ ] Load shared resource plugins (ProjectMaterial, etc.) when world loads
  - Options to consider:
    - Plugin dependencies in world plugin .uplugin
    - Manifest-based: world declares required resource plugins
    - Orchestrator handles resource plugin loading before world travel
  - Need to ensure materials/assets available before world streams in

## Later (Add When Needed)

- [ ] Multiple active events (if needed)
- [ ] Zone overlap handling (point-in-polygon for multiple zones)
- [ ] Async tile loading (if blocking becomes a problem)
- [ ] Binary format option (if JSON parsing is slow)

## Testing

- [ ] Unit test: tile data parsing
- [ ] Unit test: spatial query correctness
- [ ] Unit test: diff computation
- [ ] Integration test: load City17 tile, query POIs
- [ ] Smoke test: validation commandlet on test data

## Documentation

- [ ] Keep README.md updated with API changes
- [ ] Add code examples in header comments
- [ ] Document JSON format in schema file
