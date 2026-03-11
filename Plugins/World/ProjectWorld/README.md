# ProjectWorld

Shared world utilities for any map or tile. Provides the common tools that world plugins (City17, future worlds) use.

This README describes the reusable world layer. World-specific maps, authored content, and large asset payloads live in world plugins and are not fully represented in the public mirror.

**Key principle:** ProjectWorld owns the *tools*, world plugins own the *data*.


## 1. Purpose

ProjectWorld is a thin helper plugin that provides:

- **World IDs and coordinates** - Common types for tile IDs, world positions, bounds
- **World data loader** - Reads world data files into memory structs
- **Runtime spatial queries** - Find POIs, buildings without loading actors
- **Event/Data Layer mapping** - Toggle UE Data Layers by event name
- **Validation and incremental build hooks** - Shared logic for world builders

ProjectWorld contains **no world-specific content**. It does not know about City17 streets or buildings - it just provides the machinery that any world can use.

**World-tier plugin:** Lives under `Plugins/World/ProjectWorld/`; used by world plugins (City17, future worlds).


## 2. Architecture Overview

### 3. Key Concepts
 
 - **World Partition**: We use grid-based streaming. [See docs/world_partition.md](docs/world_partition.md) for data layer rules and Grid naming.
 - **Hierarchical LOD (HLOD)**: Pre-baked visuals for distant tiles.

### Three Layers (Don't Fight WP)

You don't "derive from" World Partition. You let UE's WP handle streaming of **empty cells**, and layer your own **tile + world-data logic on top**.

| Layer | Owns | Does |
|-------|------|------|
| **Engine (UE5 WP)** | Grid, cells, streaming | Loads/unloads cells around player automatically |
| **ProjectWorld** | Tiles, world-data | Decides WHAT goes into cells (from data or generators) |
| **City17 (world plugin)** | Authored content | Provides REAL data for part of the grid |

### Engine Layer - Stock World Partition

You do NOT write your own partition system. Just configure once:

- World size (grid extent)
- Cell size (compatible with tile size or clean divisor)
- Streaming Source actor (follows player)

WP auto-handles: load cells around source, unload far cells, keep aligned.

### ProjectWorld Layer - Tile + World-Data Logic

On top of WP, ProjectWorld adds:

1. **Tile grid math** - position -> (TileX, TileY), tile -> which WP cells
2. **World-data loader** - tile in range? Load authored data OR generate procedurally
3. **WP streaming hooks** - "tile entered range" -> apply data; "tile left" -> cleanup

WP only knows "cells". ProjectWorld knows "tiles and their data".

### World Plugin Layer - Concrete Content

City17 (or any world) is just one user:

- Provides: heightmap, authored world-data for core tiles
- Uses: ProjectWorld to turn data into actors/PCG inside WP map
- Outside core: ProjectWorld switches to procedural generation (same schema, no files)

```
+------------------+     +------------------+     +------------------+
|     City17       |     |    FutureWorld   |     |   AnotherWorld   |
|  (world data)    |     |   (world data)   |     |   (world data)   |
+--------+---------+     +--------+---------+     +--------+---------+
         |                        |                        |
         v                        v                        v
+------------------------------------------------------------------------+
|                           ProjectWorld                                  |
|  - Tile grid math                                                      |
|  - World data loader (authored OR procedural)                          |
|  - WP streaming hooks                                                  |
|  - Spatial query service                                               |
+------------------------------------------------------------------------+
         |
         v
+------------------------------------------------------------------------+
|                    Unreal Engine (stock, unmodified)                    |
|  - World Partition (grid, cells, streaming sources)                    |
|  - Data Layers                                                         |
|  - Nanite (auto-LOD for meshes)                                        |
+------------------------------------------------------------------------+
```


## 3. Data Philosophy: Patterns, Not Props

World data describes **patterns and constraints**. Tools (PCG, generators, humans) turn that into actual meshes.

### What World Data SHOULD Describe

High-level structure:

- Where buildings are (footprints, height, type, preset)
- Where roads are (polylines, width, material)
- Where zones/biomes are (residential, industrial, park)
- Where important POIs/rooms/entrances are
- Which preset/template to use for a building or interior
- Density and variation parameters

### What World Data Should NOT Be

- A giant list of every chair, every lamp, every bush
- Individual prop placements
- Raw mesh transforms

If we try to describe every prop in JSON, productivity dies and nobody (human or agent) will be happy.

### Two-Layer Pattern

**Layer 1 - World Data says:**
```json
{
  "id": "residential_block_01",
  "footprint": [[0,0], [20,0], [20,30], [0,30]],
  "floors": 5,
  "interior_preset": "residential_block_a",
  "damage_level": 2,
  "entrance_side": "north"
}
```

**Layer 2 - Generator does:**
- Look at `interior_preset`
- Use template (PCG graph, C++ generator) that knows:
  - Room sizes, corridors, stairs, doors
  - Rules for props (kitchen, bathroom, bedroom)
  - Damage/decay application

Agents edit: building type, preset name, floors, densities, damage level.
Agents do NOT: micromanage every table and chair.

### Landscape and Foliage

Same pattern - world data drives WHERE and HOW, not each instance:

```json
{
  "zone": "park_central",
  "tree_density": 0.4,
  "species_mix": ["birch", "oak", "pine"],
  "grass_coverage": 0.8,
  "slope_limit": 30
}
```

PCG reads zone + density, uses noise and rules to place trees, grass, rocks.
You adjust a small set of numbers, NOT thousands of foliage instances.

### Hero Areas (Manual Work)

Some places will always be handcrafted:

- Main plaza, key story interiors
- Unique props, story set pieces

Pattern:
```json
{
  "id": "metro_station_entrance",
  "hero_area": true,
  "pcg_override": false
}
```

- Builder leaves this area alone (no auto PCG overwrite)
- Level artist hand-builds the hero interior in editor

Data-driven does MOST of the city; humans touch the 10% most important places.

### Agent-Friendly Vocabulary

Keep fields simple and consistent:

| Field | Meaning |
|-------|---------|
| `building_type` | Residential, commercial, industrial |
| `interior_preset` | Template name for interior generation |
| `zone_type` | Biome/district classification |
| `density` | How full (0.0 - 1.0) |
| `prop_theme` | Style palette (soviet, modern, ruined) |
| `damage_level` | Decay state (0 = pristine, 3 = destroyed) |
| `random_seed` | Override for deterministic variation |

Agents can easily handle:
- "Reduce tree density for industrial zone"
- "Mark all ground-floor shops near metro to use `shop_ruined` preset"
- "Change all residential blocks within 200m of square to `damage_level = 2`"

Agents are good at editing structured text with clear fields. They are BAD at thinking about raw meshes and hand-placing actors.

### The Rule of Thumb

| Layer | Responsibility |
|-------|----------------|
| **World data (JSON)** | WHERE, WHAT TYPE, HOW DENSE, WHICH PRESET |
| **Generators/PCG** | HOW EXACTLY to lay out meshes and details |
| **Editor/Human** | SPECIAL PLACES that need emotional/story design |


## 4. One World Data File Per Tile

Each tile has **one data source** that describes everything:

- Geometry (buildings, streets, terrain layers)
- Points of interest (POIs)
- Zones and biomes
- Event tags (Blackout, Shelling, Curfew)
- Streaming priority hints

**Example path:** `Plugins/World/City17/Config/World/tile_00_00.json`

All systems read from this single source. No scattered configs.


## 5. World Data Loader

ProjectWorld provides a loader that:

1. Given `WorldName` + `TileIndex` -> finds the data file
2. Parses it into in-memory structs (`FWorldTileData`)
3. Returns typed data for systems to consume

```cpp
// World plugin provides the path, ProjectWorld does the loading
FWorldTileData TileData;
if (UProjectWorldLoader::LoadTile(TEXT("City17"), FIntPoint(0, 0), TileData))
{
    // TileData.Buildings, TileData.POIs, TileData.Zones now populated
}
```

World plugins just say "my data path is X"; loading logic is shared.


## 6. Runtime Spatial Queries

At load time, world data is read into memory. Systems can then query without loading actors:

```cpp
// Find all POIs within 200 meters
TArray<FWorldPOI> NearbyPOIs;
WorldQueryService->QueryPOIsInRadius(PlayerLocation, 200.f, NearbyPOIs);

// Find nearest hospital
FWorldPOI NearestHospital;
WorldQueryService->FindNearestPOIOfType(PlayerLocation, TEXT("Hospital"), NearestHospital);

// Get buildings in bounds
TArray<FWorldBuilding> Buildings;
WorldQueryService->QueryBuildingsInBounds(QueryBox, Buildings);
```

This is fast (in-memory lookup, no actor spawning) and works before actors stream in.


## 7. Events and Data Layers

World data tags objects and layers with event names:

```json
{
  "buildings": [
    { "id": "power_plant", "events": ["Blackout"] }
  ],
  "data_layers": {
    "City17_Event_Blackout": { "active_during": ["Blackout"] },
    "City17_Event_Shelling": { "active_during": ["Shelling"] }
  }
}
```

When game state changes, ProjectWorld toggles Data Layers:

```cpp
// GameMode or feature decides the event
WorldEventService->SetActiveEvent(TEXT("Blackout"));

// ProjectWorld finds layers tagged "Blackout" and activates them
// Layers NOT tagged "Blackout" are deactivated
```

**World says WHAT can change, GameMode decides WHICH event is active.**


## 8. POIs and Encounters

World data marks **places**, not **spawns**:

```json
{
  "pois": [
    {
      "id": "alley_industrial_01",
      "type": "Alley",
      "location": [1234.5, 5678.9],
      "encounter_slots": ["Ambush", "Patrol"]
    },
    {
      "id": "shop_trader_01",
      "type": "Shop",
      "location": [2345.6, 6789.0],
      "encounter_slots": ["Trader", "Quest"]
    }
  ]
}
```

- World defines **where** something CAN happen
- SinglePlay/OnlinePlay/features decide **what** actually spawns

This keeps world data gameplay-agnostic.


## 9. Terrain and Landscape

### One Big Heightmap, Many Streamed Pieces

Think of the ground as **one giant image of heights** (heightmap).

- **Global shape** = one continuous surface
- **Streaming** = engine loads/unloads pieces, but they still fit perfectly

There is no "joining" later if you start from one continuous heightmap.

### Where the Heightmap Comes From

Two main options:

1. **Real data** - DEM / satellite height data for a city/region, scaled to game world
2. **Generated** - Terrain tool (World Machine, Gaea, Houdini) exports one big heightmap

In both cases: **one continuous file** is your base truth.

### Slicing Into Tiles

Outside or inside UE, slice the big heightmap along your world grid:

- Tile 00_00, 00_01, 01_00, etc.
- All slices share same resolution, align at borders
- No gaps, no overlaps
- World Partition streams those pieces

You do NOT sculpt tile A and tile B independently. Always come from the same "mother" heightmap.

### Editing Without Breaking Seams

**Big changes:**
- Go back to external terrain tool
- Edit the big heightmap
- Re-slice and reimport

**Small details inside UE:**
- Use landscape layers (extra noise, craters, trenches) on top
- Avoid aggressive sculpting exactly on tile borders
- Edit across multiple components at once

Seams stay clean because edges come from the same original data.

### Landscape + World Data + PCG Together

| Source | Provides |
|--------|----------|
| **Heightmap** | Raw shape: hills, valleys, flat areas |
| **World data** | Zone types: residential, park, industrial |
| **PCG** | Combines both to decide placement |

PCG checks:
- Slope from heightmap
- Zone/type from world data

Then decides:
- Where trees can grow
- Where buildings can stand
- Where to carve roads

Agents change **types and parameters** in world data, not raw height values.

### Trees, Grass, Rocks

You do NOT hand-paint every tree forever.

Instead:
```json
{
  "zone": "forest_edge",
  "tree_density": 0.6,
  "species_mix": ["birch", "pine"],
  "noise_scale": 50,
  "slope_max": 35
}
```

PCG/foliage tools place instances based on:
- Heightmap (slope)
- Zone type from world data

Manually tweak only special cases (hero locations).

### What Agents Can Edit

Agents understand and safely edit:

| Task | How |
|------|-----|
| Change zone type | "This tile is more industrial, reduce trees" |
| Adjust parameters | "Raise hills slightly here", "flatten strip for road" |
| Apply presets | "Use `river_bank` erosion preset for this segment" |

Agents do NOT reason about raw floating-point heights for every vertex.
They operate on **rules and areas**, pipeline converts that into modified heightmaps and PCG.


## 10. Zones and Biomes for PCG

World data describes zones with PCG parameters:

```json
{
  "zones": [
    {
      "id": "residential_north",
      "polygon": [[0,0], [1000,0], [1000,1000], [0,1000]],
      "pcg_params": {
        "vegetation_density": 0.3,
        "prop_palette": "residential_props",
        "exclusion_mask": "roads"
      }
    },
    {
      "id": "industrial_east",
      "polygon": [[1000,0], [2000,0], [2000,1000], [1000,1000]],
      "pcg_params": {
        "vegetation_density": 0.05,
        "prop_palette": "industrial_props"
      }
    }
  ]
}
```

ProjectPCG reads these parameters via ProjectWorld's zone query:

```cpp
FWorldZone Zone;
if (WorldQueryService->GetZoneAtLocation(Location, Zone))
{
    float Density = Zone.PCGParams.VegetationDensity;
    // Apply to PCG graph
}
```


## 11. Deterministic Seeds

Every important thing has a stable ID in world data:

- Tile: `City17_00_00`
- Zone: `residential_north`
- POI: `alley_industrial_01`

Seeds are derived from these IDs:

```cpp
uint32 TileSeed = GetTypeHash(TileId) ^ GlobalSeed;
uint32 ZoneSeed = GetTypeHash(ZoneId) ^ TileSeed;
uint32 POISeed = GetTypeHash(POIId) ^ TileSeed;
```

**Same IDs -> same seeds -> same generated layout every time.**


## 12. Hot Reload (Editor)

When world data file changes:

1. File watcher detects change
2. ProjectWorld reloads affected tile data
3. Builder re-applies only changed objects
4. Editor updates without restart

```cpp
// In editor module
FDelegateHandle WatchHandle = FFileWatcher::Watch(
    WorldDataPath,
    FFileWatcher::FOnFileChanged::CreateLambda([](const FString& Path)
    {
        UProjectWorldLoader::ReloadTile(WorldName, TileIndex);
        UWorldBuilder::IncrementalRebuild(WorldName, TileIndex);
    })
);
```

Edit JSON -> save -> see changes in viewport.


## 13. Streaming Hints (No HLOD)

### Why We Skip HLOD

HLOD = pre-baked proxy meshes for big chunks. For ALIS (large / potentially endless), it's a trap:

- Every HLOD cluster = **extra mesh asset** on disk
- Thousands of clusters = tens or hundreds of GB of proxies
- Every rebuild = **long bake times** and huge I/O
- Layout changes = must rebuild HLOD again

**Policy: No HLOD builds for City17 or any "endless" part of the world.**

### What We Rely On Instead

| Technique | How It Helps |
|-----------|--------------|
| **Nanite** | All static buildings/props use Nanite; auto-reduces triangles over distance |
| **World Partition** | Streams cells in/out; far tiles simply not loaded (no proxies needed) |
| **Instancing** | Trees, props, facade pieces use HISM/ISM + Nanite; low draw calls |
| **Design-level LOD** | Outer/procedural areas: fewer buildings, simpler shapes, less clutter |

### Streaming Priority Hints

World data still marks objects with priority for **streaming distance**, not HLOD:

```json
{
  "buildings": [
    { "id": "cathedral", "streaming_priority": "critical" },
    { "id": "residential_block_a", "streaming_priority": "normal" },
    { "id": "shed_01", "streaming_priority": "low" }
  ]
}
```

Meaning:
- `critical` - Stay loaded at longer distances (important landmark)
- `normal` - Standard World Partition streaming behavior
- `low` - Unload earlier, lower priority

```cpp
if (Building.StreamingPriority == EStreamingPriority::Critical)
{
    // Override streaming distance to keep loaded farther
}
```

### If HLOD Is Ever Reconsidered

Only for a tiny, fixed **hero zone** where profiling proves it's needed. Default policy: **no HLOD in ALIS.**


## 14. Validation

Headless validation runs over all world data files:

```cpp
// Check for common issues
- Unique IDs (no duplicates)
- Valid asset references (meshes, materials exist)
- No overlapping buildings
- Valid polygon winding for zones
- Required fields present
```

Run before commits or in CI:

```bash
UE-Cmd -run=ValidateWorldData -World=City17
```

Fails fast with clear errors if something is broken.


## 15. Incremental Build

Builder remembers last applied world data. On change:

1. Compute diff: new, changed, removed entries
2. New entries -> spawn actors
3. Changed entries -> move/update actors
4. Removed entries -> delete actors

Only touched parts of the map update, so builds stay fast.

```cpp
FWorldDataDiff Diff = UProjectWorldDiff::ComputeDiff(OldData, NewData);
// Diff.Added, Diff.Modified, Diff.Removed
```


## 16. What Stays in World Plugins

World plugins (City17, etc.) own:

- **Actual world data files** (JSON/DSL for their tiles)
- **World-specific rules** (which data layers exist, which events are valid)
- **Builder/commandlet** that calls ProjectWorld helpers
- **Maps** (World Partition .umap files)

ProjectWorld provides tools; world plugins provide content.


## 17. Public API (Planned)

### Types

```cpp
// Tile coordinates
struct FWorldTileId { FName WorldName; FIntPoint TileIndex; };

// World position (tile-relative, meters)
struct FWorldPosition { FWorldTileId Tile; FVector2D LocalPos; };

// POI data
struct FWorldPOI { FName Id; FName Type; FVector Location; TArray<FName> EncounterSlots; };

// Building data
struct FWorldBuilding { FName Id; FName PresetId; FTransform Transform; EStreamingPriority Priority; };

// Zone data
struct FWorldZone { FName Id; TArray<FVector2D> Polygon; FWorldPCGParams PCGParams; };
```

### Services

```cpp
// Load world data
UProjectWorldLoader::LoadTile(WorldName, TileIndex, OutData);

// Spatial queries
IWorldSpatialQuery::QueryPOIsInRadius(Location, Radius, OutPOIs);
IWorldSpatialQuery::QueryBuildingsInBounds(Bounds, OutBuildings);
IWorldSpatialQuery::GetZoneAtLocation(Location, OutZone);

// Events
IWorldEventService::SetActiveEvent(EventName);
IWorldEventService::GetActiveEvent() -> FName;

// Diff and validation
UProjectWorldDiff::ComputeDiff(OldData, NewData) -> FWorldDataDiff;
UProjectWorldValidator::Validate(TileData, OutErrors) -> bool;
```


## 18. Non-Responsibilities

ProjectWorld does NOT:

- Own any specific world content (City17 streets, buildings)
- Know about gameplay features (combat, dialogue, inventory)
- Implement actual PCG graphs (that's ProjectPCG)
- Manage game state or sessions (that's GameMode)

It's a **utility layer**, not a game system.


## 19. Dependencies

- `ProjectCore` - Base types, service locator
- Engine modules: `Core`, `CoreUObject`, `Engine`, `WorldPartition`

No dependencies on:
- World plugins (City17, etc.)
- Gameplay plugins
- UI plugins


## Legacy Paths

Code marker format:
- `// LEGACY_OBJECT_PARENT_GENERALIZATION(L###): <reason>. Remove when <condition>.`

| Legacy ID | Location | Why It Exists | Remove Trigger |
|-----------|----------|---------------|----------------|
| `L004` | `Source/ProjectWorld/Public/ProjectWorldActor.h` (definition metadata block on `AProjectWorldActor`) | Definition host data is still inheritance-based in the base actor | Remove when host ownership moves to `IObjectDefinitionHostInterface`/`UObjectDefinitionHostComponent` and legacy bridge paths are deleted |

## Definition Host Metadata Policy

- `UObjectDefinitionHostComponent` is the canonical host for generalized actors.
- Host metadata (`ObjectDefinitionId`, `AppliedStructureHash`, `AppliedContentHash`) is replicated.
- Injection path (`ProjectWorldDefinitionHost::EnsureHostObject`) is allowed for spawned actors that do not provide host data natively.
- LEGACY_OBJECT_PARENT_GENERALIZATION(L004): `AProjectWorldActor` metadata block remains until inheritance-host cleanup is complete.
- While `L004` is active, host helper read/write order is intentional:
  - `AProjectWorldActor` direct fields are authoritative first.
  - interface/component host path is used for non-`AProjectWorldActor` actors.
- This ordering prevents spawn-time host write/read divergence during editor placement.


## 21. Related Docs

- [City17 README](../../World/City17/README.md) - Reference world implementation
- [ProjectPCG README](../PCG/ProjectPCG/README.md) - PCG integration
- [World Partition docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-in-unreal-engine)
