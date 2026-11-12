# ProjectWorldMeshTerrain

Replaceable Unreal terrain-realization adapter for ProjectWorld's
producer-neutral terrain contract. It owns the MeshPartition dependency, shared
definition and build policy, private semantic-channel mapping, adapter receipts,
and generated compiled sections.

Its `project_mesh_terrain:v1` descriptor declares the output revision and
editor modules; the shared pipeline revision and engine build identity complete
the World producer fingerprint. The adapter owns typed settings validation,
base-mesh realization, producer identity tags, artifact capture, output verify,
and the post-Apply MeshPartition builder. Its layout receipt authenticates the
complete adapter payload, while ProjectMaterial consumes only the stable
layout ID, version, and `layout_sha256` fields.

Terrain Apply treats the supplied canonical cells as the complete desired base
domain. It reuses existing bases whose owned per-cell identity still matches,
and removes authoring bases outside that domain. Dirty unit IDs do not select a
smaller input domain in a fresh world. A regional projection therefore belongs
only in a new disposable scene; applying it to an existing city would remove
omitted bases.

The `ProjectWorldMeshTerrainEvidence` editor module registers the Mesh Terrain
evidence subject that editor captures wait for. It only observes preview
sections and writes nothing. Its native preview headers need the narrowly derived
Engine internal include path. The editor module's actual-section fixtures use the
same include adaptation for MeshPartition's public section APIs; neither module
changes engine files or disables API validation.

The repository-level authority rule is owned by
[Canonical World Authority](../../../docs/architecture/principles.md#canonical-world-authority).
This adapter does not define or reinterpret World semantics, accept authored
changes in generated packages, or become a required dependency of ProjectWorld.
The surrounding contract and relationship graph belong to
[ProjectWorld architecture](../ProjectWorld/docs/architecture/README.md).

## Final-surface realization and fidelity

The adapter consumes ProjectWorld's typed final terrain triangles, including
optional complete-quad canonical refinement. The canonical producer owns that
representation and its quality policy; the adapter only realizes it. It retains
unrefined grid vertex ordering/UVs, carries semantic weights to inserted vertices,
and rejects conflicting attributes at coincident vertices. Source capture checks
all final vertices, triangles and role weights rather than only grid samples.

For refined cells the audit checks complete compiled CPU triangle coverage and
affine heights at triangle-intersection vertices. Missing, duplicate or
overlapping faces and stale interiors fail even when original grid corners
match. Exact native sidedness excludes impossible overlaps between disjoint
stored triangles before checking reconstructed clips. The private compiled-field
oracle uses the native triangle intersection query in a local comparison frame,
with counterclockwise input normalization, rather than a general polygon overlay.
Coverage area, affine height and derived-overlap measurements retain that frame;
restoring derived cuts to large canonical XY before measuring can invent a gap.
This avoids overlay-generated splits on near-coincident triangle edges while
retaining the complete coverage, overlap and height gates. Positive intersections
must preserve source-relative winding before normalization. The captured
`Project.World.Realization.MeshTerrain.SplitIntersection` regression also checks
equivalent reversed-both inputs, actual-only winding refusal, missing faces,
an interior hole, distinct overlap, duplicate and height-shifted fields. A native spatial tree
bounds candidate lookup. The audit's Nanite-enabled
section still exposes CPU fallback buffers; those buffers alone do not certify
GPU clusters. `Project.World.Realization.MeshTerrain.RefinedSurface` separately
captures actual Nanite and explicitly selected fallback depth and queries
compiled collision. Packaged/city performance and physics acceptance remain
separate from this disposable Editor fixture.

`Project.World.Realization.MeshTerrain.CoastalIntersection` retains a captured
four-piece cover that distinguishes local-frame measurements from canonical
roundtrip loss. Missing, duplicate, shifted and reversed fields still refuse.

The [captured composite consumer test](Source/ProjectWorldMeshTerrainEditor/Private/Tests/ProjectWorldMeshTerrainCompositeTests.cpp)
exercises the inner/provider field through native MeshPartition data and a saved,
reloaded StaticMesh. It checks both semantic channels at the native data boundary
and the complete compiled CPU field against its specifying triangles. Its
[self-contained fixture generator](../../../tools/World/CanonicalCompilation/tests/composite_mesh_fixture.py)
uses a reduced captured source quad. A hash-bound external fixture can retain a
larger captured neighborhood without making permanent tests depend on scratch
data. The bounded complete-grid DTO accepts at most 32 by 32 samples, with every
quad present; a mechanical reindex across source cells must retain owner lineage,
XYZ, weights and the original engine frame. Full-cell frontier, section partition, GPU presentation and gameplay use
their respective acceptance envelopes.

`Project.World.Realization.MeshTerrain.CanonicalAuthoredDry` uses the same
hash-bound external fixture variables, but requires a `canonical_result`
path/hash and `cell_id` instead of injected typed geometry. The normal canonical
loader authenticates the complete result before selecting one complete original
cell. `terrain_queries` independently declare immutable bilinear and final affine
heights at interior XY witnesses. `required_physics_points` declare actual final
collision heights. `player_eye` declares canonical `position` and `target`.
The [adapter](Source/ProjectWorldMeshTerrainEditor/Private/Tests/ProjectWorldMeshTerrainCanonicalFixture.cpp)
checks these inputs; the existing registered Terrain Apply and three normal modes
generate and save actual MeshPartition components. Replay unloads original objects
before reopening and rechecks configured collision and the compiled field.
The isolated nonpartitioned scene enables native external actor packaging for
Terrain Apply. Replay inventories and hashes those packages, releases the original
actors/meshes/colliders, then checks the complete reloaded actor/package bindings
and unchanged map/external bytes. This is not World Partition streaming proof.

For unchanged base Terrain and ordinary Water, the
[regional observation tests](Tests/README.md) own preparation, exact native
commands, query/physical witnesses and reduced-domain proof limits.

The representative perspective PNG uses the accepted Terrain material and a
depth/empty-geometry control. It is a saved-cell rendering check, not a Hero,
navigation, Water-presentation, packaged or geographic-quality verdict. This
exact test requires RHI, `ALIS_NO_PERSISTENT_EDITOR=1`, `-ProjectSkipFrontEnd`,
`-EditorPerProjectUserSettingsINI=<repo>/tmp/<run>/editor.ini`, and the safe runner's
`-UseTestExitOnly`. Input, output and log paths stay under repository `tmp/`;
clear unrelated basin gameplay/replay overrides and serialize the Unreal lane.

`ProjectCompositeLocalStorage` selects an isolated `CompositeConsumer` storage
comparison with an explicit inverse/forward mesh transform. It preserves the
canonical and engine world frames and the ordinary whole-field fidelity bound.
`Project.World.Realization.MeshTerrain.ContinuationStorageFrame` separately
exercises the native translated partition parent with the
[captured three-quad continuation](../../../tools/World/CanonicalCompilation/tests/ground_continuation_mesh_fixture.py).
Its configured visual and collision components decode through their observed
transforms, including a missing-compensation refusal. This storage test reports
generic collision contacts and slopes; geographic support, walkability, actual
section partition and reload, cross-cell/Water seams and production frame
selection require their own proofs. It does not change the production partition
inventory or the shared definition.

The optional [compiled-section storage transformer](Source/ProjectWorldMeshTerrainEditor/Private/ProjectWorldMeshTerrainStorageTransformer.h)
copies freshly parent-local mesh data before float conversion, retaining all
attributes. Each native section uses one XY-bounds-center/Z0 frame across its
units and an explicitly compensated actor transform. Rebuilds derive that frame
from the parent and fresh input, so an existing section translation cannot
accumulate. Cached source meshes remain immutable. The transformer is available
to private pipeline candidates; it is absent from the shared production
definition. Native cancellation and errors are observable but do not themselves
establish a publication guard; callers must qualify the actual emitted geometry.

`Project.World.Realization.MeshTerrain.SectionStorageContract` covers parent-local
reuse, multiple units, source attributes and malformed-input refusal.
`ProjectCompositeSectionStorage` selects private storage pipelines in
`ContinuationStorageFrame`. The separate exact
`Project.World.Realization.MeshTerrain.SectionStorageSplit` uses a diagnostic
native 30m grid, checks the combined stored field and every shared decoded station,
and rejects a genuine 1mm discontinuity. This grid is not production World
Partition policy. Physical queries must hit a collision component from the
declared variant; capsule footprints can span adjacent sections. GPU/collision
records retain the actual frames and any cross-section contacts. Null, foreign
and nonblocking hits exercise the ownership predicate. Disabling the complete
owned collision set removes support; restoring it restores both physical probes.
The split test also unloads every original instance and reopens the saved map in
its isolated cold Editor process. The same collector measures the fresh sections
without rebuilding, with saved identities, frames and map bytes authenticated.
These tests do not substitute for full-neighborhood gameplay or promotion.

Collision field evidence decodes the actual `BodySetup` cooked triangle meshes.
Provider input is retained separately: standard native cooking can remove a
small input facet. The comparison undoes only the native `bFlipNormals` index
swap, and records raw cooked triangles. Input-facet support queries remain
independent; switching their census to the surviving cooked facets would hide
an unsupported source point. Optional authenticated retained support witnesses
also run before and after reload, with owned-collider, height-fault and disabled
collision controls. Malformed declared witness data refuses before scene creation.

`Project.World.Realization.MeshTerrain.CoastalCookSupport` uses the
[captured two-quad fixture](../../../tools/World/CanonicalCompilation/tests/coastal_cook_mesh_fixture.py).
Its original bank loses a facet during native cooking; a bounded derived-edge
retessellation preserves the complete field and the old support point. The
exporter's `--original` variant retains the known-bad geometry for reproduction.
The positive fixture needs no retained scratch inputs. `PhysicsQueryInput`
covers the same witness parser's missing, malformed and nonfinite cases.

`Project.World.Realization.MeshTerrain.CoastalJoined` is an opt-in diagnostic,
discovered only with an explicit `PROJECT_WORLD_COMPOSITE_TERRAIN_FIXTURE`.
Prepare a fresh reduced typed terrain input with the hash-bound original
canonical base, retained constructed-runtime input and coastal context accepted
by the [coastal fixture adapter](Source/ProjectWorldMeshTerrainEditor/Private/Tests/ProjectWorldMeshTerrainCoastalFixture.cpp).
The normal fixture SHA/output variables remain mandatory; malformed declared
inputs refuse. This diagnostic needs normal RHI and the explicit gameplay,
Water and configured-Hero selections described below. It has no default
dependency on a prior scratch run; the captured cook regression above remains
self-contained.

The joined diagnostic binds native section parts, saved identities and frames,
actual cooked topology, retained separate Upper and original ordinary Water.
It checks short bidirectional Hero lanes, source-qualified near/far rendered
witnesses and native spatial unload/return. Pixel controls identify the chosen
pixel, retain all other witness classes and recover the exact original masks.
Its test grid and sampled views do not establish production partition policy,
complete-route walkability, survey accuracy or a polished final scene.

`Project.World.Realization.MeshTerrain.IdentifiedBasinSections` exercises the
accepted definition's configured collision, Nanite and fallback section pipelines
in a scratch world with a cloned definition. It binds actual actor/component
transforms, GPU fallback buffers and the collision transformer's separate output.
World rays and explicit generic capsule sweeps cover every collision face, with
collision-off/restoration controls. Its default input is the permanent captured
joint-basin exporter; no prior scratch run is required.

Optional `PROJECT_WORLD_BASIN_GAMEPLAY=1` uses actual Terrain Apply/provider
channels, the accepted shared definition read-only and a separate native World
Partition builder commandlet. The saved scene is reopened before native PIE and
configured-character walking. Saved BuildKeys and non-placeholder checks reject
silent PIE reconstruction. Each variant's saved actor package also binds the
native external-actor instance codec and runtime cell/world; a memory-package
prefix cannot establish saved ownership. This mode requires a cold exact-test process, explicit
GameMode/pawn/definition selection, source control disabled and separate parent/
child `EditorPerProjectUserSettingsINI` files under project scratch. The native
content plugin lives until process exit; owned scene packages unload first.
Adding `PROJECT_WORLD_BASIN_WATER=1` includes the registered Water producer in
that same saved scene, with the accepted `water.calm_river` binding. The shared
typed fixture helper refuses malformed model numbers and coordinates before
publishing feature ownership. Joint replay checks the frozen material identity,
complete GPU vertices/indices, zero-lift stage, nonphysical Water role and saved
package identity. Stale material evidence exercises the actual refusal path;
Water actor and mesh instances must die at both reload boundaries.
The joint mode also schedules the World-owned rendering probe after walking:
native-frame near/far views, same-array eight-neighbour-eroded candidate
agreement and explicit boundary residuals. The leaf-codec compatibility quantity
is conditional; it does not bound actual Nanite LOD or continuous footprints.
The original full-candidate identity property remains separately reported.
Native/fallback/native proxy controls, opaque/interior and boundary-height
occluders, exact mask recovery, vacuity and missing membership qualify the
observed-view recipe. FinalColor images retain accepted materials and the
existing screenshot validator.

Optional `PROJECT_WORLD_BASIN_ENCODING_PRECISION` builds an unsaved native mesh
from identical source attributes, materials and build/reduction settings, checks
the complete GPU field and requires the requested precision to be effective.
Native construction's `NeverStream` difference is reported explicitly. Without
gameplay, this is a fast encoding-only check using the same permanent fixture;
with gameplay, any transient mesh swap restores original pointers and flags.
Neither mode admits a saved candidate or production policy.
Navigation, streaming lifetime, geographic accuracy and
production admission have separate acceptance surfaces.

Optional `PROJECT_WORLD_BASIN_REPLAY` and `PROJECT_WORLD_BASIN_REPLAY_SHA256`
select a hash-bound saved-scene manifest for navigation diagnosis. The helper
validates every declared native input before copying it into fresh project
scratch, rechecks copied bytes, and binds the typed fixture, shared definition
and saved map. It reuses native loading, package ownership, physics and the same
navigation checks without a builder, long traversal or rendering. A separately
named result prevents this shortcut from claiming the omitted properties.
The default permanent test remains self-contained; replay is an optional
diagnostic input, never a dependency on an earlier scratch run.
Adding explicit `PROJECT_WORLD_BASIN_REPLAY_FULL=1` instead runs the complete
saved-scene walking, rendering and native streaming-return checks on that copy;
it still does not rerun the producer or builder.
For an authenticated coastal replay, literal
`PROJECT_WORLD_BASIN_REPLAY_FULL=streaming` instead prepares the same actual Hero
and runs streaming-return diagnosis with fresh native field/identity checks.
It omits route traversals and views, and emits a distinct diagnostic-only result.
Unchanged upstream receipts must be authenticated separately. Unswept return
setup records requested and observed positions against the exact native
subtract-then-add target for an unattached capsule; this is not bit-exact equality
to the requested anchor or a relaxation of source/physical walking limits.

Navigation samples the actual path corridor's detail surface. Every covering
polygon must return the same finite height for two distinct input heights at
exactly the requested XY. Query errors, missing detail/coverage and unsupported
links refuse; the straight waypoint chord remains a separate diagnostic.
Actual saved Terrain collision and removal/rebuild/restoration controls qualify
the sampled result. Optional `PROJECT_WORLD_BASIN_NAV_ELEVATION_RATIO` selects
a disposable zero/one native approximation comparison. It records live settings
and restores the actor property; generated candidate tiles remain until World
teardown. This comparison does not admit production policy.

The optional `PROJECT_WORLD_BASIN_NAV_TERRITORY_PROFILE` and matching
`PROJECT_WORLD_BASIN_NAV_TERRITORY_PROFILE_SHA256` select the actual World-owned
territory navigation helper on the reduced typed Bundle. This arm retains
invoker-only generation and native setting rematching. Elevation-ratio overrides
are excluded. A separately identified cell-aligned tile comparison retains the
original horizontal tiers above the helper's minimum; it is a disposable
candidate rather than production policy. A separately identified
`PROJECT_WORLD_BASIN_NAV_CELL_HEIGHT_CM` comparison changes the two fine-tier
vertical resolutions while retaining the Low tier. It records derived voxel
clearance/climb separately from the unchanged nominal agent and step settings.
Per-build observations own the effective native settings; earlier
observations describe the input state. Profile parsing alone does not qualify
full profile compatibility: the receipt explicitly separates the reduced Bundle
from the profile's grid, route, spawn and partition realization. Wider projection
queries are failure diagnostics only; they cannot satisfy the original route
height bound. Actor properties restore before disposable World teardown; generated
candidate tiles are not saved navigation authority.
