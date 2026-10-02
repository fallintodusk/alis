# Build The Universal Generated Surface Material System

**Status:** PASS (2026-10-01). T10 (committed as `5ec561b38`) and T11 are implemented; T11 landed
inside the black-box independence sweep and passed that sweep's independent review; closure follows
D25, and the full payload dependency closure stays with the 3.0.0 plan
**Scope:** the universal generated surface-material and instance domain, proved first by production
Mesh Terrain and then by one dissimilar surface: closed ProjectMaterial and ProjectTexture recipes,
metric projection, Substrate generation, bounded procedural pattern kernels, cache selection
(ProjectTexture runtime cache is selected; Mesh Partition Material Cache is not), shader/permutation/storage/memory
budgets, concern-named output identity without the `Generated` segment, ProjectWorld's data-driven
terrain-material binding, and one-pass retirement of V1
**Stable documentation owner:** [ProjectMaterial README](../../../Plugins/Resources/ProjectMaterial/README.md),
[Material generation](../../../Plugins/Resources/ProjectMaterial/docs/material_generation.md), and
[ProjectTexture README](../../../Plugins/Resources/ProjectTexture/README.md); the ProjectWorld binding
statement lives in [Territory generation](../../../Plugins/World/ProjectWorld/docs/territory_generation.md)

Agent route: read this file, then only the owner sections it cites. This file is the single active
plan for the material-system slice. The Mesh Terrain cutover is complete; do not reopen it or add a
second material todo. Reviewers start with `## Reviewer brief`.

## Current state and next steps

State on 2026-09-30: production Kazan and Manhattan resolve the generated
`/ProjectMaterial/Surfaces/Terrain/MI_ProjectTerrain_Default` through the shared Mesh Terrain MPD.
CanonicalCompilation owns final semantic surface facts; ProjectWorld maps them to the private Mesh
Terrain channel ABI; ProjectMaterial owns appearance. The current semantic roles are the
deliberately honest `ground` and `hydro_transition`; slope and height are continuous presentation
inputs and cannot manufacture factual geology or land cover.

ProjectTexture and ProjectMaterial implement the selected generated surface system. ProjectTexture
schema v3/compiler v5 exports one 512x512 linear `Structure` tile; ProjectMaterial schema v4/compiler
v5 samples it with three triplanar reads. The terrain parent and default instance both set
`PatternScaleMeters=96.0`; the object cube instance sets `0.5`. `GroundDetail` modulates roughness
only and is not treated as height or normal data. The rejected 2/4/16 m experiment resampled only
the existing B signal for appearance modulation; it did not vary the primary `PatternScaleMeters`
that controls the complete tile period.

The terrain graph starts from Unreal world position in centimeters, converts to meters,
divides by `PatternScaleMeters`, samples XY/XZ/YZ, and blends those projections by the absolute
world-space vertex normal. Packaged scale controls, an exact compiled-section-edge view, and
an aerial recurrence view support retaining the current 96 m production tile period.
At 96 m per complete output tile, source frequencies of 4, 12, 24, and 64 repeats per tile imply
nominal periods of 24, 8, 4, and 1.5 m. The final `CoverPatch` combines the 4/12-frequency inputs;
`GroundDetail` combines 12/24/64-frequency inputs. The stable ProjectTexture document now
records all three contributions.

The current generator noise nodes use tileable native functions, while the exported render target
and its mips use wrap addressing and trilinear filtering. Existing seam checks at mip 0 and mip 4
establish boundary continuity only; they do not establish that a full-tile motif is not recognizable.
Accepted packaged cube captures now compare 0.25/0.5/1.0 m on the same 1 m mesh, plus scaled,
nonuniform, moved, and rotated controls. Accepted packaged Mesh Terrain controls compare 48/96/192 m
at diagnostic strength 0.8 and 64/96/128 m at the production strength 0.12. Both paths respond to
the declared scale; the latter comparison gives no reason to change the current 96 m recipe. Wide
production views show no obvious hard restart. A later editor vertex audit identified two adjacent
compiled sections sharing the Y=-562900 edge; the packaged probe authenticated those identities,
and the unmodified 96 m production view showed continuous ground there. A sampled aerial recurrence
view showed no objectionable repeated motif; this is bounded visual evidence, not a proof for every
viewpoint. The old same-output 2/4/16 m branch remains removed; no extra sample or detail-normal
output is accepted by this investigation.

```text
canonical semantics + authored overlays
        -> ProjectTexture pattern schema v3 / compiler v5 and bounded runtime cache
        -> stable sampled Structure output
        -> ProjectMaterial surface recipes and generated parent families/instances
        -> Substrate material backend
        -> replaceable realization adapter
```

The selected production path is ProjectTexture's runtime structural cache, not Mesh Partition
Material Cache. One authenticated two-node native DAG generates a shared 512x512 RGBA8 Structure
output with a complete mip chain. One writer, explicit generating/ready/failed state, pins, exact
resident-byte accounting, budget refusal, and unpinned LRU eviction bound its lifecycle. Terrain
uses honest `world_metric_triplanar_2d`; the movable object proof uses
`object_metric_triplanar_2d`. ProjectWorld neither calls nor owns this cache.

The equivalent direct A/B arm was packaged, captured, and then removed. Cached rendering preserved
the terrain read, reduced the Development package by 4,836,032 bytes, produced smoother distant
detail through mips, and replaces four procedural noise evaluations per visible terrain pixel with
one shared sampled resource. The old CSV route crashed before sampling both arms; that common
instrument failure is not evidence against either material arm. The final selected path now has
600 authenticated steady frames through World's existing native performance consumer.
Runtime Virtual Texture (RVT) is a distinct world-space
composition tool for a future cross-primitive terrain/road/decal requirement; this slice has no
such requirement and does not implement or benchmark RVT. Texture Graph is not an accepted
Shipping runtime backend in 5.8.3: despite Runtime-typed modules and public async APIs, its engine,
device, blob, scheduler, and mix-manager initialization are inside `WITH_EDITOR`. It may be used as
an editor implementation or future adapter, but the architecture cannot depend on packaged runtime
execution.

The storage concern stays quantitative. The selected 512x512 RGBA8 output occupies 1,398,100 bytes
with all mips; the measured two-node generation peak is 2,796,200 bytes. Resource cardinality is one
ready output for the structural identity whether 1, 10, or 100 consumers request it. Assets,
shaders, and runtime resources therefore scale with unique reusable structure, never with cells,
cities, colors, scale, wetness, or ordinary material-instance variation.

The generated pattern is analytic and admits no imported texture bytes, so no new provenance entry
is required. ProjectMaterial recipes name only a stable pattern ID. ProjectTexture's accepted
authority resolves that ID to the `surface_structure_rgb_v1` output contract, semantic identity,
package hash, and current signals (`R=MacroVariation`, `G=CoverPatch`, `B=GroundDetail`). The basis
packs medium structure with high-frequency detail into blue before the child derives the exported
tile. Neither node reads alpha; ProjectMaterial consumes only RGB as linear data. The prior
four-channel readback failure describes the superseded candidate, not the current contract.
ProjectMaterial owns all appearance strengths. The final Kazan and Manhattan same-input applies
after RGB-contract regeneration had zero dirty units and zero actor changes. The pre-RGB Shipping cook
compiled `M_ProjectTerrain` for SM5 and SM6, passed archive and
IoStore validation, produced a 2,063,109,020-byte (1.921 GiB) release payload, and passed packaged
Kazan, Manhattan, ground-detail, and moved/rotated-cube fixed-view captures. The Manhattan capture
is a technical terrain-presence sanity view, not a presentation-quality shot. The ground-detail
capture proves stable metric projection and broad structure, not final photoreal microdetail. The
known City17 Landscape sampler overflow remains separate authored-content debt.

The current-RGB packaged Development board covers Kazan close/player/oblique/hydro/aerial/exact edge,
Manhattan oblique/far, and moved/rotated/nonuniform cube views without an obvious fallback or seam.
The final source-engine Shipping release also passed cook, archive, IoStore, package-size checks,
Kazan/Manhattan fixed views, a moved/rotated generated cube, and the dedicated Water proof. A
source-built Shipping Client rendered the Kazan fixture against the same staged content; Game
also rendered the fixture with the engine-confirmed SM5 feature level. These
sampled views do not establish photoreal microdetail. Independent R2 found no production-code
defect in the earlier production diff. After the identity-locality correction, both cities were
regenerated through the supported World transaction with their declared runtime profiles. Same-input
reruns had zero dirty units and zero actor changes; the final authority audit accepted all 14 scopes
and verified 3,096 artifacts byte-for-byte. The final Development package accepted the normal Kazan
product route and exactly 600 native frames: frame p95 14.64 ms against the existing 16.67 ms budget,
engine-reported GPU p95 9.65 ms (possibly inferred), and zero streaming failures. ProjectTexture's packaged CPU request-to-Ready proof
recorded 13.24 ms cold and 0.0058 ms warm with zero additional warm generation and the same resource.
The refreshed source Shipping package passed cook/archive/IoStore and packaged Kazan and Manhattan
fixed views. Those packages predate the latest fingerprint-locality and collector-capacity patch;
they remain bounded evidence for the accepted material and visual result, not a claim that the
latest source or refreshed authority has been packaged. The final delta R2 passed after tightening
the fingerprint marker parser.

Two V1-era contracts were not carried into v2. The public developer payload still declares the
deleted V1 material manifest and verifier and omits ProjectTexture, so neither payload composition
nor its dependency-closure audit can pass (E46-E48). Orphan cleanup in both owners trusts Asset
Registry referencers inside a commandlet that does not gather the registry, so a package that World
still references may be deleted (E51, A14). T10 fixes both through the existing owners: one
recipe-source digest in each owner manifest, one generic payload authority kind, and one registry
gather before cleanup. It costs one pattern and surface regeneration, with no World write, cook, or
package.

| Step | Runs | Verified by | Starts when |
|---|---|---|---|
| 1. T10 | done | T10 record | done |
| 2. T11 closure corrections | done | T11 record | done |
| 3. Closure | done | D25 conditions; the 3.0.0 plan owns the full payload closure | done |

## Contents

- Goal - Authority register - Non-goals
- Verified evidence (owners, consumers, Mesh Terrain, next-gen backends, storage, inferences,
  unverified, refuted, tiling and metric scale, public payload and orphan safety)
- Current architecture and source of truth - Capability gap - Box-first proof
- Decision (target shape, premise / KISS gate, channel ownership, public payload and orphan
  safety, alternatives)
- Required invariants - Implementation tasks (T0-T10)
- Test-first and verification plan - Documentation plan - Rollout and rollback
- Reviewer brief - Completion criteria - Review record

## Goal

Build one universal, data-driven surface domain in which ProjectMaterial generates optimized parent
materials and instances from closed JSON recipes, ProjectTexture owns reusable seamless structural
pattern recipes, and engine technologies remain replaceable backends. Terrain is the first proof,
not the architectural boundary. The result must deliver believable close and far rendering, stable
metric projection, bounded shader/permutation/VRAM/storage cost, page-granular runtime reuse where
it wins, deterministic regeneration, and no unique authored truth inside generated Unreal assets.

## Authority register

Binding `D<n>` entries outrank `## Decision`, the KISS gate, reviewer findings, and agent
preference within their scope. Cite IDs elsewhere; never restate or renumber them.

### Operator decisions

D1-D8 are the Terrain Material v2 plan the operator accepted on 2026-09-23 ("ok so prepare these
todos ..."); the operator stated no reason for them unless one is given.

- **D1** ProjectMaterial remains the owner of the terrain material.
  - Effect: recipes, compiler, archetypes, output assets, and the accepted manifest stay in
    ProjectMaterial; no second material owner appears in World.
- **D2** Remove the unnecessary `Content/Generated/...` taxonomy.
  - Effect: generation provenance moves from the folder into manifest and asset metadata; content
    is organized by concern. Scope is ProjectMaterial only; other owners' generated roots are not
    touched by this decision.
- **D3** Extend the existing ProjectMaterialEditor, not another generator.
  - Effect: the recipe, identity, manifest-last, and host-transaction lifecycle is reused; the
    definition generator is not a material route.
- **D4** Output is a generated final terrain parent and its instances.
  - Effect: no hand-authored master material, and no authoring UX is required.
- **D5** Inputs are semantic surface inputs, delivered as Mesh Terrain weight channels.
  - Effect: the material maps canonical terrain-surface semantic names to appearance and samples
    the Mesh Terrain channels that carry their weights. The recipe does not define the vocabulary.
- **D6** ProjectWorld only binds and assigns the final material.
  - Effect: within material ownership, ProjectWorld only binds and assigns the final asset; it
    compiles no graphs and owns no material parameters. CanonicalCompilation defines semantic
    surfaces and their engine-independent derivation; ProjectWorld consumes that contract and maps
    its weights to the selected representation.
- **D7** Production-quality close and far rendering with bounded shader cost.
  - Effect: measured on cooked shader platforms and the packaged quality/performance budget.
- **D8** The terrain substrate is settled first; this task freezes its final material contract only
  after that.
  - Effect: this gate is satisfied by the accepted production Mesh Terrain cutover. The material
    contract targets the shared MPD/channel receipt and does not reopen the substrate choice.
  - Reason: the accepted proposal argued that perfecting a classic-Landscape material that may be
    retired immediately is waste.
- **D9** Do not couple to existing stubs; keep the right separation-of-concerns naming hierarchy
  and dependency direction; build future-proof, data-driven, fully modular, black-boxed, decoupled,
  and component-driven. Operator's words: "we don't have fully architecture for such solutions so
  no need couple for existing stubs, we need keep the right SOC names hierarchy and vector and
  future proof oriented build with our data driven design fully modular blackboxed decoupled
  component driven".
  - Effect: the V1 prototype family gets no compatibility layer; ProjectWorld binds through data,
    not a hard-coded path; each archetype and the substrate adapter are black boxes; dependency
    direction stays World -> Resources.
  - Reason: not stated beyond the words above.
  - Date/source: 2026-09-23, this session.
- **D10** This task lives in `todo/00_current/` beside the 3.0.0 release router.
  - Effect: placement only. The completed Mesh Terrain task is archived and is not a live
    dependency graph.
  - Date/source: 2026-09-23, this session ("I guess we need 3 todos in current").
- **D11** Even though Mesh Terrain is Experimental, it is the long-term direction: after the test
  passes, the architecture wraps fully around it, with no legacy support and no legacy mentions.
  Operator's words: "even experimental it future long going goal so we will wrap around it our
  architecture after test fully without any legacy support and mentions".
  - Effect: v2 is designed for Mesh Terrain channels. The earlier conditional Landscape-adapter
    interpretation is superseded by the Mesh Terrain task's D6-D8: failed evidence starts the
    workaround loop, and only its Q6 give-up path returns to the operator. At completion no V1,
    `Generated`, or superseded-substrate mention remains in stable docs, code, comments, tests, or
    configuration.
  - Reason: "future long going goal".
  - Date/source: 2026-09-23, this session.
- **D12** A bigger package is fine if it buys more freedom, quality, and optimization, and the
  future path is proven robust. Operator's words: "if it bigger but we gain more freedom and
  quality and optimization and will be ensured that it future path robust - we fine".
  - Effect: closes Q2. Texture and shader size is still measured and reported (T6), but a size
    increase alone does not block v2.
  - Date/source: 2026-09-23, this session.
- **D13** A hand-adjustment layer is mandatory in every generation: first generate from raw data,
  then apply a flexible hand layer on top. Operator's words: "we need to understand how to keep
  extra layer that will apply hand adjusting - it's our mandatory in all generations - first
  generations by raw data then applying hand layer flexible".
  - Effect: generated surface-channel weights also take the hand layer (hand-painted surface
    corrections survive regeneration). The mechanism has one owner, the terrain hand layer designed
    in the Mesh Terrain migration task; this task consumes it and never adds a second one.
  - Date/source: 2026-09-23, this session.
- **D14** City17 is a legacy demo; once City17 is restored inside the fully rebuilt Kazan with
  gameplay, the legacy City17 content is removed outright. Operator's words: "city 17 it's just
  demo legacy, when we will rebuilt terrain and etc and restore city 17 in the full rebuilded Kazan
  with game play we will remove all just plainly".
  - Effect: any City17 texture admitted by A7 is promoted to the texture owner before City17 is
    removed; nothing in v2 depends on City17 content.
  - Date/source: 2026-09-23, this session.
- **D15** The terrain slice is the first proof of a universal generated material and texture
  domain, not a terrain-only architecture.
  - Effect: ProjectMaterial owns closed conceptual surface recipes and generated parent/instance
    families for terrain, roads, buildings, objects, water, wood, metal, and later domains;
    ProjectTexture owns reusable seamless structural pattern recipes. The implementation proves
    terrain first and exactly one dissimilar second surface before claiming universality.
  - Date/source: 2026-09-27, attached operator discussion and current request.
- **D16** Integrate Epic experimental or beta technology now when Epic is actively carrying it as
  a future path and it provides a real present advantage, as was done for Mesh Terrain.
  - Effect: experimental status alone is not a rejection. Each technology remains behind an ALIS
    recipe/adapter boundary and must pass an executable packaged proof before production authority
    depends on it. Marketing, a Runtime module label, or an exposed API is not proof that it works
    in a non-editor Shipping process.
  - Date/source: 2026-09-27, attached operator discussion and current request.
- **D17** Minimize cooked physical-asset multiplication and city-scale storage growth. Prefer
  runtime calculation and bounded page/cache reuse where it improves the measured total system.
  - Effect: no unique texture asset per cell, city, color, scale, wetness value, or minor variant;
    color/scale/cheap variation stay parameters. A runtime cache must have explicit identity,
    memory budget, eviction, cold-start, failure, and invalidation behavior. Moving unbounded bytes
    from disk to VRAM does not satisfy this decision.
  - Date/source: 2026-09-27, current request.
- **D18** Use stable metric coordinates for every generated surface, but select the coordinate frame
  by domain: world metric 3D for static irregular world geometry, object-local metric 3D for
  movable objects, and tangent/direction-aligned projection for roads, grain, and other directional
  surfaces.
  - Effect: "world aligned" becomes a projection family with real-world scale, not a mandate to
    use absolute world coordinates on every object. This prevents swimming on movable objects and
    preserves directional structure.
  - Date/source: 2026-09-27, attached operator discussion.
- **D19** Authored work always enters before dependent generation or realization, never as edits to
  generated material, texture, mesh, or map assets.
  - Effect: each domain owns an optional authored overlay over its canonical/generated inputs;
    there is no giant global post-generation patch file and no second writer to generated UAssets.
  - Date/source: 2026-09-27, attached operator discussion; consistent with D13.
- **D20** Investigate texture tiling and metric scale first on a simple box, and only then carry a
  proved result to the terrain mesh; this round is investigation, not implementation.
  - Effect: compare pattern-scale behavior in the box fixture before changing or promoting terrain
    appearance. Keep the accepted world/object coordinate-frame split (D18); this does not authorize
    a new texture output, extra terrain samples, or a second projection owner.
  - Reason: not stated.
  - Date/source: 2026-09-29, current operator request.
- **D21** Q6 = A: add `recipe_source_sha256` to the ProjectMaterial and ProjectTexture accepted
  manifests; preserve the semantic `recipe_sha256` and the generated asset identities.
  - Effect: closes Q6; the owner-manifest and payload bullets of T10's decision bind.
  - Reason: avoids maintaining duplicate C++ recipe parsers in Python.
  - Date/source: 2026-09-30, decision text the operator supplied with the instruction to fix the
    reviewed issues.
- **D22** Q7: retire the migration-specific rollback drill; keep M-10 as an active cross-owner
  reliability requirement, satisfied only by proven World transaction restoration and
  referenced-orphan retention.
  - Effect: closes Q7; the rollback-drill completion row is retired, and no generic post-success
    rollback is claimed as tested.
  - Reason: "V1 is permanently removed."
  - Date/source: 2026-09-30, same supplied text.
- **D23** T10 freezes the ProjectTexture DAG/cache, ProjectMaterial rendering, Mesh Terrain, World
  generation, and accepted appearance: no World Apply, city regeneration, cook, archive, or
  Shipping package.
  - Effect: T10 is proved by owner, payload, and read-only audits only.
  - Reason: not stated.
  - Date/source: 2026-09-30, same supplied text.
- **D24** [SUPERSEDED by D25] This task reaches PASS when T10's tests, payload composition, dependency closure, owner
  no-op guarantees, orphan safety, and independent R2 pass. Current-source Shipping certification,
  Manhattan packaged performance, clean public checkout, and final operator visual approval stay
  with the 3.0.0 router's R5.
  - Effect: development completion is separate from release certification.
  - Reason: not stated.
  - Date/source: 2026-09-30, same supplied text.
- **D25** Q8 = component-scoped dependency closure. Supersedes: D24 (its other conditions stand).
  ProjectMaterial and ProjectTexture development completion requires their accepted generated
  packages, recipes, and transitive dependencies to be complete and authenticated. The complete
  public developer-payload dependency closure remains a mandatory 3.0.0 release gate: do not
  weaken the global audit, turn its rejections into warnings, or add packages without verified
  authority and redistribution rights. Every remaining release blocker gets an explicit owner in
  the 3.0.0 plan, without one task per package.
  - Effect: closes Q8; closure also needs the T11 cleanup corrections and independent R2.
  - Reason: material completion and complete release closure are different requirements.
  - Date/source: 2026-09-30, reviewed decision text the operator supplied with the instruction to
    fix the reviewed issues.

### Operator gates

The Mesh Terrain substrate gate is closed: the production cutover has passed and its completed
evidence is archived by the World owner. This task does not depend on another active todo.

- **Q1 [CLOSED by A7, A8, and D17]:** Which surface classes and redistributable texture sets does
  the first production slice ship with? - canonical classes remain `ground` and
  `hydro_transition`; initial structure is procedural. No imported texture set is required. A
  specific exemplar may enter later only with exact provenance/license/lineage and a measured
  quality benefit that the procedural path cannot provide.
- **Q2 [CLOSED by D12]:** If production texture sets
  materially increase packaged bytes, which size trade is acceptable? - default while open: report
  exact deltas; no invented threshold.
- **Q3 [CLOSED by D16-D17]:** Should beta/experimental rendering technology wait until it becomes
  production-labeled? - no. Integrate a proven future-facing backend behind a replaceable seam,
  but do not waive packaged proof, budgets, or failure controls.
- **Q4 [CLOSED by the runtime-cache proof]:** ProjectTexture's bounded reusable structural cache
  wins this slice; Mesh Partition Material Cache remains disabled. The direct arm was an
  equivalent-quality control and is removed after packaged A/B. RVT still requires a later,
  concrete cross-primitive requirement. The historical direct-vs-cache frame/GPU delta remains
  unverified because CSV crashed before sampling either arm; the selected cache path now has a
  valid native 600-frame measurement.
- **Q5 [CLOSED by T9 evidence]:** The operator identifies texture tiling as the only current material
  issue. Is the visible problem recognizable full-tile motif recurrence, or a hard seam/restart at
  a terrain part boundary? - default while open: investigate motif recurrence first and check seams
  separately only to distinguish them; do not reopen other material-quality work. - answered by
  T9: at the 96 m production baseline, sampled continuity at an identified section edge and the
  period/half-period recurrence views found no hard seam and no objectionable motif; this is not
  a claim of universal non-repetition.
- **Q6 [CLOSED by D21]:** How does the public developer payload prove that a published
  v2 surface or pattern recipe still matches its accepted generated asset? Manifests record
  `recipe_sha256` over C++-normalized semantics, and the deleted V1 Python verifier was the
  release route's only such check. Options: (a) both owners also record an LF-normalized raw
  recipe digest, and the composer checks bytes, completeness, package hashes, and the
  surface-to-pattern dependency (one normalizer, fail-closed on any recipe edit; expected cost is
  one material and pattern regeneration, and M-14 shows same-path material changes write no World
  package); (b) port both normalizers to Python (V1 precedent, second
  normalization authority); (c) drop recipe-drift detection (weaker than the removed guard). -
  recommended: (a).
- **Q7 [CLOSED by D22]:** May the rollback-drill completion row be retired? It asked for a proved
  post-success rollback before the V1 -> v2 migration because `RollbackPrevious` has no apply mode
  (E5). The migration is accepted and D11 forbids returning to V1. A later accepted material change
  rolls back as one whole-change revert of source, recipes, manifests, and assets, then an editor
  build and a zero-write owner `Validate`; that route is generic, not material-specific. - gates:
  closure only; recommended: retire the row and record the generic route in the Review record.
- **Q8 [CLOSED by D25]:** Does D24's "dependency closure" mean the whole public payload
  audit, or the closure of this task's generated surface and pattern packages? The whole audit
  rejects 42 dependencies of World content that no payload authority selects; none is required by
  a surface or pattern package, and all predate T10 (see the T10 implementation record). -
  recommended: the latter, with the 42 owned by the 3.0.0 router's pre-candidate item. - answered
  by D25; E55-E56 correct the count to 45 and classify each by owner.

### Working assumptions

- **A1 [RESOLVED by the accepted cutover]:** Mesh Terrain is the production substrate. The
  canonical terrain-surface contract and adapter layout are accepted; only reviewed representation
  adapters depend on MeshPartition. There is no Landscape-outcome branch or compatibility path.
- **A2 [ACTIVE]:** V1 (`surface_opaque` / `landscape_basic_v1`, a textureless slope lerp) is a
  prototype stub under D9. It is replaced in one pass with no compatibility layer, and its recipes,
  archetype builder, assets, and references are removed when v2 lands.
- **A3 [REJECTED by evidence]:** The first surface vocabulary derives only from semantics the
  canonical data and realization already provide - slope from geometry, the hydro water footprint
  and shore, and road, building, and vegetation footprints. New provider ingestion (for example land
  cover) is a separate source concern. - Refuted by the architect review: channel weights are baked
  into terrain meshes and the channel map is a build dependency of every section
  (`MeshPartitionDefinition.cpp:104`), so weights are terrain-geometry inputs; terrain identity is
  only canonical terrain plus water, and roads, vegetation, and buildings depend on terrain, so
  footprint weights would change locality or create a dependency cycle. Replaced by A6.
- **A6 [ACTIVE]:** The first vocabulary uses only inputs already inside terrain identity: slope and
  height computed in the material from geometry, and the hydro water footprint and shore from the
  terrain-water projection. Road, building, vegetation, or land-cover channels need a terrain
  contract change (selectors, identity, dirty planning), and derive only from canonical data, never
  from realized dependent layers.
- **A4 [REJECTED by provenance census]:** Texture sets come first from project textures that already ship, such as the
  City17 terrain sets under `Plugins/Resources/ProjectObject/Content/Nature/Landscape/Material/Texture/`
  (`Asphalt_Damaged`, `Grass_Lawn`, `Grass_Rubble`, `Soil_Leaf`, `Soil_Rock`). Reused textures are
  promoted to the texture resource owner ([ProjectTexture](../../../Plugins/Resources/ProjectTexture/README.md))
  under ProjectMaterial's "Reuse is the promotion boundary" rule, instead of referenced from
  ProjectObject. The license of each set is confirmed against the
  [component license policy](../../../docs/legal/component_license_policy.md), under which
  unclassified or unmarked material cannot be distributed until its ownership, component, and terms
  are recorded. - Rejected as a default: repository presence and prior packaging do not prove
  redistribution rights, and no text provenance/license record was found for these candidate names.
  Replaced by A7.
- **A5 [REVISED by E25]:** The v2 object path is chosen once to make zero-World-write appearance
  tuning possible. A path change rewrites every terrain package (428 external-actor packages
  reference V1 today), so the identity migration lands with the production terrain regeneration,
  not as a separate full rewrite. Current Landscape proves same-path locality (E11); future Mesh
  Terrain accepts that contract only after M-14 proves the real builder/cook behavior.
- **A7 [ACTIVE]:** T1 treats every unclassified texture as unavailable. A candidate enters the
  design packet only with primary-source provenance, license terms compatible with public source
  redistribution, component ownership, and the required attribution record. Existing City17 assets
  may be selected only if that evidence is recovered; otherwise T1 chooses newly sourced assets
  whose terms are explicit.
- **A8 [ACTIVE]:** The smallest proposed canonical terrain-surface vocabulary compatible with A6
  names only facts its derivation proves. The safe default is neutral `ground` plus a precisely
  defined hydro-derived role such as `hydro_affected_ground`; exact names wait for T1 provenance
  review. Height and slope remain continuous geometric inputs, so ProjectMaterial may choose a
  rock-like appearance on steep ground without CanonicalCompilation asserting that rock exists.
  `rock`, grass, asphalt, rubble, road, building-footprint, or other factual classes are not admitted
  until canonical inputs and lineage prove them. T1 may rename or reject the default roles with
  evidence, but may not add an unowned or semantically stronger source.
- **A9 [ACTIVE]:** ProjectTexture recipes initially describe reusable structural algorithms and
  their bounded parameters, not colored finished textures. ProjectMaterial owns color, roughness,
  scale, semantic blending, and other appearance composition unless changing them genuinely changes
  structural pattern identity.
- **A10 [ACTIVE]:** A small number of persistent generated Unreal parent/function/cache-tag assets is
  acceptable and necessary for cook discovery and renderer integration. "Less physical assets"
  means O(archetypes and kernels), not zero UAssets and not O(cells or cosmetic variants).
- **A11 [ACTIVE]:** Substrate is the selected ProjectMaterial backend direction because UE 5.8
  enables it by default for new projects and converts non-Substrate roots at compile time, but the
  existing ALIS project-wide opt-in first requires the minimal ALIS compile/render smoke, then the
  normal final target/cook/quality/performance gates before production promotion. Begin with the
  bounded Blendable GBuffer path; Adaptive GBuffer is not accepted without a separate measured win.
- **A12 [REJECTED by E29-E31]:** Texture Graph can be the first Shipping runtime pattern-cache
  backend because it exposes Runtime modules and async render-target APIs. The installed 5.8.3
  implementation initializes its engine only under `WITH_EDITOR`, and its async task explicitly
  disables the Texture Graph cache. It remains an editor/future adapter, not current runtime
  authority.
- **A13 [RESOLVED by T9 evidence; see Q5]:** The operator narrowed the current material concern to texture tiling; the exact
  symptom (full-tile recurrence versus a hard boundary seam/restart) remains unknown (Q5). Motif
  recurrence is only the working hypothesis. Other material-quality work is not reopened.
- **A14 [CONFIRMED by M-4 red evidence]:** Orphan cleanup in the generation commandlet cannot see referencers in unloaded
  World packages, so `-CleanupOrphans` would delete a surface or pattern package that World still
  references (inferred from E51). M-4 must observe this failing before T10 changes cleanup; if M-4
  passes unchanged, this assumption is rejected and only the guard lands.

## Non-goals

- Implementing every water, road, building, vegetation, wood, metal, and object material in the
  first slice. Their future compatibility shapes the contract; terrain plus one dissimilar surface
  proves it.
- New provider or canonical semantics such as land cover.
- A custom `IVirtualTexture` producer, a persistent on-disk runtime texture cache, custom block
  compression, or an ALIS virtual-texture allocator. UE's higher-level cache paths must first prove
  insufficient.
- Texture Graph as a Shipping runtime dependency in UE 5.8.3. Its editor use remains optional and
  cannot become recipe authority.
- Any RVT implementation or comparison. A later task may admit it only for a concrete
  cross-primitive composition requirement.
- A new `DetailNormal`/`MicroHeight` output or terrain normal branch as a proposed tiling fix;
  those address close physical response, not the complete Structure tile's repeat period.
- City17's authored `M_Landscape`, its SM5 sampler overflow (its own backlog task), and
  ProjectMaterial's object material-layer system.
- Other owners' `Generated/` roots, such as `/ProjectWorldData/Generated/`, which the World
  transaction and release projection enforce.
- Replacing ProjectMaterial's private SHA-256 with ProjectCore's; the launcher engine's
  `GetSHA256Signature` assert is why it exists (ProjectMaterial pitfall 1).
- Consolidating the duplicated host-script helpers (E9c) and renaming the receipt's
  `authentication_sha256` (E5); this task records the defect but does not build a second utility
  refactor into the material slice.
- The World-local Water material builder (`ProjectWorldWaterRealization.cpp`, `BuildMaterial`,
  `M_ProjectWorldWater`); it migrates only when its own material slice is implemented.
- Physical-material or grass bindings from surface channels; the data-driven vocabulary keeps them
  possible later.

## Verified evidence

E1-E26 record the earlier census at HEAD `77ee583acfbaa22798d316929ee68361f27110d1` and launcher
UE 5.8.3 CL 58210709. E27 onward was rechecked on 2026-09-27 at HEAD
`c08b79afe9c1dbbb2ffcff49cb555bda0bd8c8a1` with the existing operator working tree left intact.
No conclusion below treats unrelated dirty files as accepted authority.

### Verified facts - ProjectMaterial

- **E1** V1 is a closed contract: schema 1, family `surface_opaque`, archetype
  `landscape_basic_v1`, compiler 1, kinds `parent` and `instance`, and only `Roughness`,
  `SlopeContrast`, `LowSlopeColor`, `SteepSlopeColor`. Unknown fields fail before any write, and a
  new parameter or archetype needs a new schema or compiler version
  ([Material generation](../../../Plugins/Resources/ProjectMaterial/docs/material_generation.md);
  `FProjectMaterialRecipeContract::Parse`, `ProjectMaterialRecipe.cpp:272-374`).
- **E2** One archetype builder is hard-wired: `RegenerateInternal` calls
  `ProjectMaterialTerrainBuilder` directly (`ProjectMaterialGenerationService.cpp:465-469`), with
  no archetype table. V1's parent graph has 9 expressions and 4 parameters, with no textures,
  functions, or layers (`ProjectMaterialTerrainBuilder.cpp:77-264`).
- **E3** Output identity is `/ProjectMaterial/Generated/<folder>/<id>.<id>` (`ResolveOutputIdentity`,
  `ProjectMaterialRecipe.cpp:394-417`). The semantic identity hashes the recipe, parent, output
  path, engine identity, and a compiler fingerprint computed by UBT over non-test source and schemas
  (`ProjectMaterialEditor.Build.cs:16-72`).
- **E4** The `Generated` segment is enforced in 12 files (17 text hits): both schemas
  (`material-recipe.schema.json:28`, `material-manifest.schema.json:43`), the manifest, the V1
  instance recipe, the README, `material_generation.md`, `ProjectMaterialGenerateCommandlet.cpp:123`,
  `ProjectMaterialGenerationService.cpp:24`, `ProjectMaterialGenerationTests.cpp:359`,
  `ProjectWorldPresentationMaterialBinding.cpp:23`, `ProjectWorldPresentationTests.cpp:96`, and
  `territory_generation.md`. It is also implicit in the commandlet's
  `GetContentDir()+"Generated"` (`:124`) and the host wrapper's `'Content\Generated'`
  (`scripts/ue/material/run_material_generation.ps1:184`), which scope the snapshot and the orphan
  scan, and in the automation test mount `/ProjectMaterialTest/Generated`
  (`ProjectMaterialGenerationService.cpp:25`, `ProjectMaterialGenerateCommandlet.cpp:109`).
- **E5** The host transaction (`scripts/ue/material/run_material_generation.ps1`, 393 lines;
  `-Mode Validate|Regenerate`, `-CleanupOrphans`, `-TimeoutSeconds` 30-3600) takes the shared
  content lock (`scripts/ue/generated_content/generated_content_mutation_lock.ps1`, the same lock
  ProjectWorld takes), refuses a running editor for this project, recovers a leftover journal,
  snapshots outputs and manifest, runs a hidden `UnrealEditor-Cmd -NullRHI` commandlet with a
  timeout, checks the receipt, rotates `Current`/`Previous`/`Rejected` evidence, and restores exact
  bytes (or exact absence) on failure. It keeps one `RollbackPrevious` bundle but has no mode that
  applies it. The README forbids invoking the commandlet (`-run=ProjectMaterialGenerate`) directly.
  The receipt's `authentication_sha256` is a plain SHA-256 consistency hash, not authentication.
  The latest receipt (`Saved/Validation/MaterialGeneration/Current/host.receipt.json`, 2026-09-09)
  is a regenerate with 0 generated and 2 skipped.
- **E6** Orphan handling scans files under `Content/Generated`. `-cleanup` deletes only packages
  with no referencers, but whether the commandlet's registry sees referencers in other plugins is
  unproven, and no test keeps a referenced orphan (`ReportAndCleanOrphans`,
  `ProjectMaterialGenerationService.cpp:289-325`).
- **E7** The manifest is not byte-stable across checkouts. `ProjectMaterialManifest::Serialize`
  writes `LINE_TERMINATOR` (`ProjectMaterialManifest.cpp:124-127`). For
  `accepted.material-manifest.json`, `git ls-files --eol` reports `i/lf w/crlf attr/text eol=lf`,
  and the working-tree SHA-256 (`2a1963ea...`) differs from the index (`4a33f739...`) while Git
  reports no change. ProjectWorld's `Write-ProjectWorldJson` writes LF with no BOM
  (`scripts/ue/world/generated_manifest.ps1:462-477`).
- **E8** ProjectMaterial has one Editor module and no runtime module; it depends on neither
  ProjectCore nor ProjectEditorCore. Packaged games load its assets through soft references
  ([README](../../../Plugins/Resources/ProjectMaterial/README.md)).
- **E8a** The engine moves to 5.8.3 (operator decision recorded as D9 in the Mesh Terrain migration
  task). The semantic identity includes engine version and changelist (E3), and the accepted
  manifest records `5.8.1-56057345`, so both V1 records become stale on 5.8.3 and the next
  generation run regenerates them at the same paths; same-path regeneration rewrites no World
  package (E11).
- **E9** ProjectObject keeps generated definitions beside their sources and marks them with asset
  tags (`bGenerated`, `SourceJsonPath`, `SourceJsonHash`), with no `Generated` folder
  (`DefinitionGeneratorSubsystem.cpp:585-603`). No neutral generation helper exists to reuse:
  ProjectEditorCore holds only definition events and MD5 hash utilities, and the
  ProjectDefinitionGenerator helpers (`FDefinitionAssetManager`, `CleanupOrphanedAssets`) are bound
  to the definition subsystem and depend on ProjectObject and ProjectGAS. ProjectMaterialEditor
  reuses none of them, which is consistent with D3.
- **E9a** Read [ProjectMaterial pitfalls](../../../Plugins/Resources/ProjectMaterial/docs/pitfalls.md)
  before changing the compiler: the launcher's `FPlatformMisc::GetSHA256Signature` asserts (hence
  the private SHA-256); `UMaterialEditingLibrary` instance setters write the value but return
  false; reloading a material in place keeps stale expressions (hence save, unload, GC, reload,
  verify); and a schema-only edit left the fingerprint stale until schemas became UBT
  `ExternalDependencies`.
- **E9b** ProjectMaterial content today (80 assets): `Base` (10), `Effect` (4, always cooked),
  `Function` (7: `MF_TextureCoordinates`, `MF_TextureNormal`, `MF_TextureColor`,
  `MF_WorldAlignedTexture`, `MF_WorldUvTiling`, `MF_Global_Wind_Intensity`, `ScaleUVbyObject`),
  `Generated/Terrain` (2), `MaterialLayer` (30, object surfaces used by ProjectElement and
  ProjectObject), and `TODO` (27 legacy masters used by City17). Authored functions such as
  `MF_WorldAlignedTexture` are candidates for world-aligned terrain texturing. Any authored
  dependency of a generated asset must enter its semantic identity by package hash, as the parent
  package hash already does (E3).
- **E9c** The host wrapper duplicates World helpers with different behaviour: `Write-JsonAtomic`
  (UTF-8 with BOM) against `Write-ProjectWorldJson` (LF, no BOM), `Get-FileSha256OrNone` against
  `Get-ProjectWorldFileSha256`, and `Restore-MaterialSnapshot` against the World journal functions.
  Consolidation is not in scope (Non-goals).

### Verified facts - consumers

- **E10** ProjectWorld binds a hard-coded path: `ProjectWorldPresentationMaterialBinding.cpp:22-25`
  fixes the object and manifest paths, and `ResolveTerrain` (`:65-186`) authenticates the manifest
  record, package hashes (ProjectCore `FProjectSha256::HashFile`), and parent. Presentation
  profiles may name only road, building, and cloud materials, and only `/Engine/` paths
  (`ProjectWorldPresentationProfile.cpp:173-185`, `IsEngineMaterialPath` `:117-120`).
  `Project.World.Realization.Presentation.ProfileContract` asserts the V1 path
  (`ProjectWorldPresentationTests.cpp:93-98`); `ProjectWorldNativeTwinTests.cpp:198-213` asserts
  that "A material-only migration updates zero terrain components". The terrain material also
  reaches the non-Landscape fallback geometry (`ProjectWorldRealizationService.cpp:524-527`, with a
  `WorldGridMaterial` default in `ProjectWorldGeneratedGeometry.cpp:368-379`), and realization
  evidence records its manifest SHA-256 (`terrain_material_manifest_sha256`,
  `ProjectWorldRealizationService.cpp:836`), so E7 makes that evidence field checkout-dependent.
- **E11** 428 World Partition external-actor packages reference `MI_ProjectTerrain_Default`:
  Kazan territory 211, Manhattan 211, P0 3, Representative 3 (name-table scan under
  `Plugins/World/ProjectWorldData/Content/__ExternalActors__/Generated/`). A reference migration
  dirties the root and every proxy; same-path tuning dirties none
  ([Territory generation](../../../Plugins/World/ProjectWorld/docs/territory_generation.md)).
- **E12** The developer payload and mirror verify material authority:
  `Plugins/Resources/ProjectMaterial/Tools/verify_material_authority.py`
  (`compute_recipe_sha256`, `verify_material_authority`),
  `scripts/git/mirror/compose_developer_payload.py` (`collect_material_authority`, `:257-320`),
  `scripts/git/mirror/developer_asset_release.json:74-84`, `audit_developer_payload.py:17`
  (ProjectMaterial is a required owner), and `scripts/git/mirror/tests/test_developer_payload.py`
  (`test_public_asset_authority_selects_every_generated_definition_pair`,
  `test_public_asset_authority_rejects_material_recipe_semantic_drift`). The recipe hash is
  recomputed outside the engine, so a recipe or manifest shape change lands in both places at once.
- **E13** The Shipping Water proof classifies Water against terrain pixels in `SCS_BaseColor` with
  the external temporal verifier's fixed thresholds
  ([Territory generation](../../../Plugins/World/ProjectWorld/docs/territory_generation.md)), so the
  terrain base colour is one of its inputs.
- **E14** The generated Landscape paints no weight layers and creates no `LandscapeLayerInfo`
  objects: it imports with an empty layer array (`ProjectWorldLandscapeRealization.cpp:784-798`).
  The canonical terrain vertex spacing is 30 m.
- **E15** The project cooks `PCD3D_SM5` and `PCD3D_SM6` (`Config/DefaultEngine.ini`). City17's
  `M_Landscape` exceeds the SM5 sampler limit and Shipping substitutes the default material; that
  legacy fix path is dropped (D14). The same failure class must not reach v2 (M-6).
- **E16** The packaged frame budget is physical RTX 4070, High, 1440p, 60 FPS, with RTX 3060 Medium
  1080p as a separate shipping qualification
  ([World Partition](../../../Plugins/World/ProjectWorld/docs/world_partition.md#kazan-reconstruction-decision-state)).

### Verified facts - Mesh Terrain material surface (installed/source 5.8.3)

- **E17** The channel material expressions live in the Editor module `MeshPartitionEditor`
  (`Engine/Plugins/Experimental/MeshPartition/Source/MeshPartitionEditor/Public/MeshPartitionMaterialExpressionUtils.h`):
  `UMaterialExpressionMeshPartitionResource` (the `Texture2DArray` channel texture),
  `...Texcoord`, `...ChannelSample` (an `FName Channel` plus a `UMeshPartitionDefinition`
  reference used to resolve the index), `...ChannelSampleIndex` (a plain index with no definition
  reference), and `...Inspector`.
- **E18** Each compiled section gets a material instance parented to the definition's `Material` in
  a single slot (`MeshPartitionEditorComponent.cpp:547-549`). UV0 is the channel UV (only one,
  auto-unwrapped, UV channel exists) and custom primitive data 0-5 is reserved for the channel
  table. Channels are 8-bit slices, at most 24, resolution about section size / `ChannelTexelSize`
  (default 100 cm), capped at 4096 px; the per-vertex source weights are interpolated into those
  slices, and only channels with non-zero weight get a slice. The plugin never sets material usage
  flags: Epic's shipped Mesh Terrain materials are `MD_Surface` with `bUsedWithNanite` and use
  material-cache and virtual-texture outputs, so v2 sets its own usage flags. Epic's own content
  includes `MLB_MT_Blend` despite E19.
- **E18a** Ways to write channel weights: named `FDynamicMeshWeightAttribute` layers on base meshes,
  texture-patch weight entries, `UWeightUtilityModifier`, `UPatchModifier` or `UNoiseModifier`
  with `bWriteToWeightChannel`, and `USimpleWriteModifier`. Texture-patch entries may not be
  callable from a project module (`UTexturePatchEntry` has no API macro; unverified). Channels can
  also drive per-triangle physical materials (`FPhysicalMaterialChannel`,
  `MinimumCollisionRelevanceWeight`); grass is reachable only through PCG ("Get Mesh Partition
  Grass Types" reads the material's grass types). An Epic knowledge-base author calls the channel
  sample "the MeshTerrain equivalent of LandscapeLayers but more versatile"; Landscape layer nodes
  do not read Mesh Terrain channels.
- **E19** Epic tracks UE-391544: channel sampling returns 0 inside Material Layer Blend assets
  (Epic forum, 2026). Epic staff: "for anything you need to ship, Runtime Virtual Texture is the
  supported option today" while the Mesh Terrain virtual-texture layer is in development.
- **E20** Epic's UE 5.8 Mesh Terrain material documentation confirms the two relevant coupling
  choices: sample by name only after selecting a Mesh Partition Definition, or sample directly by
  channel index. That supports the index adapter in the target shape, but makes stable channel order
  and equality checks mandatory.
  [Mesh Terrain Material](https://dev.epicgames.com/documentation/unreal-engine/mesh-terrain-material-in-unreal-engine).
- **E21** Epic's UE 5.8 sampler API states that a texture-asset sampler consumes a unique sampler
  slot, while wrap/clamp world-group shared samplers do not. Shared sampling can address the SM5
  slot failure class, but it does not prove instruction, texture-fetch, memory, or frame budgets;
  the deliberately over-sampled cook control and packaged measurement remain required.
  [Sampler source modes](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/SamplerSourceMode).
- **E22** Epic documents world-aligned/triplanar projection as independent of mesh UV consistency
  and provides both world-aligned texture/normal functions. It does not publish an ALIS-specific
  cost bound, so T1 must record actual graph fetches/permutations and T6 must measure cooked output.
  [Texturing material functions](https://dev.epicgames.com/documentation/unreal-engine/texturing-material-functions-in-unreal-engine).
- **E23** Repository-wide text search found the named City17 texture candidates only as binary
  assets/references and in this todo, not in a provenance or license ledger. ProjectTexture is the
  correct promotion owner, but promotion cannot manufacture redistribution rights. Q1 therefore
  remains genuinely open and A7 fails closed.
- **E24** Two primary-source candidate libraries explicitly permit raw redistribution under CC0:
  Poly Haven says all of its assets are CC0 and may be redistributed, including in a sold product;
  ambientCG says all downloadable assets are CC0 and raw files may be included in a game project.
  They are viable T1 source pools, not blanket approval of an unrecorded download: each selected
  asset still records its exact asset page, downloaded revision/hash, license snapshot, component,
  and optional credit under ALIS policy.
  [Poly Haven license](https://polyhaven.com/license),
  [ambientCG license](https://docs.ambientcg.com/license/),
  [CC0 legal code](https://creativecommons.org/publicdomain/zero/1.0/legalcode.en).
- **E25** UE 5.8.3 does not hash material package contents in
  `UMeshPartitionDefinition::GatherDependencies`. The implementation explicitly says material
  contents are not used to build sections and adds only `Material.Get()->GetPathName()`; it does hash
  modifier priorities, channel layout/texel policy, build variants, transformer dependencies, and
  physical-material channel settings (`MeshPartitionDefinition.cpp:81-115`). Epic's API states that
  `GatherDependencies` participates in deciding whether compiled sections are current, but neither
  that API page nor source inspection proves the real builder/cook no-write result for a same-path
  material content change. T1 and the Mesh Terrain Phase 1 probe therefore measure it.
  [UMeshPartitionDefinition API](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/MeshPartition/UMeshPartitionDefinition).
- **E25a** The later real builder/cook control closed that uncertainty: changing material contents
  at the same object path reused every affected section and preserved World package hashes, while
  changing the material object path rebuilt them. The accepted material system preserves this
  locality and reruns the discriminating control after changing the generated parent backend.
- **E26** The accepted boundary exists in code today. `ProjectWorld.Build.cs` and
  `ProjectWorldEditor.Build.cs` name no ProjectMaterial module; repository search finds no
  ProjectMaterialEditor or material-generation host invocation under ProjectWorld,
  ProjectWorldData, CanonicalCompilation, or the World realization scripts. ProjectWorld's current
  presentation binding authenticates material authority through data and assets instead of a
  compiler API (E10). The Mesh Terrain plan must preserve that zero-dependency boundary.

### Verified facts - universal material, cache, and storage direction (2026-09-27)

- **E27** ProjectTexture is not yet a generation system. Its only source module is a Runtime stub
  with `Core`, `CoreUObject`, and `Engine`; it has no recipes, schemas, manifests, cache ownership,
  or editor compiler. Its content is substantial: 110 tracked binary texture assets total
  622,793,241 bytes (593.94 MiB), of which the `Pattern` subtree alone is about 403.5 MiB.
  ProjectMaterial's 80 tracked binary assets total 3,100,256 bytes (2.96 MiB). The risk is therefore
  already hundreds of MiB of reusable texture content, not a merely theoretical future concern.
- **E28** The accepted Mesh Terrain Development package inspected under the project scratch area
  contains 1.163 GiB in `pakchunk10-Windows.ucas`, 0.421 GiB in `pakchunk0-Windows.ucas`, a
  0.339 GiB executable, and a 0.147 GiB pak. These files do not provide per-owner cook attribution,
  so they establish only the current product scale. The final terrain A/B must produce an
  owner-attributed cook delta before claiming that a backend reduces package size.
- **E29** Texture Graph 5.8.3 is `VersionName: 1.0 Beta`; its descriptor declares
  `TextureGraphEngine` and `TextureGraph` as Runtime modules, and the public Blueprint surface
  exposes synchronous and asynchronous rendering to `UTextureRenderTarget2D`. Those facts explain
  why it initially looked suitable for packaged runtime generation
  (`TextureGraph.uplugin:4,18-39`; `TG_BlueprintFunctionLibrary.h`; `TG_AsyncRenderTask.h`).
- **E30** The installed implementation contradicts that inference. `TextureGraphEngine::InitEngineInternal`
  says "Currently we're only supporting editor only" and places device, blob, RenderDoc, mix,
  scheduler, error-reporter, and observer initialization inside `#if WITH_EDITOR`
  (`TextureGraphEngine.cpp:131-164`). Destruction, tests, update, and related lifecycle paths repeat
  the same boundary. A Runtime module classification is therefore not a Shipping-runtime proof.
- **E31** Texture Graph's public async-render activation calls `Batch->SetNoCache(true)`
  (`TG_AsyncRenderTask.cpp:42`). An ALIS hash map retaining its returned render targets would own
  deduplication, lifetime, eviction, memory pressure, mip readiness, and failure behavior; Texture
  Graph does not make that a bounded production cache automatically.
- **E32** UE 5.8.3 contains a separate renderer Material Cache. Support is a read-only project
  switch, `r.MaterialCache.Support`, default false; runtime enable is a scalability switch,
  `r.MaterialCache.Enabled`, default true once support exists. Support also requires virtual
  texturing (`MaterialCache.cpp:9-68`). Its virtual-texture allocator processes passive LRU
  evictions and records allocation, failure, tile mapping, reallocation, and LRU-eviction stats
  (`MaterialCacheVirtualTextureAllocator.cpp:144-168,217-250,560-603`). This is a real bounded
  page cache, not an unbounded render-target registry.
- **E33** Mesh Partition integrates directly with that Material Cache. When enabled, every compiled
  section calls `UpdateMaterialCacheTextures` for its generated material instance
  (`MeshPartitionCompiledSection.cpp:192-194`). The helper discovers material-cache tags from the
  material, creates `UMaterialCacheVirtualTexture` resources for them, and enables shared shading
  (`MeshPartitionMaterialCacheCommon.cpp:26-58`). Renderer and Nanite source contain dedicated
  material-cache raster/page paths. This is the closest installed technology to D17's
  Nanite-principled material reuse, but it has no current public workflow documentation and still
  needs packaged ALIS proof.
- **E34** RVT solves a different problem. Epic documents it as generating texel data on demand and
  caching shading data over large areas; its physical pools are fixed-size, per-format LRU page
  caches. UE 5.8's Mesh Terrain documentation provides an RVT transformer for both Preview and
  Build pipelines, but labels the Mesh Terrain integration Experimental and warns to use caution
  when shipping. RVT remains the candidate for world-space cross-primitive composition, not the
  default ProjectTexture pattern store.
  [Runtime Virtual Texturing](https://dev.epicgames.com/documentation/en-us/unreal-engine/runtime-virtual-texturing-in-unreal-engine),
  [Virtual Texture Memory Pools](https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-texture-memory-pools-in-unreal-engine),
  [RVT and Mesh Terrain](https://dev.epicgames.com/documentation/en-us/unreal-engine/runtime-virtual-textures-and-mesh-terrain-in-unreal-engine).
- **E35** Substrate is the forward material model in UE 5.8: Epic says it is enabled by default in
  new projects, represents principled slabs of matter, and can simplify by platform. Existing
  non-Substrate roots are converted at compile time when the project opts in. Epic also states that
  platform support/testing is incomplete, features and UX can invalidate assets, and Adaptive
  GBuffer increases cook time and regresses runtime performance relative to Blendable GBuffer for
  the same project. This supports a generated replaceable backend and a measured opt-in, not blind
  use of every Substrate mode.
  [Substrate overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-substrate-materials-in-unreal-engine).
- **E36** ALIS already enables virtual textures but disables Substrate, and it cooks both
  `PCD3D_SM5` and `PCD3D_SM6` (`Config/DefaultEngine.ini:98,183,408-410`). Substrate opt-in is a
  project-wide rendering-contract change. Material Cache support is not configured. Neither can be
  treated as a local graph edit.
- **E37** Epic's material-instance contract matches the desired storage/permutation shape:
  parameterized instances vary appearance without recompiling the parent, while every used static
  parameter combination compiles a separate shader and can create excessive permutations. The
  domain should therefore use a small family of parents, ordinary scalar/vector/runtime-resource
  parameters for variation, and a reviewed finite list of structural compile-time variants.
  [Material Instances](https://dev.epicgames.com/documentation/en-us/unreal-engine/instanced-materials-in-unreal-engine).
- **E38** Runtime render targets are not free compression. A 2048x2048 RGBA8 target contains
  16 MiB at mip 0 and about 21.33 MiB with a full mip pyramid; a three-target structural bundle is
  about 64 MiB per resident recipe before allocator/object overhead. At 4096x4096 the same bundle
  is about 256 MiB. This arithmetic refutes an unbounded "generate every recipe once and retain it"
  cache even before cold-start generation cost is measured.
- **E39** The stable architecture boundary already supports this direction: ProjectMaterial owns
  universal material graphs, instances, recipes, generated assets, and its editor compiler;
  ProjectWorld authenticates and binds a semantic identity; ProjectWorldMeshTerrain owns the MPD
  and private channel layout; ProjectWorld has no ProjectMaterialEditor dependency. The new work
  extends those owners rather than moving material authority into World or generated assets.
- **E40** Current material coordinates are already metric and world-aligned for terrain.
  `ProjectMaterialSurfaceBuilder.cpp` creates `UMaterialExpressionWorldPosition`, divides by 100 to
  convert Unreal centimeters to meters, divides again by `PatternScaleMeters`, and samples XY, XZ,
  and YZ before blending by normalized absolute world normal. UE's
  `Engine/Source/Runtime/Engine/Public/Materials/MaterialExpressionWorldPosition.h` defines the
  default mode as world position, including material shader offsets. The terrain recipe and default
  instance both set `PatternScaleMeters=96.0`; the object cube recipe uses
  `object_metric_triplanar_2d` and `PatternScaleMeters=0.5`. The material scalar contract admits
  0.05 through 100.0. No Landscape or Mesh Terrain UVs determine this material scale.
- **E41** The complete sampled Structure tile period is the material scalar: normalized texture
  coordinates are world meters divided by `PatternScaleMeters`, and the render target wraps in U/V
  with wrap-addressed mips. At the terrain value, the full output recurs every 96 m along each
  projected axis. This is separate from the noise components' nominal periods inside that full tile.
- **E42** `ProjectTextureRuntimeAssetBuilder.cpp` sets `bTiling=true` and those repeat sizes on
  native noises at 4 (Gradient ALU), 12 (Gradient ALU), 24 (Voronoi ALU), and 64 (Value ALU), then
  derives the final RGB signals. UE's
  `Engine/Source/Runtime/Engine/Public/Materials/MaterialExpressionNoise.h` documents which native
  noise functions tile and defines `RepeatSize` as units per tile. Source inspection gives nominal
  ground periods of 24, 8, 4, and 1.5 m at `PatternScaleMeters=96`. `GroundDetail` includes all three
  12/24/64 contributions;
  `CoverPatch` combines 4/12. `Plugins/Resources/ProjectTexture/docs/pattern_generation.md`
  currently omits the 24-repeat contribution to `GroundDetail` and needs a factual correction when
  implementation is authorized.
- **E43** The packaged fixed-view route accepts fixture mesh/material, start/final location, and
  rotation, then captures one final image. It has no actor-scale or material-scalar override; the
  gate assigns the material directly instead of creating a dynamic instance. Its receipt reports
  component world bounds but not local mesh bounds, actor scale, or applied scalar. The accepted cube
  receipt therefore establishes no scale sweep and its distant final screenshot gives no calibrated
  close-face tile count (`scripts/ue/world/test/capture_packaged_fixed_view.ps1`,
  `ProjectWorldFixedViewGate.cpp`, and `tmp/material/runtime-pattern-cache/cube-proof-current/capture/fixed-view.json`).
- **E44** The 2026-09-29 post-RGB Development close/player/oblique views show broad breakup without
  an obvious fallback or projection seam; they do not span a calibrated 96 m period and cannot
  establish visible repeat recurrence. The existing mip-0/mip-4 seam test measures wrap-boundary
  continuity, not whether repeated motifs are recognizable.
- **E45** Installed UE 5.8 source confirms the current sampling path does not tile at a Mesh Terrain
  section boundary. `Plugins/Resources/ProjectMaterial/Source/ProjectMaterialEditor/Private/ProjectMaterialSurfaceBuilder.cpp`
  builds texture coordinates from the selected world/object position in meters and divides by
  `PatternScaleMeters`; the three texture samples use the texture-asset sampler by default. UE's
  `Engine/Source/Runtime/Engine/Public/Materials/MaterialExpressionTextureSample.h` says that sampler
  mode uses the UTexture address settings, and
  `Engine/Source/Runtime/Engine/Private/Materials/HLSLMaterialTranslator.cpp` obtains those settings
  and applies wrap addressing to large-world coordinates. ProjectTexture's
  `Plugins/Resources/ProjectTexture/Source/ProjectTextureEditor/Private/ProjectTextureRuntimeAssetBuilder.cpp`
  sets both output axes and mip axes to `TA_Wrap`. Thus a full output repeats each time a projected
  coordinate advances by one UV unit, i.e. every `PatternScaleMeters`; WorldPosition has no
  section-local reset. Mesh sections share the same world-space field. The repeated tile and any
  discontinuity at a section boundary are distinct properties and need separate visual checks.

### Verified facts - public payload and orphan safety (2026-09-30)

- **E46** `scripts/git/mirror/developer_asset_release.json` declares one `material_manifest`
  authority at the V1 `Data/Manifests/Materials/accepted.material-manifest.json` with recipe root
  `Data/Materials`; no ProjectTexture authority exists. `collect_material_authority` in
  `scripts/git/mirror/compose_developer_payload.py` loads
  `Plugins/Resources/ProjectMaterial/Tools/verify_material_authority.py`, deleted in `54de1c9ad`,
  and counts only `*.material.json`. The contract schema
  `scripts/git/mirror/contracts/developer_asset_release.schema.json` enumerates the same V1 kind.
  `compose()` raises `Required payload file is missing`; 6 of the 13
  `scripts/git/mirror/tests/test_developer_payload.py` tests that completed fail with it, and both
  material tests still read V1 paths. `scripts/ue/package/prepare_release_inputs.ps1` runs this
  composer through `audit_developer_payload.ps1`.
- **E47** `audit_developer_payload.py` seeds only `REQUIRED_OWNERS = {ProjectWorldData,
  ProjectExperienceData, ProjectMaterial}`, and `classify_dependency` rejects any required project
  package not selected by an approved authority (`rejected_required_project_content`). Every
  accepted surface record names
  `/ProjectTexture/Patterns/Terrain/DA_ProjectTerrainStructureCatalog.RT_ProjectTerrainStructure`
  as its pattern, so the public map closure needs a ProjectTexture seed.
- **E48** Both owners' `recipe_sha256` is SHA-256 of a C++ string built field by field from the
  parsed recipe (`ProjectMaterialSurfaceRecipe.cpp` `Normalize` at 211 and line 408;
  `ProjectTexturePatternRecipe.cpp` `Normalize` at 184 and line 288), and it feeds the artifact
  semantic identity. Neither manifest records a digest of the recipe file itself. The payload's
  `generated_definition_manifest` kind already authenticates source JSON by the SHA-256 of its
  UTF-8, BOM-stripped, LF-normalized bytes (`normalized_json_sha256`).
- **E49** Each owner rewrites its manifest only when an output was generated, the record count
  changed, or the manifest is missing (`ProjectMaterialSurfaceGenerationService.cpp:521`,
  `ProjectTextureGenerationService.cpp:393`). A record field that changes while every asset is
  skipped is not persisted.
- **E50** Each compiler fingerprint hashes every non-test `.cpp`/`.h` of its editor module, its
  `Data/Schemas/*.json`, and its `Build.cs` (`ProjectMaterialEditor.Build.cs` and
  `ProjectTextureEditor.Build.cs`, `GetCompilerFingerprintFiles`). Any owner code or schema edit
  therefore makes every output of that owner stale once.
- **E51** Both owners' `HandleOrphans` call `IAssetRegistry::GetReferencers` before deleting an
  unreferenced orphan (`ProjectMaterialSurfaceGenerationService.cpp:283-320`,
  `ProjectTextureGenerationService.cpp:233-260`); neither owner calls `SearchAllAssets` or a scan.
  Installed UE 5.8.3 `ShouldSearchAllAssetsAtStart` (`Engine/Source/Runtime/AssetRegistry/Private/AssetRegistry.cpp:11135-11197`)
  searches at startup for a non-cook commandlet only when it is listed in `[AssetRegistry]
  CommandletsUsingAR` or `-AssetGatherAll=true` is passed; ALIS config and
  `scripts/ue/material/run_material_generation.ps1` set neither. No current ProjectMaterial or
  ProjectTexture test covers orphans. World exact restore after rejection is covered by
  `scripts/ue/world/test/generated_content_transaction.Tests.ps1`.
- **E52** The V1 paragraph in `material_generation.md` that required binary transport to verify
  recipe authority was removed with V1; neither owner document now states a transport contract.
  `scripts/git/mirror/README.md` describes selection generically and remains accurate.
- **E53** Current exact tests are `Project.Material.Generation.PatternAuthority`,
  `SurfaceAssetCompilerContract`, `SurfaceManifestContract`, `SurfaceRecipeContract`,
  `SurfaceRecipeKnownBad`, `Project.Material.MeshTerrain.LayoutReceipt`,
  `Project.Material.Substrate.MinimalCompile`, and nine `Project.Texture.*` tests; the host tests
  are `run_surface_generation.Tests.ps1` and `run_pattern_generation.Tests.ps1`. The twelve
  `Project.Material.Generation.*` names in Green evidence before this investigation were V1 tests
  and no longer exist.
- **E54** `test_public_credential_patterns_match_literals_not_code_expressions` runs `bash -lc`.
  Here a login shell did not return within 20 s while `bash -c` returned at once and the pattern
  matched, so the test depends on the invoking user's login profile, not on payload code.
- **E55** Q8 reachability (read-only, inventory of `4b98d71f9` plus the T10 tree): the transitive
  closure of the five accepted surface and pattern packages is 7 packages and contains no rejected
  package, both in the audit's own inventory and in a full-registry inventory. Every rejection is
  reached from World `__ExternalActors__` of the Kazan gameplay layer, the Manhattan vegetation and
  terrain layers, or one Kazan map actor, through ProjectObject tree and water-bottle assets or the
  shared Mesh Terrain MPD.
- **E56** `scripts/ue/check/assets/export_public_dependency_inventory.py:30` scans only the seed
  artifacts, and a commandlet does not gather the registry at startup (E51), so an unselected
  dependency outside a startup-scanned path reads as `rejected_missing_package` and its own
  dependencies are never traversed. 18 of the 20 "missing" packages are present and tracked; the
  two `/Fab/` textures are engine Fab-plugin content. A copy of the exporter with
  `search_all_assets(True)` (`tmp/material/t10/q8/`, repository unchanged) gives 0 missing and 45
  rejections, all `rejected_required_project_content`: ProjectMaterial 15 (11 object
  material-layer packages, 3 functions, `Base/M_ObjectUv_Compact`; the layer system is a Non-goal
  of this task), ProjectTexture 5 (`Base/T_*`), ProjectObject 22 (AmurCork and Hornbeam trees, the
  water-bottle family), ProjectElement 1, ProjectWorldData 1 (fixture marker map), and
  ProjectWorldMeshTerrain 1 (`MPD_ProjectTerrain_Shared_v1`). The audit still fails closed; its
  reason codes and count were wrong.
- **E57** Cleanup correctness, both owners: `HandleOrphans` skips an unreferenced orphan whose
  package file fails to resolve, falls outside the output root, or fails `IFileManager::Delete`
  (`ProjectMaterialSurfaceGenerationService.cpp:341-349`, `ProjectTextureGenerationService.cpp:291-299`)
  and the run then saves the manifest without that record. In the host, `finally` removes the
  operation folder that holds the snapshot the journal names (`run_material_generation.ps1:243-244`,
  `:440-442`); when the in-process rollback itself throws, the journal survives its snapshot and
  the next run's recovery removes the output root and then fails to copy the missing snapshot.
  Only the commandlet log names retained orphans and their referencers; `commandlet.receipt.json`
  lists every orphan without separating retained from deleted, `host.receipt.json` lists none, and
  production evidence keeps only `Saved/Validation/<domain>/Current` and `Previous`.

### Inferences

- Per-vertex channel weights at 30 m spacing are coarse for footprint edges. Sharper surface
  boundaries need texture-patch weight entries or local tessellation, each with a cost to measure.
- Sampling channels by index keeps ProjectMaterial free of any reference to territory content,
  provided the channel order has exactly one owner.
- Moving the path once, together with the terrain regeneration, avoids rewriting every terrain
  package twice.
- The accepted ProjectTexture runtime cache removes repeated per-pixel procedural noise evaluation
  while sharing one sampled output per structural identity. Its absolute frame/GPU cost is measured;
  its differential frame/GPU benefit over the removed direct arm remains unverified because the
  historical CSV collector failed before sampling either arm.
- Mesh Partition Material Cache and RVT have different future duties, but neither is selected here:
  this slice has no demonstrated need for per-primitive evaluation caching or cross-primitive
  world-space composition.
- The lowest-cardinality durable shape is one parent per projection/cache/output-layout family,
  persistent instances only for stable cook/reference identities, and dynamic instances for
  ephemeral runtime variation. Exact family count must come from recipe census, not a guessed
  master-material framework.

### Assumptions and unverified areas

- Orphan retention covers references recorded in on-disk packages after one registry search; it
  cannot see references held only in memory or in a transaction snapshot, which is why the host
  refuses cleanup while a restore source exists. The public World projection holds no content lock,
  so a projection that starts during a cleanup is outside that check; the projection recovery task
  owns making the projection isolated (M-10 residual).
- Whether `-NullRHI` generation can validate cooked shader compilation; cook-time compilation is
  the expected proof route.
- The terrain feature size that should be preserved while selecting a terrain tile period.
- Export/link viability of the proposed wrapper and final generated-asset integration; source
  inspection proves the API shape but not an ALIS target build.
- Current-source Shipping appearance/quality acceptance and a valid frame/GPU timing instrument.
- The current packaged fixed-view gate has no runtime material-scalar override. A test-only generic
  scalar-parameter path (or an equally non-persistent candidate route) is needed to compare scales
  without generating one persistent material instance per candidate.
- A calibrated 1 m close-box view and a terrain view spanning at least two full candidate periods;
  current screenshots do not provide either measurement.
- Cold-start time and per-view runtime-cache measurements remain release/performance work, not a
  prerequisite for the scale mapping proof.
- Whether a later UE version makes Texture Graph's non-editor runtime lifecycle complete. UE 5.8.3
  does not.

### Refuted

- "An artist-facing Blueprint or Material-Layers terrain setup exists for generated terrain" - the
  generated path is fully C++-generated: no Blueprint, no Material Layers, no layer infos (E2, E14).
  ProjectMaterial's `MLB_*` and `ML_*` assets serve object surfaces.
- "`UMeshPartitionDefinition::GatherDependencies` hashes the material contents, so same-path tuning
  necessarily invalidates compiled sections" - false in UE 5.8.3. It records the material path only
  (E25). This refutes the claimed mechanism, not the reviewer's requested runtime probe: package-hash
  and builder/cook behavior still need direct evidence.
- "Texture Graph is already a packaged runtime cache because its modules are Runtime and its API
  returns render targets" - false for installed UE 5.8.3. Core engine initialization is editor-only
  (E30), and the async path disables Texture Graph caching (E31).
- "Runtime generation automatically saves storage without another cost" - false. Resident render
  targets consume uncompressed GPU memory unless a bounded page system owns them; E38 shows that a
  small set of 2K/4K bundles can exceed the cooked asset bytes it replaces.
- "RVT, Material Cache, Texture Graph, and a custom virtual-texture producer are interchangeable
  cache backends" - false. They respectively cache world-space composed shading, per-primitive
  material evaluation, editor procedural synthesis in 5.8.3, and low-level requested pages. Their
  ownership, invalidation, and failure domains differ.
- "Using every beta/experimental feature is future-proof" - false. D16 requires current advantage,
  an ALIS-owned replaceable contract, and executable proof. The forward direction is preserved by
  recipes and adapters, not by coupling authority to every engine experiment.
- "Terrain lacks world-aligned metric scaling" - false for the current material graph. E40 shows
  that terrain already uses world position, centimeter-to-meter conversion, and triplanar world-axis
  sampling. The unresolved issue is whether the chosen tile period and feature sizes look right.
- "Wrap addressing and passing mip seam tests mean the texture does not visibly tile" - false. E41
  and E42 show that the data is intentionally periodic; seamless boundaries prevent a hard seam but
  the complete tile still repeats at `PatternScaleMeters`.
- "Mesh Terrain's first section/part owns one tile, so the next part restarts it" - false for this
  material. The graph uses world position and has no section-local UV input; UE's texture sampler
  wraps after one full output period in UV, which maps to `PatternScaleMeters` in world meters (E45).
  Section-boundary continuity still requires its own check.
- "The rejected 2/4/16 m second sample rules out primary material-scale changes" - false. That
  candidate resampled only B as appearance modulation; it did not vary the main
  `PatternScaleMeters` input controlling the full Structure tile.
- "M-10 and the rollback drill protected only the closed V1 migration window" (the final delta R2
  record) - false for M-10. Any later recipe rename or removal with `-CleanupOrphans` can orphan a
  package that restored or current World packages reference, and the guard against that is
  unproven in the commandlet (E51). It remains true for the rollback drill (Q7).
- "A generated `DetailNormal` output solves the current tiling issue" - unsupported and off-target.
  It could add close normal response, but it does not change the current Structure recurrence and
  would add a new output/consumer path. Keep it out unless that separate quality gap is demonstrated.

## Current architecture and source of truth

```text
ProjectTexture accepted Structure output + Mesh Terrain layout receipt
                         + ProjectMaterial surface recipes
                                      |
                                      v
                      ProjectMaterialEditor compiler
                                      |
                                      v
              generated parent/instance + accepted manifest
                                      |
                                      v
             ProjectWorld authenticates and binds the material
                                      |
                                      v
        ProjectWorldMeshTerrain shared MPD + MeshPartition sections
```

CanonicalCompilation owns canonical terrain facts and surface semantics. ProjectTexture owns reusable
structural signals and its bounded runtime cache. ProjectMaterial owns visual appearance, recipes,
generated parent/instance packages, and the editor compiler. ProjectWorld authenticates and binds the
final material through data; it does not call ProjectMaterialEditor. The ProjectWorldMeshTerrain
adapter owns the shared MPD and private channel layout. See [ProjectMaterial](../../../Plugins/Resources/ProjectMaterial/README.md),
[material generation](../../../Plugins/Resources/ProjectMaterial/docs/material_generation.md),
[ProjectTexture](../../../Plugins/Resources/ProjectTexture/README.md),
[pattern generation](../../../Plugins/Resources/ProjectTexture/docs/pattern_generation.md), and
[territory generation](../../../Plugins/World/ProjectWorld/docs/territory_generation.md).

## Capability gap

The generated material system now has its first terrain and movable-object consumers. The specific
open visual question is whether the accepted Structure tile has a suitable physical scale and whether
its exact periodic repetition is visible in the intended views. This is not currently evidence of a
missing world-aligned projection: `BuildCoordinates` uses Unreal world position in centimeters,
converts to meters, and the terrain graph divides by `PatternScaleMeters` before its three world-axis
samples. The engine's stock `WorldAlignedTexture` is also a world-unit/triplanar function, but it
does not remove the repetition period of the input tile.

One material scalar currently couples complete tile period and every embedded feature scale. With
`PatternScaleMeters=96`, the complete output repeats every 96 m per projection axis; its native noise
frequencies nominally introduce 24 m, 8 m, 4 m, and 1.5 m structures. Reducing the scalar makes all
features smaller and the complete tile recur more often; increasing it enlarges all features while
extending the full-tile period. A parameter-only change may fix the apparent size, but it cannot
independently change tile recurrence while holding all feature scales fixed.

Existing RGB seam tests and wrap/mip settings cover boundary continuity, not recognizable motif
repetition. The current screenshots do not provide a calibrated close view of a known-size box or a
long enough terrain view to identify the full 96 m recurrence. The prior 2/4/16 m experiment changed a
secondary sample of B only; it did not test the primary tile scale. Therefore the smallest next proof
is an isolated box scale comparison, followed by the same accepted output and winning scale on the
actual Mesh Terrain world-metric material. Do not add a detail-normal output unless later evidence
shows that close physical response, rather than periodic repetition, is still the unmet requirement.

## Decision

### Target shape

```text
Domain canonical facts and optional authored overlays  (semantic authority)
  terrain surfaces, object material roles, road types, water state, provenance
        |
        v
ProjectTexture                                         (structural pattern authority)
  closed JSON recipe -> two-node native runtime DAG
  one shared 512x512 linear Structure output per structural identity
  bounded cache, mips, wrap addressing, pins, and unpinned LRU
  no color/scale/cell/city variants and no arbitrary graph DSL
        |
        v
ProjectMaterial                                        (appearance authority)
  closed JSON recipes + accepted ProjectTexture output + Mesh Terrain layout receipt
  generated small parent families + stable instances
  selected UE 5.8 backend: generated Substrate slabs through ProjectMaterialEditor
        |
        v
ProjectWorld data binding -> ProjectWorldMeshTerrain adapter
  shared MPD, private semantic-channel ABI, world-metric terrain sampling
        |
        v
replaceable MeshPartition realization
```

Only canonical/domain owners decide what a surface means. ProjectTexture decides reusable
structure and owns its bounded disposable runtime texture cache. ProjectMaterial decides appearance.
The Mesh Terrain adapter owns engine-specific channel and realization details. Generated Unreal
assets, cache entries, material instances, and render targets contain no unique authored authority
and are reproducible or disposable by their owner.

### Premise / KISS gate

- **Owner:** ProjectMaterial's compiler already owns recipes, deterministic identity, manifest-last
  promotion, and the host transaction (D1, D3). v2 extends that lifecycle.
- **Implemented:** a closed ProjectMaterial surface schema and family table; ProjectTexture's closed
  pattern recipe, native two-node DAG, stable output slot, and bounded runtime cache; the Mesh Terrain
  channel adapter and plain-data layout receipt; generated Substrate parents/instances; and the
  authenticated data-driven ProjectWorld binding. The cache is ProjectTexture-owned and shared per
  structural identity, not one output per territory or consumer.
- **Removed:** the `Generated` segment from identities, schemas, docs, and code; the hard-coded
  consumer path; the V1 archetype, recipes, and assets (A2); folder-based orphan detection; CRLF
  manifest bytes.
- **Capability knowingly given up:** V1's four-parameter prototype look, and the convenience that
  "everything under Generated belongs to the compiler". Ownership is proven from records instead.

### Material, instance, projection, and pattern policy

- A recipe describes a surface concept, never arbitrary Unreal expression nodes. New graph
  capabilities enter through reviewed compiler archetypes and schema versions.
- Parent materials are few and structural: projection frame, cache/output layout, blend model, and
  genuinely compile-time features. Static switches are forbidden for cosmetic variation and every
  allowed static combination is counted in the accepted permutation budget.
- Persistent Material Instance Constants exist only when a stable cook/reference identity is
  required (for example the shared Mesh Terrain MPD material). Dynamic instances own ephemeral
  runtime changes. Neither form owns semantic truth.
- Pattern identity changes only for structural changes such as algorithm, seed, frequency family,
  or output topology. Tint, roughness, wet-looking appearance, metric scale, blend weight, and
  ordinary variation stay ProjectMaterial parameters unless measurement proves otherwise.
- Projection is metric and domain-specific (D18). Static irregular terrain uses world metric 3D;
  movable objects use object-local metric 3D; roads and grain use tangent/direction-aligned
  projection. The dissimilar-surface proof must include movement to detect texture swimming.
- ProjectTexture generates one shared Structure render target per accepted structural identity.
  Its bounded cache deduplicates consumers and disposes/reuses intermediate DAG resources; ProjectMaterial
  samples the output rather than recalculating the structural noise per visible pixel. Material Cache
  is not the selected production cache and remains disabled.
- ProjectMaterial recipes name only a stable ProjectTexture pattern ID; the compiler resolves the
  output path, output ABI, semantic identity, and exact package digest from the accepted ProjectTexture
  manifest. It does not couple to a concrete producer algorithm or compiler version. Neither compiler
  writes the other owner's Content tree, and no ProjectTexture editor/compiler code loads in packaged
  targets.

### Cache roles and selected direction

1. **ProjectTexture runtime DAG/cache** is the selected production path. It generates one shared
   sampled Structure output per authenticated identity and bounds reuse by bytes, pins, failure
   state, and unpinned LRU. Current accepted output is 512x512 RGBA8 with full mips and wrap
   addressing; one ready output uses 1,398,100 bytes and the two-node generation peak is 2,796,200.
2. **Direct procedural material evaluation** was the equivalent packaged control. The accepted cached
   path preserved the terrain read, improved distant filtering, reduced the Development package by
   4,836,032 bytes, and shares one output across 1/10/100 consumers. Direct evaluation was removed;
   it is not a fallback route. Frame/GPU timing is not claimed because the official collector crashed
   before sampling either arm.
3. **Mesh Partition Material Cache** is not selected and remains disabled. Reopen only for a concrete
   measured duty the ProjectTexture cache does not own; it is not an interchangeable replacement for
   the current output cache.
4. **RVT** remains a future option only for world-space composition across primitives (for example
   terrain plus roads, decals, or splines). There is no such requirement in this task.
5. **Texture Graph** may prototype or generate compact editor assets, but cannot be the Shipping
   runtime backend in installed UE 5.8.3 (E29-E31).
6. **Custom `IVirtualTexture` producer and persistent disk cache** are YAGNI. Reopen only with a
   measured miss that neither current ProjectTexture caching nor a concrete RVT use can solve.

### Box-first tiling and metric-scale decision

- **Owner:** ProjectTexture owns the periodic structural field and its frequencies. ProjectMaterial
  owns the meter-valued `PatternScaleMeters` and projection frame. The current ProjectWorld packaged
  fixed-view fixture can load a simple cube and generated material without changing World realization.
- **Stage 1 - simple box:** use the same accepted Structure output and the existing
  `/ProjectMaterial/Surfaces/Object/MI_ProjectMetricCube` object-local material on an axis-aligned
  cube. Record the mesh local bounds and actor scale; use an unscaled 1 m face as the ruler only
  after both confirm it.
  Compare `PatternScaleMeters` values 0.25, 0.5, and 1.0 in the same close camera and lighting. A
  1 m face should show four, two, and one complete Structure periods, respectively. Add only an
  opt-in, generic scalar-parameter override to the fixed-view fixture so each candidate uses the
  same packaged material and output path; record the applied parameter/value, local mesh bounds,
  and actor scale in the receipt; create no persistent candidate MI. Do not change any other
  material parameter or create a texture, parent, or per-scale material identity. If normal
  appearance does not expose the tile count, use the existing base-color-isolated view; if that is
  still inconclusive, stop as unverified rather than adding a diagnostic graph.
- **Stage 2 - actual Mesh Terrain:** only after Stage 1 proves the meter-to-output-period mapping,
  compare the current 96 m material instance with one terrain candidate derived from the desired
  ground-feature size. Vary the existing terrain scalar only; retain the same Structure identity,
  parent/instance object paths, cameras, lighting, and packaged route. The evidence view must span at
  least two complete candidate periods over continuous flat ground, with separate close, player,
  oblique, overview, and section-boundary checks. Make the scalar-only candidate at the existing
  material path from its recipe; preserve the accepted 96 m value for exact restore if rejected, and
  promote only after visual acceptance. Use the existing same-path builder/cook locality gate to
  prove no World package rewrite; do not regenerate canonical terrain for an appearance-only scalar
  change. The cube's numeric value is not copied blindly to terrain because the domains use different
  projection frames and feature-size requirements (D18).
- **KISS gate:** the box proof changes only one existing material scalar on a transient fixture; the
  terrain proof changes only the existing material scalar at its stable path. Neither adds an output,
  node family, runtime resource, cache, or World dependency. If no scalar reduces recognizable
  recurrence while retaining the required ground feature size, stop before promotion. Only then
  investigate ProjectTexture's period/frequency design: first test whether a longer full tile can
  retain the intended physical feature frequencies, then consider aperiodic/stochastic sampling if
  needed, with its sample, memory, and cold-cache costs measured.
- **Not a tiling fix:** a dedicated `MicroHeight` / `DetailNormal` output may create closer physical
  response, but it leaves the current Structure period intact and introduces another output and
  consumer path. Keep it out of this proof unless the operator later identifies close physical
  response as a separate unmet goal.
- **Engine reference:** installed UE 5.8's `UMaterialExpressionWorldPosition` defaults to absolute
  world position. Its `UMaterialExpressionNoise` documents `RepeatSize` as units per tile and supports
  tileable Gradient ALU, Value ALU, and Voronoi noise. The current basis uses exactly those tiled
  functions. Epic's [UE 5.8 texturing material functions](https://dev.epicgames.com/documentation/unreal-engine/texturing-material-functions-in-unreal-engine)
  describe `WorldAlignedTexture` as world-unit projection and warn that moving objects can swim under
  world-fixed coordinates. That function establishes projection semantics; it does not break a
  periodic input tile.

### Terrain-surface contract and channel ownership

CanonicalCompilation owns one versioned, territory-independent terrain-surface contract: semantic
names, meanings, engine-independent derivation, and authored-override semantics. This extends its
existing ownership of canonical metric terrain, typed features, authored-overlay application, and
lineage. ProjectWorldData supplies concrete profiles, source and authored inputs, canonical bundles,
and realized packages; it does not define reusable surface semantics. No Unreal type,
MeshPartition vocabulary, material path, or numeric channel index enters the canonical contract.
ProjectWorld is a downstream consumer: it maps the canonical surface weights to the selected terrain
representation.

The Mesh Terrain adapter derives one reusable layout from that contract. It owns the MPD channel
ABI, priority layers, build variants, platform policy, and semantic-name-to-index map. Numeric
indices are private to this adapter. The default is one versioned shared MPD asset, owned by the
content-capable representation-adapter plugin and referenced by every territory. ProjectWorld itself
cannot own that asset because its descriptor has `CanContainContent=false`. Per-territory MPDs are
forbidden unless Phase 1 proves a concrete engine or transaction requirement that one shared MPD
cannot satisfy; convenience or existing generated-root layout is not sufficient evidence.

ProjectMaterial recipes are consumers: they map required semantic names to PBR appearance. The
World adapter emits and validates a schema-versioned plain-data channel-layout receipt. The existing
ProjectMaterial host independently invokes ProjectMaterialEditor with that receipt as explicit
input; World never calls the compiler, imports its API, or gains a Build.cs dependency on it. The
compiler rejects missing, duplicate, or unknown appearance bindings, generates index-based sample
nodes, and records the consumed canonical-contract and layout hashes in its manifest. An integration
gate compares the compiler receipt, adapter receipt, generated material, and shared MPD before either
transaction is promoted. The receipt authenticates a projection; it does not become another
vocabulary authority.

The vocabulary and the weight sources are canonical terrain-layer inputs, not presentation: they are part of
the terrain producer's identity and dirty planning, and changing them rebuilds the affected terrain
sections by design. Shading parameters, textures, and the archetype graph stay presentation. The
target is that tuning them at the same object path rewrites no World package, but that claim is not
accepted until the real builder/cook probe in E25 passes. If the probe rebuilds sections, Phase 1
repairs the adapter boundary and identity split before the locality invariant is documented. The
territory contract's "Universal presentation material - Fully replaceable without geography
regeneration" row is updated only with the behavior the probe proves.

MeshPartition dependencies are permitted only in explicitly reviewed representation-adapter
modules. The strict current allowlist contains the World Mesh Terrain adapter and this
ProjectMaterial Mesh Terrain sampling adapter. A future third adapter requires architecture review
and an explicit allowlist change; it is not rejected merely because the count changes. Here the
channel expressions live in the Editor module
`MeshPartitionEditor`, and the Resource expression hard-references
`/MeshPartition/Textures/Void2DArray` (`MeshPartitionMaterialExpressionUtils.cpp:27`). ProjectMaterial
therefore declares the MeshPartition plugin dependency, and the adapter's source joins the compiler
fingerprint, which today scans only `ProjectMaterialEditor`'s own directory
(`ProjectMaterialEditor.Build.cs:36-42`).

### Public payload authority and orphan safety (T10)

Outcome: a public developer checkout receives exactly the accepted generated surface and pattern
packages, with the payload refusing any recipe, package, or pattern dependency that no longer
matches its owner's accepted manifest; and no owner transaction deletes a package another package
still references.

This design follows Q6's recommended answer (a); another answer replaces the first two bullets
and keeps the rest.

- **Owner manifests carry transport identity.** Each ProjectMaterial surface record and
  ProjectTexture pattern record adds `recipe_source_sha256`: SHA-256 of the recipe's UTF-8,
  BOM-stripped, LF-normalized bytes, the same rule the payload already applies to generated
  definitions (E48). `recipe_sha256` keeps its semantic meaning and still drives the semantic
  identity, so formatting-only edits regenerate no asset. `Validate` treats a source digest
  mismatch as stale; `Regenerate` rewrites the manifest whenever any accepted record differs from
  the existing one, not only when an asset was generated (E49).
- **One generic payload authority kind.** `generated_recipe_manifest` replaces `material_manifest`
  in the contract, its schema, and the composer, with no V1 alias. The contract declares
  ProjectTexture (pattern manifest, `Data/Patterns`, `*.pattern.json`) and ProjectMaterial
  (surface manifest, `Data/SurfaceRecipes`, `*.surface.json`). For each record the composer checks
  the recipe path, `recipe_source_sha256`, and output package bytes; that every recipe file under
  the root is recorded and every record has a file; the in-owner `dependency_*` hash; and that each
  `pattern_object_path` package is selected by the ProjectTexture authority with the recorded
  `pattern_package_sha256`. The composer imports no owner code.
- **Closure audit seeds ProjectTexture.** `REQUIRED_OWNERS` gains ProjectTexture, so the pattern
  catalog is an approved seed rather than a rejected project dependency (E47).
- **Cleanup sees every referencer.** When `-CleanupOrphans` has at least one candidate, each owner
  performs a synchronous full Asset Registry search before `GetReferencers`, and refuses cleanup
  if the registry is still gathering. Runs without cleanup candidates pay no scan. Because a
  snapshot's references are invisible to the registry, the host also refuses cleanup while any
  transaction can still restore content.
- **Test-only defect.** The credential-pattern payload test uses `bash -c`, not a login shell (E54).

#### Premise / KISS gate

The owners' accepted manifests already own recipe -> package authority, and the composer already
authenticates generated-definition sources by normalized bytes. Adding one digest per record lets
the composer check every record with the helpers it has, instead of reproducing two C++
normalizers. Removed: the V1 kind, the V1 verifier dependency, and any need for a Python
normalizer. Added: one record field and one save condition per owner, one registry gather on the
cleanup path, and one ProjectTexture contract entry. Knowingly given up: a formatting-only recipe
edit now needs a no-asset `Regenerate` to refresh its manifest before `Validate` or the payload
accepts it. The owner code change makes both compiler fingerprints stale once (E50), so T10 costs
one pattern and surface regeneration; M-14 shows same-path material changes write no World package.

#### Alternatives considered for T10

- **Python ports of both normalizers (Q6 b)** - rejected: a second normalization authority that
  must track every recipe schema change, as V1's verifier did for one owner.
- **Drop recipe-drift detection (Q6 c)** - rejected by the reliability guard: it removes the only
  release check that a published recipe matches its shipped package.
- **Run each owner's `Validate` inside `make release`** - rejected: adds two Unreal processes to
  release input preparation, and the composer loses its standalone refusal for any other caller.
- **Refuse all orphan cleanup inside commandlets** - rejected: orphans would accumulate until
  manually removed; a synchronous search keeps cleanup correct at a cost paid only when needed.
- **Original M-10 World failure injection** - replaced: World exact restore is already covered
  (E51), and the unproven half of M-10 is referenced-orphan retention, which M-4 checks directly
  and cheaply in the real commandlet route.

### Alternatives considered

- **Hand-authored master plus generated instances** - rejected by D4 and by determinism.
- **Material Layers for terrain** - rejected: UE-391544 (E19), and ALIS needs no layer authoring
  UX.
- **Keep `Generated/` and add v2 beside V1** - rejected by D2, D9, and the public repo migration
  policy.
- **Reuse City17's `M_Landscape`** - rejected: an authored ProjectObject asset built on Landscape
  layer nodes that already overflows SM5 samplers (E15).
- **Route through ProjectDefinitionGenerator** - rejected by D3; that lifecycle is bound to the
  definition subsystem.
- **Texture Graph render target per recipe at runtime** - rejected for UE 5.8.3 by E30-E31 and the
  unbounded-memory result in E38. Reconsider only after a future installed engine has a proved
  non-editor lifecycle and ALIS still has a gap.
- **Bake a full texture bundle per material recipe** - rejected as the default by D17. Compact
  imported exemplars remain legal when provenance and measured quality justify them, but colors,
  scale, and minor variations do not create new texture assets.
- **One universal mega-master with many static switches** - rejected because every used static
  combination creates a shader permutation (E37). Use a small, explicit family table.
- **Runtime Virtual Texture for everything** - rejected. RVT is outside this slice and may return
  only with a measured world-space cross-primitive composition requirement.
- **Custom virtual-texture producer or stochastic anti-tiling now** - rejected by KISS/YAGNI until
  the box/terrain scalar proof shows that a larger or differently patterned periodic tile cannot
  meet the visual requirement. These options add sampling, memory, cache, or identity complexity.

## Required invariants

1. Canonical/domain owners alone define surface meaning and authored facts. ProjectTexture,
   ProjectMaterial, Unreal assets, and runtime caches never become semantic authority; canonical
   names claim no more than their derivation and provenance prove.
2. ProjectTexture owns closed structural-pattern recipes and a small reusable kernel/function
   library. ProjectMaterial owns closed appearance, projection, parent-family, and instance recipes.
   Neither contract serializes arbitrary Unreal node graphs or redefines the other's facts.
3. Generated outputs are deterministic and reproducible from accepted recipes, engine/compiler
   fingerprints, and manifests. Unchanged regeneration writes nothing; promotion, rollback,
   journal recovery, ownership marking, and orphan safety retain their current guarantees.
4. Physical assets, parent families, material functions, cache tags, shaders, and permutations
   scale with reviewed kernels and structural families, never with cells, cities, colors, metric
   scale, wetness, or ordinary instance parameters.
5. Dependencies run World -> Resources. ProjectWorld binds authenticated material identity through
   data and never calls ProjectMaterialEditor. MeshPartition and renderer details stay inside
   reviewed adapters; same-path appearance tuning writes zero World packages, while semantic/layout
   changes rebuild only the declared terrain set.
6. Metric projection uses the correct domain frame and remains stable: terrain is seamless at
   section/cache-page boundaries and large coordinates; movable objects use object-local projection
   and do not swim.
7. The selected ProjectTexture runtime cache is disposable, bounded, observable, and deterministically
   invalidated. Mesh Partition Material Cache is not selected; RVT, Texture Graph runtime, custom VT,
   and disk caches are not dependencies of this slice.
8. The generated material compiles and renders without silent fallback on supported Game/Client
   shader platforms and meets the packaged quality/performance budget. Dedicated Server builds
   cleanly, excludes the material/texture Editor compilers, and never instantiates the runtime
   texture-generation subsystem. This does not claim the whole UE Server target is RHI-free.
9. Every imported texture has exact source, revision, hash, lineage, component ownership, and a
   public-source-compatible license. Generated procedural outputs do not bypass this provenance
   boundary.
10. Terrain and one generated movable cube consume the same ProjectTexture and ProjectMaterial
    contracts. Production cutover is one pass: rollback cannot strand World references, and no V1,
    `Generated` path segment, redirect, or compatibility layer remains afterward.

## Implementation tasks

- [x] **T0 - investigation and census (read-only):** recheck current owners, compiler/runtime
      boundaries, installed UE 5.8.3 source, Epic primary documentation, config, tracked asset
      cardinality/bytes, package scale, and the completed Mesh Terrain boundary. Record D15-D19,
      E27-E39, the refutations, and the architecture below. No production asset or configuration
      mutation.
- [x] **T1 - architecture packet:** retain the accepted `ground` / `hydro_transition` semantic
      contract, one shared MPD, private channel ABI, plain-data receipt, same-path zero-World-write
      locality, and zero ProjectWorld -> ProjectMaterialEditor dependency. Add the universal
      ProjectTexture/ProjectMaterial split, metric projection families, closed parent-family policy,
      Substrate direction, and distinct cache roles. R1 reviews this packet before implementation.
- [x] **T2 - minimal Substrate feasibility smoke, isolated asset only:** compile the smallest
      Blendable GBuffer material with one bounded physical output, then render it through one
      packaged Development Game/Client route. Record no default-material fallback and one frame.
      Keep one V1 capture only as a visual sanity reference, not as a supported arm. Build Dedicated
      Server only to prove no material/texture Editor compiler dependency leaked into it. Do not test
      Material Cache, RVT, eviction, PSOs, or production performance here. This smoke answers only
      whether ALIS can compile and render the selected backend and unblocks T3-T5.
- [x] **T3 - deterministic compiler foundations in test roots:** extend ProjectMaterialEditor with
      a closed family table, concern-named identities, ownership metadata, record-based orphan
      handling, LF manifest, data-driven consumer binding, and exact rollback. Add ProjectTexture's
      closed pattern schema, normalization/hash, compiler fingerprint, manifest/ownership contract,
      and host transaction. ProjectTexture recipes expose algorithms and structural parameters,
      never Unreal node graphs. Do not add a Texture Graph runtime dependency.
- [x] **T4 - generated Substrate backend:** generate bounded slab-based parent graphs and their
      stable instances from the ProjectMaterial recipe; keep the backend behind the compiler table;
      begin with Blendable GBuffer and the T2 accepted closure budget. Turn on Substrate only
      with the full target/cook matrix and a reversible config diff. Existing generated non-Substrate
      control remains reproducible until the new package is accepted; no permanent compatibility
      path remains at cutover.
- [x] **T5 - reusable procedural pattern kernels:** implement the minimum terrain pattern as a
      two-node native DAG owned by ProjectTexture, ending in one packed Structure output with a
      full mip chain. Reuse it by structural identity and parameterize appearance in
      ProjectMaterial. No per-cell, per-city, per-color, or per-scale texture outputs.
      Imported exemplar textures require A7 provenance and an owner-attributed size justification.
- [x] **T6 - terrain family and direct/cache decision:** generate the terrain parent/instance, Mesh
      Terrain channel sampling, and distance/quality policy. Compare an equivalent direct arm with
      the bounded ProjectTexture cache, keep the cached winner, and remove the direct probe. Prove
      SM5/SM6 Shipping cook, package integrity, packaged rendering, section continuity, and
      same-path zero-World-write acceptance. The official frame collector crashes identically for
      both arms, so no frame/GPU timing result is claimed. RVT and Mesh Partition Material Cache are
      not fallback arms.
- [x] **T7 - one-cube contract proof:** generate one movable cube using the same ProjectTexture
      pattern identity and ProjectMaterial recipe/instance path as terrain, with object-local metric
      triplanar coordinates. Move and rotate it; the pattern must retain its declared coordinate
      behavior and not swim. Stop
      there: no object, road, water, or building material migration belongs to this task.
- [ ] **T8 - production migration and closure:** generate new authority beside V1 only as local
      migration state; migrate the stable binding once; accept Kazan and Manhattan; remove V1
      recipes/archetype/assets/references and every obsolete `Generated` mention in the same change.
      Run cross-owner failure injection, SM5/SM6 cook, Editor/Game/Client/Server builds, packaged
      Development/Shipping cold and warm routes, Water proof, visual acceptance, payload/license
      audit, owner-attributed package delta, final docs, and independent R2 review.
      The production migration, legacy material removal, zero-write rerun, 19 exact tests,
      pre-current-RGB Shipping cook/package, Kazan/Manhattan views, and cube proof are complete.
      Current-RGB source-engine Shipping acceptance passes for Game, Client fixture, Water,
      cook/archive/IoStore, and package size. The final regenerated authority also passes the
      packaged Development 600-frame budget, packaged cold/warm cache proof, and refreshed source
      Shipping package plus Kazan/Manhattan views. The final fingerprint/performance delta R2
      passed after one parser fix. Remaining: the V1 public payload authority (Q6).
- [x] **T9 - prove metric texture scale box-first, then terrain:** the packaged
      fixed-view fixture accepts transient scalar overrides, records effective values, local
      bounds, and actor scale, and writes no material asset. The temporary Mesh Terrain
      diagnostic probe was removed after the terrain decision.
      - [x] Capture the same 1 m cube at PatternScaleMeters 0.25, 0.5, and 1.0 with matching camera,
        macro strength, material, and actor scale. The 4/2/1 output-period counts follow from the
        verified object-local meters / PatternScaleMeters formula and wrap addressing. The screenshots
        show the expected feature-size progression.
      - [x] Exercise uniform 0.5/2.0 actor scales, nonuniform scale 1/2/0.5, and a moved/rotated cube
        at the same material scale; receipts and packaged frames are retained.
      - [x] Compare terrain at 48/96/192 m with one stable packaged camera and material path. All
        three probes must match loaded compiled sections and report the effective values.
      - [x] Compare 64/96/128 m at the production MacroStrength 0.12 with the same camera and actual
        compiled-section material path. Keep the existing 96 m recipe; 128 m is a transient
        diagnostic outside the current persistent recipe's 100 m maximum.
      - [x] Decide visual recurrence/continuity from unmodified production-strength wide views
        and an exact identified compiled-section edge. The sampled views show no objectionable
        full-tile stamp or hard edge; this is bounded visual evidence, not a mathematical
        non-repetition claim.
      - [x] Remove the MeshTerrain-specific runtime probe and its external-scalar adapter;
        retain the generic fixed-view scalar fixture.
      - [x] Keep the persistent 96 m / 0.12 recipe unchanged, so no same-path material
        locality gate is triggered by T9.
      If a scalar cannot meet recurrence and feature-size requirements together, open the narrower
      producer-periodicity investigation; do not add `MicroHeight`, `DetailNormal`, or a second sample
      as a substitute for tiling.
- [x] **T10 - public payload authority and orphan safety (D21-D24):** implemented
      `## Decision` / Public payload authority and orphan safety.
      - [x] Red first: M-26 (V1 missing-manifest error and absent recipe authority), M-27 (both
        owners' `Validate` accepted a changed source), M-4 (the commandlet deleted a surface whose
        only referencer was an unloaded on-disk package), and the journal guard failed as
        stated; the credential-pattern test now uses `bash -c` with a 30 s timeout and completes.
      - [x] Owners: `recipe_source_sha256` in each record, reader, writer, and schema, computed by
        ProjectCore's `FProjectSha256::HashNormalizedText`; `Validate` is a dry run of the
        regeneration path; the manifest is saved whenever its serialized bytes would change.
      - [x] Cleanup: one synchronous registry search per cleanup run with candidates; refusal while
        `IsGathering()`; retained orphans log each referencer and whether it is loaded. The host
        refuses cleanup while a World transaction snapshot, a World harness `snapshot` or
        `outer-snapshot` folder, a release-projection rollback, or the sibling domain's journal can
        still restore content.
      - [x] Payload: `generated_recipe_manifest` in the contract, schema, and composer with
        ProjectTexture and ProjectMaterial entries narrowed to their generated roots; every
        recorded `*_object_path` dependency must be a selected package with its accepted hash;
        `REQUIRED_OWNERS` adds ProjectTexture; V1 kind, verifier loading, and V1 test paths are
        removed. The contract change re-stamped both definition manifests' contract hash through
        the supported refresh route.
      - [x] Regenerated once, pattern before surfaces; reruns and `Validate` are zero-write; the
        World authority audit still accepts all 3,096 artifacts unchanged.
      - [x] Documentation per the Documentation plan; the 3.0.0 router separates development
        completion from R5 certification.
      - [x] T10 verification rows run; final diff reviewed; independent R2 returned PASS.
- [x] **T11 - closure corrections (D25):** implemented inside the black-box independence sweep,
      after its firewall guards were green and before its one combined owner-local refresh (1
      pattern and 4 surfaces regenerated because both owners' own implementations changed; World
      untouched).
      - [x] Red first, both owners (existing host tests): hold the unreferenced orphan open
        without delete sharing during `-CleanupOrphans`; the run accepted (E57).
      - [x] Fail closed: a resolve, containment, or delete failure returns an error through
        `OutResult.Error`, so the host rolls back; a file that is already absent stays a success.
      - [x] Host: keep the operation folder while the journal names its snapshot; a rollback
        failure reports the original rejection and leaves recovery to the next run; the test
        releases the handle, recovers, and proves the accepted bytes and `Validate`. With only the
        new `finally` guard removed, the recovery run failed on the deleted snapshot (red).
      - [x] Retained orphans: add `retained_orphans` to the commandlet receipt and host summary;
        document the lifecycle in both owner docs (left on disk, dropped from the accepted
        manifest, never selected for the payload, not reconsidered by later cleanup, disposed
        manually). No orphan database.
      - [x] Projection concurrency: no new guard paths; the host README states the limitation.
        The projection recovery task owns the fix through isolation in a linked worktree, which
        removes the live-tree restore the guard refuses on (its invariants 1 and 4).
      - [x] 3.0.0 plan: replace the 42-count item with E56's 45 owner rows and the exporter's
        registry-gather defect as that item's first fix; record ownership only, with no package
        selection or exporter change in this task.
      - [x] Focused tests, owner `Validate` no-op, read-only World audit, independent R2, then
        move this task to done.

## Test-first and verification plan

### Red evidence

| Case | Kind | Wrong behaviour captured today |
|---|---|---|
| M-1 v2 contract | Permanent guard | No v2 schema exists; the closed-contract parse test for v2 fails. |
| M-2 identity path | Permanent guard | `ResolveOutputIdentity` emits `/ProjectMaterial/Generated/...`. |
| M-3 manifest bytes | Permanent guard | Serialization writes CRLF on Windows (E7). |
| M-4 referenced orphan | Permanent guard | In each owner's host test root, generate outputs, keep an unloaded byte copy of one output on disk (the copy references the output's package path), remove that output's recipe and an unreferenced control's recipe, and run the real commandlet with `-CleanupOrphans`: the held orphan survives with the copy logged as its only referencer (`loaded=false`), and the control is deleted. With the registry search removed, both owners delete the held orphan. |
| M-5 data-driven binding | Permanent guard | `ProfileContract` asserts a hard-coded V1 path. |
| M-6 shader platforms | Acceptance | A known-bad control (a deliberately over-sampled variant) must fail the SM5 cook check before the v2 pass is trusted. |
| M-7 visuals | Acceptance | V1 same-vantage captures are recorded before v2 exists. |
| M-8 frame budget | Acceptance | The V1 packaged baseline is recorded on the E16 envelope. |
| M-9 hierarchy and naming | Reviewer-checked | Concern-named content with no lifecycle folder, and names that follow the separation of concerns. |
| M-10 cross-owner failure | Acceptance | Every World package restored after a failed World transaction must still resolve its material. Proved by composition: World exact restore (`generated_content_transaction.Tests.ps1`), referenced-orphan retention (M-4), and the host refusing cleanup while a World transaction or harness snapshot, projection rollback, or sibling journal can still restore content. Residual: the public World projection holds no content lock, owned by its recovery task. |
| M-11 adapter fingerprint | Permanent guard | Editing only the sampling adapter's source must change the compiler fingerprint; today the fingerprint scans only the module directory. |
| M-12 surface-contract ownership | Permanent guard | A recipe currently has no external canonical semantic layout to validate; missing, duplicate, or unknown appearance bindings must be rejected, numeric indices must never become recipe/domain fields, and a slope-only fixture must fail any attempted factual `rock` classification. |
| M-13 compiler boundary | Permanent guard (Architecture) | Fails if any ProjectWorld module includes, links, loads, or invokes ProjectMaterialEditor; a known-bad Build.cs dependency and API call must fail it. |
| M-14 material invalidation | Acceptance | Change only material package contents at the same object path, run the real Mesh Terrain builder and cook reuse route, and record section and World package bytes; a forced material object-path change is the known-bad rebuilding control. |
| M-15 pattern contract | Permanent guard | ProjectTexture has no schema/compiler; reject unknown algorithm, output, seed shape, structural parameter, path escape, duplicate identity, and ProjectMaterial-owned color/scale fields before mutation. |
| M-16 parent family/permutations | Permanent guard | The current compiler has one hard-wired graph and no family manifest; a fixture with an unregistered family or static combination must fail, and an excessive-static-switch control must increase the measured shader set. |
| M-17 Texture Graph runtime boundary | Architecture/packaged | A non-editor target must contain no ProjectTexture -> TextureGraph module dependency; the 5.8.3 runtime-generation hypothesis is rejected by source and, if challenged, by a minimal packaged control. |
| M-18 Mesh Partition Material Cache | Not selected | No Material Cache arm is required; ProjectTexture's bounded runtime pattern cache is the accepted route. |
| M-19 ProjectTexture cache invalidation | Permanent plus packaged | A normalized recipe no-op preserves identity/pages; a structural pattern change invalidates the exact affected cache identity; unrelated recipes remain warm and untouched. |
| M-20 Substrate target matrix | Acceptance | Game/Client compile and render the fixture on SM5/SM6 without silent fallback. Dedicated Server builds cleanly, excludes ProjectMaterialEditor/ProjectTextureEditor/TextureGraph, and does not create the ProjectTexture runtime cache; it has no rendering or performance requirement. The broader UE Server target is not asserted to be RHI-free. |
| M-21 projection frames | Permanent plus visual | Static terrain remains metric/seamless; one generated object-local cube retains metric scale and does not swim while moving and rotating. |
| M-22 asset cardinality/storage | Permanent plus cook | Changing only color, roughness, metric scale, semantic blend, territory, or cell count creates zero new texture/kernel/parent assets and no new shader family; owner-attributed cook bytes remain bounded. |
| M-23 cold/warm cache route | Packaged acceptance | Authenticated first-view and repeated traversal must distinguish cold generation/page fill from warm reuse; a route that records identical telemetry in both arms is non-discriminating and rejected. |
| M-24 box scale calibration | Test plus packaged visual acceptance | The gate now records scalar values, local bounds, and actor scale. Accepted same-camera 1 m cube captures at 0.25, 0.5, and 1.0 m show the expected feature-size progression; the exact 4/2/1 full-tile counts follow from the verified coordinate formula and wrap sampler, not a per-pixel image counter. |
| M-25 terrain period and section continuity | Packaged visual acceptance | The default 96 m material and transient 48/96/192 m at strength 0.8 and 64/96/128 m at production strength 0.12 were exercised on actual Mesh Terrain sections. Scale response passed. Current-source Editor vertex bounds and packaged loaded identities identify the Y=-562900 cm edge; final Development and Shipping views show no hard restart. Open-ground period and half-period controls show no objectionable stamp in sampled product views. Keep 96 m; do not claim mathematical non-repetition or every-edge coverage. |
| M-26 public payload recipe authority | Permanent guard | `test_developer_payload.py` selects exactly the 4 accepted surface and 1 pattern packages and rejects, each alone: an edited recipe byte, an unrecorded recipe file, a recorded recipe with no file, a package hash drift, and a surface whose `pattern_package_sha256` differs from the selected ProjectTexture package. Fails today with `Required payload file is missing` (E46). |
| M-27 recipe transport digest | Permanent guard | A formatting-only recipe edit makes the owner's `Validate` report stale; `Regenerate` then rewrites only the manifest (`generated=0`, zero shader compiles), and the next `Validate` accepts. Fails today because no record carries a source digest and the manifest is not rewritten when all assets are skipped (E48, E49). |

### Green evidence

- Current exact tests, run one at a time through
  `scripts/ue/test/unit/iterate.ps1 -TestFilter <exact name>` (E53): the seven ProjectMaterial and
  nine ProjectTexture tests named there. No CI configuration naming them was found (unverified).
- Host tests `scripts/ue/material/test/run_surface_generation.Tests.ps1` and
  `scripts/ue/texture/test/run_pattern_generation.Tests.ps1`: idempotence and exact restore after a
  child rejection through the shared transaction. T10 adds M-4 and M-27 here, because both need
  the real commandlet process.
- T10 payload: `python -m unittest scripts/git/mirror/tests/test_developer_payload.py` passes all
  tests, including M-26; `scripts/git/mirror/audit_developer_payload.ps1 -OutputPath <tmp receipt>`
  (seed plan, one Unreal inventory export, closure validation) accepts with the ProjectTexture
  catalog as an approved seed and no rejected dependency. This is the cheapest route that runs
  the real closure check; it needs no cook or package.
- T10 authority: the owner rerun after regeneration is zero-write; the World
  `audit_generated_authority.ps1` still accepts all scopes unchanged; the JSON data validator
  accepts both changed manifest schemas.
- `Project.World.Realization.Presentation.ProfileContract` and the NativeTwin material-migration
  assertion in `ProjectWorldNativeTwinTests.cpp`.
- A host-level integration test generates the adapter layout receipt and the material independently,
  rejects mismatched contract/layout hashes before promotion, and proves M-13 with the build graph.
- Focused ProjectTexture recipe/identity/owner-confinement/idempotence/rollback tests, added one
  exact test at a time; the host integration proves transaction recovery and zero-write rerun.
  Orphan retention is M-4.
- A generated-family inventory test proves M-16 and M-22 from recipes, manifest, asset registry,
  compiled shaders, and cooked owner attribution rather than filename guesses.
- The selected ProjectTexture cache is accepted by its exact generation, reuse, invalidation, and
  bounded-residency checks. M-18 is not run because Mesh Partition Material Cache is not selected;
  cold/warm runtime measurements in M-23 remain separate performance evidence.
- `scripts/git/mirror/tests/test_developer_payload.py`.
- Cook logs for `PCD3D_SM5` and `PCD3D_SM6`, the Shipping Water proof, packaged Game/Client
  performance through the World performance evidence owner, a clean dependency-only Dedicated
  Server build, and World L2 or L4 as the World pipeline layers select.

### Acceptance matrix

The invariants above own architecture. This matrix owns the detailed execution checks.

| Concern | Cheapest proof | Final proof | Stop condition |
|---|---|---|---|
| closed owner contracts | M-1, M-12, and M-15 parser/refusal cases | accepted independent manifests and generated outputs | graph DSL, duplicate semantic authority, or permissive parse |
| deterministic safe generation | identity/no-write, LF bytes, orphan, journal, and rollback tests | repeated production apply and interrupted recovery drill | unstable identity, foreign deletion, or incomplete restore |
| adapter boundary and fingerprint | M-11 and M-13 dependency/fingerprint controls | clean public developer build | reverse World dependency or untracked shaping source |
| World binding and locality | M-5 plus M-14 same-path/path-change controls | both territories resolve one authenticated identity with zero same-path World writes | hard-coded binding, same-path rewrite, or non-discriminating control |
| semantic inputs and layout | M-12 exact-name, private-index, dependent-input, and slope-to-rock refusal | both territories use the same honest canonical contract and MPD layout | overclaimed semantics, dependent realized input, or divergent layout |
| physical asset and permutation scale | M-16 and M-22 parameter/cell/city variations | owner-attributed Shipping asset, shader, PSO, and byte inventory | growth by ordinary data variants |
| minimal Substrate feasibility | T2 compile plus one packaged Development frame | none; T2 only unblocks foundations | compile failure or default-material fallback |
| Game/Client rendering | M-6 and M-20 on SM5/SM6 | clean Shipping cook/render with accepted quality and performance | overflow, fallback, target failure, or budget regression |
| Dedicated Server isolation | module graph and ProjectTexture server guard | clean release-equivalent Server build without material/texture Editor compiler modules | editor/compiler dependency, runtime texture-cache creation, or build failure |
| ProjectTexture structural cache | M-19 generation/reuse/invalidation and bounded-residency tests | packaged cold/warm route and owner-attributed memory evidence | unbounded residency, wrong invalidation, or no cache reuse |
| projection and tiny second surface | static terrain checks plus M-21 cube movement/rotation | packaged terrain and one generated movable cube | seam, swimming, or scale drift |
| box-first scale then terrain tiling | M-24 before M-25; compare tile period separately from seam continuity | calibrated box counts, then same-path terrain views spanning multiple periods | terrain is changed before the box mapping passes, or period/feature scale cannot both meet the visual requirement |
| production behavior | V1 sanity capture, frame baseline, Water fixture | Kazan/Manhattan visual acceptance, frame budget, and Shipping Water proof | rejected quality, frame regression, or Water misclassification |
| provenance and public payload | A7 fail-closed inventory and M-26 | payload dependency audit with the ProjectTexture seed; clean public checkout at the 3.0.0 router's R5 step 4 | unclassified texture, missing authority, or recipe/package drift |
| one-pass cutover and rollback | M-4 in both owners plus World exact restore (M-10) | post-cutover asset/reference grep; rollback drill per Q7 | stranded reference, deleted referenced package, or any compatibility/legacy residue |

## Documentation plan

- **Authoritative owners:** the ProjectMaterial README (content tree and ownership) and
  `material_generation.md` (surface recipe, parent-family/instance contract, identity path,
  ownership marking, manifest format, archetype table, Substrate backend, and cache adapter
  selection); ProjectTexture README plus one focused generation contract doc (pattern recipe,
  structural identity, kernel/output policy, manifest, and transaction).
- **Also:** `territory_generation.md` (the terrain binding statement and resolution through data);
  `territory_contract.md` (the presentation-material row: shading stays replaceable without
  geography regeneration only to the extent proved by M-14, while the canonical surface contract
  and weight sources are terrain inputs); CanonicalCompilation owns the semantic contract and
  authored-override derivation; the Mesh Terrain adapter owner documents the private channel ABI,
  layout receipt, and one shared MPD;
  ProjectMaterial `pitfalls.md` only for new traps; `docs/data/README.md` and
  `docs/data/structure.md` only where they state the path, and only as links to the owner.
- **Renderer facts:** the Mesh Terrain adapter doc owns MPD and realization settings; ProjectTexture
  docs own the structural-pattern and runtime-cache contract. They link to ProjectMaterial for
  appearance, without assigning the Mesh Terrain adapter an unselected Material Cache role.
  Higher architecture docs state only canonical authority -> generated material authority ->
  replaceable renderer adapters. No stable RVT policy is added without a real consumer.
- **Duplication avoided:** consumers link to `material_generation.md`. Stable docs, code, tests, and
  configuration never link to this task.
- **T10:** `material_generation.md` and `pattern_generation.md` each state that the accepted
  manifest records `recipe_source_sha256` (what it hashes), that `Validate` compares it, and that a
  formatting-only edit regenerates no asset but refreshes the manifest; this replaces
  `material_generation.md`'s current "Formatting-only recipe changes are no-ops". Both state that
  cleanup gathers the Asset Registry before its referencer check. `material_generation.md`'s
  Verification paragraph moves orphan retention from the focused C++ tests, which do not cover it
  (E53), to the host test. `developer_asset_release.schema.json` owns the new kind's fields;
  `scripts/git/mirror/README.md` stays generic unless a sentence becomes false.

## Rollout and rollback

- Material generation keeps its host transaction: lock, snapshot, journal, and exact restore on
  failure.
- Before the production migration, prove a post-success rollback route, because
  `RollbackPrevious` has no apply mode today (E5).
- World packages that change with the identity migration roll back through the World transaction
  owner as part of the terrain regeneration. T8's order keeps V1 present until that regeneration is
  accepted, so a World rollback never restores references to deleted assets (M-10).
- Any Substrate config change promotes only after the candidate package passes. Rollback restores
  the exact previous config and generated material authority together; cached pages are disposable
  and never participate in authority rollback.
- Runtime cache exhaustion follows ProjectTexture's bounded refusal/eviction behavior proved by its
  owner tests. It cannot trigger
  recipe generation, write Content, or invent a second cache owner at runtime.

## Reviewer brief

If this scale/tiling plan is independently reviewed, keep the review limited to the box-first
sequence and its evidence. Use the `architect` skill and the review contract; D1-D20 bind. Check the
current material builder, ProjectTexture output setup,
the packaged fixed-view test gate, installed UE 5.8 sampler source, and the relevant owners:
[ProjectMaterial](../../../Plugins/Resources/ProjectMaterial/docs/material_generation.md),
[ProjectTexture](../../../Plugins/Resources/ProjectTexture/docs/pattern_generation.md), and
[Territory generation](../../../Plugins/World/ProjectWorld/docs/territory_generation.md).

Verify or challenge only these points:

- world-metric projection already exists; the full Structure period is
  `PatternScaleMeters`, separate from internal noise periods and from a hard tile-edge seam;
- UE's wrap sampler repeats in UV, while the current material's world position means Mesh Terrain
  sections do not restart texture coordinates at their boundaries;
- the packaged fixture can vary one generic material scalar transiently without creating candidate
  assets or coupling ProjectWorld to ProjectMaterialEditor;
- a measured 1 m box gives the proposed 4/2/1 full-period calibration at 0.25/0.5/1.0 before any
  terrain material change;
- only after that proof, a same-path terrain comparison checks recurrence, required feature scale,
  section continuity, and zero World writes separately;
- `MicroHeight` / `DetailNormal` is not presented as a fix for periodic recurrence; no new output,
  sample, cache, or rendering subsystem is needed unless the scalar proof fails.

Do not reopen the Mesh Terrain cutover, the accepted ProjectTexture cache choice, Substrate
architecture, or the already reviewed T1 ownership packet. Do not start another technology survey
or implement the fixture change as part of a review. This reviewer brief is not an additional gate
before the operator-approved box proof. Return `PASS`, `PATCH`, `BLOCKER`, or `UNVERIFIED`; each
required finding must give evidence, consequence, and the smallest safe fix.

## Completion criteria

`PASS` requires:

- The ten architecture invariants hold and every applicable acceptance-matrix row has
  discriminating evidence.
- ProjectMaterial and ProjectTexture compile independent closed recipes into a bounded family of
  generated parents/functions/instances with no graph DSL and no asset growth by cell, city, color,
  scale, or ordinary parameter variation.
- Substrate and the selected terrain path pass the real ALIS target/cook/package matrix. The
  ProjectTexture cache remains the selected pattern path; Mesh Partition Material Cache is outside
  scope unless a measured requirement reopens it.
- One production terrain family and one generated movable cube consume the same contracts; V1 and
  every obsolete `Generated` reference are removed; ProjectWorld binding remains data-only.
- Kazan and Manhattan pass cooked shader/permutation/PSO, package/storage, frame/memory/hitch,
  cache, Water, payload/license, and visual checks.
- Stable docs state current ownership and operations, and independent R1/R2 return `PASS`.

## Review record

### 2026-09-23 - initial investigation

- **Trigger:** the operator's accepted 3.0.0 plan: Terrain Material v2 built on the selected
  substrate, with no coupling to existing stubs.
- **Root cause:** V1 is a prototype contract (see Capability gap).
- **Fix:** this plan.
- **Verification:** read-only census of ProjectMaterial, its consumers, and the installed
  MeshPartition material expressions. `git ls-files --eol` and SHA-256 comparisons confirmed E7.
  No build, test, or generation ran.
- **Authority:** D1-D10 recorded; Q1-Q3 opened; A1-A5 active.

### 2026-09-23 - independent architect review (PATCH)

- **Trigger:** the operator asked for `/architect` review; a fresh non-author reviewer returned
  `PATCH` and spot-checked the E-facts against the tree and the installed engine.
- **Root cause:** the first draft treated channel weights as presentation, left the MeshPartition
  dependency and fingerprint coverage undeclared, gave the cross-owner migration no order, and
  duplicated the Mesh Terrain task's substrate gate.
- **Fix:** weights and vocabulary are terrain inputs (Channel vocabulary ownership and A6); the
  substrate sampling adapter is the second allowed MeshPartition dependent with fingerprint
  coverage (M-11); the migration keeps V1 until the World result is accepted, with a
  failure-injection proof (M-10); the substrate gate is named in
  prose with its single owner; the test mount joins the E4 census; the LF fix may land early. The
  reviewer's engine claims were rechecked in source before being applied.
- **Authority:** the draft's substrate gate Q1 was removed as a duplicate of the Mesh Terrain task's
  Q1 (todo/README.md: name the single owner in prose); the former Q2 and Q3 are now Q1 and Q2. A3
  rejected by evidence; A6 added.

### 2026-09-23 - operator direction and reviewer brief

- **Trigger:** "even experimental it future long going goal so we will wrap around it our
  architecture after test fully without any legacy support and mentions", and "ensure that all
  finding are in todos and if some has questions or maybe better pathes - propose reviewer to
  investigate it carefully via web and our goals and standarts".
- **Fix:** the no-legacy completion rule and `## Reviewer brief` were added; the host-helper duplication
  and the receipt misnomer handed to the backlog task that owns them.
- **Authority:** D11 added; A1 resolved by D11.

### 2026-09-23 - operator answers to the open gates

- **Trigger:** the operator's plain-words answers (bigger is fine, the hand layer is mandatory in
  every generation, City17 is legacy, the engine moves to 5.8.3).
- **Fix:** E8a (stale V1 records on 5.8.3) and its T0 check; the hand-layer investigation added to
  the reviewer brief.
- **Authority:** D12-D14 added; Q2 closed by D12.

### 2026-09-23 - investigation closure after R1

- **Trigger:** the operator requested a complete review picture without implementation.
- **Finding:** the City17 candidates have no located text provenance/license ledger, so prior
  packaging cannot make them the default. Epic's current docs support indexed channels,
  shared-sampler design, and world-aligned projection, but publish no ALIS performance bound. T0
  also proposed a needless stale-V1 production rewrite, and the plan lacked the required
  invariant-to-proof matrix and exact full test names.
- **Fix:** A4 is rejected, A7 fails closed, and A8 supplies the smallest input-honest role set;
  E20-E24 record primary/current evidence and two CC0 candidate source pools; T0 validates
  before deciding whether V1 must be regenerated; all 12 exact tests and the 20-row proof matrix are
  explicit.
- **Verification:** repository census and Epic primary documentation only. No asset was opened for
  editing, generated, moved, or licensed; no test/build ran; no engine checkout or Git state changed.
- **Authority:** A7-A8 added; D1-D14 unchanged. This authoring pass is not the required independent R1.

### 2026-09-24 - reviewer boundary corrections (PATCH applied)

- **Trigger:** the operator asked to evaluate and apply the reviewer's four architecture findings.
- **Finding:** the recipe-owned vocabulary duplicated World semantic authority, the target diagram
  retained a Landscape adapter despite D11, and the dependency rule encoded a permanent count
  instead of an adapter boundary.
- **Fix:** ProjectWorld now owns the versioned terrain-surface contract; the Mesh Terrain adapter
  owns the private name/index ABI and reusable MPD recipe; ProjectMaterial owns appearance mappings
  and one Mesh Terrain-only production adapter; dependency enforcement is a strict reviewed
  allowlist rather than an architectural count.
- **Verification:** repository owner map, current schemas and compiler code, Epic 5.8.3 source tag,
  and Epic's current Mesh Terrain/MPD/material documentation were read. The 5.8.1-to-5.8.3 plugin
  delta does not change channel or material-expression ownership. No build or generation ran.
- **Authority:** D1-D14 are unchanged; the corrections implement their existing ownership and
  no-legacy intent. This authoring pass is not the required independent R1.

### 2026-09-24 - reviewer canonical-boundary corrections (PATCH applied)

- **Trigger:** the operator supplied the next reviewer PATCH and asked for a critical code and
  architecture check, valid fixes, and explicit refutations.
- **Finding:** the prior draft still made a downstream ProjectWorld contract authoritative for
  CanonicalCompilation, made the World adapter call ProjectMaterialEditor, allowed territory-local
  MPDs by default, assumed same-path material locality without the real builder, and left the
  material-side effects of those gaps underspecified.
- **Fix:** CanonicalCompilation owns the engine-independent surface contract; the World adapter
  publishes a plain-data layout receipt; the ProjectMaterial host invokes its compiler independently;
  one content-capable adapter owner publishes one shared MPD unless Phase 1 proves that impossible;
  M-13 and M-14 prove the compiler boundary and builder/cook material invalidation behavior.
- **Verification:** the owner map, canonical compiler flow, ProjectWorld descriptor and Build.cs
  files, UE 5.8.3 `UMeshPartitionDefinition::GatherDependencies`, and Epic's current MPD/material
  documentation were read. Epic supports MPD reuse across partitions. The local source hashes only
  the material path, not its contents, so that reviewer implication was narrowed while the runtime
  probe was retained. No build, generation, package, or production mutation ran.
- **Authority:** D1-D14 are unchanged. This authoring pass is not the required independent R1.

### 2026-09-24 - planning revision accepted for R1 (PASS)

- **Trigger:** reviewer recheck passed the architecture packet and directed the next work to fresh
  independent R1 rather than another speculative design cycle.
- **Finding:** A8 still used `steep exposed rock` even though A6 supplies only continuous slope and
  height geometry, not geological or land-cover evidence. That label could later mislead non-visual
  consumers.
- **Fix:** A8 now keeps slope continuous, uses only provenance-honest canonical roles, and leaves
  rock-like fallback appearance to ProjectMaterial. T1, M-12, and the R1 brief require
  a slope-to-`rock` known-bad refusal. No new owner, subsystem, task, or architecture branch was
  added.
- **Verification:** the correction was checked against A6, Q1, the canonical ownership invariant,
  and the existing adapter/material boundary. No build, generation, package, or production mutation
  ran.
- **Authority:** D1-D14 are unchanged. The packet is ready for fresh independent R1; no further
  architecture redesign precedes that review.

### 2026-09-24 - T1 and Mesh Terrain Phase 1 evidence

- **T1 result:** retain only `ground` and `hydro_transition`; slope and height are continuous shader
  inputs and cannot create factual rock, mud, soil, shore, wetness, or land-cover classes. Initial
  channel realization is named vertex weights at the truthful 30 m source resolution, private ABI
  indices 0/1, UNorm8 compiled encoding, UV0, 3000 cm texel size, and a 4096 maximum dimension.
- **Ownership:** `ProjectWorldMeshTerrain` owns the one shared MPD and layout receipt;
  `ProjectMaterialEditor` is the independent receipt consumer. ProjectWorld has zero build, include,
  load, or API dependency on ProjectMaterialEditor.
- **Measured locality:** the real builder reuses sections and preserves World package hashes after a
  same-object-path material content change. The material-path-change control rebuilds, proving that
  the instrument discriminates the property under test.
- **Still blocked for production:** no texture bytes are admitted until exact source revisions,
  per-file SHA-256 values, CC0/license evidence, transformation/import lineage, and a ProjectTexture
  machine-readable sidecar authority exist. City17 binary presence is not provenance. T3-T5 remain
  gated on the Mesh Terrain Phase 3 decision and those admission requirements.

### 2026-09-25 - independent final R2 review (PATCH)

- **Accepted architecture:** the honest `ground` / `hydro_transition` vocabulary, continuous slope
  and height, one shared MPD, private channel ABI, plain-data receipt, and zero ProjectWorld ->
  ProjectMaterialEditor dependency remain sound. No further architecture cycle is required.
- **Evidence correction:** the generated Kazan Landscape control loads
  `MI_ProjectTerrain_Default`; retained sampler-overflow errors belong to legacy City17
  `M_Landscape`. The current-state summary no longer attributes that unrelated failure to Kazan.
- **Production gates:** exact texture revision/hash/license/lineage and machine-readable
  ProjectTexture sidecar authority remain open. They block production asset admission, not the
  completed T1 design work.
- **Substrate gate:** the packaged Mesh Terrain Phase 3 gate rejects before center collision, so
  T3-T5 remain blocked. No material asset generation or production binding occurred.

### 2026-09-27 - universal material and runtime-cache investigation (REVIEW REQUIRED)

- **Trigger:** the operator expanded the terrain slice into a universal generated material,
  instance, and texture-pattern domain; directed use of credible future Epic beta/experimental
  technology; and required substantially less cooked asset multiplication through runtime
  calculation/cache reuse.
- **Verified current state:** the Mesh Terrain cutover is complete; current ownership and locality
  boundaries remain valid. ProjectTexture has no recipe/compiler/cache implementation but already
  contains 593.94 MiB of tracked texture assets. ProjectMaterial remains an Editor-only closed V1
  compiler with 2.96 MiB of tracked assets. ALIS enables virtual texturing, disables Substrate, and
  cooks SM5 and SM6.
- **Critical correction:** the prior discussion's Texture Graph Shipping-runtime proposal is not
  viable on installed UE 5.8.3. Runtime module labels and async render APIs exist, but core engine
  initialization is compiled only with `WITH_EDITOR`, and the async task disables its own cache.
  Retaining full render-target bundles would also be unbounded VRAM, not a free storage win.
- **Selected direction:** ProjectTexture owns closed structural-pattern recipes and reusable
  procedural kernels; ProjectMaterial owns closed surface recipes, a small parent-family/instance
  set, metric projection, and generated Substrate slabs; Mesh Partition Material Cache is the
  primary page-cache candidate; RVT is reserved for proved cross-primitive world composition;
  direct evaluation is the control; custom VT/disk cache is deferred.
- **Evidence standard at that review:** the first draft required a broad packaged comparison across
  direct, Material Cache, and RVT before foundations. The later KISS review below replaces that
  execution order; source inspection still does not promote an experimental backend.
- **Files changed:** this todo only. No code, asset, config, generated authority, build output, or
  external state was changed.
- **Authority:** D15-D19 and Q3-Q4 added; A9-A12 added. The task is ready for independent R1, not
  implementation.

### 2026-09-27 - KISS execution review (PATCH applied)

- **Accepted:** the reviewer correctly retained the ProjectTexture/ProjectMaterial ownership split,
  generated Substrate direction, Material Cache as the sole terrain cache candidate, tiny second
  consumer, and rejection of Texture Graph runtime/custom VT/disk cache/mega-master designs.
- **Fixed:** T2 is now only a minimal Substrate compile plus packaged Development frame. Durable
  compiler contracts start after that smoke; the identical direct-versus-Material-Cache terrain A/B
  runs later in T6. RVT is outside this slice, Dedicated Server is dependency/build-only, and the
  second consumer is exactly one generated movable cube.
- **Narrowed, not deleted:** the reviewer was right that 30 architecture invariants duplicated the
  task. Ten durable invariants now own architecture, while the removed sampler, transaction,
  locality, cache-pressure, provenance, package, Water, and cutover obligations remain in one
  acceptance matrix. Treating those safety checks as disposable would have been incorrect.
- **Verification:** the revised sequence and matrix were checked against D1-D19, the installed-source
  findings E29-E36, the accepted Mesh Terrain boundary, and the current ProjectMaterial and
  ProjectTexture ownership. No implementation, asset generation, build, package, or config change
  was performed.

### 2026-09-27 - implementation checkpoint (T2 PASS, T3 foundations in progress)

- **T2 accepted:** Substrate is enabled with the Blendable GBuffer. The terrain builder now emits
  one explicit `UMaterialExpressionSubstrateSlabBSDF`; its closed graph test and the isolated
  transient slab compile test pass.
- **Real route:** the saved parent and instance regenerated transactionally, then compiled on SM5
  and SM6 in the complete Development cook. The cook compiled nine changed shader jobs with zero
  failures for `M_ProjectTerrain`; package, archive integrity, IoStore policy, and the packaged Kazan
  fixed-view receipt passed. The accepted frame is under
  `tmp/material/substrate_t2/render-explicit-slab/` and is disposable evidence, not a stable input.
- **Idempotence:** the immediate second regeneration reported `generated=0`, `skipped=2`, and
  `shader_compiles=0` with the same accepted manifest hash.
- **Server boundary:** static dependency inspection confirms both new compiler modules are
  Editor-only and absent from runtime/Server targets. The installed launcher engine refuses every
  Server target before compiling project code (`Server targets are not currently supported from
  this engine distribution`), so a Server binary remains unverified until the release-equivalent
  server toolchain is available.
- **Unrelated warning:** the full package still reports the known City17 authored Landscape SM5
  16-sampler overflow. Kazan Mesh Terrain and the new explicit Substrate material do not report a
  fallback. City17 remains separate content debt and is not a reason to reintroduce Landscape
  compatibility into this pipeline.
- **T3 status:** closed ProjectTexture pattern and ProjectMaterial surface recipe/manifest contracts,
  engine Major.Minor compatibility, accepted pattern-authority checks, LF serialization, and exact
  parser controls are implemented and focused tests pass. Asset compiler host transactions and the
  production v2 cutover remain open; T3 is intentionally not marked complete.

### 2026-09-28 - production terrain implementation checkpoint (PATCH)

- **Implemented:** ProjectTexture owns the closed analytic terrain-pattern recipe and generated
  material function. ProjectMaterial owns closed surface recipes, two bounded Substrate parent
  families, stable terrain/object instances, manifests, compiler identity, transactions, and
  concern-named `/ProjectMaterial/Surfaces/...` outputs. ProjectWorld remains data-only and has no
  ProjectMaterialEditor dependency.
- **Migration cleanup:** deleted material assets and references under `/ProjectMaterial/Generated`,
  removed old compiler/schema/command compatibility, removed `terrain_metric_v1` and
  `object_metric_v1` archetype names, and corrected the packaged Mesh Terrain runtime gate to expect
  the current `/ProjectMaterial/Surfaces/...` chain. The current shared MPD's `_v1` name is its own
  Mesh Terrain contract version, not material compatibility, and is intentionally unchanged.
- **Cache decision:** direct evaluation is selected. Installed engine source shows Material Cache
  support is off by default and requires explicit cache tags; current cook evidence names no shading
  bottleneck. No speculative cache arm, RVT, runtime Texture Graph, custom VT, or disk cache was
  added.
- **Determinism and locality:** final generation produced four assets and compiled two parents; the
  immediate rerun produced `generated=0`, `skipped=4`, and `shader_compiles=0`. The final production
  Kazan apply reported zero dirty units in every layer and zero created, updated, removed,
  self-saved, vegetation, building, or gameplay rewrites.
- **Verification:** AlisEditor Development built cleanly; the host transaction Pester test passed;
  `Project.Material` passed 7/7; all 417 inline schemas passed. The final Shipping cook compiled
  `M_ProjectTerrain` for PCD3D_SM5 and PCD3D_SM6, cooked 6,858 packages, passed archive integrity and
  IoStore policy, and produced 1.920 GiB with a 1.168 GiB largest container. The exact final package
  passed the Kazan fixed-view gate with no visible terrain fallback or section seam. City17's
  separate authored Landscape still exceeds the SM5 sampler limit.
- **Remaining:** T7 has generated object-local parent/instance authority and parser/compiler proof,
  but no move/rotate packaged visual. T8 therefore remains open for that proof and independent R2.
  The release wrapper also spends minutes hashing dirty generated assets one path at a time before
  and after a roughly 104-second BuildCookRun; optimize that owner in a separate task.

### 2026-09-28 - universal pattern boundary and native kernel checkpoint (PATCH)

- **Reviewer findings accepted:** the recipe-to-object-path coupling, duplicated signal strengths,
  false triplanar names, proof-only sine pattern, and premature cache conclusion were real.
- **Black-box boundary:** a ProjectMaterial recipe now carries only `pattern_id`. The accepted
  ProjectTexture manifest resolves the function path, semantic identity, package hash, and the
  `surface_structure_scalar_v1` ABI. ProjectMaterial neither knows nor validates ProjectTexture's
  algorithm or compiler version; tests prove an alternate safe ID parses, an arbitrary producer
  algorithm/compiler resolves, a wrong output ABI fails, and changed package bytes fail.
- **Owner-clean signal ABI:** ProjectTexture owns deterministic seed, scale ratios, and three
  normalized named signals. ProjectMaterial owns macro, structure, and detail appearance strengths.
  The duplicated strengths and hard-coded centering constants are removed.
- **Truthful implementation:** `world_metric_3d` and `object_metric_3d` replace the false triplanar
  contract. The deterministic function uses two filtered native gradient noises and one filtered
  native value noise, each bounded to `[0, 1]`, with derivative filter width and one level.
- **Verification:** AlisEditor Development built cleanly; focused recipe, pattern-authority, and
  generated-asset compiler controls passed; all 417 inline schemas passed. Regeneration and its
  immediate rerun produced the same accepted authorities with zero rewrites and zero shader
  compiles on the rerun. A fresh Shipping BuildCookRun compiled `M_ProjectTerrain` for SM5 and SM6,
  cooked 6,789 runtime packages, passed archive integrity and IoStore policy, and produced a
  1.924 GiB payload. No ProjectWorld or Mesh Terrain code changed.
- **Separate debt:** the same cook reproduced City17's authored Landscape SM5 16-sampler overflow.
  It is not emitted by the generated Mesh Terrain path and remains outside this task.
- **Remaining:** one final direct-graph performance capture; the packaged cube move, rotate, and
  non-uniform scale proof; accepted Kazan and Manhattan close/far/hydro views; independent R2.
  Material Cache remains absent unless the final profile first names a shading bottleneck.

### 2026-09-28 - runtime DAG/cache implementation and final Shipping gate (R2 REQUIRED)

- **Implemented:** ProjectTexture v3 now owns a real two-node native runtime DAG, authenticated
  catalog, stable sampled output slot, GPU generation, mips, Wrap addressing, exact byte accounting,
  one-writer state, pins, budget refusal, and unpinned LRU. ProjectMaterial consumes only the stable
  pattern ID and owns triplanar projection and appearance. ProjectWorld owns neither compiler nor
  cache calls.
- **Production selection:** an equivalent direct four-noise arm and the cached arm were packaged
  and captured under the same Development route. Cached output preserved the terrain read, improved
  distant filtering, reduced payload by 4,836,032 bytes, and shares one 1,398,100-byte ready output
  across 1/10/100 consumers. The temporary direct arm was then removed.
- **Instrument limit:** the official packaged performance gate crashes both arms before sampling
  with the same UE 5.8 CSV-profiler assertion. That discriminates the collector failure from a
  material-arm failure but leaves frame/GPU timing unverified. No performance number is inferred.
- **World corrections:** the implementation also fixed renderer-facing Mesh Terrain triangle
  winding and included the adapter compiler fingerprint in the base identity/receipt so stale
  realized sections cannot survive adapter-code changes. Exact known-bad controls cover both.
- **Verification:** 19 exact tests passed in one current editor process. Pattern regeneration no-op
  was `generated=0`, `skipped=1`, `shader_compiles=0`; surface regeneration no-op was
  `generated=0`, `skipped=4`, `shader_compiles=0`. Kazan and Manhattan realization reruns were
  zero-mutation after their accepted rebuilds. The current Shipping route built AlisEditor and
  Alis, cooked 6,791 packages, compiled the generated terrain and object parents for SM5/SM6,
  passed Pak/IoStore/archive/source-state checks, and produced 2,063,109,020 bytes. Kazan,
  Manhattan, a replacement ground-detail view, and the moved/rotated cube passed packaged
  fixed-view receipts and visual inspection. The ground view proves stable metric projection and
  broad structure, not final photoreal microdetail.
- **Reviewer correction:** requiring a Pareto win in every metric or treating the shared profiler
  crash as a cache rejection would be incorrect. Promotion is supported by bounded cardinality,
  package, repeated-work, filtering, locality, and product-render evidence; timing remains an
  explicit gap. City17's authored Landscape sampler overflow remains unrelated debt.
- **Remaining:** the runtime output must first transport a real alpha channel or move to a truthful
  smaller current-only ABI. The per-channel GPU regression fails only `DetailHeight` at mip 0 and
  mip 4; UI/translucent and Surface/translucent produced empty targets and were removed. After the
  ABI is repaired and the equivalent A/B/package/visual evidence is refreshed, independent R2 and
  a repaired/replaced frame instrument remain. The original invalid close-detail frame is excluded;
  its replacement is accepted only for projection and broad-structure evidence.

### 2026-09-28 - installed Canvas audit and RGB ABI correction

- **Reviewer point rejected:** in installed UE 5.8, `FCanvas::SetWriteDestinationAlpha` affects the
  texture `DrawTile(..., AlphaBlend)` overload. `UCanvas::K2_DrawMaterial` builds a material tile
  and bypasses that branch; `BLEND_AlphaComposite` maps directly to `SE_BLEND_AlphaComposite`.
  The proposed setter therefore does not enable destination-alpha data writes for this material
  draw path.
- **Implemented:** current-only ProjectTexture output contract `surface_structure_rgb_v1`. The
  basis stores macro, medium, and a bounded aggregate/high-frequency blend in RGB; the child derives
  `MacroVariation`, `CoverPatch`, and `GroundDetail` using only parent RGB. Generator opacity is
  constant, and ProjectMaterial uses blue for normal and roughness responses. One RGBA8 output,
  slot, DAG, cache, and triplanar sample count remain; no RDG, extra texture, or World regeneration
  was added.
- **Generated content:** ProjectTexture accepted one generated output; ProjectMaterial accepted
  all four. Repeat generations skipped 1/1 and 4/4 outputs with zero shader compiles.
- **Verification:** `scripts/ue/build/build.bat AlisEditor Win64 Development` succeeded.
  `Project.Texture.Runtime.TwoNodeGeneration` passed 1/1 after asserting RGB range, pairwise channel
  distinction, wrap continuity, mip 0/mip 4, cache reuse, and package-byte stability. Full JSON data
  validation passed 434 inline schemas. The first ProjectMaterial generation compiled two shaders;
  the no-op rerun compiled zero.
- **Still open:** no post-change close-ground Development frame was captured. The existing
  `VisualVerification` planner requires `LandscapeStreamingProxy` descriptors and is not a valid
  capture route for the production Mesh Terrain map. The old Shipping package is not final
  acceptance; performance timing and independent R2 remain open. No world regeneration or Shipping
  package was run for this correction.

### 2026-09-29 - receipt, sampler, and normal review (PATCH)

- **Receipt check:** the current layout receipt's authenticated `receipt_sha256` is `234ac9a5...`;
  both accepted terrain records contain that value. The raw JSON file hash is different because the
  manifest field is the canonical receipt-payload identity, not a byte hash of JSON serialization.
  The historical `a4765012...` receipt itself is not retained, so its exact field-level difference
  cannot be reconstructed. The material wrapper requires an explicit receipt path; no stale-file
  auto-selector exists. No World regeneration was needed or run.
- **Accepted corrections:** ProjectTexture now marks render targets non-sRGB and samples DAG data as
  `LinearColor`; ProjectMaterial samples the same linear data. The current generated normal was one
  scalar copied into both XY components, so the texture-derived normal was removed. `GroundDetail`
  now affects roughness only. ProjectMaterial recipes reject the removed `StructureStrength` field
  under schema v4/compiler v5; ProjectTexture remains schema v3/compiler v5.
- **Verification:** AlisEditor Development build passed; exact material graph, removed-field, and
  runtime DAG generation tests passed. Pattern regeneration accepted one output and its rerun skipped
  one with zero shader compiles. Surface regeneration accepted four assets and its rerun skipped all
  four with zero shader compiles. JSON validation passed 434 schemas.
- **Still open:** no Shipping package, frame timing, or independent R2 was performed. The first
  current Development view set is only a visual sanity check: the reframed close view shows broad
  color breakup but not convincing physical microdetail, so do not call material quality final.
  No World regeneration was performed.

### 2026-09-29 - packaged Development visual sanity check

- **Execution:** the project package script built Alis and AlisEditor Development, cooked 6,860
  packages, compiled the generated terrain material for SM5 and SM6, and passed IoStore/archive
  verification. The full configured map set was cooked. Five packaged fixed-view receipts were
  accepted: player-height, oblique, and three close-ground attempts. The initial close framing put
  near geometry across the image; the first open-ground replacement still showed a horizon. The
  final downward close framing is the useful close-ground evidence.
- **Visual:** the usable player-height, oblique, and downward close frames show generated ground
  shading and broad green/soil breakup; no obvious fallback material or projection seam is visible.
  The close frame does not establish convincing fine physical detail after removing the false
  duplicated-scalar normal. Keep geometric normals and roughness-only `GroundDetail` for now; any
  height-derived normal is a separate quality decision, not a reason to restore the fake normal.
- **Artifacts:** `tmp/material/rgb-normal-review-development/visual-player-height/fixed-view.png`,
  `visual-oblique-slope/fixed-view.png`, and `visual-ground-close-down/fixed-view.png`.
- **Still open:** no Shipping acceptance, frame timing, or independent R2 was performed. No World
  regeneration was performed.

### 2026-09-29 - same-output detail-scale experiment (rejected)

- **Hypothesis:** a second triplanar sample of the accepted Structure render target could expose
  close detail without another runtime texture resource. The candidate used the same shared
  projection-weight basis, read only B, and fed roughness plus bounded albedo variation.
- **Evidence:** packaged Development builds at `DetailScaleMeters` 2, 4, and 16 were captured with
  matching Kazan close-ground, player-height, and oblique cameras. All fixed-view receipts were
  accepted. None showed a convincing close-detail improvement over the baseline broad breakup;
  the 16 m candidate also did not reveal new distant noise or seams.
- **Decision:** remove the candidate parameter and graph branch. It added three per-pixel terrain
  fetches with no demonstrated appearance gain, while the available frame collector remains invalid.
  Keep the current v4/compiler v5 three-sample graph and do not claim the close-ground quality gap is
  closed. Do not derive normals from `GroundDetail`, which is not an authored height signal.
- **Frequency note:** the current ProjectTexture basis uses 64-repeat detail noise and 12-repeat
  medium noise inside one output tile. A consumer's metric tile scale therefore does not equal the
  embedded signal period; the effective periods are the tile scale divided by those repeat counts.
  Future scale candidates must account for that producer frequency and still pass packaged views.
- **Artifacts:** baseline captures are under `tmp/material/rgb-normal-review-development/`; the
  rejected 2 m and 16 m builds and their matching captures are under `tmp/material/detail-scale-2m/`
  and `tmp/material/detail-scale-16m/`. The 4 m build and captures are under
  `tmp/material/detail-scale-development/`. No World regeneration was performed.

### 2026-09-29 - box-first metric-scale and UE sampler investigation

- **Trigger:** the operator requested a simple-box scale proof before changing the terrain material,
  plus an installed-engine-source check of where tiling occurs relative to one Mesh Terrain part.
- **Finding:** the current graph already projects in world meters for terrain and object-local meters
  for the cube. The complete tile repeats at `PatternScaleMeters` because the ProjectTexture output
  wraps and the material samples world/object meters divided by that scalar. Mesh Terrain sections do
  not own/restart the UV period. Tile recurrence, edge continuity, physical feature size, and section
  continuity must be judged separately. The current cube screenshot and historical 2/4/16 m
  secondary-channel experiment do not prove the primary scale mapping.
- **Fix:** recorded UE 5.8 source evidence E40-E45, added the ordered box-first/terrain-second gate,
  made the transient scalar/bounds/scale receipt need explicit, and added M-24/M-25 and T9. Refuted
  `DetailNormal` as a cure for the current periodic tiling issue. No stable architecture or owner
  contract changed.
- **Verification:** read-only inspection of ProjectMaterial's coordinate/sample builder,
  ProjectTexture's tiled noise and wrap setup, the fixed-view PowerShell/C++ route, and installed UE
  5.8 `MaterialExpressionTextureSample.h`, `MaterialExpressions.cpp`, and
  `HLSLMaterialTranslator.cpp`. Official Epic world-aligned texture docs were checked. No code,
  generated asset, build, shader compile, test, package, or Unreal process changed or ran; only this
  TODO was edited.
- **Authority:** D20; Q5 remains open and non-blocking, with motif recurrence as the working
  hypothesis until a calibrated view distinguishes it from a hard seam or part-boundary restart.

### 2026-09-29 - packaged box calibration and Mesh Terrain scale probe

- **Boundary:** only the transient packaged fixed-view fixture and Mesh Terrain diagnostic probe
  were used. ProjectTexture/ProjectMaterial recipes and generated assets, canonical terrain, and
  generated World packages were untouched. The captured executable is the existing Development
  package with the previously built test binary; these captures did not cook or compile shaders.
- **Box control:** accepted packaged receipts for a 100 cm local cube at PatternScaleMeters 0.25,
  0.5, and 1.0 record matching camera, unit actor scale, MacroStrength 0.8, and the exact scalar
  readback. The material graph converts local Unreal centimeters to meters, divides by the scalar,
  then samples a wrap-addressed output. Therefore the 1 m face has 4/2/1 complete output periods.
  The frames show the expected increase in physical feature size as the scale rises. Separate
  accepted captures cover actor scales 0.5 and 2.0, nonuniform scale 1/2/0.5, and moving from
  (100400, 100000, 10050) to (100600, 100000, 10050) before rotation to (0, 45, 15). These are
  transform/appearance checks, not an animation-time swimming test.
- **Terrain control:** packaged Kazan captures used one camera at (100000, 100000, 25200), looking
  toward (100000, 100000, 10200), BaseColor isolation, and the same material path. The accepted
  48/96/192 m transient direct-render-component probes each matched 26 compiled sections, overrode
  26 render components and 26 slots, and reported zero Material Cache textures. At MacroStrength
  0.8, 48 m has finer/more frequent visible breakup, 96 m is intermediate, and 192 m is broader and
  quieter. The ordinary unmodified product capture at the recipe's 96 m / 0.12 defaults was also
  accepted and visually matches the same-value transient capture.
- **Interpretation:** the live Mesh Terrain material path responds to PatternScaleMeters, and no
  obvious restart is visible across the sampled field. The full 96 m output-tile period is distinct
  from the embedded signal periods (24/8/4/1.5 m); screenshots prove scale response but do not yet
  identify a recognizable 96 m recurrence or inspect every section edge. Q5 therefore remains
  open. An earlier high-altitude capture was rejected by the diagnostic probe because the probe
  origin exceeded its search radius; lowering the origin corrected the test, and the final captures
  all have accepted receipts.
- **Evidence:** box frames are under `tmp/material/tiling-scale/box/`; terrain frames and separate
  probe receipts are under `tmp/material/tiling-scale/terrain/`. The exact production default frame
  is `production_topdown_default_material/fixed-view.png`; transient terrain controls are
  `direct_topdown_pattern_48_strength_08/fixed-view.png`,
  `direct_topdown_pattern_96_strength_08/fixed-view.png`, and
  `direct_topdown_pattern_192_strength_08/fixed-view.png`.
- **Verification:** accepted packaged Development fixed-view receipts for all box and terrain
  controls; source formula and wrap-address settings rechecked. No World regeneration, persistent
  recipe update, material generation, shader compilation, package cook, or live Editor was run.
  The packaged processes exited through the wrapper; no Alis or UnrealEditor process remains.

### 2026-09-29 - production-strength terrain scale decision

- **Production control:** accepted packaged Kazan BaseColor captures compare PatternScaleMeters
  64/96/128 at MacroStrength 0.12, with the same camera, material path, and transient direct
  compiled-section override. Each probe matched and overrode 26 sections/components/slots. The
  64 m image is somewhat busier, 128 m broadens the patches, and the existing 96 m value remains
  the most balanced of these sampled views. No persistent recipe, generated asset, World package,
  shader, or cooked package changed. The 128 m value was diagnostic only; the persistent schema
  currently caps PatternScaleMeters at 100 m.
- **Wider view:** packaged, unmodified 96 m / 0.12 Kazan views at a higher camera altitude were
  accepted. No obvious hard texture restart is visible in sampled terrain, but roads/buildings
  interrupt the surface and the camera was not placed against an independently identified compiled
  section boundary. Do not report these views as complete section-edge or non-repetition proof.
- **Decision:** keep 96 m and do not add anti-tiling machinery. If a later controlled continuous
  view exposes a recognizable full-tile motif while smaller ground features must stay the same
  size, investigate ProjectTexture's tile period and internal frequency separately. Retire or
  development-gate the MeshTerrain-specific diagnostic probe after T9 is decided.
- **Evidence:** `tmp/material/tiling-scale/terrain/direct_topdown_pattern_64_strength_012/`,
  `direct_topdown_product_pattern_96_macro_012/`, and
  `direct_topdown_pattern_128_strength_012/` contain the production-strength frames and probe
  receipts. `production_wide_topdown_96_strength_012/` and
  `production_wide_empty_cell_96_strength_012/` contain wider unmodified packaged frames. A
  candidate farther outside the realized terrain rendered black despite an accepted camera
  receipt and is excluded from visual conclusions.

### 2026-09-30 - T9 section edge, recurrence, and cleanup

- **Exact edge:** the accepted current Kazan canonical compile result was audited against the
  generated map (`315` compiled sections). The Editor audit's actual LOD0 render vertices place
  sections `CompiledSection_UAID_7085C2D2E177150603_1821759033` and
  `CompiledSection_UAID_7085C2D2E177150603_1821779034` on opposite sides of `Y=-562900 cm`,
  overlapping `X=[-722900,-536900] cm`. The packaged Development diagnostic independently
  matched those two loaded section identities at the fixed-view subject. Actor bounds were
  overconservative and cooked CPU vertex positions were unavailable; neither was used to infer
  the final edge. Temporary audit instrumentation was reverted after recording its receipt.
- **Product view:** `tmp/material/overnight/visual/known_open_section_edge_product/` captures
  the unmodified 96 m / 0.12 material with player/target X=-630000, Y=-562900 cm and
  Z=41600/26600 cm. The continuous open ground has no visible hard restart at the identified
  ownership edge. The diagnostic frame/receipt is in the adjacent
  `known_open_section_edge_probe/` directory. This is bounded visual continuity evidence,
  not proof for every section or lighting condition.
- **Recurrence:** `tmp/material/overnight/visual/low_feature_scout/`,
  `period_plus_96m/`, and `half_period_plus_48m/` contain fixed-camera product BaseColor
  captures on a low-feature Kazan cell. A motif shifts as expected at +96 m while the +48 m
  control differs; the wide view spans multiple periods without objectionable recognizable
  stamping at the production strength. Keep the 96 m recipe and do not start a producer change.
- **Cleanup:** the MeshTerrain-specific runtime probe and its external-scalar fixed-view flag
  were removed. The reusable fixed-view scalar fixture stays in ProjectWorld's testing seam.
- **Generation:** ProjectTexture validated and regenerated twice with `generated=0`,
  `skipped=1`, and zero shader compiles on both regenerate runs. A fresh source-bound layout
  receipt changed from the accepted terrain-material manifest identity; ProjectMaterial
  regenerated only the two terrain outputs (`generated=2`, `skipped=2`), then reran as
  `generated=0`, `skipped=4`, zero shader compiles. No World/geography asset was rewritten.
- **Focused gates:** AlisEditor Development compiled after probe deletion; fixed-view 1/1,
  ProjectTexture 9/9, ProjectMaterial 7/7, Mesh Terrain seam 1/1, and all 434 inline JSON
  schemas passed. The current generated terrain package hashes match the accepted material
  manifest; the only source-asset rewrites were the two terrain materials and their manifest.
- **Final Development:** the current-source package built and cooked both SM5 and SM6 terrain
  shadermaps, passed archive integrity and IoStore policy, and measured 2.124 GiB with a
  1.168 GiB largest container. `tmp/material/overnight/visual/final_development/` contains
  accepted packaged Kazan close/player, oblique, hydro, aerial recurrence, and exact edge views;
  Manhattan oblique/far views; and generated cube initial, moved/rotated, and nonuniform-scale
  controls. The first Kazan close candidate faced a wall and was excluded; the replacement
  open-ground/player frames are the visual evidence. No fallback or hard terrain-section seam
  was visible in those sampled views. The cube proof uses transient 0.5 m / 0.8 diagnostic
  scalars to make its metric pattern legible; no authored instance changed.
- **Performance instrument:** the standard CSV route previously crashed before sampling.
  A bounded native Unreal Insights attempt captured packaged Kazan CPU/GPU frame events,
  but its 39-frame fixed-view window includes loading and screenshot-exit stalls, and the
  traced process reported an anomalous nonzero exit after an accepted view receipt and clean
  shutdown log. These exploratory frame events are not the authenticated gameplay performance
  route, so the performance budget remains UNVERIFIED. Evidence is in
  `tmp/material/overnight/performance/native_trace/`; no renderer/material change follows.
- **Independent R2:** reviewed the production diff, manifest/asset identity, section-edge
  evidence, and packaged Development board; found no production-code defect. It identified
  two stale TODO status statements, corrected above. R2 did not independently rerun builds.
- **Source targets:** configured UE 5.8.3 source engine built `AlisClient Win64 Shipping`
  and `AlisServer Win64 Development` to executable and target metadata with exit code 0.
  The Server manifest lists ProjectTexture/ProjectMaterial but no TextureGraph plugin or
  ProjectTextureEditor/ProjectMaterialEditor runtime dependency; its UBT log contains no
  compile action for either editor compiler. The ProjectTexture subsystem's
  `ShouldCreateSubsystem` refuses dedicated Server creation. UBT also compiled UE RHI and
  Renderer modules for this Server target, so the previous broad renderer-free acceptance
  wording was false and is narrowed above to the material-specific boundary.
- **Final source release:** the first source-release attempt stopped during its Editor-side
  object-definition export because the source Editor lacked the newly built `ProjectCore` module.
  The nearby VisionOS SDK validation warning was nonfatal. Building the configured source
  `AlisEditor Win64 Development` and rerunning the exact exporter produced a byte-identical
  contract to the launcher control. One replacement source-release run then built Game Shipping,
  cooked all configured maps with SM5/SM6 shader platforms enabled, and passed archive and
  IoStore checks. The release payload is 2,068,414,093 bytes (1.926 GiB), its largest file is
  1,254,631,664 bytes
  (1.168 GiB), and the one 1,886,274,499-byte archive is below the split threshold. The source
  release summary is `tmp/material/overnight/final_shipping_package/package_summary.txt`.
- **Shipping product:** accepted fixed views under
  `tmp/material/overnight/visual/final_shipping/` cover Kazan player and the exact known section
  edge, Manhattan oblique and far terrain, and the moved/rotated generated cube. A source-built
  Shipping Client, temporarily placed beside the staged Game executable to reuse the identical
  cooked content, also accepted the Kazan fixture; its probe copy was moved out of the staged
  package afterward, so the release summary remains the packaged Game inventory. Game runtime
  logs show ProjectTexture's two-node Structure output ready at 1,398,100 resident and
  2,796,200 peak bytes. A separate Game run with `-sm5` accepted the Kazan fixture; its D3D12
  log confirms creation at Max Feature Level SM5. The Client also accepted the same fixture when
  passed `-sm5`, although that target emitted no runtime log to independently record the selected
  feature level. No material fallback is visible in the sampled terrain or cube views.
- **Shipping Water:** the existing proof ran on the final Game package with a real character
  input route, reached the requested Water area, and captured accepted base-color and final-color
  views. The temporal control accepted 2,010,369 blue pixels with zero classification flips and
  zero mean blue-channel difference. Receipts/images are under
  `tmp/material/overnight/visual/final_shipping/water/`.
- **Dependency and payload boundary:** source Client Shipping and Server Development builds
  succeeded; Game/Client/Server target manifests include ProjectTexture and ProjectMaterial but
  no TextureGraph, ProjectTextureEditor, or ProjectMaterialEditor plugin. The generated pattern
  is analytic and introduced no imported pixel texture as authority. The final cook reported an
  SM5 sampler overflow in City17's separately authored `M_Landscape`; it did not report a
  generated Mesh Terrain material failure and is outside this task's cutover boundary.
- **Remaining:** the official CSV collector still crashes before sampling both equivalent arms,
  and the bounded native trace did not yield authenticated steady gameplay frame/GPU timing.
  Packaged cold/warm cache timing is also not authenticated. Performance acceptance is
  UNVERIFIED. Keep T8 and this TODO open; do not infer PASS from
  successful visual, build, and package checks alone.

### 2026-09-30 - identity locality and final-package performance (DELTA R2 OPEN)

- The old and new layout receipts differed in `adapter_compiler_sha256`, not engine, layout,
  shared MPD, or build policy. Deleting the runtime-only diagnostic probe did not participate in
  that editor-module fingerprint. The old input scan did include audit/receipt commandlets and
  omitted runtime shapers. The explicit fingerprint set now hashes realization/layout shapers,
  their build policy, and schema but excludes diagnostics/tests. A discriminating Pester control
  stayed unchanged for diagnostic bytes and changed for producer bytes (8/8).
- An accidental no-runtime-profile replay exposed a real wrapper gap: a realization profile
  declared a runtime ID, but the wrapper accepted its omission and retired runtime actors. The
  wrapper now rejects a missing/mismatched runtime ID before content mutation. The focused test
  observed RED (it previously launched Unreal) and GREEN after the guard. Both cities were
  restored with their exact declared runtime profiles through the normal transaction route.
- The guard changed the existing common World producer fingerprint even though it could only
  reject inputs; the next apply unnecessarily dirtied all layers. The final same-input Kazan and Manhattan applies then had zero
  dirty units and zero created/updated/removed actors. The authority audit accepted 14 scopes
  and 3,096 byte-identical artifacts. The current layout receipt is
  `Saved/Validation/WorldRealization/fingerprint-locality-final/current-layout-receipt.json`.
  Material regeneration generated only two terrain outputs, then generated zero on rerun with
  zero shader compiles; validation accepted.
- The final Development package is `tmp/material/native_steady/final_development_verified/`.
  Its normal Kazan product route accepted. Its 600-frame native receipt at
  `tmp/material/native_steady/final_runtime_verified_2/native-steady.json` reports frame p50/p95/p99
  13.07/14.64/15.51 ms, engine-reported GPU p95 9.65 ms (possibly inferred), RHI p95 5.57 ms, and zero streaming failures at
  2560x1440 High D3D12 SM6 on the declared RTX 4070 profile. The existing frame p95 budget is
  16.67 ms. The separate ProjectTexture log records 13.24 ms cold CPU request-to-Ready, 0.0058 ms
  warm, two cold DAG-node generations, zero warm generations, one cache hit, identical resource,
  1,398,100 resident bytes, and 2,796,200 peak bytes. It does not claim GPU-complete latency.
- The refreshed source Shipping package is `tmp/material/native_steady/final_shipping_verified/`:
  Game build, 6,860-package cook, archive/IoStore checks, 2,068,427,405-byte payload, and
  1,886,274,151-byte release zip accepted. Kazan player and Manhattan oblique/far fixed views
  from its staged executable accepted. The source editor first required an incremental rebuild
  to load current `ProjectSave`; the exact exporter then passed. City17's authored Landscape
  16-sampler SM5 warning remains outside this generated-terrain scope.
- Earlier independent R2 covered the production architecture before this delta. A fresh
  independent review of the fingerprint/performance/guard delta has not run. Do not mark T8
  complete or move this TODO to done until that review accepts the final diff.

### 2026-09-30 - final locality correction and impact-driven workflow (R2 OPEN)

- The existing pre-correction manifests and the accepted source tree had the same 3,096 artifact
  paths. The prior broad dirty run was caused by the
  common World wrapper fingerprint including an admission-only runtime-profile guard. The
  approximately 1,996-file earlier diff also included restoration after the missing-runtime
  replay; it cannot be attributed to the guard alone.
- `generator_fingerprint.ps1` now excludes the explicitly marked admission-only wrapper region
  and refuses unmatched region markers. Focused tests demonstrate that changing that guard
  leaves all producer fingerprints stable, while changing wrapper producer behavior changes
  them. The Mesh Terrain adapter test also changes its runtime transformer and observes the
  adapter compiler fingerprint move. No geography, semantic, or material producer was edited.
- The fingerprint-definition change made the active manifests stale once. Two supported Apply
  transactions were already in flight before the new no-regeneration request; both committed.
  A read-only audit now accepts 14 current scopes and all 3,096 owned artifact bytes. Comparing
  current manifests with HEAD gives the same 3,096 paths, unchanged input identities and layer
  contracts, and 277 changed artifact byte digests (primarily vegetation serialization). This is a
  one-time refresh cost, not evidence of changed geography. No further generation, cook, or
  package is part of this development pass.
- The collector now reserves its known 600-frame capacity before capture. The exact collector
  automation test passes, as do the focused fingerprint tests, PowerShell parse checks, and an
  incremental Editor build. The installed UE 5.8.3 engine path can infer `FFrameData` GPU time;
  therefore the old 600-frame receipt proves only the selected settled Kazan frame budget, not
  a direct-vs-cached GPU delta or all traversal routes. ProjectTexture was Ready in the packaged
  log before the route's native capture; its separate cold/warm proof is CPU request-to-Ready.
- The existing testing and build workflow now chooses gates by changed output/dependency,
  reuses unchanged cooked content where valid, and reserves full Shipping/cross-city acceptance
  for a release candidate or an otherwise unprovable packaged property. Release authenticity,
  authority audit, and transaction rules remain unchanged. A fresh independent R2 over this
  final delta remains the only closure step; the prior Shipping package is not recertified for
  the latest diagnostic code or refreshed generated bytes.

### 2026-09-30 - independent final delta R2 (PATCH)

- **Scope:** the `a23a2a39d` source/doc delta, a read-only authority audit, and a manifest
  comparison with its parent. No generation, cook, package, stage, or commit.
- **Fingerprint defect fixed:** the admitted region only computes and compares the runtime ID; that
  value is unread after the region, and the wrapper's first write (evidence directory) follows it.
  The marker count, however, counted only well-formed marker lines, so a malformed END inside a
  region silently extended the exclusion to the next valid END: a probe edit of the hidden
  producing line left the digest unchanged. The parser now requires every marker token to delimit a
  well-formed region. The new test failed before and passes after; the fingerprint suite passes
  11/11; all 8 real producer fingerprints are identical before and after the fix; the authority
  audit accepts before and after. `territory_contract.md` now states the admission-only rule.
- **Guard coverage:** `realization_runtime_profile.Tests.ps1` now rejects missing and mismatched
  runtime profiles and admits the declared one, stopping at the next pre-write refusal with no
  evidence written. Mutants (no guard, presence-only guard, always-throw guard) are each killed by
  the expected arm. World fast suite: 127/127.
- **Authority:** 14 scopes and the same 3,096 paths; input identities and layer contracts
  unchanged; 277 byte digests changed and equal the 277 committed packages (Kazan 146 and
  Manhattan 125 vegetation actors, 3 replaced Kazan gameplay actors, 2 water materials, 1 map).
  The active-set chain parent -> Kazan Apply -> Manhattan Apply equals HEAD. Manifests record
  byte digests only, so semantic equality is inferred from unchanged inputs and producers, not
  measured.
- **Performance:** the capacity test exercises the collector calls and 600-frame cap the gate
  uses; the gate wiring (one `steady_center` route, reserve, cap, streaming rejection) is
  inspected, not unit-tested. The receipt has 600 samples, zero streaming failures, and
  ProjectTexture Ready before capture; it predates the reserve patch. Boundaries: frame p95
  14.64 ms for the selected Kazan package only; GPU p95 engine-reported and possibly inferred;
  cold/warm is CPU request-to-Ready; the historical direct-vs-cache frame delta is unverified.
- **New required defect:** `scripts/git/mirror/developer_asset_release.json` still declares the
  V1 `Data/Manifests/Materials/accepted.material-manifest.json` and `*.material.json` recipes, the
  composer loads the V1 verifier deleted in `54de1c9ad`, and ProjectTexture is undeclared. Of the
  13 `test_developer_payload.py` tests that completed, 6 fail with that missing-manifest error, so
  `prepare_release_inputs.ps1` (R5 step 4) and `make mirror` (R6 step 1) cannot compose the
  payload. Minimal gate: the payload unit tests plus payload `plan` on the current tree. Design is
  Q6. The credential-pattern test, which spawns Git Bash per case, stalled in this shell, so the
  last 3 tests did not run; that stall is unrelated to this delta.
- **Rows without evidence:** Manhattan frame/memory/hitch, shader/PSO inventory, current-source
  Shipping, clean public checkout, and operator visual acceptance are packaged-product properties;
  the 3.0.0 router's R2 now assigns them to R5. M-10 and the rollback drill never ran; the V1
  migration window they protected has closed, so retiring them needs operator acceptance.
- **Task ownership:** the duplicate ProjectTexture runtime-cache backlog task is closed as merged;
  this task owns its remaining items.
- **Verdict:** delta PASS; task PATCH until Q6 is implemented and the operator decides on M-10 and
  the rollback drill.

### 2026-09-30 - public payload and orphan-safety investigation (REVIEW REQUIRED)

- **Trigger:** "check our docs, audit issues and think how to proper fix", after the final delta
  R2 left Q6 and the unrun M-10 and rollback-drill rows open.
- **Root cause:** the V1 removal in `54de1c9ad` deleted the material verifier and V1 manifest but
  not their consumers: the payload contract, composer, schema, closure-audit seed set, and tests
  (E46, E47). The owner docs lost the transport contract at the same time (E52). Separately, the
  V1 orphan concern (E6) was never tested, and v2 cleanup still trusts an ungathered commandlet
  registry (E51).
- **Findings:** E46-E54. The final delta R2 claim that M-10 guarded only the closed V1 window is
  refuted; M-10
  now composes World exact restore with M-4. The stale Green evidence test list is replaced from
  E53. The credential-test stall is a login-shell dependency in the test (E54).
- **Fix proposed:** T10 and `## Decision` / Public payload authority and orphan safety.
- **Verification:** read-only. Payload tests reproduced the missing-manifest failure; installed
  UE 5.8.3 source read for registry startup; `bash -c` versus `bash -lc` timed. No code, asset,
  config, stable doc, stage, or commit changed; only this todo.
- **Authority:** Q7 and A14 added; Q6 unchanged and still open. D1-D20 unchanged.

### 2026-09-30 - T10 implementation (PASS; closure pending Q8)

- **Trigger:** the operator supplied the reviewed T10 patches and decisions (D21-D24) with the
  instruction to fix the issues.
- **Red:** the payload suite failed with the V1 missing-manifest error (6 tests) and the new
  recipe-authority cases found no recipe authority (8 cases); both owners' `Validate` accepted a
  changed recipe source; the surface commandlet deleted an orphan whose only referencer was an
  unloaded on-disk package; cleanup ran with a pending restore source. The two host tests were
  themselves stale (ProjectTexture wrote a V1 recipe; the surface test's edit anchor matched
  nothing) and were repaired first against unchanged code.
- **Fix:** ProjectCore `FProjectSha256::HashNormalizedText`; `recipe_source_sha256` in both owner
  manifests; `Validate` as a dry run of regeneration; manifest saved when its bytes change; one
  registry search per cleanup with candidates and refusal while gathering; host refusal while a
  World snapshot, projection rollback, or sibling journal can restore content;
  `generated_recipe_manifest` payload authority with cross-owner dependency checks; ProjectTexture
  closure seed; credential test on `bash -c` with a timeout.
- **Test corrections found on the way:** the first M-4 control was not unreferenced, because a
  byte copy of a package keeps a reference to the source package's path; M-4 now uses that copy as
  the holder of the orphan it copies, for both owners. The complete-payload control asserted 88
  ProjectObject definitions, stale since 2026-09-21 (80); it now derives every count from its
  manifest.
- **Verification:** ProjectCore normalization parity 1/1 against a Python digest (LF, CRLF, CR,
  BOM, non-ASCII); 8 affected owner tests 1/1 each; host tests 9/9 on the final code; payload suite 19/19; data
  validation 478 files. Removing the registry search makes each owner's M-4 fail at its retention
  assertion; the source was restored byte-identical. Production: pattern generated 1, surfaces
  generated 4 with 2 shader compiles; immediate reruns 0 generated and 0 compiles; `Validate`
  accepts; only `compiler_fingerprint`, `semantic_identity`, package, pattern, and dependency
  hashes changed besides the new digest; all five digests equal Python's on the real recipes. The
  World authority audit accepts 3,096 unchanged artifacts. The contract change re-stamped both
  definition manifests' `release_contract_sha256` through the refresh route; nothing else in them
  changed.
- **Dependency closure:** composition now works and the audit approves all 3,103 selected
  packages, including every surface and pattern package. It rejects 42 World-content dependencies
  that no payload authority selects or the audit's registry cannot resolve; none is reachable from
  a surface or pattern package. Recorded as a 3.0.0 pre-candidate item; Q8 asks how D24 treats it.
- **Independent R2:** PATCH, then PATCH on recheck. The required finding was that the cleanup
  guard keyed on journal files that World may write under any manifest root; it now keys on
  restorable snapshots. The recheck found four World test harnesses that keep outer snapshots under
  `tmp/world/<harness>/<run>/`; the guard now also refuses on any `snapshot` or `outer-snapshot`
  folder under `tmp/world/`, none of which exists in a settled checkout, and a host case covers it.
  The public World projection's missing content lock stays with the projection recovery task (M-10
  residual), which R2 accepted as recorded. Final R2 verdict: PASS, covering every
  `New-ProjectWorldGeneratedSnapshot` caller that restores live content and confirming no
  settled-state false refusal. Optional follow-ups not taken because each owner-source edit forces another
  generated-asset refresh: a warning when an unreferenced orphan's delete fails. Pre-existing and
  unchanged: a retained orphan leaves the manifest and is not reconsidered; the definition
  authority refresher writes CRLF on Windows.
- **Authority:** D21-D24 added; Q5 closed by T9 evidence, Q6 by D21, Q7 by D22; Q8 added; A13
  resolved, A14 confirmed.

### 2026-09-30 - T10 closure review (PAUSED)

- **Trigger:** the operator supplied the reviewed Q8 decision and a narrow cleanup correction, then
  asked to record this work and run the black-box independence correction first.
- **Findings:** the deletion finding is confirmed in both owners and the host rollback defect is
  new (E57); the reachability proof holds, and the audit's 42 are really 45 owned rejections
  (E55-E56). Refuted: that the shared lock alone is the permanent restore boundary; a crashed World
  transaction's snapshot restores later under that same lock, so a pending-recovery check stays
  necessary however the projection is fixed.
- **Verification:** read-only. The audit inventory and a full-registry probe were classified with
  the unchanged validator. No code, asset, stable doc, stage, or commit changed; only this todo.
- **Authority:** D25 added and D24 superseded; Q8 closed; T11 added.

### 2026-10-01 - T11 implemented and task closed (PASS)

- **Trigger:** the operator's GO for the black-box independence sweep with T11 in it (its D10).
- **Root cause:** E57: skipped deletion failures, and a host `finally` that deleted the snapshot a
  surviving journal names.
- **Fix:** fail-closed orphan deletion in both owners; the host keeps the operation folder while
  the journal exists and reports rejection plus rollback failure; `retained_orphans` in the
  commandlet receipt and host summary; owner docs describe the retained-orphan lifecycle; 3.0.0
  plan lists the 45 owned rejections.
- **Verification:** red: the held-open orphan case accepted the run; with only the new `finally`
  guard removed, recovery failed on the deleted snapshot. Green: both owners' held-open cases
  (journal and snapshot kept, next run restores exact bytes), host suites 9/9 and 4/4, the
  combined refresh (pattern 1, surfaces 4, reruns 0), both `Validate`, World audit unchanged,
  payload suite 19/19. Independent review: PATCH (G5 coverage, skill wording), then PASS.
- **Authority:** no change; D25 conditions met.
