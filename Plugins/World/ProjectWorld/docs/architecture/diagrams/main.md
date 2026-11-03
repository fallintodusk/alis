# Main View

This view shows ProjectWorld's boundary between engine-independent authority
and Unreal realization/runtime consumption.

```mermaid
flowchart LR
    Sources[Accepted source evidence] -->|accepted receipts| Compiler[CanonicalCompilation]
    Compiler -->|provider-neutral compile result| Canonical[ProjectWorldData canonical authority]
    Canonical -->|authenticated base or selected-child effective input| Editor[ProjectWorldEditor]
    Refinement[CanonicalCompilation refinement API] -->|separately identified feature child| Canonical
    Host[World generation host] -->|qualified canonical transition and recovery| Refinement
    Host -->|selected Apply and output recovery| Editor
    Editor -->|saved output and accepted consumed identities| Host

    subgraph ProjectWorld[ProjectWorld]
        Editor -->|producer-neutral transaction| Registry[Layer producer registry]
        Editor -->|generated output| Runtime[ProjectWorld runtime]
        Contract[World contracts] -->|schema and identity rules| Editor
        Contract -->|runtime interpretation| Runtime
    end

    Leaves[Road, Water, Vegetation, Building, Gameplay editor modules] -->|register shared editor contract| Registry
    Adapter[ProjectWorldMeshTerrain editor adapter] -->|register shared editor contract| Registry
    Leaves -->|generated packages and manifests| Runtime
    Adapter -->|shared MPD and compiled sections| MeshPartition[Epic MeshPartition]
    Material[ProjectMaterial authority] -->|material object path, saved bytes authenticated| Adapter
    Material -->|accepted semantic role, saved resource identity| Editor
    Registry -->|declared reference-only capability| Presentation[Existing material slots]
    Presentation -->|same geometry, separate adoption provenance| Runtime
    MeshPartition -->|generated package and manifest| Runtime
    Runtime -->|World Partition and streaming APIs| Unreal[Unreal runtime]
    Fixtures[ProjectWorldTestData] -->|synthetic contract inputs| Editor
```

The [refinement-owned views](../../../../../../tools/World/CanonicalCompilation/refinement/docs/architecture/diagrams/README.md)
describe candidate construction and selection internals. The
[World host contract](../../../../../../scripts/ue/world/README.md) owns coordinated
canonical/World commit and recovery; a child pointer change alone cannot certify
saved World output. Pure Ground/platform construction is not an admitted child.
