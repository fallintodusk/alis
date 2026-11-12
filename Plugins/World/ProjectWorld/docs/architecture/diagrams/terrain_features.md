# Terrain and feature consumption

Current representation boundaries. The [territory contract](../../territory_contract.md#terrain-and-feature-separation)
owns their authority and adjustment semantics.

```mermaid
flowchart LR
    Raster[Accepted raster observations] --> Compiler[CanonicalCompilation]
    Vectors[Accepted vector and context records] --> Compiler
    Patches[Provenance-bound authored Terrain patches] --> Compiler
    Compiler --> Ground[Final canonical Terrain]
    Compiler --> Water[Canonical Water geometry and level]
    Compiler --> Features[Canonical feature geometry, volumes and modeled fields]
    Compiler --> Grade[Explicit immutable civil grades]

    Ground --> Adapter[ProjectWorldMeshTerrain]
    Adapter --> Mesh[Generated Mesh Terrain sections]
    Ground --> Queries[Final-surface and immutable-grid query contract]
    Grade --> Queries
    Queries --> Placement[Road, Building, Vegetation and placement consumers]
    Water --> WaterProducer[Registered Water producer]
    WaterProducer --> WaterMesh[Separate Water mesh]
    Features --> Placement
    Features --> Constructed[Registered constructed-surface producer]
    Constructed --> StructuralMesh[Separate structural meshes]
    Placement --> FeatureActors[Separate generated feature actors]

    Authored[Protected authored Unreal content] --> Anchors[Canonical anchor resolution]
    Ground --> Anchors
    Features --> Anchors
    Anchors --> Instances[Generated anchors referencing authored content]
```

Feature geometry and generated objects do not feed height changes back into
Terrain during Unreal realization. The selected source/model representation,
surface query and consumed input identities determine each consumer's behavior.
Terrain refinement and canonical authored patches remain upstream of realization.

The raster remains evidence for its declared surface kind and accuracy. A DSM
can contain object or canopy heights even when generated object meshes are
separate. This view does not assert a bare-earth extraction capability.
