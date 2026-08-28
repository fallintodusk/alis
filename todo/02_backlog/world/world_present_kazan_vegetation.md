# Present Kazan vegetation

**Status:** RESEARCH COMPLETE / IMPLEMENTATION NOT SELECTED
**R1:** PASS - 2026-08-26
**Change type:** Generated World presentation and deterministic vegetation realization
**Owning black box:** ProjectWorld vegetation realization
**Concrete configuration owner:** ProjectWorldData realization profile
**Concrete mesh owner:** ProjectObject content
**Campaign:** [Kazan presentation research](world_plan_kazan_presentation_campaign.md)
**Operator sequencing prerequisite:** [Shared material generation core](../content/material_generate_assets_from_json.md)
must be implemented and accepted before this concern is selected for implementation.
Research may continue now.

## Research decision

Keep the accepted canonical-record -> deterministic cell placement -> cell-owned
instanced-component architecture. Do not introduce a second scatter authority.

The current generator has one real extensibility limit: canonical vegetation semantics
participate in cell input hashing but not mesh selection. That limit does not justify a
semantic contract by itself. If V0 proves repetition among otherwise equivalent generic
trees, A1 first expands the qualified flat `mesh_assets` list on the accepted v1 tuple.
Semantic palette S1 is selectable only if V0 qualifies at least two meaningfully distinct
semantic asset groups and proves that semantic selection addresses the observed gap.

Density diagnosis is independent: `area_spacing_m` already belongs to v1 and a safe
spacing-only profile change does not inherently require a semantic schema migration.
However, the analytical `31.82 m` Kazan projection produces one post-exclusion candidate
inside the known `grass` area under current v1 behavior. Unless an exact production-path
dry-run disproves that diagnostic, D1 requires minimal non-tree guard G1 on a new v2 tuple.
G1 retains flat generic asset selection and does not prebuild semantic-palette machinery.
Rendering representation stays HISM for every first candidate;
ISM is benchmarked only if the accepted Nanite-only target and measured evidence justify
a separate representation change.

This is a researched candidate, not implementation authorization or visual acceptance.

## Product outcome

Make Kazan vegetation frequent and varied enough to sell the city envelope while
preserving:

- canonical geographic authority;
- deterministic and idempotent regeneration;
- cell-local World Partition ownership and rollback;
- road, water, and authored-mask exclusions;
- packaged RTX 4070 High 1440p/60 acceptance;
- a JSON data-definition path that can admit future qualified tree assets without a code
  branch per species.

## Stable routes

- [World realization SOT](../../../Plugins/World/ProjectWorld/docs/territory_generation.md)
- [Territory contract](../../../Plugins/World/ProjectWorld/docs/territory_contract.md)
- [World Partition contract](../../../Plugins/World/ProjectWorld/docs/world_partition.md)
- [World generation pitfalls](../../../Plugins/World/ProjectWorld/docs/pitfalls.md)
- [Plugin dependency rules](../../../docs/architecture/plugin_rules.md)
- [Testing strategy](../../../docs/agents/canonical.md#7-testing-strategy)
- [World proof layers and TestData ownership](../../../docs/testing/world_pipeline_layers.md)
- [ProjectObject asset owner](../../../Plugins/Resources/ProjectObject/README.md)
- [ALIS plugin enablement](../../../Alis.uproject)
- [ProjectPCG descriptor](../../../Plugins/World/PCG/ProjectPCG/ProjectPCG.uplugin)
- [ProjectPCG boundary](../../../Plugins/World/PCG/ProjectPCG/README.md)
- [Kazan realization profile](../../../Plugins/World/ProjectWorldData/Data/Profiles/Realization/kazan_territory_v1.realization.json)
- [Kazan validation profile](../../../Plugins/World/ProjectWorldData/Data/Profiles/EndToEndValidation/kazan_territory_v1.validation.json)
- [Epic UE 5.8 ISM guidance](https://dev.epicgames.com/documentation/en-us/unreal-engine/instanced-static-mesh-component-in-unreal-engine)
- [Epic UE 5.8 Nanite Foliage status](https://dev.epicgames.com/documentation/en-us/unreal-engine/nanite-foliage)
- [Epic UE 5.8 PCG framework](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-framework-in-unreal-engine)
- [Epic UE 5.8 Virtual Shadow Maps](https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine)

## Boundary declaration

### Expected to change if selected

- ProjectWorldEditor only if G1 or S1 is selected: vegetation validation, classification,
  deterministic selection, realization routing, registry, and every code-constructed
  synthetic/test consumer found by the v1 census;
- the Kazan vegetation profile/validation settings for any selected candidate;
- every tracked active profile, fixture, and validation expectation consuming v1 if G1
  or S1 selects a v2-only cutover;
- the closed realization-profile JSON schema only if G1 or S1 is selected;
- generated Vegetation actors/components only, through normal immutable regeneration;
- the active Vegetation manifest/reference through the normal transaction; superseded
  immutable manifests remain untouched provenance;
- validation topology and stable World docs only after a candidate is accepted.

### Explicitly untouched

- canonical Kazan bundle, source ingestion, and vegetation semantics;
- terrain, water, roads, buildings, gameplay placements, authored overlays, and their
  manifests;
- landscape material, water material, road material, and universal material recipes;
- runtime scatter, per-tree actors, collision, navigation, and HLOD;
- ProjectObject asset bytes; V0 qualification and A1/S1 references are read-only uses;
- ProjectPCG, PCG graphs, Foliage Mode, Landscape Grass, Procedural Foliage, Nanite
  Foliage, and manual editor paint;
- production files while this todo remains in research review.

Unexpected propagation into any untouched owner is a stop condition.

## Verified ALIS baseline

### Authority and accepted topology

The active canonical authority is
`kazan_territory_v1:86821a9914b758e4c536f72fd549333b1953605decbb74ba9c71c2c32360809b`.
The accepted end-to-end topology records:

```text
210 canonical cells
146 vegetation cell actors
279 HISM components
7,528 candidates
7 road exclusions
19 water exclusions
1 authored-mask exclusion
7,501 retained instances
```

`7,528 - 7 - 19 - 1 = 7,501`, so the accepted aggregate shows no silent
over-cap truncation. It does not prove that no individual cell lands exactly on the
1,024-instance ceiling because current receipts do not expose per-cell populations.

The active Vegetation manifest has 146 cell-local actor artifacts. The accepted
realization tuple is `project_vegetation_instances:v1` with canonical-cell dirtying,
zero dependency halo, and explicit dependencies on terrain, water, and roads.

### Current profile

The profile uses:

```text
mesh_assets:
  Hornbeam Medium
  AmurCork Big
area_spacing_m: 45.0
area_jitter_fraction: 0.35
scale: 0.85 .. 1.15
maximum_instances_per_cell: 1,024
placement_policy: canonical_points_and_lattice_areas
nanite: true
collision: no_collision
```

ProjectObject contains six candidate tree StaticMesh assets: five Hornbeam shapes and one
AmurCork shape. Only two are admitted by the current profile. The names do not establish
a qualified broadleaf group, rendering fitness, or botanical truth; V0 must inspect the
assets and their actual presentation before either A1 or S1 can use them.

### Canonical semantic coverage

A read-only census of the active canonical bundle found 1,953 vegetation features:

```text
feature class:
  foliage_point: 1,475
  vegetation_area: 478

vegetation_class on areas:
  wood: 464
  forest: 13
  grass: 1

foliage_class:
  tree: 1,475

leaf_type:
  broadleaved: 266
  needleleaved: 184
  mixed: 20

leaf_cycle:
  deciduous: 189
  evergreen: 139
  semi_evergreen: 1
  mixed: 1

species:
  Prunus serrulata: 4
  Pinus strobus: 1
```

Most point records have only the generic `tree` class. Species is present on only five
features. Therefore generator v2 must not build a general taxonomy engine or require
species coverage. The first admitted selector set is limited to semantic dimensions that
both the corpus and qualified assets can support.

The one `grass` area is `alis:osm:way:81810837` in cell `x1:y3`. A read-only analytical
replica of the current deterministic lattice and exclusion rules found zero candidates at
`45.0 m`. At `31.82 m`, it projects one post-exclusion candidate at
`(380984.26042, 6187271.38598)`, outside the replicated road, water, and authored-mask
exclusions. This is diagnostic projection, not an authoritative production-path dry-run.
It proves no current accepted instance wrong because this feature contributes zero at
`45.0 m`. It makes G1 plus D1 the expected route, while an exact production dry-run may
still falsify the projection and reopen D1 on unchanged v1.

### Verified code behavior

- `HashResolvedCellInput` hashes `VegetationClass`, `FoliageClass`, `LeafType`,
  `LeafCycle`, and `Species`.
- `AddInstance` receives only a stable instance ID and point. It chooses a mesh by
  hashing `deterministic_seed | StableId | mesh` into the flat mesh array.
- Every non-`foliage_point` vegetation feature currently enters `AddAreaInstances`;
  there is no `VegetationClass` gate before lattice generation.
- Changing `leaf_type` invalidates the vegetation input hash in the existing test but
  cannot change the selected mesh. The current semantics are therefore invalidation
  inputs, not presentation inputs.
- Instances are sorted by stable ID and then truncated to the per-cell maximum. Current
  stats do not record pre-cap cell population or truncation count.
- Realization creates one static, no-collision, no-navigation HISM component per used
  mesh in each cell actor. It sets no component cull distance and disables HLOD.
- `Apply` accepts only `project_vegetation_instances:v1`; registry validation and the
  profile schema also know only the flat v1 settings shape.
- Existing focused tests cover placement determinism, exclusions, admitted asset load
  plus Nanite enablement, and retired-cell persistence. They do not cover semantic mesh
  selection, fallback, per-cell truncation telemetry, or a v2 tuple.
- The current tracked v1 consumer search finds the Kazan realization profile and validation
  expectation, the active Kazan Vegetation manifest, and C++-constructed synthetic layers
  in `ProjectWorldVegetationRealizationTests.cpp` and
  `ProjectWorldRealizationProfileTests.cpp`. The tracked ProjectWorldTestData realization
  JSON is currently Landscape/Water-only; it is not itself a Vegetation v1 consumer.
  Superseded immutable Vegetation manifests are provenance, not executable consumers.

## V0 - mandatory read-only diagnosis

No profile or generated bytes change in V0.

1. Reconnect Unreal MCP and inspect all six ProjectObject tree assets. Record exact
   package path, bounds, triangle/Nanite state, shape-preservation mode, material slots,
   opacity/masking, WPO use and disable distance, shadow behavior, collision, and asset
   dependencies. The MCP connection was unavailable during this research, so these
   binary properties remain explicitly unverified.
2. Capture authenticated packaged baseline views at dense centre, representative
   residential/open areas, centre-to-edge route, and an edge unload/reload return.
3. Record near/mid/far visibility, canopy volume, repetition, empty-area coverage,
   cluster pop, shimmer, shadow noise, and whether the perceived rarity is placement or
   distant representation loss.
4. Record `stat unit`, `stat gpu`, `stat rhi`, `stat streaming`, primitive/instance
   counts, HISM build/culling cost, Nanite triangles/clusters, VSM page invalidation,
   GPU memory, working set, and streaming failures on the physical RTX 4070 contract.
5. Add read-only per-cell population analysis from canonical/profile inputs. Report
   maximum, p95, cells above the projected density-candidate ceiling, exact-at-cap cells,
   and projected truncation before any density change. Authenticate the grass diagnostic:
   zero current candidates and the projected density candidate's candidate/exclusion result.
6. Classify each mesh first as qualified generic variety or rejected. Separately record
   whether inspected evidence supports a distinct broadleaf, conifer/evergreen, or other
   semantic group. A filename alone cannot admit either classification.

V0 selects branches independently:

```text
visual already satisfies the operator
  -> no Vegetation implementation

repetition is proven; additional shapes are qualified but not semantically distinct
  -> A1 flat-palette expansion on project_vegetation_instances:v1

semantic mismatch is proven and at least two distinct semantic groups are qualified
  -> S1 semantic palette on project_vegetation_instances:v2

semantic mismatch is proven but the required asset groups do not exist
  -> record an asset gap; do not invent a mapping or generator contract

sparse placement alone is proven
  -> D1 single 31.82 m density candidate
  -> v1 only if the production dry-run proves zero post-exclusion non-tree candidates
  -> otherwise G1 minimal non-tree guard at 45.0 m, then D1 on v2

both gaps are proven
  -> choose A1 or S1 from qualified asset evidence at current population
  -> choose v1 or G1 from the production density preflight
  -> run D1 only after the selected variety/correctness candidate passes
```

If foliage only loses volume at distance, investigate the admitted asset's stable Nanite
settings instead of selecting a variety or density branch. Never use S1 merely because a
new version is needed for G1, and never invent a false species mapping.

## A1 - flat-palette variety candidate

A1 is the cheapest response when V0 proves visual repetition and qualifies additional
generic shapes but cannot prove distinct semantic groups. It changes only the explicit
ordered `mesh_assets` profile list on `project_vegetation_instances:v1`; no parser,
schema, registry, placement algorithm, or source asset changes.

The isolated A1 candidate must authenticate every added ProjectObject asset, regenerate
only Vegetation artifacts through normal immutable replacement, and compare identical
views and runtime evidence. Because the stable hash maps into the list length, adding a
mesh can reselect shapes across the existing population; that is expected presentation
output, not a geometry-position change. Coordinates, exclusions, transforms, counts,
cell ownership, and HISM representation remain fixed.

## G1 - minimal non-tree correctness candidate

G1 is selected only when the exact D1 production preflight finds a post-exclusion
candidate from a known non-tree vegetation area. It introduces the smallest clean v2
contract: keep v1's flat `mesh_assets` selection and settings shape, classify the admitted
area semantics before lattice generation, and skip known non-tree areas such as `grass`.
It does not add `asset_palette`, semantic selectors, weights, or leaf/species taxonomy.

The selected G1 schema and registry accept only `project_vegetation_instances:v2`; the
new source rejects v1 under the clean-cutover policy. `wood` and `forest` remain
tree-bearing. Missing or unknown area semantics preserve generic v1 placement and are
reported only as aggregate diagnostic counts; they are not falsely called semantic
matches. G1 records `non_tree_feature_skipped_count` and
`unknown_area_fallback_feature_count`, with no per-instance warning spam. Placement,
generic mesh selection, exclusions, transforms, cap, and HISM output remain unchanged for
features not skipped. The internal namespaces advance to
`project_vegetation_cell_input_v2` and `project_vegetation_cell_v2`.

G1 must pass at `45.0 m` before D1 changes density. If a later accepted change needs
semantic asset selection, that is a new closed generator version unless S1 was selected
instead of G1 in this same not-yet-implemented decision.

## S1 - closed semantic palette candidate

S1 is selected only if V0 proves a semantic presentation gap and qualifies at least two
meaningfully distinct semantic asset groups. Repetition among one generic group selects
A1, and a non-tree correctness need without semantic assets selects G1. Candidate name
`S1` is not the generator version: if selected before any v2 implementation, its tuple is
`project_vegetation_instances:v2`.

The profile remains the concrete selection owner and references ProjectObject-owned
StaticMesh assets. Do not create a second World asset registry. Do not require full
ObjectDefinition actor composition for static HISM source meshes; that would couple a
cell renderer to ProjectObject spawning behavior it does not consume.

The v2 JSON shape must be closed and data-driven, approximately:

```json
{
  "asset_palette": [
    {
      "asset": "/ProjectObject/.../SM_Tree_A.SM_Tree_A",
      "semantic_match": {
        "leaf_type": ["broadleaved"],
        "leaf_cycle": ["deciduous"]
      },
      "weight": 1
    },
    {
      "asset": "/ProjectObject/.../SM_Tree_B.SM_Tree_B",
      "semantic_match": {
        "leaf_type": ["needleleaved"],
        "leaf_cycle": ["evergreen"]
      },
      "weight": 1
    }
  ],
  "fallback_assets": [
    "/ProjectObject/.../SM_Tree_Default.SM_Tree_Default"
  ]
}
```

Contract rules:

- schema rejects unknown fields and selector keys;
- `fallback_assets` and `asset_palette` are both required and non-empty;
- v2 defines a closed known vocabulary from the admitted canonical contract. A known
  value without a qualified asset is `unsupported`; an absent value or non-empty value
  outside that vocabulary is `unknown`. This distinction owns the receipt classification;
- asset paths are normalized, unique, loadable, Nanite-enabled, and inside admitted
  content mounts;
- arrays are normalized before hashing so JSON order cannot accidentally change output;
- selector fields are optional conjunctions; values within one field are alternatives;
- a candidate must match every present selector field;
- the narrowest matching entry set wins; ties use stable seeded weighted selection;
- supported known tree semantics use the qualified matching palette;
- absent, unknown, or mixed tree semantics use deterministic generic fallback without
  per-instance warning;
- known but unsupported tree semantics use the declared fallback but increment an
  aggregate unsupported-fallback receipt count; they are never reported as correct
  semantic matches;
- known non-tree vegetation classes emit no tree instance. Initial v2 treats `wood` and
  `forest` as tree-bearing area classes and `grass` as non-tree, subject to V0 production
  dry-run confirmation;
- invalid selector values or zero/negative weights fail profile validation;
- no substring inference, case folding, fuzzy species mapping, wildcard taxonomy, or
  code branch for Kazan;
- `species` is excluded from the first v2 schema. A later version may admit it only after
  exact matching assets and fixtures exist; five source records do not justify
  speculative species logic;
- placement coordinates, jitter, transforms, exclusions, cap, cell ownership, and
  HISM representation remain byte-for-byte equivalent when the same mesh is selected;
- semantic selection and fallback identity participate in cell semantic hashes and
  immutable manifest evidence;
- the v2 implementation bumps both internal namespaces to
  `project_vegetation_cell_input_v2` and `project_vegetation_cell_v2`;
- receipts record `semantic_supported_instance_count`,
  `semantic_unknown_fallback_instance_count`,
  `semantic_unsupported_fallback_instance_count`, and
  `non_tree_feature_skipped_count`. The first three count retained instances; the last
  counts canonical features skipped before lattice generation. No per-instance warning
  or log spam is allowed.

S1 may include additional qualified generic shapes inside an admitted group, but A1 owns
variety when only one group is qualified. It must not label any asset as conifer or
evergreen merely to cover canonical values.

## D1 - single density candidate, conditional

D1 is selected whenever V0 proves insufficient placement density; selection does not
depend on a palette/repetition gap. Its execution requires G1 or S1 when the production
dry-run finds a post-exclusion known non-tree candidate. There is no spacing matrix.

The single proposed candidate is approximately twice the area density:

```text
current spacing: 45.0 m
candidate spacing: 45 / sqrt(2) = 31.82 m
```

Before regeneration, a deterministic dry-run must report projected candidates and
retained instances per cell, including the known grass area's classification. Any
predicted silent truncation is a blocker. Prefer a profile spacing that stays within the
existing cap; do not raise the cap just to hide a failed preflight. Point-feature count
remains unchanged.

D1 changes only `area_spacing_m`. It may remain on v1 only when the authoritative dry-run
proves zero post-exclusion candidates from known non-tree features. The analytical grass
projection makes G1 plus D1 the expected Kazan route; only contrary production evidence
can reopen D1 on v1. S1 is not implied. Palette, seed, jitter, scale, component type,
collision, HLOD, and exclusions remain fixed. Compare the accepted pre-density `45.0 m`
state against exactly the same candidate at `31.82 m`; spacing is the only changed
variable. The paired receipt records both candidate identities and executable hashes.

## UE 5.8 capability decisions

| Capability | Verified UE 5.8 state | Decision for first candidate |
|---|---|---|
| ISM/HISM | Epic recommends ISM for Nanite-only use, while HISM can still suit thousands of static instances or fallback meshes; project-specific testing is required. | Keep accepted HISM to isolate palette/density. Benchmark ISM later only as one separate measured representation candidate. |
| Standard PCG | Epic's setup guide says PCG must be enabled per project. The exact installed UE 5.8 `PCG.uplugin` nevertheless sets `EnabledByDefault: true`; ALIS also explicitly enables `ProjectPCG`, whose descriptor enables Engine `PCG`. | Do not adopt. Availability/default state does not justify duplicating canonical placement, cell ownership, fingerprints, or rollback. Future enrichment may consume accepted records only through a separately researched adapter. |
| GPU PCG | Beta. | Reject for this release concern. It adds an execution authority and GPU pipeline without a proven product gap. |
| PCG Biome | Experimental. | Reject for release authority. |
| Procedural Foliage Tool | Enabled through Experimental editor preferences. | Reject. It is a second scatter/simulation authority. |
| Foliage Mode / Landscape Grass | Native authoring/scatter workflows. | Reject as generated authority; manual paint and landscape-driven scatter bypass canonical records and immutable manifests. |
| Nanite PreserveArea | Available but documented as the legacy foliage technique. | Audit current assets and test only if V0 proves distant volume loss. Do not call it the modern default or change assets silently. |
| Nanite Voxelize / Nanite Foliage | Experimental; Epic warns settings may change or be removed. | Do not ship or make it an R1 dependency. Revisit in a future engine-qualified research slice. |
| WPO wind | Supported but adds vertex work, conservative bounds, and VSM cache invalidation. | Audit only. Any material/WPO change is a separate ProjectMaterial-owned candidate and must meet the material-core prerequisite. |

Nanite-enabled foliage cull distances and instance fading are not a useful first lever,
and the current HISM realization sets no draw distance. V0 must still distinguish true
placement sparsity from Nanite simplification, occlusion, and material/shadow behavior.

## Alternatives rejected now

- More canonical features: source authority is frozen and the observed product gap has
  not been traced to missing source data.
- Per-tree actors: destroys the admitted instancing and streaming cost model.
- Manual Foliage Mode paint: non-idempotent, unauthenticated, and outside canonical
  ownership.
- Landscape Grass: creates material/landscape-driven population outside vegetation
  records and exclusions.
- PCG, PCG Biome, or Procedural Foliage replacement: duplicate placement authority and
  add regeneration/rollback surfaces without a proven need.
- Runtime scatter: violates persistent generated-layer authority.
- Nanite Foliage/Voxelize migration: experimental and an asset/rendering migration, not
  a density fix.
- HLOD: the admitted World Partition contract explicitly keeps vegetation instance-owned
  with zero vegetation HLOD.
- Broad density matrix: unnecessary expense; one mathematically bounded candidate is
  enough after diagnosis.
- Exact species taxonomy: the corpus and asset library do not support it.
- Automatic ProjectMaterial work: palette and spacing do not create material resources.
  The separate operator-approved campaign prerequisite still remains until explicitly
  changed.

## Implementation order after selection

1. Complete V0 and record the independently selected variety, correctness, and density
   branches: no-op, A1, G1, S1, D1, or the justified combination.
2. Add read-only per-cell pre-cap/truncation and semantic-classification telemetry before
   any density change. This diagnostic surface must not change generated bytes.
3. For A1 only, qualify each additional asset, change the v1 flat list in an isolated
   candidate, and prove no source/schema/registry change occurred.
4. For G1 only, write failing tests for known non-tree suppression, generic fallback,
   aggregate counts, internal v2 namespaces, stable ordering, and version routing.
5. For S1 only, write failing focused tests for v2 parsing, semantic classification,
   selection, fallback, receipt counts, invalid inputs, internal v2 namespaces, stable
   ordering, and version routing.
6. Before a v2-only cutover, enumerate every tracked
   `project_vegetation_instances:v1` consumer: serialized production/test profiles,
   validation expectations, code-constructed fixtures, registry assertions, and the
   active manifest selected by `active_set.json`. Classify superseded immutable manifests
   separately as provenance. An unclassified or unmigratable active consumer is a stop.
7. For the selected G1 or S1 shape only, register one closed
   `project_vegetation_instances:v2` contract and atomically migrate every active v1
   consumer from step 6. TestData consumes that same selected shape; there is no second
   v2 schema. Here `atomically` means one clean logical cutover with no mixed supported
   state; manifests/`active_set.json` use their existing transaction, and no new
   cross-filesystem transaction system is created for source/profile files. The new source
   rejects v1 and contains no alternate v2 shape.
8. Run the selected A1, G1, or S1 candidate at `45.0 m`, verify immutable-manifest
   locality, and capture paired visual/runtime evidence.
9. For D1, run the exact `31.82 m` production preflight and one candidate. Use v1 only if
   it disproves the projected grass candidate; otherwise accept G1 or S1 first and use v2.
10. Stop for operator visual approval before promotion and stable receipt migration.

## Verification contract

### Focused automated checks

- Existing exact tests remain green:
  - `Project.World.Realization.Vegetation.PlacementContract`
  - `Project.World.Realization.Vegetation.ExclusionContract`
  - `Project.World.Realization.Vegetation.AssetContract`
  - `Project.World.Realization.Vegetation.RetirementPersistence`
- For a G1/S1 cutover, produce a machine-readable tracked-consumer census with path,
  consumer kind, active/provenance classification, old tuple, and selected replacement.
  After migration, repository scan plus structured profile/active-manifest loading must
  prove zero executable v1 consumers and that production and synthetic tests use the same
  selected v2 settings shape. Historical immutable manifests may still record v1.
- For A1, extend the asset contract with every added path and prove the v1 tuple, schema,
  placement positions, transforms, exclusions, counts, and stable IDs remain unchanged.
- For G1, add exact tests proving:
  - known `grass` areas emit no tree and increment `non_tree_feature_skipped_count`;
  - `wood`/`forest` and missing/unknown area semantics retain flat generic selection,
    with only unknown features incrementing `unknown_area_fallback_feature_count`;
  - no palette/selector/weight field is accepted by the selected minimal v2 schema;
  - v2 emits only v2 cell input/output namespaces and current source rejects v1;
  - unchanged tree-bearing inputs preserve meshes, transforms, exclusions, and counts;
  - cap telemetry, locality, retirement, and exact rollback remain correct.
- For S1, add exact tests proving:
  - semantic change selects the expected admitted group and changes only Vegetation
    semantic identity;
  - absent/unknown/mixed semantics resolve through deterministic generic fallback;
  - known unsupported tree semantics use declared fallback and increment only the
    aggregate unsupported count;
  - `wood`/`forest` areas remain tree-bearing while known `grass` emits no tree instance
    and increments the non-tree feature count;
  - invalid selectors, weights, duplicate normalized paths, missing assets, non-Nanite
    assets, and empty fallback fail closed with owner-scoped logging;
  - JSON key and palette order normalization is deterministic;
  - unchanged selected mesh preserves transforms, exclusions, counts, and stable IDs;
  - v2 emits only v2 cell input/output namespaces and current source rejects a v1 profile
    after semantic cutover;
  - cap telemetry reports pre-cap count and truncation explicitly;
  - unrelated canonical cells and non-Vegetation layers remain byte-identical;
  - retirement and rollback restore the exact prior active generation.
- For D1, add a production-shaped preflight assertion for per-cell candidate, exclusion,
  post-exclusion, cap, and non-tree counts at both `45.0 m` and `31.82 m`. The D1 test must
  fail if a known non-tree candidate would reach placement under the selected tuple.

Use exact dev filters during implementation. Broad World gates run only at the accepted
end-of-slice checkpoint under the project gate policy.

### Regeneration and locality

- A1 and a safe D1 remain v1. G1 or S1 changes persisted generator behavior and therefore
  requires a clean v2 contract. Only the selected G1 or S1 schema is implemented.
- D1 alone can remain `project_vegetation_instances:v1` only when its authoritative
  preflight proves no known non-tree candidate survives exclusions. Otherwise G1 or S1
  plus D1 uses v2 after the guard is independently accepted at the original population.
- A1/G1/S1/D1 use normal immutable Vegetation-manifest replacement. No in-place generated
  actor mutation and no metadata-only fingerprint migration.
- Record active authority ID, profile hash, generator tuple, producer fingerprint,
  generation ID, previous/new manifest hashes, artifact hashes, per-cell counts,
  branch-relevant classification/fallback counts, and truncation count.
- Non-Vegetation active manifest hashes and generated artifact hashes must remain exact.
- Re-running identical inputs must produce a semantic no-op and no file rewrite.
- A v2-only source tree must fail the cutover gate if any tracked active profile, fixture,
  validation expectation, registry assertion, or current active manifest still requires
  `project_vegetation_instances:v1`.

### Runtime and performance

Primary comparison contract:

```text
physical RTX 4070
D3D12
High
2560x1440
packaged Development for instrumented timing
World Partition 512/1536
same route, cameras, warm-up, and sample duration
Frame p95 <= 16.67 ms
zero streaming failures
```

Record frame/Game/Draw/GPU p50/p95/p99, memory, committed working set, GPU memory,
loaded cells, actor/component/instance counts, draw calls, Nanite cluster/triangle stats,
VSM page invalidations, and streaming churn. Shipping separately proves cook and product
route correctness; it does not replace Development instrumentation.

Reject a candidate on any correctness, locality, streaming, memory, or frame gate even
if a screenshot looks better. If D1 fails after an accepted 45 m A1, G1, or S1 candidate,
keep that accepted candidate. If the selected branch fails, restore V0 and return that
gap to research.

### Visual and operator proof

- Authenticate every capture with map, build, profile, generator tuple, authority,
  camera transform, timestamp, and current active manifest.
- Use existing VisualVerification/MCP capture ownership; add no Vegetation-specific
  camera subsystem.
- Capture identical near/mid/far centre, residential/open, road edge, water edge, and
  centre -> edge -> centre views.
- For D1, pair the accepted pre-density `45.0 m` state with the same candidate at
  `31.82 m`; record both candidate/profile identities and executable hashes so spacing is
  proven to be the only changed variable.
- Prove no trees on protected roads/water/masks, no floating/sunken roots, no obvious
  cell seam/pop, no severe repetition, and visibly improved frequency/variety.
- Automated evidence precedes the final human walkthrough. Only the operator can accept
  presentation quality.

## Rollback and temporary-file ownership

Each implementation run owns:

```text
tmp/world/presentation/vegetation/<run-id>/
  baseline/
  candidate/
  receipts/
  screenshots/
  logs/
```

Before mutation, the owner captures one exact `-1` snapshot of only the selected branch's
changed bytes. A1/D1 include the ProjectWorldData profile/validation, generated external
actor packages, Vegetation manifest, `active_set.json`, World metadata, and executable
identity. G1/S1 additionally include changed ProjectWorldEditor source, schema, registry,
tests, every active v1 consumer found by the census, and the census receipt itself. A
post-G1/S1 source tree executes v2 only. Old immutable v1 manifests remain provenance,
not a reason to retain an executable v1 parser or rewrite historical bytes.

Rollback reverses the selected implementation patch and restores the complete exact v1
consumer set together with v1 code, schema, registry, profiles, tests, generated packages,
manifest, and active authority as applicable, then rebuilds through the standard
launcher-engine project script only when source changed.
It records the restored executable identity before the old accepted generation is run or
validated again. It never asks current v2 code to execute v1 and does not retain a
duplicate engine/source build or unnecessary packaged binary solely for rollback.

The owner script retains the accepted run plus exactly one previous `-1` recovery state,
deletes superseded run-local scratch and obsolete candidates, and never uses system temp
or destructive Git operations. After restoration it reruns the focused locality and
retirement checks. Other layer authorities remain untouched.

## Documentation propagation after implementation

Only after operator acceptance:

- if G1 is selected, update the owning stable territory-generation and World Partition
  docs with the minimal v2 non-tree guard, generic fallback, and telemetry;
- if S1 is selected, document its v2 semantic palette, fallback, and telemetry instead;
- if A1 alone is selected, record the admitted generic asset set in the existing profile
  documentation without inventing a semantic contract;
- update profile/schema documentation and validation topology;
- record the selected/rejected performance candidate in the stable evidence owner;
- archive the implementation todo only when the logical work and receipts are complete;
- never link a transient todo from stable docs or code.

## R1 accepted decisions

1. A1 flat-palette expansion on v1 is the first variety candidate unless V0 qualifies at
   least two meaningfully distinct semantic groups for S1.
2. The grass result is an analytical post-exclusion projection. G1 plus D1 is expected,
   while exact production preflight retains authority to reopen D1 on v1.
3. Minimal G1 v2 keeps flat generic selection and excludes palette/taxonomy machinery;
   S1 is a separately selected closed v2 shape.
4. Current HISM remains fixed for A1/G1/S1/D1. ISM is a later isolated benchmark only if
   evidence justifies it.
5. PCG and Experimental Nanite Foliage remain rejected for this release concern despite
   their newer UE 5.8 capabilities.
6. G1/S1 uses a clean cutover with no executable v1 compatibility and complete exact
   consumer/source restoration on rollback.
7. The cutover requires a repository-wide active v1 consumer census and one logical
   migration to the selected v2 shape. Historical immutable manifests are provenance,
   not executable-consumer requirements.
8. D1 compares the same accepted pre-density `45.0 m` candidate with spacing as the only
   changed variable and records both executable identities.
