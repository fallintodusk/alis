# World Partition Architecture

Comprehensive guide for World Partition setup, data-driven manifests, and in-editor workflows.

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Kazan reconstruction decision state](#kazan-reconstruction-decision-state)
3. [Evidence-grounded starting design](#evidence-grounded-starting-design)
4. [World Manifest Design](#world-manifest-design)
5. [Generation Strategy](#generation-strategy)
6. [Loading Flow](#loading-flow)
7. [Automated design and performance gate](#automated-design-and-performance-gate)
8. [Editor Workflow](#editor-workflow)
9. [Validation & Testing](#validation--testing)
10. [Troubleshooting](#troubleshooting)
11. [Outstanding Work](#outstanding-work)

---

## Architecture Overview

**Goals:**
- One persistent World Partition map per realized world
- `ProjectWorld` owns world logic, types, policies, and validation
- ProjectWorldData contains source JSON and derived Unreal assets
- Machine-readable profiles apply and verify map settings without manual UI authority

**Key Components:**
- **World Partition Map**: minimal persistent root plus spatially loaded actors
- **World Manifests** (`UProjectWorldManifest`): map root, regions, and metadata
- **Region Descriptors** (`FProjectWorldRegionDescriptor`): Individual streaming regions
- **Loading Subsystem** (`ProjectLoadingSubsystem`): Orchestrates world streaming
- **Experience Descriptors**: loading-owned logical IDs that select map and game mode

---

## Kazan reconstruction decision state

Accepted requirements:

- the complete `5.58 km`-radius product territory is generated, supports
  player collision, and proves spatial load, unload, and reload;
- World Partition configuration is data/profile-driven and reproduced by
  automation rather than retained as undocumented editor state;
- use one profile-driven 2D runtime grid; `256 m` cells and a `768 m` loading
  range are the evidence-based baseline, not an editor default;
- use one logical Landscape for Kazan v1, divided into streaming proxies;
- the RTX 4070 quality target is High at 1440p and 60 FPS; RTX 3060-class
  hardware targets Medium at 1080p and 60 FPS with an explicit 30 FPS fallback;
  more expensive Epic or Cinematic presets are optional tiers, not gates;
- manual editor traversal and screenshots are diagnostics, never acceptance
  authority.

The architecture is frozen. The automated performance slice still selects the
winning runtime-grid values, Landscape proxy bundling, and measured
Nanite/instancing settings from the bounded candidates below. HLOD is disabled
for `kazan_territory_v1` and is not a candidate. Existing representative-map
settings are evidence only and cannot silently become production defaults.

## Evidence-grounded starting design

Epic documents one 2D Runtime Hash grid as the default and warns that multiple
runtime grids can negatively affect performance. Its Big City example uses a
`256 m` runtime cell and `768 m` loading range. Kazan adopts those values as
the baseline candidate and changes them only through the automated gate.

The production profile exposes one 2D grid's cell size, loading range,
streaming-source roles, slow-streaming policy, and geometry policy. The build
applies those values and the receipt reads them back from the produced map.
Candidate comparison starts with `128/768`, `256/768`, and `512/1536` metres;
the gate may reject candidates before a full cook when per-cell actor/package
weight or source-speed coverage is already invalid.

The compiler's `930 m` canonical cells are source and regeneration units.
They are independent from runtime grid cells and Landscape streaming proxies.
Forcing all three grids to match is not a design goal.

Official basis:
[World Partition](https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-in-unreal-engine),
[HLOD](https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition---hierarchical-level-of-detail-in-unreal-engine), and
[Builder commandlets](https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-builder-commandlet-reference).

---

## World Manifest Design

World manifests (`UProjectWorldManifest`) carry:

- **WorldRoot**: Soft reference to the World Partition map
- **Regions**: Array of `FProjectWorldRegionDescriptor` entries
  - Region ID (string)
  - Display name (localized)
  - Bounds (world space coordinates)
  - Partition grid reference
  - Optional asset references
- **FeatureDependencies**: Cross-plugin features required for the world to load
- **PartitionMetadata**: Future hook for auto-generated JSON/DataAsset with cell-to-region mapping

Experience descriptors belong to ProjectLoading. Menu widgets request their
logical IDs and do not own map package paths. No `ProjectData` plugin or
`UProjectExperienceManifest` is part of the current architecture.

---

## Generation Strategy

The canonical-to-Unreal adapter already creates generated World Partition
maps for bounded profiles. Production scale extends that route; it does not
introduce a manual map-authoring authority.

1. Read ProjectWorldData's accepted JSON/profile and canonical
   bundle.
2. Create or update the minimal persistent map and generated-owned spatial
   actors.
3. Apply the declared runtime hash, Landscape, Data Layer, Nanite, and
   instancing settings.
4. Build required derived data through supported commandlets without an HLOD
   build.
5. Read the produced descriptors/settings back and emit an acceptance receipt.
6. Activate immutable artifact manifests only after every gate passes.

The generated map is a derived serialized representation. Editing it by hand
does not change the source JSON and is rejected as generated-tree drift.

### UE 5.8 native reuse gate

ALIS owns geographic policy, deterministic canonical inputs, identity,
provenance, dirty scope, orchestration, and acceptance. Unreal owns Unreal
coordinate conversion, geometry primitives, asset construction, rendering,
partitioning, serialization, and streaming. Before implementing a generator,
inspect the installed UE 5.8 Runtime/Core APIs and plugins for a native
primitive. Custom Unreal infrastructure is permitted only when a focused twin
records the exact ALIS invariant the native path cannot satisfy.

| Need | UE 5.8 first choice | ALIS boundary and required proof |
|---|---|---|
| Projected CRS to engine space | `GeoReferencing` Runtime plugin | Supply accepted CRS/origin data and prove the geodetic error budget; do not add a second CRS transformer. |
| Landscape partitioning | `FLandscapeConfigHelper::PartitionLandscape` | Orchestrate one logical Landscape and verify proxy/cell identity; do not construct proxies manually. |
| Polygon-with-holes and working mesh | `FGeneralPolygon2d` and `FDynamicMesh3` from `GeometryCore` | Keep canonical geometry in JSON; use native double-precision Unreal structures only while realizing it. |
| Boolean, clipping, and constrained triangulation | `GeometryAlgorithms`, including `TConstrainedDelaunay2` | The installed `Geometry Processing` plugin is Beta in UE 5.8 and is enabled only for Editor targets. The exact twin proves holes, shared edges, semantic no-op identity, and save/reload. Packaged runtime consumes saved StaticMesh assets without this generation dependency; the integrated candidate must still prove cook and packaged loading before production admission. |
| Persistent static mesh | `FMeshDescription` and `UStaticMesh::BuildFromMeshDescriptions` | Build and save inside the Editor commandlet transaction; do not invent static-mesh serialization or runtime generation. |
| Water shading | Single Layer Water material shading model and native material output expression | UE 5.8 Nanite rejects this shading model, and the shading-model enum alone does not compile a valid water material. Water uses persistent non-Nanite StaticMesh assets; ALIS owns material parameters and canonical surface geometry, not another water renderer. |
| Spatial ownership and streaming | World Partition and OFPA | ALIS maps canonical cells to actors/packages and audits them; UE loads and unloads spatial actors. |
| Vegetation in 3C | Evaluate partitioned PCG before custom placement infrastructure | Admission still requires deterministic identity, exact manifests, dirty scope, Nanite/instancing, and measured runtime proof. |

The immediate twin therefore routes accepted canonical data through
GeoReferencing, LandscapeConfigHelper, GeometryCore and the gated
GeometryAlgorithms module, MeshDescription/StaticMesh, Single Layer Water,
Nanite only for compatible opaque geometry, and World Partition. Blueprint
Geometry Script is not a core dependency.

Generated-package no-op is decided from deterministic semantic input/output
identity before serialization. Re-saving an unchanged Unreal package is not a
byte-stability contract and must not be used to decide whether generation is a
no-op.

The Experimental Water plugin is not canonical or realization authority: its
WaterBody lifecycle and implicit landscape behavior do not own ALIS identity,
cell packages, dirty scope, or transaction semantics. Experimental
MeshPartition/MegaMesh is also excluded from Slice 3. Either may be
reconsidered only through a later operator-approved contract when Epic changes
its maturity and a focused proof shows a simpler compliant path.

ALIS references Epic headers and modules through normal Unreal Build Tool
dependencies. Never copy or vendor Epic Engine source into the public
repository; developers obtain the engine from Epic. Distribution remains
subject to the current [Unreal Engine EULA](https://www.unrealengine.com/eula/unreal).

### Landscape topology

Kazan v1 uses one logical Landscape identity with many
`LandscapeStreamingProxy` actors, not one unrelated Landscape per source tile
and not the representative adapter's one always-loaded Landscape actor as the
production endpoint. Epic's tiled-heightmap World Partition import creates
this model and exposes both Landscape proxy grid size and region size.
The logical root carries shared Landscape identity and settings; the heavy
terrain components live in spatial proxies, so one logical Landscape does not
mean loading the whole territory at once. Production validation must count and
verify proxies and must not retain the fixture adapter's exactly-one-actor
assumption.

The v1 baseline maps each `930 m` canonical source cell to one `31 x 31`-quad
Landscape component at `30 m` vertex spacing and one streaming proxy. The
15 x 14 technical envelope therefore contains 210 components under one logical
Landscape, well below Epic's recommended maximum of 1024 components for its
largest Landscapes. The performance gate may bundle adjacent components into
fewer proxies, but it may not change component-to-canonical-cell identity,
shared-edge samples, or dirty-regeneration ownership.

The 1024-component guidance is not a maximum world-generation area. It is a
practical topology recommendation for one logical Landscape. World Partition
streams proxies, and the versioned multi-partition escape below permits later
territory growth without introducing many Landscapes in v1.

The world descriptor models `terrain_partitions` as a list from the start but
Kazan v1 contains only `kazan_main`. This is a small escape hatch, not an
instruction to create many Landscapes. If later additive expansion would
exceed the 1024-component recommendation, engine constraints, or measured
memory/streaming gates, regeneration splits stable canonical-cell groups into
additional logical Landscape partitions. The split changes only a versioned
derived topology: CRS/grid authority, cell IDs, layer IDs, anchors, and
gameplay references remain unchanged. No hand migration or parallel terrain
authority is permitted.

Landscape Edit Layers remain the non-destructive separation mechanism for
generated base height, generated road/deformation passes, and protected
authored corrections. Streaming proxies divide component/package ownership;
edit layers divide terrain-edit authority. Neither replaces the world
generation-layer manifest.

### Production realization invariants

The `kazan_territory_v1` realization uses
`FLandscapeConfigHelper::PartitionLandscape(..., 1)` to produce exactly 210
cell-owned components and 210 streaming proxies at 30 m sample spacing. Each
component/proxy keeps its canonical cell identity. GeoReferencing placement is
validated against the same projected coordinate authority with a maximum
error of 0.01 m; the accepted production path currently observes zero error.

Water is realized as persistent cell-local `UStaticMesh` assets and spatial
actors. Each authoritative surface is prepared once over its complete
canonical-cell domain, then the same prepared triangles are clipped into the
dirty target cells. This keeps polygon holes, ribbon width, shared XY/Z seams,
and semantic identity independent of whether the operation is full or
incremental. Water assets use the profile-owned Single Layer Water material,
remain non-Nanite, and generate no collision, navigation data, mesh distance
field, authored LOD chain, or HLOD representation.

An unchanged Apply must preserve actors, packages, layer manifests, and the
active manifest set without invoking map serialization. UE 5.8 may retain an
empty structural World Partition HLOD setup row; it is not an HLOD reference.
Policy enforcement counts actual default/nested HLOD layer references and
generated HLOD artifacts, all of which must remain zero. Clearing an empty
structural row is forbidden because it creates false package churn when the
engine restores that row on load.

Clean reconstruction may recreate engine-internal identifiers, but it must
reproduce the same stable cell/package ownership and layer semantic identity.
Rejected operations restore the exact prior files, and protected Authored
packages remain byte-identical across Apply, reconstruction, rollback, and
Delete.

### Geometry representation policy

`kazan_territory_v1` has no HLOD layers, HLOD actors, proxy/merged/simplified
HLOD meshes, or HLOD companion packages. The runtime gate verifies their
absence. This rejects the second distant-representation authority and its
territory-scale build, storage, cook, and regeneration cost.

Supported generated static meshes use Nanite when their material path is
compatible, instead of authored LOD chains or HLOD replacement geometry.
UE 5.8 explicitly excludes Single Layer Water from Nanite's supported shading
models, so cell-local water is a persistent non-Nanite StaticMesh with no
authored LOD chain and no HLOD fallback. Landscape keeps its native component
and World Partition proxy streaming and builds its Nanite representation; UE-required
non-Nanite Landscape data remains an engine implementation dependency, not a
second project LOD policy. Vegetation uses instance ownership (ISM/HISM or the
accepted PCG output) with Nanite-enabled static meshes. Experimental Nanite
Foliage systems are not required by this decision and still need their own
profile-specific proof and supported fallback. Geometry that cannot use an
admitted native or Nanite path is rejected from the territory profile until
explicitly decided; water is the narrowly proven native material exception and
does not authorize another custom renderer or HLOD.

Active P0 and representative maps contain no HLOD companion assets or
serialized HLOD layer references. Older immutable manifest generations retain
their historical inventory for provenance and recovery. Generic transaction
code may recognize those records for exact ownership, rollback, and cleanup,
but active and future generation must not recreate them.

Official basis:
[Landscape heightmap import](https://dev.epicgames.com/documentation/en-us/unreal-engine/importing-and-exporting-landscape-heightmaps-in-unreal-engine) and
[Landscape Technical Guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-technical-guide-in-unreal-engine), plus
[Landscape Edit Layers](https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-edit-layers-in-unreal-engine) and
[Nanite Landscapes](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-nanite-with-landscapes-in-unreal-engine).

---

## Loading Flow

1. **Boot**: the game loads `MainMenuWorld` and creates the menu UI.
2. **Selection**: an explicit menu entry requests a logical experience ID.
3. **Resolution**: the loading registry resolves its experience descriptor to
   the map and game mode.
4. **Travel**: `ProjectLoadingSubsystem` performs validated map travel.
5. **Streaming**: World Partition streams spatial cells from the player and
   any explicitly enabled streaming sources.

**See Also:**
- [Loading documentation](../../../../docs/loading/README.md) - Boot and loading routes
- [Build documentation](../../../../docs/build/README.md) - Supported build commands

---

## Automated design and performance gate

The initial design is selected in three evidence stages:

1. Static partition audit: actor bounds, reference bundles, expected cells,
   per-cell package weight, Landscape proxy ownership, Data Layer membership,
   Nanite/instance policy, and confirmed HLOD absence.
2. Commandlet/cook audit: generated descriptors, build success, cook size,
   missing packages, and settings read-back.
3. Deterministic packaged traversal: dense centre, long diagonal, perimeter,
   unload/reload backtrack, and a future-vehicle speed stress route.

Receipts record loaded and activated cells, time-to-ready, streaming failures,
peak process memory, p95/p99/max hitch, CPU/GPU frame time, and hardware/preset
identity. A human walkthrough cannot accept or reject a configuration.

The accepted primary quality gate is RTX 4070, High preset, 1440p, and 60 FPS.
In frame terms, p95 must remain at or below `16.67 ms`; a `33.34 ms` threshold
is only a 30 FPS fallback/failure boundary, not a 60 FPS success gate. Epic or
Cinematic presets may target stronger cards and are never allowed to weaken
the High gate.

The accepted lower tier is RTX 3060-class hardware, Medium preset, 1080p, with
a 60 FPS target and an explicit user-selectable 30 FPS fallback. A vague
`30-60 FPS` acceptance band is rejected because it permits unstable frame
pacing. Use Unreal scalability groups, bounded TSR or resolution scaling, and
normal `GameUserSettings`; do not create per-GPU rendering code.

The performance slice evaluates the declared candidates with identical builds,
routes, hardware state, and capture rules. Hard correctness, memory, cook,
streaming, and frame-budget gates are applied first. Among passing candidates,
the selector chooses the lowest p99 hitch cost, then peak memory, then cell
activation churn. It writes the winner and evidence into the machine-readable
profile/receipt and updates this SOT. No extra operator approval is required
when a candidate stays inside these frozen constraints. If none passes, or a
solution would change quality, territory, hardware, data ownership, or the
one-grid/one-logical-Landscape architecture, the slice stops for an operator
decision.

Epic's Lumen guidance assigns High to a 60 FPS target, Epic to 30 FPS, and
Medium to lower-end PCs. See the
[Lumen Performance Guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-performance-guide-for-unreal-engine) and
[Scalability Reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/scalability-reference-for-unreal-engine).

---

## Editor Workflow

### Quick Start (~5 minutes)

1. **Launch Editor** from the repository root with `scripts/ue/run/run_editor.bat`.
2. **Open Map**: `/ProjectWorldData/Generated/Representative/L_ProjectWorldKazan_Representative`.
3. **Verify World Partition**: World Settings -> World Partition checkbox enabled
4. **Load Data Layers if present**: use the Data Layer Outliner and the names
   stored by the selected map; do not infer a layer from an example name.
5. **Check Streaming Cells**: Verify correct cells are visible
6. **Bake (if needed)**: Build -> Navigation/Lighting only if you touched relevant actors; do not build HLOD for the territory profile
7. **Save & Test**: run one exact relevant automation test through
   `scripts/ue/test/unit/run_single.ps1` as shown below.

### Preparing the Editor

**Editor Preferences:**
- Enable *Loading & Saving -> Monitor Content Directories* (for JSON hot reload)
- Disable *Live Coding* during heavy world edits (avoid unnecessary recompiles)
- Enable *General -> Miscellaneous -> Enable Editor Utility Hot Reload*

**Project Settings:**
- Set *Editor -> Asset Editor Open Location* to *Last Docked Tab* (keeps World Partition panels consistent)

**Command Line:**
- Use `-ulog` to place logs under `Saved/Logs/` for easier diffing

### Working with World Partition

**Streaming Cells:**
- Open Cell Selection window: **Window -> World Partition**
- Toggle only the neighborhoods you're modifying (saves memory)
- Use `stat levels` console command to verify loaded cells

**Nanite / Foliage:**
- Keep generated static-mesh assets Nanite-enabled where their admitted
  pipeline supports it.
- Keep foliage instance-owned; do not assign territory actors or PCG output
  to HLOD layers.
- Do not run an HLOD build for the territory profile.

**Data Layers:**
- No ProjectWorld Data Layer naming convention is frozen yet.
- Record an accepted convention here before adding new generated-world layers.
- Load/unload via Data Layer Outliner (right-click -> Load Selected)

### Post-Edit Validation

Run these console commands from the editor Output Log:

```
wp.Runtime.ToggleLoading   // Ensures runtime streaming is responsive
stat levels                // Verifies expected cells are loaded
wp.Runtime.DumpState       // Outputs streaming state to Saved/Logs/WorldPartition.log
```

---

## Validation & Testing

### Automated Tests

```powershell
# One exact loading test
scripts/ue/test/unit/run_single.ps1 "ProjectLoading.Subsystem.Validation.ValidRequest"

# One exact generated-world test
scripts/ue/test/unit/run_single.ps1 "Project.World.Authored.Anchor.Coordinate"
```

### Manual Checks

**Before committing map changes:**
1. [OK] World Partition enabled in World Settings
2. [OK] Any Data Layers follow the convention accepted for this map
3. [OK] Streaming cells load correctly in PIE
4. [OK] Console validation commands pass (`wp.Runtime.DumpState` shows no errors)
5. [OK] The exact tests relevant to the edit pass

**Asset Manager Integration:**
```bash
# Verify accepted WorldManifest assets are discoverable under the owning
# world-data plugin mount in Saved/Logs/LogAssetManager.
```

---

## Troubleshooting

### Common Issues

| Symptom | Cause | Fix |
|---------|-------|-----|
| "No Loaded Region(s)" after UE 5.7.2 migration | Editor streaming settings reset | *Editor Preferences -> World Partition*: uncheck "Enable Loading in Editor"; *Project Settings -> Level Instance*: check "Enable Streaming"; restart editor |
| Cells refuse to load in PIE | Data layer not loaded | World Partition window -> right-click data layer -> *Load Selected* |
| Lighting rebuild prompts after every save | Auto-build option enabled | Disable *Editor Preferences -> Level Editor -> Miscellaneous -> Automatically Apply Build Data* |
| Map fails in automation | World Partition config mismatch | Run the one exact failing test through `scripts/ue/test/unit/run_single.ps1`; follow `docs/testing/troubleshooting.md` for its log and report evidence |
| World manifest not found in Asset Manager | Asset not scanned or wrong owning plugin | Check the world-data descriptor, plugin mount, scan registration, and `WorldManifest` primary asset ID |
| Region streaming hangs | Circular dependency in FeatureDependencies | Review manifest FeatureDependencies, ensure no circular references |

### Debug Commands

```
# World Partition state dump
wp.Runtime.DumpState

# Show loaded levels
stat levels

# Toggle runtime loading
wp.Runtime.ToggleLoading

# Asset Manager diagnostics
AssetManager.DumpAssetRegistry WorldManifest
```

### Log Files

- **World Partition**: `Saved/Logs/WorldPartition.log`
- **Asset Manager**: `Saved/Logs/LogAssetManager.log`
- **Loading Subsystem**: `Saved/Logs/LogProjectLoading.log`

---

## Outstanding Work

**Automation:**
- Implement manifest auto-generation using World Partition runtime descriptors
- Wire telemetry storage for region load times
- Define analytics schema for session/world/region transitions

**Testing:**
- Create `ProjectWorld` test module for World Partition validation
- Add automated tests for region streaming
- CI integration for map validation

**Documentation:**
- Document all Data Layer conventions
- Create region descriptor templates
- Add World Partition best practices guide

**Status:** not yet implemented; tracked in the active world milestone.

---

## See Also

- [Loading documentation](../../../../docs/loading/README.md) - Phase and subsystem routes
- [Asset Manager Configuration](../../../../docs/loading/asset_manager.md) - Primary asset setup
- [Experience Manual](../../../Gameplay/ProjectGameplay/docs/manual_experiences.md) - Player experience configuration
- [Architecture Principles](../../../../docs/architecture/principles.md) - Data-driven patterns
