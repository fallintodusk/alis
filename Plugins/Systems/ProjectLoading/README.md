# ProjectLoading (Content Loading System)

## Purpose
GameInstance subsystem that handles **CONTENT LOADING + UI** after engine initialization.
Shows loading screens and orchestrates 6-phase loading pipeline for maps, assets, and features.
Module boundary: lives in Systems tier, depends on ProjectCore for interfaces and descriptors; Orchestrator does not depend on ProjectLoading.

## Critical Architectural Split

**Orchestrator = Code Lifecycle (PostConfigInit, <1 sec)**
- Loads plugin DLLs and modules during boot
- NO content, NO UI (runs before Slate exists)
- **See:** `Plugins/Boot/Orchestrator/README.md`

**ProjectLoading = Content Loading + UI (PostEngineInit+, 5-30 sec)**
- Loads maps, assets, textures, audio, GameFeatures
- Shows loading screens (Slate/UMG)
- Runs AFTER Orchestrator boot completes
- Plugins describe WHAT to load via experience descriptors (in ProjectCore)
- ProjectLoading provides HOW to load (pipeline + UI)
- Manifest entry point (e.g., MainMenuWorld) is read from manifest/state after engine init (data only) and loaded via `ILoadingService`

## What It Ships

- `UProjectLoadingSubsystem` - Implements `ILoadingService` interface
- `FProjectLoadingHandle` - Async handle for progress tracking
- 6-phase loading pipeline (ResolveAssets, MountContent, PreloadCriticalAssets, ActivateFeatures, Travel, Warmup)
- Loading screen UI framework (Slate/UMG)
- Blueprint-friendly delegates (OnLoadStarted, OnPhaseChanged, OnProgress, OnCompleted, OnFailed)
- Telemetry for phase duration tracking
- Cancellation support

## How Plugins Use ProjectLoading

**Pattern:** Plugins describe WHAT to load (experience descriptors in ProjectCore), ProjectLoading handles HOW (pipeline + UI).

**Steps:**
1. Plugin creates descriptor (C++ class with constructor defaults, CDO pattern) in ProjectCore.
2. Plugin registers descriptor via `ProjectExperienceRegistration.h` (ProjectCore) in `StartupModule()`.
3. ProjectLoading fetches descriptors from the registry, resolves AssetManager paths, and builds `FLoadRequest`.
4. Game code calls `ILoadingService::StartLoad()` via service locator (no IOrchestrator interface involved).

**Full implementation:** See `../../todo/current/loading.md` for complete pattern with code examples.

## 6-Phase Loading Pipeline
 
ProjectLoading executes content loading in 6 sequential phases. 
 
### Flow Visualization
 
### Flow Visualization
 
```
[StartLoad]
    |
    v
1. ResolveAssets (Validate dependencies, convert IDs)
    |
    v
2. MountContent (Mount .utoc/.ucas containers)
    |
    v
3. PreloadCriticalAssets (Load data/UI to prevent hitches)
    |
    v
4. ActivateFeatures (Enable GameFeature plugins)
    |
    v
5. Travel (ServerTravel / Map Load)
    |
    v
6. Warmup (Compile Shaders, GC, Cleanup)
    |
    v
[Complete]
```
 
### Phase Details
 
| Phase | Purpose | Implementation details | Skip Condition |
|-------|---------|------------------------|----------------|
| **1. ResolveAssets** | Translate `PrimaryAssetId` to soft object paths. Validates dependencies. | [ResolveAssetsPhaseExecutor.cpp](Source/ProjectLoading/Private/Executors/ResolveAssetsPhaseExecutor.cpp) | Never |
| **2. MountContent** | Dynamic mounting of IoStore containers (.utoc/.ucas). *See Boot Flow Context below.* | [MountContentPhaseExecutor.cpp](Source/ProjectLoading/Private/Executors/MountContentPhaseExecutor.cpp) | Always (stub) |
| **3. PreloadCriticalAssets** | Async load of critical UI/Data tables to prevent hitches during travel. | [PreloadCriticalAssetsPhaseExecutor.cpp](Source/ProjectLoading/Private/Executors/PreloadCriticalAssetsPhaseExecutor.cpp) | No assets |
| **4. ActivateFeatures** | Toggle GameFeature modules (Already loaded by Orchestrator, but activated here). | [ActivateFeaturesPhaseExecutor.cpp](Source/ProjectLoading/Private/Executors/ActivateFeaturesPhaseExecutor.cpp) | No features |
| **5. Travel** | Executes `UEngine::Browse` / `ServerTravel`. | [TravelPhaseExecutor.cpp](Source/ProjectLoading/Private/Executors/TravelPhaseExecutor.cpp) | Never |
| **6. Warmup** | Precompiles shaders (PSO), forces GC, and cleans up handles. | [WarmupPhaseExecutor.cpp](Source/ProjectLoading/Private/Executors/WarmupPhaseExecutor.cpp) | Never |
 
### Boot Flow Context (Phase 2)
 
**Where does content come from?**
 
1.  **Startup (Orchestrator)**: Loads plugin DLLs and code. *Phase 2 does nothing here.*
2.  **Initial Load (ProjectLoading)**: Runs after engine init. Reads manifests.
3.  **Runtime (ProjectLoading)**: Phase 2 is reserved for **Download-on-Demand** (DLC/Patching) where new `.utoc` files must be mounted before assets can be resolved.
 
 Currently, Phase 2 is a stub because the Orchestrator handles all initial mounting.
 
 **Phase executors:** `Source/ProjectLoading/Private/Executors/` - One executor per phase

## Loading Screen UI

ProjectLoading provides infrastructure for loading screens (Slate/UMG):
- Shows/hides loading screen automatically
- Updates progress bar (0.0 to 1.0)
- Displays phase names and status messages
- Handles cancellation UI (optional)

**Implementation:** Bind UMG widget to `UProjectLoadingSubsystem` delegates (`OnPhaseChanged`, `OnProgress`).

**Full example:** See `../../todo/current/loading.md` for complete loading widget implementation.

## Dependencies

- Requires `ProjectCore` for interfaces (`ILoadingService`, `ILoadingHandle`, `FLoadRequest`, `FLoadPhaseState`) and experience descriptors/registry (data only)

## Known Issues

### MoviePlayer + Ray Tracing (UE 5.5)

**Status:** MoviePlayer subsystem DISABLED

MoviePlayer causes `FRayTracingGeometryManager::Tick()` assertion crash when Ray Tracing is active.
The engine's MoviePlayer triggers an extra render tick that violates RT's single-tick-per-frame requirement.

**Workaround:** `MOVIEPLAYER_SUBSYSTEM_ENABLED 0` in `ProjectLoadingMoviePlayerSubsystem.cpp`

**Impact:** No animated loading screen during map travel freeze. Phase 3 (PreloadCriticalAssets) reduces freeze time by preloading assets before travel.

### Phase 3 Thread Affinity

Phase 3 (PreloadCriticalAssets) requires game thread because `StreamableManager::RequestAsyncLoad()` asserts `IsInGameThread()`.

**Solution:** `RequiresGameThread()` returns `true` in `PreloadCriticalAssetsPhaseExecutor.h`. Pipeline scheduler dispatches to game thread automatically via `PhaseRetryStrategy::ExecuteOnGameThreadIfNeeded()`.

## Preload Handle Lifecycle (Phase 3 + 6)

**Problem solved:** Without managed handles, preloaded assets become GC-able before Travel.

**Flow:**
1. Phase 3: `bManageActiveHandle=true`, store in `Context.Runtime.PreloadHandles`
2. Phase 5: Travel - map loads, WP streaming starts
3. Phase 6: Release handles - WP-referenced assets stay, "wrong picks" unload
4. **Safety net:** `ON_SCOPE_EXIT` in orchestrator releases orphaned handles on abnormal exit

**Source:**
- Runtime struct: [ProjectLoadPhaseExecutor.h:62-87](../../Plugins/Systems/ProjectLoading/Source/ProjectLoading/Private/ProjectLoadPhaseExecutor.h#L62)
- Safety net: [LoadPipelineOrchestrator.cpp:54-65](../../Plugins/Systems/ProjectLoading/Source/ProjectLoading/Private/Pipeline/LoadPipelineOrchestrator.cpp#L54)

## Error Codes (Quick Reference)

| Range | Phase | Example |
|-------|-------|---------|
| 100-199 | ResolveAssets | 101 = ManifestNotFound |
| 200-299 | MountContent | 201 = DownloadFailed |
| 300-399 | PreloadCriticalAssets | 301 = AssetLoadFailed |
| 400-499 | ActivateFeatures | 401 = FeatureNotFound |
| 500-599 | Travel | 501 = MapNotFound |
| 600-699 | Warmup | 601 = ShaderCompileFailed |
| 800-899 | System | 801 = Cancelled, 802 = Timeout |

**Full list:** [ProjectLoadPhaseExecutor.h](../../Plugins/Systems/ProjectLoading/Source/ProjectLoading/Private/ProjectLoadPhaseExecutor.h)

## Blueprint Events

| Event | Purpose |
|-------|---------|
| `OnLoadStarted` | Pipeline began |
| `OnPhaseChanged` | Phase transition |
| `OnProgress` | Progress update (0.0-1.0) |
| `OnCompleted` | Success with telemetry |
| `OnFailed` | Failure with error info |

**Source:** [ProjectLoadingSubsystem.h](../../Plugins/Systems/ProjectLoading/Source/ProjectLoading/Public/ProjectLoadingSubsystem.h)

## Architecture Documentation

**Phase executors:** See `Source/ProjectLoading/Private/Executors/` for phase implementation details
