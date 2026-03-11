# City17 TODO

## Immediate

- [x] Add plugin-local `Config/DefaultGame.ini` entries for `PrimaryAssetTypesToScan` pointing to `/City17/...` directories. (2025-12-10)
- [ ] Create initial tile world data `Config/World/tile_00_00.json`.
- [ ] Implement `UCity17TileMetadata` data asset for tile bounds and grid info (agent-editable C++).

## World Data System

- [ ] Define tile world data structure (`docs/schemas/tile_worlddata.schema.json`):
  - `tile`: id, TX, TY, real-world bounds (optional)
  - `terrain_layers[]`: height/material references
  - `roads[]`: polylines in tile coordinates
  - `lots[]`: building footprints, zoning type
  - `buildings[]`: modular preset references + transform
  - `props[]`: street furniture, lights, signage
  - `pois[]`: POI markers with type, radius, associated encounters
  - `data_layer` per object: which UE Data Layer owns the actor
- [ ] Build content dependency graph from world data references:
  - Extract mesh/material soft refs from buildings[], props[]
  - Prefetch assets before tile streams in (reduce pop-in)
- [ ] Add validation commandlet: `UE-Cmd -run=ValidateCity17WorldData`

## Zones & Biomes

- [ ] Create zone world data `Config/World/zones.json`:
  - Define zones by bounding polygons or grid regions
  - Per-zone PCG parameters: vegetation density, prop palette, exclusion masks
  - Per-zone audio/ambient settings (optional)
- [ ] Implement `FCity17ZoneData` loader to provide zone params to shared PCG subsystem

## Builder Workflow

- [ ] Implement `UCity17TileBuilder` commandlet:
  - Read world data -> spawn/update actors -> assign Data Layers -> save map
  - Support incremental rebuild (diff world data vs existing actors)
- [ ] Add editor tool: `Tools -> ALIS -> City17 -> Rebuild Tile From World Data`
- [ ] Implement world data hot-reload (file watcher in editor, auto-rebuild on save)

## PCG Integration

- [ ] Define PCG seed strategy: `tile_seed = hash(tile_id + global_seed)`
- [ ] Add City17-specific PCG parameter sections to zone world data (density, variation, exclusion)
- [ ] Expose `FCity17ZoneData` to shared `UProjectPCGSubsystem` (lives in ProjectPCG plugin)
  - City17 provides data, ProjectPCG owns the subsystem that feeds PCG graphs

## Runtime Queries

- [ ] Implement `UCity17WorldDataProvider` to feed City17 tile data to ProjectWorld loader
- [ ] Register City17 data path with `UProjectWorldLoader` (see ProjectWorld README section 4)
- [ ] Use `IWorldSpatialQuery` from ProjectWorld for POI/building lookups (no City17-specific interface needed)

## Data Layer Activation

- [ ] Define City17 events in tile data (Blackout, Shelling, Curfew, etc.)
- [ ] Tag Data Layers with event names in tile data (see ProjectWorld README section 6)
- [ ] Use `IWorldEventService` from ProjectWorld to toggle layers (no City17-specific subsystem needed)

## Streaming (No HLOD)

**Policy:** No HLOD in ALIS. We rely on Nanite + WP streaming + instancing instead.

- [ ] Define streaming priority hints in world data (`priority: critical/normal/low`)
- [ ] Implement `GetStreamingPriority()` override based on world data
- [ ] Ensure all static meshes are Nanite-enabled
- [ ] Use HISM/ISM for repeated props (trees, street furniture, facade pieces)

## Validation & Testing

- [ ] Add smoke test: `scripts/ue/test/smoke/city17_worlddata_load.bat`
- [ ] Add integration test: world data -> builder -> verify actors match
- [ ] Add schema validation test: reject invalid world data with clear errors

## Documentation

- [ ] Add world data editing guide for agents in README section 9
- [ ] Document builder workflow with examples
- [ ] Reference schema location: `docs/schemas/tile_worlddata.schema.json`
