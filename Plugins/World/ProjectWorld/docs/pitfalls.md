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

**File.** `tools/World/VisualVerification/app/{census,plan_vantages}.py`;
`Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/Tests/ProjectWorldTerritoryVisualSweepTests.cpp`.

**Regression test.** `Project.World.Realization.Territory.VisualSweep` solves
the overview altitude from measured proxy bounds, so an authored constant
cannot reintroduce the framing failure.

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

**Fix.** Use the engine's own automation screenshot path,
`UAutomationBlueprintFunctionLibrary::TakeHighResScreenshot(ResX, ResY,
Filename, Camera, ...)`. It locks the level viewport to a supplied
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

**File.** `Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/Tests/ProjectWorldTerritoryVisualSweepTests.cpp`.

---

## 14. Canonical height quantization terraces flat terrain

**Symptom.** Rendered terrain shows regular corduroy banding, strongest when
viewed top-down over near-flat ground, while the realization height gate
reports a perfect match.

**Root cause.** The canonical profile declares `height_quantization: 0.1`, so
every sample snaps to a 10 cm lattice. On Kazan's floodplain that produces flat
plateaus separated by 10 cm cliffs: measured over 40 cells, ~50% of adjacent
samples are EXACTLY equal and only ~11% of possible levels are distinct. Flat
plateaus have constant normals and the cliffs are normal discontinuities, which
shading renders as bands. This is data, not a shading artifact, and it is NOT
nearest-neighbour upsampling (run-length analysis shows no spike at k=2/3).

**Fix.** Not yet chosen - reduce quantization, or accept it and rely on a
terrain material with detail normals. What IS fixed is that it can no longer
pass unnoticed; see the self-referential-gate rule in
[canonical.md](../../../../docs/agents/canonical.md).

**File.** `tools/World/VisualVerification/app/surface.py`.

**Regression test.** `surface.py` gates `terrace_ratio <= 0.45` and
`level_utilisation >= 0.20`, both measured against canonical directly rather
than against the engine. It currently FAILS at 0.498 / 0.111, which is the
correct report for the present data.
