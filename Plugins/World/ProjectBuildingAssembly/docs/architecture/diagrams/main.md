# Building assembly consumption

```mermaid
flowchart LR
    Canonical[ProjectWorld canonical feature and final Terrain] -->|public volume and triangle support| Adapter[World building adapter]
    Authored[Authored overlays] -->|protected selections| Adapter
    Adapter -->|post-classification selected exclusion| Massing[Existing massing builder]
    Kit[Original kit contract] -->|dimensions and provenance| Assembly[Building assembly module]
    Adapter -->|qualified rectangle and support envelope| Assembly
    Assembly -->|prepared plan and output validation| Native[Native ISM and Static Mesh components]
    Massing -->|remaining blockouts| Scene[World buildings projection or isolated scene]
    Native -->|selected assembly| Scene
```

The adapter belongs to ProjectWorldBuildingEditor. The assembly module has no
ProjectWorld dependency and cannot write Terrain. World owns persistent kit assets,
assembly actors and remaining massing through one buildings-layer manifest and
transaction. Concrete production admission remains separate from the fixture.
