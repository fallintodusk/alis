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
