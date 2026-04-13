# Investigate Low FPS (12 FPS → 33 FPS in Editor)

## Status: DONE — Root causes identified, primary fixes applied

**Hardware:** RTX 3060 Laptop (6 GB VRAM) + Intel Iris Xe iGPU
**Scene:** City17_Persistent_WP (PIE)
**Before:** 12 FPS / 79 ms — "Video memory exhausted (364 MB over budget)"
**After:** 33 FPS / 29 ms — 2.75x improvement

> **Note:** Only the three confirmed fixes below are applied. All other hypotheses (scalability unlock, MaxLODSize, VSM lights, Path Tracing, code fixes) require further testing before applying.

---

## Confirmed Fixes (applied)

| # | Fix | Impact | Config Change |
|---|-----|--------|---------------|
| 1 | Disable second monitor + close browser | 12→16 FPS. Biggest single win — freed VRAM from DWM + browser GPU process | Auto-watcher script via VS Code task |
| 2 | `r.HairStrands.Enable=0` | Most significant FPS boost among all feature toggles | `Config/DefaultEngine.ini` |
| 3 | `r.Streaming.PoolSize=2000` (was 8000) | Eliminated "video memory exhausted" warning. 8 GB pool on 6 GB card was the root cause of VRAM thrashing | `Config/DefaultEngine.ini` |

---

## Test Results (2026-04-01)

### Hypothesis 1: Dual Monitor → BUSTED (secondary factor)

| Config | FPS | GPU ms | Over Budget |
|--------|-----|--------|-------------|
| Dual monitor, pool 8 GB | 12.03 | ~79 ms | 364 MB |
| Single monitor, pool 8 GB | 16.18 | 60.29 ms | 931 MB |

Second monitor = ~4 FPS cost, not the primary cause.

### Hypothesis 2: Streaming Pool Too Large → CONFIRMED

| Pool Size | FPS | GPU ms | Over Budget |
|-----------|-----|--------|-------------|
| 8000 (8 GB) | 16.18 | 60.29 ms | 931 MB |
| 2000 (2 GB) | 25.92 | 37.29 ms | 306 MB |

+60% FPS. Eliminated VRAM overcommitment.

### Hypothesis 3: Feature Toggles (cumulative, pool 2 GB, single monitor)

| Change | FPS | GPU ms | Over Budget |
|--------|-----|--------|-------------|
| Pool 2 GB only | 25.92 | 37.29 ms | 306 MB |
| + HairStrands off | 30.90 | 31.98 ms | 250 MB |
| + SW Lumen (HW RT off) | 29.60 | 32.23 ms | 72 MB |
| HW RT re-enabled | ~30 | ~32 ms | ~100-250 MB |
| All optimized + watcher | 33.44 | 29.01 ms | — |
| Heavy scene (UAZ, 3300K prims) | ~30 | 31.43 ms | 5 MB (RT geom) |

**Findings:**
- HairStrands off = biggest feature-level FPS gain
- SW Lumen = worse FPS than HW RT on this GPU. Keep HW RT ON.
- MegaLights = already disabled, no impact
- RT Geometry warning (405/400 MB) = minor, scene-dependent

---

## Root Cause

`r.Streaming.PoolSize=8000` (8 GB) on a 6 GB card. Total VRAM demand ~9-11 GB vs 6 GB available. Windows paged GPU memory to system RAM → catastrophic FPS.

### VRAM Budget (original config)

| Consumer | Est. VRAM |
|----------|-----------|
| Texture streaming pool | 8,000 MB |
| Nanite geometry | 300-800 MB |
| Lumen HW RT | 200-400 MB |
| Virtual Shadow Maps | 150-300 MB |
| Hair Strands | 100-200 MB |
| Virtual Textures | 100-200 MB |
| Distance Fields | 50-150 MB |
| Render targets / GBuffer | 150-300 MB |
| OS / DWM / dual monitor | 200-400 MB |
| **Total demand** | **~9,250-10,750 MB** |
| **Available** | **6,144 MB** |
| **Deficit** | **~3,100-4,600 MB** |

---

## Pending Investigation (not yet tested)

- [ ] `r.Streaming.PoolSize 1500` — tighter pool
- [ ] `r.Shadow.Virtual.OnePassProjection.MaxLightsPerPixel 8` (currently 32)
- [ ] `r.PathTracing 0` — free VRAM from unused allocation
- [ ] `MaxLODSize 2048` for non-character texture groups
- [ ] Scalability unlock (DefaultScalability.ini — all levels identical, no scaling)
- [ ] Motion matching character isolation (Mutable sync builds, MetaHuman cost)
- [ ] Code fixes (see below)

### Profiling Plan for Remaining Systems

**Step 1: Character Isolation**
- [ ] Open City17 level, delete/hide the motion matching character
- [ ] Run `stat unit` — record Game / Draw / GPU / RHIT ms
- [ ] Run `stat gpu` — record total GPU ms and top 5 passes
- [ ] Run `stat rhi` — record total VRAM allocated
- [ ] Run `stat streaming` — record pool usage vs budget
- [ ] Restore character, repeat all stats. If delta >10 ms — character is a significant contributor.

**Step 2: Feature Toggle Test (remaining)**

| # | Console command | What it disables | Expected gain |
|---|----------------|-----------------|---------------|
| 1 | `r.Shadow.Virtual.OnePassProjection.MaxLightsPerPixel 8` | Reduce VSM lights from 32 | Low-Medium |
| 2 | `r.PathTracing 0` | Path tracing resource allocation | Low |
| 3 | `r.VirtualTextures 0` | Virtual texture cache | Low-Medium |
| 4 | `r.GenerateMeshDistanceFields 0` | Distance fields | Low |

- [ ] Record FPS after each toggle (cumulative)
- [ ] Identify contributors worth permanently disabling

**Step 3: Scalability Differentiation**
- [ ] Test `sg.ShadowQuality 0` vs `sg.ShadowQuality 3` — measure FPS delta
- [ ] Test `sg.GlobalIlluminationQuality 0` vs `3` — measure FPS delta
- [ ] If significant deltas — unlock scalability tiers in `DefaultScalability.ini`

**Step 4: Unreal Insights (deep profiling)**
- [ ] Launch with `-trace=default,gpu` and `-statnamedevents`
- [ ] Record 30-second capture in City17
- [ ] Analyze GPU timeline for heaviest passes

---

## Code Issues Found (untested impact)

**Critical (per-frame):**
1. `RotatorKinematicDriver.cpp:119` — OverlapMultiByObjectType every tick per door. Fix: throttle or gate on angle change.
2. `InstanceArraysObject/Base/Actor.cpp` — bCanEverTick=true with empty Tick(). Hundreds of instances ticking for nothing.
3. 21x `LoadSynchronous()` at runtime — blocks game thread, causes hitches.

**High (per-interaction):**
4. `InteractionComponent.cpp:90` — GetComponents() + sort on every trace hit. Fix: cache.
5. `ProjectCharacter.cpp:705-749` — SetSkeletalMeshAsset() on every Mutable update without change check.

**Medium:**
6. Chaos driver retry timers without backoff.
7. Mutable sync builds (`CookAsync=0`, up to 1s/tick block).
8. Verbose UE_LOG in motion hot paths.

---

## Reference Files

| File | Purpose |
|------|---------|
| `Config/DefaultEngine.ini` | Streaming pool, feature toggles |
| `Config/DefaultScalability.ini` | Scalability overrides (locked) |
| `Config/DefaultDeviceProfiles.ini` | MaxLODSize=4096 everywhere |
| `<user-home>\Scripts\UE_MonitorWatcher.ps1` | Auto-disable 2nd monitor |
| `.vscode/tasks.json` | Auto-launches watcher on workspace open |
