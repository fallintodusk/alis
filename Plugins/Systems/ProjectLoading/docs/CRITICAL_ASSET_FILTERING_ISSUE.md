# Critical Asset Filtering Issue - Root Cause Analysis

**Status:** BROKEN - Reference counting rejects ALL World Partition assets
**Impact:** 9.1s freeze during City17 load, 794 assets stuck in async loading
**Date:** 2025-12-11

---

## The Problem

### Current Behavior
```
DiscoverMapDependencies: Found 2745 direct dependencies
DiscoverMapDependencies: Discovered 794 heavy assets (before filtering)
DiscoverMapDependencies: Filtered to top 0 shared assets (cap=20, total candidates=0)
InitialExperienceLoader: Auto-discovered 0 assets via CriticalSoftPaths

Phase 3: CriticalSoftPaths count = 0
Warning: Phase 3: NO ASSETS TO PRELOAD
Warning: Phase 3: This means map will load during Travel phase (causes freeze)

PIPELINE_COMPLETED duration=35.9ms
LoadMap(/City17/Maps/City17_Persistent_WP) took 9.114642 seconds
```

**Result:**
- Loading pipeline completes in 35ms (nothing to preload)
- ServerTravel/LoadMap takes 9.1 seconds (blocking freeze)
- Loading screen visible for only 35ms → pointless
- All 794 assets load synchronously → freeze

---

## Root Cause

### Reference Counting Strategy (InitialExperienceLoader.cpp:406-488)

**Code logic:**
```cpp
// Count how many times each asset is referenced
TMap<FSoftObjectPath, int32> AssetRefCounts;

for (const auto& Pair : AssetRefCounts)
{
    const int32 RefCount = Pair.Value;

    // Skip single-reference assets (likely streaming cell specific)
    if (RefCount == 1)
    {
        continue; // ← THIS REJECTS ALL 794 ASSETS
    }

    // Score shared assets: RefCount × TypeWeight
    const int32 Score = RefCount * TypeWeight;
    PrioritizedAssets.Add(FAssetPriority(AssetPath, Score));
}
```

**Why ALL assets have RefCount=1:**
- **World Partition architecture:** Each streaming cell owns unique asset INSTANCES
- AssetRegistry sees unique FAssetIdentifier per cell (different package paths)
- Example: `Cell_A/TreeMesh_1.uasset` vs `Cell_B/TreeMesh_2.uasset`
  - Both are instances of the same mesh
  - But AssetRegistry treats them as separate assets
  - Each gets RefCount=1
- UE's internal asset system deduplicates at load time (shared memory)
- But our discovery happens BEFORE load → sees duplicates as unique

**City17 WP map characteristics:**
- 2745 total dependencies
- 794 heavy assets (meshes, textures, materials)
- **0 assets with RefCount > 1** (all unique per cell)
- World Partition manages streaming AFTER initial load

---

## The "900 Assets Stuck in Async Loading" Issue

User's observation: "9 hundreds assets stuck in async loading"

**What's actually happening:**
1. Phase 3 preloads 0 assets (35ms)
2. ServerTravel loads City17 synchronously (9.1s freeze)
3. World Partition begins streaming cells during gameplay
4. Each cell tries to async-load its 794 unique assets
5. Async loading queue backs up → stuttering during gameplay

**Timeline:**
```
T+0s:    Click City17 button
T+0.04s: Pipeline completes (no preload) - loading screen disappears
T+0.04s: ServerTravel BLOCKS for 9.1s (black screen or last frame)
T+9.1s:  City17 loads, gameplay begins
T+9.1s+: World Partition streams cells → 794 assets in async queue → stuttering
```

---

## Why Existing Solutions Don't Work

### ❌ MoviePlayer + Ray Tracing
- Crashes in `FRayTracingGeometryManager::Tick()`
- Can't disable RT at runtime
- Architectural UE limitation

### ❌ Reference Counting Filter
- Assumes shared assets exist
- WP architecture: each cell has unique instances
- 794 discovered → 0 candidates → 0 preloaded

### ❌ Load All 794 Assets
- 60+ second timeout
- Defeats purpose of World Partition streaming
- OOM risk on lower-end hardware

---

## Solution Approaches

### Option 1: Size-Based Priority (RECOMMENDED)

**Strategy:** Preload heaviest assets first (by memory footprint), ignore reference count

**Changes to `DiscoverMapDependencies()`:**
```cpp
// Instead of RefCount filter, use asset SIZE filter
TArray<FAssetPriority> PrioritizedAssets;

for (const FSoftObjectPath& AssetPath : OutDependencies)
{
    // Get asset size from AssetRegistry
    TArray<FAssetData> Assets;
    AssetRegistry.GetAssetsByPackageName(FName(*AssetPath.GetLongPackageName()), Assets);

    for (const FAssetData& Asset : Assets)
    {
        int64 AssetSize = 0;
        Asset.GetTagValue("AssetSize", AssetSize);

        // Score by size × type weight
        int32 TypeWeight = GetTypeWeight(Asset.AssetClassPath);
        int64 Score = AssetSize * TypeWeight;

        PrioritizedAssets.Add(FAssetPriority(Asset.GetSoftObjectPath(), Score));
    }
}

// Sort by score (higher = bigger/more expensive)
PrioritizedAssets.Sort([](const FAssetPriority& A, const FAssetPriority& B)
{
    return A.Score > B.Score;
});

// Take top 20 heaviest assets
const int32 MaxAssets = FMath::Min(20, PrioritizedAssets.Num());
OutDependencies.Reset();
for (int32 i = 0; i < MaxAssets; ++i)
{
    OutDependencies.Add(PrioritizedAssets[i].Path);
}
```

**Pros:**
- Works for WP maps (doesn't rely on reference count)
- Preloads heaviest assets → biggest impact on load time
- Universal approach (works for any map)

**Cons:**
- `AssetSize` tag may not be accurate/available
- Need to calculate size at runtime (slower discovery)

---

### Option 2: Type-Based Priority

**Strategy:** Preload ALL materials first (shader compile time), then top meshes/textures

**Changes:**
```cpp
// Separate assets by type
TArray<FSoftObjectPath> Materials, Meshes, Textures;

for (const FSoftObjectPath& AssetPath : OutDependencies)
{
    const FTopLevelAssetPath& AssetClass = GetAssetClass(AssetPath);

    if (IsMaterial(AssetClass))
        Materials.Add(AssetPath);
    else if (IsMesh(AssetClass))
        Meshes.Add(AssetPath);
    else if (IsTexture(AssetClass))
        Textures.Add(AssetPath);
}

// Preload ALL materials (shader compile is expensive)
OutDependencies.Reset();
OutDependencies.Append(Materials);

// Fill remaining slots with meshes/textures
const int32 RemainingSlots = 20 - OutDependencies.Num();
const int32 MeshesToAdd = FMath::Min(RemainingSlots / 2, Meshes.Num());
const int32 TexturesToAdd = RemainingSlots - MeshesToAdd;

OutDependencies.Append(Meshes.GetData(), MeshesToAdd);
OutDependencies.Append(Textures.GetData(), TexturesToAdd);
```

**Pros:**
- Simple logic (no size calculation)
- Materials preload → avoids shader compile stutters
- Fast discovery

**Cons:**
- Arbitrary split (50% meshes, 50% textures)
- May preload small materials instead of huge meshes

---

### Option 3: Hybrid Approach (SIZE + TYPE)

**Strategy:** Score by `AssetSize × TypeWeight`, but remove RefCount requirement

**Changes:**
```cpp
// Remove RefCount filter (line 448-451)
// if (RefCount == 1) { continue; }  ← DELETE THIS

// Keep type-based scoring
int32 TypeWeight = 1;
if (IsMaterial(AssetClass))
    TypeWeight = 10;
else if (IsMesh(AssetClass))
    TypeWeight = 5;
else if (IsTexture(AssetClass))
    TypeWeight = 3;

// Score by SIZE × TypeWeight (not RefCount)
int64 AssetSize = GetAssetSize(AssetPath); // New function
const int64 Score = AssetSize * TypeWeight;
PrioritizedAssets.Add(FAssetPriority(AssetPath, Score));
```

**Pros:**
- Balances size and type importance
- Universal (works for WP and non-WP maps)
- Prioritizes expensive materials + huge meshes

**Cons:**
- Requires accurate asset size metadata
- More complex than Option 2

---

## Recommended Solution: Option 2 (Type-Based Priority)

**Why:**
- Fast implementation (no size calculation)
- Materials preload → biggest impact (shader compile time)
- Works immediately for City17 WP map
- Simple, predictable behavior

**Expected result:**
```
Phase 3: Discovered 794 heavy assets
Phase 3: Materials: 150, Meshes: 300, Textures: 344
Phase 3: Preloading: 20 materials
Phase 3: QUEUED 20 shared assets from CriticalSoftPaths
Phase 3: PreloadCriticalAssets duration = 3-5s
PIPELINE_COMPLETED duration = 3500ms (not 35ms)

ServerTravel duration = 5-6s (not 9.1s)
```

**Impact:**
- Loading screen visible for 3.5s (animated progress)
- Static frame at 90% for 5-6s (ServerTravel freeze)
- Total perceived load time: 8-9s (vs 9.1s black screen)
- Smooth progress bar → professional UX

---

## Implementation Plan

### Step 1: Remove RefCount Filter

**File:** `InitialExperienceLoader.cpp` (line 448-451)

**DELETE:**
```cpp
// Skip single-reference assets (likely streaming cell specific)
if (RefCount == 1)
{
    continue;
}
```

---

### Step 2: Implement Type-Based Priority

**File:** `InitialExperienceLoader.cpp` (line 423-488)

**REPLACE** priority calculation logic:

```cpp
// Build priority list: prefer materials (shader compile) > meshes > textures
TArray<FSoftObjectPath> Materials, Meshes, Textures;

for (const auto& Pair : AssetRefCounts)
{
    const FSoftObjectPath& AssetPath = Pair.Key;
    const FTopLevelAssetPath* AssetClassPtr = AssetClasses.Find(AssetPath);

    if (!AssetClassPtr)
    {
        continue;
    }

    const FTopLevelAssetPath& AssetClass = *AssetClassPtr;

    if (AssetClass.GetAssetName().ToString().Contains(TEXT("Material")))
    {
        Materials.Add(AssetPath);
    }
    else if (AssetClass == UStaticMesh::StaticClass()->GetClassPathName() ||
             AssetClass == USkeletalMesh::StaticClass()->GetClassPathName())
    {
        Meshes.Add(AssetPath);
    }
    else if (AssetClass.GetAssetName().ToString().StartsWith(TEXT("Texture")))
    {
        Textures.Add(AssetPath);
    }
}

// Preload materials first (shader compile is expensive)
OutDependencies.Reset();
const int32 MaxMaterials = FMath::Min(20, Materials.Num());
for (int32 i = 0; i < MaxMaterials; ++i)
{
    OutDependencies.Add(Materials[i]);
}

// If we have room, add meshes and textures
if (OutDependencies.Num() < 20)
{
    const int32 RemainingSlots = 20 - OutDependencies.Num();
    const int32 MeshesToAdd = FMath::Min(RemainingSlots / 2, Meshes.Num());

    for (int32 i = 0; i < MeshesToAdd; ++i)
    {
        OutDependencies.Add(Meshes[i]);
    }

    const int32 TexturesToAdd = FMath::Min(20 - OutDependencies.Num(), Textures.Num());
    for (int32 i = 0; i < TexturesToAdd; ++i)
    {
        OutDependencies.Add(Textures[i]);
    }
}

UE_LOG(LogProjectLoading, Display,
    TEXT("DiscoverMapDependencies: Type-based filtering - Materials=%d, Meshes=%d, Textures=%d -> Preloading %d"),
    Materials.Num(), Meshes.Num(), Textures.Num(), OutDependencies.Num());
```

---

### Step 3: Test

**Build:**
```bash
Build.bat AlisEditor Win64 Development -Module=ProjectLoading
```

**Run standalone game:**
```bash
python scripts/ue/standalone/test.py --timeout 30
```

**Expected log:**
```
DiscoverMapDependencies: Found 2745 direct dependencies
DiscoverMapDependencies: Discovered 794 heavy assets
DiscoverMapDependencies: Type-based filtering - Materials=150, Meshes=300, Textures=344 -> Preloading 20
InitialExperienceLoader: Auto-discovered 20 assets via CriticalSoftPaths

Phase 3: CriticalSoftPaths count = 20
Phase 3: QUEUED 20 shared assets from CriticalSoftPaths
Phase 3: PreloadCriticalAssets duration = 3500ms

ServerTravel duration = 5-6s
PIPELINE_COMPLETED duration = 3500ms
```

---

## World Partition Asset Lifetime (CORRECTED)

**Question:** Will preloaded assets stay in memory after map loads?

**Answer:** ONLY if you keep hard references (handles or UObject pointers) alive.

**How StreamableManager actually works:**
1. `bManageActiveHandle=false` (current code): Manager releases handle after delegate fires
2. `bManageActiveHandle=true`: Manager keeps handle until you call `ReleaseHandle()`
3. Assets stay resident ONLY while something references them:
   - Active `FStreamableHandle` you're holding
   - `UObject*` stored somewhere
   - Currently loaded WP cell referencing the asset

**Previous bug (NOW FIXED):**
- Handle was LOCAL variable in `PreloadCriticalAssetsPhaseExecutor::Execute()`
- `bManageActiveHandle=false` - manager didn't keep it
- Handle went out of scope when function returned
- Assets became unloadable BEFORE Travel phase even started!

**Current implementation (FIXED):**
- `bManageActiveHandle=true` - we control handle lifetime
- Handle stored in `Context.Runtime.PreloadHandles`
- Released in Warmup phase (or via ON_SCOPE_EXIT safety net in orchestrator)
- Failure paths explicitly call `ReleaseHandle()` to prevent leaks

**Implementation details:**
```cpp
// Phase 3: Store handle in Context.Runtime.PreloadHandles
TSharedPtr<FStreamableHandle> Handle = StreamableManager.RequestAsyncLoad(
    AssetsToLoad,
    FStreamableDelegate(),
    FStreamableManager::AsyncLoadHighPriority,
    true,   // bManageActiveHandle = TRUE (we own lifetime)
    false,
    TEXT("ProjectLoading_CriticalPreload")
);
Context.Runtime.PreloadHandles.Add(Handle);

// Phase 6 (Warmup): Release after WP streaming started
Context.Runtime.ReleasePreloadHandles();

// Safety net in LoadPipelineOrchestrator::Execute():
ON_SCOPE_EXIT { Context.Runtime.ReleasePreloadHandles(); };
```

**After release:**
- If asset is needed by currently loaded WP cells -> stays resident (WP refs keep it)
- If asset was wrong pick (far away, never used) -> becomes unloadable after GC

**Testing note:** PIE lies about unloading (editor may keep assets around).
Validate in **Standalone** or **packaged build** before concluding "it never unloads".

**Verification commands (packaged build):**
- `memreport -full` before preload, after preload, after releasing handles + GC
- `stat memory` and `stat streaming` to see if memory drops

**Memory impact (with proper handle lifecycle):**
- 20 preloaded assets pinned through Travel: ~50-200 MB temporary
- Released after WP takes over: only WP-referenced assets remain
- No permanent overhead from "wrong picks"

---

## Next Steps

1. [x] Implement Type-Based Priority (remove RefCount filter) - DONE
2. [ ] Test City17 load (expect 20 materials preloaded, 3-5s Phase 3)
3. [ ] Verify loading screen visible for 3.5s (not 35ms)
4. [ ] Monitor ServerTravel duration (expect 5-6s, not 9.1s)
5. [ ] Check WP streaming behavior (preloaded assets stay loaded)
6. [ ] Update `loading_progress.md` with new strategy
7. [ ] Update `fix_loading_progress.md` TODO checklist

---

## Type-Based Filtering (IMPLEMENTED)

**Previous broken logic:**
```cpp
// Skip single-reference assets (likely streaming cell specific)
if (RefCount == 1) { continue; }  // <-- NUKES EVERYTHING IN WP MAPS
```

**New type-based priority (InitialExperienceLoader.cpp:423-478):**
1. Categorize discovered assets by type: Materials, Meshes, Textures
2. Fill slots: Materials first (shader win), Meshes, Textures
3. Cap at 20 total (bounded memory)

**Why materials first:** Shader compilation is the biggest win. Materials trigger PSO precompilation during preload, reducing shader stutter during gameplay.

**Log markers to verify:**
```
DiscoverMapDependencies: Type-based filtering - Materials=X Meshes=Y Textures=Z -> Preloading 20 (cap=20)
Phase 3: CriticalSoftPaths count = 20
Phase 3: COMPLETED in 3.50s  // was 35ms when filtering to 0
```
