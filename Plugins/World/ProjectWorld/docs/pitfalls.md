# ProjectWorld Pitfalls

Verified bug archive for generated-world realization. Read before touching
the area; add an entry after fixing a non-trivial bug.

Format: Symptom / Root cause / Fix / File / Regression test.

## Runtime presentation must make the Sun movable before rotating it

**Symptom.** Presentation reports the selected profile, but an initialized Game
or PIE world's Sun keeps its spawn rotation and emits a mobility warning.

**Root cause.** Runtime movement guards reject rotating the registered stationary
light before its mobility changes. An uninitialized Game-world fixture does not
exercise that guard.

**Fix.** Set movable mobility before applying rotation. Read actual actor state
when verifying runtime presentation.

**File.** [Presentation realization](../Source/ProjectWorldEditor/Private/ProjectWorldPresentationRealization.cpp).

**Regression test.** `Project.World.Realization.Presentation.GameRotation`
initializes actors before applying the real profile and compares actual rotation
and intensity. The regional PIE observer also reads back Sun/exposure settings.

## Distant Water footprints dominate dependency selection

**Symptom.** A local Terrain comparison spends minutes finding affected Water
cells even though fitting finishes quickly.

**Root cause.** Dirty-cell selection repeats raw footprint distance work for
every halo sample, including samples far outside the feature bounds.

**Fix.** Reuse the existing prepared geometry once per old/new Water feature.
Keep continuous footprint/halo intersection and both declared memberships.

**File.** [Terrain surface owner](../../../../tools/World/CanonicalCompilation/app/terrain_surface.py).

**Regression test.** `TerrainSurfaceTests` rejects segment work for a disjoint
footprint and compares dirty scope with raw geometry for holes, ribbons, narrow
polygons and halo bands. Existing cold/incremental Water tests cover consumers.

## Regional views must select one compiled visual variant

**Symptom.** A native fixture authenticates its complete Terrain fields but its
capture includes coincident Nanite and fallback meshes.

**Root cause.** The fixture deliberately builds and reloads all alternatives.
Platform loading policy does not hide explicitly co-resident compiled actors;
enumerating every main mesh component includes both visual variants.

**Fix.** Capture one declared visual variant. Suppress other main components'
visibility and shadow casting for that scope, then restore their original state
on every exit. Keep field/collision verification for all alternatives. A useful
image still needs its own lighting, framing and rendering proof.

**File.** [Canonical regional observer](../../ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/Private/Tests/ProjectWorldMeshTerrainCanonicalFixture.cpp).

**Regression test.** `Project.World.Realization.MeshTerrain.CanonicalRegion`
rejects the two-variant capture and records the selected variant and positive
component census. Multiple compiled sections of the same variant are valid.

## Disposable physics probes can leak external actor packages

**Symptom.** A native regional Terrain check passes collision calibration but
saved replay refuses an unsaved external actor package.

**Root cause.** The foreign-box negative control was created as an ordinary
actor in an external-actor world. Destroying the actor did not remove its package
from the world's package ownership census.

**Fix.** Spawn the calibration actor as transient and explicitly disable actor
package creation. Retain the exact saved-package census and missing-file refusal;
do not hide the leaked package by filtering the census.

**File.** [Collision calibration](../../ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/Private/Tests/ProjectWorldMeshTerrainCollisionFixture.cpp).

**Regression test.** `Project.World.Realization.MeshTerrain.PhysicsProbePackageIsolation`
calls the real calibration in an external-actor world and compares the complete
package census. `CanonicalRegion` also checks all saved packages through fresh
map loading, explicit actor residency and actual Terrain/Water reconstruction.

## A material object does not prove native Terrain appearance

**Symptom.** Saved Terrain passes whole-field and collision comparisons while
the image uses fallback shaders or lacks the generated channel resources.

**Root cause.** Direct transformer injection bypassed native provider/channel
generation, and material-object availability was mistaken for platform shader
readiness.

**Fix.** Regional appearance uses the native builder, requires the section's
channel texture/table and checks the actual component material's completed
platform shader resource. Keep controlled diagnostic images separate from World
lighting and geographic acceptance. Inspect the images even when these guards
pass.

**File.** [Canonical regional observer](../../ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/Private/Tests/ProjectWorldMeshTerrainCanonicalFixture.cpp).

**Regression test.** `Project.World.Realization.MeshTerrain.CanonicalRegion`
requires actual channel resources and shaders after fresh saved-map loading.
Its all-field comparison also covers unchanged canonical grids; its diagnostic
images do not establish full-world visual acceptance.

## A neighborhood maximum can hide new terrain creases

**Symptom.** A local height correction improves four provider comparisons and
keeps the maximum neighborhood slope unchanged, while boundary faces steepen.

**Root cause.** An already steeper unchanged face dominates the maximum. Neither
that scalar nor source agreement observes each face or its external neighbor.

**Fix.** Report every incident face gradient and both internal/external edge
normal angles. Keep shape quality distinct from source departure and topology;
do not infer visual or gameplay acceptance from the unchanged maximum.

**File.** [Madison observer](../../../../tools/World/CanonicalCompilation/tests/madison_ground_replay.py).

**Regression test.** `MadisonGroundTests.test_face_observer_detects_changes_hidden_by_unchanged_global_maximum`
retains the unchanged 49.3% maximum while detecting six steepened faces and the
larger external crease. Native collision/rendering need their separate checks.

## Different float frames can reopen a shared shoreline

**Symptom.** An inferred Water boundary preserves canonical nodes and the exact
stage, but saved Water and supporting Terrain differ by up to 14.35 micrometers
at the shore. Source fidelity alone misses this physical composition error.

**Root cause.** Terrain stored engine-global float positions while Water stored
cell-local float positions plus a double actor origin. Independent casts move the
same canonical boundary differently. Equal-height grouping or later horizontal
union can also replace a joint boundary under another source's coordinate rule.

**Fix.** A joint-model unit uses one engine-global storage frame for all groups,
preserves each joint source's coordinate rule and requires exact independent
interiors both before emission and after source approximation. It skips final
union. Unsupported interactions refuse the complete unit. A scope-exit cleanup
clears all joint-unit outputs on every failure, including a later group's missing
preparation evidence after an earlier group emitted successfully.

**Files.** [Cell composition](../Source/ProjectWorldWaterEditor/Private/ProjectWorldWaterCellMesh.cpp)
and [independence guard](../Source/ProjectWorldWaterEditor/Private/ProjectWorldWaterJointBoundary.cpp).

**Regression tests.** `Project.World.Water.IdentifiedBasinSharedFrame` covers
mixed-family frames, unknown late preparation, old-old overlap and overlap
introduced only by encoding. `IndependentBoundaryGuard` covers exact contacts,
opposite winding, nonfinite/degenerate inputs and bounded pair scans. Stored
shoreline acceptance still requires complete field/edge overlay against the
actual supporting Terrain; this does not certify later section generation.

## Native Water can re-quantize an already-noded basin boundary

**Symptom.** Water has the correct stored stage, but its saved footprint crosses
the supporting bank. The captured joint basin has 0.018 m2 of Water below higher
Terrain, with up to 1.67 mm negative depth.

**Root cause.** Native emission applied the base 0.01 m coordinate quantization
again to an inferred model's already-qualified finite boundary. All 26 added
boundary nodes were replaced. An input encoding certificate and exact stored
stage cannot qualify this later geometric change.

**Fix.** Only `identified_static_lined_basin:v1` preserves supplied canonical XY
before float storage. Other function families retain their coordinate behavior.
Joint stored-field overlay remains a separate requirement: Water and Terrain can
still encode coordinates in different float frames.

**File.** [Water mesh builder](../Source/ProjectWorldWaterEditor/Private/ProjectWorldWaterMeshBuilder.cpp).

**Regression test.** `Project.World.Water.IdentifiedBasinCanonicalXY` compares all
45 specifying nodes with their direct source-to-float conversion and rejects
missing or centimeter-snapped replacement nodes.

## An inferred contained stage can inherit the ordinary Water lift

**Symptom.** A typed inferred basin at 20.20 m saves Water at 20.45 m, above
its modeled retention rim. Changing its spill also leaves unit/verification
identities unchanged.

**Root cause.** Exact-stage behavior and spill dependency hashing recognized only
the observed contained function ID. A distinct identified model fell through to
the ordinary presentation offset, despite carrying an explicit spill bound.

**Fix.** One closed native family classifier recognizes both explicit contained
IDs. Stage validation, grouping and source approximation share the exact-level
rule; unit and verification identities bind the consumed spill. Invalid versions
or spill bounds refuse before emission and clear the result. Canonical JSON
admission remains closed to the inferred model pending separate qualification.

**Files.** [Contained-stage classifier](../Source/ProjectWorldEditor/Public/ProjectWorldContainedStage.h)
and [Water mesh builder](../Source/ProjectWorldWaterEditor/Private/ProjectWorldWaterMeshBuilder.cpp).

**Regression tests.** `Project.World.Water.IdentifiedBasinTypedConsumer` uses the
complete captured 45-node basin through source construction, composition and
StaticMesh save/reload. `Project.World.Water.ContainedFunctionAdmission` retains
the closed JSON boundary; `Project.World.Water.ExactCanonicalLevel` covers the
existing observed and ordinary families.

## A derived Terrain clip can invent an overlap between disjoint stored faces

**Symptom.** Saved compiled Terrain retains the specified face census and height
field, but the fidelity observer reports overlapping compiled faces. Independent
exact checks of the actual stored triangles find no positive overlap.

**Root cause.** A derived Boolean intersection is a floating reconstruction.
Testing that reconstructed clip against another stored face can report an overlap
outside the clip's actual source face. Counts and small height errors cannot
distinguish this observer defect from real bad coverage.

**Fix.** Use the engine's exact orientation predicates to certify a separating
edge between the two actual triangles before testing their derived clips. Only
provably disjoint interiors skip that overlap branch. Coverage, height and real
overlap checks retain their existing limits; unknown or degenerate predicates do
not supply a separation certificate.

**File.** [Terrain fidelity observer](../../ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/Private/ProjectWorldMeshTerrainSurfaceFidelity.cpp).

**Regression test.** `Project.World.Realization.MeshTerrain.CompositeConsumer`
uses a self-contained captured quad through native conversion, save and reload.
Its planar positive control and equal-area overlap/hole control distinguish valid
separation from an observer that merely accepts missing or duplicate coverage.

## Canonical restoration can erase measured Terrain coverage

**Symptom.** Complete compiled Terrain is rejected for a small area deficit even
though an independent exact replay certifies complete coverage and valid heights.

**Root cause.** Native triangle clipping computes accurate local intersection
points. Restoring those derived points to large canonical coordinates before
area, height and overlap measurements introduces another rounding boundary.
A captured four-piece cover loses 1.143e-8 m2 through that restoration alone.

**Fix.** Keep both compared triangles and every derived measurement in one local
frame per specifying face. Global lookup, original-triangle exact separation and
winding guards retain their original inputs. Coverage and height limits stay
unchanged; no measured positive face is discarded to make the result pass.

**File.** [Terrain fidelity observer](../../ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/Private/ProjectWorldMeshTerrainSurfaceFidelity.cpp).

**Regression test.** `Project.World.Realization.MeshTerrain.CoastalIntersection`
uses the captured cover and missing, duplicate, shifted and reversed controls.

## Collision provider input is not the final cooked surface

**Symptom.** A stored bank passes field checks, but a ray at one specified facet
misses while a full-size capsule finds neighboring support.

**Root cause.** `GetPhysicsTriMeshData` exposes provider input. Native cooking can
remove a facet below its validity threshold, so a created body and the input
census do not establish final physical coverage. A captured bank has 80 provider
facets and 79 cooked facets, with the removed centroid unsupported.

**Fix.** Decode actual cooked geometry and retain independent input-centroid
support queries. Retessellate the derived model within its existing field,
boundary and role constraints; keep the old missed point as a separate witness
before and after reload. Do not suppress a known unsupported point through a
smaller probe census, a numeric area waiver or disabled native cleanup.

**Files.** [Cooked collector](../../ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/Private/Tests/ProjectWorldMeshTerrainSectionCollection.cpp)
and [support checks](../../ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/Private/Tests/ProjectWorldMeshTerrainCollisionFixture.cpp).

**Regression test.** `Project.World.Realization.MeshTerrain.CoastalCookSupport`
retains both complete original quads, the repaired field and the old centroid.
The self-contained fixture exporter can also emit the original failing bank.

## Realization evidence can alias consumed inputs

**Symptom.** A diagnostic receipt destroys a canonical document, profile or
disposable manifest authority instead of preserving failed-operation inputs.

**Root cause.** A permitted output directory can also contain inputs. Result
replacement, log files, atomic `.tmp` files and generated dirty-input documents
are independent writers; checking only the final result filename misses aliases.

**Fix.** Require disjoint input/output paths before host deletion or native
realization. Protect the entire canonical and host manifest trees, explicit
profile inputs, all sidecars and generated dirty-input writes. The direct native
result writer enforces its own input isolation as well.

**Files.** `scripts/ue/world/realization_evidence.ps1` and
`Source/ProjectWorldEditor/Private/ProjectWorldRealizationEvidence.h`.

**Regression tests.** `Project.World.Realization.EvidenceInputIsolation`,
`CommandletBoundary`, `EvidencePaths`, `Layers.HostBoundary` and
`scripts/ue/world/test/realization_evidence.Tests.ps1`. Refusal checks compare
exact protected bytes and run before the native operation starts.

## Profiled Road splits can fail native storage or compiled fidelity

**Symptom.** A valid affine Road elevation refuses after subdivision at large
projected coordinates, or its saved fallback and physics footprint differs from
its source even though sampled collision rays hit.

**Root cause.** Internal height-field splits can map to identical double or
float positions. Per-face conversion can create conflicting shared heights.
Rounding XY can also move a referenced vertex across a surface crease while
retaining the height sampled before conversion.
Native Boolean integer normalization can also split a connected collinear float
graph. Separately, Nanite Auto fallback intentionally simplifies the source;
centroid ray hits do not prove footprint or abutment preservation.

**Fix.** Omit only exact duplicated mapped fan edges and certify storage against
each original ribbon domain. Retain native coverage and distance guards, with
GeometryCore exact predicates for connectivity of the actual stored positions.
For referenced XYZ fields, validate raw aliases first, then sample the complete
field at actual stored XY and bind one final float height per exact XY/profile.
Recheck final Terrain clearance. Profile-containing cell meshes use full native
fallback without source trimming and include that variant in unit identity;
ordinary cells keep their existing path. The
[Road contract](territory_contract.md#geospatial-authority) owns these boundaries.

**Files.** `Source/ProjectWorldRoadEditor/Private/ProjectWorldRoadElevation.cpp`,
`ProjectWorldRoadStoredPolygon.cpp`, `ProjectWorldRoadSurface.cpp` and
`ProjectWorldRoadRealization.cpp` in the same directory.

**Regression tests.** `Project.World.Realization.Road.MappedSplit`,
`StoredPolygon`, `StoredGroup`, `StoredCollinear`, `CanonicalElevation` and
`ElevationSavedConsumer` and `ReferencedProjectedStorage` under the same exact
test namespace. The saved-consumer
test checks actual triangle/position multisets, cold no-op and profile removal;
live pixels and character movement remain separate evidence.

## Continuous alignment can fail despite matching mesh vertices

**Symptom.** A road ribbon intersects terrain between its vertices, or a narrow
water surface is occluded despite sampled water clearance. Nearby source-road
changes can also retain an unchanged road input hash.

**Root cause.** Bilinear centerline sampling does not represent the canonical
terrain's two affine triangle planes or ribbon width. Lattice-point membership
misses water contained between samples. A flowing function's local height does
not bound every chord in a realized water triangle. Tangent context omitted from
road identity permits reuse of geometry built from different inputs.

**Fix.** Partition road ribbons at canonical triangle intersections and hash
the same bounded tangent and neighboring terrain inputs. Canonical hydro support
constrains corners of intersected quads under explicit quality limits and uses
a conservative flowing-knot floor. Include old/new halo support in dirty closure.
The [territory contract](territory_contract.md#geospatial-authority) owns policy
and authority; whole-territory regeneration cannot repair missing input identity.

**Files.** `Source/ProjectWorldRoadEditor/Private/ProjectWorldRoadRealization.cpp`,
`Source/ProjectWorldRoadEditor/Private/ProjectWorldRoadSurface.cpp`,
`Source/ProjectWorldEditor/Private/ProjectWorldTerrainSurface.cpp`, and
`tools/World/CanonicalCompilation/app/terrain_water_coverage.py`.

**Regression tests.** `Project.World.Realization.Road.FullFootprintConformity`
(source, compiled fallback, save/reload and collision),
`Project.World.Realization.Road.ConsumedGeometryIdentity`,
`Project.World.Realization.Alignment.TriangleClearanceOracle`,
`Project.World.Realization.Alignment.WaterTriangleEnvelope`, and
`tools.World.CanonicalCompilation.tests.test_water_coverage.WaterCoverageTests`.

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

**File.** Presentation implementation files compiled into the same ProjectWorld
unity translation unit. File-local helper names and constants must remain
distinct across those files.

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
and `TerritoryTwinTests`.

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
`TerritoryTwinTests`.

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

**Regression test.** `plan_vantages` solves the overview altitude from the
bounds of the descriptors carrying the terrain runtime role, so an authored
constant cannot reintroduce the framing failure.
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

A whole-territory census in milliseconds: terrain and water presence by the
runtime roles in the descriptor `Tags`, spatial-loading flags, bounds, extent,
relief, lighting set. No rendering, no editor build. Run this BEFORE any visual
step. Note the dump is space-delimited `key:value`, not real CSV, and
always-loaded actors (e.g. `DirectionalLight`) carry `IsValid=false` with no
`Min`/`Max`, so bounds must be parsed optionally or those rows vanish and the
lighting check falsely fails.

**File.** `tools/World/VisualVerification/app/actor_descriptors.py` (dump
format), `census.py`.

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

---

## 16. Loaded actors do not prove startup rendering is settled

**Symptom.** The unattended evidence receipt is accepted and reports all 861
actors, 210 Landscape proxies and 1180 visible primitives, but the PNGs contain
disconnected checkerboard Landscape components. The same map renders coherently
after the live editor has advanced normally.

**Root cause.** `-ExecCmds` runs during editor startup. Loading the complete
World Partition bounds and calling `UpdateWorldComponents` does not make the
new render state safe to capture in that same engine frame. A render-thread
flush completes queued work, but it does not replace the missing game/editor
frame boundaries.

**Fix.** Keep the loader adapter alive and defer `CaptureScene()` until three
distinct editor frames have passed with no pending asset compilation. Apply a
bounded timeout and write a rejected receipt if readiness never arrives.

**File.** `ProjectWorldEditorModule.cpp`,
`ProjectWorldEvidenceReadiness.h`.

**Regression test.** `Project.World.Evidence.ReadinessContract` pins the
three-frame, compilation-reset rule. The rendered replacement gate is the cold
`capture_visual_evidence.ps1` route itself; operator inspection must still
confirm semantic coherence because hashes and dimensions cannot detect this
failure class.

## 17. An empty recovery journal is not metadata-only

**Symptom.** A metadata-only manifest publication fails before commit. Running
the supported transaction recovery removes an existing generated map even
though the attempted operation never edited content.

**Root cause.** Generated-content recovery always removes the journal's map
paths before replaying `snapshot_records`. An empty record
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
directions: existing map bytes are restored from real snapshot
records, while an initially absent target is removed when the record set is
empty.

## 18. Destroying an external actor does not retire its package file

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

## 19. Do not rebuild water meshes to classify vegetation candidates

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

## 20. Replace components before positioning a generated cell actor

**Symptom.** Generated vegetation cells have distinct stable identities and
correct instance counts, but their actors and instances overlap near world
origin instead of occupying their canonical cells.

**Root cause.** A generic `AActor` has no transform independent of its root.
The generated HISM root was registered at identity, so the external package
owned no durable cell origin. A second lifecycle defect hid attempted fixes:
whole-layer producer drift reused actors when their old semantic tag matched,
allowing a new producer fingerprint to advance without refreshing old bytes.

**Fix.** Assign the canonical cell origin to the new HISM root before component
registration. A stale producer fingerprint marks only its own layer
identity-dirty. The producer checks that identity during actor reuse; content
dirty units still propagate only from changed canonical or profile inputs.
An output implementation change requires the owning descriptor's output
revision to advance and its verify baseline to be re-recorded.

**File.**
`Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldVegetationRealization.cpp`.

**Regression test.**
`Project.World.Realization.Vegetation.RetirementPersistence` requires the root
to own the canonical cell origin and proves whole-layer producer refreshes
rewrite unchanged cell semantics. `generated_layer_manifest.Tests.ps1`
requires stale producer fingerprints to mark only their own layer
identity-dirty. Live
editor evidence compares vegetation and road actors for the same cell identity
after save and reload.

## 21. Configure unbound post-process volumes before making them nonspatial

**Symptom.** Editor structure and product-route checks pass, but the packaged
Kazan screenshot is an all-white frame under the generated sun. The intended
fixed-exposure post-process actor is absent after World Partition streaming.

**Root cause.** `APostProcessVolume::CanChangeIsSpatiallyLoadedFlag()` returns
false while the volume is unbound. Setting `bUnbound` first therefore prevents
the generator from clearing `bIsSpatiallyLoaded`; the actor stays spatial at
world origin and can unload during real gameplay.

**Fix.** Temporarily keep the volume bound, set it nonspatial, then make it
unbound. Current-state validation must also reject an existing unbound spatial
actor so regeneration migrates legacy packages instead of accepting them.

**File.**
`Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationRealization.cpp`.

**Regression test.**
`Project.World.Realization.Presentation.ActorLifecycle` proves both new actor
creation and migration of the legacy streamed state. Product screenshot
validation independently rejects uniform white and black frames.

---

## 22. Runtime-only code must not fingerprint generated geography layers

**Symptom.** Changing a World Partition runtime profile advances all six Kazan
generated-layer scopes and can rewrite otherwise frozen actor or mesh packages.
A candidate tournament then compares different world bytes instead of one
world under different streaming profiles.

**Root cause.** There were two propagation paths. Runtime partition and
evidence-host sources were once included in the shared producer fingerprint.
After that was corrected, the realization profile compiler still included
`runtime_profile_id` in the execution hash consumed by every normalized layer
contract. The first locality harness changed only the `-RuntimeProfile`
argument, not the realization SOT field, so it produced a false green result.

**Fix.** The map's output revision and profile input identity own runtime
partition changes; producer fingerprints exclude source and evidence-host text.
Record `none` as
the runtime identity of every geography/gameplay layer. Keep the full
realization execution hash for the map, but derive layer contracts from a
second execution identity that excludes `runtime_profile_id`. Exercise copied
realization SOTs in owner-confined `tmp/world` authority and perform the
one-time honest layer reconstruction before claiming locality.

**File.** `ProjectWorldRealizationProfile.cpp`,
`scripts/ue/world/generator_fingerprint.ps1`,
`scripts/ue/world/realization_layer_operation.ps1`, and
`scripts/ue/world/test/integration/runtime_profile_locality.ps1`.

**Regression test.** `generator_fingerprint.Tests.ps1` proves revision and data-input locality;
`generated_layer_manifest.Tests.ps1` proves semantic stability; the production
`runtime_profile_locality.ps1` integration changes the copied realization SOT
`512/1536 -> 128/768 -> 512/1536` while requiring all six manifest entries and
artifact hashes to remain identical.

---

## 23. A runtime profile switch must not retire stable route actors

**Symptom.** A candidate profile Apply either cannot reclaim
`ProjectWorld_PlayerStart` or reaches runtime acceptance with zero always-loaded
player starts, even though the baseline map was valid.

**Root cause.** Pre-Apply cleanup required the actor's mutable runtime-profile
tag to equal the requested candidate. It destroyed the stable grid-and-role
actor before runtime realization could update the tag. The pending UObject then
retained its deterministic name until garbage collection.

**Fix.** Treat same-grid runtime-role actors as current whenever runtime
realization is requested, and keep this policy behind the map-owned runtime
contract. Runtime identity reconciliation retires malformed competing owners
before creating one deterministic replacement.

**File.**
`Source/ProjectWorldEditor/Private/ProjectWorldGeneratedActorLifecycle.cpp`
and `Source/ProjectWorldEditor/Private/ProjectWorldRuntimeRealization.cpp`.

**Regression test.**
`Project.World.Realization.Runtime.ProfileSwitchLifecycle` preserves stable
identity across a candidate switch;
`Project.World.Realization.Runtime.StaleIdentityReplacement` converges stale
role, GUID, and object-name owners.

---

## 24. Canonical-cell ownership is not runtime base-cell containment

**Symptom.** A static audit rejects every bounded runtime candidate because one
930 m canonical-cell actor intersects more than four 128 m, 256 m, or 512 m
runtime base cells, despite correct packaged streaming and stable ownership.

**Root cause.** The audit treated the World Partition base-cell size as the
generation ownership unit. Kazan generation is owned by frozen 930 m canonical
cells. World Partition may assign a larger actor to a higher hierarchy level;
road ribbon bounds may also extend slightly beyond the cell rectangle while
remaining owned by one `RoadCell` identity.

**Fix.** Validate cell-local layer actors against their canonical identity and
the realized Mesh Terrain authoring-cell extent. Record runtime-cell intersection
and package attribution as candidate metrics. Fail invalid bounds, missing cell
identity/packages, undeclared non-terrain reference bundles, or broken Mesh
Terrain/Data Layer/HLOD policy; do not invent a universal four-cell limit.

**File.** `ProjectWorldStaticPartitionAudit.cpp` and
`scripts/ue/world/audit_runtime_partition.ps1`.

**Regression test.** `Project.World.Realization.Runtime.StaticPartitionCellSpan`
proves the base-cell counting math. The read-only production audit verifies all
three Kazan candidates against the same 850 generated actors without saving the
map.

---

## 26. Compile provenance is not generated-world product semantics

**Symptom.** A content-identical incremental compile rejects D3 even though geometry,
semantic ownership, collision, and presentation are unchanged. Editing only the D3
evidence serializer can also stale every active geography producer fingerprint.

**Root cause.** `ProjectWorld.Input=<compile receipt hash>` was included in the product
semantic fingerprint, and the evidence serializer was included in the former
generated-byte source hash. Both are evidence/provenance surfaces, not generated content.

**Fix.** Exclude only the compile-receipt provenance tag from D3 while retaining cell,
grid, geometry, and other semantic tags. Producer identity now uses declared
revisions and data inputs, not evidence serializer source. A content-identical incremental compile may
advance provenance without claiming a different realized World.

**File.** `ProjectWorldSemanticEvidence.cpp` and
`scripts/ue/world/generator_fingerprint.ps1`.

**Regression test.**
`Project.World.Realization.Presentation.SemanticFingerprintIgnoresInputProvenance`
proves provenance stability and semantic sensitivity;
`generator_fingerprint.Tests.ps1` proves evidence-only source changes move no producer.

---

## 27. Water material correctness does not prove a visible surface

**Symptom.** Generated Water actors are loaded, visible, no-collision, and reference a
compiled blue Single Layer Water material, but Landscape completely covers the surface
from every normal above-water camera. An opaque diagnostic material is also invisible
from above and becomes a solid plane when viewed from below.

**Root cause.** The mesh builder normalized XY triangles to counter-clockwise winding.
Unreal renders clockwise triangles as front-facing, so the generated water faces down.
Material, cook, streaming, and elevation checks cannot detect this geometry error.

**Fix.** Preserve the authored +Z vertex normals while reversing every positive-Z
cross-product triangle to clockwise winding before adding it to the MeshDescription.
Do not hide the defect with a two-sided material.

**File.**
`Source/ProjectWorldEditor/Private/ProjectWorldWaterMeshBuilder.cpp`.

**Regression test.**
`Project.World.Realization.NativeTwin.WaterCanonicalContract` requires every generated
water polygon to be a clockwise triangle in Unreal space. Live Editor and packaged
visual evidence must still frame a known water surface from above.

---

## 28. A visible Water surface can still z-fight with terrain

**Symptom.** Water is blue and front-facing, but broad reservoirs alternate between
blue and terrain green in triangular patches. A repeated fixed-camera capture changes
which pixels classify as Water even though neither camera nor canonical data moved.

**Root cause.** A Water estimator may legitimately produce the same or a lower Z than
the sampled DSM beneath it. Coplanar triangles z-fight; positive DEM intrusions can
hide Water completely. A reflective Single Layer Water placeholder adds presentation
noise without solving either vertical-order failure. A constant Water offset only fixes
the coplanar case and cannot safely cover multi-metre DEM disagreement.

**Fix.** Canonical Compilation preserves the sampled terrain as `water_fit`, derives
the final terrain surface below exact Water footprints with the configured clearance,
then applies authored terrain patches and validates the clearance again. The Mesh
Terrain adapter consumes that final surface without another clamp. The replaceable
Water mesh independently applies the profile-owned positive `surface_offset_m`. Do not
raise Water by the worst DEM error, bend Water to Terrain, encode the correction only
in material, or enable a second Water authority.

**Files.**
`tools/World/CanonicalCompilation/app/terrain_surface.py`,
`ProjectWorldLayerInventory.cpp`, `ProjectWorldWaterMeshBuilder.*`, and the owning
compiler and realization profiles.

**Regression tests.**
`tools/World/CanonicalCompilation/tests/test_terrain_surface.py` pins the staged
vertical order, authored-patch order, clearance, and dry-Terrain locality;
`Project.World.Realization.Layers.PersistentWater` pins the metric offset and
Water geometry contract;
`Project.World.Water.CoplanarRibbonComposition` pins native equal-plane union,
final quantized disjointness, holes, seams, source provenance and height preservation;
`scripts/ue/world/test/water_temporal_stability.ps1` rejects repeated-pose Water pixel
classification flips and color drift.

**Additional cause: overlapping ribbon caps.** A single Water actor can contain
multiple coplanar surfaces when independent buffered flow branches are appended.
An actor/asset duplicate census cannot detect this. Equal constant-plane footprints
are composed by the Water cell builder through native polygon union; different or
nonconstant height functions remain explicit unresolved cases. Inspect final
triangles, not just source actor counts. MeshPartition's virtual-texture fallback
component is also not a second visible terrain plane: native setup disables its
main pass, depth pass, ray-tracing visibility and shadows. Verify those actual flags
before deleting an apparent duplicate component.

**Additional cause: PIE authoring/compiled overlap.** Native MeshProviderModifier
marks its DynamicMeshComponent Editor-only, but that flag does not hide the PIE
copy. Coincident authoring and compiled surfaces can produce identical sampled
depths while their shading competes during camera motion. Equal depth therefore
does not exclude double drawing. The Mesh Terrain partition hides only its tagged
source actors' Editor-only primitives at Game-world startup and registration.
`Project.World.Realization.MeshTerrain.AuthoringVisibility` covers the native
provider default, Game/PIE suppression, Editor preservation, late registration,
unrelated/runtime primitive exclusion and listener cleanup. Use matched moving
views and renderer-isolation controls, not only actor/asset duplicate keys.

---

## 29. A broad final-arrival radius makes collision-slide evidence nondeterministic

**Symptom.** The packaged playable-tour route reaches the edge and returns successfully,
but some runs descend in a dense collision pocket and record zero forward slide. Repeating
the same source can land near a usable facade and pass, so an ordinary package GREEN does
not explain the variance.

**Root cause.** The broad traversal arrival radius is appropriate for streamed territory
waypoints, but a loose final center return can descend in a dense collision pocket. Even
25 m leaves Manhattan's origin-side return far enough from the intended collision-proof
area to record zero forward slide. Position alone also counts a fast pass through the
precise center as arrival. Immediate vertical input suppresses native flight braking,
allowing drift before contact. Trying unrelated lateral keys only measures arbitrary
horizontal escape and weakens the meaning of "forward input slid along a blocking surface."

**Fix.** Keep the broad radius for territory traversal and the default 25 m final
return for Kazan. The Manhattan operation explicitly selects a 5 m final return;
the receipt records that radius. The precise operation coasts through native flight
braking and requires settled horizontal motion before descent. This settlement is
scoped to precise return: changing Kazan's descent starting state can land it in a
different blocked pocket, and a smaller radius alone does not prevent that.
Preserve one forward `W` slide after blocked
descent; do not add direction retries as a substitute for deterministic arrival.

**File.**
`Source/ProjectWorld/Private/Presentation/ProjectWorldPlayableTourDriver.cpp` and
`scripts/ue/world/test/performance/run_manhattan_showcase_prototype.ps1`.

**Regression test.**
`Project.World.PlayableTour.CenterArrival` rejects a fast precise-center pass and
preserves standard arrival semantics. The uncooked admission runs this native test.
The permanent [uncooked gameplay admission](../../../../scripts/ue/world/README.md#uncooked-gameplay-admission)
must prove real return, descent and forward slide on both production maps before
requesting a frozen release. Final packaged Development still re-proves at least
1 m of slide and authenticates real input, collision, streaming and performance
in the same process. The Manhattan wrapper rejects missing slide/input proof.

---

## 30. Full-frame repeated captures can measure sky instead of Water

**Symptom.** A Shipping Water pair looks visually stable, but the temporal verifier
reports many blue classification flips. A valid Water target can also be rejected because
an older route required an unrelated absolute travel distance.

**Root cause.** A player-context camera includes blue sky, and recreating its transient
`SceneCapture2D` for each PNG discards the rendering state that
`bAlwaysPersistRenderingState` is meant to preserve. Full-frame blue classification then
cannot attribute changes to Water. UE base-color capture PNGs can also store meaningful
RGB with alpha zero; alpha-compositing those images through GDI+ turns the classified
pixels black. Absolute path length is also not evidence that the configured target was
reached.

**Fix.** Keep one capture actor, component, render target, transform, and projection alive
for both images. Anchor a top-down orthographic `SCS_BaseColor` capture to the authenticated
Water coordinate. This is the same built-in capture source used by UE's World Partition
minimap and preserves geometry/depth while removing sky and lighting noise. Accept travel
from the actual start-to-target distance, arrival radius, final error, and accumulated
real-input path; do not replace the stale distance with another magic constant.

Read the capture's BGRA bytes without alpha composition. The alpha channel is not Water
identity; the blue classifier owns RGB geometry/material evidence. Do not promote that
diagnostic pass into product-appearance evidence: capture one additional target-centered
`SCS_FinalColorLDR` frame with normal show flags and inspect those exact Shipping pixels.

**Files.**
`Source/ProjectWorld/Private/Presentation/ProjectWorldRuntimeScreenshotCapture.*`,
`ProjectWorldShippingWaterProofGate.*`, and
`scripts/ue/world/test/performance/run_kazan_playable_tour.ps1`.

**Regression tests.**
`ProjectWorld.PlayableTour.ShippingWater.TargetRelativeTravel` pins the derived travel
contract. `water_temporal_stability.Tests.ps1` pins UE zero-alpha base-color blue,
Water-to-ground flips, and color drift. The exact packaged Shipping Water process proves
the persistent capture session and real rendered Water/Landscape boundary, while its
separate final-color image proves the normal product presentation.

---

## 31. Requested performance flags are not runtime pacing evidence

**Symptom.** A packaged run clusters near the 60 Hz boundary and is classified as a
content regression from Frame/Render p95 alone. Its launcher uses `-VSync=0`, which
looks like a request to disable VSync but is not UE boolean-flag syntax.

**Root cause.** UE parses VSync as the boolean `-vsync` or `-novsync` flag. Requested
command-line text does not prove the state used during sampling, and other pacing
owners such as `t.MaxFPS`, frame smoothing, fixed frame rate/time step, benchmark mode,
or dynamic resolution can independently change the measurement envelope. In the
2026-09-01 rejected run, the in-process gate log already showed `r.VSync=0` and
`t.MaxFPS=0`, so the ambiguous flag was real harness debt but did not prove the measured
miss was caused by VSync.

**Fix.** Launch with `-novsync`. In the one-shot acceptance process, establish and read
back the uncapped native-resolution envelope before sampling, record every value in the
performance receipt, and fail closed on a contradiction. Do not weaken the frame budget,
select the best rerun, enable fixed-timestep benchmark behavior, or optimize World content
without a localized runtime workload.

When identical package bytes show material host/render scheduling variance, use one fixed
three-execution sample rather than retrying until green. Preserve every child. Pool the
exact C++ collector frames and apply the unchanged nearest-rank p95 once. UE's rich CSV is
diagnostic evidence, not that exact sample authority: retained 2026-09-01 runs contained
six additional numeric rows and recomputed to slightly different p95 values. The producer
therefore emits a separate exact sample projection. PowerShell consumers must also test
that every envelope property exists before casts, because a missing property can otherwise
become zero or false and appear valid.

UE can log `RequestExitWithStatus(..., 10)` for an individual performance-only miss while
the packaged Windows process object reports normal exit 0. Classify the child from its
authenticated receipt plus normal-exit log and retain its frames for pooling when that is
the only rejection. Record the observed process code; do not manufacture 10. Any abnormal
process exit or non-performance error still fails immediately.

**Files.**
`Source/ProjectWorld/Private/Presentation/ProjectWorldPerformanceEnvelope.*` and the
packaged performance runners.

**Regression tests.**
`ProjectWorld.PlayableTour.PerformanceEnvelope.Uncapped` pins UE flag parsing and rejects
every capped/synthetic envelope state. `performance_envelope.Tests.ps1` pins documented
launcher syntax and strict property authentication.
`ProjectWorld.PlayableTour.PerformanceSamples.ExactProjection` pins one exact row per
collector frame. `performance_aggregate.Tests.ps1` pins three children, nearest-rank
pooling, slow-frame preservation, observed 0/10 performance-only exits, abnormal-exit
refusal, order invariance, and structural refusal.

## 32. Packaged runtime state is not immutable release payload

**Symptom.** A normal packaged product run exits cleanly but the runner reports that the
package tree changed.

**Root cause.** Orchestrator writes install-local state below
`Windows/Alis/LocalAppData`, while UE writes normal runtime state such as
`GameUserSettings.ini` below `Windows/Alis/Saved` and its runtime manifest below
`Windows/Engine/Saved`. Hashing those owned runtime projections together with the
shipped executable, pak, packaged config, and content makes every first boot look like
payload corruption.

**Fix.** Hash immutable package payload and exclude only the exact Orchestrator and UE
runtime-state roots. Continue to fail on every mutation outside them. Record the digest
scope and excluded paths in aggregate/composite evidence.

**Files.**
`scripts/ue/world/test/performance/project_world_performance_evidence.ps1` and
`scripts/ue/world/test/performance/run_kazan_playable_tour.ps1`.

**Regression test.**
`performance_aggregate.Tests.ps1` proves that runtime-state writes preserve the payload
digest while an executable-byte change moves it.

## 33. Producer fingerprints must not depend on source line endings

**Symptom.** Generated artifacts and manifest hashes are byte-identical, but a clean
public clone rejects every active World scope as produced by different source.

**Root cause.** The producer fingerprint hashed ordinary text files as raw bytes. A
Windows generation checkout used CRLF while the public Git projection used LF, so the
same C++, PowerShell, and JSON semantics acquired different producer identities.

**Fix.** Fingerprint declared output and pipeline revisions, engine build identity,
and declared data inputs. JSON data inputs normalize line endings to LF; binary
inputs hash byte-for-byte. A source edit alone does not alter producer identity.
Use the metadata-only formula migration only after every producer verify matches
its accepted baseline and the audit reports fingerprint currency as the sole drift.

**Files.**
`scripts/ue/world/generator_fingerprint.ps1` and generated World manifests.

**Regression test.**
`generator_fingerprint.Tests.ps1` proves source-edit stability and that a revision
bump or declared data-input edit moves only the owning producer fingerprint.

## 34. A terrain-only spawn trace can place the player inside valid geometry

**Symptom.** The packaged product route proves ordinary input and PreviewFlight, but
the possessed character stops after a short ascent near the center spawn. The failure
looks like an elevated terrain collision surface.

**Root cause.** Ground placement repeatedly ignored every blocking actor until a
terrain-tagged actor was found. At the Kazan production center this skipped a
`ProjectWorld.BuildingMassing.v2` static mesh and placed the capsule on terrain below
the building. An upward sweep using the possessed character's capsule proved the
building was the first blocker. The ascent distance alone did not identify its owner.

**Fix.** Ground placement uses the first blocking Pawn trace. Terrain collision remains
a separate representation-neutral probe. The movement check selects a nearby direction
whose first-blocking ground stays within the character step-height envelope, so a valid
roof placement does not immediately step into open air. Do not ignore buildings, roads,
water, or gameplay actors merely to make the placement actor be terrain.

**Files.**
`Source/ProjectWorld/Private/Presentation/ProjectWorldProductRouteGate.*`.

**Regression test.**
`Project.World.Presentation.ProductRouteGroundPlacementPolicy` pins the single
first-blocking trace and rejects the former actor-ignore loop and terrain-only mode. The
packaged Kazan product route proves grounded movement, the independent terrain
probe, road/building collision, traversal, and center -> edge -> center streaming
together.

## 35. Valid Mesh Terrain data can be invisible because of triangle winding

**Symptom.** Mesh Terrain actors, partitions, channels, bounds, collision, and material
bindings all exist, but the terrain is absent from a normal one-sided packaged render.
A temporary two-sided material makes it visible.

**Root cause.** The regular grid triangles were emitted with the opposite winding from
the renderer-facing convention. Correct vertex positions and authored +Z normals do not
make a back-facing triangle visible.

**Fix.** Emit each grid quad as `A,C,B` and `B,C,D`. Keep the production material
one-sided; two-sided rendering is only a diagnostic discriminator.

**File.**
`Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/Private/ProjectWorldMeshTerrainProducer.cpp`.

**Regression test.**
`Project.World.Realization.MeshTerrain.RendererFacingWinding` checks the rendered-face
cross products and includes the former order as a known-bad control.

## 36. Mesh Terrain reuse must include declared producer identity

**Symptom.** A Mesh Terrain adapter fix compiles and the canonical input and material
path are unchanged, but realization reports a no-op and retains stale generated mesh
packages.

**Root cause.** Base-layer reuse compared only canonical input and material
identity. A declared output revision could then advance over old geometry.

**Fix.** Include `ProjectWorld.MeshTerrain.Producer` in base actor identity and
reuse comparison. It carries the declared Mesh Terrain output revision, shared
pipeline revision, engine build identity, and data-input digests. Refresh an
identity-dirty layer without propagating a content dirty unit to dependents;
keep the layout receipt's consumed ABI independent of this provenance.

**Files.**
`Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/Private/ProjectWorldMeshTerrainProducer.cpp`
and `ProjectWorldMeshTerrainLayoutReceipt.cpp`.

**Regression test.**
`Project.World.Realization.MeshTerrain.BaseIdentity` rejects stale material or
producer identity while accepting an exact match.

---

## 37. A settled compile queue does not prove Mesh Terrain is drawn

**Symptom.** An unattended Kazan evidence capture returns an accepted receipt
whose PNGs show buildings and water but no terrain. A session that keeps the
editor running longer draws the terrain.

**Root cause.** An Editor world never loads the runtime-only compiled sections.
It draws preview sections that MeshPartition builds asynchronously after the
base actors load and register with the partition's editor component. That build
is not an asset compilation, so the three-frame compile wait of entry 16 passed
before the build finished and before any preview section existed. Registration
itself lags loading by a tick: for one frame all base modifiers are loaded and
none is registered.

**Fix.** The capture also waits for every registered evidence subject to be
drawn. ProjectWorldEditor declares `IProjectWorldEvidenceSubject` as a modular
feature; the Mesh Terrain adapter reports the loaded base modifiers aimed at
its partition, those whose preview section is visible in the editor, and active
preview builds, and counts a partition without any loaded base modifier as one
undrawn unit. A frame where a present subject expects no unit, misses a drawn
unit, or has a pending build resets the ready count; a subject never drawn
rejects the receipt at the existing timeout with its counts in `message`.

**Files.** `ProjectWorldEditorModule.cpp`, `ProjectWorldEvidenceReadiness.h`,
`Public/ProjectWorldEvidenceSubject.h`, and the
`ProjectWorldMeshTerrainEvidence` module of ProjectWorldMeshTerrain.

**Regression test.** `Project.World.Evidence.SubjectDecision` pins the
count-to-decision rule, `Project.World.Evidence.ReadinessContract` pins the
reset on an undrawn frame, and `Project.World.MeshTerrain.EvidenceSubjectRegistered`
pins the adapter registration. The rendered gate is the
`capture_visual_evidence.ps1` route itself.

---

## 38. Output changes require a producer revision and verify baseline

**Symptom.** A source edit changes saved generated geometry while the manifest
producer fingerprint remains current.

**Root cause.** Source files are intentionally outside the fingerprint. A
changed output without an output-revision bump has no declared identity change.

**Fix.** Bump the owning descriptor's output revision, re-record its verify
baseline, and run the output sabotage. Shared realization changes bump the
pipeline revision and re-record every affected baseline.

**Files.** The owning `Data/Producers/` descriptor and
`Data/TestFixtures/Verify/` baseline.

**Regression test.** The producer's `Project.World.Realization.Verify.*` test
compares saved output to its recorded projection; BaselineRules rejects an
unchanged verification identity with changed projection.

---

## 39. Mesh Partition builder must ignore stale Asset Registry cache in an isolated Matrix

**Symptom.** A canonical-authority Kazan Matrix realizes a fresh World
Partition map, but the Mesh Partition post-apply builder tries to delete old
compiled-section descriptors whose external actor packages are in the Matrix
backup. UE 5.8 crashes on a checked cast of the missing actor.

**Root cause.** The first Apply creates a fresh map with zero loaded actor
references. The separate builder process reads the project Asset Registry
cache, which can still list compiled sections from the pre-Matrix map. Its
cleanup path assumes every listed compiled-section package can load.

**Fix.** The Mesh Terrain producer passes UE 5.8's
`-NoAssetRegistryCache` to `WorldPartitionMeshPartitionBuilder`. The builder
then discovers current packages from disk. The Matrix still restores the
exact pre-run generated tree after every outcome.

**Files.** `ProjectWorldMeshTerrainLayerProducer.cpp` and
`tools/World/EndToEndValidation/app/execution.py` (the canonical-only route
creates its per-profile receipt directory explicitly).

**Regression test.** The `kazan_territory_v1` canonical-authority Matrix runs
the post-apply builder on an isolated production map and requires its accepted
compiled-terrain inventory plus exact generated-tree restoration. The focused
`TerritoryMatrixContractTests.test_canonical_matrix_reaches_authority_without_source_or_compile`
also checks canonical-mode work-directory creation.

---

## 40. Fingerprint migration candidates need the staging manifest schema ID

**Symptom.** S5 O4 refuses the first metadata-only fingerprint migration with
`Manifest schema identity is invalid for its authority root` before publishing
any candidate. The outer recovery restores both authorities.

**Root cause.** An active manifest stores a schema reference relative to its
authority root. Prospective candidate validation expects the staging schema
ID; the migration helper copied the active reference unchanged.

**Fix.** `New-ProjectWorldFingerprintMigrationCandidate` sets the staging
schema ID after copying the prior document. `Publish-ProjectWorldActiveSet`
still writes the root-relative schema reference to the immutable manifest.

**Regression test.** `generated_manifest.Tests.ps1` publishes a candidate
cloned from an enrolled active manifest. The original S5 O4 preflight checked
12 nonterrain candidates; after finding old building actors, the migration
limits metadata-only candidates to 10 scopes.

---

## 41. A current building manifest can coexist with old actor tags

**Symptom.** A production Apply reports no dirty building units but tries to
replace a named building actor and fails to spawn it. The actor has
`ProjectWorld.BuildingMassing.v1` while the active manifest and realization
profile declare v2. Its original creation history is unverified.

**Root cause.** The manifest audit authenticates package bytes and metadata;
it does not inspect the actor's live producer tag. The building pass detects
the missing v2 output. After retiring the v1 actor, Unreal keeps its UObject
name reserved until collection, and a spawn requiring that exact name fails.

**Fix.** Treat the affected building scopes as identity-dirty and rebuild them
during the one-lock S5 migration. The building producer ignores a retired
named actor and requests a free actor name; live actors with conflicting tags
still fail. The current v2 tag and cell identity own subsequent discovery.

**Regression test.** `Project.World.Realization.Buildings.SavedStaleRecovery`
persists a stale actor, detects it through the ownership lifecycle, replaces
it, reloads, and proves the following Apply writes no actors or meshes.
`Project.World.Realization.Verify.BuildingRetiredIdentityProjection` keeps
retired actors visible to saved-output comparison. The 61 historical Kazan
actor and mesh payload pairs were compared with the repaired saved output:
all 342 building records matched in placement, geometry, material, collision,
and other meaningful fields after excluding version tags and allocation IDs.

---

## 42. Realization after isolated reconstruction must ignore stale Asset Registry cache

**Symptom.** A Matrix first Apply reconstructs a production map, but the next
unchanged Apply sees missing external actor package warnings and attempts to
rewrite a saved building mesh. The save fails even though the first leg and
saved-output projections accepted.

**Root cause.** Matrix moves generated packages aside before the first Apply.
A later Unreal process can read Asset Registry entries from the pre-isolation
tree. Disabling the cache only for the read-only projection processes leaves
the realization process exposed.

**Fix.** `realize_canonical_world.ps1` starts the realization commandlet with
`-NoAssetRegistryCache`, so every leg discovers the current on-disk packages.

**Regression test.** The canonical-authority Kazan Matrix starts with the map
and layer packages isolated, runs a full first Apply, both saved projections,
and an unchanged second Apply with zero actor and mesh writes. Its outer
transaction restores the original generated tree byte for byte.

---

## 43. Numbered navigation actor names can crash Unreal CSV capture

**Symptom.** The packaged Kazan playable-tour product route accepts, then its
Development performance child exits 3 shortly after CSV capture starts. The
CSV processing thread asserts at `CsvProfiler.cpp:1623` because an `FName`
stat ID exceeds its 51-bit mask.

**Root cause.** In the installed UE 5.8.3 source, `NavigationSystem.cpp`
records `NavTasks`, `NavTasksDelays`, and `NavInvokers` CSV stats using the
navigation-data actor name. A generated Recast actor name can carry a numeric
suffix. UE converts the dynamic stat string to `FName`, whose number occupies
the high bits rejected by the CSV stat register. The failing stat was
`NumRemainingTasks_RecastNavMesh_UAID_7085C2D2E177660503` with a nonzero
number field; the live debugger identified the `NavTasks` category.

**Fix.** `Config/DefaultEngine.ini` disables these three engine navigation
diagnostic CSV categories for the project. The playable-tour gate still records
the rich CSV and uses its native per-frame consumer for Frame, Game, Render,
GPU, memory, and streaming acceptance. The product route separately verifies
navigation and terrain collision. Do not disable the CSV capture or relax the
performance budget to avoid this engine assertion.

**File.** `Config/DefaultEngine.ini` (`[CsvProfiler]`). The producer is in the
installed engine's `NavigationSystem.cpp`, not in a World generator.

**Regression test.** The matching pre-fix packaged Development child exited 3
twice. With the project configuration, the same packaged Kazan product and
performance route exits 0, both receipts accept, and the CSV and exact raw
sample files are nonempty. The full three-child playable-tour Candidate gate
remains the release prerequisite.

---

## 44. An empty vegetation actor must not prove runtime instancing

**Symptom.** A vegetation-tagged generated actor with no static mesh component
could pass the territory instancing check when other runtime gates passed.

**Root cause.** The scan initialized the instancing result to true and only
changed it while visiting mesh components. An empty component list left true.

**Fix.** Require at least one mesh component on every vegetation actor and
require each component to be a nonempty HISM. A selected vegetation layer with
no vegetation actors also fails; a deliberately vegetation-free projection may
leave the instancing probe false.

**File.** `Source/ProjectWorldEditor/Private/ProjectWorldTerritoryRuntimeAcceptance.cpp`.

**Regression test.** `Project.World.Realization.Runtime.TerritoryInstancingPolicy`
covers the selected/present/proof decision table and the empty-actor case.

---

## 45. Upward flight can remain trapped below a building overhang

**Symptom.** Real forward/upward input remains active with zero velocity during
an obstacle-clearance leg. Repeating the whole release does not clear the trap.

**Root cause.** The capsule meets both a wall and an underside. Forward/upward
input has no free direction; the previous driver only increased ascent input.
This was observed in uncooked PIE before final return, independently of the
packaged final-arrival slide failure in entry 29.

**Fix.** The playable driver's existing obstacle phase sweeps the actual scaled
capsule upward using its collision channel and responses. A blocking underside
selects real backward/upward input until clear above the hit plane. If the
backward capsule sweep is blocked, it selects an open lateral direction and
keeps that direction while it remains open. A Manhattan control showed that
backward-only retreat can move about 30 m and then stop against another wall.
If backward and both sides are blocked, an open forward capsule sweep is the
last escape option. Initial traversal W is released on entering recovery;
only a direction selected by recovery is retained. The earlier "enclosed"
fixture had no front wall: its open forward exit was never considered.
The revised fixture measures that opening and adds a fourth wall to prove
bounded refusal for actual enclosure. This fixture defect is confirmed;
the intermittent real-map timeout's exact exit geometry was not retained.
An enclosed capsule still refuses instead of changing collision. The retreat
has a ten-second bound and retains the existing ascent, leg and tour bounds.
Clearance, arrival and terminal cleanup release all escape keys. No physics or
generated collision is changed.

**File.** `Source/ProjectWorld/Private/Presentation/ProjectWorldPlayableTourDriver.cpp`.

**Regression test.** Exact `Project.World.PlayableTour.OverhangRecovery` uses a
real physics-world underside, blocked rear wall and open side. It exercises
backward-first selection, lateral and forward fallback/retention, clearance,
measured four-wall bounded refusal and key cleanup. The real-map proof belongs to
[uncooked gameplay admission](../../../../scripts/ue/world/README.md#uncooked-gameplay-admission).

---

## 46. PIE match readiness is too late to configure the runtime viewport

**Symptom.** An uncooked gameplay probe refuses 1920x1080 despite requesting
2560x1440; missing traversal fields can obscure this native startup failure.

**Root cause.** Native PIE startup waits for match readiness. Product graphics
application can resize the real viewport before that wait completes.

**Fix.** The Editor test maintains its transient render-target dimensions during
startup and travel, on actual mismatch only. Native acceptance limits and saved
preferences remain intact. The outer contract reports the first native refusal
before checking downstream traversal fields. Operational ownership is in
[uncooked gameplay admission](../../../../scripts/ue/world/README.md#uncooked-gameplay-admission).

**Files.** `Source/ProjectWorldEditor/Private/Tests/ProjectWorldUncookedSessionTest.cpp`
and `scripts/ue/world/test/integration/uncooked_playable_tour_contract.ps1`.

**Regression tests.** The uncooked receipt fixture requires the native startup
error code. The actual two-city probe proves render-target correction precedes
traversal and authenticates both real gameplay receipts.

---

## 47. Building collision and debug shading mask reversed exterior winding

**Symptom.** A debug building is visible, but an opaque one-sided replacement
shows dark interior surfaces or underlying terrain instead of its exterior walls
and roof. Collision still blocks movement from both directions.

**Root cause.** Conventional outward cross products are opposite UE's clockwise
static-mesh front-face convention. Native normal recomputation follows emitted
triangle winding. Double-sided query collision and debug shading do not prove
exterior visibility. Geometry Scripting's default build-settings application can
also recompute copied source normals; distinguish raw source from RenderData.

**Fix.** The building triangle emitter reverses its authored order before vertex
creation and derives outward normals from the native reverse cross. Ordered
vertex emission changes mesh semantic identity. Building-owned input output
revision 2 changes dirty-unit identity before the realization's early skip;
reversing polygon instance order alone is insufficient. Canonical geometry,
positions/bounds, actor tag domains and unrelated layers retain their contracts.
Do not compensate with inverted material normals or a Two Sided material.

**Files.** `Source/ProjectWorldBuildingEditor/Private/ProjectWorldBuildingMeshBuilder.cpp`,
`Source/ProjectWorldBuildingEditor/Private/ProjectWorldBuildingRealization.cpp`,
`Data/Producers/project_building_massing.json`, and its normalized saved-output
baseline under `Data/TestFixtures/Verify/`. The cell identity and declared producer
revision must both reflect changed emission; changing only the cell digest leaves
incorrect producer attribution.

**Regression tests.** `Project.World.Realization.Buildings.ExteriorFacing`
requires all twelve cube triangles and native/stored normal agreement.
`Project.World.Realization.Buildings.ExteriorRender` checks one-sided native
wall/roof depth, shader normals and a rejected away-facing camera in a transient
fixture. Existing production outputs require their separately authorized affected
building update; a fixture PASS does not mutate or certify those outputs.

## 48. Water updates require a broad save without owned actor persistence

**Symptom.** Selected Road/Water/Vegetation execution writes Water meshes but
refuses at the shared World save guard, despite unchanged canonical inputs.

**Root cause.** Water updated external actors without saving their packages or
reporting self-saved mutations. Broad orchestration had previously supplied the
missing persistence step; a clean selected no-op did not exercise this defect.

**Fix.** The Water owner saves each changed external actor through Unreal's native
package writer before reporting success, using the established Road/Building
pattern. Failed saves refuse; the shared-map guard stays intact. Unchanged
semantics still write nothing. This does not change canonical water functions.

**File.** `Source/ProjectWorldWaterEditor/Private/ProjectWorldWaterRealization.cpp`.
**Regression.** `Project.World.Water.PreparedPresentation` requires owned actor
persistence. Selected changed-output acceptance must include actual mesh/actor
saves, child rejection with byte-exact rollback, cold replay and zero-write no-op;
an unchanged selected fixture alone cannot prove that persistence boundary.

## 49. Water preparation context changes output without dirtying its cell

**Symptom.** Changing another cell's reference to a shared Water feature changes
triangles in an existing cell, while incremental planning keeps that cell clean.

**Root cause.** Native CDT preparation clips each feature to the combined bounds
of its consuming cells before triangulation. Hashing only the local cell and
feature omits a consumed input; identical feature coordinates do not imply an
identical prepared triangulation.

**Fix.** Water realization and input hashing share the feature preparation-domain
calculation. Each valid consumed feature contributes those bounds to the unit
identity. Unconsumed features and other-cell membership changes preserving the
domain do not invalidate the unit; its own consumed membership remains an input.
The persisted unit-input identity is an output contract. Its correction advances
Water's declared output revision and the affected Water/Pipeline verify baselines,
even when saved geometry projections remain equal. It requires Water-only
revalidation at the appropriate acceptance boundary; it does not change canonical
authority or justify regenerating unrelated layers.

**Files.** `Source/ProjectWorldWaterEditor/Private/ProjectWorldWaterSourceContext.h`,
`ProjectWorldWaterLayerProducer.cpp` and `ProjectWorldWaterRealization.cpp`.

**Regression tests.** `Project.World.Water.PreparationDomainIdentity` witnesses a
native geometry change and its input-identity change, with unchanged-context
controls. `Project.World.Realization.Layers.WaterPreparationDomain` exercises the
actual planner under unchanged shared contracts/fingerprints, selecting the
affected Water cell and leaving Terrain clean. `Project.World.Water.SourceEmissionReplay`
authenticates native replay against saved source descriptions without Apply.

## 50. A physical Water material invalidates a debug-color visibility oracle

**Symptom.** A valid dark or green river fails a temporal gate that recognizes only
blue BaseColor, or two empty captures appear stable and are mistaken for Water proof.

**Root cause.** Single Layer Water appearance comes from physical scattering and
reflection; the accepted material's BaseColor is black. Albedo is not a geometry
label. Clear depth can also match between captures, while synchronous fixture
captures do not establish that newly created scene state has rendered across frames.

**Fix.** Use paired full-scene and identified-Water native HDR/depth capture views,
float alpha readback and a shared projection. Bound candidates by Water geometry
and keep clear depth outside those bounds. Encode numeric visible-Water masks,
reject membership changes and retain actual materials. Keep normal FinalColor
inspection separate. Exact coplanar duplicates still need geometry/renderer proof.

**Files.** `Source/ProjectWorld/Private/Presentation/ProjectWorldWaterVisibilityCapture.cpp`,
`ProjectWorldRuntimeScreenshotCapture.cpp`, `ProjectWorldShippingWaterProofGate.cpp`,
and `scripts/ue/world/test/water_temporal_stability.ps1`.

**Regression.** `Project.World.Presentation.WaterVisibilityCapture` uses native
latent frame progression and real RiverCalm. It checks stable coverage, empty
background, an opaque occluder, a 0.05 cm competitor, membership refusal/recovery,
unchanged binding and settled same-tick capture startup. The verifier's Pester
tests reject colored beauty pixels, visibility flips and a stable empty mask.

## 51. Whole-feature classification misses emitted horizontal Water overlaps

**Symptom.** Two ribbons produce coincident horizontal faces in a cell although
one source has a slope elsewhere, so the constant-feature union leaves them both.

**Root cause.** A globally nonconstant source can emit a local plateau. Compatibility
must be evaluated on final geometry as well as on whole-feature input classification.

**Fix.** The Water owner groups final horizontal triangles by exact float height,
unions their XY coverage and checks native constrained-triangulation success.
Measure emitted float positions with double subtraction/cross products; float
products can erase the area of a valid long thin face. Check native tessellation
coverage separately, then bound emission-area drift by the actual coordinate
rounding errors. Reject collapsed or reversed faces before replacement. Preserve holes, true slopes,
unequal heights and every input source identity. Failure leaves the original
description unchanged; disjoint geometry is a byte-preserving no-op. Advance the
owning output revision and verifier identities before affected-output acceptance.
Do not snap unequal planes or reinterpret mixed plateau-transition contacts as
proven source connectivity. Pre-composition feature triangle ranges are historical
attribution evidence, not final composed triangle ranges.

**Files.** `Source/ProjectWorldWaterEditor/Private/ProjectWorldWaterHorizontalComposition.cpp`
and `ProjectWorldWaterCellMesh.cpp`.

**Regression.** `Project.World.Water.LocalPlateauComposition` reproduces the
feature-wide classification defect. `Project.World.Water.HorizontalPieces` checks
holes, shared borders, retained slopes, float coverage, no-op identity and invalid
input refusal. `Project.World.Water.HorizontalFloatPrecision` rejects the
known-bad float-area calculation on a valid thin face.
`Project.World.Water.CompositionCandidateReplay` authenticates selected city inputs,
requires a nonempty candidate and measures qualified exact-horizontal overlaps
without changing saved geometry. `Project.World.Water.CityCandidateCensus` builds
all city Water candidates in memory and compares saved triangle positions/order;
it verifies package-byte/dirty-state preservation and reports affected scope,
not persistence or final rendered acceptance.
`Project.World.Water.CoplanarRibbonComposition` retains the original
constant-feature composition controls. Saved production outputs need their own
transactional revalidation; fixture success does not promote them.

## 52. Grid-corner checks cannot certify a refined terrain interior

**Symptom.** Canonical grid samples match while a narrow Water footprint remains
above the actual triangulated ground, or a final-surface probe reports no faces
although compiled rendering and collision hit them.

**Root cause.** Sparse replacement can change a quad's interior without moving
its original corners. Placement must query the replacement triangles, and
compiled acceptance must measure full coverage and affine interior heights.
For a projected spatial probe, native 3D box intersection also rejects boxes
with zero Z thickness; a flat XY tree needs a 2D overlap predicate.

**Fix.** Load the canonical final representation once, refuse missing replacement
coverage, retain bilinear placement outside selected quads, and include final
vertices in navigation bounds and fixture identity. Check complete source
topology/weights and compiled triangle intersections. Use native XY box
intersection for broad-phase pruning. Verify actual Nanite/fallback pixels and
collision separately; CPU fallback data alone does not prove Nanite rendering.

**Files.** `Source/ProjectWorldEditor/Private/ProjectWorldCanonicalRefinement.cpp`,
`ProjectWorldGeneratedGeometry.cpp`, `ProjectWorldRuntimeNavigation.cpp`, and
`ProjectWorldMeshTerrain`'s `ProjectWorldMeshTerrainSurfaceFidelity.cpp`.

**Regression.** `Project.World.Terrain.RefinedQueries` distinguishes nonplanar
triangle interpolation from bilinear interpolation and rejects authenticated
coverage, weight, duplicate-vertex and cracked-edge controls.
`Project.World.Realization.MeshTerrain.RefinedSurface` rejects stale/missing/
duplicate faces, checks actual compiled buffers, renders both proxy types and
measures collision. `Project.World.Realization.Runtime.RefinedNavigationBounds`
rejects bounds derived from the original grid alone.

## 53. Coarse Water triangles slope across a quantized source plateau

**Symptom.** Two canonical Water point evaluators agree locally, but a coarse
triangle interpolates from a nearby plateau to a legitimate higher corner and
overlaps another source at a slightly different rendered height.

**Root cause.** Footprint-only triangulation samples the source function at its
vertices. The emitted affine interior is a representation of those samples,
not the discontinuous quantized nearest-segment function. A near-depth detector
can also miss positive-area overlaps whose complete height range is above its
threshold. Source coordinate equality does not establish source connectivity.

**Fix.** Certify the entire convex overlap against every possibly nearest source
segment. Refine only compatible source-level approximations, evaluate new
vertices through the existing source evaluator, and propagate the same edge
vertices to incident source faces before exact horizontal composition. Preserve
high corners, coverage and holes. Do not flatten a whole sloping triangle, cut
only its overlap, or independently round shared edge points. Refuse unresolved
refinement budgets, float precision and unproven cell seams before replacement.
Unknown or unequal local functions remain separate; this does not select a
crossing law or terrain-conditioning policy.

**Files.** `Source/ProjectWorldWaterEditor/Private/ProjectWorldWaterSourceApproximation.cpp`,
`ProjectWorldWaterMeshBuilder.cpp` and `ProjectWorldWaterCellMesh.cpp`.

**Regression.** `Project.World.Water.SourceApproximation` exercises a translated
source-shaped transition, its incident faces, all positive-area overlaps,
retained high corner, projected coverage, separate hole, deterministic replay
and unequal-level control. `Project.World.Water.SurfaceRegionCertificate` rejects
threshold-crossing and nearest-segment competition controls.
`Project.World.Water.SourceApproximationRefusal` checks explicit seam, membership
and bounded-resolution refusals with exact mesh-byte preservation.

## 54. Component geometry hits are not world-channel blocking results

**Symptom.** SavedProjection or a named packaged building ray rejects a real
wall/roof intersection although the component has working physics and blocks
Pawn. The native component query returns true with `bBlockingHit == false`.

**Root cause.** UE 5.8 `LineTraceComponent` delegates through BodyInstance to
`LineTrace_Geom`, whose conversion uses `BuildOverlapAll`. The hit's blocking
flag describes that query filter, not the component's Pawn collision response.

**Fix.** Require the successful native component geometry query, the matching
queried component and a finite impact. Keep physics availability, Pawn-block
configuration, body identity and transform checks separate. Compare the named
packaged hit with its current source physics reference within 0.05 cm; report
source-plane error separately. This does not establish globally identical
physics triangles or replace gameplay collision checks.

**Files.** `Source/ProjectWorldEditor/Private/Verify/ProjectWorldBuildingSurfaceReference.cpp`
and `Source/ProjectWorld/Private/Presentation/ProjectWorldProductBuildingEvidence.cpp`.

**Regression.** `Project.World.SavedProjection.BuildingComponentPhysicsReference`
and `Project.World.Presentation.ProductBuildingComponentPhysics` use native
static-mesh bodies in isolated physics worlds, observe the nonblocking geometry
hit, and reject Pawn-ignore and disabled-collision controls.

## 55. Material evidence must inspect components independently of actor subclass

**Symptom.** A real city passes movement and layer collision, but material binding
evidence reports no terrain consumer despite generated textures being ready.

**Root cause.** The readback enumerated only `AStaticMeshActor`. Epic's native
Mesh Partition compiled section derives from `AActor` and owns static mesh
components, so the readback skipped the actual terrain renderer.

**Fix.** Select runtime semantic roles on actors, then enumerate their registered,
visible main-pass static mesh components through Engine APIs. Preserve resource
readiness, sharing and role requirements. No Mesh Partition implementation
dependency or material/geometry regeneration belongs in this probe.

**Files.** `Source/ProjectWorld/Private/Presentation/ProjectWorldProductMaterialEvidence.cpp`.

**Regression.** `Project.World.Presentation.ProductMaterialComponents` observes
the generic-actor terrain case fail before the correction, and rejects hidden
components and uninitialized texture targets. Parameter readback establishes
bindings, not rendered pixel correctness or packaged performance.

## 56. Building geometry rays cannot substitute for the player's collision response

**Symptom.** Named wall/roof rays match saved physics, but their receipt cannot
establish measured normals or the player's capsule response at those surfaces.

**Root cause.** Component ray queries do not use the possessed capsule's shape,
object channel or response container. Copying a source normal is not a measured
impact normal. An observer that retains its last accepted result after refusal
can also publish stale evidence.

**Fix.** Keep source-reference rays and live-capsule native component sweeps as
separate observations in the building-evidence owner. Record measured normals,
named contact identity, penetration and other-blocker outcomes. Clear accepted
evidence before each probe and require both observations in receipt readers.
Retain source-plane bias as a measurement; capsule contacts need not equal ray
points and do not introduce a new terrain or collision-fidelity policy.

**Files.** `Source/ProjectWorld/Private/Presentation/ProjectWorldProductBuildingEvidence.cpp`,
`scripts/ue/world/test/performance/project_world_building_evidence.ps1`, and
`scripts/ue/package/release_shipping.py`.

**Regression.** `Project.World.Presentation.ProductBuildingComponentPhysics`
distinguishes native normals from an oblique source direction and refuses when
the pawn ignores WorldStatic although building rays still intersect. Existing
PowerShell and Python packaged-evidence fixtures reject absent/invalid capsule
observations. The PowerShell reader converts native double-precision tolerances
to Double before comparison; its regression includes the round-trip JSON value.

## 57. A cache bundle's requested output need not be its primary texture

**Symptom.** Packaged material resources are ready and the native cold/warm log
shows reuse, but the host reader reports missing terrain or aggregate cache proof.

**Root cause.** The native request authenticates and publishes the entire output
bundle, then logs the requested slot. Material load order can request Masks or
Normal first. Requiring that log target to equal the primary Structure or Signals
texture incorrectly makes acceptance depend on request order.

**Fix.** Correlate the accepted semantic identity with a live bound output in the
exact authenticated primary package. Preserve the primary binding, material-chain,
readiness, warm-generation and global-budget checks. Do not change runtime request
order, add another generation request, or admit unknown same-package siblings.

**Files.** `scripts/ue/package/release_shipping.py`.

**Regression.** `test_cache_accepts_requested_bound_secondary_bundle_outputs`
fails before the correction. Adjacent controls reject an unbound same-package
target, a bound foreign package, a wrong identity and an unready secondary output.

## 58. Empty native objects must remain serializable under StrictMode

**Symptom.** A valid native input census fails while producing its deterministic
shared-input digest, with a missing `Name` property error.

**Root cause.** The sorted JSON projection accessed `.Properties.Name` on an empty
PSCustomObject. StrictMode treats that empty member expansion as a missing property.

**Fix.** Enumerate the properties before selecting their names. Preserve empty
objects and arrays as values; do not convert a failed read into an empty value.

**Files.** `scripts/ue/world/material_reference_operation.ps1`.

**Regression.** `native_refinement_transition.Tests.ps1` exercises an unchanged
empty native object through the actual digest builder. The material-reference
and manifest lifecycle tests cover the shared helper's direct consumers.

## 59. Paired authority recovery must authenticate everything it restores

**Symptom.** A canonical selection can be reversed before missing backups are
detected, or a readable corrupt backup can be restored and recovery discarded.
An interrupted final World rename also leaves staged bytes that an ordinary cold
reader correctly refuses.

**Root cause.** Content-only recovery checks are insufficient when the journal
also coordinates canonical selection. Its snapshot can cover carried owners as
well as selected writers; successful child status does not authenticate final
package bytes.

**Fix.** Authenticate prior accepted artifacts across the actual snapshot coverage
before reversing canonical selection or restoring packages. Verify restored bytes
before cleanup. Use the existing full committed-artifact validator on successful
paired completion. Recovery may read only staging pinned by its expected World
hash; unknown states retain the journal and snapshot.

**Files.** `scripts/ue/world/canonical_selection_transition.ps1`,
`generated_content_transaction.ps1`, `generated_manifest.ps1`, and
`realization_authority_publication.ps1` in the same script owner.

**Regression.** `canonical_selection_transition.Tests.ps1` covers missing/corrupt
selected and carried backups, bad restoration, damaged committed output, unknown
staging and failure after canonical selection before the World commit.

## 60. Whole-quad refinement can move a dry civil anchor

**Symptom.** A Road follows new Water conditioning, a retained UV profile refuses
its support, or a Building moves despite unchanged dry visible Terrain.

**Root cause.** A final wrapper and its immutable grade support have different
identities. Selected whole quads also change placement queries from regular-grid
bilinear sampling to affine triangles. Wet-footprint overlap alone misses dry
anchors affected by that query change.

**Fix.** The canonical refinement child declares explicit original-grid civil
grade references. Road retains its native triangle/UV grade and checks final
Terrain separately. Building retains its existing owner-grid bilinear anchor;
its identity consumes that reference rather than unrelated final-wrapper bytes.
Unreferenced queries remain final-surface queries. Preservation supplies no
unobserved bridge classification, foundation design or geographic quality waiver.

**Files.** CanonicalCompilation refinement's `civil_grade.py` and `publication.py`;
ProjectWorld's `ProjectWorldCivilGrade.cpp`, Road surface/elevation realization,
and Building mesh builder/input identity.

**Regression.** `test_civil_grade.py` covers whole-quad/neighbor ownership,
refusal, no-op and removal. Native `CivilGradeAdmission`, Road and Building
`ImmutableCivilGrade` cover malformed support, retained actual mesh vertices,
default-query controls, final penetration and consumed-input locality.

## 61. Outside-owner placement must select the clamped final surface

**Symptom.** A Building whose full-footprint bounds center lies beyond its owner
cell samples an old grid height despite a refined final surface at the owner edge.
Missing final edge coverage can also silently return an old height.

**Root cause.** Refined-quad dispatch uses raw XY while only the bilinear fallback
clamps. The same logical placement query therefore selects different authority
depending on whether the input is already within the owner's sample domain.

**Fix.** Clamp once before representation selection and evaluation. A selected
final surface refuses missing coverage. Building's input identity includes the
changed query contract for default anchors with a refined owner, including
cross-cell fragments. Explicit immutable-grade anchors and unrefined inputs
retain their identities; unrelated layer revisions do not advance.

**Files.** `ProjectWorldGeneratedGeometry.cpp`, Building's
`ProjectWorldBuildingRealization.cpp` and their owner-local tests.

**Regression.** `Project.World.Terrain.RefinedQueries` checks both owner edges,
missing coverage, unrefined and explicit base queries.
`Project.World.Realization.Building.RefinedOwnerClampedAnchor` checks actual
Building mesh elevation, cross-cell owner identity and retained hashes for
unrefined/immutable-grade inputs.

## 62. Constructed cover outlines must preserve exact mesh connectivity

**Symptom.** A connected rotated surface refuses component correspondence after
float conversion, even though every source and stored shared edge is intact.

**Root cause.** Native polygon union normalizes coordinates to an integer frame.
Its reconstructed polygons can split an exact triangle cover at different internal
edges before and after float storage. Unequal float axis spacings can also leave
a rotated source corner without a supported point in the adjacent 3x3 grid.

**Fix.** Extract source and stored outlines through native mesh boundary loops
using exact shared nodes and existing collinear subdivisions. Temporary topology
fans never become emitted geometry. Retain the independent coverage, holes,
component, height and slope gates. If adjacent conversion fails, bounded extra
candidates use one coarsest storage interval; check their actual rounded
displacement and source membership before accepting a node.

**Files.** `ProjectWorldConstructedOutline`, `ProjectWorldConstructedBoundary`,
the constructed mesh builder and its coverage validator.

**Regression.** `Project.World.Realization.Constructed.Outline` retains the
captured projected cover and checks exact T junctions, holes, nested islands,
separate/point-touching components and invalid covers. `Constructed.Boundary`
checks the rotated corner, rounded bound escapes, unchanged adjacent results
and unknown-source refusal. `Constructed.Coverage` retains missing-face,
filled-hole, duplicate and false component-correspondence controls.

## 63. Constructed support changes must invalidate verification fixtures

**Symptom.** Native Road input identity changes after a valid support or model
change, but a verification fixture keeps its previous digest.

**Root cause.** Bundle serialization omitted typed constructed endpoint/domain
fields, referenced model bindings and the constructed surface itself. Geometry
alone could not distinguish changed support or observed versus modeled basis.

**Fix.** Serialize the complete typed constructed inputs and sort reference keys.
Keep absent default fields out of the existing ordinary serialization. This
changes evidence identity, not production admission or geometry.

**Files.** `Verify/ProjectWorldVerifyIdentity.cpp` and
`ProjectWorldReferencedSurfaceTest.cpp`.

**Regression.** `Project.World.Realization.Verify.ConstructedEndpointIdentity`
reuses the native reference fixture. Valid relation, support-model and basis
mutations invalidate verification while the recorded ordinary digest stays fixed.

## 64. Default navigation creation can ignore supported-agent clearance

**Symptom.** The possessed standing character is taller than its selected
navigation data even though native agent lookup succeeds. A configuration-only
correction then produces an agent-registration refusal.

**Root cause.** Native single-agent lookup returns its main data without a size
match. The default creation overload spawns raw Recast data with fallback agent
dimensions; it does not apply the live supported-agent configuration. Comparing
only the navigation class and tile size also hides changed clearance.

**Fix.** Declare the project supported agent and create missing data through the
native configured factory. Require registered, correctly owned data matching the
live default agent. Refuse unexpected or mismatched preexisting actors; roll back
only an owned failed creation. Map verification includes actual agent identity,
dimensions and runtime mode. Advance map output and comparison revisions before
ordinary saved-map reconciliation; a configuration edit is not map acceptance.

**Files.** `ProjectWorldRuntimeNavigation.cpp`,
`Verify/ProjectWorldMapVerifyProjection.cpp` and project navigation configuration.

**Regression.** `Project.World.Realization.Runtime.NavigationAgentConfiguration`
checks loaded native configuration and taller-capsule refusal/restoration.
`Runtime.NavigationDataOwnership` covers reuse and stale/unexpected actor refusal.
`Runtime.NavigationMapProjection` changes actual native agent fields and runtime
mode, requiring a changed digest and exact restoration.

## 65. Deprecated navigation cell size does not configure current resolution tiers

**Symptom.** A project requests small navigation cells while newly-created native
data still uses the engine default resolution.

**Root cause.** Current Recast generation reads `NavMeshResolutionParams`.
The old scalar cell size migrates only when loading older serialized navigation
versions; it does not configure current newly-created data.

**Fix.** Configure the native fixed-array resolution tiers and remove the
obsolete scalar. Preserve High <= Default <= Low cell widths. Read effective
values from actual native data and include those values in map verification.
Changing voxel resolution does not itself prove geographic or navigation quality.

**Files.** Project navigation configuration, map verification projection and
`ProjectWorldRuntimeNavigationTests.cpp`.

**Regression.** `Project.World.Realization.Runtime.NavigationAgentConfiguration`
checks the effective default resolution and tier order on registered native data.
`Runtime.NavigationMapProjection` changes all native tier dimensions and contour
settings, requiring changed digests and exact restoration.

## 66. Existing navigation settings survive a map producer revision

**Symptom.** Product Apply refuses an unregistered stale map-owned agent, or
accepts an existing native actor whose vertical resolution remains stale.

**Root cause.** Strict data resolution validates or creates the native actor;
it does not reconcile existing settings. Advancing a producer revision or
observing the settings in verification does not itself update them. Native
registration also caches agent keys, so changing a registered actor's agent
properties is not equivalent to loading an unregistered stale actor.

**Fix.** Full map Apply uses explicit owned navigation reconciliation before
domain configuration. Validate ownership and accepted settings before mutation,
unregister before changing agent keys, require actual native registration and
rematched tiers, then invalidate the generated field. Preserve strict generic
resolution and refuse conflicting or inconsistent ownership. Native LoadMap can
retire failed-registration data, so verify serialized transitions separately.

**Files.** `ProjectWorldRuntimeNavigation.cpp`,
`ProjectWorldRuntimeRealization.cpp` and `ProjectWorldRuntimeProfileTests.cpp`.

**Regression.** `Project.World.Realization.Runtime.ProductNavigationDomain`
covers stale unregistered agent and vertical-resolution repair, lawful larger
tiles, clean no-op behavior, actual support removal/restoration and conflicting
native actor refusal.

## 67. JSON conversion and output hashes do not establish a typed source handoff

**Symptom.** Invalid source evidence passes file authentication, a structural
field disappears on load, or annotation changes unnecessarily dirty Road meshes.

**Root cause.** Native JSON accessors can coerce numeric strings and fractions;
the reader also accepts nonfinite numbers. File hashes authenticate bytes without
proving their declared role, context or policy association. Admission metadata
and geometric dependencies answer different questions.

**Fix.** Parse the closed structural contract into the existing typed field.
Verify the complete output inventory before matching retained inputs to exact
request and feature associations. Require literal finite numeric types and
integral byte counts. Keep admission metadata in verification identity while
geometric consumption uses the semantic model and actual XYZ. Source primitive
qualification remains upstream.

**Files.** `ProjectWorldCanonicalBundle.cpp`,
`ProjectWorldConstructedContractParsing.cpp`,
`ProjectWorldCanonicalRetainedInputs.cpp`, `ProjectWorldCanonicalFiniteJson.h`
and `Verify/ProjectWorldVerifyIdentity.cpp`.

**Regression.** `Project.World.Realization.Constructed.CanonicalJsonAdmission`
covers captured source-shaped parsing, role/type refusal and metadata-only
geometry locality. `CanonicalImportBundle` consumes a freshly generated Python
child and checks declared evidence, context/policy binding and cold repeated
admission. `CanonicalImportIdentity` changes each added metadata input.

## 68. Reloaded structural meshes can retain file handles during retirement

**Symptom.** Selective removal succeeds before reload but fails to delete an
owned mesh package after reload, with a file-sharing error.

**Root cause.** Detaching the external actor's linker does not detach the
separate mesh package linker and its bulk-data file handle.

**Fix.** Reset loaders for the exact derived owned mesh package before retiring
its file. Never follow an attached foreign mesh to choose the deletion target.
Report the failed filename if native deletion fails.

**File.** `ProjectWorldConstructedEditor/Private/ProjectWorldConstructedOutput.cpp`.

**Regression.** `Project.World.Realization.Constructed.DependencyLocalLifecycle`
checks selective removal after save/reload, undeclared-removal refusal, preserved
neighbor bytes and subsequent reload/no-op.

## 69. Simple-ring encoding does not prove a basin contour is convex

**Symptom.** An inferred basin accepts a reflex contour despite its strictly
convex model contract.

**Root cause.** The model delegated convexity to a shared encoding certificate
whose contract also supports simple concave rings for other consumers.

**Fix.** Keep finite/simple encoding in the shared verifier and check every
exact contour turn in the basin owner before native construction.

**File.** [Identified basin](../../../../tools/World/CanonicalCompilation/refinement/identified_basin.py).

**Regression.** `IdentifiedBasinTests.test_reflex_contour_refuses_instead_of_building_a_basin`
inserts an inward notch in the captured influence contour and proves refusal
without input mutation. Valid convex basin and shared concave-ring proofs remain.

## 70. Building placement centers are not physical interaction footprints

**Symptom.** A joint basin candidate accepts a Building overlapping the selected
Ground while the Building's owner and placement center lie outside it.

**Root cause.** A placement-support helper deliberately returns the center
clamped to the owner cell; it cannot establish physical separation.

**Fix.** Basin admission uses selected effective Building-volume polygons, or
the outline when volumes are absent, and includes prospective constructed
features from earlier operations. An unresolved physical envelope refuses the
whole candidate before publication.

**File.** [Basin admission](../../../../tools/World/CanonicalCompilation/refinement/basin_admission.py).

**Regression.** `BasinAdmissionTests` checks an unselected-owner crossing
footprint, an effective volume outside its outline, and an actual earlier
structural creation, preserving base bytes and leaving no refused output.

## 71. General polygon overlay can reject a valid compiled triangle comparison

**Symptom.** A compiled-field audit rejects a complete stored field with a native
triangle intersection convex-contract error, despite a small height error.

**Root cause.** The general polygon overlay quantizes its paths and can split a
near-coincident positive triangle intersection. Treating that output as a single
convex intersection makes the audit fail before measuring the real coverage.

**Fix.** The MeshTerrain private field oracle uses the native triangle-only
intersection query with local finite, nondegenerate, counterclockwise inputs.
Exact separating halfplanes exclude disjoint interiors. Existing coverage,
positive overlap and height limits still reject actual field defects. Positive
intersection pairs must agree in their original orientation: normalization alone
would hide an actual-only winding reversal.

**File.** [Compiled-field fidelity](../../ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/Private/ProjectWorldMeshTerrainSurfaceFidelity.cpp).

**Regression.** `Project.World.Realization.MeshTerrain.SplitIntersection` retains
the captured source face and stored neighborhood, equivalent reversed-both inputs,
actual-only winding refusal, an interior hole, distinct overlap and missing,
duplicated and height-shifted controls. `SectionStorageSplit` exercises
the complete permanent fixture through actual saved/reloaded sections.
