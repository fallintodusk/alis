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
so a whole territory is censused in milliseconds with no rendering and no
editor build. Terrain and water are found by the runtime roles their actors
carry in the descriptor tags (`ProjectWorld.Terrain.v1`,
`ProjectWorld.Water.v1`), never by class or actor name. The census checks that
both are present and spatially loaded, the lighting set, and terrain relief. It
does not judge terrain-to-water heights: canonical compilation owns that
relation and fails closed when it breaks.

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

Green surface plus a green height check establish the measured sampling
characteristics and fidelity to canonical. They do not prove bare-earth source
suitability, accurate construction geometry or player-visible quality.

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

On each of those frames every registered evidence subject must also be drawn.
A representation adapter registers an `IProjectWorldEvidenceSubject` (declared
by ProjectWorldEditor) and reports for the open editor world whether it is
present, how many units it expects, how many are drawn, and how many builds are
pending; the command alone turns those counts into a decision. Mesh Terrain's
subject counts the loaded base modifiers aimed at its partition and how many of
them have a visible preview section, because an editor world never loads the
runtime-only compiled sections and builds its preview sections asynchronously
after the actors load. A frame with an undrawn subject resets the count, and a world with
no present subject waits for compilations only. The receipt `message` carries
each present subject's final counts; a subject never drawn within the timeout
rejects the receipt with them.

Compare a change only against a control session that loads and settles the
same way. Editor worlds never generate ProjectTexture's runtime pattern pixels,
so appearance driven by those signals is not visible in these captures
([pattern generation](../../../Plugins/Resources/ProjectTexture/docs/pattern_generation.md)).

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
normal product poses returning identical bytes is a hard rejection - that is the
stale-frame signature. Uniform diagnostic buffers can legitimately match across
poses; the hash gate covers normal product views. A repeated pose is captured
once as a REPORTED control; it is not
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

### Paired geometry and presentation review

Use identical framing to separate geometry, material and lighting defects:

| Plan `render_mode` | Representation | What it can establish |
| --- | --- | --- |
| `lit` (default) | Unchanged product materials, lights and exposure | Actual appearance and player readability |
| `base_color` | Native deferred Base Color, without scene lighting | Material boundaries and whether shadows hide a surface |
| `lighting_only` | Native neutral diffuse override, without material normal detail or specular | Geometry under the existing lights, independently of dark or noisy albedo |
| `world_normal` | Native signed GBuffer normals encoded as RGB | Surface orientation and suspicious shading discontinuities |

These are simple diagnostic surfaces, not replacement production materials.
The capture modifies only its transient component/view. It does not edit or save
world materials, lights, exposure, geometry or collision. Normal RGB encodes
`normal * 0.5 + 0.5`; it includes material normals and is not a topology or
collision certificate. Base Color and normals require deferred rendering;
forward rendering refuses instead of silently substituting a lit image.
Translucent materials do not have the same GBuffer coverage as opaque surfaces;
absence from a buffer view alone does not establish missing Water geometry.

Generate the paired overview plan with:

```powershell
python tools/World/VisualVerification/app/plan_vantages.py <descriptor-dump> <output-plan> --diagnostics
```

For a source-bound local plan, use `plan_vantages.with_diagnostics(plan)` before
passing it to the existing capture wrapper. It retains each product pose and
adds the three diagnostic poses, including the same FOV and image size. Review
the normal player-eye view first, then compare its diagnostic siblings. The
receipt labels each actual render mode and its product/diagnostic role. The
independent reader rejects unknown modes, diagnostics labelled as product, and
diagnostic sets without all four views at the same measured pose. Ordinary
normal-only captures remain supported; their authentication is not a visual PASS.

A clear diagnostic image does not satisfy a normal-presentation requirement.
For procedural material appearance, use Game/PIE with actual Ready outputs as
defined by [ProjectTexture](../../../Plugins/Resources/ProjectTexture/docs/pattern_generation.md#contract).
A cold Editor's empty encoded-normal target can produce nearly horizontal
shading normals over level collision geometry. Compare native output pixels and
the runtime normal buffer before classifying that as a Terrain, normal-graph or
exposure defect. Runtime-primed Editor images remain a different envelope from
actual play; label them separately. Use the existing saved/regional runtime view
route for acceptance, and retain actual play-world, support and camera evidence.

Use the comparison to select the responsible owner: wrong shape/placement ->
canonical or realization; correct shape with bad albedo -> material; correct
shape/material with unreadable shade -> presentation. Do not change Terrain
heights to compensate for darkness or reject an approximate roof solely because
it differs from detailed architecture.

Native references: [Epic viewport modes](https://dev.epicgames.com/documentation/en-us/unreal-engine/viewport-modes-in-unreal-engine?application_version=5.8),
installed `SceneCaptureRendering.cpp` / `SetupSceneViewExtensionsForSceneCapture`,
`SceneCapturePixelShader.usf`, and `EditorViewportClient.cpp` lighting overrides.

### Local support and context

For a local Water/Ground contact, derive the camera and target from the
authenticated feature footprint after coordinate conversion. A name containing
`player` does not establish that the eye stands on dry support or that the rim
is in frame: a pose above a pool center can look past the entire feature. Read
back actual support, pose and FOV, and inspect the normal image with full scene
context before closing the selected-candidate observation window. Nearby mock
volumes may obstruct a view that worked in an isolated Terrain fixture.

Temporary visibility, shadow, exposure or debug-buffer controls can identify
the responsible owner; label them as controls and restore them. They do not
replace a normal product view or justify a Terrain height adjustment. Preserve
completed native execution evidence when correcting framing or a final reader;
do not repeat full generation merely to obtain another camera angle.

## Output locations

The public capture entrypoint accepts absolute and repository-relative vantage
plan paths through the shared
[capture input resolver](../../../scripts/ue/world/capture_input_paths.ps1).
Selected-target admission uses the same resolver before World mutation. Missing
plans refuse before output creation or Editor launch; an absolute path is never
joined to the repository a second time.

All scratch output goes under the project `tmp/` tree:

```
tmp/world/visual_verification/
    actor_descs.csv          raw descriptor dump
    receipts/                census.json, surface.json, vantages.json
    screenshots/             capture output
```

Nothing under `tmp/` may be an input to a committed test, doc, or script.

## Scope

Covers terrain and water, found by their runtime roles. A later generated
layer attaches by adding its role check to `census.py` and its vantages to
`plan_vantages.py`; a representation that an editor capture must wait for
registers its own evidence subject. None of these reopens the stage boundaries
above.
