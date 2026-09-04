# Research: Manhattan performance delta between Development and Shipping

**Status:** BACKLOG / WATCH. Not active Release 2.0 implementation work.
**Created:** 2026-09-03
**Owners:** ProjectWorld presentation/performance owners plus packaging/config owners
**Trigger:** Operator reported subjectively very poor Manhattan runtime performance while
flying the packaged Shipping Candidate. Every Development measurement taken on 2026-09-03
came back healthy, including an operator-driven windowed session.

## Not a release blocker

This does not gate Release 2.0.0 and no work here is scheduled. It is recorded so the evidence
does not have to be re-derived if the symptom returns.

The cheap way to resolve it, whenever someone happens to run the Shipping Candidate: use the
same envelope as the Development evidence, so the observation is actually comparable.

```text
same Shipping Candidate
RTX 4070
2560x1440
scalability High (level 2)
same VSync / FPS-cap policy
same window/fullscreen mode
```

Then:

- fine at that matched envelope -> close this file; the original experience was then almost
  certainly saved display settings or session specifics, not the world;
- still dramatically worse at the SAME envelope -> reopen as real work, because users run
  Shipping, and start with the instrumentation options below rather than optimizing the World.

Observation is enough for that call. Shipping cannot report frame timings (see the
instrumentation section), and the only question is whether measurement is warranted at all.

## Why this exists

The symptom did not reproduce in any Development configuration. The remaining difference is
the build configuration itself, so the question is no longer "is Manhattan too heavy" but
"what differs between our Development and Shipping packages". This file records the measured
evidence and the ranked hypotheses so the work does not have to be re-derived later.

Do not treat any hypothesis below as diagnosed. None has been tested.

## Measured evidence

Envelope, verified from the gate receipts rather than assumed: Development, RTX 4070, D3D12,
2560x1440, scalability High (level 2), VSync off, `t.MaxFPS 0`, dynamic resolution off.

Every automated row below ran with `-RenderOffScreen`, so none of them exercised real
swapchain presentation. That is a real limitation of this whole data set, not a footnote.

| Condition | Frame p95 | Game p95 | Render p95 | GPU p95 | Notes |
| --- | --- | --- | --- | --- | --- |
| Stationary settled, headless | 13.44 | 6.54 | 13.44 | 6.37 | zero hitches after frame 117 |
| Five-route territory sweep | 11.67-12.53 | 5.8-6.7 | 11.65-12.53 | 7.2-9.8 | 1,327 cell activations |
| 360 s tour, memory trace ON | 19.81 | 8.42 | 19.82 | 7.95 | CONTAMINATED, see below |
| 360 s tour, no trace (control) | 12.88 | 6.49 | 12.84 | 8.20 | no drift; last window fastest |
| Operator interactive, windowed | not captured | - | - | - | real presentation; operator reported no hit |
| Kazan control (accepted) | 13.64 | 5.08 | 13.65 | 9.46 | for comparison |

Budget is 16.67 ms. Manhattan sits inside it and uses LESS GPU than accepted Kazan.

The operator session is the only run with a real window and real input. Its CSV was never
flushed - the session was closed without `CsvProfile Stop` - so only its `game.log` survives.
The "no hit" for that row is the operator's direct observation, not a measurement.

Supporting facts:

- `peak_loaded_cells` stayed pinned at 24-25 in every run that reported it (the three
  performance-gate runs). World Partition residency is bounded; cells are released. The
  stationary run used no gate and therefore reported no cell metrics.
- Process memory 2.197 GB (94 s sweep) vs 2.202 GB (360 s tour) = +4 MB. GPU memory
  3.727 GB vs 3.747 GB. No accumulation across 172-1,327 cell activations.
- Zero streaming failures in all runs.
- Bottleneck is the RENDER THREAD in every window: Render p95 tracks Frame p95 almost
  exactly while GPU sits at roughly half the frame. Largest single render bucket is
  `RenderOther` at ~3.84 ms (unattributed). Shadows measured 0.76 ms; Lumen showed no
  measurable cost; Nanite showed no measurable time cost.

### Measurement trap worth remembering

A 360 s run with `-trace=...memory...` reported +14% monotonic degradation and 4.20 GB
process memory. The untraced control of the identical route reported no drift and 2.20 GB.
The 4.2 GB utrace was itself the cause. Memory tracing manufactures exactly the monotonic
signature a cumulative-leak investigation is looking for. Always run an untraced control
before believing drift.

## What is different about Shipping (unverified hypotheses, ranked)

1. **Display envelope.** No `FullscreenMode`/`ResolutionSizeX`/`bUseVSync`/`FrameRateLimit`
   defaults exist in `Config/*.ini`, so the packaged build uses per-user saved
   `GameUserSettings`. The Shipping session very likely ran fullscreen at native monitor
   resolution, while every measurement above was 2560x1440. If the operator display is 4K
   that is 2.25x the pixels, and GPU cost scales with it. Cheapest thing to check first.
2. **Shader/PSO behaviour.** `Config/DefaultGame.ini` sets `bShareMaterialShaderCode=False`,
   which disables the shared shader code library that a bundled shader pipeline cache would
   need. `Config/DefaultEngine.ini` enables `r.PSOPrecache.Resources=1` and
   `r.PSOPrecache.ProxyCreationWhenPSOReady=1`. First-encounter PSO compilation behaves
   differently between a Development build with warm local shader state and a cooked Shipping
   build, and Manhattan presents far more distinct material/mesh combinations than Kazan.
3. **Scalability defaults.** Every Development run above settled at scalability level 2
   ("High"), verified from the gate receipts and from the final `[ShadowQuality@2]` line in
   each log; the engine applies `@3` at init and the gate then forces
   `RequiredHighQualityLevel = 2`. The Shipping build resolves its level from saved user
   settings or a device profile instead, so it may not have been at High at all.
4. **Session shape.** The operator's Shipping session was longer and human-driven; the
   automated routes were 94 s and 360 s. Duration or path shape may still matter.

## Instrumentation: what Shipping strips, and what is still available

- Shipping strips the console, so `stat unit` is unavailable. Engine `Build.h` defines
  `ALLOW_CONSOLE_IN_SHIPPING 0` by default while non-shipping configurations get
  `ALLOW_CONSOLE 1`.
- `FProjectWorldProductPerformanceGate` refuses outright in Shipping with
  "This Shipping build does not include UE CSV profiler support"
  (`ProjectWorldProductPerformanceGate.cpp:439`), because `CSV_PROFILER` is compiled out.

`ALLOW_CONSOLE_IN_SHIPPING` and the stats defines are overridable in principle, but they live
in engine code, so overriding them means recompiling the engine with a unique build
environment. On a launcher engine that is not possible - the same precompiled-binaries
limitation recorded in `Alis.Target.cs`. They therefore land in the source-engine bucket
alongside Test configuration, and are not a cheaper shortcut to it.

"Shipping cannot be instrumented" is NOT the same as "the delta cannot be investigated".
Epic provides standard routes for exactly this problem; the constraint here is which of them
this project's engine install supports.

### Test configuration - Epic's intended answer, blocked on the launcher engine

`Trace/Config.h` defines `UE_TRACE_ENABLED = 1` for every configuration EXCEPT Shipping, so a
Test build keeps full Unreal Insights tracing, stats, console and CSV while being an
optimized non-editor build. `Alis.Target.cs` already points here: "Use Test config for
diagnostic builds with assertions."

Verified blocker: `Engine/Config/BaseEngine.ini` `InstalledPlatformConfigurations` lists
DebugGame, Development and Shipping for `PlatformType="Game"` on Win64, and contains ZERO
`Configuration="Test"` game entries. The launcher engine therefore cannot build Test. This is
the same root reason `Alis.Target.cs` gates `bUseLoggingInShipping` behind `bSourceEngine`:
"launcher Shipping binaries are precompiled without logging."

So Test requires a source engine. `scripts/config/ue_path.conf` declares
`UE_SOURCE_PATH=<ue-path>`; confirm whether it has actually been built before
planning around it, because a first build of that engine is a multi-hour operation. This is a
cost decision for the operator, not a technical dead end.

Shipping trace has an engine-side switch, `UE_TRACE_ENABLED_SHIPPING_EXPERIMENTAL`, default 0
and explicitly marked EXPERIMENTAL by Epic. It also needs a unique build environment, so it
lands in the same source-engine bucket.

### Routes that work on the existing Shipping Candidate today, with no engine work

These need no engine rebuild and no instrumentation compiled into the game, because they
attach to the process or the graphics API from outside:

- GPU frame capture: PIX for Windows, NVIDIA Nsight Graphics, RenderDoc. These answer
  "which pass costs what" on the real Shipping binary, which is the fastest way to test the
  display-envelope and pixel-cost hypotheses.
- CPU sampling: Superluminal, Intel VTune, or Windows ETW / Windows Performance Analyzer.
  These need the Shipping PDBs. `scripts/ue/package/package_release.ps1` strips debug info by
  default (it reports `NODEBUGINFO = True` unless `-IncludeStagedDebugFiles` is passed), so
  the existing Candidate has no symbols and a symbol-bearing re-package is required first.
  Symbols alone do not change the shipped code path.
- The packaged Shipping log still records device profile, resolution and RHI selection at
  startup even when verbose logging is stripped; read it before assuming anything.

## Cheapest next experiments (in order)

1. Read the operator's actual display resolution, fullscreen mode, VSync and frame cap from
   the Shipping session's `GameUserSettings.ini` and startup log, then re-run Development at
   that exact envelope, fullscreen included. If it degrades, the answer is the display
   envelope and nothing about Manhattan or World Partition. Costs one Development run.
2. If the envelope matches and Development is still healthy, capture the Shipping Candidate
   with PIX or Nsight. Still no engine work.
3. Only if both are inconclusive, decide whether the source engine build is worth it to get
   Test configuration and full Insights.
4. Only then investigate PSO/shader-library behaviour, and only with measurements.

## Web/documentation checks worth doing when this is picked up

- UE 5.8 PSO precaching and bundled PSO caches: interaction with
  `bShareMaterialShaderCode=False`, and whether a shader pipeline cache is expected for
  Shipping.
- Whether `r.PSOPrecache.ProxyCreationWhenPSOReady=1` has a measurable steady-state cost in
  dense scenes versus only affecting pop-in.
- Development vs Shipping renderer differences that plausibly affect render-thread cost.
- Whether installed/launcher engines can be made to build a Test game target at all, or
  whether a source engine is genuinely required for it.
- `UE_TRACE_ENABLED_SHIPPING_EXPERIMENTAL`: what it actually enables in 5.8, its cost, and
  whether Epic considers it usable outside internal testing.
- Current guidance for capturing a packaged UE title with PIX / Nsight Graphics, including
  what the build must retain for symbols to resolve.

This is a well-trodden problem for UE teams; prefer finding Epic's documented route over
inventing one here.

## Non-goals

- No HLOD, Nanite, VSM, Lumen, geometry, Building v2, cell-size or loading-range changes.
  Nothing measured so far implicates any of them; GPU is roughly half the frame and Manhattan
  is cheaper on GPU than accepted Kazan.
- No optimization of `RenderOther` until a real regression is reproduced and attributed.
- No re-running the memory trace without an untraced control alongside it.

## Evidence paths

Scratch, disposable:

- `tmp/world/manhattan_showcase/perf/dense_settled_62ca29196fa2/` - stationary baseline
- `tmp/world/manhattan_showcase/perf/sweep_b0d8c9988ef8/` - five-route sweep
- `tmp/world/manhattan_showcase/perf/tour_62bab548728a/` - 360 s traced (contaminated)
- `tmp/world/manhattan_showcase/perf/tour_notrace_8510cfe9351c/` - 360 s control
- `tmp/world/manhattan_showcase/perf/operator_153399050c11/` - operator windowed session
- `tmp/world/manhattan_showcase/perf/run_*.ps1` - the diagnostic launchers used

Kazan control metrics:
`Saved/Validation/WorldRealization/playable-tour/0035b0e27b1247b2b648b91ed06c10cf/development/performance-aggregate.json`

## Related change already made

`FProjectWorldProductPerformanceGate` gained the same explicit non-interactive policy the
product-route gate already had: interaction required by default, omitted only when the
operation passes `-ProjectWorldProductRouteSkipInteraction`, with
`gameplay_interaction_required` recorded in the receipt. Without it the performance gate
could not measure a showcase territory that has no gameplay-placement layer. No threshold or
sampling semantics were changed.
