# World Partition Architecture

Current generated-world realization, representation, streaming, and acceptance
policy for ProjectWorld.

## Contents

- Architecture and ownership
- Generated terrain representation
- Unreal native reuse
- World Partition and locality
- Generated authority and transactions
- Runtime and performance acceptance
- Editor workflow and verification
- Scope boundaries

## Architecture and ownership

ProjectWorld owns producer-neutral world realization contracts, generated-layer
registration, transactions, manifests, World Partition policy, and runtime
acceptance. Concrete profiles, canonical bundles, generated packages, and maps
belong to ProjectWorldData.

Generated terrain follows one path:

```text
CanonicalCompilation final terrain surface
    -> ProjectWorld terrain producer contract
    -> ProjectWorldMeshTerrain adapter
    -> Epic MeshPartition authoring and compiled sections
    -> World Partition streaming
```

CanonicalCompilation owns terrain elevation, Water fitting, hydro conditioning,
authored terrain patches, and named surface semantics. Unreal realization does
not reinterpret, clamp, or classify that final surface.

ProjectWorldMeshTerrain is the only component allowed to depend on Epic
MeshPartition. It owns the shared Mesh Partition Definition, private channel
indices, authoring actors, compiled sections, adapter receipts, and the mapping
from canonical semantic names to MeshPartition channels. ProjectWorld does not
depend on ProjectMaterialEditor. Material appearance remains ProjectMaterial
authority and is authenticated through plain data and saved asset identity.

World Partition controls runtime residency. Canonical cells control compilation,
dirty regeneration, and generated ownership. MeshPartition sections control the
terrain representation. These grids have different responsibilities and are not
forced to match.

## Generated terrain representation

Kazan and Manhattan select the registered `project_mesh_terrain:v1` producer.
Their realization profiles own current numeric policy, including section
complexity and runtime mapping. The production profiles use one shared adapter
definition and a fixed producer-owned section bound; they do not split sections
to match the World Partition runtime grid.

Each canonical terrain cell supplies the immutable final height field and named
semantic weights. The adapter emits the platform variants required by the
accepted MeshPartition contract:

- collision sections for gameplay and navigation;
- Nanite render sections where the target supports them;
- full-resolution non-Nanite fallback sections for supported render targets.

Compiled sections are derived output. Their identity includes canonical terrain
input, the normalized producer contract, adapter/compiler fingerprint, shared
definition layout, and build policy. An unchanged Apply and builder run must
reuse unchanged sections and write no geography packages.

The terrain channel ABI is semantic at the ProjectWorld boundary and private at
the adapter boundary. Canonical names describe only facts their derivation
proves. The current terrain surface uses `ground` and `hydro_transition`; slope
alone must not be promoted to a factual geological class such as `rock`.

Generated Landscape is not a supported producer, fallback, migration route, or
parallel terrain authority. City17 remains separately authored Landscape
content under its own plugin. Its continued existence does not authorize
Landscape code in the generated-world pipeline.

## Unreal native reuse

ALIS owns geographic policy, deterministic canonical input, identity,
provenance, dirty scope, orchestration, and acceptance. Unreal owns engine
geometry, assets, rendering, partitioning, serialization, and streaming.

| Need | Native owner | Project boundary |
|---|---|---|
| Projected coordinates | GeoReferencing | ProjectWorld supplies authenticated CRS and origin data and verifies placement error. |
| Terrain realization | MeshPartition | ProjectWorldMeshTerrain adapts canonical cells and owns the shared definition and channel layout. |
| Polygon and mesh operations | GeometryCore and GeometryAlgorithms | ProjectWorld keeps canonical geometry engine-independent and uses native editor geometry only during realization. |
| Persistent meshes | MeshDescription and StaticMesh | Editor realization builds saved assets; packaged runtime never regenerates them. |
| Spatial residency | World Partition and OFPA | ProjectWorld maps stable generated ownership to spatial actors and audits the result. |
| Repeated vegetation | HISM | The admitted producer keeps deterministic cell-local placement authority. |

Blueprint Geometry Script is not a core dependency. Experimental Water does not
own canonical or realization identity. MeshPartition is isolated behind the
replaceable ProjectWorldMeshTerrain adapter rather than exposed across
ProjectWorld.

## World Partition and locality

Each realized world has one minimal persistent World Partition map plus spatial
generated actors. Runtime partition settings are map-owned and profile-driven.
When a realization profile declares a runtime profile ID, the apply/validate
wrapper requires that exact runtime profile before any content transaction.
Changing only a runtime profile must preserve canonical cells, generated-layer
manifests, terrain producer input, and geography package bytes.

Runtime route actors keep stable grid-and-role identity across accepted runtime
profiles. ProjectWorld may update runtime policy in place, but it removes a
route actor only when runtime realization is absent or the canonical grid no
longer owns that role.

Terrain locality is canonical-cell based:

- a bounded canonical terrain change dirties only affected terrain cells and
  declared dependants;
- a Water geometry or fitted-surface change dirties the intersecting final
  terrain cells and their dependants;
- a same-path material content change dirties ProjectMaterial authority, not
  terrain geography;
- a runtime-grid change does not rebuild MeshPartition sections;
- removal uses the union of current and previously accepted owned units.

Water, roads, building massing, vegetation, and gameplay placement keep their
existing cell- or object-local producers. They depend on the neutral terrain
contract and must not inspect MeshPartition internals.

HLOD is disabled for the generated territories. No generated HLOD layer, actor,
proxy mesh, simplified mesh, or companion package may appear. Nanite is used for
compatible opaque generated meshes. Water remains a persistent non-Nanite
StaticMesh because its admitted shading path is not a Mesh Terrain or HLOD
fallback.

## Generated authority and transactions

The generated map is a serialized projection, never an editable source of
truth. Apply reads an authenticated canonical bundle and realization profile,
preflights the complete layer graph, then mutates through the existing snapshot,
journal, rollback, manifest, and active-set transaction.

Every generated layer owns an exact artifact root and manifest. External actor
or object packages belong to a layer only when the commandlet names each exact
file under the confined map roots. Tags and filename prefixes do not establish
durable ownership.

An unchanged operation preserves map packages, actors, compiled sections,
manifests, and active scope identity. A rejected operation restores the exact
prior files, including prior absence. Authored roots remain byte-identical
across Apply, reconstruction, rollback, and Delete.

Generated-package no-op is decided from deterministic semantic input and output
identity before serialization. Unreal package byte stability is not used as the
semantic no-op definition.

## Runtime and performance acceptance

The acceptance route is product-first:

```text
default game entry
    -> menu experience selection
    -> ProjectLoading travel
    -> production GameMode and possessed character
    -> generated terrain collision and navigation
    -> centre to edge to centre streaming
```

A direct map open, Editor fly-through, or synthetic pawn is diagnostic only.
The packaged route authenticates the selected experience, map, runtime profile,
player, generated terrain owner, collision, interaction, unload, and reload.

Static partition audit and packaged telemetry measure different properties.
Static audit verifies actor bounds, external packages, canonical ownership,
runtime mapping, references, Data Layers, representation policy, and HLOD
absence. Packaged telemetry verifies actual streaming completion, failures,
frame time, memory, and product behavior.

The product performance gate can opt into `-ProjectWorldNativeSteady` in a
packaged Development run. After the accepted product route, it waits for
streaming completion plus 60 settled frames, then collects exactly 600 frames
at the current player location without a screenshot or CSV capture. The
existing native engine performance consumer supplies Frame, Game, Render,
GPU, and RHI timings; the frame p95 is compared with the existing 16.67 ms
budget. GPU timing is engine-reported and can be inferred rather than directly
measured by hardware. Missing or non-discriminating GPU samples reject the measurement.
ProjectTexture prewarm and cache timing remain evidence owned by ProjectTexture,
not dependencies of the World gate.

Topology and runtime-grid tuning reopen only when the same accepted package and
hardware envelope shows a reproducible regression. A material appearance task
does not authorize terrain topology changes. World Streaming Insights and debug
draw commands are diagnostic tools, not acceptance authority.

## Editor workflow and verification

Use the project wrappers; do not repair generated packages manually.

```powershell
# Apply or validate generated authority through the documented world wrapper.
Get-Content scripts/ue/world/README.md

# Build the Editor target.
.\scripts\ue\standalone\build.ps1

# Run one exact automation test during iteration.
.\scripts\ue\test\unit\iterate.ps1 -TestFilter <exact-test-name>
```

The command and receipt owner for realization, recovery, manifest validation,
partition audit, packaged runtime proof, and durable authority audit is
[Canonical World Realization](../../../../scripts/ue/world/README.md). Test
depth and gate escalation are owned by
[World pipeline layers](../../../../docs/testing/world_pipeline_layers.md).

Useful native diagnostics include `wp.Editor.DumpActorDescs`,
`wp.Runtime.DumpStreamingSources`, and the `wp.Runtime.ToggleDraw*` family.
Diagnostic output does not replace the applicable static, commandlet, packaged,
or authority receipt.

## Scope boundaries

- ProjectWorld remains producer-neutral and cannot include MeshPartition or
  ProjectMaterialEditor dependencies.
- ProjectWorldMeshTerrain contains the complete MeshPartition-specific adapter.
- ProjectMaterial owns richer terrain appearance and its generated material
  authority; appearance work is independent of geography regeneration.
- ProjectWorldData owns concrete Kazan and Manhattan profiles and generated
  assets.
- City17 owns its authored Landscape and any City17-specific material repair.
- A future terrain producer replaces the registered tuple and passes the same
  locality, transaction, cook, packaged-runtime, and authority gates. It does
  not coexist as a fallback writer.

Official engine references:
[World Partition](https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-in-unreal-engine),
[Mesh Partition Definition](https://dev.epicgames.com/documentation/unreal-engine/mesh-partition-definition-in-unreal-engine), and
[Builder commandlets](https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-builder-commandlet-reference).
