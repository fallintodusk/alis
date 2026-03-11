# Plugin Architecture

> **Source of Truth:** [architecture/source_of_truth.md](architecture/source_of_truth.md) + C4 DSL (`architecture/diagrams/workspace.dsl` and `architecture/diagrams/views/`). Use these as the map; implementation lives with each plugin.

**Universal modular game architecture for Unreal Engine 5. Keep docs code-free: reference the class/method in the owning plugin instead of pasting snippets.**

See [principles.md](principles.md) for naming conventions and philosophy.

## Authoritative Plugin Layout (Current)

This layout is authoritative. It maps to our C4 tiers and SOLID guardrails.

```
Plugins/
  Boot/
    Orchestrator/
      -> Updatable coordinator for features/systems
      -> At app start (boot) checks and applies plugin updates before gameplay modules load
      -> Resolve deps, download/verify, stage, activate (install-before-load)
      -> User approval may gate application

  Foundation/
    ProjectCore/
      -> Base types, helpers, service locator
      -> No gameplay/UI
    ProjectSharedTypes/
      -> Shared data-only types (structs/enums) used across plugins
      -> No AssetManager calls, no gameplay logic

  Systems/
    ProjectLoading/
      -> 6-phase loading pipeline (implements ILoadingService from ProjectCore)
      -> Called by Features via ServiceLocator (DIP pattern)
      -> Full delegate API for progress tracking

    ProjectSave/
      -> Save/load service and slot management
      -> UI-agnostic

    ProjectSettings/
      -> Settings storage/defaults, typed read/write API
      -> Used by gameplay and UI

  World/
    ProjectWorld/
      -> World Partition/HLOD/streaming policies + UProjectWorldManifest data assets
      -> World framework (tiles, streaming, spatial queries)

    PCG/
      ProjectPCG/
        -> PCG engine integration: nodes, registries, runtime generation

      ProjectForestBiomesPack/
        -> Content-only forest biomes pack

      ProjectUrbanRuinsPCGRecipe/
        -> Content-only PCG recipes for urban ruins

    City17/
      -> Concrete gameplay world using ProjectWorld framework and PCG recipes

    MainMenuWorld/
      -> Frontend world composition (maps, lighting, data layers)

  UI/
    ProjectUI/
      -> CommonUI/MVVM setup, widget stacks, base widgets, theming
      -> No front-end flow logic

    ProjectMenuMain/
      -> Front-end menu feature: menu logic + screens
      -> Structure internally Core/ and Shell/

    ProjectMenuGame/
      -> In-game / pause menu feature shown during gameplay

    ProjectSettingsUI/
      -> Settings screens using ProjectSettings service

  Gameplay/
    ProjectGameplay/
      -> Base gameplay classes and interfaces (GameMode base, PlayerController base)

    ProjectMenuPlay/
      -> GameMode for main menu / frontend world (lightweight, no pawn/HUD)

    ProjectSinglePlay/
      -> GameMode for single-player gameplay (ensures Combat, Dialogue, Inventory features)

    ProjectOnlinePlay/
      -> GameMode for online multiplayer gameplay (client/server, replication, matchmaking)

  Features/
    ProjectFeature/
      -> Feature contracts (FeatureInitContext, FeatureRegistry)
      -> All Features depend on this for self-registration

    ProjectCombat/
      -> Combat gameplay feature (weapons, damage, health) - self-contained with own interfaces

    ProjectDialogue/
      -> Dialogue/conversation gameplay feature with UI - self-contained

    ProjectInventory/
      -> Inventory gameplay feature (items, slots, rules) with its UI - self-contained
```

Guardrails
- DIP: Features depend on their own interfaces, Gameplay orchestrates Features. Features never depend on Systems directly.
- ProjectUI (framework): UI tech only; no flow decisions. UI plugins decide which screens to show.
- Systems vs World: Infrastructure (save/load/settings) -> Systems. Geography/maps -> World.
- Features are self-contained: Each Feature defines own interfaces, Gameplay modes orchestrate them.
- Immutable rule: do not add new plugins to `Alis.uproject`. Boot stays minimal; Orchestrator registers plugin paths at runtime via `IPluginManager` and activates by manifest.
- No global FSM: Use native GameMode/MatchState for match lifecycle. Menu coordinates via ILoadingService.

Dependency Rules (Updated)
```
Foundation/              # No deps on anything below
    |
    v
Systems/                 # Can depend on Foundation
    |
    v
World/ProjectWorld       # Can depend on Foundation, Systems
    |
    v
World/PCG/*              # Can depend on ProjectWorld
    |
    v
World/City17, MainMenuWorld  # Can depend on World/*, Gameplay/*
    |
    v
Gameplay/*               # Can depend on Foundation, Systems, World (NOT Features directly)
    |
    v
Features/*               # Self-contained, depend on Foundation + own interfaces (NOT Gameplay)
```

Settings UI
- `ProjectSettingsUI` stays a separate UI plugin that owns settings widgets and view models.
- Both `ProjectMenuMain` (front-end) and `ProjectMenuGame` (in-game) declare a plugin dependency on `ProjectSettingsUI` and navigate into it when needed.

Current State Snapshot
- Present (matching layout):
  - Boot: Orchestrator (Plugins/Boot/Orchestrator)
  - Foundation: ProjectCore (Plugins/Foundation/)
  - Systems: ProjectLoading, ProjectSave, ProjectSettings (Plugins/Systems/)
  - World: ProjectWorld, City17, MainMenuWorld (Plugins/World/)
  - World/PCG: ProjectPCG, ProjectForestBiomesPack, ProjectUrbanRuinsPCGRecipe (Plugins/World/PCG/)
  - UI: ProjectUI, ProjectMenuMain, ProjectMenuGame, ProjectSettingsUI (Plugins/UI/)
  - Gameplay: ProjectGameplay, ProjectMenuPlay, ProjectSinglePlay, ProjectOnlinePlay (Plugins/Gameplay/)
  - Features: ProjectFeature (contracts), ProjectCombat, ProjectDialogue, ProjectInventory (Plugins/Features/)
- Deprecated/Removed:
  - (done) ProjectExperience - Deleted (redundant with ProjectLoading + GameMode/MatchState)
  - (done) ProjectData - Deleted (manifests moved to domain plugins)
  - (done) Systems/ProjectUI (framework code) -> UI/ProjectUI
  - (done) Systems/ProjectWorld -> World/ProjectWorld
  - (done) Systems/ProjectPCG -> World/PCG/ProjectPCG
  - (done) Features/ProjectUrbanRuinsPCGRecipe -> World/PCG/ProjectUrbanRuinsPCGRecipe
  - (done) Features/ProjectForestBiomesPack -> World/PCG/ProjectForestBiomesPack
  - (done) Gameplay/ProjectFeature -> Features/ProjectFeature

---

## Quick Links to Detailed Docs

| Plugin | Architecture & Details |
|--------|------------------------|
| **ProjectCore** | [Plugins/Foundation/ProjectCore/README.md](../Plugins/Foundation/ProjectCore/README.md) |
| **Orchestrator** | [Plugins/Boot/Orchestrator/README.md](../Plugins/Boot/Orchestrator/README.md) |
| **ProjectLoading** | [Plugins/Systems/ProjectLoading/README.md](../Plugins/Systems/ProjectLoading/README.md) |
| **ProjectMenuMain** | [Plugins/UI/ProjectMenuMain/README.md](../Plugins/UI/ProjectMenuMain/README.md) |
| **ProjectObject** | [Plugins/Resources/ProjectObject/README.md](../Plugins/Resources/ProjectObject/README.md) |

---

## Dependency Rules

```
Core (foundation)
  ^
  |-- Systems/
  |-- UI/
  |-- Data/
  +-- Online/
       ^
     Content/ (declares dependencies in manifests)
```

**Forbidden:**
- Core -> anything else
- UI -> Systems
- Systems -> UI
- Content -> direct code dependencies

---

## MetaHuman and Heavy Content Strategy

MetaHumans and other heavy content use a layered approach that keeps UE workflows intact while enabling granular updates.

### Layered Architecture

```
Layer 1: UE Default Location (don't fight the tools)
  Content/MetaHumans/Common/     -> Shared MH framework assets
  Content/MetaHumans/<CharName>/ -> Per-character MH source assets

Layer 2: Game Logic (plugin-based definitions)
  ProjectObject/Content/Human/<CharName>/
    -> Character DataAssets with soft refs to Layer 1
    -> Gameplay config, abilities, equipment slots
    -> Small footprint (kilobytes, not megabytes)

Layer 3: Update Granularity
  Current:  BuildUnit = download unit (normally a plugin, can be content-pack)
            MetaHumans in /Game/MetaHumans ship with base BuildUnit until packs implemented
  Future:   Per-chunk updates within BuildUnits (IoStore containers)
```

### Terminology

- **BuildUnit** - The download/update unit in the manifest. Usually a plugin, can be a content-pack.
- **Plugin** - A UE plugin that maps 1:1 to a BuildUnit (has `.uplugin` + `BuildUnit.yaml`).
- **Content-pack BuildUnit** - A BuildUnit for non-plugin content (future, for `/Game/MetaHumans` etc.).
- **Pack/Chunk** - A sub-unit inside a BuildUnit (future IoStore split).

### Why This Pattern

1. **Don't fight UE tooling** - MetaHuman Creator expects default paths. Moving Common breaks tools.
2. **BuildUnit = download unit** - BuildUnit.yaml tracks code + content hashes. Most are plugins, some are content-packs.
3. **Soft references** - Character definitions use `TSoftObjectPtr` to MH assets. Download pack -> mount -> resolve.
4. **Future chunking** - IoStore containers can be split (pakchunk10, pakchunk101+) when needed, but BuildUnit-level works now.
5. **CDN-side entitlements** - Manifest filtered server-side based on `requires_claim` (not launcher-side).

### MetaHuman Update Flow

```
1. ProjectObject plugin updates (small, frequent)
   -> New character definitions, balance tweaks
   -> Redownload: ~KB

2. MetaHuman Common updates (rare, large)
   -> Epic releases new MH version
   -> Redownload: ~GB (but infrequent)

3. Per-character MH updates (moderate)
   -> New character added or existing modified
   -> Redownload: ~100MB per character
```

### Best Practices

- **Keep MH in default location** - `Content/MetaHumans/` (don't relocate to fit plugin taxonomy)
- **Definitions in ProjectObject** - `ProjectObject/Content/Human/<CharName>/`
- **Outfits stay with character** - Avoid cross-plugin wardrobe refs (UE 5.7 limitation)
- **OnDemand activation** - Heavy content loads only when needed
- **Current: base BuildUnit** - MetaHumans ship with base project until content-pack BuildUnits implemented
- **Future: per-pack BuildUnits** - Common as one content-pack, each character as another

### Future: Chunk-Level Updates

When needed (not now):
- Assign ChunkIds via PrimaryAssetLabels: `10` = Common, `101+` = per-character
- Manifest extends with `packs[]` alongside `plugins[]`
- Launcher downloads individual chunks, not whole plugins

This keeps the current simple model while preserving the upgrade path.

---

## Quick Reference

| Need to...            | Create in... / Configure via...     |
|-----------------------|-------------------------------------|
| Add interface/type    | `Core/Core/Public/`                 |
| Add gameplay system   | `Systems/[Name]System/`             |
| Add UI screen         | `UI/[Name]UI/`                      |
| Add new map           | `Content/[Project]_[Name]/`         |
| Add editor tool       | `Tools/ValidationTools/`            |
| Add MetaHuman char    | `Content/MetaHumans/<Name>/` + def in `ProjectObject/Content/Human/<Name>/` |

---

See [principles.md](principles.md) for detailed guidance.

---
