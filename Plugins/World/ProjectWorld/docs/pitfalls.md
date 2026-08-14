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
