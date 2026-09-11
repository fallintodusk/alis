# Visual Verification

Proves a realized ALIS world territory is actually there, actually placed, and
actually plausible - independently of whether the realization pipeline reports
success.

## Why this component exists

A territory once passed its realization height gate with `215040/215040`
samples matching canonical at `0.0031 m` maximum error while rendering as a
visibly terraced surface framed so tightly that no reviewer ever saw more than
13% of it. Nothing in the pipeline was lying; the checks simply could not see
those failures:

| Failure | Why the existing gate missed it |
| --- | --- |
| Terraced surface | Height tolerance is derived from the canonical profile's own `height_quantization`, so artifacts sitting on that lattice are inside tolerance by construction. |
| Unreviewable framing | Capture cameras carried authored altitudes sized for a small tile. Nothing compared them against the realized extent. |
| "Nothing is loading" | Editor World Partition never streams by camera. Absence of streaming was read as a defect when it was correct behaviour. |

The lesson is that **a gate whose tolerance comes from the same constant that
produces the artifact can only ever pass.** Each stage below is deliberately
anchored on something the realization pipeline does not control.

## Stages

### 1. Census - presence and placement

```
# in the editor, with the territory open
wp.Editor.DumpActorDescs <repo>/tmp/world/visual_verification/actor_descs.csv

python tools/World/VisualVerification/app/census.py \
    tmp/world/visual_verification/actor_descs.csv \
    tmp/world/visual_verification/receipts/census.json
```

World Partition actor descriptors are readable **without loading the actors**,
so a 210-cell territory is censused in milliseconds with no rendering and no
editor build. Checks layer counts, single logical landscape, spatial-loading
flags, the lighting set, relief, and water-below-terrain.

Proves presence and placement **only**. Never report a green census as visual
approval.

### 2. Surface - is canonical a plausible terrain

```
python tools/World/VisualVerification/app/surface.py \
    <canonical>/canonical/terrain \
    tmp/world/visual_verification/receipts/surface.json \
    [cell-limit]
```

Reads canonical cell documents directly and never consults the engine, so it
is immune to the self-reference that defeats the height gate. Measures the
detected vertical lattice, terrace ratio (adjacent samples exactly equal),
supported level ratio, and single-step ratio.

Every cell is measured by default. Cell documents are named by grid
coordinate, so an unqualified head of the file list is a contiguous column
strip rather than a territory: the optional limit therefore strides across
the territory, and the receipt records `sampling` as `complete` or
`strided_subsample`.

The two gates are independent and neither subsumes the other:

| Gate | Catches | Blind to |
| --- | --- | --- |
| `terrace_ratio <= 0.45` | repeated samples, including nearest-neighbour upsampling | a coarse lattice whose neighbours still differ |
| `supported_level_ratio >= 0.50` | a surface that does not deliver the vertical resolution it declares | repeated samples that keep the level count |

`supported_level_ratio` is distinct levels divided by
`min(relief / declared quantization + 1, samples)`. It is scale-free by
construction, so a genuinely flat lake cell scores 1.0 instead of being
rejected for being flat. `level_utilisation` (distinct levels per sample) is
reported but NOT gated: it is bounded by relief, so an absolute floor on it
rejects good low-relief data.

```
python -m unittest discover tools/World/VisualVerification/tests
```

Green surface **plus** green height check together mean "correct data,
correctly realized". Neither proves that alone.

### 3. Capture - operator evidence

**Status: implemented.** Runs in the live editor, offscreen, outside automation.

```
python tools/World/VisualVerification/app/plan_vantages.py \
    tmp/world/visual_verification/actor_descs.csv \
    tmp/world/visual_verification/receipts/vantages.json

.\scripts\ue\world\capture_visual_evidence.ps1 `
    -Map /<world-data-plugin>/Generated/Territory/<map> `
    -VantagePlan tmp/world/visual_verification/receipts/vantages.json

python tools/World/VisualVerification/app/verify_capture.py \
    tmp/world/visual_verification/receipts/capture.json
```

The capture is a transient `USceneCaptureComponent2D` rendering into its own
render target: no viewport, no window, no comparison path, and nothing saved.
`-RenderOffscreen -unattended` keeps a real RHI and a real frame loop while
opening no window, so the capture neither fights the operator for foreground
focus nor depends on it.

The console command loads the full World Partition bounds and then defers the
capture until three distinct editor frames report no pending asset compilation.
Do not collapse that wait into the startup frame: all actors can be present
while Landscape render state is still incomplete, producing authenticated but
visually false checkerboard frames.

It is NOT a commandlet. That envelope was tried and abandoned on measured
evidence - see ProjectWorld pitfall 15 for the full list of things that were
verified true while the frames stayed black.

The wrapper binds the evidence to the REQUESTED map: it refuses a receipt whose
`map_package` differs. A startup map that fails to load leaves the editor on a
fallback world, which otherwise yields a perfectly authenticated capture set of
the wrong territory - verified by negative test, where a nonexistent map
produced three accepted captures of City17.

`verify_capture.py` then authenticates the set independently of the engine: it
re-reads PNG headers for actual dimensions and re-hashes every file. Distinct
poses returning identical bytes is a hard rejection - that is the stale-frame
signature. A repeated pose is captured once as a REPORTED control; it is not
gated, because UE carries temporal rendering state between frames and healthy
imagery legitimately differs on a repeat.

Water has one narrower product gate. When the current blockout contract requires a
solid-blue Water surface, compare the first and repeated-pose PNGs with:

```powershell
.\scripts\ue\world\test\water_temporal_stability.ps1 `
    -ReferencePath tmp/world/visual_verification/screenshots/water.png `
    -RepeatPath tmp/world/visual_verification/screenshots/water.control.png `
    -ReceiptPath tmp/world/visual_verification/receipts/water_temporal.json
```

This does not require whole-frame byte equality. It classifies only clearly blue Water
pixels, then bounds blue-to-ground classification flips and mean per-channel drift.
The generic repeated-pose control remains diagnostic for all other materials.

The former in-automation sweep,
`Project.World.Realization.Territory.VisualSweep`, was REMOVED. It was not
removed for being the wrong idea - it was the wrong EXECUTION ENVELOPE. Inside
`Automation RunTests`, `GIsAutomationTesting` is true, and
`UAutomationBlueprintFunctionLibrary::TakeHighResScreenshot` then completes on
screenshot COMPARISON against stored ground truth rather than on capture. With
no baseline established, every request hung until timeout. Do not reintroduce
an in-automation sweep to recover this stage.

Required properties of the accepted route:

- render-capable UE execution OUTSIDE `Automation RunTests` /
  `GIsAutomationTesting` comparison mode, so completion means capture;
- the engine's own capture API owns frame sequencing.
  Do **not** hand-drive with console `BugItGo` + `HighResShot` + sleeps: that
  depends on OS foreground state, so a backgrounded editor silently yields
  missing or stale frames, and it fights the operator for focus;
- vantages **derived from realized territory bounds**, never authored - the
  overview altitude is solved from the actual extent against the camera field
  of view. This removes the authored-altitude failure class permanently, and
  that solve is owned by `plan_vantages` in stage 1, which survives unchanged;
- capture actors transient and cleaned up, so producing evidence never modifies
  the opened territory;
- output is operator EVIDENCE, never an acceptance gate on its own.

The comparison path (`AScreenshotFunctionalTest`,
`TakeAutomationScreenshotAtCamera`) remains the correct engine mechanism for
genuine baseline comparison tests. It is simply not what evidence capture
needs. Choosing the wrong branch is the single easiest way to lose a day here;
see the screenshot entry in
[ProjectWorld pitfalls](../../../Plugins/World/ProjectWorld/docs/pitfalls.md).

## Output locations

All scratch output goes under the project `tmp/` tree per the AGENTS.md rule
"SCRATCH FILES LIVE IN PROJECT `tmp/` ONLY":

```
tmp/world/visual_verification/
    actor_descs.csv          raw descriptor dump
    receipts/                census.json, surface.json, vantages.json
    screenshots/             capture output (route not yet implemented)
```

Nothing under `tmp/` may be an input to a committed test, doc, or script.

## Scope

Covers terrain and water, the layers admitted so far. Later generated layers
attach by extending the expectation table in `census.py` and adding vantages in
the sweep test; neither requires reopening the stage boundaries above.
