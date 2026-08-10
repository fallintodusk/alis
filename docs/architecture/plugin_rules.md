# Plugin Architecture

> See also: [docs/agents/canonical.md](../agents/canonical.md) for agent/dev quick reference.

> **Source of Truth:** [source_of_truth.md](source_of_truth.md) + C4 DSL (`diagrams/workspace.dsl` and `diagrams/views/`). Use these as the map; implementation lives with each plugin.

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

    ProjectWorldData/  # Approved target; creation is an open execution task
      -> Data/content-only Kazan JSON, manifests, generated maps, and authored overlays
      -> Depends on ProjectWorld contracts; no Source module or custom generator logic

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
- World logic vs data: ProjectWorld owns reusable world schemas, definition
  types, generators, realization, serialization/replication support, runtime
  services, and validation. ProjectWorldData owns authoritative Kazan JSON and
  its derived Unreal content under its own mount; it is data/content-only and
  does not fork world-generation logic.
- Features are self-contained: Each Feature defines own interfaces, Gameplay modes orchestrate them.
- Immutable rule: do not add new plugins to `Alis.uproject`. Boot stays minimal; Orchestrator registers plugin paths at runtime via `IPluginManager` and activates by manifest.
- No global FSM: Use native GameMode/MatchState for match lifecycle. Menu coordinates via ILoadingService.

Composition Ownership vs Consumption
- Composition ownership (`CreateDefaultSubobject<T>` in a constructor) requires a concrete type at compile time and therefore a hard module dependency on the owning plugin. This is distinct from component consumption.
- Consumers that only read state or bind events MUST go through an interface in `ProjectCore` (e.g. `IVitalsEventsSource` for vitals death/damage events), discovered via `UClass::ImplementsInterface(...)`, not via the concrete class.
- Example: `ProjectCharacter` constructs `UProjectVitalsComponent` (composition -> hard dep on `ProjectVitals`), while `ProjectSinglePlay` binds death handlers through `IVitalsEventsSource` (consumption -> no dep on `ProjectVitals`).
- Anti-pattern: adding `ProjectVitals` (or any provider plugin) to a consumer module's Build.cs just to reach its delegates or enums. Hoist the shared types/interfaces to `ProjectCore` instead.

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
    +--> World/ProjectWorldData  # Data/content-only; consumes ProjectWorld contracts
    |
    +--> World/PCG/*             # Can depend on ProjectWorld
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
  - World: ProjectWorld, ProjectWorldData (approved target), City17, MainMenuWorld (Plugins/World/)
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
| **ProjectCore** | [Plugins/Foundation/ProjectCore/README.md](../../Plugins/Foundation/ProjectCore/README.md) |
| **Orchestrator** | [Plugins/Boot/Orchestrator/README.md](../../Plugins/Boot/Orchestrator/README.md) |
| **ProjectLoading** | [Plugins/Systems/ProjectLoading/README.md](../../Plugins/Systems/ProjectLoading/README.md) |
| **ProjectMenuMain** | [Plugins/UI/ProjectMenuMain/README.md](../../Plugins/UI/ProjectMenuMain/README.md) |
| **ProjectObject** | [Plugins/Resources/ProjectObject/README.md](../../Plugins/Resources/ProjectObject/README.md) |

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

## UI plugin dependency pattern (clarified 2026-04-22)

**Rule**: a UI plugin for feature X MAY depend on feature X for its public domain types (enums, tags, read-only data structs). It MUST NOT reach into X's internal implementation (private components, non-public helpers, global state).

Cross-plugin services + events + commands must route through interfaces in `ProjectCore` (`IVitalsReadOnly`, `IInventoryReadOnly`, `IVitalsEventsSource`, `IInventoryDropCommandTarget`, etc.). Consumers that are NOT "the UI of X" (other gameplay plugins, gamemode, ai, etc.) must never declare a direct Build.cs dep on X.

### Pattern comparison

| Scenario | UI depends on Feature? | Consume via Core? |
|---|---|---|
| `ProjectInventoryUI` -> `ProjectInventory` | NO (interfaces cover 100% of UI needs) | YES (IInventoryReadOnly / IInventoryCommands / IInventoryDropCommandTarget) |
| `ProjectVitalsUI` -> `ProjectVitals` | YES (for `EVitalState` / `EFatigueState` domain enums shared by feature + UI hysteresis state) | YES for events + config (IVitalsEventsSource, FVitalsConfig) |
| `ProjectSinglePlay` -> `ProjectVitals` | N/A (not a UI plugin) | YES ONLY (IVitalsEventsSource in Core; no direct dep) |
| `ProjectCharacter` -> `ProjectVitals` | N/A (not a UI plugin) | Composition owner - `CreateDefaultSubobject<UProjectVitalsComponent>` requires concrete type; DIP cannot abstract component construction. Keep the dep, document it. |

### Why the split

Enums and primitive value types that are part of the feature's PUBLIC CONTRACT (not its internal implementation) legitimately belong with the owning feature plugin. Hoisting them to Core pollutes Core with domain types ("vitals" is not foundation concern). The cost - a Build.cs dep from the UI plugin onto its own feature plugin - is architecturally honest: UI-for-X is naturally tied to X.

Forcing the UI-zero-dep pattern universally leads to type-placement gymnastics (enum hoisting) and the UE serialization trap it implies (CoreRedirects that can't be merged to main per `docs/editor/class_migration.md`). ProjectInventoryUI is zero-dep because its interfaces are rich enough to denormalize every UI-relevant field to a primitive - not because the rule is universal.

### Decision rule for new UI plugins

1. If the feature exposes a complete-enough interface (read-only queries return primitives only), keep the UI plugin zero-dep on its feature.
2. If the UI needs domain types (enums, flags, shared state) that would require artificial denormalization to flatten, accept the feature dep. Document why in `Plugins/UI/<Name>/README.md`.
3. Events + commands always route through ProjectCore interfaces (never via direct component reference from consumers outside the feature).

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
