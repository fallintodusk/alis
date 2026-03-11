# City17

City17 is the reference ALIS world plugin: a dense urban tile built on World Partition, data layers, and external manifests.

It is an **example gameplay world** with:
- Urban environment and POIs.
- Support for combat and other mechanics (via GameModes and features, not directly).
- Room for procedural content (PCG graphs driven by manifests).


## 1. Purpose

City17 provides:

- A concrete implementation of the **world / tile model**:
  - One 8.192 km x 8.192 km World Partition map.
  - Standard grid and data layer conventions.
- A testbed for:
  - Single-player mode (`ProjectSinglePlay`).
  - Online/multiplayer mode (`ProjectOnlinePlay`) later.
- A reference pattern for future cities / regions in ALIS:
  - Tiling.
  - Manifests.
  - PCG integration.
  - Agent workflows.

City17 does **not** implement gameplay logic itself. It provides the space, not the rules.


## 2. Position in ALIS architecture

Layering:

- **Foundation tier**
  - `ProjectCore`, `ProjectLoading`, `ProjectExperience`, etc.
- **Gameplay tier**
  - `ProjectSinglePlay` (single-player mode).
  - `ProjectOnlinePlay` (online / multiplayer mode).
- **World tier**
  - `City17` (this plugin).
  - Future world plugins.

City17 is a **world plugin**:

- Owns maps and world-specific data (tile metadata, data layers).
- Provides map entries for ProjectMenuMain's map browser.
- GameMode binding options:
  - **Option A:** WorldSettings.DefaultGameMode (compile-time)
  - **Option B:** Travel URL `?game=...&Mode=...` (runtime, recommended)
- Does NOT own gameplay or features - those come from GameMode.


## 3. Activation strategy

- **OnDemand (code)**:
  - City17 DLL is loaded at boot by Orchestrator as an OnDemand plugin (code-only ready).
- **OnDemand (content)**:
  - City17 content (IoStore bundles) is mounted by `ProjectLoading` when requested by:
    - Current experience.
    - Manifest / entrypoint configuration.

Typical flow:

1. Menu / launcher picks experience `"City17"`.
2. `ILoadingService` starts loading `"City17"`:
   - Ensures the City17 plugin is loaded (DLL).
   - Requests content bundles (IoStore) for City17.
3. Engine loads the City17 map.
4. WorldSettings choose the GameMode (single-player or online).
5. GameMode consolidates gameplay features; City17 stays content-only.


## 4. Responsibilities

City17 is responsible for:

- **World Partition maps and tile setup**
  - Main WP map for the central tile:
    - For example `City17_WP_00_00.umap`.
  - World Partition configuration:
    - Single runtime grid.
    - Cell size 256 m.
    - Loading radius ~768–1024 m.
- **Data layer composition**
  - Terrain, buildings, POIs split into documented data layers.
- **Streaming region configuration**
  - Streaming source(s) and grid parameters for the City17 tile.
- **PCG integration**
  - PCG graphs and assets (if any) that operate inside City17.
  - PCG is driven by external manifests / parameters, not hard-coded logic.
- **Map entry in ProjectMenuMain**
  - City17 maps are listed in ProjectMenuMain's map browser.
  - No experience descriptors - loading is controlled by:
    - FLoadRequest (map soft path + CustomOptions)
    - GameMode chosen via URL (`?game=...&Mode=...`)
  - GameMode (ProjectSinglePlay/ProjectOnlinePlay) handles feature loading.
- **World Settings**
  - Declares, per map:
    - Single-player GameMode (`ProjectSinglePlay`).
    - Online GameMode (`ProjectOnlinePlay`).
  - GameMode acts as feature consolidator.
- **Tile metadata**
  - Stores tile indices (TX, TY) and optional real-world bounds as data, not code.

In short: City17 owns **where** things are and **what** the space looks like, not **how** the game rules work.


## 5. Non-responsibilities

City17 explicitly does **not**:

- Implement gameplay logic:
  - No combat formulas.
  - No dialogue systems.
  - No inventory logic.
- Implement gameplay features:
  - No ProjectCombat / ProjectDialogue / ProjectInventory code.
  - No feature-specific components.
- Implement core procedural generation algorithms:
  - Those live in a dedicated `ProjectPCG` or similar systems plugin.
- Own single-player or online orchestration:
  - Those are the responsibility of `ProjectSinglePlay` and `ProjectOnlinePlay`.

World -> GameMode -> Features is the direction, not the reverse.


## 6. Dependencies

City17 can operate in two modes:

**Option A - World Settings binding (compile-time):**
- City17 depends on ProjectSinglePlay / ProjectOnlinePlay
- GameMode set in World Settings

**Option B - URL binding (runtime, RECOMMENDED):**
- City17 has NO gameplay plugin dependencies
- GameMode specified via travel URL (`?game=...&Mode=...`)
- Same map can run single-player or online by changing only the travel URL

Core dependencies (both options):
- **ProjectCore** - Base utilities and interfaces
- **ProjectLoading** - Content mounting, load phases

Optional dependencies:
- **ProjectPCG** - Shared PCG logic (if PCG used)

City17 must **not** depend directly on:
- Individual feature plugins (ProjectCombat, ProjectDialogue, ProjectInventory)
- Other world plugins

**Key principle:** City17 provides geography. GameMode provides gameplay.


## 7. World scale and tiling (City17 tile spec)

City17 follows the ALIS global tiling model.

Tile definition:

- One tile = **8,192 m x 8,192 m**.
- Coordinates in tile space:
  - `x` increases east.
  - `y` increases north.
  - Units: meters.

In Unreal Engine:

- 1 meter = 100 Unreal units.
- Tile width = `8192 * 100 = 819,200` units.

World Partition grid in City17 maps:

- Single runtime grid:
  - Cell size: **256 m x 256 m**.
  - Loading radius: **~768–1024 m**.
- Cells per tile:
  - `8192 / 256 = 32` per axis.
  - Total: **32 x 32 = 1024 cells**.

City17 maps assume these numbers. Tools and agents can rely on them.


## 8. Data layer conventions (City17)

City17 uses data layers to organize environment, gameplay, and world states.

Suggested baseline:

- `City17_Environment_Base`
  - Core city layout:
    - Terrain / ground.
    - Major buildings.
    - Big static meshes and structures.
- `City17_Gameplay_Common`
  - Doors, triggers, cover, nav blockers, interactions.
- `City17_Event_*`
  - World state variants:
    - `City17_Event_Blackout`
    - `City17_Event_Shelling`
    - `City17_Event_MartialLaw`
- `City17_Interior_*`
  - Interior layers, per district or building group:
    - `City17_Interior_DistrictA`
    - `City17_Interior_MetroHub`
- Optional:
  - `City17_HandTuned`
    - For very specific hand-crafted areas where agents should not overwrite content.

Rules:

- Base environment lives in `*_Environment_Base`.
- Core gameplay elements live in `*_Gameplay_Common` or event layers.
- Event and mission states live in `*_Event_*`.
- Hand-tuned areas are small and explicitly documented.


## 9. Source-of-truth for geometry and content

City17’s geometry and layout are driven by **manifests**, not the `.umap`.

Per tile, City17 uses a manifest file, for example:

- `World/Geo/City17/tile_00_00.json`
- or `World/Geo/City17/tile_00_00.dsl`

Manifest concept:

- `tile`:
  - `id`: `"City17_00_00"`
  - `TX`, `TY`: integers.
  - Optional real-world bounds.
- `terrain_layers`:
  - Height and material info.
- `roads`:
  - Polylines in tile coordinates.
- `lots`:
  - Building footprints and zoning.
- `buildings`:
  - Instances of modular building presets.
- `props`:
  - Street furniture, lights, signage.
- `data_layer` per object:
  - Which data layer each object belongs to.

Editors and agents modify these manifests. The maps are rebuilt from them.


## 10. Builder workflow (City17)

A world builder tool (commandlet + editor tool) uses manifests to build City17 maps.

High-level steps:

1. Read City17 tile manifest(s).
2. For each category (terrain, roads, buildings, props):
   - Compute required actors.
3. Open or create the City17 tile map (for example `City17_WP_00_00`).
4. Spawn / update actors:
   - Place them according to tile coordinates (meters -> Unreal units).
   - Set data layer membership (`City17_Environment_Base`, etc.).
   - Prefer one-file-per-actor to keep diffs clean.
5. Save the map.

Usage:

- In editor:
  - `Tools -> ALIS -> City17 -> Rebuild Tile 00_00 From Manifest`.
- Command line (example):
  - `UEEditor-Cmd ... -run=AlisTileBuild -World=City17 -Tile=00_00`.

No engine restart is needed to apply manifest changes:
- Update manifest -> run builder -> save.


## 11. Loading pattern (no experience descriptors)

City17 does NOT register experience descriptors. Loading is controlled by:

1. **Map entry in ProjectMenuMain** - User selects map from browser
2. **FLoadRequest** - Contains map path and CustomOptions
3. **GameMode** - Chosen via URL, handles feature loading

```
ProjectMenuMain (map list)
       |
       v
FLoadRequest
  MapSoftPath: /City17/Maps/City17_Persistent.City17_Persistent
  CustomOptions:
    game = /Script/ProjectSinglePlay.ASinglePlayerGameMode
    Mode = Medium
       |
       v
ProjectLoading (travel)
       |
       v
ASinglePlayerGameMode
  - Loads mode config (Beginner/Medium/Hardcore)
  - Ensures feature plugins loaded
  - Attaches feature components to pawn
```

The GameMode (not the world) decides which features are active.
This allows the same map to support different gameplay modes.


## 12. Activation flow

Complete activation flow for City17 single-player:

1. Player selects "City17" map in W_MapBrowser (ProjectMenuMain)
2. ProjectMapListViewModel builds FLoadRequest:
   - MapSoftPath = City17 map asset
   - CustomOptions["game"] = ASinglePlayerGameMode
   - CustomOptions["Mode"] = Medium
3. ILoadingService->StartLoad(Request)
4. ProjectLoading pipeline:
   - Resolves assets
   - Mounts content
   - Builds travel URL from CustomOptions
   - Calls ServerTravel()
5. Engine loads City17 map
6. ASinglePlayerGameMode::InitGame():
   - Parses Mode from URL
   - Loads FSinglePlayModeConfig
   - Ensures required feature plugins are loaded
7. ASinglePlayerGameMode::HandleStartingNewPlayer():
   - Spawns PlayerController and Pawn
   - Attaches feature components to pawn
   - Configures components via IFeatureConfigurable
8. Player starts in City17 with features active

**Multiplayer** uses same flow with:
- CustomOptions["game"] = AOnlinePlayGameMode
- GameMode from ProjectOnlinePlay handles online features


## 13. Guidelines for contributors and agents

### 13.1. Human responsibilities

Humans:

- Define and evolve:
  - The visual and structural identity of City17.
  - Data layer taxonomy and naming.
  - Tile coverage (which areas City17 includes).
- Review:
  - Manifest changes that add or remove major structures.
  - Data layer changes.
  - Tile boundaries and new tiles.

Acceptable manual work:

- Creating / duplicating maps from the tile template.
- Setting World Settings GameMode override (if used).
- Hand-tuning small areas in dedicated layers like `City17_HandTuned`.


### 13.2. Agent responsibilities

Agents may:

- Edit City17 manifests:
  - Roads, buildings, props, zones.
  - Data layer assignments for new content.
- Propose:
  - New PCG parameter sets for parts of City17 (but the PCG logic itself lives in `ProjectPCG`).
  - Layout adjustments based on references or performance notes.
- Generate:
  - Initial layouts from external data sources (for example OpenStreetMap).
  - Variants for different event data layers.

Agents must **not**:

- Edit `.umap` files directly.
- Change World Partition grid or cell size in City17 maps.
- Add or remove plugin dependencies in `City17.uplugin`.
- Introduce direct references from City17 to feature plugins (combat, dialogue, inventory).
- Bypass manifests and builder workflow.


### 13.3. Invariants

City17 invariants:

- Tile size and grid:
  - Tile: 8192 m x 8192 m.
  - Cell size: 256 m.
  - 32 x 32 cells per tile.
- Source-of-truth:
  - Geometry and placements come from manifests, not hand-edited maps (except small hand-tuned layers).
- Dependencies:
  - World can remain agnostic of gameplay plugins (Option B recommended)
  - GameModes depend on features via soft references, not via City17
- Loading pattern:
  - Map selected via ProjectMenuMain map browser
  - GameMode chosen via travel URL (`?game=...&Mode=...`)
  - No experience descriptors - GameMode handles features

Any change that breaks these invariants must be treated as an architectural change and documented (ADR) before implementation.


## 14. Related docs

- `Plugins/ProjectSinglePlay/README.md`
  - Single-player orchestrator design.
- `Plugins/Worlds/.../README.md`
  - Global worlds and tiling spec (if kept separately).
- `Plugins/Boot/Orchestrator/README.md`
  - OnDemand activation and plugin lifecycle.
