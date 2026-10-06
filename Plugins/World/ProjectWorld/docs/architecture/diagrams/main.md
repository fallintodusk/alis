# Main View

This view shows ProjectWorld's boundary between engine-independent authority
and Unreal realization/runtime consumption.

```mermaid
flowchart LR
    Sources[Accepted source evidence] -->|accepted receipts| Compiler[CanonicalCompilation]
    Compiler -->|provider-neutral compile result| Canonical[ProjectWorldData canonical authority]
    Canonical -->|accepted realization input| Editor[ProjectWorldEditor]

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
    MeshPartition -->|generated package and manifest| Runtime
    Runtime -->|World Partition and streaming APIs| Unreal[Unreal runtime]
    Fixtures[ProjectWorldTestData] -->|synthetic contract inputs| Editor
```
