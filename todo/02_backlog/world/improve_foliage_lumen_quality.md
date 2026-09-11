# Improve foliage Lumen quality after ISM/HISM RT disable

Status: open. Visual-quality follow-up to the [resolved City17 sprint+jump crash](../01_done/debug/investigate_shipping_crash_sprint_jump.md). The crash fix removed instanced static mesh / HISM geometry from the ray tracing scene, which fixed the GPU stall but visibly degraded foliage GI / reflections / shading. This todo recovers as much of that visual quality as possible without bringing the crash back.

## Context

Crash fix (commit `f8e8ffab7`) added to [`Config/DefaultEngine.ini`](../../Config/DefaultEngine.ini):

```ini
r.RayTracing.Geometry.InstancedStaticMeshes=0
```

That excludes all ISM/HISM geometry from the RT scene. Effect on foliage:

- **GI**: Lumen previously got high-quality lighting from HW RT on foliage. Now those meshes feed only into screen-space probes and the Global Distance Field. Visibly flatter, less occluded, less colored bounce. **The user reports "foliage in common looks bad as shit"**.
- **Reflections**: SSR fallback only. Off-screen foliage no longer reflects.
- **Shadows**: still fine, VSM is on (`r.Shadow.Virtual.Enable=1`).
- **Direct lighting**: unchanged (rasterizer path).
- **Path tracing**: unchanged, still off by default.

Important constraint already in `DefaultEngine.ini:128`: `r.Lumen.TraceMeshSDFs=0`. With ISM/HISM excluded from HWRT and Mesh SDF detail traces disabled, Lumen likely falls back to less precise screen-probe + Global Distance Field paths for much of the foliage contribution. This is a strong candidate for the severe visual regression, but should be verified with the **Lumen Scene / Mesh Distance Field / Global Distance Field visualizers** before any tuning lands. Also note: UE 5.7 deprecates SWRT detail-tracing in favor of HWRT, so Mesh SDF is a quality recovery, not a forward-looking primary path.

## Candidate experiments (cheapest first)

### Experiment 1 - flip TraceMeshSDFs back on (cheap diagnostic, not a guaranteed final path)

```ini
; ALIS foliage quality recovery test after disabling ISM/HISM HWRT.
; Safer than re-enabling ISM in HWRT, but profile dense foliage carefully -
; UE 5.7 treats Mesh SDF detail tracing as a legacy/deprecated path.
r.Lumen.TraceMeshSDFs=1
```

What it does: makes Lumen software tracing use Mesh Signed Distance Fields (per-asset) instead of falling back to the lower-resolution Global Distance Field. No per-instance fanout, so the HISM-crash path is not reopened.

Cost: lower crash risk than re-enabling HWRT for ISM/HISM, but the SDF detail trace can be **non-trivial in dense / overlapping Megascans scenes** - exactly our case. This is a diagnostic and partial quality-recovery attempt, not an automatic win. Profile (`stat lumen`, `stat gpu`, frame time delta) on the City17 test loop before keeping it.

Verification: same as the resolved crash baseline (process exits cleanly, no `Swapchain Resized` during gameplay, no GPU TDR Event ID 4101 / 117 / 141 / 142, no `*.nv-gpudmp`).

### Experiment 2 - controlled HWRT re-entry (combined CVar + asset-level cleanup)

Single experiment that **must run together** - asset-level "Visible in Ray Tracing" is meaningless while the global ISM CVar is `0`, so the two parts are inseparable.

#### 2a. Modern CVar namespace (UE 5.x - the project is 5.7)

```ini
; Re-enable ISM/HISM HWRT but with strict culling so distant/small
; instances stay out of the BLAS. Without culling, this reopens the
; original City17 crash path.
r.RayTracing.Geometry.InstancedStaticMeshes=1
r.RayTracing.Geometry.InstancedStaticMeshes.Culling=1
r.RayTracing.Geometry.InstancedStaticMeshes.CullClusterRadius=5000
r.RayTracing.Geometry.InstancedStaticMeshes.CullClusterMaxRadiusMultiplier=5
r.RayTracing.Geometry.InstancedStaticMeshes.LowScaleRadiusThreshold=50
r.RayTracing.Geometry.InstancedStaticMeshes.LowScaleCullRadius=1500
r.RayTracing.Geometry.InstancedStaticMeshes.CullAngle=0
```

The old NVIDIA UE 4.23 guide uses the prefix `r.RayTracing.InstancedStaticMeshes.*`; UE 5.x moved these under `r.RayTracing.Geometry.InstancedStaticMeshes.*`. Verify the exact set is registered in our 5.7 build before relying on default values:

```powershell
& "<ue-path>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    "<project-root>\Alis.uproject" `
    -ExecCmds="cvarlist r.RayTracing.Geometry.InstancedStaticMeshes; quit" `
    -log

Select-String "<project-root>\Saved\Logs\Alis.log" `
    -Pattern "r.RayTracing.Geometry.InstancedStaticMeshes"
```

#### 2b. Asset-level surgical RT visibility (paired with 2a, NOT independent)

With the ISM CVar back on plus culling, asset-level "Visible in Ray Tracing" finally decides which foliage gets to participate in the RT scene. Sweep [`Plugins/Resources/ProjectObject/Content/Nature/ExteriorPlant/`](../../Plugins/Resources/ProjectObject/Content/Nature/ExteriorPlant/) (the same tree this project's foliage `SM_*` assets live under, including the Amur Cork / Hornbeam meshes the parent commit `0b4f34121` already touched):

- Background / dense filler foliage (low visual impact when removed from RT): keep "Visible in Ray Tracing = false".
- Hero / close-foreground foliage (high visual impact): re-enable.

Reference pattern: commit `0b4f34121` ("disable rt on the foliage") only handled Amur Cork + Hornbeam in 5 City17 cells, so the bulk of this work is unstarted.

#### Risk + safeguards

This brings back the exact path that crashed last time. Mandatory safeguards:

- Run only after Experiment 1 has been measured.
- Run with `-gpucrashdebugging` (Aftermath active).
- Keep `EWindowMode::WindowedFullscreen` mapping in place (the existing project default).
- If `Swapchain Resized` appears during gameplay, or any TDR event 4101 / 117 / 141 / 142 fires, revert the entire experiment immediately.

## Verification per experiment

For each experiment, repackage with `make package` and replay the City17 sprint+jump scenario from a packaged Shipping build with `-gpucrashdebugging`. Compare against the `01_done/debug/investigate_shipping_crash_sprint_jump.md` baseline:

- No `Swapchain Resized` outside boot in `Alis.log`.
- No `NumGeometrySegments` delta exceeding ~256 per event during gameplay (the resolved crash run threshold).
- Process exits cleanly (exit code 0, `LogExit: Exiting.` line present).
- Event Viewer 4101 / 117 / 141 / 142 stays empty during the test window.
- No `*.nv-gpudmp` written.
- Visual A/B on a fixed City17 pose: side-by-side screenshots of foliage GI / reflections / contact shadowing pre- and post-experiment.

If any experiment regresses crash safety, revert immediately and re-record what we learned about the failure mode.

## Out of scope

- Disabling HW RT globally (`r.RayTracing=False`) - too broad; non-instanced meshes currently work fine with HWRT.
- Re-enabling ISM/HISM HWRT globally without culling - too risky; this reopens the original City17 crash path (the resolved investigation).
- Switching Lumen back to software-only globally - too broad a regression for a foliage-quality task.
- Path tracer for gameplay - permanent CPU/GPU cost is too high.

## Risk table

| Option | Crash risk | Visual recovery | Notes |
| :--- | ---: | ---: | :--- |
| Experiment 1: `r.Lumen.TraceMeshSDFs=1` | Low | Medium | Best first test; UE 5.7 treats SWRT detail as legacy/deprecated. Profile in dense foliage. |
| Experiment 2 (full): re-enable ISM HWRT + culling + asset-level cleanup | Medium / High | High | Closest to original crash path. Mandatory `-gpucrashdebugging` + revert criteria. |
| Asset-level RT visibility ALONE (no CVar change) | n/a | none | **Invalid** - global ISM CVar of `0` makes per-asset "Visible in Ray Tracing" inert. |
| Experiment 1 only, then stop | Low | Medium | Minimum-risk landing path if visuals are "good enough" after Experiment 1. |

## Order of operations

1. **Experiment 1 first** - single CVar (`r.Lumen.TraceMeshSDFs=1`). Repackage with `make package`, run the City17 sprint+jump repro, do visual A/B vs. the baseline.
2. **If quality acceptable, stop here.** Lock in the CVar, close this todo.
3. **If quality still bad, Experiment 2 as one combined unit** - modern CVar namespace + culling + paired asset-level "Visible in Ray Tracing = false" sweep on background/dense filler. Re-enable RT visibility only on hero/foreground foliage. Run with `-gpucrashdebugging` + WindowedFullscreen.
4. **If crash safety regresses during step 3** (any `Swapchain Resized` outside boot, any TDR 4101 / 117 / 141 / 142, any `*.nv-gpudmp`): revert the entire experiment immediately to:
   ```ini
   r.RayTracing.Geometry.InstancedStaticMeshes=0
   r.Lumen.TraceMeshSDFs=1   ; only if Experiment 1 was kept
   ```
   and document what crashed where in this todo's history.

KISS: try Mesh SDF first; only reopen HWRT foliage if screenshots prove the quality gap is still unacceptable.

## References

- Resolved parent: [todo/01_done/debug/investigate_shipping_crash_sprint_jump.md](../01_done/debug/investigate_shipping_crash_sprint_jump.md)
- Plan trail: `<user-home>/.claude/plans/investigate-carefully-in-plan-cached-fiddle.md`
- Commit: `f8e8ffab7` ("fix(graphics): mitigate City17 sprint+jump crash via RT and fullscreen")
- Unreal Directive - `r.Lumen.TraceMeshSDFs` console variable reference (definition + cost notes for dense scenes).
- Tom Looman - "Unreal Engine 5.7 Performance Highlights" (UE 5.7 deprecates SWRT detail tracing; HWRT is the recommended quality scaling path).
- NVIDIA Developer - "Tips and Tricks: Getting the Best Ray Tracing Performance Out of Unreal Engine 4.23" (concept reference for ISM RT culling; CVar names are 4.23-era and have moved under `r.RayTracing.Geometry.*` in UE 5.x).
- Epic Games Developers - Ray Tracing Performance Guide in Unreal Engine (instance-count sensitivity; ~100 k instances is a common real-time budget warning).
- Epic Games Developers - Hardware Ray Tracing in Unreal Engine (significant scene update cost above ~100 k instances).
