# Structure

## Purpose

ProjectWorld defines reusable Unreal runtime and editor behavior for realizing
canonical geographic authority as streamed Worlds. Concrete Kazan and Manhattan
data stays outside this component.

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
- World-specific gameplay or presentation.

## Composition

| Part | Responsibility |
|---|---|
| `ProjectWorld` | Runtime contracts, services, and reusable actors |
| `ProjectWorldEditor` | Canonical-input realization and editor validation |
| Territory contracts | Stable authority, identity, and acceptance semantics |
| World Partition policy | Runtime streaming and cell policy |

## Relationships

Relationships are drawn once in [the main view](diagrams/main.md).

Through the canonical manifest contract, realization consumes authenticated
engine-independent inputs. Through generated manifests and package identity,
runtime code consumes only output produced by the accepted realization
transaction.

## Invariants

1. Source evidence, canonical authority, generated Unreal state, and runtime
   state remain distinct ownership layers.
2. Concrete city data never forks reusable generation or realization logic.
3. Generated state changes only through its owning realization and promotion
   routes and is never repaired by hand.
4. A receipt or hash proves identity only for the boundary it authenticates.
5. While compatibility marker `L004` exists, definition-host helpers read and
   write `AProjectWorldActor` direct metadata before using the generic
   interface/component host route. This preserves placement round trips until
   the compatibility path is removed with its consumers and tests.

The [legacy/component integration regression](../../../../Test/ProjectIntegrationTests/Source/ProjectIntegrationTests/Private/Integration/ObjectParentGeneralizationIntegrationTest.cpp)
covers both supported host shapes.

Concrete World support and retirement policy belongs to each World data or
product-composition owner, not to this reusable component.
