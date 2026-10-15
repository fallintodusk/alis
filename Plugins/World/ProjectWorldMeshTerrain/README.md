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

The `ProjectWorldMeshTerrainEvidence` editor module registers the Mesh Terrain
evidence subject that editor captures wait for. It only observes preview
sections and writes nothing, and it is the only module that includes
MeshPartition's editor preview headers, which need the Engine internal include
path.

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
match. A native spatial tree bounds candidate lookup. The audit's Nanite-enabled
section still exposes CPU fallback buffers; those buffers alone do not certify
GPU clusters. `Project.World.Realization.MeshTerrain.RefinedSurface` separately
captures actual Nanite and explicitly selected fallback depth and queries
compiled collision. Packaged/city performance and physics acceptance remain
separate from this disposable Editor fixture.
