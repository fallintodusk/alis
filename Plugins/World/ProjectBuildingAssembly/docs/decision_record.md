# ProjectBuildingAssembly - Standing Decision Record

Status: editor-only component with a portable layout core, original test kit and
native ISM realization, consumed by the opt-in World building projection. The [current architecture](architecture/structure.md)
distinguishes implemented behavior from this accepted production target.

## Decision

ALIS buildings will be produced by fully procedural modular assembly, not by
monolithic hand-built building meshes:

- JSON building blueprints will describe each building; planned contract
  locations are `Data/Buildings/` and `Data/Schemas/`;
- reusable mesh kits will supply the modular pieces; planned location
  `Data/Kits/`;
- C++ assembly in the editor component realizes the prototype; later admission
  will expose the complete blueprint-to-geometry workflow;
- reference photography drives blueprint authoring, not asset sculpting.

## Authority split with ProjectWorld

SOT for the full split: [Building geometry authority
split](../../ProjectWorld/docs/territory_contract.md#building-geometry-authority-split).
Summary:

- ProjectWorld owns canonical footprints, height evidence, massing
  constraints, and disposable territory blockouts.
- ProjectBuildingAssembly owns optional modular assemblies for SELECTED
  stable building feature IDs, consumed through a stable public
  footprint/massing contract; it never couples to ProjectWorld internals
  and never owns geographic truth. The implemented editor adapter lives in
  ProjectWorldBuildingEditor and passes a neutral layout input to Assembly.
  Assembly has no ProjectWorld dependency; the
  [current architecture](architecture/structure.md) owns that composition.
- An assembled feature explicitly replaces or suppresses its ProjectWorld
  blockout; competing building geometry is forbidden. Unselected buildings
  keep ProjectWorld massing. Authored hero buildings remain protected
  overlays and are never forced through the assembly generator.

## Regeneration governance

Assembly output is a generated layer under the ProjectWorld layered
regeneration contract: profile-owned, disposable, manifest-owned, and
subject to per-layer admission (synthetic implementation, protected
fixtures, full regeneration matrix) before its first territory or playable
use. An isolated disposable prototype proves its chosen inputs and realization;
enabling the generated layer in a territory still requires that admission.

## Conditional intake

Legacy art kits enter only after a licensing/provenance audit and a
measured integration prototype; kit intake is conditional even though the
procedural direction itself is not.

## Prototype preflight

ProjectWorld already exposes canonical footprints and effective volumes in
[its canonical input types](../../ProjectWorld/Source/ProjectWorldEditor/Public/ProjectWorldCanonicalBundle.h)
and final terrain sampling in
[its geometry query](../../ProjectWorld/Source/ProjectWorldEditor/Public/ProjectWorldGeneratedActors.h).
These provide an input seam for an isolated prototype; they do not establish
production layer admission. The editor prototype marshals a qualified rectangle
and support envelope through its own public layout contract. Complete Terrain/Water release
acceptance and Terrain preparation optimization are not prototype entry gates.
Verify the chosen feature's footprint, effective volume, owner and final-Terrain
placement against its pinned input. A query or support failure blocks that case;
unrelated geography does not. Another cell's Terrain proof is not foundation proof.

The [ground-anchor regression](../../ProjectWorld/Source/ProjectWorldBuildingEditor/Private/Tests/ProjectWorldBuildingGroundTest.cpp)
covers existing massing consumption and refusal when final ground is absent.
Current massing uses a footprint-centre anchor and spatial authored masks;
whole-foundation support and explicit selected-feature assembly replacement
are not established by that test. A prototype must prove its own replacement
and deterministic local regeneration without changing unselected massing.

Legacy mesh filenames alone establish neither a usable modular kit nor its
dimensions, snap grid, or provenance. Admit a tiny kit under the
[world asset policy](../../../../docs/legal/world_data_and_asset_policy.md)
before using it. The original procedural test kit is defined under
[Data](../Data/README.md); this does not admit a production art kit. The City17 vendor
array migration is a separate scope, not a prerequisite for this prototype.
Original procedural test pieces with recorded authored provenance can satisfy
the tiny-kit preflight without vendor intake. Source-derived building inputs
retain their own provenance; creating the pieces does not change those terms.

## Owed before first production use

- Admit the concrete production selection through the existing World building
  projection's profile/manifest and recovery route.
- Qualify the consumed kit and chosen footprint/support cases at the intended
  fidelity; unsupported cases remain explicit refusals.
- Wire deterministic input/output and regeneration evidence into the world
  validation route and complete its required admission matrix.
