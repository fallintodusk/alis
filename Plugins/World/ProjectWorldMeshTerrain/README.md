# ProjectWorldMeshTerrain

Replaceable Unreal terrain-realization adapter for ProjectWorld's
producer-neutral terrain contract. It owns the MeshPartition dependency, shared
definition and build policy, private semantic-channel mapping, adapter receipts,
and generated compiled sections.

The layout receipt's adapter compiler fingerprint covers the reviewed
realization and layout-shaping source set, including the runtime transformer
and partition. Audit commandlets and tests do not participate; changing
diagnostics alone cannot invalidate generated terrain or its material binding.

The repository-level authority rule is owned by
[Canonical World Authority](../../../docs/architecture/principles.md#canonical-world-authority).
This adapter does not define or reinterpret World semantics, accept authored
changes in generated packages, or become a required dependency of ProjectWorld.
The surrounding contract and relationship graph belong to
[ProjectWorld architecture](../ProjectWorld/docs/architecture/README.md).
