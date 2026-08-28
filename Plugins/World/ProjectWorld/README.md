# ProjectWorld

Partial shared world foundation for maps and world plugins such as City17.
This document also records the target design for capabilities that have not
been implemented yet.

This README describes the reusable world layer. World-specific maps, authored content, and large asset payloads live in world plugins and are not fully represented in the public mirror.

**Key principle:** ProjectWorld owns the *tools*, world plugins own the *data*.

## Implementation Status

Status: **PARTIAL IMPLEMENTATION / DESIGN PROPOSAL**.

Implemented in `Source/ProjectWorld`:

- `UProjectWorldManifest` and `FProjectWorldRegionDescriptor`, including
  manifest validation and primary-asset identity.
- `AProjectWorldActor` definition application and definition identity.
- `IObjectDefinitionHostInterface`, `UObjectDefinitionHostComponent`, and
  object-definition host helpers.
- `ProjectWorldEditor` canonical receipt validation and deterministic Unreal
  realization for bounded generated maps.
- Authenticated semantic presentation binding from `terrain.default` to the accepted
  universal ProjectMaterial terrain instance, with durable assignment across every
  Landscape streaming proxy in the logical family.
- Exact procedural preview for non-Landscape fixtures and stock Landscape
  import for compatible canonical grids.

Planned, but not implemented:

- Tile coordinate and world-position types and grid math.
- `UProjectWorldLoader` and authored/procedural tile loading.
- `IWorldSpatialQuery` and its runtime subsystem.
- Event/Data Layer mapping services.
- Runtime world-data diffing and editor hot reload.
- ProjectPCG integration and concrete world-plugin manifests/data.

Unless a section explicitly names an implemented type above, API snippets and
workflows in this README are design proposals; do not use an example here as
evidence that its API exists.


## 1. Purpose

ProjectWorld is intended to become a thin helper plugin that provides:

- **World IDs and coordinates** - Common types for tile IDs, world positions, bounds
- **World data loader** - Reads world data files into memory structs
- **Runtime spatial queries** - Find POIs, buildings without loading actors
- **Event/Data Layer mapping** - Toggle UE Data Layers by event name
- **Validation and incremental build hooks** - Shared logic for world builders

ProjectWorld contains no world instance data or UE content. Synthetic fixtures
live in the editor-only `ProjectWorldTestData` plugin; Kazan production data
lives in `ProjectWorldData`.

**World-tier plugin:** Lives under `Plugins/World/ProjectWorld/`; used by world plugins (City17, future worlds).


## 2. Architecture Overview

First read: [World Reconstruction Architecture and Observability](docs/architecture_overview.md).

### 3. Key Concepts
 
 - **World Partition**: We use grid-based streaming. [See docs/world_partition.md](docs/world_partition.md) for data layer rules and Grid naming.
 - **Territory generation**: Expand generated geography before importing or
   polishing legacy map content. [See docs/territory_generation.md](docs/territory_generation.md).
 - **Geometry representation**: Production generated worlds use World
   Partition streaming, Nanite where supported, and instance ownership. HLOD
   generation is disabled; see [docs/world_partition.md](docs/world_partition.md).

### Three Layers (Don't Fight WP)

You don't "derive from" World Partition. You let UE's WP handle streaming of **empty cells**, and layer your own **tile + world-data logic on top**.

| Layer | Owns | Does |
|-------|------|------|
| **Engine (UE5 WP)** | Grid, cells, streaming | Loads/unloads cells around player automatically |
| **ProjectWorld** | Tiles, world-data | Decides WHAT goes into cells (from data or generators) |
| **City17 (world plugin)** | Authored content | Provides REAL data for part of the grid |

### World Build Boundary

The reusable build path has four distinct identities and no shared grid SOT:

```text
provider partition -> compiler cell -> generated artifact -> World Partition cell
```

- Source ingestion preserves provider IDs, coordinates, precision, and terms.
- The external World Compiler converts accepted inputs into canonical ALIS
  features and an aligned metric terrain grid.
- A narrow Unreal adapter realizes canonical cells as persistent,
  transactionally replaceable engine assets.
- ProjectWorld and stock World Partition own runtime lookup and streaming.

Compiler cells are addressed by generated `grid_id` plus integer x/y. They do
not change when a city grows, an acquisition envelope changes, or an Unreal
streaming profile is retuned. Mapping to World Partition cells is explicit.

Canonical attributes distinguish source-derived, inferred, procedural, and
authored values. Provider geometry is evidence, not a finished mesh. Roads
remain semantic graphs and splines; buildings remain footprint-driven DNA;
terrain remains a quantized aligned grid. Meshes and `.uasset` files are
rebuildable projections.

Water, land-cover, vegetation-area, and explicit foliage-point inputs enter as
provider-neutral records and join canonical cells without changing the grid.
Future forest realization uses zones, density, species rules, and deterministic
seeds rather than one permanent runtime record per tree. Hero places remain
protected authored overlays.

The command, profile, schema, and report contracts live in
[`tools/World/CanonicalCompilation/README.md`](../../../tools/World/CanonicalCompilation/README.md).

#### Unreal realization contract

`ProjectWorldEditor` accepts one exact, accepted Canonical Compilation receipt.
Before editor mutation it verifies every declared output byte size and SHA-256,
exact schema version, coverage/provenance profile identity, grid/cell identity,
terrain bounds and spacing, manifest-to-feature ownership, provenance result,
and coordinate contract. It never reads raw provider data.

Generated maps are restricted to the declared data owner:
`/ProjectWorldTestData/Generated/` for tests and
`/ProjectWorldData/Generated/` for Kazan. `/ProjectWorld/Generated/` is
invalid.
Owned actors use stable GUIDs and the `ProjectWorld.Generated.v1` tag; input,
grid, cell, terrain, and feature identities remain on actor tags. Untagged
authored actors are preserved. An Apply run records actual actor/component
semantics as a SHA-256 fingerprint and reports generated source bytes.

An Apply run updates matching generated cell and GeoReferencing actors in
place, removes only stale generated identities, and rebuilds their owned mesh
payloads. This keeps World Partition external-actor packages stable while
ensuring a same-identity algorithm or presentation change reaches the saved
map.
The D3 fingerprint includes edit-layer names and content, not engine-assigned
layer GUIDs. A receipt proves the protected authored-correction layer keeps its
GUID and bytes while an existing Landscape is regenerated. Clean reconstruction
may receive new internal layer GUIDs without changing semantic or package-path
identity.

The representative adapter imports one stock 16-bit Landscape without engine
resampling. Production topology is one logical Landscape partitioned into
streaming proxies. Its baseline is one `31 x 31`-quad component and proxy per
canonical cell; the documented World Partition performance gate may bundle
adjacent proxies without changing cell ownership. `Generated Base` is updated
only for changed, component-aligned terrain cells; `Generated Roads` is
reserved for generated road deformation; `Authored Corrections` is never
written by regeneration. Incompatible small fixtures use an exact procedural
preview and fail when Landscape is required.
Canonical terrain row zero is the northern edge and maps to the Landscape's
north-west row without another reversal. GeoReferencing proves both projected
-> Unreal -> projected error and equality with the transform used for actual
actors at cell corners, shared edges, and a feature point. The bounded road
slice selects one canonical identity with valid fragments in two cells and
requires those fragments to share a boundary coordinate.

Generated road previews tessellate and terrain-sample canonical fragments so
the preview remains visible over the realized heightfield. This is visual
draping, not final road grading; terrain deformation remains owned by the
`Generated Roads` Landscape layer. Building previews mass each canonical
polygon part independently, so disjoint MultiPolygon parts are never bridged
by one aggregate box. Final footprints, facades, roofs, and interiors remain
rebuildable presentation concerns rather than canonical identity.

The selected presentation profile under `Data/Presentation/` is the single
editable contract for material references, the outdoor environment, exposure,
and normalized capture viewpoints. Its runtime loader accepts only exact v1
documents, schema-safe identifier tokens, the approved parameterized terrain
parent, and `/Engine/` references for the other materials. ALIS stores the
configuration and generated actor identities; it neither copies nor relicenses
the referenced engine content. Fixture Apply derives the terrain material
instance under `/ProjectWorldTestData/Generated/Presentation/`; production Apply uses
`/ProjectWorldData/Generated/Presentation/`. It creates
or updates one generated actor per environment role, and owns one stable
camera per named viewpoint. Actor GUIDs derive from grid ID, presentation
role, and actor role, so profile/style transitions update the same actors and
a clean-map rebuild preserves D3 identity. An unchanged material instance is
not saved again. Untagged
hero overlays remain outside this ownership boundary and survive regeneration.

The fixed EV100 value is realized through deterministic Manual metering and
derived physical-camera values. This keeps physical light units stable while
project-wide auto exposure is disabled. Each generated capture camera embeds
the same settings so World Partition streaming cannot change capture exposure.

The representative runtime profile under `Data/Runtime/` is the single
editable contract for its bounded evidence route and acceptance budgets. It
does not define production territory playability or World Partition settings.
It pins an exact canonical grid and road identity, route endpoint inset, navigation
bounds, and explicit Nanite, instancing, and HLOD decisions. The strict v1
loader rejects a different grid, missing route, unknown policy, or malformed
budget before editor mutation.

Apply gives only the pinned road collision and navigation ownership, keeps its
generated cell actors and route endpoints spatially loaded, and keeps the
route navigation volume always loaded. A persisted navigation invoker covers
the full route. The single always-loaded Recast authority stays internal to
the map package; it is not a streamable external actor. Acceptance performs one real visibility hit in every declared
route-fragment cell, projects both endpoints onto Recast, and requires a
nonzero cross-cell path. It verifies the exact streamed and always-loaded actor
roles, proves that the unique route uses procedural rather than Nanite or
instanced static-mesh geometry, and requires zero HLOD proxies. Structural
evidence records generated source bytes, allocated procedural vertex/index
buffer bytes, actor count, a mesh-section draw-call upper bound, and
regeneration time. These are scoped metrics, not claims about total process
memory or observed RHI draw calls.

The commandlet realization route uses `-NullRHI`; it records the frozen frame
budget but cannot measure rendered frame time. Packaged non-NullRHI acceptance
owns the fixed-camera warmup and p95 sample, together with machine and RHI
identity. Every rendered frame in the fixed sample window counts toward p95,
including catastrophic stalls; a viewpoint's `sample_count` means frames
observed, never frames that survived a filter. A non-finite or non-positive
timing value is not a rendered frame and rejects the run immediately
(`presentation_gate_frame_time_invalid`) instead of silently extending the
window. Runtime roles may stream as the fixed viewpoint sequence advances;
ownership is inspected on every warmup and sampling tick, observed required
roles accumulate across viewpoints, and the complete set must be observed by
the final viewpoint. Any loaded actor carrying a runtime role with a stale
profile or hash, or a role duplicated in one loaded state, rejects the run at
whichever tick it first appears. The per-tick inspection cost is deliberately
inside the measured window; it biases p95 upward, the fail-safe direction.
The gate receipt records the measuring process identity, and validation
requires it to be the staged Shipping executable for the same end-to-end
operation. The gate requires zero HLOD layers, proxies, and HLOD-eligible
generated actors.

The supported wrapper treats generated map, World Partition external payload,
and generated presentation material as one bounded transaction. A rejected
Apply restores their exact prior files, including restoring an absent target
to absence, so no rejected runtime state becomes the next run's baseline. If
restoration itself fails, the wrapper preserves and reports the recovery copy
instead of deleting the only source for manual recovery.

Water, land-cover, vegetation-area, and explicit foliage-point records now
enter through Source Ingestion and Canonical Compilation, never through this
visual profile. Their later PCG, water, and foliage realizations are
transactionally replaceable consumers of those records. This prevents
presentation tuning from becoming a
second GIS source of truth.

The compiler grid owns one stable vertical origin used by coordinate mapping,
GeoReferencing, generated actor placement, and Landscape height encoding. The
adapter never derives that origin from current terrain samples, so a lower
incremental height cannot shift protected authored content.

The supported command route and evidence location are owned by
[`scripts/ue/world/README.md`](../../../scripts/ue/world/README.md).
The complete source-to-cooked-package acceptance route is owned by
[`tools/World/EndToEndValidation/`](../../../tools/World/EndToEndValidation/README.md).

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


## 13. Streaming Hints and Render Profiles

### Production Geometry Policy

Production ALIS generated worlds have no secondary HLOD proxy world. World
Partition streams cells, supported static geometry uses Nanite, and repeated
objects remain instance-owned. Landscape and other admitted geometry retain
their native representation paths.

If future far-field coverage is required beyond streamed cells, it must be an
explicit coarse geography layer with its own source, identity, ownership, and
regeneration contract. It must not be introduced as HLOD-generated proxy
geometry. Generic lifecycle code still recognizes immutable historical HLOD
records for audit, recovery, and cleanup only.

### Candidate Techniques

| Technique | How It Helps |
|-----------|--------------|
| **Nanite** | All static buildings/props use Nanite; auto-reduces triangles over distance |
| **World Partition** | Streams cells in/out; far tiles simply not loaded (no proxies needed) |
| **Instancing** | Trees, props, facade pieces use HISM/ISM + Nanite; low draw calls |
| **Design-level LOD** | Outer/procedural areas: fewer buildings, simpler shapes, less clutter |

The representative region benchmarks these concerns:

1. Far field: coarse streamed or generated representation.
2. Playable district: World Partition with Nanite and instancing.
3. Immediate gameplay area: native collision, navigation, interaction, and
   replication-authoritative geometry.

Record disk amplification, build time, memory, draw calls, streaming latency,
collision/navigation behavior, and regeneration cost before selecting a
profile.

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

### HLOD Decision

HLOD is disabled for active and future production generation. Acceptance
requires zero HLOD layers, actors, proxy geometry, companion packages, and
HLOD-eligible generated actors. Historical manifests remain immutable.


## 14. Validation

Verification has four authorities:

1. Source Ingestion and Canonical Compilation validate provider and canonical
   contracts through their public commands.
2. The [realization wrapper](../../../scripts/ue/world/README.md) owns
   transactional map mutation and Unreal realization receipts.
3. [End-to-End Validation](../../../tools/World/EndToEndValidation/README.md)
   owns profile-scoped D0-D3, clean rebuild, package, IoStore, and distribution
   acceptance.
4. Live MCP inspection supports review but is never an acceptance dependency.
   Its security, serialization, and evidence rules live in the
   [Unreal Editor MCP policy](../../../docs/ue_engine/mcp_editor_control.md).

The validation profile is the map and stage-routing SOT. A live audit resolves
the target map from that profile, then:

- confirms the loaded world package and generated ownership roots;
- requires `Map Check` to finish with zero errors and zero warnings;
- compares the live GeoReferencing CRS and projected origin with canonical
  coverage and measures a 3x3 projected -> Unreal -> projected round trip;
- checks generated cell, Landscape, route, collision, navigation, streaming,
  and presentation identities against the accepted realization receipt;
- enumerates the presentation profile's named cameras and captures the
  required fixed viewpoints; and
- confirms that observation did not leave dirty map or content packages.

Use exact `Project.World.*` automation IDs from
`Source/ProjectWorldEditor/Private/Tests/`. A live MCP test command proves only
dispatch until the editor log reports the exact test's terminal success.
Likewise, inspect the terminal `Map check complete` counts rather than treating
console-command success as a pass.

Screenshots expose geometry and presentation defects, but accepted JSON
receipts remain the reproducibility proof. Never manually repair generated
actors or assets to make a live inspection pass; change the owning profile or
generator and reapply through the supported wrapper.


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


## 16. What Stays in World-Data Plugins

World-data plugins own:

- source, profiles, controls, and accepted canonical JSON bundles;
- accepted canonical indexes and generated Unreal artifact manifests;
- generated definitions and serialized Unreal assets derived from that JSON;
- World Partition maps and external actors/objects;
- authored overlays protected by the regeneration contract;
- world-specific values selected through ProjectWorld schemas and interfaces.

ProjectWorld owns all reusable world logic, schemas,
definition/serialization types, generators, replication support, runtime
services, and validation. It has `CanContainContent=false` and owns no
concrete artifact manifests. Actor types and generation lifecycle code are
reusable logic; actor instances are serialized only in the owning data
plugin's map and external-package files.
`ProjectWorldData` is data/content-only: its authoritative JSON lives under
`Plugins/World/ProjectWorldData/Data/`, durable generation manifests under
`Data/Manifests/`, derived packages under `/ProjectWorldData/Generated/`, and
protected authored packages under `/ProjectWorldData/Authored/`. Its descriptor
activates the ProjectWorld dependency and supplies one validated content mount;
it does not add a `Source` module, fork generator logic, or provide a custom
builder.

Realization layer contracts use the reusable schema under
`ProjectWorld/Data/Schemas/project_world_realization_profile.schema.json` and
concrete owner profiles under `Data/Profiles/Realization/`. The generic
commandlet emits exact layer inventories; the existing world transaction
wrapper verifies them and publishes conditional `layer_*` manifests. There is
no second layer authority, transaction engine, or filename-based ownership.

`ProjectWorldTestData` has the same data/content-only boundary for synthetic
fixtures, but is enabled only for Editor targets and excluded from shipping.


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
