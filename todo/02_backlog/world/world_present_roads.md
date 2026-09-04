# Present World roads

**Status:** RESEARCH COMPLETE / IMPLEMENTATION NOT SELECTED
**Scope:** Universal generated-World road presentation; Kazan is the first measured
fixture, not the owner or a runtime branch.
**R1:** PASS - 2026-08-26
**R1 candidate:** one universal textureless asphalt material on frozen road geometry
**Universal material owner:** ProjectMaterial recipes, generated resources, and manifest
**World geometry owner:** ProjectWorld `project_road_mesh:v1` realization
**World presentation consumer:** ProjectWorld `presentation:v1` Road binding,
authentication, reference assignment, and migration
**Concrete data owner:** Each territory data owner; ProjectWorldData owns Kazan road facts
and geometry policy only
**Implementation prerequisite:** [Shared material generation core](../../01_done/content/material_generate_assets_from_json.md)
must be implemented and accepted first. Research may continue now.

## Research result

The smallest credible first road improvement is material-only:

```text
ProjectMaterial universal Road parent/default MIC
        ->
ProjectWorld closed road.default binding
        ->
existing cell-owned territory road meshes
```

Keep the accepted road graph, classes, widths, tessellation, terrain drape, topology,
collision, Nanite, actor ownership, and World Partition behavior unchanged. Do not add
markings, decals, splines, PCG, shoulders, dressing, or class-specific geometry in K1.

This is a researched candidate, not an implementation selection or visual acceptance.

## Product outcome

Replace the obvious debug road surface with believable low-cost asphalt while retaining
the road network already accepted as prototype-good. Ground, oblique, junction, boundary,
and aerial views must improve without hiding a geometry regression or consuming the narrow
physical RTX 4070 1440p/60 budget.

## Routes

### Stable owners and contracts

- [ProjectMaterial owner](../../../Plugins/Resources/ProjectMaterial/README.md)
- [ProjectWorld owner](../../../Plugins/World/ProjectWorld/README.md)
- [World realization SOT](../../../Plugins/World/ProjectWorld/docs/territory_generation.md)
- [World generation pitfalls](../../../Plugins/World/ProjectWorld/docs/pitfalls.md)
- [Plugin dependency rules](../../../docs/architecture/plugin_rules.md)
- [Testing strategy](../../../docs/agents/canonical.md#7-testing-strategy)

### Kazan first-fixture evidence

- [Realization profile](../../../Plugins/World/ProjectWorldData/Data/Profiles/Realization/kazan_territory_v1.realization.json)
- [Presentation profile](../../../Plugins/World/ProjectWorldData/Data/Presentation/kazan_representative_v1.json)

## Verified current state

### Accepted road authority

- `project_road_mesh:v1` consumes canonical road features and cell-local
  `road_fragment` representations.
- The accepted realization profile selects `primary`, `primary_link`, `secondary`,
  `secondary_link`, `tertiary`, `tertiary_link`, `residential`, `unclassified`, and
  `living_street`.
- Its frozen geometry policy is `0.15 m` surface offset, `7.5 m` maximum segment length,
  Nanite enabled, complex-as-simple collision, `terrain_drape`, and
  `overlap_same_owner` intersections.
- Road width comes from each canonical feature. Representation parts are clipped into
  owning cells; one spatially loaded StaticMesh actor and asset are realized per cell
  containing selected roads.
- Road actors have HLOD disabled and do not affect navigation.
- Slice 4 selected runtime profile `512/1536` and accepted the existing roads during the
  operator prototype walkthrough. Presentation work does not reopen that geometry.

### Persisted active census

The researched active road scope is generation 10. Its manifest contains:

```text
161 road StaticMesh assets
161 external road actors
322 total artifacts
210 canonical cell input records
1 terrain dependency input
```

This census is diagnostic only. Implementation preflight must resolve the current active
road manifest and recount its artifacts; it must not hard-code `161`.

### Current mesh representation

`ProjectWorldRoadRealization.cpp` currently produces:

- one polygon group and one material slot named `Road`;
- one UV channel using cell-local XY position scaled by `0.001`;
- constant dark vertex color `(0.08, 0.08, 0.08, 1.0)`;
- `+Z` normals and world-`+X` tangents;
- potentially multiple road ribbons in one cell mesh.

The current presentation profile assigns Engine
`/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial`. The only verified
visual diagnosis from current code is a uniform dark debug-style ribbon. No fresh paired
capture in this research proves shimmer, z-fighting, floating edges, gaps, or orientation
defects. R0 must authenticate those observations before K1.

### Current material and locality coupling

- The presentation schema/parser requires one `materials.road` soft path and currently
  rejects non-Engine road materials.
- ProjectWorld resolves that path and passes the material into road realization.
- `ProjectWorldRoadRealization::Apply` first skips clean cells. Even when a cell reaches
  the operation, it skips again if the existing actor/mesh semantic identity matches.
  Both skips occur before the material slot is reassigned.
- Therefore a geometry-current road mesh can retain a stale material reference.
- The road cell input hash already contains road facts, terrain artifact identity, and
  normalized layer contract. It does not contain the presentation-profile or material
  digest.
- However, the road producer fingerprint currently includes shared presentation parser,
  schema, and material-realization sources, and the active road manifest envelope records
  `presentation_profile_sha256`. Those are stale evidence/locality couplings.
- `New-ProjectWorldLayerInputIdentity` already removes runtime-profile identity from
  generated layer authority, but does not remove presentation-profile identity.
- Manifest semantic no-op comparison currently ignores the input envelope. A profile-only
  value change does not dirty road cells or advance an otherwise unchanged road manifest,
  but the active manifest can retain stale presentation-envelope evidence.

### Dependency shape

- ProjectWorld currently has no ProjectMaterial plugin declaration and no Build.cs
  dependency on ProjectMaterial.
- The accepted resource contract requires content consumption through a stable soft path.
- The first World consumer must add a normal ProjectWorld `.uplugin` dependency on
  ProjectMaterial if packaging proof requires it, with zero ProjectMaterial or
  ProjectMaterialEditor Build.cs dependency.
- ProjectMaterialEditor remains the Editor-only compiler. ProjectWorld never compiles a
  material graph.

### Installed engine evidence

The configured daily engine is launcher UE 5.8.1, changelist 56057345. Opaque Default Lit
materials, material instances, decals, Landscape Splines, and PCG are available engine
surfaces. Availability is not selection. K1 needs only standard material behavior and does
not justify new runtime systems or geometry authorities.

## Reviewer feedback audit

### Accepted

- Correct the owner split to ProjectMaterial universal resources, ProjectWorld consumption,
  and ProjectWorldData road facts.
- Use a material-first K1 and keep road geometry frozen.
- Start with a textureless asphalt parent/default MIC because current UV/tangent data is
  not road-local and is unnecessary for the smallest proof.
- Reject markings, decals, Landscape Splines, SplineMesh roads, PCG, shoulders, dressing,
  class-material variants, RVT, and new geometry attributes for K1.
- Add an independently checkable material-reference seam that cannot be bypassed by the
  current geometry short-circuits; do not force a mesh rebuild to apply a reference.
- Treat the first reference cutover as a real road-package migration and use the existing
  immutable manifest plus atomic active-set authority model.
- Use fresh paired visuals and packaged physical RTX 4070 measurements. Historical Slice 4
  values are risk indicators only.
- Keep `road.default` binding, authentication, and reference assignment in
  `presentation:v1`; `project_road_mesh:v1` fingerprints geometry-generation sources only.
- Permit clean R0/K1 binaries to differ, record both executable hashes, and retain no
  compatibility field merely to force a same-binary comparison.

### Corrected or narrowed

1. The reviewer describes presentation identity as part of road geometry dirty identity.
   That is not accurate at current HEAD. Per-cell dirty calculation is already based on
   canonical road/terrain inputs and the layer contract. The stale coupling is in the
   producer fingerprint and manifest input-evidence envelope. Implementation must split
   those concerns without rewriting the working dirty-plan semantics.
2. `same mesh packages` means the same package/object identities, not byte identity. The
   one-time material-reference cutover necessarily changes every discovered road mesh
   package that serializes the old pointer. Subsequent same-path material updates must not
   write those packages.
3. Zero added actors, sections, and geometry-driven draw calls are structural K1 targets.
   Total renderer draw/pass cost still requires measurement; a new material can change
   BasePass/shader cost without changing geometry structure.
4. Zero texture memory means K1 has no texture dependencies. It is verified from the
   generated graph/manifest and runtime diagnostics, not assumed from the recipe text.
5. `road_surface_basic_v1` is a proposed closed ProjectMaterial archetype. The material-core
   todo still admits exactly one first implemented archetype after campaign prioritization.
   Road research does not silently expand that executable compiler contract.
6. Zero-reference proof is road-owner scoped. The old Engine material is not deleted and
   cannot require a project-wide zero-reference result because the current building profile
   also uses it. Only the Road profile field and road mesh references must reach zero.
7. Removing the full presentation hash from road authority does not mean deleting the
   manifest field at current schema version. Manifest validation requires the field and
   accepts `none`. Generated Road layer identity must write `presentation_profile_sha256`
   as `none`; literal field removal requires a separately reviewed versioned schema migration.

### Refuted

- A fresh visual defect list cannot be inferred from old screenshots or mesh structure.
  Only the debug material is verified weak today; R0 decides whether any geometry symptom
  exists.
- Native DBuffer, splines, or PCG availability is not evidence that Kazan needs them.
- The old City17 road backlog is not Kazan authority or acceptance evidence.
- Marking every road dirty and calling `BuildFromMeshDescriptions()` is not an acceptable
  material migration. It couples presentation to geometry regeneration and increases risk.

## Selected K1 contract

ProjectMaterial owns one reusable Road family:

```text
family: surface_opaque
archetype: road_surface_basic_v1
parent: /ProjectMaterial/Generated/Road/M_ProjectRoad.M_ProjectRoad
default instance: /ProjectMaterial/Generated/Road/MI_ProjectRoad_Default.MI_ProjectRoad_Default
```

K1 is:

- Surface domain, Opaque, Default Lit;
- textureless and static at runtime;
- dark neutral asphalt base color;
- high scalar roughness;
- non-metallic with bounded scalar specular;
- no opacity, emissive, normal texture, procedural noise, WPO, PDO, displacement,
  wetness, puddles, dirt, runtime MPC, or dynamic instance service;
- no use of current vertex color, UV0, or tangent orientation.

Exact numeric defaults are recipe data selected by the material owner and captured in the
candidate receipt. The schema admits only the parameters required by this graph. It does
not become an arbitrary material DSL.

K1 does not create hierarchy. Existing canonical widths already provide geometric
hierarchy. K1 tests whether removing the debug surface lets that accepted hierarchy read.
If it does not, stop and return to focused research; do not automatically add K2.

## Ownership and dependency decision

```text
ProjectWorldData
  road graph, class, width, clipped representations, sourced semantic facts
        ->
ProjectWorld project_road_mesh:v1 geometry producer
  cell mesh geometry, collision, Nanite, World Partition ownership only
        |
        v
ProjectWorld presentation:v1 Road consumer
  closed road.default binding, authentication, reference assignment/migration
        ^
        |
ProjectMaterial
  universal Road recipe, parent, default MIC, compiler contract, manifest
```

ProjectWorldData must end with no road material path, recipe, graph parameter, MIC identity,
or material manifest. ProjectWorld's `presentation:v1` consumer owns the closed
`road.default` binding and stable final MIC identity. ProjectMaterial has no knowledge of
Kazan, road cells, or ProjectWorld.

## Reference and authority migration

### Normal consumer seam

Material assignment is a separate `presentation:v1` consumer pass, not a branch inside the
road geometry producer:

```text
authenticate accepted road.default first
-> run project_road_mesh:v1 geometry work or geometry no-op
-> always run the Road presentation consumer for expected road outputs
   -> material slot already road.default: no write
   -> material slot differs: replace Road slot and save mesh only
```

Authentication must pass before any mutation. The independent consumer pass must execute
regardless of both current geometry early-outs. A reference-only update must not call
`BuildFromMeshDescriptions()`, save the external actor/map, or mutate collision, Nanite,
bounds, navigation, transforms, tags, or World Partition state.

### One-time cutover

Within the existing recoverable ProjectWorld operation/manifest transaction:

1. Resolve the active road scope and discover all road mesh, actor, and map packages.
2. Capture one exact owner-scoped `-1` state and hashes for expected-untouched packages.
3. Generate, reload, and authenticate the accepted ProjectMaterial Road resources.
4. Fail before World mutation unless every current road mesh has exactly one `Road` slot
   and matches its accepted geometry semantic identity.
5. Replace only the old material reference on every discovered road mesh; never rebuild
   MeshDescription.
6. Reload and prove exact semantic parity for positions, indices, UV0, tangents, normals,
   colors, sections, triangle count, bounds, Nanite, BodySetup/collision, and navigation.
7. Prove all external road actor packages and map packages stayed byte-identical.
8. Capture new road asset SHA-256 values with unchanged geometry semantic outputs in one
   new immutable road manifest generation.
9. Atomically promote only the road scope entry in `active_set.json`, then run the normal
   read-only authority audit.
10. Remove the old Road profile field/reference only after zero Road-owned/profile
    references are proved. Do not delete or require global retirement of the Engine asset.

Any failure restores exact `-1` bytes/absence and the prior active set before another
attempt. A metadata-only manifest migration is forbidden when persisted mesh bytes change.

### Steady state

After cutover, changing the accepted MIC at the same soft path changes only ProjectMaterial
recipe/output/manifest authority. It produces zero road dirty units, zero World package
writes, and zero road manifest advancement. ProjectWorld evidence authenticates the
material identity/digest as a consumer input without making it road geometry authority.

The shared presentation schema/parser/fingerprint separation is implemented once. If
Landscape lands first, Road inherits its accepted seam rather than creating a second
version. Freeze the producer source sets as:

```text
project_road_mesh:v1
  common geography envelope + road geometry contract + geometry-generation sources only

presentation:v1
  road.default binding + material authentication + reference assignment/migration
```

The current mixed `ProjectWorldRoadRealization.cpp` responsibility therefore requires the
smallest source-set split or helper extraction that makes those fingerprints truthful.
Material identity/digest stays in presentation/operation evidence. Generated Road manifest
input identity writes `presentation_profile_sha256: none`; irrelevant presentation changes
never advance road geometry authority.

Prefer landing this locality split with the first Road reference cutover: changed mesh
bytes then advance the Road manifest normally with the new producer fingerprint and `none`
presentation identity. If the split is intentionally landed earlier while all artifact
bytes and semantic outputs are unchanged, use the existing fingerprint-v2 metadata
migration path and a reviewed metadata normalization to `none`; never regenerate geometry.
Metadata-only migration is forbidden once a serialized mesh reference changes.

## Candidate funnel

### R0 - fresh control and diagnosis

Using the accepted packaged product route and stable cameras, capture:

- player-height road surface;
- `road_oblique`;
- representative junction;
- generated cell boundary;
- terrain-road edge;
- aerial hierarchy.

Authenticate exact map, executable, active road manifest, `512/1536`, camera transforms,
road actor/mesh identity, material slot/path, collision, and WP state. Record whether any
geometry, seam, elevation, or collision symptom actually reproduces.

If a geometry defect reproduces, stop. It is a regression against accepted road authority,
not permission for this presentation concern to repair geometry.

### K1 - universal asphalt

Generate the closed material candidate, perform the bounded reference migration, and take
identical captures. Accept the visual candidate only if:

- the uniform debug/placeholder ribbon appearance is materially improved;
- roads read as asphalt at player and oblique views;
- accepted width hierarchy remains legible at aerial view;
- junctions, boundaries, and terrain edges show no new artifact;
- geometry/collision/WP parity and authority migration pass;
- packaged performance gates pass.

If K1 is correct and cheap but visually insufficient, return this concern to research with
the failed views. Do not automatically add markings, textures, class variants, or geometry.

## Deferred feature decisions

| Capability | K1 decision | Revisit only with evidence |
|---|---|---|
| Texture/material functions | Defer | K1 is visibly too flat and current mesh coordinates are sufficient |
| World-aligned texture | Defer | A bounded texture candidate is justified after K1 |
| Lane markings | Defer | Product view proves hierarchy/legibility requires them |
| DBuffer or mesh decals | Reject for K1 | Marking/damage need plus overlap and performance budget |
| Landscape/SplineMesh roads | Reject | Never replace canonical generated road authority |
| PCG road dressing | Reject for K1 | Separate researched decoration owner consumes road semantics |
| Shoulders/curbs | Reject for K1 | New geometry concern with its own authority review |
| Per-class material variants | Defer | K1 fails to expose width hierarchy |
| RVT/WPO/PDO/displacement | Reject for K1 | Separate demonstrated rendering requirement |

## Expected future change boundary

May change only after implementation selection:

- ProjectMaterial Road recipe, closed archetype support, generated parent/default MIC,
  material manifest, focused tests, and stable docs;
- ProjectWorld `.uplugin` content dependency if package proof requires it;
- ProjectWorld road binding/authentication/reference-only assignment and focused tests;
- shared presentation schema/parser decomposition needed to remove material ownership from
  WorldData and restore producer-local fingerprints/evidence;
- affected Kazan/test presentation profiles only to remove the road material field;
- all and only preflight-discovered road mesh package references during the one-time cutover;
- one immutable road-owner manifest generation plus one atomic active-set promotion;
- validation harness/evidence and owning stable docs.

Must remain untouched:

- canonical source/compiled road data, selected classes, widths, representations, topology,
  cell clipping, surface offset, segment length, terrain drape, and intersections;
- mesh geometry attributes, collision, Nanite, transforms, navigation, actor/map packages,
  World Partition ownership, runtime profile, HLOD policy, loading/menu/gameplay paths;
- terrain, water, vegetation, buildings, gameplay objects, and their generated packages;
- Lumen, quality, resolution, foliage, environment, and loading range as performance offsets;
- ProjectMaterial runtime service/API, ProjectWorld graph compiler, or Kazan material recipe.

Unexpected cross-owner propagation is a hard stop and returns the owning seam to review.

## Test-first acceptance

Add exact failing tests before production mutation when Road is selected:

| Test | Required proof |
|---|---|
| `Project.Material.Generation.RoadArchetypeGraph` | Closed Opaque/Default Lit textureless graph; deterministic save/reload; material owner only |
| `Project.World.Realization.Road.MaterialBinding` | `road.default` resolves an accepted MIC/digest and fails closed before mutation when missing, wrong, corrupt, or unaccepted |
| `Project.World.Realization.Road.MaterialReferenceUpdate` | Current geometry plus old slot writes mesh reference only; no MeshDescription rebuild or actor/map write |
| `Project.World.Realization.Road.MaterialUpdateLocality` | Same-path material update changes only ProjectMaterial authority; zero World dirty/write/manifest delta |
| `Project.World.Realization.Road.ProfileGeometryOnly` | WorldData rejects material path/recipe/graph/MIC/manifest fields but retains road geometry policy |
| `Project.World.Realization.Road.ReferenceMigration` | Exactly discovered mesh packages change; complete geometry/collision parity and actor/map byte identity |
| `Project.World.Realization.Road.AuthorityPromotion` | New immutable road manifest, changed asset SHA, stable semantic outputs, atomic promotion, clean audit |
| `Project.World.Realization.Road.Rollback` | Injected failures before/after promotion restore exact `-1` state and pass authority audit |
| generator fingerprint replacement cases | Binding/assignment changes move `presentation:v1` only; geometry-generation changes move `project_road_mesh:v1` only |
| generated layer input-identity locality | Road manifest stores presentation identity as `none`; profile-only changes do not dirty or advance Road authority |

Retain existing authority:

- `Project.World.Realization.CrossCellRoadIdentity`;
- `Project.World.Realization.Geometry.RoadTerrainDrape`;
- Slice 4 orientation-sensitive road collision proof;
- generated authority and producer-local fingerprint tests;
- packaged centre -> edge -> centre streaming/product route.

Use exact tests during iteration. Broad gates run only as the final accepted-slice envelope,
not as the debugging loop.

## Visual, MCP, and packaged evidence

MCP/live Editor readback authenticates object identity and state:

```text
same actor identities and transforms
same mesh package/object identities
same geometry/collision/Nanite/WP state
new authenticated road.default material reference
```

It does not claim one-time mesh byte identity. VisualVerification captures identical R0/K1
cameras. Human review selects visual quality only after structural tests pass. Editor FPS is
diagnostic; packaged physical hardware is performance authority.

Performance comparison uses one paired setup:

```text
physical RTX 4070
D3D12, High, 2560x1440
packaged Development
same driver, build configuration/toolchain, 512/1536 profile, route, cameras, warmup, and samples
record R0 and K1 executable SHA-256 independently; binaries may differ
```

Do not retain obsolete `materials.road` parsing or add a compatibility switch merely to
force a same-binary comparison.

Record Frame p95/p99/max, Game/Render/GPU p95, process/GPU memory, BasePass/material shader
statistics, instruction/permutation diagnostics, draw-call delta, package/cook delta,
streaming failures, wrong-cell events, and centre-edge-centre unload/reload correctness.

Hard requirements remain Frame p95 `<= 16.67 ms` and zero streaming failures. Historical
Slice 4 overall p95 `14.999 ms` and dense-centre p95 `15.969 ms` are risk indicators, not
reusable candidate evidence. K1 structurally adds no actors, decals, or sections and has no
texture dependency, but measurement decides its real cost.

## Rollback and temporary-file ownership

Before mutation, the owning wrapper stores one exact `-1` snapshot under:

```text
tmp/world/presentation/roads/<run-id>/rollback/
```

It covers prior/absent ProjectMaterial Road assets and manifest, affected code/profile state,
all discovered road mesh bytes, prior road manifest, `active_set.json`, and tool identity.
Expected-untouched actor/map packages are hashed, not duplicated.

All receipts, captures, and diagnostics remain in sibling owner folders. On success, handled
failure, and retry, the wrapper deletes rejected captures, shader dumps, temporary package
copies, transient config, duplicate backups, failed outputs, and obsolete logs. Retain only
the compact accepted evidence and one exact rollback while the transaction is open. Delete
obsolete rollback bytes after closure. Nothing in `tmp/` is a committed input.

No Git reset/clean, canonical road regeneration, terrain rebuild, or manual map edit is a
rollback mechanism.

## Ordered future implementation

1. [ ] Implement and accept the shared material core with the campaign-selected first
       archetype. If Road is not first, keep its archetype absent until Road is selected.
2. [ ] Capture fresh R0 structural, visual, manifest, and packaged performance evidence.
3. [ ] Add the focused red tests above, including the current early-out regression and
       producer-evidence locality correction.
4. [ ] Add the smallest closed Road archetype/recipes and generate/authenticate the parent
       and default MIC through ProjectMaterialEditor.
5. [ ] Add/inherit the shared profile/schema/fingerprint separation. Extract the Road
       presentation consumer from geometry producer sources, set Road layer presentation
       identity to `none`, and remove material implementation from WorldData without
       perturbing geography dirty semantics.
6. [ ] Add the ProjectWorld `road.default` binding and pre-mutation authentication.
7. [ ] Implement the independent `presentation:v1` reference-assignment pass so it runs
       after either geometry mutation or no-op and cannot be bypassed by geometry early-outs.
8. [ ] Run the one-time transaction, semantic parity, immutable road manifest promotion,
       zero-reference proof, and authority audit.
9. [ ] Run exact tests, identical R0/K1 visual proof, and paired packaged RTX 4070 gate.
10. [ ] If K1 fails visually, return only Road to research. If it passes, promote durable
        contracts into stable owner docs and clean owner temporary files.

## Stable documentation propagation after accepted implementation

- ProjectMaterial README/data docs own the Road family/archetype, recipe/resource identity,
  compiler/manifest/rollback behavior, and no-consumer-knowledge rule.
- ProjectWorld README and territory-generation docs own `road.default`, authentication,
  assignment, reference migration, and geometry/presentation locality.
- ProjectWorldData README/data docs state that it owns road geography/semantic facts and
  geometry policy, not material implementation.
- Plugin dependency and C4 indexes show the content soft-reference relationship without a
  ProjectMaterial runtime service or reverse World dependency.
- The World pitfall owner records the early-out/reference-migration trap and its regression.

Stable docs/code never link back to this transient todo.

## Stop conditions

Return to focused Road research if K1 is visually inadequate; road-local UV/tangent data,
textures, markings, class variants, shoulders, or curbs become necessary; any geometry,
collision, Nanite, actor/map, or cross-layer package changes appear; ProjectMaterial cannot
express the candidate within its selected closed archetype policy; or packaged performance
fails. Do not grow this implementation slice in response.

## Remaining authentication and R1 close-out

- [x] Reviewer supplied a comprehensive material-first Road packet.
- [x] Agent verified the current road generator, early-outs, profile parser, realization
      settings, active manifest census, producer fingerprints, authority transaction,
      installed UE 5.8.1 identity, tests, and accepted runtime/performance SOTs.
- [x] Corrected the reviewer's dirty-identity claim and clarified package/draw/texture proof.
- [x] Applied the second review's producer-locality, independent-binary, and Road visual
      criterion corrections, narrowing manifest-field removal to schema-valid `none`.
- [x] Same reviewer rechecked the corrected packet and returned PASS on 2026-08-26.
- [x] Propagated research completion to the campaign router/current orchestrator without
      starting Road implementation or Vegetation research.
