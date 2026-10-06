# Territory Generation

Lean entry point for world reconstruction. Open only the contract section or
owner README needed for the current task; do not load the deep contract as a
default session bootstrap.

## Route by task

| Task | Single source of truth |
|---|---|
| Full architecture, ownership, data flow, and operator observability | [Architecture](architecture/README.md) |
| Generation order, identity, geospatial authority, error budget | [Territory contract](territory_contract.md#purpose) |
| Logic/data ownership and persistent canonical authority | [Territory contract](territory_contract.md#ownership-layers) |
| Layer add/remove/order and precise dirty regeneration | [Territory contract](territory_contract.md#layered-regeneration-contract-scale-out-precondition) |
| Authored anchors and protected overlays | [Territory contract](territory_contract.md#authored-anchor-semantics-byte-equality-is-not-enough) |
| Generated manifests, transactions, enrollment, retirement | [Territory contract](territory_contract.md#generated-artifact-manifest-and-drift-validation) |
| World-data roots and proof split | [Territory contract](territory_contract.md#world-data-roots-and-manual-polish-layer) |
| Kazan v1 envelope, grid, margins, and ceilings | [Territory contract](territory_contract.md#territory-envelope-v1-operator-decision-2026-08-10) |
| Delivery stages and final acceptance | [Territory contract](territory_contract.md#delivery-stages) |
| World Partition design and measurement | [World Partition](world_partition.md) |
| Native reuse gate and excluded experimental systems | [World Partition](world_partition.md#unreal-native-reuse) |
| Realization, enrollment, recovery, and audit commands | [Canonical World Realization](../../../../scripts/ue/world/README.md) |
| Source, compile, validation, and acceptance commands | [World tools](../../../../tools/World/README.md) |
| Test-layer selection and cadence | [World pipeline layers](../../../../docs/testing/world_pipeline_layers.md) |
| Full replay versus canonical-authority Matrix selection | [World pipeline layers](../../../../docs/testing/world_pipeline_layers.md#fresh-agent-route) |
| Verified implementation traps | [Pitfalls](pitfalls.md) |

## Owner map

| Owner | Responsibility |
|---|---|
| `ProjectWorld` | Reusable world contracts, C++/script logic, and tests; no concrete UE content. |
| `ProjectWorldMeshTerrain` | Replaceable MeshPartition adapter, shared MPD, private channel layout, compiled-section realization, and Mesh runtime proof. |
| `ProjectMaterial` | Universal material graphs, instances, recipe schemas, generated material assets, and Editor compiler. |
| `ProjectWorldData` | Concrete territory profiles, authored data, persistent canonical bundles, generated UE packages, and manifests. |
| `ProjectExperienceData` | Data-only product-composition records that route normal loading to concrete generated maps; no world semantics. |
| `ProjectWorldTestData` | Editor-only synthetic inputs and authored fixtures; generated packages/manifests are ignored transient test output. |
| `SourceIngestion` | Acquire, verify, normalize, and receipt provider data. |
| `CanonicalCompilation` | Compile admitted inputs into deterministic canonical cells and persistent bundles. |
| `EndToEndValidation` | Compose evidence boundaries; it does not own source, compiler, or Unreal generation logic. |

Concrete data always follows its data owner. Reusable logic never defaults to
a concrete production or fixture plugin.

World material assignment is semantic and soft-reference based. ProjectWorld
authenticates the `terrain.default` ProjectMaterial identity and saved asset; it
does not compile material graphs. ProjectWorldMeshTerrain owns the shared MPD
binding and the private mapping from canonical surface names to MeshPartition
channels. Same-path material tuning changes ProjectMaterial authority without
changing canonical geography or rebuilding unchanged Mesh Terrain sections.

ProjectWorld has no dependency on ProjectMaterialEditor. The adapter communicates
its channel layout and build policy through a schema-validated plain-data receipt,
so richer terrain appearance can evolve without importing material-editor code into
World realization.

Packaged Water appearance remains a separate product-acceptance concern. Water is
realized from canonical geometry and fitted elevation as cell-local non-Nanite
StaticMeshes. Its diagnostic BaseColor capture proves Water geometry and occlusion;
its FinalColor capture proves lit product presentation. Neither capture creates a
second terrain authority or changes the canonical surface.
