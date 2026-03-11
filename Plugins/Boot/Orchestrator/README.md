# Orchestrator (Plugin Lifecycle Manager)

## Purpose
Base plugin (PostConfigInit phase) that manages **CODE/MODULE LIFECYCLE ONLY**. Loads plugin code and modules during early boot. Content loading (IoStore, maps, UI) is deferred to **ProjectLoading** via `ILoadingService` using the manifest entry point (e.g., MainMenuWorld) after engine init.

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
- Download/verify new plugin versions if needed
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
    install-before-load (download, verify, stage, register, load, activate) - no restart
else if content_hash changed:
    content update (IoStore mount handled by ProjectLoading) - no restart
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

**Validation boundary:** The Launcher is the source of truth for manifest/hash/signature validation before boot. Orchestrator trusts the verified manifest/state and does not re-validate DLL signatures at runtime (unless explicitly enabled for secure boot builds).

## Loading Phase
- Loaded by BootROM at PostConfigInit
- Entry point: `IOrchestratorModule::Start(FBootContext)`
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

## Validation
```
scripts/ue/build/build.bat ProjectEditor Win64 Development -Module=OrchestratorCore
scripts/ue/test/unit/orchestrator_tests.bat
scripts/ue/test/integration/boot_integration_test.bat
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
- Manifest schema: `~/repos_alis/cdn/docs/manifest.schema.json` (`activation_strategy` field)
- Dynamic views: `dynamic_update_content.dsl`, `dynamic_update_code.dsl`, `dynamic_rollback.dsl`

## Self-Update Process
1. Detect new Orchestrator version in manifest
2. Download and verify new Orchestrator DLL
3. Stage to `<local-app-data>/Project/Orchestrator/{version}/`
4. Write new version to `current_orchestrator_version.txt`
5. Next boot, BootROM loads new Orchestrator version
6. New Orchestrator takes over and can update plugins

## Rollback Behavior
1. State Manager reverts to `last_good.json` on failure
2. Dependency Resolver loads last known good plugin versions
3. Plugin Registry registers last good plugins
4. Orchestrator reports rollback to client
5. Event Bus receives `update.failed` event
6. Application boots successfully to last known good state

**Guarantee:** System always boots to last known good state, even if updates fail.

## Module Structure

```
Orchestrator/
    Source/
        OrchestratorCore/          # Main module
            Private/
                OrchestratorCoreModule.cpp      # Module entry point
                OrchestratorManifest.cpp        # Manifest parser
                OrchestratorState.cpp           # State manager
                OrchestratorDownload.cpp        # Download manager
                OrchestratorHash.cpp            # Hash verifier
                OrchestratorSignature.cpp       # Signature validator
                OrchestratorPluginRegistry.cpp  # Plugin registry
                OrchestratorRegistry.cpp        # Dependency resolver (DLL ordering)
            Public/
                OrchestratorCoreModule.h
        OrchestratorTests/         # Test suite
    README.md                      # This file
```

## Dependencies
- **ProjectCore** - For interfaces only (IOrchestratorRegistry, IProjectFeatureModule, IFeatureModuleRegistry)
- UE modules: Core, CoreUObject, Engine, Projects (IPluginManager), ModuleManager, GameFeatures, HTTP, Json

**Note:** ProjectCore is a lightweight abstraction layer with no gameplay logic. This dependency is for SOLID compliance (depend on abstractions, not implementations).

## Security Considerations
- Manifest signature verification (allowlisted signing keys)
- Authenticode verification for DLLs (Windows)
- SHA-256 hashes for all plugins (code and content tracked separately)
- Atomic state file writes (`.tmp` then rename)

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
- Manifest fetch and parsing
- Plugin downloads
- Hash and signature verification
- Dependency resolution
- Plugin activation
- Update success/failure and rollback
