# Structure

## Purpose

ProjectWorld defines producer-neutral Unreal runtime and editor behavior for
realizing canonical geographic authority as streamed Worlds. Concrete Kazan and
Manhattan data and engine-specific terrain adapters stay outside this component.

## Owns

- Runtime World contracts, spatial services, and reusable actor behavior.
- Editor realization of accepted canonical inputs.
- Generated-layer identity, transaction boundaries, and validation hooks.
- World Partition and streaming policy shared by concrete Worlds.

## Does not own

- Acquisition and engine-independent compilation, owned by
  [World tools](../../../../../tools/World/README.md).
- Concrete geographic authority and generated packages, owned by
  [ProjectWorldData](../../../ProjectWorldData/README.md).
- Synthetic fixtures, owned by
  [ProjectWorldTestData](../../../ProjectWorldTestData/README.md).
- MeshPartition adaptation, shared MPD policy, and compiled terrain sections,
  owned by [ProjectWorldMeshTerrain](../../../ProjectWorldMeshTerrain/README.md).
- Material graph compilation and generated material authority, owned by
  `ProjectMaterial`; ProjectWorld never depends on ProjectMaterialEditor.
- World-specific gameplay or presentation.

## Composition

| Part | Responsibility |
|---|---|
| `ProjectWorld` | Runtime contracts, services, and reusable actors |
| `ProjectWorldEditor` | Producer-neutral canonical-input realization, transactions, registry, and editor validation |
| `ProjectWorldRoadEditor`, `ProjectWorldWaterEditor`, `ProjectWorldVegetationEditor`, `ProjectWorldBuildingEditor`, `ProjectWorldGameplayEditor` | Layer-owned settings, hashing, realization, artifacts, and verifies |
| Territory contracts | Stable authority, identity, and acceptance semantics |
| World Partition policy | Runtime streaming and cell policy |
| Presentation resource resolution | Read accepted material manifests and authenticate role bindings, package bytes and loaded state |

## Relationships

Relationships are drawn once in [the main view](diagrams/main.md).

Through the canonical manifest contract, realization consumes authenticated
engine-independent inputs. Through generated manifests and package identity,
runtime code consumes only output produced by the accepted realization
transaction. Layer producers register through `IProjectWorldLayerProducer` and
consume the shared editor contracts. The Mesh Terrain adapter alone depends on
MeshPartition; generic ProjectWorld code does not expose that dependency to
consumers. The contributor route and revision rule live in the
[territory producer contract](../territory_contract.md#add-or-change-a-layer-producer).

The existing World presentation reader resolves a single material per role from
ProjectMaterial's accepted manifest. It authenticates instance and parent files,
their loaded origin and clean state before realization. Road, building and water
owners receive the same resource context during assignment and saved inventory
inspection. No World component compiles a ProjectMaterial graph or depends on
ProjectMaterialEditor. Without a prepared water binding, the existing local
diagnostic path remains explicit; existing diagnostic artifacts stay enrolled
until a reference census and the owning transaction permit retirement.

Reference-only application uses the same producer registry and transaction as
geometric realization, with a distinct capability declared by each presentation
owner. All selected owners validate before any save. Existing slot metadata and
geometry payloads are retained; geometric Apply and artifact reconstruction are
excluded. Accepted geometry lineage remains immutable while material-resource
provenance is recorded separately. The [operator route](../../../../../scripts/ue/world/README.md#material-references-without-geometric-realization)
owns invocation and recovery requirements.

Subsequent selected geometry authenticates the retained original presentation
source and adopted logical declarations/object paths. Current appearance bytes
remain ProjectMaterial authority; historical adoption is binding-time provenance.
Same-path material tuning therefore does not rewrite World authority. Only the
selected geometric owner receives new lineage, with its existing dependency guards.

## Invariants

1. Source evidence, canonical authority, generated Unreal state, and runtime
   state remain distinct ownership layers.
2. Concrete city data never forks reusable generation or realization logic.
3. Generated state changes only through its owning realization and promotion
   routes and is never repaired by hand.
4. A receipt or hash proves identity only for the boundary it authenticates.
5. Generated terrain has one active producer; no fallback writer or parallel
   generated terrain authority is supported.
6. While compatibility marker `L004` exists, definition-host helpers read and
   write `AProjectWorldActor` direct metadata before using the generic
   interface/component host route. This preserves placement round trips until
   the compatibility path is removed with its consumers and tests.

The [legacy/component integration regression](../../../../Test/ProjectIntegrationTests/Source/ProjectIntegrationTests/Private/Integration/ObjectParentGeneralizationIntegrationTest.cpp)
covers both supported host shapes.

Concrete World support and retirement policy belongs to each World data or
product-composition owner, not to this reusable component.
