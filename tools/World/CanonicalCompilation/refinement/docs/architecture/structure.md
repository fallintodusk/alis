# Canonical refinement structure

## Purpose and boundary

This CanonicalCompilation child derives final-surface candidates from an
immutable authenticated compile result. It owns candidate construction,
feature-child preparation and qualified selection primitives. It does not
acquire provider data, recompile the base, generate Unreal meshes or coordinate
World output commits. The [surface contract](../../../terrain_refinement.md)
owns supported models, provenance, refusal and activation semantics.

## Immediate parts

| Part | Owner |
|---|---|
| Public construction and selection operations | [API](../../api.py) |
| Pure raster Ground construction | [Ground constructor](../../ground.py) |
| Finite Ground sampling and shared-edge station closure | [Resolution constructor](../../ground_resolution.py) |
| Pure source-associated platform Road construction | [Constructed-surface constructor](../../constructed_surface.py) |
| Construction, feature publication and selection implementation | [Implementation](../../app/README.md) |
| Result, request and selection schemas | [Contracts](../../contracts/README.md) |

## Relationships and invariants

The [main view](diagrams/main.md) states this child's boundary relationships.
Model-specific views are routed from the [diagram index](diagrams/README.md).

The immutable base and selected child retain separate identities. Current
candidate preparation authenticates consumed implementation/source/native
inputs and producer origin; frozen read/recovery authenticates retained bytes
without claiming current producer equivalence. The [selection contract](../../../terrain_refinement.md#qualified-selection-publication)
owns those distinct lifecycles.

Pure Ground, contained-basin and constructed-platform capabilities are not
production admission. Their fixture consumption cannot be used as a publication
certificate. Closed bridge/confluence feature preparation and existing qualified
selection are different public operations. The [effective-authority contract](../../../terrain_refinement.md#effective-authority-candidates)
owns accepted selection; consumers must not add hidden geographic corrections.

Source qualification and OSM context stay with [SourceIngestion](../../../../SourceIngestion/README.md).
Qualified derived Ground admission belongs to its
[supplemental products child](../../../../SourceIngestion/supplemental/README.md).
Native geometry stays with [ExecutionEnvironment](../../../../ExecutionEnvironment/README.md).
The [World transaction](../../../../../../scripts/ue/world/README.md#canonical-realization-contract)
coordinates canonical selection with Unreal output/recovery. This child changes
one canonical pointer; it does not promise atomicity across both owners.
