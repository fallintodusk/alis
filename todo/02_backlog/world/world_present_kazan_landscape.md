# Present Kazan landscape

**Status:** TECHNICALLY ACCEPTED K1 - FINAL PRODUCT WALKTHROUGH PENDING
**R1:** PASS - superseded in ownership/path scope by the universal-resource decision
**R2:** PASS - universal-resource ownership and authority migration accepted
**Material-core reconciliation:** PASS
**Existing code owner:** ProjectWorld reusable Landscape realization and contracts
**Universal material owner:** ProjectMaterial recipes and generated resources
**World consumer owner:** ProjectWorld semantic binding, assignment, and authentication
**Concrete geography owner:** ProjectWorldData Kazan geography/semantic facts only
**Campaign:** [Kazan presentation research](world_plan_kazan_presentation_campaign.md)
**Implementation prerequisite:** [Shared material generation core](../content/material_generate_assets_from_json.md)
passed post-baseline re-accreditation on 2026-08-28.

## Product outcome

Replace the obvious striped placeholder surface with a coherent terrain treatment that
reads from player height through aerial footage. Preserve the accepted Kazan relief,
collision, generated geography, World Partition behavior, and prototype frame budget.

K1 is implemented and technically accepted after isolated Material Core, Water locality,
current authority, Matrix, and fresh fixed-view visual re-accreditation. The operator's
final packaged walkthrough remains the product judgment; it may accept K1 or explicitly
admit the bounded K2 visual candidate. K2 is not implied by the current candidate.

## Stable routes

- [ProjectWorld ownership](../../../Plugins/World/ProjectWorld/README.md)
- [Territory contract](../../../Plugins/World/ProjectWorld/docs/territory_contract.md)
- [Territory generation](../../../Plugins/World/ProjectWorld/docs/territory_generation.md)
- [World Partition contract](../../../Plugins/World/ProjectWorld/docs/world_partition.md)
- [Plugin rules](../../../docs/architecture/plugin_rules.md)
- [World proof layers](../../../docs/testing/world_pipeline_layers.md)
- [Presentation profile](../../../Plugins/World/ProjectWorldData/Data/Presentation/kazan_representative_v1.json)

## Verified baseline

### Current ALIS behavior

- Installed engine resolution is launcher UE 5.8.1, changelist `56057345`, from
  `scripts/config/ue_path.conf`. Daily work must not switch to a source-built engine.
- The Kazan profile selects
  `/Engine/OpenWorldTemplate/LandscapeMaterial/MI_ProcGrid.MI_ProcGrid` and supplies
  two checker colors, a line color, and `tile_scale=350.0`.
- The installed `MI_ProcGrid` asset exists under the UE 5.8 engine content tree.
- `ProjectWorldPresentationProfile.cpp` and the presentation profile schema currently
  require that exact engine material rather than accepting an owner-provided parent.
- `ProjectWorldPresentationMaterialRealization.cpp` creates or updates only the
  generated terrain MIC checker colors, line color, and tile scale.
- The current generated object identity exists in the wrong long-term owner and is a
  one-time migration input, not the future stable identity:
  `/ProjectWorldData/Generated/Presentation/MI_ProjectWorldTerrain_kazan_representative_v1`.
- Current profile SHA-256 is
  `9A15A0B200F4C86081D831603DA397A215ED086D4DD4574E803FD2CCAB328CD2`.
- Current generated MIC SHA-256 is
  `BC8257FA138D681EA0E415096304CD8C33E89183B81CA95B42DC9BA579A8A4EC`.
- Landscape realization changes the Landscape material pointer only when needed and
  refreshes component material instances when the Landscape is created or that pointer
  differs. A same-path generated MIC update therefore still needs rendered/readback proof;
  code inspection alone does not prove every existing proxy refreshed correctly.
- The accepted runtime profile is `512/1536` on physical RTX 4070, High, 2560x1440,
  D3D12. Stable evidence records 850 generated actors and 210 Landscape proxies.
- The historical dense-centre frame p95 of `15.969 ms` is a risk indicator, not a
  reusable baseline. Every candidate comparison must capture a fresh paired baseline.

### UE 5.8 capability decision

| Capability | Decision | Reason |
|---|---|---|
| Core Landscape and Material Editor | Select | Existing production terrain representation and native material path. |
| Material Instances | Keep | Existing generated MIC is the bounded parameterization and rollback surface. |
| Landscape coordinates plus ordinary normal math | Select | Enough for scale and slope treatment without a second geography authority. |
| Native material Noise | Bounded diagnostic only | Epic warns that Noise can be expensive; admit at most one measured low-frequency mechanism. |
| Baked macro/detail texture | Optional single enhancement | Admit only if the minimal candidate passes performance but is visibly too flat. |
| Runtime Virtual Texturing | Defer | Adds assets, volumes, writers, samples, and another measured need not yet established. |
| Landscape Patch | Reject here | Changes height or weight authority even though its results can be baked. |
| PCG or PCG Biome | Reject here | Population tooling does not own ground shading; PCG Biome is experimental. |
| Landscape Grass Output | Reject here | Would create a second vegetation placement authority. |
| Texture Graph | Optional offline authoring only | Maturity labels differ between local descriptor and Epic documentation; no runtime dependency is justified. |
| Mesh Terrain | Hard reject | Experimental replacement architecture would reopen accepted heightfield ownership. |
| Nanite Landscape | Preserve and authenticate | It is already part of the frozen Kazan geometry policy, not a new material candidate. |

Official references:

- [Landscape materials](https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-materials-in-unreal-engine)
- [Material scalability](https://dev.epicgames.com/documentation/unreal-engine/scalability-reference-for-unreal-engine)
- [Nanite Landscape](https://dev.epicgames.com/documentation/unreal-engine/using-nanite-with-landscapes-in-unreal-engine)
- [Landscape Patch](https://dev.epicgames.com/documentation/unreal-engine/landscape-patch-system)
- [Texture Graph](https://dev.epicgames.com/documentation/en-us/unreal-engine/getting-started-with-texture-graph-in-unreal-engine)

## Reviewer decision audit

### Accepted

- Treat the periodic engine grid material as the leading visual diagnosis and do not
  reopen terrain height, relief, proxies, collision, or World Partition.
- Use core UE Landscape material capability through ProjectMaterial resources and the
  existing ProjectWorld consumer. Do not create a new plugin.
- Keep the first material deliberately small and automatic: color, roughness, slope,
  scale, and one bounded macro-breakup mechanism at most.
- Generate the replacement through the accepted shared material transaction, not the
  current ProjectWorld MIC helper. R2 intentionally changes the legacy WorldData identity.
- Use a candidate funnel with paired visual and packaged performance evidence instead
  of an open-ended material feature matrix.
- Treat City17 only as read-only evidence for useful rendering heuristics.

### Corrected or strengthened

1. The debug material is the leading diagnosis, not a closed root cause. A transient
   flat/native material diagnostic from identical views must show the grid disappears
   while terrain relief remains before implementation begins.
2. The fingerprint-locality defect is already proven, not conditional. The current
   terrain-only material realization source participates in presentation, Landscape,
   road, and building fingerprints, and its test currently expects all four to move.
   Locality repair is a required precondition.
3. `Nanite Landscape: defer` is stale relative to the stable Kazan contract. Preserve
   the accepted Nanite policy without enabling, disabling, rebuilding, or tuning it as
   part of this material candidate. Authenticate the serialized state and built-data
   freshness before acceptance; stop if runtime state contradicts the contract.
4. The profile hash and source fingerprint do not authenticate material bytes. The
   presentation consumer receipt must include the accepted ProjectMaterial identity and
   digest without making Landscape geometry, roads, or buildings dirty.
5. UE provides the material expressions needed for slope classification, but no special
   native Landscape slope-authority system was verified. Use ordinary normal math and
   avoid claiming a capability the engine does not expose.
6. The exact `0.70 ms` historical headroom is not current acceptance evidence. Candidate
   K0 and K1 must be measured together under the same packaged conditions.
7. This review is an R1 `PATCH`, not an R1 `PASS`. Apply its corrections and request the
   same reviewer recheck; do not mark research complete from a conditional expected PASS.
8. The previously selected authored-parent plus ProjectWorld-generated-MIC path is
   superseded by the shared material-core decision. Both parent and MIC are generated by
   `ProjectMaterialEditor`; ProjectWorld only consumes and authenticates their final path
   and digest.

### R2 universal-resource correction

The R1 same-path WorldData design is superseded. Terrain is a reusable concept, not a
Kazan entity. ProjectMaterial owns the universal parent/default MIC, ProjectWorld owns the
semantic binding and assignment, and ProjectWorldData owns no terrain material recipe,
path, graph parameter, generated MIC, or material manifest.

This correction also changes the locality claim. The first cutover must update whatever
Landscape/map packages persist the old MIC reference, so those package bytes cannot be
promised stable. It must prove that only the material reference changed and that height,
topology, components/proxies, Nanite state, and collision semantics stayed unchanged.
After cutover, same-path ProjectMaterial recipe updates must write only ProjectMaterial
assets/manifests and must not dirty or save World geometry packages.

## Selected architecture

```text
ProjectMaterial universal recipes
  Data/Materials/Terrain/
    M_ProjectTerrain.material.json
    MI_ProjectTerrain_Default.material.json
        |
ProjectMaterialEditor compiler + ProjectMaterial manifest
        |
ProjectMaterial universal generated resources
  /ProjectMaterial/Generated/Terrain/M_ProjectTerrain
  /ProjectMaterial/Generated/Terrain/MI_ProjectTerrain_Default
        |
ProjectWorld closed binding: terrain.default -> MI_ProjectTerrain_Default
        |
ProjectWorld Landscape assignment/authentication
        |
Existing ALandscape and Landscape proxies

ProjectWorldData -> heights, land-cover/semantic facts, geography only
```

`ProjectMaterialEditor` owns graph/MIC generation, compile validation, resource manifests,
and exact material rollback. ProjectWorld owns the reusable `terrain.default` binding,
final asset authentication, Landscape assignment/refresh, reference migration, and
evidence. It must not depend on the Editor compiler module or retain the parallel terrain
MIC generator. ProjectWorldData's presentation profile removes `materials.terrain` and
`terrain_parameters`; remaining legacy presentation fields are migrated only by their own
accepted concerns.

The universal parent and default MIC are disposable generated content under
ProjectMaterial. ProjectWorld consumes only the final stable identity and accepted digest.
Kazan supplies no material graph or parameter JSON. Do not add a generic parameter map or
a runtime dependency on the Editor compiler.

### Minimal material contract

The first candidate is opaque and Default Lit, using:

- Landscape coordinates for stable world scale;
- world or vertex normal math for a bounded flat-to-slope blend;
- restrained flat-ground and exposed-slope colors;
- bounded roughness variation;
- no more than one low-frequency macro-breakup mechanism.

Do not add painted weight layers, road marks, foliage output, PCG, RVT, Landscape Patch,
displacement, runtime time logic, custom HLSL, or texture stacks. If native Noise exceeds
the instruction budget, prefer one baked single-channel macro texture; do not keep both.

### Locality and authenticated-input precondition

Add a red regression before the material change:

```text
same-path ProjectMaterial terrain recipe/output bytes change
  -> ProjectMaterial manifest moves
  -> presentation validation records the new accepted digest
  -> zero World dirty units and zero World package writes
  -> project_landscape:v1 stays stable
  -> project_road_mesh:v1 stays stable
  -> project_building_massing:v1 stays stable
  -> every other owner stays stable

ProjectWorld terrain.default binding path changes
  -> presentation assignment becomes dirty
  -> geography producer identities stay stable
```

Repair only the proven terrain seam:

- remove `ProjectWorldPresentationMaterialRealization` from every producer source set and
  delete that parallel MIC-generation owner after the shared material path replaces it;
- keep ProjectMaterial compiler/recipe/manifest identity in the independent material
  resource authority. No ProjectMaterial recipe, compiler source, or output digest enters
  a World geography producer fingerprint;
- bind ProjectWorld's `terrain.default` consumer to the final MIC identity. The accepted
  digest belongs in validation/operation evidence, not a geometry dirty key;
- keep one external JSON profile, but split its executable validation/schema identity into
  a stable common envelope plus terrain, road, and building concern contracts;
- keep the common envelope limited to schema/version/profile identity and section routing.
  Concern field parsing and validation are not common merely because the JSON is shared;
- compose the root schema from independently fingerprintable concern sub-schemas. Other
  concern sections remain outside road/building producer identities and are not redesigned
  by this Landscape slice;
- bind producer sources as follows:

```text
presentation:v1
  = ProjectWorld terrain.default binding identity + assignment contract

project_landscape:v1
  = common geography envelope + Landscape geometry contract

project_road_mesh:v1
  = common envelope + road contract + road realization sources

project_building_massing:v1
  = common envelope + building contract + building realization sources
```

- resolve `terrain.default`, hash the material package, and authenticate it against the
  ProjectMaterial manifest before any assignment mutation;
- record material identity/digest and the persisted Landscape pointer in the presentation
  receipt. Keep the digest out of Landscape geometry, road, and building dirty decisions;
- remove `materials.terrain` and `terrain_parameters` from the Kazan presentation profile,
  parser, and schema in the same clean migration. Do not retain compatibility fields;
- extend the presentation consumer evidence rather than adding a concrete material path
  or package digest to generic geography source fingerprints;
- use the existing fingerprint-v2 metadata migration path only for unchanged package bytes
  and producer identities. Unaffected road/building scopes remain exact; never regenerate
  their geometry for this ownership correction.

The one-time path cutover is a separate bounded reference-plus-authority migration, not a
metadata-only migration. Preflight must discover every Landscape/map package that persists
the old MIC pointer and the active manifest scope that owns each package. The transaction:

1. snapshots those packages, the old immutable manifests, and `active_set.json`;
2. saves only the proven material references and authenticates unchanged serialized
   height, topology, components/proxies, Nanite state, collision, and semantic hashes;
3. writes a new immutable manifest generation for only each affected owner scope, with new
   package SHA-256 values and unchanged geometry semantic identities;
4. links the manifest through `accepted_operation_id` to an operation receipt that records
   the reference-only migration reason;
5. atomically promotes all affected entries in `active_set.json` together and runs the
   read-only authority audit; and
6. retires the old WorldData MIC only after the promoted audit and a zero-reference scan.

Old manifests remain immutable provenance. A failure before or after promotion restores
the exact reference packages and prior `active_set.json`, then re-runs the authority audit.
After this one-time cutover, same-path ProjectMaterial changes advance only the material
manifest and cause zero World manifest advancement or World package writes.

If the smallest repair propagates outside presentation and Landscape ownership, stop and
return to architecture review rather than normalize unrelated churn.

## Candidate funnel

### D0 - diagnosis only

Apply a transient flat/native test material in an isolated Editor fixture or live session.
Capture the same authenticated ground, oblique, dense-centre, and aerial views. Do not save
the production map. D0 passes only if periodic grid/moire disappears and silhouette, relief,
collision, roads, buildings, and streaming remain unchanged.

### K0 - paired control

Capture current `MI_ProcGrid` before the parser/schema clean cutover. Record fresh images,
performance, profile/material hashes, package identity, and executable hash. K0 is evidence
and the exact `-1` rollback state; the new parser does not retain support for its obsolete
checker contract.

### K1 - primary product candidate

Small automatic Landscape parent/MIC generated through the accepted material core as
described above. Compare one bounded macro mechanism,
not a feature matrix. Material stats and Shader Complexity are diagnostic; physical packaged
RTX 4070 timing is authority.

### K2 - conditional visual enhancement

Admit exactly one baked macro/detail texture only if K1 passes every correctness and
performance gate but the operator rejects it as visibly too flat. K2 replaces the K1 macro
mechanism; it does not accumulate another stack. If K1 reads well, K2 does not exist.

Run dense-centre comparison first. Only the surviving Landscape/material candidate receives
full centre-to-edge-to-centre traversal and Shipping menu-to-Kazan correctness. No survivor
means exact `-1` transaction/file restoration and return of the concern to research.

## Change locality

### Expected future changes

- ProjectMaterial universal terrain parent/MIC recipes, generated resources, accepted
  manifest, and only the selected archetype support in the shared compiler.
- Kazan presentation profile/schema/parser removal of terrain material implementation
  fields, plus concern separation required for independent legacy road/building migration.
- ProjectWorld terrain material consumption/assignment and focused refresh tests, plus
  deletion of `ProjectWorldPresentationMaterialRealization` as a second MIC generator.
- the normal `ProjectWorld.uplugin` content dependency on `ProjectMaterial` when required
  by packaged activation/cook proof; zero ProjectWorld `Build.cs` dependency on
  ProjectMaterial or ProjectMaterialEditor and zero material runtime-service dependency.
- ProjectWorld `terrain.default` binding and presentation evidence separated from every
  geography producer fingerprint.
- A one-time reference migration for only packages proven to persist the old MIC pointer.
- Retirement of the old ProjectWorldData terrain MIC after zero-reference proof.
- Stable ProjectWorld docs only after an implementation is accepted.

### Must remain untouched

- canonical terrain height, extent, relief, land cover, and source data;
- Landscape/proxy layout, edit layers, collision, component count, and topology;
- accepted `512/1536` runtime profile and World Partition policy;
- water, roads, vegetation, buildings, gameplay objects, loading, and menu contracts;
- generated road/building/vegetation/water packages and their artifact hashes;
- project rendering settings, Nanite policy, Lumen policy, and unrelated plugins;
- City17 assets and architecture.
- the accepted ProjectMaterial compiler implementation; if Landscape exposes a missing
  compiler capability, stop and return that owner to review instead of patching it here.

## Invariants and proof traceability

| Invariant | First proof | Acceptance proof |
|---|---|---|
| Grid is material-caused | D0 identical-view capture | K0/K1 rendered comparison |
| Geography is unchanged | canonical/manifest hashes | topology, collision, and proxy readback |
| Only owning producers move | red fingerprint-locality test | replacement test plus manifest diff |
| Parent/MIC use shared compiler | material manifest and no-local-generator tests | generated asset/manifest readback |
| Material bytes are authenticated | digest-change unit test | input receipt names final MIC, parent/dependencies, and digests |
| Universal MIC identity is stable after cutover | derived-path assertion | package/object readback after realization |
| WorldData owns no terrain presentation | schema/path rejection tests | zero terrain recipe/path/MIC/manifest inventory |
| Reference migration changes no geography semantics | before/after semantic census | topology, height, proxy, Nanite, and collision parity |
| World authority follows changed package bytes | red stale-active-manifest test | new immutable owner manifests, atomic active-set promotion, and authority audit |
| Later material updates write no World package | same-path material change test | package hash/mtime census outside ProjectMaterial |
| Nanite policy is preserved | serialized flag and freshness readback | packaged proxy/state evidence |
| Visual result is coherent | fixed ground/oblique/aerial captures | operator inspection of survivor |
| Runtime gate survives | paired dense-centre K0/K1 | full packaged traversal on survivor |
| Product route survives | focused native Landscape tests | Shipping menu -> Kazan proof |
| Rollback is bounded | exact `-1` snapshot/restore test | old code/profile/recipe/parent/MIC/manifest evidence |

## Ordered implementation

1. [x] Confirm the shared material core is accepted and archived. Do not begin Landscape
       implementation or extend the compiler from this concern.
2. [x] Capture D0 without saving production state; close the placeholder-grid diagnosis.
3. [x] Before clean cutover, capture the fresh K0 evidence and exact `-1` code/schema,
       profile, old WorldData MIC, future ProjectMaterial recipe/manifest/output presence
       or absence, persisted Landscape reference packages, executable, and receipts.
4. [x] Add failing tests for source locality, ProjectMaterial-only ownership, stable new
       universal MIC identity, WorldData terrain-field rejection, absence of a
       ProjectWorld-side compiler, and same-path material updates causing zero World writes.
5. [x] Reconcile producer source sets to actual consumers. The existing presentation
       parser remains cohesive; no speculative physical source split was required.
6. [x] Metadata-migrate only unchanged package/producer identities and prove unaffected
       road/building scopes exact; do not regenerate geometry or use metadata-only
       migration for any package whose serialized reference changes.
7. [x] Add focused failure tests for invalid semantic binding, missing/corrupt final MIC,
       parent/dependency digest change, and non-owner data/fingerprint stability.
8. [x] Add the closed universal parent/MIC recipes under
       `ProjectMaterial/Data/Materials/Terrain/`; generate
       `/ProjectMaterial/Generated/Terrain/M_ProjectTerrain` and
       `/ProjectMaterial/Generated/Terrain/MI_ProjectTerrain_Default` transactionally.
9. [x] Migrate the terrain profile contract/consumer in one pass; remove obsolete
       checker-only fields and `ProjectWorldPresentationMaterialRealization` code/tests
       rather than keeping compatibility baggage.
10. [x] Add the closed ProjectWorld `terrain.default` binding. Authenticate the material
        manifest/final MIC before mutation and record its digest in presentation evidence,
        never a geography dirty key.
11. [x] Run the bounded reference-plus-authority migration from the old WorldData MIC.
        Discover exact packages/owner scopes, prove semantic parity, write new immutable
        affected-owner manifests, atomically promote `active_set.json`, pass the authority
        audit, and only then prove zero old referencers before transactional retirement.
12. [x] Run focused material-consumption/native Landscape tests and R2 review.
13. [x] Run paired K0/K1 dense-centre performance and fixed-view visual evidence.
14. [x] Do not admit K2 automatically. K1 removed the dominant debug grid; only an explicit
        operator visual rejection may reopen the bounded K2 condition and material-core review.
15. [x] Run one common Check plus all planner-selected P0, representative, and territory
        Matrices as the slice-exit L2 gate.
16. [x] Promote only the survivor through persistent realization, packaged traversal, and
        Shipping product-route proof; stop at any unexpected cross-owner propagation.
17. [x] Update stable owner docs, clean disposable evidence, and record rollback receipts.

## Technical acceptance - 2026-08-27

- Material generation operation: `d4f54ef196104e299156f0f68bfffdeb`.
- Accepted material manifest SHA-256:
  `b5630b00aef3c9c2c3e4de5a26abd8b5f1fb7f37c3b2030174c51585e084a9f4`.
- Final terrain MIC package SHA-256:
  `44746ded15ca1ad793d429e89feead907ddac376052be86679021893173d4684`.
- Old WorldData terrain MIC: zero binary references and zero Asset Registry/MCP
  referencers before transactional retirement.
- Final authority audit:
  `Saved/Validation/WorldAuthority/landscape-k1-final-authority.json` - accepted,
  10 scopes and 1,371 byte-identical owned artifacts.
- Slice-exit common Check:
  `Saved/Validation/WorldPipeline/check-20260827T020359Z/result.json` - accepted.
- P0 Matrix: `run-20260827T020740Z` - accepted.
- Representative Matrix: `run-20260827T021450Z` - accepted.
- Territory Matrix: `run-20260827T022254Z` - accepted.
- Packaged playable-tour operation: `00aab7d6e02f4f3cbaa40b7add088daf`.
- Physical RTX 4070, D3D12, High, 2560x1440: Frame p95 `14.198 ms`,
  GPU p95 `10.467 ms`, zero streaming failures, normal Development and Shipping exits.
- K0 -> K1 Frame p95 delta: `+0.057 ms`; GPU p95 delta: `+0.103 ms`.
- Operator package: `Saved/PackageRelease/KazanPlayableTour/Current`.
- Visual result: K1 removes the dominant alternating green debug grid. The bounded
  slope-only V1 material remains intentionally simple until the operator judges it.
- During L2, an honest P0 rejection exposed compile-receipt provenance leaking into D3.
  The focused red/green regression now excludes only `ProjectWorld.Input=` from product
  semantics; all real semantic tags remain authoritative. Evidence-only source changes
  also move zero generated-byte producer fingerprints.

## Post-baseline F4 re-accreditation - 2026-08-28

- Current Terrain parent SHA-256:
  `bb3309a6d825d9de87865ce08c61491ad9526a0222abe4ea1fcb0ca37d89a533`.
- Current Terrain MIC SHA-256:
  `cedada1d9470825db0464667c9cb5cac6a05944c1ae113a42504633a34f48218`.
- Common Check: `Saved/Validation/WorldPipeline/check-20260828T095846Z/result.json`.
- P0 Matrix: `Saved/Validation/WorldPipeline/run-20260828T100409Z/result.json`.
- Representative Matrix:
  `Saved/Validation/WorldPipeline/run-20260828T101107Z/result.json`.
- Territory Matrix: `Saved/Validation/WorldPipeline/run-20260828T101909Z/result.json`.
- Metadata-only current-fingerprint migration:
  `Saved/Validation/WorldAuthority/f4-current-fingerprint-migration.json`.
- Final current authority audit:
  `Saved/Validation/WorldAuthority/f4-post-migration-authority.json` - accepted,
  10 scopes and 1,373 byte-identical artifacts.
- Fresh nine-view visual evidence:
  `Saved/Validation/WorldVisual/f4-landscape-k1-20260828T104840Z/capture.json`.
  The exact same pre-K1 viewpoints show the former debug grid; current K1 removes it
  while retaining Terrain relief and aligned Road/Building geometry.
- The Water generation 13 -> 14 binary difference was reproduced as independent clean
  Unreal package metadata variability with unchanged material semantic identity. Exact
  no-op remains the primary invariant and wrote zero artifacts.

## Exact proof surfaces

- Fingerprint locality: `scripts/ue/world/test/generator_fingerprint.Tests.ps1`.
- Profile contract: exact UE test
  `Project.World.Realization.Presentation.ProfileContract` through
  `scripts/ue/test/unit/iterate.ps1 -TestFilter <exact-name>`.
- Native Landscape topology:
  `Project.World.Realization.NativeTwin.LandscapePartitionAndEditLayers`.
- Native proxy identity:
  `Project.World.Realization.NativeTwin.LandscapeProxySemanticIdentity`.
- Existing scoped realization Check and affected Matrix only at slice exit, following the
  linked proof-layer SOT. Broad filters are not a discovery loop.

Performance capture conditions are physical RTX 4070, High, 2560x1440, D3D12, packaged
Development for paired diagnosis and Shipping for final product-route correctness. Record:

- Frame p95, p99, and max;
- Game, Render, and GPU p95;
- process and GPU memory;
- shader instruction/permutation diagnostics and Shader Complexity capture;
- package/cook size delta;
- streaming failures, wrong-cell events, and centre-edge-centre reload correctness.

K0 and K1 must use the same physical machine, driver, settings, route, build configuration,
and capture method. Their binaries may differ because K1 follows a clean parser/schema
cutover; record the executable SHA-256 in both receipts and never preserve obsolete parser
compatibility merely to force a same-binary comparison.

Hard candidate requirements remain Frame p95 `<= 16.67 ms` and zero streaming failures.
Do not infer current headroom from an older session.

## Rollback and temporary-file ownership

Before schema/parser/material/reference migration, the owners capture fresh K0 evidence and
one exact `-1` snapshot covering changed code/schema files, profile bytes, the old WorldData
MIC, new ProjectMaterial recipe/manifest/output presence or bytes, every identified
Landscape/map reference package, affected immutable World manifests, `active_set.json`,
World metadata, and executable identity. K0 is the
rollback state, not a compatibility path in the new parser.

If K1 succeeds, obsolete checker fields, validation, and tests stay deleted. If K1 fails,
rollback reverses the implementation patch and restores the exact old profile, WorldData
MIC, ProjectMaterial recipe/output/manifest state, persisted reference packages, and World
manifest/`active_set.json` authority from `-1`; it then passes the read-only authority audit
before old K0 can execute again. Never teach the new parser the
obsolete checker contract merely to regenerate K0. Rollback does not hand-edit a saved map,
rebuild terrain height, binary-move material packages between identities, or use destructive
Git operations.

All candidate receipts live under:

`tmp/world/presentation/landscape/<run-id>/`

The owning script must remove abandoned candidate packages, shader dumps, screenshots, and
other disposable intermediates. It retains only the current accepted receipt and one `-1`
rollback snapshot per active transaction until closure, then removes obsolete rollback data.
Nothing under `tmp/` may become a committed test or documentation input.

## Stable documentation promotion

Accepted implementation must move the durable boundary into stable owners:

- `Plugins/Resources/ProjectMaterial/README.md` and its material-generation docs own
  universal terrain recipe/resource and compiler contracts;
- `Plugins/World/ProjectWorld/docs/architecture_overview.md` and
  `territory_generation.md` own semantic material binding, assignment, authentication,
  and separation from geography dirty identity;
- `Plugins/World/ProjectWorldData/README.md` and `Data/README.md` state that WorldData owns
  geography/sourced semantic facts and no terrain material implementation;
- Landscape/presentation schema documentation records removal of legacy terrain material
  fields and the clean no-compatibility cutover;
- the existing World pitfall owner records the one-time reference-migration/no-geometry-
  rebuild rule if implementation exposes a reusable trap.
- `docs/architecture/plugin_rules.md` and the C4 model record ProjectMaterial as a
  lower-level Resources content leaf consumed by soft identity/content dependency, never
  through a ProjectWorld `Build.cs` or runtime-service dependency;
- World authority docs record that serialized reference changes require new immutable
  affected-owner manifests plus atomic `active_set.json` promotion and authority audit.

Stable docs, code, tests, and schemas must not reference this todo. The todo remains open
until these durable updates and their router links are verified.

## Non-goals

- terrain height regeneration, smoothing, or reconstruction;
- a new Landscape plugin or runtime material controller;
- a manually authored Landscape parent or a ProjectWorld-owned parent/MIC generator;
- a Kazan/ProjectWorldData terrain material recipe, path, MIC, or material manifest;
- manual layer painting or new land-cover semantics;
- Landscape Patch, PCG, PCG Biome, Landscape Grass Output, RVT, or Mesh Terrain;
- a Nanite, Lumen, World Partition, or project-CVar experiment;
- heavy Noise stacks, custom HLSL, or third-party auto-landscape frameworks;
- copying City17 material architecture;
- implementation before all concern research is complete and priority is selected.

## Remaining authentication

Live Editor MCP tools were unavailable during this agent verification. The stable SOT requires
Nanite Landscape, but current serialized `bEnableNanite`, built-data freshness, and all 210
proxy states were therefore not authenticated live. This is an implementation-time preflight,
not permission to change the frozen Nanite policy. Any contradiction stops the slice.

## Research close-out

- [x] Reviewer produced a landscape-only comprehensive proposal and UE 5.8 option comparison.
- [x] Agent verified critical claims against installed UE 5.8.1, current ALIS owners,
      frozen authority, generator fingerprints, manifests, and packaged target.
- [x] Recorded the selected owner and smallest design, corrections, rejected alternatives,
      ordered implementation actions, test-first proof, performance evidence, and rollback.
- [x] Same reviewer rechecked this completed packet and returned R1 PASS.
- [x] Operator superseded R1's WorldData ownership with universal ProjectMaterial
      ownership and ProjectWorld semantic binding.
- [x] Same reviewer rechecked the R2 ownership, locality, dependency, and authority-
      migration correction and returned PASS.
- [x] Selected K1 after the seven-concern comparison and accepted ProjectMaterial core.
- [x] Implemented K1 as an integrated candidate and routed this todo back to backlog.
- [ ] Re-accredit Water locality and Material Core, then perform the final operator
      visual walkthrough.
