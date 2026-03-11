# Asset Manager Registration in Modular Plugin Setup

## Problem

Plugin-local `Config/DefaultGame.ini` with `PrimaryAssetTypesToScan` is NOT reliably auto-merged in modular plugin setups. Asset Manager reads config at engine init (`StartInitialLoading`), before dynamic plugins may be mounted.

**Solution:** Each experience declares its own scan specs + preload groups. Runtime scanning via `ScanPathsForPrimaryAssets()` is the correct pattern for modular/DLC content.

## Naming: What is "Experience"?

**Experience = high-level player context** (map + mode + content set)

In Lyra/UE terms, an Experience is "what kind of game the player is in right now":
- Combination of GameMode, pawn data, input config, abilities
- Selected per level, loaded asynchronously
- Often via GameFeature plugins

Our `UProjectExperienceDescriptorBase` follows this pattern:
- **Gameplay intent**: Which map, which mode, which features
- **Loading profile**: How to get content into memory (scans, packs, preloads)

**Future refactor option** (not now): Extract `FExperienceLoadingProfile` struct to separate gameplay from loading tech. Low priority - current structure works.

## Architecture: Per-Experience Model

### Core Principle

Every "playable experience" (MainMenu, City17, future maps) declares:
1. **Scan specs** - What directories to scan for primary assets
2. **Preload groups** - What assets to load before/after Travel

This is the runtime equivalent of `PrimaryAssetTypesToScan`, but **per experience**, not in root config.

### Data Flow

```
Player clicks "City17" in MainMenu
    |
    v
ILoadingService::BuildLoadRequestForExperience("City17")
    |
    v
InitialExperienceLoader::BuildLoadRequest()
    |
    v
City17ExperienceDescriptor::GetAssetScanSpecs()
    -> Returns: Type="Map", Dirs="/City17/Maps"
    -> Returns: Type="PrimaryAssetLabel", Dirs="/City17/Labels"
    |
    v
EnsureAssetScans() -> ScanPathsForPrimaryAssets() for each spec
    -> City17 map now registered in AssetManager
    |
    v
GetPrimaryAssetIdForPath() -> Resolves MapAssetId
    -> MapAssetId = Map:/City17/Maps/City17_Persistent_WP
    |
    v
ResolveAssets phase -> Validates MapAssetId (now valid!)
    |
    v
PreloadCriticalAssets phase -> Loads CriticalAssetIds (meshes, textures)
    -> Progress bar: 10-70%
    -> NOTE: Does NOT load map here (see Critical Insight below)
    |
    v
Travel phase -> Loads map via ServerTravel
    -> Progress bar: 70-85%
    |
    v
Warmup phase (post-travel) -> Load City17_Warmup assets
    -> Progress bar: 85-100%
    |
    v
Gameplay
```

## Critical Insight: Map Loading

**DO NOT preload map (UWorld) in Phase 3!**

`StreamableManager` cannot properly load `UWorld` assets because:
- World Partition requires special handling
- Level streaming has complex dependencies
- Shader compilation blocks async loading

**Correct approach:**
- Phase 3 loads OTHER critical assets: meshes, textures, materials, data assets
- Map loads in Travel phase via `ServerTravel` (synchronous, ~10s)

This is why `PreloadCriticalAssetsPhaseExecutor.cpp` explicitly skips map preloading:
```cpp
// NOTE: Do NOT preload map (UWorld) here - map loading must happen in Travel phase.
// Phase 3 is for preloading OTHER critical assets: meshes, textures, materials.
```

## API: BuildLoadRequestForExperience()

**New API for loading experiences** (added to fix MenuPlayPlayerController bypass):

```cpp
// ILoadingService.h:116
virtual bool BuildLoadRequestForExperience(
    FName ExperienceName,           // e.g., "City17", "MainMenuWorld"
    FLoadRequest& OutRequest,       // Populated with MapAssetId, scan specs applied
    FText& OutError                 // Error message if build fails
) = 0;
```

**Use this instead of manually building FLoadRequest** to ensure:
1. Descriptor lookup from registry
2. `EnsureAssetScans()` called (runtime asset registration)
3. `MapAssetId` resolved via `GetPrimaryAssetIdForPath()`
4. `CriticalAssetIds`, `WarmupAssetIds` populated from descriptor

**Example usage:**
```cpp
// MenuPlayPlayerController.cpp
TSharedPtr<ILoadingService> LoadingService = FProjectServiceLocator::Resolve<ILoadingService>();

FLoadRequest Request;
FText BuildError;
if (!LoadingService->BuildLoadRequestForExperience(FName("City17"), Request, BuildError))
{
    UE_LOG(LogMenuPlayPC, Error, TEXT("Failed: %s"), *BuildError.ToString());
    return;
}

// Apply custom options
Request.LoadMode = ELoadMode::SinglePlayer;
Request.CustomOptions.Add(TEXT("game"), TEXT("/Script/ProjectSinglePlay.ASinglePlayerGameMode"));

// Start load
TSharedPtr<ILoadingHandle> Handle = LoadingService->StartLoad(Request);
```

## Why This Works for Modular Content

- **DefaultGame.ini only defines generic types** - `"Map"`, `"PrimaryAssetLabel"` for `/Game/Maps`
- **Every modular map plugin brings its own descriptor and labels**
- **Runtime scanning picks up new content** when plugin is mounted
- **No core config changes** when launcher adds new maps

## UE Asset Manager Flow (Engine Code References)

### Config Loading Path (Engine Init)

```
UAssetManager::StartInitialLoading()                    // AssetManager.cpp:3700
  -> ScanPrimaryAssetTypesFromConfig()                  // AssetManager.cpp:3707
       -> GetSettings().PrimaryAssetTypesToScan         // AssetManager.cpp:3395
          (reads from merged DefaultGame.ini at init)
```

### Runtime Scanning (Our Solution)

```
ILoadingService::BuildLoadRequestForExperience()
  -> UProjectLoadingSubsystem::BuildLoadRequestForExperience()
       -> FInitialExperienceLoader::BuildLoadRequest()
            -> EnsureAssetScans(Descriptor)             // InitialExperienceLoader.cpp:80
                 -> Descriptor.GetAssetScanSpecs()      // :195
                 -> AssetManager.ScanPathsForPrimaryAssets() // :229
            -> GetPrimaryAssetIdForPath(MapSoftPath)    // :90
```

### Key Methods

| Method | Location | Purpose |
|--------|----------|---------|
| `BuildLoadRequestForExperience()` | ILoadingService.h:116 | Entry point for experience loading |
| `ScanPathsForPrimaryAssets()` | AssetManager.h:108 | Runtime scanning - our solution |
| `ScanPrimaryAssetTypesFromConfig()` | AssetManager.cpp:3385 | Init-time config reading |
| `GetPrimaryAssetIdForPath()` | AssetManager.h | Resolves soft path to PrimaryAssetId |

## Experience Descriptor Pattern

### Required: GetAssetScanSpecs()

```cpp
// City17ExperienceDescriptor.cpp
void UCity17ExperienceDescriptor::GetAssetScanSpecs(TArray<FExperienceAssetScanSpec>& OutSpecs) const
{
    // Scan for maps (registers map in AssetManager at runtime)
    if (!LoadAssets.Map.ToSoftObjectPath().IsNull())
    {
        FExperienceAssetScanSpec Spec;
        Spec.PrimaryAssetType = TEXT("Map");
        Spec.BaseClass = UWorld::StaticClass();
        Spec.Directories.Add(TEXT("/City17/Maps"));
        Spec.bForceSynchronousScan = true;
        OutSpecs.Add(Spec);
    }

    // Scan for asset labels (Critical, Warmup groups) - FUTURE
    // FExperienceAssetScanSpec LabelSpec;
    // LabelSpec.PrimaryAssetType = TEXT("PrimaryAssetLabel");
    // LabelSpec.Directories.Add(TEXT("/City17/Labels"));
    // OutSpecs.Add(LabelSpec);
}
```

### Preload Groups via Labels (Future)

| Label Asset | Purpose | When Loaded |
|-------------|---------|-------------|
| `City17_Critical` | All meshes, materials, data assets for first frame | PreloadCriticalAssets phase (before Travel) |
| `City17_Warmup` | Heavier but early assets | Warmup phase (after Travel) |
| `City17_Background` | Nice-to-have, stream while playing | Background (during gameplay) |

## Progress Bar Mapping

| Phase | Progress | What Happens |
|-------|----------|--------------|
| ResolveAssets | 0-10% | Scan specs, register assets |
| PreloadCriticalAssets | 10-70% | Async load critical assets (NOT map) |
| Travel | 70-85% | Load map via ServerTravel |
| Warmup (post-travel) | 85-100% | Load warmup assets, final shaders |

## What You Cannot Get

- **No granular "map loading %" from inside Travel** - Game thread is blocked, no stable API
- **MoviePlayer is optional** - Fragile (ray tracing bug), doesn't solve modular registration
- **Accept Travel is opaque** - Keep it short by doing heavy lifting in PreloadCriticalAssets
- **Map preloading via StreamableManager** - Doesn't work for UWorld (use ServerTravel)

## Why No Diagnostics in StartupModule

**TL;DR:** `StartupModule()` runs before engine init, so Asset Manager is never initialized there.

### UE5 Startup Order

```
FEngineLoop::PreInit()
  -> Modules with LoadingPhase::Default load (StartupModule() called)
  -> Asset Manager NOT initialized yet

FEngineLoop::Init()
  -> UAssetManager::StartInitialLoading()
  -> Asset Manager NOW initialized
```

### Consequence

Any Asset Manager diagnostic in `ProjectLoadingModule::StartupModule()` would:
1. Always find `UAssetManager::IsInitialized() == false`
2. Log non-actionable "Asset Manager not initialized" warning
3. Train developers to ignore warnings (bad)

### Where Diagnostics Belong

| Environment | Registration | Diagnostic Location |
|-------------|--------------|---------------------|
| Editor | Config merge (reliable) | None needed |
| Cooked build | `EnsureAssetScans()` | After `ScanPathsForPrimaryAssets()` |

Diagnostics are in `EnsureAssetScans()` (InitialExperienceLoader.cpp) where:
- Asset Manager is guaranteed initialized (GameInstance subsystem context)
- Runtime scan has completed
- Warnings are actionable ("0 assets found" means descriptor is misconfigured)

### Historical Note

An Asset Manager diagnostic existed in `StartupModule()` during early development to debug config merge issues. It was removed because:
1. It always fired (Asset Manager not yet initialized)
2. The solution (runtime scanning) made it obsolete
3. Proper diagnostics now exist in `EnsureAssetScans()`

## Diagnostic Logs

### BuildLoadRequestForExperience (ProjectLoadingSubsystem.cpp:466)
```
LogProjectLoading: BuildLoadRequestForExperience: Building request for 'City17'
LogProjectLoading: BuildLoadRequestForExperience: Success - MapAssetId=Map:/City17/Maps/City17_Persistent_WP
```

### EnsureAssetScans (InitialExperienceLoader.cpp:178)
```
LogProjectLoading: EnsureAssetScans: Called for 'City17'
LogProjectLoading: EnsureAssetScans: Scanning Type='Map', BaseClass='World'
LogProjectLoading:   Directory: /City17/Maps
LogProjectLoading: EnsureAssetScans: ScanPathsForPrimaryAssets found 1 assets of type 'Map'
```

### Phase 3 PreloadCriticalAssets
```
LogProjectLoading: Phase 3: Input - MapAssetId=Map:/City17/Maps/City17_Persistent_WP (Valid=YES)
LogProjectLoading: Phase 3: MapAssetId is valid - will load during Travel phase
LogProjectLoading: Phase 3: Preload Critical Assets - NO ASSETS TO PRELOAD (skipping)
```

## Related Files

| File | Purpose |
|------|---------|
| `ProjectLoadingModule.cpp` | Module startup (no diagnostics - see "Why No Diagnostics in StartupModule") |
| `ILoadingService.h` | `BuildLoadRequestForExperience()` interface |
| `ProjectLoadingSubsystem.cpp` | Implementation + `FLoadingServiceAdapter` |
| `InitialExperienceLoader.cpp` | `EnsureAssetScans()` - runtime scanning + diagnostics |
| `PreloadCriticalAssetsPhaseExecutor.cpp` | Phase 3 - loads CriticalAssetIds (NOT map) |
| `ProjectExperienceDescriptorBase.h` | `FExperienceAssetScanSpec` struct |
| `City17ExperienceDescriptor.cpp` | City17 experience implementation |
| `MenuPlayPlayerController.cpp` | Uses `BuildLoadRequestForExperience()` |
