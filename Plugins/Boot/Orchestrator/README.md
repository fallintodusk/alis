# Orchestrator (Plugin Lifecycle Manager)

Early boot plugin that manages code and module lifecycle, then hands runtime content loading off to `ProjectLoading`.

## Purpose
Base plugin (PostConfigInit phase) that manages **CODE/MODULE LIFECYCLE ONLY**. Loads plugin code and modules during early boot. Content loading (IoStore, maps, UI) is deferred to **ProjectLoading** via `ILoadingService` using the manifest entry point (e.g., MainMenuWorld) after engine init.

## Implementation Status

The boundary above is the target architecture, but delivery migration is not
complete:

- `OrchestratorAPI.h` remains a public compatibility surface. Its BootROM-era
  comments do not define current ownership, and it must remain available until
  release/downstream evidence supports deprecation or removal at a documented
  breaking-version boundary.
- `FOrchestratorCoreModule::ApplyHotUpdates` and the cold-update path still
  contain legacy download and extraction behavior.
- `FOrchestratorIoStore` is an unused placeholder that does not mount content.
- ProjectLoading owns the mount phase, but explicit content-pack requests fail
  closed until real IoStore mounting is implemented.

Do not treat content hot update as production-ready. Gate new delivery work on
one signed manifest/trust policy, migrate download and installation to
Launcher, keep Orchestrator focused on code/module lifecycle, and implement
mounting in ProjectLoading before removing the legacy paths.

## Critical Architectural Split

**Orchestrator = Code Lifecycle (PostConfigInit, <1 sec)**
- Loads plugin DLLs and modules
- Runs during early boot (before Slate, before GEngine)
- NO content loading, NO UI during this phase
- Duration: <1 second (code is fast)
- Module boundary: no dependency on ProjectLoading; boot tier ends after modules are loaded.

**ProjectLoading = Content Loading + UI (PostEngineInit+, 5-30 sec)**
- Loads maps, assets, textures, audio, GameFeatures
- Mounts IoStore (.utoc/.ucas) and shows loading screens (Slate/UMG)
- Runs after engine init (when Slate is ready)
- Uses manifest.entryPoint passed from Orchestrator to start the pipeline (e.g., MainMenuWorld)

## Responsibilities (CODE ONLY)
1. IPC context reading (Launcher IPC payload: auth token, manifest path, install path, engine build ID)
2. Manifest management (parse/validate JSON, verify engine_build_id)
3. State management (`latest.json`, `last_good.json`, `pending_updates.json`)
4. Pending updates application (code changes staged from previous session)
5. Dependency resolution (version constraints, topo sort, cycle detection)
6. Plugin registration (`IPluginManager::AddToPluginsList`, `MountExplicitlyLoadedPlugin`)
7. Module loading (`FModuleManager::LoadModule` for plugin modules in dependency order)
8. Rollback support (revert to `last_good.json` on failure)

## Non-Responsibilities (Launcher Does This)
- [X] **NO manifest downloads** -> Launcher downloads and verifies manifest before starting game
- [X] **NO plugin downloads** -> Launcher downloads all payloads (code.zip, content.utoc/ucas)
- [X] **NO hash verification** -> Launcher verifies SHA-256 hashes before extraction
- [X] **NO signature validation** -> Launcher validates manifest signature and Authenticode DLLs
- [X] **NO payload extraction** -> Launcher extracts all plugins to install path before starting game

## Design Philosophy
**Launcher = BIOS + Bootloader**, **Orchestrator = Init System**, **Feature Plugins = Services**

Orchestrator is analogous to an init system (systemd, launchd) that:
- Starts services (feature plugins) in dependency order
- Manages service state and lifecycle
- Maintains rollback state for recovery (last_good.json)
- All downloads/verification happen BEFORE game starts (Launcher's job)

## Boot vs Runtime Behavior

**Boot Mode (PostConfigInit, <1 sec) - CODE ONLY**
- Parse manifest (already downloaded by Launcher)
- Load state files (`latest.json`, `last_good.json`)
- Register plugins with `IPluginManager` (metadata only)
- Load modules via `FModuleManager` (DLLs only)
- Call `IProjectFeatureModule::Start()` for Boot plugins
- NO content loading, NO downloads, NO IoStore mounting
- NO calls into ProjectLoading; runtime content is handled later via `ILoadingService`.

**Entry Point Handoff**
- manifest.entryPoint (e.g., MainMenuWorld) is stored during boot
- GameInstance/ProjectLoading reads manifest/state after engine init and starts loading via `ILoadingService` (data handoff only; Orchestrator has no dependency on ProjectLoading)
- ProjectLoading mounts IoStore, loads assets, and shows UI

**Runtime Update Mode (future)**
- Apply pending updates from previous session
- Consume plugin versions already downloaded, verified, and installed by Launcher
- Load OnDemand modules (code only)
- Content mounting remains in ProjectLoading

## Architecture Components (Boot Mode Only)
- **Manifest Parser** - Parses JSON manifest (pre-downloaded by Launcher)
- **State Manager** - Reads/writes state files, supports rollback
- **Plugin Registry** - Tracks plugin state (IsLoaded/Unloaded/Mounting), reverse dependency map
- **Module Loader** - Loads/unloads DLLs via `FModuleManager`
- **Dependency Resolver** - Dependency ordering and cycle detection (DLLs only; no IoStore)

## Update Decision Logic (single rule)

```
if code_hash changed:
    require Launcher-installed payload, then register, load, and activate
else if content_hash changed:
    require Launcher-installed payload, then request ProjectLoading mount
else:
    up-to-date (no action)
```

## Plugin Activation Strategies

**Boot-Time Activation (Essential Plugins)**
- Manifest: `"activationStrategy": "Boot"`
- Loaded immediately at startup by Orchestrator (DLLs only)
- Boot plugins: ProjectCore, ProjectLoading, ProjectSave, ProjectSettings, ProjectWorld, ProjectPCG, ProjectGameplay, ProjectUI, ProjectMenuMain, ProjectSettingsUI, ProjectMenuPlay, MainMenuWorld (content mounted later by ProjectLoading)
 - No ProjectLoading dependency at boot; handoff is via manifest/state files. Later, GameInstance uses `ILoadingService` to load the entry point.

**On-Demand Activation (Gameplay Features)**
- Manifest: `"activationStrategy": "OnDemand"`
- DLLs loaded at boot (code only); content is mounted on demand via ProjectLoading and `ILoadingService`
- Examples: ProjectCombat, ProjectDialogue, ProjectInventory, ProjectMenuGame, ProjectSinglePlay, ProjectOnlinePlay, City17, content packs

**Lifecycle**
1. Discover - Orchestrator registers all plugins at boot (metadata only)
2. Trigger - Game/UI requests activation (e.g., experience descriptor uses `ILoadingService`)
3. Load Module - Module was already loaded at boot (code available)
4. Mount Content - ProjectLoading mounts IoStore/assets via `ILoadingService`
5. Activate - `IProjectFeatureModule::Start()` registers gameplay hooks when content is ready
6. Deactivate (optional) - Unload on map change or memory pressure (code stays loaded)

**Note:** There is no IOrchestrator interface registered. Runtime callers use `ILoadingService` (ProjectLoading) to mount content and pull modules as needed.

**Validation boundary:** Launcher is the trust root for manifest signatures,
payload hashes, Authenticode, installation, and release selection. Orchestrator
validates schema, `engine_build_id`, dependency consistency, and the presence
of the Launcher-selected installed code. It never establishes a second trust
or release-selection path.

## Loading Phase
- Loaded by Unreal's module manager at PostConfigInit after Launcher starts the game
- Entry point: `FOrchestratorCoreModule::StartupModule()` reads Launcher context
- Runs before engine plugin activation phases (DLLs only; content deferred to ProjectLoading)

## Boot UI Strategy
- No Slate/CommonUI at boot (PostConfigInit has no Slate)
- Boot is <1 second; engine splash is sufficient
- ProjectLoading shows loading UI after engine init

## Key Interactions
- `IPluginManager::AddToPluginsList()` - Register plugins (metadata)
- `IPluginManager::MountExplicitlyLoadedPlugin()` - Make plugins discoverable
- `FModuleManager::LoadModule()` - Load plugin modules (DLLs only)
- `UGameFeaturesSubsystem::LoadGameFeaturePlugin()` - Activate features (ProjectLoading-managed)

## External Plugin Roots

- `Plugins/ThirdParty/` is for shared tracked external plugins.
- `Plugins/Local/` is for ignored machine-local editor tooling.
- Editor-only external plugin loading may scan both roots.
- Runtime/package flows must not depend on `Plugins/Local/`.

## Validation
```
scripts/ue/build/build.bat AlisEditor Win64 Development
scripts/ue/test/integration/autonomous_boot_test.bat
scripts/ue/test/smoke/boot_test.bat
```

## State Files Location
- Windows: `<local-app-data>/Project/State/`
- Linux: `~/.local/share/Project/State/`
- Mac: `~/Library/Application Support/Project/State/`

## Manifest Location
- Windows: `<local-app-data>/Project/Manifests/`
- Linux: `~/.local/share/Project/Manifests/`
- Mac: `~/Library/Application Support/Project/Manifests/`

## Logs Location
- Windows: `<local-app-data>/Project/Logs/orchestrator.log`
- Linux: `~/.local/share/Project/Logs/orchestrator.log`
- Mac: `~/Library/Application Support/Project/Logs/orchestrator.log`

## Architecture Documentation
- C4 Component View: `../../docs/architecture/diagrams/views/component_orchestrator.dsl`
- C4 Container View: `../../docs/architecture/diagrams/views/container_plugins.dsl`
- Manifest schema: maintained in access-controlled repository `cdn` and intentionally not mirrored in this public source tree
- Dynamic views: `dynamic_update_content.dsl`, `dynamic_update_code.dsl`, `dynamic_rollback.dsl`

## Launcher-Driven Orchestrator Update

1. Launcher reads the signed manifest and selects the Orchestrator version.
2. Launcher downloads, verifies, and stages the selected DLL.
3. If `current_orchestrator_version.txt` remains part of the handoff, Launcher
   is its sole writer.
4. Launcher starts the game against the selected installed release.
5. Unreal's module manager loads Orchestrator at PostConfigInit.
6. Orchestrator validates compatibility and activates installed modules; it
   does not download, select, or update plugins.

The selected-version handoff is target architecture, not a currently proven
self-update path. Legacy Orchestrator update code remains migration debt.

## Rollback Behavior

- Launcher owns the authoritative selected-release and last-known-good chain.
- Orchestrator may keep derived activation checkpoints and report failures, but
  it must not independently select a competing release.
- After an activation failure, the next launch uses the Launcher-selected
  fallback release.

This behavior remains unproven until authoritative fallback and activation
failure recovery are implemented and tested end to end.

## Module Structure

```
Orchestrator/
    Source/
        OrchestratorCore/          # Main module
            Private/
                OrchestratorCoreModule.cpp      # Module entry point
                OrchestratorManifest.cpp        # Manifest parser
                OrchestratorState.cpp           # State manager
                OrchestratorDownload.cpp        # Legacy migration debt; Launcher target
                OrchestratorHash.cpp            # Legacy delivery helper; Launcher target
                OrchestratorSignature.cpp       # Legacy delivery helper; Launcher target
                OrchestratorIoStore.cpp         # Unused placeholder; ProjectLoading target
                OrchestratorPluginRegistry.cpp  # Plugin registry
                OrchestratorRegistry.cpp        # Dependency resolver (DLL ordering)
            Public/
                OrchestratorAPI.h             # Legacy public compatibility surface; removal requires a versioned decision
                OrchestratorCoreModule.h
        OrchestratorTests/         # Test suite
    README.md                      # This file
```

## Dependencies
- **ProjectCore** - For interfaces only (IOrchestratorRegistry, IProjectFeatureModule, IFeatureModuleRegistry)
- UE modules: Core, CoreUObject, Engine, Projects (IPluginManager), ModuleManager, GameFeatures, HTTP, Json

**Note:** ProjectCore is a lightweight abstraction layer with no gameplay logic. This dependency is for SOLID compliance (depend on abstractions, not implementations).

## Security Considerations
- Launcher: manifest signatures, SHA-256 payload hashes, Authenticode,
  installation, and selected-release/last-known-good ownership.
- Orchestrator: schema, `engine_build_id`, dependency graph, installed-payload
  presence, and atomic writes for derived activation state.
- ProjectLoading: mount only already installed content authorized by the
  Launcher-selected manifest.

## DIP Compliance (SOLID Architecture)

`IProjectFeatureModule` interface lives in **ProjectCore** (not OrchestratorCore). This is **DIP-compliant** because:

1. **Features depend on interface abstraction**, not Orchestrator implementation details
2. **Both Features and Orchestrator depend on ProjectCore** (stable abstraction layer)
3. **No circular dependencies**: Features -> ProjectCore <- OrchestratorCore

```
ProjectCore (Abstraction Layer)
+-- Interfaces/IProjectFeatureModule.h  <- Pure interface
+-- Interfaces/IFeatureModuleRegistry.h <- Registration API
+-- FBootContext                        <- Context struct (data only)
+-- FeatureModuleRegistry.cpp           <- Stores interface pointers

OrchestratorCore (Boot/Loading Layer)
+-- FOrchestratorCoreModule    <- Queries registry, calls interface methods

Features (Feature Layer)
+-- Implements IProjectFeatureModule
+-- Registers via RegisterFeatureModule() global function
```

**Why in ProjectCore?**
- Breaks circular dependency between OrchestratorCore and Features
- ProjectCore is the central abstraction layer (also hosts ILoadingService, IOrchestratorRegistry)
- Both high-level (Features) and low-level (Orchestrator) modules depend on abstractions

## Performance Characteristics
- Plugin registration: ~10ms per plugin
- Module loading: ~100ms per module
- Typical full boot: 2-5 seconds for all boot plugins (code only)
- Content mounting time is owned by ProjectLoading (5-30 seconds depending on assets)

## Telemetry
OpenTelemetry traces for:
- Launcher handoff and manifest parsing
- Schema and `engine_build_id` validation
- Installed-payload presence checks
- Dependency resolution
- Plugin activation
- Activation success/failure and derived checkpoint state

Manifest fetch, payload download, hash/signature verification, installation,
release selection, and authoritative rollback telemetry belong to Launcher.
