# Performance - Outdoor Lag in Shipping Build

**Status:** CLOSED. User-visible outdoor lag resolved. All 11 original log items + R1-R4 verified closed in `ALIS_20260428_113909` Shipping log. Forward-facing items moved to backlog with explicit reopen criteria.
**Priority:** N/A (closed; reference only)
**Date:** 2026-04-17 (initial); 2026-04-22 (sync-load audit); 2026-04-27 (re-verification on City17); 2026-04-28 (R1+R3+R4 closure verified, R2 closed as accepted noise, R5+R6 spun out to backlog, archived to `01_done/`)
**Original build:** ALIS_20260417_173648 (Shipping, Win64)
**Final verified build:** ALIS_20260428_113909 (Shipping, Win64)
**Final log:** `<local-app-data>\Alis\Saved\Logs\Alis.log` (2026-04-28 11:53; verifies R1+R3+R4 clean)

The headline perf bug was a cluster of 11 log items contributing to outdoor
entry stutter. All addressed. R1 (save-load reapplied Epic scalability) was
the only real engineering bug; clamping save-applied graphics quality to High
in `UProjectSaveSubsystem::ApplyGameSettings` closed it. Verified in Shipping
log: save load now keeps `ViewDistanceQuality@2` and `r.TSR.History.ScreenPercentage:100`.

Forward-facing items spun out to backlog so they survive this archive:

- `todo/02_backlog/systems/perf_spawn_path_async_loads.md` (R5 - sync-load conversions; reopen if spawn-path stalls become user-visible).
- `todo/02_backlog/systems/perf_mutable_insights_capture.md` (R6 - Mutable Insights right-sizing; reopen if Mutable spills past 256 MiB budget again).

This file is kept in `01_done/` as reference: the verification cheat-sheet,
engine-backed standards, key log lines, and the original symptom investigation
remain useful for future perf work in similar territory.

---

## Done

Verified clean in `ALIS_20260427_200846` log unless noted otherwise.

- **#1 OffsetRootBone CVar spam:** 72,534 -> 0 (CVar registered in `GASPConsoleVariables.cpp`).
- **#2 PSO precache + disk cache:** 33/50 -> 40/50 precached. `D3D12.PSO.DiskCache=1`, `DriverOptimizedDiskCache=1`, `PSOPrecache.Resources=1`, `PSOPrecache.ProxyCreationWhenPSOReady=1` all confirmed in log.
- **#3 Phase 3 preload (City17):** 0 -> 20/928 heavy assets preloaded. `bSerializeDependencies=True` produces runtime deps. (MainMenu still empty - expected, light scene.)
- **#5 (partial) Invalid ShaderMap (8 editor nulls):** Original 8 mesh-component nulls assigned in editor. A different ShaderMap error remains - tracked as R2.
- **#6 AbilitySet scan failure:** 0 occurrences. Scan spec removed from `ProjectGASModule.cpp`; re-add with `bRequireNonEmpty=true` once `DefinitionGenerator` produces real assets.
- **#7 HairStrands missing package:** 0 occurrences. `/HairStrands` added to `DirectoriesToAlwaysCook` in `DefaultGame.ini`.
- **#8 PathTracing in Shipping:** `r.PathTracing:0` confirmed in log.
- **#9 (partial) TSR clamp on first run:** `EnsureFirstRunDefaults` correctly clamps to 100 at first launch. Save-load reverts it - tracked as R1.
- **#10 r.setres 1280x720 startup CVar:** Still set at startup (line 387) but immediately overridden by `EnsureFirstRunDefaults` (desktop res + windowed fullscreen). Cosmetic only.
- **#11 SM_Tree_Hornbeam variants + AmurCork (R3 closed 2026-04-28):** RT disabled on all 5 Hornbeam variants (Medium, Big_Single via editor; Big, Big_Double, Small via MCP Python) and SM_Tree_AmurCork_Big. Audit/fix records: `Saved/Logs/sm_rt_audit.json` + `sm_rt_disable.json`. Other Nature foliage was already correctly configured (Nanite=true, RT=false).
- **R4 Mutable budget bumped to 256 MiB (2026-04-28):** `mutable.WorkingMemory=262144` in `DefaultEngine.ini`. ~7% headroom over observed 240 MiB peak. R6 (Insights capture) only if future logs still spill - see Remaining.
- **R1 TSR save-load revert closed + verified (2026-04-28, build `ALIS_20260428_113909`):** `UProjectSaveSubsystem::ApplyGameSettings` now clamps `Settings.GraphicsQuality` to `[0, 2]` (High) before `SetOverallScalabilityLevel`, mirroring `EnsureFirstRunDefaults` policy. Struct default also dropped from `3` (Epic) to `2`. Both [Plugins/Systems/ProjectSave/Source/ProjectSave/Public/Data/ProjectSaveData.h:65](../../Plugins/Systems/ProjectSave/Source/ProjectSave/Public/Data/ProjectSaveData.h) and [Plugins/Systems/ProjectSave/Source/ProjectSave/Private/ProjectSaveSubsystem.cpp:332-361](../../Plugins/Systems/ProjectSave/Source/ProjectSave/Private/ProjectSaveSubsystem.cpp). **Verified in Shipping log:** line 2206 `Loading save from slot: SaveSlot_AutoSave` -> 2207 `ViewDistanceQuality@2` (not @3) -> 2212 `r.TSR.History.ScreenPercentage:100` (not 200) -> 2296 `Applied graphics settings (Quality: 2, Resolution: 1920x1080, VSync: 1)`.
- **R2 closed as accepted log noise (2026-04-28):** Once-per-session `LogMaterial: Error: Loading a material resource None with an invalid ShaderMap!` fires during MainMenu -> City17 transition, immediately after `FlushAsyncLoading(102): 1 QueuedPackages`. Engine source [`MaterialShared.cpp:1531`](<ue-path>/Engine/Source/Runtime/Engine/Private/Materials/MaterialShared.cpp): inline ShaderMap deserializes with `bValid=false`; `GetFriendlyName()` returns "None" because owning UMaterial is null. **Diagnosed not actionable:** (1) Python audit of 1328 MICs found 10 with broken parent in editor source, but **0 of the 10 cooked into Shipping** ([Saved/Logs/mi_broken_triage.json](../../Saved/Logs/mi_broken_triage.json)) - they're not the culprit; (2) `[Core.Log] LogMaterial=Verbose` added for build `ALIS_20260428_113909` produced ZERO additional context (only 2 LogMaterial lines in 4199, the warning still says "None"); (3) one occurrence per session, no visual artifact, no perf cost. Per Epic forum guidance ([thread](https://forums.unrealengine.com/t/valid-shadermap-and-not-returned-true-errors/772084)) this bug class is unresolved when UE only prints "None". Verbose flag removed from `DefaultEngine.ini` after capture. **Reopen if:** warning starts repeating (>1/session) or visual checker-pattern appears.

**Follow-up (separate decision, not bundled in R1):** [ProjectSettingsService.cpp:264-273](../../Plugins/UI/ProjectSettingsUI/Source/ProjectSettingsUI/Private/ProjectSettingsService.cpp) currently allows GraphicsQuality up to Epic (`[0, 3]`) on the user-driven settings UI path. If ALIS policy is "High max **by default**" (current patch is sufficient), no change needed. If policy is "High max **always**" (Epic forbidden everywhere), the UI save/apply path must clamp to 2 too. Decide policy before bundling further work.

---

## Verification cheat-sheet (per item)

Run each check after a fresh Shipping run against
`<local-app-data>\Alis\Saved\Logs\Alis.log`. All commands assume the repo
working directory and a bash shell (git-bash). For PowerShell substitute
`Select-String -Path $env:LOCALAPPDATA\Alis\Saved\Logs\Alis.log -Pattern '...'`.

Variable: `L="$LOCALAPPDATA/Alis/Saved/Logs/Alis.log"` (or paste the absolute path).

### Closed items - regression sentinels (these MUST stay clean)

- **#1 OffsetRootBone CVar spam** — `grep -c "a.animnode.offsetrootbone.enable" "$L"` -> expect `0`
- **#2 PSO precache + disk cache** — `grep -E "PSO\.DiskCache=1|DriverOptimizedDiskCache=1|PSOPrecache\.Resources=1|PSOPrecache\.ProxyCreationWhenPSOReady=1" "$L"` -> all four lines present; `grep "PSO creation hitches" "$L"` -> precached ratio should be `>= 40/50` (improving over time as disk cache populates)
- **#3 Phase 3 preload (City17)** — `grep "DiscoverMapDependencies.*City17.*Found" "$L"` -> non-zero direct deps; `grep "Phase 3.*Preload Critical Assets - Starting" "$L"` followed by a `Preloading [N]` line with `N >= 1` for City17 (not `NO ASSETS TO PRELOAD`)
- **#6 AbilitySet scan** — `grep "ProjectAbilitySet.*0 registered" "$L"` -> expect empty
- **#7 HairStrands missing** — `grep -E "/HairStrands.*SkipPackage|StableRodsSystem.*does not exist" "$L"` -> expect empty
- **#8 PathTracing in Shipping** — `grep "r.PathTracing:" "$L"` -> only matches should be `r.PathTracing:0`; never `:1`
- **#10 r.setres** — `grep "r.setres:" "$L"` -> startup line `1280x720` is acceptable IF followed by `LogConfig: Set CVar [[r.setres:` at desktop res from `EnsureFirstRunDefaults`. Cosmetic only.
- **R1 TSR save-load** — `awk '/LogProjectSave: Loading save from slot:/{p=1} p && /ViewDistanceQuality@3/{print "FAIL@3"; exit 1} p && /TSR.History.ScreenPercentage:200/{print "FAIL TSR=200"; exit 1}' "$L"` -> expect no FAIL output; also `grep "Applied graphics settings (Quality: 2," "$L"` -> at least one match per save load
- **R3 Hornbeam + AmurCork (Nanite+WPO+RT)** — `grep "Nanite instanced static mesh using World Position Offset not supported in ray tracing" "$L"` -> expect empty
- **R4 Mutable budget** — `grep "Failed to keep memory budget" "$L"` -> expect empty (or, if hits, all should show `Budget: 262144` and confirm we still need R6 Insights). Confirm budget applied: `grep "mutable.WorkingMemory:262144" "$L"` -> 1 match.

### Remaining items - check whether they have moved

- **R2 Invalid ShaderMap on City17** — `grep "Loading a material resource None with an invalid ShaderMap" "$L"` -> when 0, R2 closes. Until then count should not grow.
- **R5 Sync-load conversions in spawn path** — no direct log signal. Soft check: City17 outdoor entry frame time / hitch count via Insights or `stat unit`. Reopen if a new map shows visible spawn-path stalls.
- **R6 Mutable Insights** — only run if R4 sentinel above shows budget spills again (i.e. `grep "Failed to keep memory budget" "$L"` returns hits despite the 256 MiB cap).

### One-shot pass/fail (run all sentinels at once)

```bash
L="$LOCALAPPDATA/Alis/Saved/Logs/Alis.log"
fail=0
test "$(grep -c 'a.animnode.offsetrootbone.enable' "$L")" = "0" || { echo "FAIL #1 CVar spam"; fail=1; }
grep -q 'ProjectAbilitySet.*0 registered' "$L" && { echo "FAIL #6 AbilitySet"; fail=1; }
grep -qE '/HairStrands.*SkipPackage|StableRodsSystem.*does not exist' "$L" && { echo "FAIL #7 HairStrands"; fail=1; }
grep -q 'r.PathTracing:1' "$L" && { echo "FAIL #8 PathTracing on"; fail=1; }
grep -q 'Nanite instanced static mesh using World Position Offset not supported in ray tracing' "$L" && { echo "FAIL R3 Nanite+WPO+RT"; fail=1; }
awk '/LogProjectSave: Loading save from slot:/{p=1} p && (/ViewDistanceQuality@3/ || /TSR.History.ScreenPercentage:200/){exit 1}' "$L" || { echo "FAIL R1 TSR revert"; fail=1; }
grep -q 'Failed to keep memory budget' "$L" && { echo "WARN R4 Mutable still spilling - consider R6 Insights"; }
grep -q 'Loading a material resource None with an invalid ShaderMap' "$L" && { echo "WARN R2 ShaderMap still firing"; }
test $fail -eq 0 && echo "OK: closed sentinels clean"
```

---

## Symptoms

- Heavy lag/stutter outdoors on City17 map entry - indoor OK
- Lag subsides after some time (assets stream in / shaders compile)
- Returning outdoor causes lag again
- Running makes it significantly worse (more geometry enters view)

---

## Proven from Log

### 1. AnimNode CVar Spam (~72,534 warnings)

```
LogBlueprintUserMessages: Warning: Failed to find console variable 'a.animnode.offsetrootbone.enable'.
```

~72,534 occurrences in repeated pairs at very high frequency throughout the session, strongly suggesting per-frame or near-per-frame spam. The OffsetRootBone anim node queries a CVar that does not exist in this engine build. This warning is repeated at very high frequency and likely adds avoidable CPU/log overhead.

### 2. PSO Creation Hitches (line 2185)

```
LogPSOHitching: Encountered 50 PSO creation hitches so far (1 graphics, 49 compute). 33 of them were precached.
```

50 PSO hitches within seconds of map load. Only 33/50 were precached - 17 hitches are runtime pipeline state compilations that stall the game thread.

**Root cause config (lines 761-762):**

```
LogD3D12RHI: Not using pipeline state disk cache per r.D3D12.PSO.DiskCache=0
LogD3D12RHI: Not using driver-optimized pipeline state disk cache per r.D3D12.PSO.DriverOptimizedDiskCache=0
```

PSO disk cache is **disabled**, so the build cannot benefit from persisted PSO reuse between sessions.

### 3. Mutable Memory Budget Overflow (lines 2325-2338)

```
LogMutableCore: Failed to keep memory budget. Budget: 102400, Current: 93228, New: 65535
LogMutableCore: Failed to keep memory budget. Budget: 102400, Current: 158721, New: 48
LogMutableCore: Failed to keep memory budget. Budget: 102400, Current: 180621, New: 65535
```

Mutable character build overflows 100MB budget by ~80%, peaking at 180MB. This happens during character mesh generation shortly after map load.

### 4. Loading Pipeline - No Asset Preload (lines 1317-1318)

```
LogProjectLoading: Warning: Phase 3: Preload Critical Assets - NO ASSETS TO PRELOAD
LogProjectLoading: Warning: Phase 3: This means map will load during Travel phase (causes freeze)
```

Map assets are not preloaded - all loading happens synchronously during travel, blocking the game thread.

### 5. Invalid ShaderMap Material (line 1806)

```
LogMaterial: Error: Loading a material resource None with an invalid ShaderMap!
```

A material failed to load its compiled shaders - causes fallback to default material (black/wrong surfaces) and possible runtime recompile stall.

### 6. GlobalAssetScan Failure (line 1174)

```
LogProjectLoading: Error: EnsureGlobalAssetScans: VERIFICATION FAILED - mandatory type 'ProjectAbilitySet' has 0 registered assets after scan.
```

AbilitySet assets not found in cooked build. Gameplay features depending on ability sets may silently fail to initialize.

### 7. HairStrands Missing Package (line 2202)

```
LogStreaming: Warning: LoadPackage: SkipPackage: /HairStrands/Emitters/StableRodsSystem - The package to load does not exist on disk
```

Missing Niagara system. Impact unclear - may cause a single async stall or repeated retries.

---

## Graphics Quality / Config Concerns

These are not proven performance hits, but increase hitch sensitivity:

```
(line 387)  r.setres:1280x720              -- startup resolution override, may not be intentional
(line 425)  r.PathTracing:1                 -- path tracing enabled in Shipping (usually unnecessary)
(line 491)  r.TSR.History.ScreenPercentage:200  -- TSR supersampling at 200% (heavy)
```

Path tracing enabled in Shipping is unusual - adds GPU overhead even if not actively used. TSR at 200% history doubles internal resolution. Combined with ray tracing + Lumen HW RT + 8GB texture pool, this config is hitch-sensitive.

### Nanite + WPO + RT Foliage (line 2177)

```
LogStaticMesh: Warning: Nanite instanced static mesh using World Position Offset not supported in ray tracing yet
```

Affects `SM_Tree_Hornbeam_Medium`. Quality issue (wrong RT reflections/shadows), not a crash.

---

## Hypotheses (need code verification)

| Hypothesis | Why plausible | What to verify |
|------------|--------------|----------------|
| CVar spam causes measurable frame cost | 72K warnings at very high frequency likely add avoidable CPU and logging overhead | Profile with/without the spam |
| Mutable overflow causes GC pressure during outdoor entry | Overflow timing correlates with map load | Check if GC runs coincide with frame drops |
| PSO misses cause the "stutter that fades" pattern | Uncached PSOs compile on first encounter | Enable disk cache, compare second-run behavior |
| Empty preload causes initial freeze | Log explicitly says "causes freeze" | Wire preload list, measure travel phase |

---

## Likely Cause Ranking

### Direct runtime cost (proven)

| # | Cause | Impact | Fix Effort | Status |
|---|-------|--------|------------|--------|
| 1 | **CVar spam (72K warnings/session)** | **High** - CPU + log I/O, buries real issues | Register CVar | VERIFIED |
| 2 | PSO cache + precache disabled | **High** - 17+ runtime pipeline compiles per session | Config change | VERIFIED |
| 3 | No asset preloading in Phase 3 | **Medium** - sync map load freeze | Serialize deps | VERIFY on City17 |

### Likely runtime cost (needs verification)

| # | Cause | Impact | Fix Effort | Status |
|---|-------|--------|------------|--------|
| 4 | Mutable memory budget overflow | **High** - 80% over budget, likely GC pressure | Increase budget | VERIFIED (MainMenu) |
| 5 | Invalid ShaderMap material | **Medium** - wrong surfaces + possible recompile stall | Resave/recook material | VERIFIED |
| 6 | AbilitySet scan failure | **Low** - unknown cascade | Remove scan spec | VERIFIED |
| 7 | Missing HairStrands package | **Low** - unclear stall impact | Cook plugin content | VERIFIED |

### Quality / config concerns

| # | Cause | Impact | Fix Effort | Status |
|---|-------|--------|------------|--------|
| 8 | PathTracing enabled in Shipping | **Medium** - unnecessary GPU overhead | Config change | VERIFIED |
| 9 | TSR 200% history | **Medium** - doubles internal resolution | First-run clamp | VERIFIED (@3->@2) |
| 10 | r.setres:1280x720 override | **Low** - no first-run auto-detect | First-run auto-detect | VERIFIED (1920x1080) |
| 11 | Nanite+WPO+RT foliage | **Low** - quality only | Asset fix | VERIFIED |

---

## Recommended Actions

### FIXED (verified by code/editor change, no repackage needed to confirm)

1. **CVar spam killed** - registered `a.animnode.offsetrootbone.enable` in `GASPConsoleVariables.cpp`.
   ABP Chooser Tables query this exact name every tick; without registration, 72K warnings/session.

5. **Invalid ShaderMap material** - null materials found and assigned in editor (8 mesh components).

6. **AbilitySet scan removed** - removed scan spec registration from `ProjectGASModule.cpp`.
   All current GAS effects (GE_ThresholdDebuff_*, GE_ConditionRegen, etc.) are applied via C++,
   not through AbilitySet data assets. No assets exist at `/ProjectGAS/AbilitySets` yet.
   DefaultGame.ini still has the PrimaryAssetTypesToScan entry (CookRule=AlwaysCook) so any
   future authored assets cook without code changes. Re-add scan spec with `bRequireNonEmpty=true`
   when DefinitionGenerator starts creating AbilitySet .uassets.

11. **SM_Tree_Hornbeam_Medium** - "Support Ray Tracing" disabled on asset in editor.

### IMPLEMENTED - verify in next Shipping build

2. **PSO precaching + disk cache enabled** in `DefaultEngine.ini [ConsoleVariables]`:
   - `r.PSOPrecache.Resources=1` - resource-based precaching during postload (first-run help)
   - `r.PSOPrecache.ProxyCreationWhenPSOReady=1` - delay proxy until PSO compiled (no pop-in)
   - `D3D12.PSO.DiskCache=1` - persist compiled PSOs across sessions (second+ run help)
   - `D3D12.PSO.DriverOptimizedDiskCache=1` - driver-optimized disk cache
   Note: D3D12.* CVars take no `r.` prefix; render-layer ones do.
   Precaching reduces first-encounter hitches; disk cache eliminates recompilation on later runs.
   If outdoor hitches persist after repackage, test `D3D12.PSOPrecache.KeepLowLevel=1`.

3. **Phase 3 preloading for Shipping** - added `[AssetRegistry] bSerializeDependencies=True`
   in `DefaultEngine.ini`. Cooked builds should now include package dependency data so
   `DiscoverMapDependencies()` can find map assets at runtime. Executor code was already enabled.
   **Risk:** UE docs do not explicitly confirm this is the correct lever for cooked-runtime deps.

4. **Mutable budget raised** - `mutable.WorkingMemory=262144` (256 MiB, was 210 MiB after the first fix).
   This gives ~7% headroom over the observed City17 pending demand
   (Current 180621 + New 65535 = 246156 KiB, ~240 MiB).
   R6 remains conditional: capture Unreal Insights Mutable channels only if
   future logs still show budget spills. The working-memory limit is a flush
   hint, not a hard cap.

7. **HairStrands cook rule added** - `+DirectoriesToAlwaysCook=(Path="/HairStrands")`
   in `DefaultGame.ini`. Missing Niagara system `StableRodsSystem` should now cook.

8. **PathTracing disabled** - `r.PathTracing=0` in `DefaultEngine.ini`
   (numeric form; canonical for CVars and matches UE log output).
   Was enabled "for cinematics" but adds GPU overhead in all Shipping sessions.
   Can be re-enabled at runtime via console.

9-10. **First-run auto-detection** - added `EnsureFirstRunDefaults()` in `AlisGI::Init()`:
    - Detects first run via `LastConfirmedScreenResolution == 0x0`
    - Sets desktop resolution via `FDisplayMetrics` + windowed fullscreen
    - Runs `RunHardwareBenchmark()` + `ApplyHardwareBenchmarkResults()`
    - Clamps all quality to High (@2) max - safe starting point, users raise manually
    - High = TSR History 100% (not 200%), reasonable shadow/GI/reflection cost
    - Saves to `GameUserSettings.ini` so subsequent launches use saved settings
    - Runs at GameInstance::Init, before any map load or render

---

## Engine-Backed Fix Directions

### PSO Stutter - use the full UE pipeline, not only disk cache

Use Unreal's full PSO precache pipeline, not a single toggle.

**Standard:**
- Keep `r.PSOPrecaching=1` (default).
- Keep component precache on.
- Test `r.PSOPrecache.Resources=1` for City17 or other outdoor-heavy maps if memory budget allows.
- Enable persisted D3D12 caches (D3D12.* CVars take no `r.` prefix):
  - `D3D12.PSO.DriverOptimizedDiskCache=1`
  - `D3D12.PSO.DiskCache=1`
- If second-run stutter remains, test:
  - `D3D12.PSOPrecache.KeepLowLevel=1`
  - `r.PSOPrecache.KeepInMemoryUntilUsed`
  - `r.PSOPrecache.KeepInMemoryGraphicsMaxNum`
  - `r.PSOPrecache.KeepInMemoryComputeMaxNum`

**Project research:**
- Check whether ALIS disabled only disk caches, or also effective PSO precache coverage for outdoor resources.
- Check whether City17 outdoor assets are entering view before their PSOs are compiled.
- Capture representative packaged playthroughs (not editor play) and compare first-run vs second-run hitch behavior.

**Design rule:** PSO policy is per-platform and per-content-class, not per-bug. Outdoor maps, high-variant materials, and Mutable-driven character views need an explicit PSO strategy.

Ref: [PSO Precaching](https://dev.epicgames.com/documentation/unreal-engine/pso-precaching-for-unreal-engine)

### Empty Preload Stage - deterministic preload ownership

UE Asset Manager model: primary assets must be scanned first; `GetPrimaryAssetData` only works for already-scanned assets. `RequestSyncLoad` can stall the game thread for seconds; `RequestAsyncLoad` exists to avoid that and keeps handles managed until release.

**Standard:**
- Mandatory gameplay data must be registered as Primary Assets before gameplay starts.
- Populate preload lists from Asset Manager scan results, not from ad hoc lazy discovery during travel.
- Load with Asset Manager / Streamable handles before travel.
- Do not use sync loads in gameplay or UI paths.

**Project research:**
- Inspect `UAssetManager::StartInitialLoading` in ALIS and verify all mandatory types are scanned there.
- Verify `CriticalAssetIds` / `CriticalSoftPaths` are built from scanned asset data, not from runtime object lookups.
- Search for `RequestSyncLoad`, `LoadSynchronous`, `TryLoad` in loading, UI, and inventory paths.

**Design rule:** Split into: (1) registration - what exists, (2) preload policy - what must be resident before a phase, (3) residency - what stays loaded and for how long. Do not mix those three concerns in one cache class.

Ref: [Asset Management](https://dev.epicgames.com/documentation/unreal-engine/asset-management-in-unreal-engine)

### AbilitySet Scan Failure - fail fast at boot

If something is a required gameplay asset, it should be a primary asset with explicit scan rules and explicit cook rules.

**Standard:**
- `ProjectAbilitySet` must be treated as mandatory gameplay data, not opportunistic content.
- Give it explicit Primary Asset registration and explicit cook rules.
- Shipping boot should verify mandatory primary asset types are registered before map travel.

**Project research:**
- Verify scan root, base class, and type name for `ProjectAbilitySet`.
- If ALIS uses Game Features, verify the type is declared in `UGameFeatureData::GetPrimaryAssetTypesToScan()`.
- Verify production cook rule is not `NeverCook` / `ProductionNeverCook`.

**Design rule:** A shipping build should fail fast on missing mandatory primary asset types, not discover it during first ability use.

Ref: [Asset Management](https://dev.epicgames.com/documentation/unreal-engine/asset-management-in-unreal-engine), [GameFeatureData::GetPrimaryAssetTypesToScan](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/GameFeatures/UGameFeatureData/GetPrimaryAssetTypesToScan/1)

### Mutable Runtime - budget from measurements, not guesswork

Epic's Mutable docs: runtime generation uses CPU, working memory, and disk streaming bandwidth. The working memory limit is not a hard cap and can be set with `mutable.WorkingMemory`. Epic recommends using Unreal Insights Mutable channels to understand runtime cost. Mutable runtime is marked Beta.

**Standard:**
- Measure real peaks with Unreal Insights before changing the memory budget.
- Size `mutable.WorkingMemory` from observed outdoor-entry peaks plus safety margin.
- Do not force first-time character generation at the same moment as outdoor scene reveal if it can be warmed earlier.

**Project research:**
- Capture an outdoor-entry Insights session with Mutable CPU and memory channels enabled.
- Check whether outdoor hitches correlate with first character build or first texture streaming requests from Mutable.
- Check whether character generation can be moved to loading screen, shelter, spawn prep, or a hidden warmup window.

**Design rule:** Mutable should be treated as a scheduled runtime job with explicit budget and timing, not as invisible side work during a critical camera reveal.

Ref: [Mutable Resource Usage at Runtime](https://dev.epicgames.com/documentation/en-us/unreal-engine/mutable-resource-usage-at-runtime-in-unreal-engine)

### Shipping Render Defaults - conservative by default

Epic's docs: Path Tracer is the offline path for Movie Render Queue / high-quality frame rendering. WPO in ray tracing is expensive (dynamic BLAS rebuilds); UE ignores WPO in RT by default. Nanite docs recommend clamping Max World Position Offset Displacement.

**Standard:**
- `r.PathTracing` should be off in normal gameplay shipping builds unless explicitly required.
- Mass foliage should not opt into expensive ray-tracing WPO paths by default.
- Clamp material `Max World Position Offset Displacement` on Nanite foliage materials.

**Project research:**
- Audit ALIS shipping config for `r.PathTracing`, TSR history scale, and any debug-quality overrides.
- For `SM_Tree_Hornbeam_Medium`, inspect: component/foliage visibility in RT, Evaluate WPO in RT, material WPO magnitude, foliage WPO disable distance.

**Design rule:** The shipping renderer should default to stable gameplay rendering. Expensive offline-quality or debug-quality paths must be explicitly opted in per platform.

Ref: [Ray Tracing Features](https://dev.epicgames.com/documentation/unreal-engine/ray-tracing-and-path-tracing-features-in-unreal-engine)

### OffsetRootBone Spam - project bug, not engine feature toggle

`FAnimNode_OffsetRootBone` is documented as **experimental**. The Game Animation Sample lists known issues. No official documentation exists for `a.animnode.offsetrootbone.enable` as a shipping control.

**Standard:**
- Treat the missing CVar lookup as a project bug.
- Do not query undocumented / absent CVars every update in Shipping.
- If OffsetRootBone is only used for experimental motion matching support, gate it behind a shipping-safe default and fail silently when the control is absent.

**Project research:**
- Search ALIS animation blueprints, animation libraries, and support modules for the exact CVar string.
- Check whether the spam comes from a Blueprint utility, custom anim node wrapper, or a copied sample graph.
- Verify whether OffsetRootBone is actually needed in the active shipping locomotion path.

**Design rule:** Experimental animation nodes do not get invisible shipping dependencies. If a feature is optional, its shipping behavior must be deterministic when the controlling CVar or plugin is absent.

Ref: [FAnimNode_OffsetRootBone](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/AnimationWarpingRuntime/BoneControllers/FAnimNode_OffsetRootBone), [Game Animation Sample](https://dev.epicgames.com/documentation/en-us/unreal-engine/game-animation-sample-project-in-unreal-engine)

---

### Build-Time Prevention

**Standard:**
- Validate missing soft references at cook time (HairStrands, invalid ShaderMap sources).
- Validate shipping render config - flag `r.PathTracing=1` and debug-quality overrides in production ini.
- Fail cook on mandatory asset type absence (`ProjectAbilitySet` with 0 scanned assets).
- Add automated packaged first-run / second-run hitch smoke test to CI.

**Design rule:** Shipping performance problems that are detectable at build time should not survive to runtime.

Ref: [Data Validation](https://dev.epicgames.com/documentation/unreal-engine/data-validation-in-unreal-engine)

**Scripts and Makefile targets (2026-04-17):**

| Script | Purpose | Needs Editor | `make` target | In `make check` |
|--------|---------|-------------|---------------|-----------------|
| `scripts/ue/check/config/validate_shipping_ini.py` | Ini audit (PSO, PathTracing, TSR, deps) | No | `check-config` | Yes |
| `scripts/ue/check/assets/check_primary_assets.py` | Asset Manager type completeness | No | `check-primary-assets` | Yes |
| `scripts/ue/check/assets/validate_soft_refs.bat` | Null materials / broken mesh refs | Yes | `check-refs` | No (slow) |
| `scripts/ue/test/smoke/packaged_boot_test.ps1` | Post-package hitch smoke test | No (exe) | `test-package EXE=...` | No (post-build) |

- `make check` runs: check-config, check-primary-assets, check-uht, check-blueprints, check-assets
- `make check-refs` is opt-in (needs editor, ~30-60s)
- `make test-package EXE=<path>` runs packaged Shipping build and checks PSO hitches, Mutable overflow, CVar spam

---

## Code Research Checklist

```text
1. DONE - Verified scan specs in ProjectGASModule, GlobalAssetScanRegistry, InitialExperienceLoader.
   Removed premature AbilitySet scan. DefaultGame.ini has PrimaryAssetTypesToScan entries.

2. DONE - ALIS does not use Game Features for asset scanning. Uses GlobalAssetScanRegistry.

3. DONE 2026-04-22 (audit) - see "Sync-load audit (2026-04-22)" section below.
   Conversions to async loads deferred (buildable C++).

4. DONE - CVar registered in GASPConsoleVariables.cpp. ABP queries it by exact name.

5. DONE - PathTracing=False, PSO disk cache + precache enabled, TSR clamped via first-run High cap,
   SM_Tree_Hornbeam_Medium RT disabled.

6. NOT DONE - Insights capture with Mutable CPU/memory channels on outdoor entry. See R6 above; required to right-size R4 (Mutable budget overflow) from data.

7. DONE - CI validators wired:
   - shipping config flags -> `make check-config` (in `make check`)
   - primary asset presence -> `make check-primary-assets` (in `make check`)
   - null materials/soft refs -> `make check-refs` (opt-in, needs editor)
   - packaged hitch smoke -> `make test-package EXE=...` (post-build)
```

---

## Key Log Lines

| Line | Category | Detail |
|------|----------|--------|
| 2126+ | CVar spam | ~72,534x `a.animnode.offsetrootbone.enable` |
| 761-762 | PSO cache disabled | `r.D3D12.PSO.DiskCache=0` |
| 2185 | PSO hitches | 50 hitches, 17 uncached |
| 2325-2338 | Mutable overflow | Budget 102400, peak 180621 |
| 1317-1318 | No preload | Phase 3 skipped |
| 1806 | Invalid ShaderMap | `Loading a material resource None with an invalid ShaderMap!` |
| 387 | r.setres override | `r.setres:1280x720` at startup |
| 425 | PathTracing on | `r.PathTracing:1` in Shipping |
| 491 | TSR 200% | `r.TSR.History.ScreenPercentage:200` |
| 1174 | AbilitySet missing | 0 assets after scan |
| 2202-2203 | HairStrands missing | Package not on disk |
| 2177 | Nanite+WPO+RT | Hornbeam tree incompatibility |

---

## Sync-load audit (2026-04-22)

Checklist item #3 completed. Audit scope: `Plugins/Systems/ProjectLoading/**`,
`Plugins/UI/Project{InventoryUI,UI,UIFramework}/**`,
`Plugins/Features/ProjectInventory/**`, `Plugins/Resources/ProjectObject/**`,
`Plugins/Foundation/ProjectCore/**`. Test modules excluded. 15 total hits
(12 `LoadSynchronous`, 2 `TryLoad`, 1 `RequestSyncLoad`).

### HOT call sites (likely contributors to outdoor lag)

All hot sites are in `Plugins/Resources/ProjectObject/Source/ProjectObject/Private/Spawning/ObjectSpawnUtility.cpp` inside `SpawnFromDefinition`:

| File:Line | Load target | Why HOT |
|---|---|---|
| ObjectSpawnUtility.cpp:823 | mesh asset | primary actor spawn path; per-actor stall |
| ObjectSpawnUtility.cpp:873 | groom binding asset | already flagged in the initial investigation; each groom-enabled NPC stalls a frame |
| ObjectSpawnUtility.cpp:950 / 987 / 1022 | skeletal anim class | per skeletal/anim/groom component on spawn |
| ObjectSpawnUtility.cpp:1088 | material override | per material slot |
| ObjectSpawnUtility.cpp:1327 | post-process material | per trigger volume with PP effect |
| InteractableActor.cpp:372 / 415 / 435 | mesh / anim class / material override | `ApplyDefinition_Implementation`; unknown frequency (ticked vs one-shot) |
| ProjectInventoryComponent.cpp:3411 | ability-set (TryLoad) | `Internal_EquipItem`; per equip action |

### COLD call sites (kept for reference; NOT lag contributors)

- `ObjectSpawnUtility.cpp:347` (`GetLootProfile` fallback), `:1751` (`SpawnFromDefinition` public overload fallback)
- `Plugins/Systems/ProjectLoading/Source/ProjectLoading/Private/Experience/InitialExperienceLoader.cpp:85` - warmup `RequestSyncLoad` at startup
- `Plugins/UI/ProjectUI/Source/ProjectUI/Private/ProjectUIThemeManager.cpp:79` - theme change
- `Plugins/UI/ProjectUI/Source/ProjectUI/Private/Subsystems/LoadingScreenSubsystem.cpp:86` - level-load widget bootstrap
- `Plugins/Systems/ProjectLoading/Source/ProjectLoadingMoviePlayer/Private/Subsystems/ProjectLoadingMoviePlayerSubsystem.cpp:388` - movie player init

### Recommended conversions (deferred; buildable C++)

Priority order for the next warm-build session:

1. `ObjectSpawnUtility.cpp:873` (groom binding) - already flagged; clearest win.
2. `ObjectSpawnUtility.cpp:823` (mesh asset) - primary spawn path.
3. `ObjectSpawnUtility.cpp:1088/1327` (materials) - per-slot compounds.
4. `InteractableActor.cpp:372/415/435` - determine call frequency first; if one-shot on map enter, convert to async with a brief gate.
5. `ProjectInventoryComponent.cpp:3411` (ability-set) - low priority; cheap per call, only on equip.

Pattern: `FStreamableManager::RequestAsyncLoad` + completion delegate that
finishes the spawn/apply step once loaded. Follow the existing
`UObjectDefinitionCache` residency pattern already in use for object
definitions (see `inventory_stacks_dragdrop_shipping.md` "Implemented"
section for the prior cache-ownership cleanup - same design applies here).
