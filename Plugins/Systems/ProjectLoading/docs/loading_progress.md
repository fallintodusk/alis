# Loading Progress System - Reality Check

**Status:** KISS approach - viewport widget + pipeline progress
**Related:** [Todo](../todo/fix_loading_progress.md)

---

## THE TRUTH

### What We Cannot Do (Without Engine Patches)

1. **No real-time progress during `ServerTravel/LoadMap`**
   - This is a blocking engine call
   - Game code doesn't tick inside it
   - No API exposes "37.4% loaded"

2. **MoviePlayer + Ray Tracing = Crash**
   - `FRayTracingGeometryManager::Tick()` assertion
   - Can't disable RT at runtime
   - Video-only might work but no progress bar

3. **No way to avoid the freeze without architecture rework**
   - Seamless travel = major refactor
   - World Partition doesn't stream incrementally during first load

### What We CAN Do

1. **Show accurate progress for OUR pipeline** (Phases 1-6)
   - Resolve assets, mount content, async loads = real percentage
   - This is 80-90% of perceived load time if we preload properly

2. **Show static loading frame during blocking `LoadMap`**
   - Last drawn frame stays on screen
   - No black screen
   - User sees "Loading... 90%" for 1-2 seconds

3. **Move heavy work BEFORE travel**
   - Preload City17 assets in Phase 3
   - PSO warmup (future)
   - Shrink "black hole time" from 9s to ~1s

---

## KISS Target Design

### Architecture

```
Player clicks City17
    |
    v
LoadingScreenSubsystem::OnLoadStarted
    |-- Create W_LoadingScreen
    |-- Add to viewport (NOT world-bound)
    |-- Subscribe to pipeline progress
    v
Pipeline executes (Phases 1-6)
    |-- Phase 1: Resolve Assets -> 10%
    |-- Phase 2: Mount Content -> 30%
    |-- Phase 3: Preload City17 Assets -> 80%
    |   |-- Hardcoded list of heavy assets
    |   |-- StreamableManager async load
    |   |-- Real progress from loaded/total
    |-- Phase 4: Warmup -> 85%
    |-- Phase 5: Travel -> 90% (STATIC FRAME)
    v
[FREEZE] ServerTravel/LoadMap (1-2s)
    |-- Widget still visible (last frame)
    |-- No progress updates
    v
PostLoadMapWithWorld fires
    |
    v
LoadingScreenSubsystem::OnCompleted
    |-- Fade out widget
    |-- Remove from viewport
```

### Key Principles

1. **Widget lives in viewport, not world**
   - Use `UGameViewportClient::AddViewportWidgetContent`
   - Survives `ServerTravel`

2. **Accept approximate progress**
   - Pipeline phases = 0-90%
   - Static frame during LoadMap = 90-100%
   - This is how most games work

3. **Preload shared assets only**
   - Auto-discover map dependencies via AssetRegistry
   - Count references (skip single-use assets)
   - Score by importance: `RefCount × TypeWeight`
   - Load top 20 shared assets (vs 794 total discovered)
   - WP handles cell streaming automatically

---

## Implementation Plan

### Step 1: Fix Widget Ownership

**Current problem:** Widget might be world-bound or destroyed on map change.

**File:** `LoadingScreenSubsystem.cpp`

**Change:**
```cpp
void ULoadingScreenSubsystem::OnLoadStarted(const FLoadRequest& Request)
{
    if (!LoadingWidget)
    {
        // Create widget
        LoadingWidget = CreateWidget<UW_LoadingScreen>(GetGameInstance(), LoadingWidgetClass);

        // Add to VIEWPORT, not world
        if (UGameViewportClient* Viewport = GetGameInstance()->GetGameViewportClient())
        {
            Viewport->AddViewportWidgetContent(LoadingWidget->TakeWidget(), 1000); // High Z-order
        }
    }

    // Bind to pipeline events
    LoadingWidget->SetProgress(0.0f);
    LoadingWidget->SetVisible(true);
}

void ULoadingScreenSubsystem::OnCompleted(...)
{
    if (LoadingWidget && GetGameInstance()->GetGameViewportClient())
    {
        // Fade out (optional)
        LoadingWidget->PlayFadeOut();

        // Remove after delay
        FTimerHandle Handle;
        GetWorld()->GetTimerManager().SetTimer(Handle, [this]()
        {
            if (LoadingWidget && GetGameInstance()->GetGameViewportClient())
            {
                GetGameInstance()->GetGameViewportClient()->RemoveViewportWidgetContent(LoadingWidget->TakeWidget());
                LoadingWidget = nullptr;
            }
        }, 0.5f, false);
    }
}
```

---

### Step 2: Flush Frame Before Travel

**File:** `TravelPhaseExecutor.cpp`

**Add before `ServerTravel()`:**
```cpp
// Set progress to 90% and flush a frame
ReportProgress(Context, 0.9f, LOCTEXT("PreparingTravel", "Preparing to travel..."));

// Flush rendering commands to ensure loading screen is visible
if (UGameViewportClient* Viewport = GetWorld()->GetGameViewportClient())
{
    FlushRenderingCommands();
}

// Small delay to let viewport render
FPlatformProcess::Sleep(0.033f); // One frame at 30fps

// NOW call ServerTravel (this will freeze)
GetWorld()->ServerTravel(MapPath, false);
```

---

### Step 3: Auto-Filter Shared Assets (DONE)

**File:** `InitialExperienceLoader.cpp::DiscoverMapDependencies()`

**Smart filtering for World Partition maps:**
- Discovers all dependencies via AssetRegistry (e.g., 794 assets for City17)
- **Counts references** to each asset (how many cells use it)
- **Skips single-reference assets** (cell-specific, WP streams them)
- **Scores shared assets**: `Score = RefCount × TypeWeight`
  - Material: TypeWeight=10 (shared + expensive to compile)
  - StaticMesh: TypeWeight=5 (less shared, but expensive)
  - Texture: TypeWeight=3 (often shared)
- Takes **top 20 shared assets only**

**Result:** ~20 high-priority shared assets (vs 794 total)

**Why this works:**
- WP handles cell streaming automatically
- We only preload assets used across MULTIPLE cells
- Materials/shaders benefit most from preload (compile time)

**No hardcoding needed!** Works for any WP map automatically.

**File:** `PreloadCriticalAssetsPhaseExecutor.cpp`

**Enable CriticalSoftPaths:**
```cpp
// Load CriticalSoftPaths (auto-discovered + filtered assets)
UE_LOG(LogProjectLoading, Display, TEXT("Phase 3: CriticalSoftPaths count = %d"), Context.Request.CriticalSoftPaths.Num());
for (const FSoftObjectPath& SoftPath : Context.Request.CriticalSoftPaths)
{
    AssetsToLoad.Add(SoftPath);
}
UE_LOG(LogProjectLoading, Display, TEXT("Phase 3: Queued %d assets from CriticalSoftPaths"), Context.Request.CriticalSoftPaths.Num());
```

**No limit needed** - auto-discovery already filters to ~20 shared assets with reference counting.

---

### Step 4: Delete MoviePlayer Wiring

**File:** `ProjectLoading.uplugin`

**Remove module:**
```json
{
    "Name": "ProjectLoadingMoviePlayer",
    "Type": "Runtime",
    "LoadingPhase": "PostConfigInit"
}
```

**Or keep disabled** - your choice. MoviePlayer can stay for startup splash only.

---

### Step 5: Progress Mapping

**File:** `ProjectLoadingSubsystem.cpp`

**Map phases to progress:**
```cpp
float UProjectLoadingSubsystem::CalculateProgress(ELoadPhase Phase, float PhaseProgress)
{
    // Map phases to 0-90% (leave 10% for ServerTravel freeze)
    switch (Phase)
    {
        case ELoadPhase::ResolveAssets: return 0.1f * PhaseProgress;
        case ELoadPhase::MountContent:  return 0.1f + (0.2f * PhaseProgress);
        case ELoadPhase::PreloadCriticalAssets: return 0.3f + (0.5f * PhaseProgress); // Heavy phase
        case ELoadPhase::Warmup:        return 0.8f + (0.05f * PhaseProgress);
        case ELoadPhase::Travel:        return 0.85f + (0.05f * PhaseProgress); // Static frame
        case ELoadPhase::PostLoad:      return 0.9f + (0.1f * PhaseProgress);
        default: return 0.0f;
    }
}
```

---

## Expected Result

**User experience:**
1. Click "City17" button
2. Loading screen appears: "Loading... 0%"
3. Progress bar animates smoothly: 0% -> 10% -> 30% -> 80%
   - This takes 5-7 seconds (our pipeline phases)
   - Real progress from async asset loading
4. Progress bar reaches 90%, text changes: "Entering City17..."
   - Static frame (no updates)
   - Lasts 1-2 seconds (ServerTravel freeze)
5. Map appears, loading screen fades out

**Total perceived load time:**
- Before: ~10s of black screen
- After: 7-8s of animated progress + 1-2s static frame

---

## Tried Approaches (Why They Failed)

### 1. RefCount==1 Filter for WP Maps
**Problem:** Filtered ALL assets to 0. World Partition uses unique asset instances per cell, so every asset has RefCount=1. No "shared" assets exist from AssetRegistry's perspective.
**Fix:** Switched to type-based priority (Materials > Meshes > Textures).

### 2. Blocking Wait Loop with Sleep()
**Problem:** `FPlatformProcess::Sleep(0.016f)` pauses thread but doesn't pump Slate. Widget froze at 42.5% because UI couldn't update.
**Fix:** Added `FSlateApplication::Get().PumpMessages()` and `Tick()` inside wait loop.

### 3. Fire-and-Forget (No Wait)
**Problem:** Phase 3 completed instantly (~0ms). No time to show progress 0-90%. User saw brief flash then ServerTravel freeze.
**Fix:** Reverted to blocking wait loop WITH Slate pumping for animated progress.

### 4. GetDependencies Inside Per-Asset Loop
**Problem:** Called `GetDependencies()` once per asset in each package. If package has N assets, dependencies appended N times causing massive duplication.
**Fix:** Track visited packages with `TSet<FName>`, call `GetDependencies` once per package outside asset loop.

### 5. MoviePlayer for Loading Screen
**Problem:** `FRayTracingGeometryManager::Tick()` assertion crash. MoviePlayer incompatible with ray-tracing enabled scenes.
**Fix:** Use viewport UMG widget instead. Survives ServerTravel when added to viewport (not world).

### 6. Slate in Systems Layer (Layering Violation)
**Problem:** `ProjectLoading` (Systems) calling `FSlateApplication::PumpMessages()` directly violated layer separation. UI framework shouldn't be pulled into Systems.
**Fix:** Dependency injection - added `Context.OnPumpFrame` callback. UI layer (LoadingScreenSubsystem) sets callback during Initialize. Systems just calls it without knowing about Slate.

### 7. Async Load Stuck at 0% (Game Thread Blocked)
**Problem:** Async load never progressed - ALL 20 assets stuck at 0% for 60 seconds then timeout. Blocking game thread with Sleep() + Slate pump wasn't enough.
**Fix:** Added `ProcessAsyncLoading(true, 0.016f)` in wait loop. This ticks the async loading system to process IO and callbacks.
**Note:** Don't use `ProcessThreadUntilIdle` - causes stack overflow due to recursion when already inside a task graph task.

---

## What We're NOT Doing

1. ❌ MoviePlayer integration (RT crash)
2. ❌ Real-time progress during ServerTravel (impossible)
3. ❌ Loading all 794 assets (timeout)
4. ❌ Perfect 0-100% progress (approximate is fine)

## What We ARE Doing

1. ✅ Viewport widget (survives travel)
2. ✅ Pipeline-driven progress (real async loads)
3. ✅ **Shared assets only** (794 -> 20, type-based priority)
4. ✅ Static frame during freeze (last frame visible)
5. ✅ Professional UX (smooth progress bar)
6. ✅ **Clean architecture** (UI layer injects Slate pump via callback)

---

## Files Changed

| File | Action | Status |
|------|--------|--------|
| `ProjectLoadPhaseExecutor.h` | Add OnPumpFrame callback to context | DONE |
| `PreloadCriticalAssetsPhaseExecutor.cpp` | Use Context.OnPumpFrame instead of Slate | DONE |
| `ProjectLoading.Build.cs` | Remove Slate/ApplicationCore dependencies | DONE |
| `ProjectLoadingSubsystem.h` | Add SetPumpFrameCallback API | DONE |
| `LoadPipelineOrchestrator.cpp` | Wire callback from subsystem to context | DONE |
| `LoadingScreenSubsystem.cpp` (UI) | Inject Slate pump callback | DONE |
| `ProjectUI.Build.cs` | Add ApplicationCore dependency | DONE |
| `InitialExperienceLoader.cpp` | Type-based asset filtering | DONE |

**Architecture:** Systems layer calls Context.OnPumpFrame(). UI layer provides the implementation.

---

## Next Actions

### Test the Implementation

**All code changes are complete.** Now test:

1. Rebuild: `scripts/ue/build/build.bat AlisEditor Win64 Development`
2. Run standalone game
3. Click City17 button
4. **Expected:** Loading screen animates 0-90% during Phase 3 preload (3-5 seconds)
5. **Expected:** Static frame at 90% during ServerTravel freeze (1-2 seconds)
6. Check log for:
   - `"Phase 3: Waiting for async load with Slate pumping..."`
   - `"Phase 3: Async load progress: X%"`
   - `"Subscribed to ProjectLoadingSubsystem events (with frame pump callback)"`

### If Problems

- No progress animation: Check if LoadingScreenSubsystem initialized before load starts
- Widget not visible: Check W_LoadingScreen is in viewport (high Z-order)
- Crash: Check log for assertion failures

This is the **robust, shippable solution** without engine patches.
