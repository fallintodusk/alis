# Territory Generation

Lean entry point for world reconstruction. Open only the contract section or
owner README needed for the current task; do not load the deep contract as a
default session bootstrap.

## Route by task

| Task | Single source of truth |
|---|---|
| Full architecture, ownership, data flow, and operator observability | [Architecture overview](architecture_overview.md) |
| Generation order, identity, geospatial authority, error budget | [Territory contract](territory_contract.md#purpose) |
| Logic/data ownership and persistent canonical authority | [Territory contract](territory_contract.md#ownership-layers) |
| Layer add/remove/order and precise dirty regeneration | [Territory contract](territory_contract.md#layered-regeneration-contract-scale-out-precondition) |
| Authored anchors and protected overlays | [Territory contract](territory_contract.md#authored-anchor-semantics-byte-equality-is-not-enough) |
| Generated manifests, transactions, enrollment, retirement | [Territory contract](territory_contract.md#generated-artifact-manifest-and-drift-validation) |
| World-data roots and proof split | [Territory contract](territory_contract.md#world-data-roots-and-manual-polish-layer) |
| Kazan v1 envelope, grid, margins, and ceilings | [Territory contract](territory_contract.md#territory-envelope-v1-operator-decision-2026-08-10) |
| Delivery stages and final acceptance | [Territory contract](territory_contract.md#delivery-stages) |
| World Partition design and measurement | [World Partition](world_partition.md) |
| UE 5.8 native reuse gate and excluded experimental systems | [World Partition](world_partition.md#ue-58-native-reuse-gate) |
| Legacy transition | [Legacy world transition](legacy_world_transition.md) |
| Realization, enrollment, recovery, and audit commands | [Canonical World Realization](../../../../scripts/ue/world/README.md) |
| Source, compile, validation, and acceptance commands | [World tools](../../../../tools/World/README.md) |
| Test-layer selection and cadence | [World pipeline layers](../../../../docs/testing/world_pipeline_layers.md) |
| Verified implementation traps | [Pitfalls](pitfalls.md) |

## Owner map

| Owner | Responsibility |
|---|---|
| `ProjectWorld` | Reusable world contracts, C++/script logic, and tests; no concrete UE content. |
| `ProjectMaterial` | Universal material graphs, instances, recipe schemas, generated material assets, and Editor compiler. |
| `ProjectWorldData` | Kazan profiles, authored data, persistent canonical bundles, generated UE packages, and manifests. |
| `ProjectWorldTestData` | Editor-only synthetic inputs and authored fixtures; generated packages/manifests are ignored transient test output. |
| `SourceIngestion` | Acquire, verify, normalize, and receipt provider data. |
| `CanonicalCompilation` | Compile admitted inputs into deterministic canonical cells and persistent bundles. |
| `EndToEndValidation` | Compose evidence boundaries; it does not own source, compiler, or Unreal generation logic. |

Concrete data always follows its data owner. Reusable logic never defaults to
a concrete production or fixture plugin.

World material assignment is semantic and soft-reference based. ProjectWorld maps a
meaning such as `terrain` or `road.default` to a stable ProjectMaterial identity and
authenticates assignment in its presentation operation. It does not compile material
graphs. ProjectWorldData may provide sourced facts such as land cover or road class,
but never material paths, shader parameters, recipes, generated materials, or material
manifests.

`terrain.default` resolves to
`/ProjectMaterial/Generated/Terrain/MI_ProjectTerrain_Default`. Before mutation,
ProjectWorld authenticates the accepted ProjectMaterial manifest, instance package,
semantic identity, parent dependency, and current parent bytes. A reference migration
updates and dirties the root Landscape and every external streaming-proxy package in the
same logical Landscape family. Later same-path material tuning changes ProjectMaterial
authority only and does not dirty World packages or geography manifests.
