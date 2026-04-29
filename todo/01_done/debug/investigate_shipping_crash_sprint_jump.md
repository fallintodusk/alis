# Investigate Shipping crash: sprint+jump in City17

Status: **RESOLVED 2026-04-28**. Build C (`r.RayTracing.Geometry.InstancedStaticMeshes=0` + WindowedFullscreen mapping in `ProjectSettingsService.cpp` and `ProjectSaveSubsystem.cpp`) verified on the same City17 sprint+jump repro under a packaged Shipping build with `-gpucrashdebugging` (Aftermath active). Fix landed in commit **f8e8ffab7**. Visual cost on foliage Lumen GI tracked separately in [improve_foliage_lumen_quality.md](improve_foliage_lumen_quality.md).

External plan + full review trail: `<user-home>/.claude/plans/investigate-carefully-in-plan-cached-fiddle.md`

## Resolution (2026-04-28)

Verified by repackage + replay of the same scenario:

| Marker | Original crash run (14:48 local) | Build C verification run (16:03-16:06 local) |
| :--- | :--- | :--- |
| Total session | 1 min 46 s, ended mid-frame at frame `[93]` | 2 min 36 s, clean exit at frame `[979]` |
| `LogExit: Exiting.` | absent | present (clean shutdown path) |
| `Swapchain Resized` during gameplay | 4 in 3 seconds | zero |
| Frame counter | wrapped `[950] -> [66]` (D3D device reset) | monotonic 0 -> 979 |
| Worst gameplay-time SBT delta | `3072 -> 5120` (+2048 segments), 24 s, ~3 fps -> stall + death | `2560 -> 4608` (+2048 segments), 19 s, ~19 fps, no stall |
| Aftermath active | n/a (`r.GPUCrashDebugging=0`) | yes, `EnableResourceTracking` (`-gpucrashdebugging` flag) |
| `*.nv-gpudmp` written | no | no (none needed) |
| Event Viewer 4101 / 117 / 141 / 142 in test window | n/a | none |
| Process exit code | killed by OS (silent) | 0 |

The decisive piece of evidence: identical magnitude SBT growth (+2048 segments) ran cleanly in Build C in 19 s with normal frame advance, vs. stalling the GPU for 24 s at ~3 fps in the original. Same SBT slot count, much less GPU work behind each segment because instanced static mesh / HISM BLAS-fanout no longer participates in the RT scene.

## What actually fixed it (attribution)

**Step 2 (`r.RayTracing.Geometry.InstancedStaticMeshes=0`) did the heavy lifting.** Removing ISM/HISM from the RT scene cut the per-instance BLAS / refit cost that was dominant in foliage / clutter tiles (HISM clusters can carry thousands of instances per asset). Same SBT growth, fraction of the GPU work.

**Step 1 (WindowedFullscreen mapping) was untested-but-kept insurance.** Because Step 2 prevented any stall, the run never reached the path where DWM would have force-broken exclusive fullscreen. WindowedFullscreen stays as the project default — if a future stall happens, it removes the swapchain mode-switch cascade from the failure chain. Microsoft's DXGI flip-model docs confirm modern borderless is at parity with FSE, so no perf cost.

Step 3 (diagnostic profile via `-gpucrashdebugging`) was visibility, not remediation. Aftermath was active and watching but had nothing to capture.

## Out of scope (separate follow-ups)

- **Foliage Lumen GI quality regression** under ISM-RT-off — tracked in [improve_foliage_lumen_quality.md](improve_foliage_lumen_quality.md).
- **Build D attribution experiment** (`r.RayTracing.PersistentSBT=0`) was not run because Step 2 alone was sufficient. Document the option in the foliage todo if we ever want to try recovering RT visuals on a subset of meshes.
- **Build E asset-level surgical cleanup** (per-mesh "Visible in Ray Tracing = false") not pursued — broader CVar landed first and is sufficient. Asset-level work is now optional polish, gated on whether the foliage todo wants finer control.

## What happened

Date: 2026-04-28, ~14:48-14:49 local (Moscow, UTC+3). Build: public release `ALIS_20260428_130503`, target `Alis-Win64-Shipping`.

Reproduction (1 minute 46 seconds total):

1. Launched packaged Shipping build into MainMenu, clicked Play -> City17.
2. Walked around picking up interactables (Backpack, Crowbar, Apartment Key, Cigarette).
3. Sprinted (Shift) and jumped (Space) around the area for ~30 s.
4. App closed silently. No `CrashContext.runtime-xml`, no minidump, no UE crash reporter dialog.

Logs: `<local-app-data>/Alis/Saved/Logs/`
- `Alis.log` (369 KiB, 3136 lines) - the gameplay session.
- `Alis_2.log` (181 KiB, 1709 lines) - second instance launched at 14:49:40, sat in main menu (the user / launcher restarted after the death).

Latest crash dumps in `Saved/Crashes/` are from 2026-04-17 - nothing from this incident.

## Evidence chain (Alis.log)

| Line | Local time | Frame | Event |
| ---: | :--- | ---: | :--- |
| 3122 | 14:49:17 | 795 | `LogRenderer: Recreating Persistent SBTs ... NumGeometrySegments: 3072 -> 5120` (RT shader binding table grew by 2048 segments while World Partition streamed new tile) |
| 3125 | 14:49:41 | **865** | First swapchain resize, Fullscreen 1->0. Frame counter only advanced **70 frames in 24 seconds = ~3 fps -> hard GPU stall** |
| 3128 | 14:49:42 | 950 | Fullscreen 0->1 (engine restored) |
| 3131 | 14:49:43 | **66** | Fullscreen 1->0. **Frame counter wrapped 950 -> 66**, consistent with D3D device removal/reset (not yet proof - need DXGI_ERROR_DEVICE_REMOVED / DRED / Event ID 4101 confirmation) |
| 3134 | 14:49:44 | 93 | Fullscreen 0->1 (final line, process killed shortly after) |

No `Fatal:`, no `RequestExit`, no `LowLevelFatalError`, no shutdown sequence in the tail. Material-error at line 1946 (`Loading a material resource None with an invalid ShaderMap!`) is benign cook artifact during MainMenu->City17 transition; not the cause.

## Highest-confidence failure chain (not yet proven; web research strongly supports it)

**Wording rule for this todo: "highest-confidence failure chain", not "proven root cause".** Final proof requires Event Viewer 4101 / 117 / 141 / 142, `DXGI_ERROR_DEVICE_REMOVED`, DRED, or an Aftermath dump from a repro build with `-gpucrashdebugging`.

Working chain: GPU stall during hardware ray tracing scene update (persistent SBT rebuild while World Partition streamed in heavy ISM/HISM tiles in City17) -> Windows TDR / device-reset path -> exclusive-fullscreen mode-switch storm on top of that -> silent process death before UE reached its fatal handler. The 4 swapchain resizes are a *symptom* of GPU stall, not a code bug in the project's fullscreen / settings paths.

External evidence (web research, 2026-04-28):

- Epic Developer Community Forums, "GPU crash in RayTracingScene" - **Epic staff explicitly suggest testing `r.RayTracing.PersistentSBT=0`** for hangs related to persistent SBTs and streaming. Our log line `Recreating Persistent SBTs ... NumGeometrySegments 3072 -> 5120` is exactly this path. Strongest external lead.
- Epic Developer Community Forums, "Unreal 5.7 RayTracing Crash" - active thread, foliage / Nanite / WPO / cached RT geometry implicated in 5.7.1; 5.7.2 adds more logging for this class. Community confirmation that scenes with transparency / foliage are over-represented.
- Epic Hardware Ray Tracing documentation - significant scene update cost above ~100 k instances (City17 + Megascans + WP streaming exceeds that).
- NVIDIA Developer - ISM/foliage are RT acceleration-structure cost multipliers; instance culling is the standard remediation.
- Microsoft Learn - WDDM TDR default timeout 2 s; resets graphics stack on GPU non-response. Registry-level TDR tweaks are testing/debugging tools, NOT application-level mitigation.
- Microsoft Learn - DXGI flip model: modern borderless/windowed is at parity with classic exclusive fullscreen; Windows fullscreen optimizations already run FSE games in optimized borderless mode. So WindowedFullscreen has no perf penalty.
- Epic - "Dealing with a GPU Crash When Using Unreal Engine" recommends `-gpucrashdebugging` (and `-nvaftermathall -gpucrashdebugging` + `GPUDebugCrash hang` to validate the dump path).

## Confirmed NOT the cause

Verified by reading:

- [`Plugins/UI/ProjectSettingsUI/Source/ProjectSettingsUI/Private/ProjectSettingsService.cpp:194-257`](../../Plugins/UI/ProjectSettingsUI/Source/ProjectSettingsUI/Private/ProjectSettingsService.cpp) - `ApplyGraphics` only fires on Settings UI Apply. User was in gameplay, not Settings.
- [`Plugins/Systems/ProjectSave/Source/ProjectSave/Private/ProjectSaveSubsystem.cpp:332-364`](../../Plugins/Systems/ProjectSave/Source/ProjectSave/Private/ProjectSaveSubsystem.cpp) - `ApplyGameSettings` only fires on save-load (boot, already done at 14:48:19, single resize logged then).
- [`Source/Alis/Private/AlisGI.cpp:18-77`](../../Source/Alis/Private/AlisGI.cpp) - `EnsureFirstRunDefaults` runs once and gates on `LastCPUBenchmarkResult < 0`. Skipped on every run after the first.
- Motion Matching plugin (`Plugins/ThirdParty/MotionMatching/`, `Plugins/Gameplay/ProjectSkeletalCapabilities/`): zero render-state side effects from Sprint/Jump. All `DDCvar.*` are `ECVF_Default`. Sprint handler is trivial `bIsSprinting = true; RefreshMovementSpeed();`.
- `ProjectVitals` stamina/sprint cost: no rendering changes (no motion blur / camera shake / post-process toggle on stamina exhaustion).

## Likely contributing factor

Recent commit `0b4f34121` ("disable rt on the foliage") only disabled RT visibility on **2 tree species** (Amur Cork, Hornbeam) across **5 City17 cells**. Most foliage and other instanced static meshes (HISM/ISM clutter) are still in the RT scene. Combined with `r.RayTracing=1` and `r.Lumen.HardwareRayTracing=1` (verified in `Config/DefaultEngine.ini:126,138` and reaffirmed by log line 865 `Ray tracing is enabled (dynamic). Reason: r.RayTracing=1 and r.RayTracing.EnableOnDemand=1`), every streamed-in instance triggers BLAS build + TLAS rebuild + SBT growth.

## Mitigation landed (committed to working tree, not yet committed to git)

Order matches user-approved direction. Honest claim per step is qualified — none of these promise "no crash"; they each remove a specific layer from the failure chain.

1. **WindowedFullscreen everywhere** - changes the project's "Fullscreen" toggle to map to `EWindowMode::WindowedFullscreen` instead of exclusive `EWindowMode::Fullscreen`. Two C++ sites:
   - `Plugins/UI/ProjectSettingsUI/Source/ProjectSettingsUI/Private/ProjectSettingsService.cpp` (the `ApplyGraphics` fullscreen branch)
   - `Plugins/Systems/ProjectSave/Source/ProjectSave/Private/ProjectSaveSubsystem.cpp` (the `ApplyGameSettings` mode mapping)
   `AlisGI.cpp:46` already uses `WindowedFullscreen` for first-run; this aligns the runtime / save-load paths. Honest claim: removes exclusive-fullscreen mode switching from the failure chain. Does NOT promise survival of a real GPU hang. Microsoft DXGI flip-model docs say borderless is at parity with classic FSE, so no perf penalty.
2. **Pull ISM/HISM out of RT scene** - one line in `Config/DefaultEngine.ini`:
   ```ini
   r.RayTracing.Geometry.InstancedStaticMeshes=0
   ```
   Broad impact (all instanced static meshes, not only foliage). The only step in the plan that targets the root cause directly. Visual regressions to verify on the City17 test loop before locking in.

## Diagnostic options (NOT committed; swap in for repro only)

Two separate diagnostic experiments to run from a local override, not from `Config/DefaultEngine.ini`. Both go in `<local-app-data>\Alis\Saved\Config\Windows\Engine.ini`:

- **Persistent SBT off** (Epic-suggested for our exact log signature):
  ```ini
  [/Script/Engine.RendererSettings]
  r.RayTracing.PersistentSBT=0
  ```
  If repro disappears, persistent-SBT rebuild path is involved (Epic's hypothesis on the public RT-scene crash thread). If repro persists, problem is below the persistent-SBT layer.

- **GPU crash debugging** (visibility, not remediation):
  ```ini
  [/Script/Engine.RendererSettings]
  r.GPUCrashDebugging=1
  r.D3D12.BreadCrumbs=1
  ```
  Or just pass `-gpucrashdebugging` (and optionally `-nvaftermathall`) on the launch command line. Goal: emit `*.nv-gpudmp` / `LowLevelFatalError` instead of silent termination on the next stall. Verify `r.D3D12.BreadCrumbs` default in 5.7 first.

Tear down: delete the local override before the next play session.

## Bisect test matrix

| Build | Local change | What it answers |
| :--- | :--- | :--- |
| **A — Baseline** | revert the 3 working-tree changes (WindowedFullscreen + ISM RT off) | Confirm repro still 100 % reliable. |
| **B — Borderless only** | Step 1 only (WindowedFullscreen) | Does removing exclusive-FS from the chain change the failure mode (cleaner device-removed log)? |
| **C — ISM/HISM RT off** | Step 1 + Step 2 (current working tree) | Fastest broad mitigation. If repro disappears, RT scene cost from instanced meshes was the dominant factor. |
| **D — Persistent SBT off** | Step 1 + Saved/Config override `r.RayTracing.PersistentSBT=0` | Tests Epic's exact hypothesis. |
| **E — Asset-level RT cleanup** | Step 1 + asset-by-asset "Visible in Ray Tracing = false" on every `Content/.../SM_*` foliage / clutter mesh | Surgical fallback if Build C visuals are unacceptable. |

Strongest plausible final shipping config: **B + C**. **D** is purely diagnostic. **E** replaces **C** if visual cost is rejected.

## Diagnostic recipe (for the next reproduction)

For the repro session, do **not** edit the public `Config/DefaultEngine.ini`. Two cleaner options:

Option A - launch arg only (recommended, leaves the repo clean):

```bat
cd /d C:\path\to\packaged\ALIS_<date>\Windows
Alis-Win64-Shipping.exe -gpucrashdebugging
```

Option B - if `-gpucrashdebugging` isn't enough, drop a *throwaway* override into the user's local Saved/Config without touching the source tree:

```ini
; <local-app-data>\Alis\Saved\Config\Windows\Engine.ini
[/Script/Engine.RendererSettings]
r.GPUCrashDebugging=1
r.D3D12.BreadCrumbs=1
```

Run the City17 sprint+jump reproduction. Expected new artifacts on stall:

- `*.nv-gpudmp` next to `Alis.log` (Aftermath dump),
- `LowLevelFatalError` line in `Alis.log` instead of mid-frame truncation,
- `LogD3D12RHI` lines mentioning `DXGI_ERROR_DEVICE_REMOVED` / `DRED` if device-removed path is reached.

Verify `r.D3D12.BreadCrumbs` default in 5.7 first - if already on, skip option B.

Tear down:
- Option A: just don't pass the arg next launch.
- Option B: delete the local `Engine.ini` override.

## Verification next time

After reproducing the original sprint+jump City17 route on a build with all 3 mitigations:

1. Inspect the new log:
   ```powershell
   Select-String -Path "$env:LOCALAPPDATA\Alis\Saved\Logs\Alis.log" `
     -Pattern "Recreating Persistent SBTs|GPU timeout|DeviceRemoved|DXGI_ERROR|Swapchain Resized|D3DERR_DEVICELOST"
   ```
   Success threshold: no `Swapchain Resized` outside boot phase; SBT growth events under a small delta (e.g. `B - A < 256`).
2. Inspect Windows Event Viewer for TDR / display reset events:
   ```powershell
   Get-WinEvent -LogName System -MaxEvents 300 |
     Where-Object { $_.Id -in @(4101,153,4102,141,117) } |
     Select-Object TimeCreated, Id, ProviderName, Message
   ```
   Event ID 4101 = "Display driver stopped responding and has recovered" (TDR fired). 141 / 117 often accompany hard resets.
3. Confirm whether a `*.nv-gpudmp` / `LowLevelFatalError` is now produced on stall (Step 3 success criterion).

## Open questions / next investigation

- Did Step 2 (`r.RayTracing.Geometry.InstancedStaticMeshes=0`) cause visible Lumen GI / reflection / shadow regressions in City17 cinematics? If yes, fall back to surgical asset-by-asset RT-visibility unticking on every foliage / ISM mesh.
- Is `r.D3D12.BreadCrumbs` already `1` by default in UE 5.7? If yes, drop it from the diagnostic ini block.
- Confirm the TDR hypothesis with the Event Viewer query above. If no Event ID 4101 fired, the silent termination has a different cause (memory corruption, stack overflow inside RT BLAS build, etc.) and the investigation widens.
- The recent commit `0b4f34121` is incomplete; if Step 2 has unacceptable visual cost, plan a sweep of all `Plugins/Resources/*/Content/.../SM_*` static meshes that are foliage/clutter to disable "Visible in Ray Tracing" individually.

## Authority (web research, 2026-04-28)

- Microsoft Learn - WDDM Support for Timeout Detection and Recovery (TDR): Windows resets the graphics stack when GPU work cannot complete/preempt in time (default 2 s). TDR registry knobs are debugging tools, not application-level mitigation.
- Microsoft Learn - DXGI flip model: modern borderless / windowed flip-model is at parity with (or better than) classic exclusive fullscreen.
- Epic Developer Community Forums - "GPU crash in RayTracingScene": **Epic staff suggest `r.RayTracing.PersistentSBT=0`** for hangs related to persistent SBTs and streaming. Direct match for our log.
- Epic Developer Community Forums - "Unreal 5.7 RayTracing Crash": active thread, foliage / Nanite / WPO / cached RT geometry implicated in 5.7.1; Epic confirms 5.7.2 adds more logging.
- Epic Hardware Ray Tracing documentation: significant scene update cost above ~100 k instances.
- NVIDIA Developer: ISM/foliage are RT acceleration-structure cost multipliers; instance culling is the standard remediation.
- Epic - "Dealing with a GPU Crash When Using Unreal Engine" recommends `-gpucrashdebugging` (and `-nvaftermathall -gpucrashdebugging` + `GPUDebugCrash hang` to validate the dump path).
- Epic Console Variables Reference - `r.D3D12.BreadCrumbs` is an existing GPU breadcrumb knob; verify default in 5.7 before adding to ini.
