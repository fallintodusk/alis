# ProjectWorld Pitfalls

Verified bug archive for generated-world realization. Read before touching
the area; add an entry after fixing a non-trivial bug.

Format: Symptom / Root cause / Fix / File / Regression test.

---

## 1. Route navigation volume looks mis-oriented on a diagonal road

**Symptom.** In the editor, the generated route's collision/navigation box
visibly does not follow the road direction. Reported live 2026-08-06 on the
representative Kazan map. The road ribbon itself was correct.

**Root cause.** `ANavMeshBoundsVolume` was placed unrotated and sized from
the axis-aligned extents of the route endpoints
(`abs(End.X - Start.X)`, `abs(End.Y - Start.Y)`). For any road that is not
axis-aligned that is the diagonal's bounding hull, not a route-local box -
so the wider the diagonal, the more wrong it looks and the more unrelated
space it claims for navigation.

**Fix.** Yaw the volume actor onto the route direction
(`Atan2(RouteDelta.Y, RouteDelta.X)`) and size it route-local: X = route
length/2 + padding, Y = padding only. The brush is built in actor space
along X, so the rotation makes the box follow the road. The applied yaw is
recorded in the realization receipt as `runtime_route_volume_yaw_degrees`.

**File.** `Source/ProjectWorldEditor/Private/ProjectWorldRuntimeRealization.cpp`
(`Apply`, route navigation volume section).

**Regression test.** `Project.World.Realization.Runtime.RouteCollision`
(orientation assertions), plus the E2E `runtime_route_unproven` gate.

---

## 2. A presence-only collision probe cannot prove road orientation

**Symptom.** The runtime acceptance passed while road collision could still
be laterally collapsed, mis-facing, or the wrong width. The probe reported
"collision proven in every route fragment cell" regardless.

**Root cause.** The probe traced ONE point - the polyline interior point -
straight down and only checked that the hit actor carried the expected
feature and cell tags. Both conditions survive almost any orientation or
width error, because the cell actor owns terrain, roads, and buildings in
ONE procedural mesh component: a terrain hit 0.65 m below the road carries
exactly the same actor tags as a road hit. Tag identity therefore proves
nothing about which surface was hit.

**Fix.** Two changes, both needed:

1. Discriminate by height band, not by tags. A hit counts as the road only
   when it lands within 35 cm of the expected lifted road surface
   (`SampleTerrain + 0.65 m`); the tolerance stays well under the road lift
   so terrain can never satisfy it.
2. Probe a road-local frame of 7 points per fragment cell, derived from the
   segment tangent at the sample point: centre, +/- tangent (along), and
   +/- half of the half-width (across) MUST hit the road band with an
   upward impact normal (`ImpactNormal.Z >= 0.94`); +/- beyond the declared
   half-width MUST NOT hit the band. Orientation, width, and facing are all
   observable only because on-road and off-road expectations are asserted
   together.

**File.** `Source/ProjectWorldEditor/Private/ProjectWorldRuntimeRealization.cpp`
(`TraceRouteSurface`, `ProbeRouteCollision`, `RepresentationInteriorFrame`).

**Regression test.** `Project.World.Realization.Runtime.RouteCollision`.
Sabotage-verified: compiling with
`PROJECT_WORLD_SABOTAGE_ROAD_ORIENTATION=1` collapses the ribbon rails onto
the tangent and the probe fails specifically with "Road collision covers the
declared ribbon along and across the tangent." The toggle lives in
`ProjectWorldGeneratedGeometry.cpp` and must stay `0` in committed code.

---

## 3. CRLF in a hash-authoritative document breaks the authority on a clean clone

**Symptom.** Nothing locally. On a fresh clone (or after git touches the
files), every `manifest_sha256` in `active_set.json` mismatches and
`Read-ProjectWorldActiveSet` fails closed with "Active manifest hash
mismatch" - for a reason that has nothing to do with content.

**Root cause.** `ConvertTo-Json` emits CRLF on Windows, and the manifest
writer persisted those bytes verbatim. The repository declares `eol: lf`
for these paths (`git check-attr text eol` reports `text: set, eol: lf`),
so git rewrites them to LF on checkout. The recorded SHA-256 values were
computed over the CRLF bytes, so the on-disk bytes after a clone can never
match the authority that describes them. This class of bug is invisible on
the machine that produced the files.

**Fix.** Both writers (`Write-ProjectWorldJson` and the active-set record
writer in `Publish-ProjectWorldActiveSet`) normalize CRLF to LF before
writing. Superseded manifest generations written before the fix keep their
CRLF bytes - they are immutable historical documents, are not referenced by
the active set, and nothing hashes them.

**Rule.** Any document whose ON-DISK bytes are hashed as authority must be
written with deterministic, platform-independent line endings that match
the repository's declared `eol` attribute. Check `git check-attr text eol`
before trusting a stored hash of a text file.

**File.** `scripts/ue/world/generated_manifest.ps1`.

**Regression test.** `generated_manifest.Tests.ps1` - "writes authority
documents with LF-only bytes so a clean clone still verifies" asserts no CR
byte in the active-set record or a published scope manifest.

---

## 4. Two same-named anonymous-namespace helpers in one module break the unity build

**Symptom.** `error C2264: 'TagValue': error in function definition or
declaration; function not called`, with a note pointing at a DIFFERENT
translation unit's definition.

**Root cause.** UE compiles a module's `.cpp` files as one unity blob.
Two files in the same module each defining `TagValue` / `HasTagValue` in an
unnamed namespace collide - the anonymous namespace does not isolate them
once the sources are concatenated.

**Fix.** Give module-internal helpers file-distinct names
(`RuntimeTagValue` / `HasRuntimeTagValue` alongside the presentation-gate
originals), or hoist one shared implementation.

**File.** `Source/ProjectWorld/Private/Presentation/ProjectWorldPresentationSampling.cpp`
vs `.../ProjectWorldPresentationGate.cpp`.

**Regression test.** The build itself; no runtime test applies.

---

## 5. UE-created navigation data changes external-actor package paths

**Symptom.** An unchanged clean reconstruction intermittently fails with
`generated_package_path_churn`; one `RecastNavMesh` external-actor path is
removed and a different path appears.

**Root cause.** `GetDefaultNavDataInstance(Create)` spawns navigation data
with a new actor GUID. World Partition derives the external package path from
that GUID, so equivalent rebuilds can produce different persistent paths.

**Fix.** Keep the single always-loaded Recast authority inside the map package.
It is not a streamable world actor and therefore must not claim an external
actor package path. The helper is separate from route probing because package
ownership is a persistence concern.

**File.** `Source/ProjectWorldEditor/Private/ProjectWorldRuntimeNavigation.cpp`.

**Regression test.**
`Project.World.Realization.Runtime.NavigationDataOwnership`, plus the Matrix
generated-package path-stability gate.

---

## 6. Centerline pruning drops edge water before its width exists

**Symptom.** A ribbon whose centerline is outside the selected territory is
missing even though its buffered surface enters an edge cell. A careless fix
can also resurrect a centreline that polygon authority intentionally removed.

**Root cause.** The compiler selected water by original-geometry cell
membership before resolving width and buffering. Later footprint membership
could not recover an already-discarded feature. Filtering intermediate water
state also erased `visible=None`, the marker for a fully polygon-suppressed
axis, allowing its raw source line to fall through.

**Fix.** Classify the complete admitted water population first. Use polygons
and final quantized ribbon footprints for target relevance, preserve every
suppression marker, and use the existing canonical terrain halo only when a
footprint enters while its axis stays outside the core. Reject an axis beyond
that halo instead of guessing Z.

**Files.** `tools/World/CanonicalCompilation/app/features.py`, `water.py`,
and `water_geometry.py`.

**Regression tests.** `WaterSemanticsTests.test_ribbon_outside_grid_uses_footprint_and_terrain_halo`,
`test_ribbon_reaching_beyond_terrain_halo_fails_closed`,
`test_complete_water_prepass_keeps_fully_suppressed_axis_hidden`, and
`test_complete_water_prepass_does_not_promote_outside_non_surface`.

---

## 7. Polygon-suppressed river tails are not necessarily a broken seam

**Symptom.** A valid grouped river fails canonical seam validation after its
polygon-owned middle section suppresses the overlapping centerline ribbon.

**Root cause.** The validator required every pair of same-identity line
representations in adjacent cells to share an endpoint. Suppression can leave
two disconnected uncovered tails whose endpoints do not lie on the common cell
boundary, so there is no seam to compare.

**Fix.** Compare line endpoints only when both representations actually touch
the same common cell boundary. A real shared-boundary mismatch still fails.
Keep the stricter rule for roads: every multi-cell road fragment must
participate in an exact shared-boundary seam.

**File.** `tools/World/CanonicalCompilation/app/validation.py`.

**Regression tests.**
`GeometryAuthorityTests.test_polygon_suppressed_river_tails_need_no_false_shared_seam`
`GeometryAuthorityTests.test_disconnected_cross_cell_road_still_fails_closed`,
and `LandscapeWaterTwinTests`.

---

## 8. Outer-ring-only loading loses canonical water authority

**Symptom.** Unreal fills a lake island/hole and cannot reproduce the canonical
standing or flowing water Z function even though the JSON is valid.

**Root cause.** The reusable geometry loader retained only polygon outer rings,
and the canonical feature model had no typed, versioned water-surface function.

**Fix.** Preserve polygon and multipolygon hole topology, parse the closed water
surface contract fail-closed, retain `function_version`, accept only the two
implemented v1 ID/version pairs, and pass the result to the
GeometryCore/GeometryAlgorithms MeshDescription adapter.

**Files.** `ProjectWorldGeometryParsing.*`,
`ProjectWorldWaterContractParsing.*`, and `ProjectWorldWaterMeshBuilder.*`.

**Regression tests.**
`Project.World.Realization.NativeTwin.WaterCanonicalContract` and
`LandscapeWaterTwinTests`.

---

## 9. Water material enum and package bytes are not realization identity

**Symptom.** A generated Single Layer Water material reports a shader compile
warning, or an unchanged re-save changes the `.uasset` SHA-256 and is mistaken
for a semantic regeneration.

**Root cause.** UE 5.8 requires a
`UMaterialExpressionSingleLayerWaterMaterialOutput` node with connected water
inputs; `SetShadingModel(MSM_SingleLayerWater)` alone is invalid. Unreal package
serialization may also change bytes across a save even when generated semantics
are unchanged.

**Fix.** Build the native Single Layer Water output expression and finish shader
compilation before accepting the material. Decide no-op from deterministic
generated semantic identity before writing; package hashes authenticate the
bytes that were written, not whether a new write is required.

**Files.** `ProjectWorldWaterMeshBuilder.*` and
`Tests/ProjectWorldWaterNativeTwinTests.cpp`.

**Regression test.**
`Project.World.Realization.NativeTwin.WaterAssetPersistence`.

---

## 10. Global identity on the logical Landscape breaks cell-local regeneration

**Symptom.** One changed terrain cell, or even a water-only canonical-input
change, dirties the logical map package instead of only the affected spatial
package.

**Root cause.** The logical Landscape stored the whole bundle input hash and
per-cell terrain hashes, and treated any updated component as a reason to dirty
the root actor. Those identities changed more broadly than the package owner.
Even after correcting those flags, the outer service unconditionally called
`SaveLevel()` for cell-local changes and rewrote clean logical-map bytes.

**Fix.** Keep only stable topology, grid, material, edit-layer, and logical
identity on the root Landscape. Store each terrain-input identity on its
Landscape component and the cell ID on its streaming proxy. Validate the
component's actual `SectionBase` and canonical bounds before accepting that
ownership. Save the persistent level only for a new map or a genuinely dirty
root package; otherwise save only dirty external packages.

**Files.** `ProjectWorldLandscapeRealization.cpp`,
`ProjectWorldRealizationService.cpp`, `Tests/ProjectWorldNativeTwinTests.cpp`,
and `scripts/ue/world/test/integration/realization_layer_lifecycle.ps1`.

**Regression tests.**
`Project.World.Realization.NativeTwin.LandscapePartitionAndEditLayers` proves
ownership and dirty flags. The isolated L1 runner's `-ProvePackageLocality`
mode proves that a genuine one-cell terrain change rewrites exactly its proxy,
that a water-only change rewrites no terrain package, and that both leave the
logical `.umap` byte-identical through the real wrapper/commandlet save path.

---

## 11. A realized territory "looks broken" but every layer is present

**Symptom.** Operator opens the generated territory, flies the camera, and
reports "just a piece of terrain, no World Partition auto loading, no water".
Reported live 2026-08-18 on the 210-cell Kazan territory.

**Root cause.** Three independent things, none of which is a generation defect:

1. **Editor World Partition never streams by camera.** Editor loading is by
   Loaded Region / always-loaded adapter, not viewport position. Camera-driven
   streaming is a RUNTIME behaviour. Absence of streaming in the editor is
   correct, not a defect.
2. **Everything was already resident**, so nothing could appear to stream.
   `obj list class=LandscapeStreamingProxy` returned all 210, and
   `obj list class=StaticMeshActor` returned all 145 water actors.
3. **Every capture camera framed a fraction of the world.** The presentation
   profile's `ProjectWorld_Capture_*` cameras carry authored altitudes sized
   for a small tile: 90 m, 320 m and 900 m. Framing a 13,950 m territory at
   90 degrees horizontal FOV needs 6,975 m. Those cameras therefore showed
   1.3%, 4.6% and 12.9% of the territory, and two of the three sat OUTSIDE the
   territory extent entirely.

**Fix.** Derive capture altitude from the realized extent
(`altitude = span / 2 / tan(fov/2)`), never author it as a constant. Diagnose
presence with descriptors before ever reaching for a screenshot.

**File.** `tools/World/VisualVerification/app/{census,plan_vantages}.py`.

**Regression test.** `plan_vantages` solves the overview altitude from measured
proxy bounds, so an authored constant cannot reintroduce the framing failure.
The former `Project.World.Realization.Territory.VisualSweep` automation test
that first carried this solve was REMOVED - it ran inside
`Automation RunTests`, where the screenshot gates below make evidence capture
impossible. Ownership moved to the VisualVerification component; do not
reintroduce an in-automation sweep to recover it.

---

## 12. `wp.Editor.DumpActorDescs` is the fast scene-truth tool

**Symptom.** Diagnosing "is the world actually there" by launching editors,
taking screenshots, and eyeballing them - slow, subjective, and it answers the
wrong question.

**Root cause.** World Partition actor descriptors are readable WITHOUT loading
the actors. Reaching for rendering first skips the cheap, total, objective
check.

**Fix.** With the territory open:

```
wp.Editor.DumpActorDescs <repo>/tmp/world/visual_verification/actor_descs.csv
python tools/World/VisualVerification/app/census.py <csv> <receipt.json>
```

Full 210-cell census in milliseconds: per-class counts, spatial-loading flags,
bounds, extent, relief, lighting set. No rendering, no editor build. Run this
BEFORE any visual step. Note the dump is space-delimited `key:value`, not real
CSV, and always-loaded actors (e.g. `DirectionalLight`) carry
`IsValid=false` with no `Min`/`Max`, so bounds must be parsed optionally or
those rows vanish and the lighting check falsely fails.

**File.** `tools/World/VisualVerification/app/census.py`.

---

## 13. Automated screenshots must use the engine automation path

**Symptom.** Console-driven capture (`BugItGo` + `HighResShot`) silently
produces no file, or a stale frame from the previous camera position.

**Root cause.** `HighResShot` captures on the next RENDERED frame. A
backgrounded or occluded editor does not render, so the request never
completes. `Slate.bAllowThrottling 0` alone does not fix it. Worse, the
workaround (stealing OS foreground) fights the operator for focus and is
refused outright by Windows when another process owns foreground. Console `|`
chaining is also not supported - only the first command runs.

**Fix.** Use the engine's own screenshot API,
`UAutomationBlueprintFunctionLibrary::TakeHighResScreenshot(ResX, ResY,
Filename, Camera, ...)` - but read the envelope rule at the end of this entry
before choosing where to call it from, because the same call means different
things inside and outside an automation test. It locks the level viewport to a supplied
`ACameraActor` in pilot mode, forces game view, flushes pending loads, delays a
configurable interval, and returns a pollable `UAutomationEditorTask`. Epic has
shipped this since 4.27 alongside `AScreenshotFunctionalTest` and
`TakeAutomationScreenshotAtCamera`. Engine source:
`Engine/Source/Developer/FunctionalTesting/`. Requires the `FunctionalTesting`
module in `.Build.cs`.

**Two gates bite when this is driven from inside `Automation RunTests`.**

1. `FWaitForInteractiveFrameRate` requires >= 10 FPS and times out at 600 s,
   then `AddError`s and proceeds. The 210-proxy territory with HLOD disabled
   and everything resident sustains **3-4 FPS**, so this always burns the full
   600 s and fails the test on framerate alone. Measured 2026-08-18; shaders
   were NOT compiling, this is steady-state cost.

2. `TakeHighResScreenshot`'s completion state branches on `GIsAutomationTesting`
   (`AutomationBlueprintFunctionLibrary.cpp`, `FScreenshotTakenState`):
   - inside an automation test it waits on
     `FAutomationTestFramework::OnScreenshotCompared` - i.e. screenshot
     **comparison**, not capture;
   - outside one it waits on `FScreenshotRequest::OnScreenshotRequestProcessed`
     - i.e. capture.

   With no ground-truth baseline established, `OnScreenshotCompared` never
   fires and every task hangs until its own timeout. Observed: all five
   vantages timed out at 60 s while the surrounding test correctly measured
   the territory (span 13950 m, relief 94.3 m, 210 proxies, 145 water actors)
   and solved the 6975 m overview altitude.

**Therefore:** to capture EVIDENCE images, drive `TakeHighResScreenshot` from
an editor commandlet or exec path where `GIsAutomationTesting` is false, so
completion means capture. Use the in-test path only for genuine screenshot
COMPARISON against stored `GroundTruthData`, which is what
`AScreenshotFunctionalTest` and `TakeAutomationScreenshotAtCamera` are built
for. Choosing the wrong branch is the single easiest way to lose a day here.

**File.** `tools/World/VisualVerification/` owns evidence capture. The
`ProjectWorldTerritoryVisualSweepTests.cpp` automation test that originally hit
this was removed precisely because it sat inside the wrong envelope.

---

## 14. Canonical height quantization terraces flat terrain

**Symptom.** Rendered terrain shows regular corduroy banding, strongest when
viewed top-down over near-flat ground, while the realization height gate
reports a perfect match.

**Root cause (CORRECTED 2026-08-19).** The banding is real and is in the data,
but it is INHERITED FROM THE SOURCE, not produced by `height_quantization:
0.1`. Copernicus GLO-30 hydro-flattens water bodies and plateaus low-relief and
void-filled ground, so adjacent 30 m postings are already bit-identical before
canonical compilation touches them. Measured on the raw Float32 source in
32x32 blocks, before and after applying the 0.1 m lattice:

```text
relief band   blocks   raw source   at 0.1 m   delta
0-3 m              6      1.000       1.000    +0.000
3-10 m            11      0.932       0.934    +0.002
10-25 m           28      0.378       0.397    +0.019
25+ m             32      0.002       0.025    +0.023
```

In exactly the cells that fail, quantization contributes nothing; its only
measurable effect lands on high-relief cells, which pass comfortably. The
source also declares 4.0 m vertical accuracy at 90% confidence, so the 0.1 m
lattice is already 40x finer than the data's own accuracy.

**Superseded claim.** The original entry blamed the 0.1 m lattice, citing ~50%
equal adjacent samples and ~11% distinct levels over 40 cells. Both figures were
gate artifacts: the 40-cell default sample was the first 40 filenames, which is
three of fifteen territory columns - a contiguous strip of the flattest,
water-dominated ground - and `level_utilisation` was compared against an
absolute floor it cannot reach at low relief. Over all 210 cells the territory
measures `terrace_ratio` 0.269, which passes.

**Fix.** Canonical terrain and its 0.1 m quantization are RETAINED; a finer
lattice cannot recreate variation the source does not contain. The acceptance
gate was corrected instead (see below). Residual banding in low-relief,
water-adjacent ground is a presentation concern to be addressed by terrain
material detail normals, which change no elevation.

The transferable lesson is a MISSING ACCEPTANCE DIMENSION, not a
self-referential gate. The height gate was correct and its tolerance was
legitimate: comparing FINAL against canonical at source precision is a valid
fidelity check. It simply never claimed to measure surface quality, and no
other gate did either - so a real defect was structurally invisible to a green
board. Apply the four gate-scope questions in
[canonical.md section 7](../../../../docs/agents/canonical.md): what does this
gate prove, over what authority, which defect classes are invisible to it, and
does another quality dimension need its own independent gate.

**File.** `tools/World/VisualVerification/app/surface.py`.

**Regression test.** `surface.py` gates `terrace_ratio <= 0.45` and
`supported_level_ratio >= 0.50`, both measured against canonical directly
rather than against the engine, over EVERY cell by default. The accepted
territory reports 0.269 / 0.738 and passes. `level_utilisation` is retained as
a diagnostic only: it is bounded by `(relief / quantization + 1) / samples`, so
comparing it to an absolute floor rejects legitimately flat ground.
`tools/World/VisualVerification/tests/test_surface.py` pins both the low-relief
acceptance and the undelivered-resolution rejection.

## 15. A commandlet cannot render a scene capture

**Symptom.** `USceneCaptureComponent2D::CaptureScene()` in a commandlet writes
perfectly valid PNGs of the render target's clear colour. Every structural
signal says the capture should work.

**What was verified before concluding.** All of these were true and the frames
were still black:

```text
FApp::CanEverRender()            true (-AllowCommandletRendering, no -NullRHI)
RHI                              real D3D12, feature level SM6
World->Scene                     non-null
World Partition editor cells     explicitly loaded via FLoaderAdapterShape
actors / visible primitives      378 / 566, including a DirectionalLight
capture component                registered and visible, target 1920x1080
UpdateWorldComponents + SendAllEndOfFrameUpdates
CommandletHelpers::TickEngine    one simulated engine frame, render section
                                 gated on exactly this envelope
FlushRenderingCommands           after every CaptureScene
```

**Root cause.** A commandlet is a deliberately raw host. `CaptureScene()`
enqueues a scene render through `ISceneRenderBuilder`, but the surrounding
frame lifecycle a commandlet provides is not sufficient for the capture to
composite. Chasing the remaining difference inside the renderer is not worth
it.

**Fix.** Run evidence capture in the LIVE EDITOR, which runs the ordinary frame
loop, and trigger it with the `ProjectWorld.CaptureEvidence` console command
plus `-RenderOffscreen -unattended`. Offscreen keeps a real RHI and a real
frame loop while opening no window, so the capture neither fights the operator
for foreground focus nor depends on it. The capture code itself is unchanged
between the two hosts - only the host differs.

**Lesson.** `-AllowCommandletRendering` makes *some* rendering work
(the Landscape edit-layer merge in pitfall 14 genuinely needs it and genuinely
works). It does not make a commandlet a rendering host in general. Verify the
specific rendering feature in the specific envelope before building on it.

**File.** `Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldEditorModule.cpp`,
`scripts/ue/world/capture_visual_evidence.ps1`.

**Regression test.** `tools/World/VisualVerification/tests/test_verify_capture.py`
rejects a capture set whose poses return identical bytes, which is what a
non-rendering host produces.

## 16. An empty recovery journal is not metadata-only

**Symptom.** A metadata-only manifest publication fails before commit. Running
the supported transaction recovery removes an existing generated map and the
shared presentation root even though the attempted operation never edited
content.

**Root cause.** Generated-content recovery always removes the journal's map
and presentation paths before replaying `snapshot_records`. An empty record
set means those paths were initially absent; it does not mean "do not touch
content." Writing an `apply` journal with an existing map but no real snapshot
therefore makes recovery correctly restore the wrong declared state.

**Fix.** Any operation using the generated-content journal must create its
records through `New-ProjectWorldGeneratedSnapshot` before writing the
journal, even when the intended authority change is metadata-only. Use the
smallest valid map as the recovery anchor, clean the snapshot and journal on
success, and invoke the supported recovery path on failure. Do not hand-author
an empty record set for existing content.

**File.** `scripts/ue/world/generated_content_transaction.ps1`,
`scripts/ue/world/generated_manifest.ps1`.

**Regression test.**
`scripts/ue/world/test/generated_content_transaction.Tests.ps1` proves both
directions: existing map/presentation bytes are restored from real snapshot
records, while an initially absent target is removed when the record set is
empty.

## 17. Destroying an external actor does not retire its package file

**Symptom.** Regeneration changes a cell from populated to empty. The actor is
gone from the editor world, but its old World Partition external-actor package
remains on disk and later appears as stale or unowned generated content.

**Root cause.** `UWorld::EditorDestroyActor` destroys the actor object; it does
not guarantee deletion of the actor's existing external `.uasset`. Counting
that path as a self-saved mutation can also suppress the broad world save, so
no later operation retires the file.

**Fix.** Capture the external package filename before destroying the actor,
destroy the actor, then explicitly delete the package file. Fail the operation
if either step fails.

**File.**
`Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldVegetationRealization.cpp`.

**Regression test.**
`Project.World.Realization.Vegetation.RetirementPersistence` creates an
occupied cell and an external package marker, regenerates the cell with zero
instances, and requires the package file to be absent.

## 18. Do not rebuild water meshes to classify vegetation candidates

**Symptom.** Kazan realization remains CPU-bound in vegetation input capture
after roads finish, and the Matrix can exceed its outer timeout.

**Root cause.** Rebuilding constrained Delaunay water meshes for every cell and
again for hash, apply, and capture turns a point-exclusion query into repeated
whole-feature triangulation. Large canonical water polygons make that cost
dominate the realization.

**Fix.** Classify vegetation points directly against canonical polygon rings,
holes, and ribbon clearances. Build one resolved exclusion context per cell and
reuse it for both the input hash and placement. The water mesh is a water-layer
artifact, not a vegetation input.

**File.**
`Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldVegetationExclusions.cpp`,
`Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldVegetationPlacement.cpp`.

**Regression test.**
`Project.World.Realization.Vegetation.ExclusionContract` covers polygons,
holes, ribbons, roads, masks, and input dirtiness. The Kazan Matrix exercises
the full 210-cell input capture under the acceptance timeout.

## 19. Replace components before positioning a generated cell actor

**Symptom.** Generated vegetation cells have distinct stable identities and
correct instance counts, but their actors and instances overlap near world
origin instead of occupying their canonical cells.

**Root cause.** A generic `AActor` has no transform independent of its root.
The generated HISM root was registered at identity, so the external package
owned no durable cell origin. A second lifecycle defect hid attempted fixes:
whole-layer producer drift reused actors when their old semantic tag matched,
allowing a new producer fingerprint to advance without rebuilding old bytes.

**Fix.** Assign the canonical cell origin to the new HISM root before component
registration. A whole-layer dirty marker must bypass per-actor semantic reuse.
The wrapper adds that marker whenever an accepted layer manifest carries a
stale producer fingerprint, so implementation changes reconstruct their layer
before new authority can be published.

**File.**
`Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldVegetationRealization.cpp`.

**Regression test.**
`Project.World.Realization.Vegetation.RetirementPersistence` requires the root
to own the canonical cell origin and proves whole-layer producer refreshes
rewrite unchanged cell semantics. `generated_layer_manifest.Tests.ps1`
requires stale producer fingerprints to inject whole-layer dirty work. Live
editor evidence compares vegetation and road actors for the same cell identity
after save and reload.
