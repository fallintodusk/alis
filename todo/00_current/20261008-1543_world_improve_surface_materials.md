# World Surface Presentation

**Status:** CURRENT - BrownEarth frozen; D32 surface diagnosis and D33 building-owner isolated correction verified; remaining-surface visual selection, production adoption and comparative GPU benefit OPEN
**Scope:** approved compacted dry-soil candidate in reversible Kazan/Manhattan previews and independent default-surface proxies; persistent adoption remains gated
**Release:** required 3.0.0 concern; release authority stays in the [release plan](00_release_3.0.0.md#authority-register)  
**Stable documentation owners:** [material generation](../../Plugins/Resources/ProjectMaterial/docs/material_generation.md), [pattern generation](../../Plugins/Resources/ProjectTexture/docs/pattern_generation.md), and [World territory contract](../../Plugins/World/ProjectWorld/docs/territory_contract.md)

## Contents

- Goal - Authority register - Non-goals
- Verified evidence - Current architecture - Problem and root cause
- Decision - Required invariants - Implementation tasks
- Test-first and verification plan - Documentation plan
- Rollout and rollback - Completion criteria - Review record

## Goal

Improve the scale, detail, colors, and lit readability of terrain, roads, building
massing, and water in existing Kazan and Manhattan worlds. Preserve geography and
geometry while making repeated appearance tuning an owner-local operation.

## Authority register

### Operator decisions

- **D1** Use the reviewed terrain-first plan, then improve default water, road,
  and building-massing material presentation.
  - Reason: improve materials.
  - Source: operator request, 2026-10-08.
- **D2** Respect ProjectTexture -> ProjectMaterial -> ProjectWorld black-box
  boundaries. Material-only tuning must not trigger World regeneration,
  canonical compilation, or broad rebuilds.
  - Reason: preserve modularity and locality guarantees.
  - Source: operator request, 2026-10-08.
- **D3** A first-time serialized material-reference migration is permitted only
  when necessary, with focused proof and rollback.
  - Reason: not stated.
  - Source: operator request, 2026-10-08.
- **D4** Reuse and link existing water, road, and building presentation backlog
  tasks; do not create competing sources of truth.
  - Reason: not stated.
  - Source: operator request, 2026-10-08.
- **D5** Use owner-local tests, scratch previews, and focused integration during
  development; editor or PIE is the fast visual acceptance path.
  - Reason: avoid repeated full builds, cooks, and World regeneration for debugging.
  - Source: operator requests, 2026-10-08.
- **D6 [SUPERSEDED IN PART by D8 - read-only alignment investigation timing]** This step prepares documentation only. Agree on this todo before
  implementing it or conducting the dedicated alignment investigation.
  - Reason: not stated.
  - Source: operator request, 2026-10-08.
- **D7** Audit the attached alignment feedback and fix these todos against actual
  code, architecture, goals, and previous agreements.
  - Effect: clarify the geometry/presentation handoff without implementing either
    concern or changing D2/D3/D6's locality and approval boundaries.
  - Reason: not stated.
  - Source: operator follow-up, 2026-10-08.
- **D8** Update the alignment todo and run its read-only editor investigation now.
  - Supersedes: D6's material-agreement prerequisite for that bounded read-only
    alignment investigation only. This material todo's implementation and visual
    adoption gates remain unchanged.
  - Reason: not stated.
  - Source: operator answer to the scope question, 2026-10-08.
- **D9** Run the agent's Editor in the background without gaining focus; update
  its existing launch instructions or scripts where necessary.
  - Source: operator follow-up, 2026-10-08.
- **D10** Implement and verify bounded T0a autonomously, including focused
  RED/GREEN tests, real MCP pixel experiments, rejection/revert and unchanged-World
  proof. Obtain one independent architect review at the completed boundary, apply
  justified PATCH findings and stop that review at PASS. Continue approved local
  prototypes; evaluate persistent in-editor saving separately in isolated fixtures.
  - Supersedes: D6's documentation-only limit for this bounded work and A3's
    investigation-only status. Does not authorize persistent production adoption,
    production migration, broad regeneration or release operations.
  - Reason: establish a fast reliable JSON -> ProjectMaterial -> live Editor loop.
  - Source: operator authorization, 2026-10-08.
- **D11 [SUPERSEDED IN PART by D12 - sample/landscape ordering]** Continue the authorized material work and compare actual rendered terrain
  against beautiful, optimized production references from the web. Do not make a
  4 m / 8 m / 16 m preference question the prerequisite for further investigation.
  - Effect: choose and evaluate the next local quality experiment from evidence;
    does not close Q2 or authorize production adoption.
  - Source: operator continuation and response to the visual-direction question,
    2026-10-08.
- **D12** Generate and inspect realistic soil through our existing pipeline first.
  Compare real soil patterns from the web and inspect UE sources. Apply the result
  to landscape only after its sample quality is approved.
  - Supersedes: D11's immediate terrain-comparison ordering, not its reference
    research or D10's bounded local-prototype authorization.
  - Reason: establish realistic source texture quality before landscape application.
  - Source: operator correction, 2026-10-09.
- **D13** Keep a separate normal texture map and use simple generated units that
  Unreal's out-of-box PBR material system consumes. Check this against our owners
  and update this todo.
  - Reason: keep surface detail and lighting responsibilities separate.
  - Source: operator follow-up, 2026-10-09. The statement about shadows is a
    technical premise, clarified in the soil/PBR decision below.
- **D14** Critically evaluate the attached reviewer feedback against code,
  architecture, goals and previous agreements; apply valid in-scope fixes and
  explicitly refute incorrect points.
  - Reason: not stated.
  - Source: operator review-and-fix request, 2026-10-09.

- **D15** Continue beyond infrastructure/PBR design checks: generate soil textures
  through ProjectTexture, render an isolated PBR sample under different lighting,
  and present actual screenshots before claiming high visual quality.
  - Effect: continue D10/D12's bounded local implementation and sample evidence;
    does not close Q2a or authorize city application or production promotion.
  - Source: operator continuation, 2026-10-09.

- **D16** Make maximum reuse of relevant out-of-box Unreal capabilities mandatory,
  including Experimental features, before engineering custom equivalents.
  - Effect: apply the standing [Native Engine First](../../docs/architecture/principles.md#native-engine-first)
    rule to subsequent soil work; prioritize native-tool comparison and live tuning.
    Does not close Q2a, authorize production adoption, or require enabling unrelated plugins.
  - Source: operator instruction accompanying the native-tool strategy review,
    2026-10-09.

- **D17** Refine only the winning LooseAggregates branch into SoftPores, MacroPatch
  and HybridCompact previews; reduce harsh pits, improve patch distribution and
  investigate broad repetition through the existing resource/material owners.
  Use matched close/grazing/3x3/near-mid-far/motion evidence and generated maps;
  update the affected todos and stop again at Q2a before city application.
  - Effect: bounded isolated quality work only; no new framework, production
    recipe adoption, World/canonical regeneration, staging or release operation.
  - Source: operator refinement request and supplied composition feedback,
    2026-10-09.

- **D18** Accept MacroPatch as the development direction, not Q2a completion.
  Combine SoftPores normal/grain/roughness response with independently controlled
  broad pigment variation and smooth distance composition. Use existing texture
  assets and one Slab; let mips filter detail before explicit fades. Compare at
  player-eye height toward 2/5/12/30 m, including grazing light and shader cost.
  - Effect: one bounded ProjectMaterial experiment only. No additional generated
    texture set, native-tool survey, framework, World/canonical regeneration or
    city application. Present evidence at Q2a; landscape permission remains open.
  - Source: operator Q2a PATCH and continuation, 2026-10-09.

- **D19** Continue beyond D18 through up to three focused source/material quality
  cycles, choosing ProjectTexture or ProjectMaterial from rendered evidence.
  Target realistic dry soil, compare against references and DetailMacro, retain
  the existing three-output bundle and reuse native engine facilities first.
  - Effect: transient owner-local candidates, exact checks and visual iteration;
    one independent architect review of the completed engineering milestone and
    concrete corrections. No production recipes/manifests, staging, World/canonical
    regeneration, Matrix, cook, release or city application. Alternative reference-
    based sources may be recommended, not adopted. Stop at Q2a or a demonstrated
    quality plateau; engineering review is not aesthetic approval.
  - Source: operator's attached autonomous quality-cycle authorization, 2026-10-09.

- **D20** Prioritize a focused native Texture Graph packaged-runtime/cache
  investigation over the proposed CC0 import benchmark. Preserve ProjectTexture
  as the public owner and compare against its current producer at equal outputs.
  - Effect: installed-source investigation and isolated candidates/exact checks;
    no production adoption, World/canonical regeneration, Matrix or release.
    The supplied request explicitly retains a permission boundary for an isolated
    cook when prior authorization forbids it. D19's no-cook limit has not been
    waived by a reviewer recommendation. Q3 records that boundary.
  - Source: operator's submitted runtime-investigation request, 2026-10-09.
- **D21** Investigate minimal native extensions when direct Texture Graph support
  is insufficient; a native limitation is not automatic abandonment or BLOCKER.
  - Order: native API/configuration, suitable subclass/thin ProjectTexture adapter,
    scoped project-local plugin patch/fork, then a small engine-source patch as
    last resort. Compare maintenance and measured benefits with the current producer.
  - Effect: bounded extension experiments/focused fixes; no uncontrolled engine
    fork, production adoption or broad regeneration. Preserve immutable identity,
    whole-bundle readiness, sharing, bounded resources and failure recovery, with
    one public cache authority. Review a changed plugin/runtime boundary independently.
    Pin UE, minimize diffs, add regression proof and document upgrade/rebase and
    license-compliant distribution before retaining a fork. Never silently copy
    Epic source into the public repository. Q3 remains the cook boundary.
  - Source: operator's additional native-extension authorization, 2026-10-09.
- **D22** Build, cook and execute the isolated Win64 Development Texture Graph
  fixture described by Q3, excluding city and production assets.
  - Supersedes: D19's no-cook limit only for that disposable backend proof;
    production and ordinary visual-iteration boundaries remain unchanged.
  - Source: operator's explicit answer to Q3, 2026-10-09.
- **D23** Complete the isolated native runtime/cache/PBR integration and up to
  three soil-quality cycles autonomously, with independent architect reviews at
  meaningful completed boundaries. Preserve D22's accepted execution milestone.
  - Effect: owner-local ProjectTexture implementation and focused tests, thin
    opt-in integration, justified private native fixes, instrumentation/failure
    injection, incremental fixture builds, necessary scoped recooks and repeated
    packaged execution are authorized. Prefer skipcook for source-only fixes.
    Prove actual GC/cancellation/shutdown, whole-bundle identity/sharing, budget,
    physical residency/release, equal-output CPU/GPU cost and real material pixels.
    Preserve the accepted producer as the default and ProjectTexture as the sole
    public generation/cache authority. Do not stop for routine technical choices.
  - Gates: production activation, legal/distribution decisions, city application,
    full World/canonical work and release remain unauthorized. Stop at final Q2a,
    a demonstrated unsafe architectural conflict or required scope expansion.
  - Source: operator's autonomous mission attachment and explicit GO, 2026-10-10.
- **D24** Continue autonomously after D23's focused technical proofs, using web
  references, installed UE sources and the appropriate implementation skills.
  - Effect: a focused native soil-quality pass in isolated scratch assets, followed
    by actual rendered comparisons and owner-local verification. Preserve the
    existing runtime/cache boundary; do not reopen completed infrastructure without
    a concrete defect. Production adoption, city application and release stay gated.
  - Source: operator continuation, 2026-10-10.
- **D25** Finish the remaining authorized isolated quality work and obtain an
  independent architect review of its completed boundary.
  - Effect: focused native material/source experiments and their verification,
    correction and documentation. Existing visual, production and release gates
    remain binding; this is not permission for city application or publication.
  - Source: operator continuation requesting completion with architect, 2026-10-10.
- **D26** Continue after the accepted D25 boundary and evaluate the supplied review.
  - Effect: one focused loose-particle/interior comparison at 512, retaining
    GranularRaised as compacted dry earth. Independently advance isolated road,
    building and water default samples through the proven Editor workflow.
    This changes prototype sequencing only; Q2a still gates landscape application,
    and Q1/Q2 still gate production archetypes, references and adoption.
    Reproduce an accepted exact source through the cooked backend before promotion;
    Editor graph proof is not transferable packaged acceptance.
  - Source: operator continuation with reviewer feedback, 2026-10-11.
- **D27** Complete the remaining soil layers without degrading the achieved source,
  so the same owner-local pattern can be reused for other environmental surfaces.
  - Effect: preserve GranularRaised's source/maps and near shading; compare bounded
    material-only distance composition, verify filtering, motion and exact restoration,
    and prepare one reviewable soil candidate. Existing Q2a, production and release
    gates remain binding; completion intent does not manufacture visual acceptance.
  - Source: operator soil-preservation and completion request, 2026-10-11.
- **D28** Keep Q2a open and calibrate the isolated soil's color and lighting before
  further artistic tuning. Preserve the achieved source and completed D27 review.
  - Effect: inspect the fixture lighting/postprocessing, use native unlit/HDR/display
    captures and gray/white controls, isolate normal/AO response, compare two bounded
    ProjectMaterial palettes and retain the cheaper graph unless the extra distance
    sample demonstrates worthwhile visible benefit. No texture/cache changes, city
    application, production settings/adoption, World regeneration or release.
  - Source: operator attachment requesting the bounded calibration pass, 2026-10-11.
- **D29** Accept calibrated BrownEarth as compacted dry soil for transient Kazan
  and Manhattan trials. This supersedes Q2a's visual hold for this candidate only.
  - Effect: use the existing three-output bundle and cheaper material composition;
    preserve the accepted linear palette and source pixels. Investigate metric
    projection and normal spaces, bind small loaded terrain previews reversibly,
    compare actual city lighting with matched cameras, inspect slopes, seams,
    roads/water transitions and motion, and prove shared resources and exact revert.
    Focused fixes are authorized. Request independent architect review only for
    a changed boundary or demonstrated correctness risk. Stop at the next city
    visual checkpoint. No canonical compilation, World regeneration, Matrix,
    cook, release, persistent reference migration or production backend activation.
  - Source: operator Q2a acceptance and T1g GO, 2026-10-11.
- **D30** Close D29's visual checkpoint for BrownEarth compacted dry terrain and
  freeze its accepted inputs. Prioritize isolated road, building and water samples
  rather than further soil refinement.
  - Effect: retain the D29 palette, source bundle and accepted composition. Residual
    distance repetition, varied land cover and city lighting are separate concerns.
    Continue the existing D26 owner-local prototype route. Q1/Q2 production
    archetypes, references and persistent adoption remain pending; exact cooked
    source/material and measured GPU cost remain unverified. No material-appearance
    change authorizes canonical compilation or full World regeneration.
  - Evidence limit: operator directly reviewed Kazan; Manhattan visual/restoration
    evidence is accepted on the agent's report, not independently inspected images.
  - Source: operator D29 PASS and continuation decision, 2026-10-12.
- **D31** Continue autonomous quality development for roads, water and buildings,
  critically comparing actual rendered pixels with the supplied asphalt references
  and the reviewed finite release target.
  - Effect: use native facilities and isolated parameter/source experiments;
    reject weak candidates internally instead of interrupting for each choice.
    Bounded transient city trials are in scope with exact restoration. Preserve
    BrownEarth, existing owners, staging and generated geography. Q1/Q2 persistent
    adoption, broad regeneration, canonical compilation, cook and release remain
    gated. Self-evaluation cannot manufacture the operator's final visual approval.
  - Source: operator autonomous quality request and attached D30 review, 2026-10-12.

- **D32** Close and verify the focused remaining-surface evidence corrections.
  - Effect: validate actual exterior wall/roof and water targets, native buffers,
    camera visibility and depth; preserve retained source detail while investigating
    asphalt repetition. Use owner-local experiments and exact restoration. D31's
    bounded engineering review remains closed; BrownEarth stays frozen.
    A confirmed generated-building geometry defect is recorded separately from
    material tuning; source correction scope is asked explicitly before expanding
    into the World building producer. No production migration, canonical or World
    regeneration, Matrix, cook or release is authorized.
  - Source: operator "so close and verify all fully", 2026-10-12, after review of
    the attached diagnostic-first D32 continuation.

- **D33** Correct the confirmed building-facing defect in its ProjectWorldBuilding
  owner and verify isolated RED/GREEN fixtures.
  - Effect: source emission and building-owned incremental identity may change;
    existing production city meshes, canonical inputs and other layer outputs
    remain untouched. This authorizes neither city regeneration nor production
    material adoption. Use one-sided compiled/rendered controls and preserve the
    existing query/physics collision contract.
  - Source: operator answer to the building-owner scope decision, 2026-10-12.

Release inclusion, final frozen R5/R6 gates, the historical baseline, and performance
policy are owned by release-plan D19-D22. This task does not authorize those operations.

### Operator gates

- **Q1 [PARTIALLY AUTHORIZED by D10]:** Bounded T0a development, local material
  prototypes and isolated persistence evaluation are authorized. Remaining
  production implementation/migration/adoption requires an operator decision.
- **Q2 [OPEN - NON-BLOCKING]:** Which matched visual candidate is accepted for
  persistent adoption? Present evidence after prototypes; do not infer approval
  from a successful shader compile.
- **Q2a [SCOPED PASS by D29]:** BrownEarth on the original cheaper composition is
  accepted as compacted dry soil for transient city evaluation. Preserve linear
  SoilDarkColor (0.085, 0.065, 0.046), SoilLightColor (0.24, 0.18, 0.12), and
  GrainColor (0.30, 0.26, 0.20). This accepts neither universal/loose soil nor
  production material/backend adoption. D27's extra distance sample remains a
  comparison only. D25/D27 engineering review is closed; rejected D26 loose-soil
  variants and earlier SoftPores/MacroPatch comparisons are historical evidence.
- **D29 city visual checkpoint [PASS by D30]:** BrownEarth is accepted for this
  compacted-terrain development stage. The operator directly inspected Kazan and
  accepted the reported Manhattan evidence with that limitation recorded. Do not
  reopen soil tuning without a demonstrated regression. Q2 persistent adoption
  and the remaining surfaces' visual decisions stay separate.

- **Q3 [CLOSED by D22]:** Authorize building,
  cooking and executing one disposable Win64 Development Texture Graph fixture
  under ignored `tmp/world/surface_presentation/texture_graph_runtime/`? Scope is
  one map, one preauthored 512-square native graph and its required shader/assets,
  with existing producer control; exclude ALIS cities and production recipes.
  Source/extension investigation continues independently. This is a backend
  acceptance exception, not permission to cook during ordinary visual iteration.

### Working assumptions

- **A1 [ACTIVE - PARAMETER-ONLY CANDIDATES INSUFFICIENT in T1b]:** Tune existing terrain instance parameters before changing its
  graph or the reusable pattern. Add another projection/sample only if matched
  views show the current graph cannot meet the goal.
- **A2 [ACTIVE]:** Start road/building defaults with a shared simple opaque lit
  parent and distinct instances; start water with the retained textureless candidate.
  Add detail only when visual evidence justifies its shader/resource cost.
- **A3 [ACTIVE - IMPLEMENTATION DIRECTION under D10]:** Prioritize JSON-driven, in-editor
  instance preview before additional material complexity. Reuse the Definition
  generator's orchestration pattern, while ProjectMaterial retains recipe parsing,
  compilation, identity and recovery ownership. D10 authorizes the bounded
  implementation but does not approve persistent production saving.
- **A4 [ACTIVE - RESOURCE COUNT UNDECIDED]:** Compare a compact structure/normal
  bundle with a bundle that adds independent packed surface masks. Two textures
  are not an accepted limit. Select from rendered quality, channel independence,
  generation/resident cost and consumer sampling evidence, not texture count alone.

## Non-goals

- Road/terrain and water/terrain alignment, intersections, elevation offsets,
  layering, topology, shore geometry, or z-fighting fixes. The separate current
  alignment concern owns their investigation.
  Apparent broken shorelines/road coverage must be classified against unchanged
  geometry/depth before shader tuning. Do not hide occlusion, missing geometry,
  or a sampling mismatch with masking, render priority, displacement, or offsets.
- Invented lane markings, windows, facades, roof attributes, land-cover truth,
  districts, or other unsourced semantic detail.
- WaterBody lifecycle, simulation, buoyancy, underwater gameplay, WPO/displacement,
  RVT, a new renderer/cache/registry, imported material packs, or Blueprint orchestration.
- Release regeneration, packaging, signing, tagging, publication, or performance
  certification during this task's development loop (release-plan D19-D22).

## Verified evidence

### Verified facts

1. [Terrain instance recipe](../../Plugins/Resources/ProjectMaterial/Data/SurfaceRecipes/Terrain/MI_ProjectTerrain_Default.surface.json)
   currently selects the metric terrain archetype. Its scale is 96 m and macro/detail
   strengths are both 0.12. The graph's detail signal affects roughness, not a normal map
   ([surface builder](../../Plugins/Resources/ProjectMaterial/Source/ProjectMaterialEditor/Private/ProjectMaterialSurfaceBuilder.cpp),
   `BuildParent`). These are baseline values, not approved target values.
2. [Pattern contract](../../Plugins/Resources/ProjectTexture/docs/pattern_generation.md)
   describes embedded signal frequencies. Reducing the tile scale shrinks them together.
   [Runtime subsystem](../../Plugins/Resources/ProjectTexture/Source/ProjectTexture/Private/ProjectTextureRuntimeSubsystem.cpp),
   `IsGenerationWorld`, admits Game, PIE, and GamePreview only. A plain Editor
   capture cannot establish runtime pattern quality.
3. [Surface schema](../../Plugins/Resources/ProjectMaterial/Data/Schemas/surface-recipe.schema.json)
   admits terrain/world, terrain-detail/world, object/local and isolated soil/UV archetypes. It does not admit a
   textureless lit or water archetype. The old backlog statement that only terrain
   exists is stale; use current schema/compiler evidence.
4. [Presentation profile](../../Plugins/World/ProjectWorldData/Data/Presentation/kazan_representative_v1.json)
   selects the engine debug vertex-color material for roads and buildings.
   [Profile loader](../../Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationProfile.cpp)
   restricts those paths to `/Engine/`; [binding resolver](../../Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationMaterialBinding.cpp)
   authenticates only `terrain.default` today.
5. [Road realization](../../Plugins/World/ProjectWorld/Source/ProjectWorldRoadEditor/Private/ProjectWorldRoadRealization.cpp)
   and [building realization](../../Plugins/World/ProjectWorld/Source/ProjectWorldBuildingEditor/Private/ProjectWorldBuildingRealization.cpp)
   detect a changed material path, then enter mesh-building paths. A separate
   material-only reassignment branch is needed; it is not an existing capability.
6. [Water realization](../../Plugins/World/ProjectWorld/Source/ProjectWorldWaterEditor/Private/ProjectWorldWaterRealization.cpp)
   builds a generator-local opaque blue material and requires `solid_opaque` settings.
   The [territory contract](../../Plugins/World/ProjectWorld/docs/territory_contract.md#layered-regeneration-contract-scale-out-precondition)
   retains that blockout restriction. Its replacement changes a material contract,
   not canonical water elevation.
7. [Terrain dependency regression](../../Plugins/World/ProjectWorldMeshTerrain/Source/ProjectWorldMeshTerrainEditor/Private/Tests/ProjectWorldMeshTerrainMaterialDependencyTests.cpp)
   distinguishes appearance changes from material-path changes using transient copies.
   [Material host](../../scripts/ue/material/README.md) already owns isolated test roots,
   locking, saved-asset authentication, failure recovery, and previous-state restoration.
   It refuses generation while a same-project Editor is running.
8. [Definition editor subsystem](../../Plugins/Editor/ProjectDefinitionGenerator/Source/ProjectDefinitionGenerator/Private/DefinitionGeneratorEditorSubsystem.cpp)
   watches source directories, debounces changes, validates and offers regeneration;
   its specific-file operation routes changed definitions to their existing owner.
   Detection alone is not automatic asset regeneration. Do not copy its broad
   `ValidateAll` scan as a requirement for one material recipe.
9. [Surface builder](../../Plugins/Resources/ProjectMaterial/Source/ProjectMaterialEditor/Private/ProjectMaterialSurfaceBuilder.cpp)
   can find an existing instance and update editor scalar/vector parameters in place.
   `BuildInstance` validates the complete parameter set before clearing overrides;
   T0a also uses that shared validation. Parameter application alone does not
   establish persistent loaded-reference or recovery safety.
   [Generation service](../../Plugins/Resources/ProjectMaterial/Source/ProjectMaterialEditor/Private/ProjectMaterialSurfaceGenerationService.cpp)
   independently refuses production mutation outside its dedicated commandlet.
   Its replacement path can delete old packages, and save verification unloads and
   reloads packages. A live-reference-safe route is not currently exposed.
10. A current native Editor capture renders Mesh Partition's transient preview MID,
    whose parent is the existing terrain instance. Rebinding the saved terrain-provider
    components did not affect these pixels. A temporary MID on the actual preview
    component produced a visible color change and restored the original appearance.
    Renderer selection belongs to the consuming preview surface; ProjectMaterial
    must not import private Mesh Partition implementation to reach it.
11. The accepted `terrain_metric` graph shares one `PatternScaleMeters` across its three triplanar
    samples and all packed channels. Shrinking that value changes cover and macro
    pattern size together with roughness detail; it exposes no independent fine-scale
    color or shading-normal control. T1b's ground-level captures confirm the remaining
    flat presentation. This is a material limitation, not evidence of incorrect geometry.
    The isolated `terrain_detail_metric` experiment extends only ProjectMaterial;
    its fine sample and gradient do not change accepted recipes or the ProjectTexture ABI.
12. [Native texture builder](../../Plugins/Resources/ProjectTexture/Source/ProjectTextureEditor/Private/ProjectTextureRuntimeAssetBuilder.cpp),
    `BuildBasisMaterial`/`BuildGroundMaterial`, generates smooth gradient, Voronoi
    and value signals for the accepted two-node terrain pattern. The isolated
    [soil builder](../../Plugins/Resources/ProjectTexture/Source/ProjectTextureEditor/Private/ProjectTextureSoilBuilder.cpp)
    now admits a separate closed three-node Structure/Normal/Masks bundle. Its
    authored fixtures remain outside the production recipe census. The accepted
    terrain B channel remains artistic detail, not a calibrated height map.
13. [Dry soil reference](https://polyhaven.com/a/brown_mud_dry) and
    [compacted mud reference](https://polyhaven.com/a/brown_mud) were inspected as
    actual images. Clumps, pores and granular variation distinguish them from the
    current uniform noise treatment. Their shaded preview images are references,
    not lighting-free albedo or height data to copy into generated channels.
14. Installed UE sources confirm native `UMaterialExpressionNoise` and
    `UMaterialExpressionVectorNoise` generation; `Engine/Shaders/Private/Random.ush`
    implements periodic Voronoi/gradient/value noise. The installed
    `Engine/Plugins/TextureGraph/Shaders/Expressions/Expression_NormalFromHeightMap.usf`
    samples wrapped height in four neighboring directions and encodes a normalized
    gradient as RGB. This is source evidence for a native height-to-normal approach,
    not permission to introduce TextureGraph's separate runtime.
15. [Epic texture-mask guidance](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-texture-masks-in-unreal-engine)
    supports packed AO, roughness and metallic masks and requires linear sampling.
    UE does not automatically interpret a project-defined "PBRA" layout; the
    material must wire its declared channels. The current native texture builder
    writes RGB through `EmissiveColor`, with constant opacity and AlphaComposite
    blending. Its contract deliberately excludes alpha. A meaningful fourth
    channel therefore needs a proved native write route, not a renamed channel.
16. The current surface builder connects Substrate `DiffuseAlbedo` and `Roughness`,
    but no material AO output or sampled F0. Installed UE's
    `UMaterialExpressionSubstrateMetalnessToDiffuseAlbedoF0` converts conventional
    base-color/specular/metallic inputs to the existing Slab parameterization.
    `Config/DefaultEngine.ini` enables Lumen/Substrate and disables static lighting;
    installed `LumenScreenProbeGather.cpp` enables material AO by default. These
    support testing native material AO, not claiming an untested AO texture already
    affects this material. Normal perturbation does not replace pore occlusion.

### Inferences and unverified areas

T0a establishes the explicitly triggered JSON -> ProjectMaterial -> transient live
instance loop and rejection/revert behavior. Its pattern Ready event preceded the
rendered proof. T1b adds matched views in both cities and higher-resolution ground-level
inspection, but none of the parameter-only candidates is a finished landscape.
Rendered comparison supports further local material experiments; it does not establish
Q2 visual adoption, photogrammetric detail, persistent saving, or GPU acceptance.
The operator reports the accepted technical baseline; this task does not rerun it.

## Current architecture and source of truth

| Black box | Contract and responsibility | Expected change |
|---|---|---|
| ProjectTexture | Accepted reusable structural outputs; pattern shape, readiness, and cache | The isolated three-output soil fixture uses its native builder/runtime/cache; accepted terrain resources remain unchanged |
| ProjectMaterial | Recipes, closed graphs, generated parents/instances, authenticated surface manifest | In-editor instance preview seam and its proof first; terrain appearance and minimal closed lit/water archetypes when needed |
| ProjectDefinitionGenerator / ProjectPlacementEditor | Definition detection/regeneration and definition actor synchronization | Untouched; reuse orchestration ideas without routing surfaces through DataAsset generation or replacing World actors |
| ProjectWorldEditor | Semantic resource selection and authentication | Extend existing default binding/profile admission, not graph compilation |
| Road/Building/Water editor leaves | Material assignment to owned mesh outputs | Material-only assignment, validation, inventory, and locality proofs |
| ProjectWorldMeshTerrain | Existing terrain channels, shared definition, geometry realization | Untouched; retain terrain material object path and channel layout |
| ProjectWorldData | Concrete profiles and serialized generated references | Only necessary presentation selections/material references and their owned inventory |
| SourceIngestion / CanonicalCompilation | Geographic authority | Untouched, including generated canonical bundles |

The [World structure](../../Plugins/World/ProjectWorld/docs/architecture/structure.md)
owns composition. No World module gains a ProjectMaterialEditor dependency.
An unexpected need to regenerate an untouched owner's output is ARCHITECTURAL RED;
stop and review the boundary rather than broadening the operation.

Retained consumer research is reused under D4:

- [Water presentation research](../02_backlog/world/20260825-1717_world_improve_water_presentation.md)
  owns the retained W1 candidate and excluded water features.
- [Road presentation backlog](../02_backlog/world/20260825-1717_world_improve_road_presentation.md)
  supplies its consumer goal and geometry exclusions.
- [Building presentation research](../02_backlog/world/20260825-1717_world_improve_building_presentation.md)
  owns the retained neutral massing candidate and semantic exclusions.

These are prior research inputs, not prerequisites or parallel implementation tasks.
This current todo coordinates their material portions for D1. It does not replace
stable contracts with backlog prose. On completion, reconcile those portions with
their existing task records rather than leaving duplicate unresolved material work.

## Problem and root cause

The capability gap is material presentation, not missing geography. Existing terrain
appearance controls can be tuned; simple reusable defaults for the other surfaces
are absent from the closed compiler and binding contract. Serialized reference changes
also currently reach geometry-building paths. Shader tuning cannot repair that locality
gap, and a new water shader cannot repair terrain/water vertical-order defects.

## Decision

### Premise / KISS gate

ProjectMaterial already owns appearance and its compiler lifecycle. Start with D1,
A1, and A2 through that owner. Add no services, ports, callbacks, caches, asset registries,
or generation pipelines. Replace the water leaf's local material graph only after the
reusable default authenticates. Reuse native material instances and transient component
overrides for visual trials; this gives up persistent preview edits and simulated water.

### Alternatives considered

- Tune terrain parameters only: first experiment, but cannot supply the three missing defaults.
- Import a material pack or add RVT/WaterBody: additional assets/lifecycles without a
  demonstrated need; outside this material-only goal.
- Force geometry dirty for changed references: violates D2; implement a bounded
  material-only branch under D3 instead.

### Soil/PBR sample before landscape (D12/D13)

Use the existing ProjectTexture native DAG/stock render-target route to generate
reusable soil structure and its separate normal texture from the same height field.
Evaluate all required PBR properties before choosing a resource count (A4):

| Property | Soil sample treatment | Owner boundary |
|---|---|---|
| Color | Lighting-free color variation; compare mask-derived color with a separate color output if necessary | ProjectMaterial owns tint/remapping; ProjectTexture owns reusable source structure |
| Normal | Separate map derived coherently from height, with declared space/encoding | ProjectTexture generates; ProjectMaterial decodes/projects |
| Roughness | Independent spatial structure where grains/clumps differ; keep adjustable base/range | ProjectTexture provides structural mask; ProjectMaterial defines final roughness |
| Metallic | Constant zero for the soil candidate | ProjectMaterial; no varying metallic texture needed for soil |
| Specular | Start at native dielectric F0; do not confuse roughness/wet appearance with arbitrary specular noise | ProjectMaterial; add a spatial mask only for a justified distinct surface |
| AO/cavity | Compare pore-scale occlusion with a neutral AO=1 control | ProjectTexture provides a height-based occlusion signal; ProjectMaterial wires native AO and controls influence |

Include an independent packed-mask candidate in the isolated comparison rather
than assuming height/noise can stand in for all these properties. The compact
candidate packs only the structural signals it actually needs alongside the normal
map. The independent candidate adds a third surface-mask output, allowing separately
controlled cavity, roughness variation and material/color separation. A separately
generated color output remains possible if masks cannot reproduce the reference.
Neither candidate nor its resource count is yet accepted. Shared native generation
is reused; this is not a generic optional-output framework.

Conventional ORM-style packing or a four-channel metallic/roughness/specular/AO
layout is valid when explicitly contracted. Soil does not require spatial metallic
or specular variation merely to fill channels. Do not call structural masks final
ORM values: ProjectMaterial's recipe still owns their remapping. Keep packed scalar
data non-sRGB. The fourth channel is unavailable on today's generation route; prove
native RGBA write/readback/filtering and identity before contracting on alpha.
RGB-only masks are a viable first route, not a prohibition on future RGBA packing.
Measure the actual formats: today's RGBA8 render target already allocates four
channels, so using alpha would not add residency bytes at the same dimensions.
Warnings about alpha changing compressed texture sizes do not establish that cost
for this uncompressed runtime target. They remain relevant if the output format changes.
Shared height may remain a generation intermediate; deriving normal/cavity does not
require exporting or sampling height in the final material. Cavity must represent
local occlusion, not simply relabel arbitrary height/noise as AO.

Do not overwrite `surface_structure_rgb_v1` channel semantics or enroll a soil
fixture as production authority. The new kernel/output contract needs focused
architect review under the existing owner policy before implementation; its exact
format, slot identity, normal convention and physical height scale remain to be specified.

Keep native Substrate shading: generated color -> `SubstrateSlabBSDF.DiffuseAlbedo`,
roughness -> `Roughness`, decoded normal -> `Normal`, with dielectric F0 (soil is
nonmetal). This is the existing ProjectMaterial graph family, not a custom shader
or a change of renderer. [Epic PBR guidance](https://dev.epicgames.com/documentation/en-us/unreal-engine/physically-based-materials-in-unreal-engine)
and [material input semantics](https://dev.epicgames.com/documentation/en-us/unreal-engine/material-inputs-in-unreal-engine)
separate these responsibilities. Normal maps change lighting response, not geometry
or silhouette, and do not supply geometric cast shadows. Pore-scale AO/cavity is a
separate concern to test explicitly; never bake directional lighting or scene shadows
into albedo. For conventional metallic/specular values use the native Substrate
conversion helper; do not connect those scalar masks directly to F0 as if equivalent.
Wire material AO at its native output and inspect actual lit Lumen pixels, including
neutral/known-occluded controls. [Epic's Lumen guidance](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-global-illumination-and-reflections-in-unreal-engine)
documents material AO support and its static-lighting restriction. A buffer view or
parameter readback alone does not prove the contribution. Keep renderer settings fixed.

Generated RT data stays linear; an eventual color texture must explicitly declare
its encoding. Normal maps need explicit signed-vector decode, DirectX orientation,
space and amplitude, with flat-normal and reversed-axis controls. On a UV test plane
use its tangent basis. Landscape triplanar application additionally needs correct
per-projection normal transforms/blending; do not feed tangent normals directly
into the existing world-space input. Verify filtering/mips and renormalization;
stock RTs do not gain normal-map compression/import semantics automatically.

No new service, cache, generic graph compiler, imported pack, or per-city texture
copy. D20/D21 permit an isolated Texture Graph backend/extension investigation;
they do not install it as production authority. Extend only the existing owner admission, bundle identity,
generation and authentication needed for the selected outputs. Readiness must cover
the whole selected bundle before use; failures must not expose partial/mixed identities.

Single Layer Water is a candidate, not a proven low-cost result on these meshes.
[Epic's water documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/single-layer-water-shading-model-in-unreal-engine)
describes its scattering/refraction pass; [Substrate documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-substrate-materials-in-unreal-engine)
and the installed `UMaterialExpressionSubstrateSingleLayerWaterBSDF` are native starting
points. Verify the chosen graph against the configured engine and existing Substrate
settings. Do not change global rendering settings to make the candidate pass.

## Required invariants

1. Canonical inputs, mesh topology/positions/normals, transforms, bounds, collision,
   navigation, cell ownership, actor identity, Nanite, and streaming policy remain fixed.
2. Same-path appearance edits rebuild no World geometry and rewrite zero World
   packages/manifests. Pattern-unit changes rebuild ProjectTexture's affected outputs;
   ProjectMaterial consumers rebuild only when their consumed path/contract changes.
   Parent graph changes rebuild ProjectMaterial outputs, not World outputs.
3. First-reference adoption changes only necessary material slots and their authenticated
   inventories, plus bounded retirement of the obsolete leaf-owned water material;
   no unchanged MeshDescription or compiled terrain section is rebuilt.
4. Missing/duplicate bindings, corrupt bytes, incompatible graphs, and migration
   failures refuse without leaving protected state changed. No debug-material fallback.
5. Appearance adds no fabricated canonical semantics or per-city/per-cell resource copies.
6. Visual acceptance uses matched lit views with ready patterns; editor/PIE GPU evidence
   remains bounded development evidence, not packaged performance certification.
7. Soil samples use coherent structure/normal identities and native PBR shading.
   Landscape application waits for Q2a; no directional shadows are baked into maps.

## Implementation tasks

T0a and local prototypes are authorized by D10. Persistent production adoption and
the production binding/migration tasks remain gated by Q1/Q2:

- [X] **T0a - One-recipe in-editor loop, before material complexity:** expose a
  narrow ProjectMaterial-owned preview operation for an existing instance recipe.
  Reuse its parser, schema, dependency authentication and parameter application;
  do not implement a second JSON-to-material compiler in MCP Python. Begin with
  one explicitly selected recipe and a transient instance/copy applied to loaded
  components. Prevalidate the complete candidate before mutation, retain the prior
  bindings/overrides/dirty state, and restore them on refusal, interruption or revert.
  Do not delete, recreate or unload production packages during this preview.
  Prove a JSON GroundColor edit at one Kazan vantage reaches actual rendered pixels,
  then revert without saving. Hold the Editor process, camera, lighting and RHI fixed;
  authenticate before/after World and canonical path/hash inventories.
  Identify the rendered component's material and parent chain first. A changed
  parameter readback or provider material slot does not prove changed viewport pixels.
  Once the single-recipe operation works, use the existing DirectoryWatcher and
  editor-timer debounce pattern if automatic detection is needed; dirty paths must
  select only their material recipe closure. Keep all orchestration in the existing
  material owner; no Definition compiler dependency, service, watcher framework,
  actor-replacement event or full World request.
- [ ] **T0b - Isolated persistence evaluation (D10); production adoption gated:**
  Persistent in-editor adoption is a separate proof: pre-existing unsaved state must
  be preserved or explicitly refused, the sole writer must coordinate the generated
  content lock, saved assets and manifest identity must agree, loaded references must
  retain valid identity, and injected failure/recovery plus save/reload must pass in
  an isolated mount before production saving is proposed. The existing commandlet
  remains the supported fallback for structural changes until equivalent live-editor
  proof exists. Do not remove its production guard to make preview work.
- [ ] **T0 - Baseline:** inventory current materials/references and record dirty state.
  Capture fixed player-height, close-ground, oblique, aerial, slope, neighboring-cell,
  shore, road, and building views on existing maps. Hold lighting, exposure, camera,
  resolution, and RHI fixed. Store run-specific receipts/captures separately under
  `tmp/world/surface_presentation/<run>/`. Establish exact before/after path/hash sets.
  At suspect road/shore views distinguish geometry/depth from appearance; hand
  geometric findings to [Road and Water Alignment](20261008-1543_world_investigate_road_water_alignment.md).
  A geometrically correct boundary remains a presentation candidate here. Do not
  require a geometry correction to prototype materials on otherwise valid surfaces.
- [ ] **T1 - Soil sample, then approved terrain prototype (D12/D13):** T1b rejected
  parameter-only appearance; T1c's implementation passed but its derivative-noise
  appearance is insufficient. Preserve that evidence, not its landscape-first order.
  - **T1d - Source units [PARTIAL - D25 grain semantics corrected in private 512/1024 candidates; visual acceptance and production promotion open]:** specify and review the smallest closed soil kernel and
    compact/independent-mask candidate contracts at ProjectTexture's boundary.
    Before further custom shape work, inspect installed Vector Noise Voronoi seed
    position/distance with Cellnoise, native remaps, height-to-normal functions,
    and Texture Graph operators. Give an isolated Texture Graph authoring comparison
    priority; establish whether it can run without changing production configuration
    or disrupting the working Editor. If unavailable or unsuitable, record the
    concrete limitation and use the closest viable native option. An Experimental
    label is not a reason to skip the comparison. The ban on a separate TextureGraph
    runtime in this plan does not exclude a disposable authoring experiment.
    Prefer a small set of meaningful transient generator parameters and MID controls
    to repeated hard-coded C++ weight changes. These experiment controls are not
    new production schema fields. Preserve one coherent height -> normal -> masks
    bundle and its existing identity/cache authority; do not silently retarget a
    ready structural identity after changing generator parameters.
    The initial native comparison is complete within its diagnostic scope; do not
    repeat the broad native survey. D17 narrowed the completed source comparison to
    LooseAggregates-SoftPores, LooseAggregates-MacroPatch and
    LooseAggregates-HybridCompact. D25's native composition and grain candidates now
    provide the retained source comparison. D27 freezes GranularRaised's structural
    source and derived maps for its preservation pass; improve distance composition
    in ProjectMaterial rather than restart or soften that source. The old pore,
    distribution and sparse-grain controls are historical comparisons.
    Preserve their controls without treating them as the next required experiment.
    Broad metric pigment/roughness composition belongs
    to ProjectMaterial, not a new source-generation or cache framework.
    Retain completed three-family evidence; present the frozen current source and
    selected distance candidate with matched lighting, maps, metric tiling,
    near/mid/far and motion evidence. Compare reproducibility, generation,
    warm reuse, actual residency and GPU cost; unknown measurements stay unverified.
    Investigate native cache/streaming options before any custom lifetime extension;
    reuse of one pinned bundle does not prove bounded residency for new identities.
    Test simple ProjectMaterial macro composition in isolation against the visible
    repetition before more source complexity. A 1.5 m 3x3 plane does not judge
    multi-metre patch distribution: also use a larger, metrically correct surface.
    Compare existing mip filtering first; add smooth normal/cavity/roughness
    distance treatment only where rendered evidence justifies it. Keep one opaque
    Substrate Slab and coherent normal space, with constant dielectric F0.
    Native anti-tiling is conditional on simpler composition remaining inadequate;
    measure extra sampling and normal orientation, not only smooth screenshots.
    Terrain slope/projection application still waits for Q2a.
    Public contract or plugin-dependency changes
    require one independent architect review at that boundary, closed at PASS.
    Implement in isolated roots
    through its native builder/runtime, with focused RED/GREEN fixtures. Shape
    irregular clumps, pores and grains from native periodic noise/remaps before
    further palette tuning. Reuse existing generation/cache ownership. Add the
    minimum required admission only if the selected native candidate changes the
    proven three-output contract. Preserve packed roughness/cavity masks, their
    independent controls and the channel/encoding checks above. Contract on RGBA
    only after proving a varying alpha field survives generation and filtering.
    Keep the accepted terrain pattern unchanged.
  - **T1e - Separate normal [PARTIAL - preserved maps/mips and D29 moderate-slope reorientation verified; exact packaged adoption open]:** generate it from that same bounded height field, using
    native height-neighbor operations; compare flat-height/flat-normal controls and
    known axis ramps. Test tiling, finite/unit-length vectors, decode, mips, generation
    identity, cold/warm reuse and refusal without partial readiness. The LQ screen-
    derivative prototype remains historical, not the final separate-map solution.
  - **T1f - Isolated PBR sample [SCOPED Q2a PASS under D29 - calibrated BrownEarth compacted dry soil; universal/loose soil and production adoption unaccepted]:** render a metrically sized UV plane and curved
    sample in disposable PIE/game-preview, through ProjectMaterial's authenticated
    fixture compiler. Show individual maps and lit surface together under fixed
    neutral and grazing light, then move the light to expose baked-shadow mistakes.
    Check 3x3 tiling, near/far mip behavior and motion shimmer. Capture actual pixels
    through MCP; it controls/observes, not compiles. Record shader samples/instructions,
    generated memory and available GPU measurements; unknown cost stays unknown.
    Compare compact and independent packed-mask candidates with matched lighting;
    isolate roughness, AO and normal effects with neutral and known-bad controls.
    Do not declare two textures optimal, or a four-channel map required, without
    quality and cost evidence on this sample.
    D18's SoftPores combination is completed historical evidence. D27 retains
    GranularRaised's close normal/grain/roughness response and its mip filtering.
    D28 additionally permits the requested palette correction and controlled native
    lighting/capture diagnosis, with the original source and response unchanged.
    Compare references and combination on one unchanged physical plane at 1.7 m
    eye height, looking toward 2/5/12/30 m, under matched neutral/grazing light.
    Deliver distinct near/distance boards and playable motion plus a frame sheet;
    a GIF flattened by delivery does not prove operator shimmer inspection.
    D29 accepts this sample for transient city evaluation only. Other candidates
    require their own acceptance; persistent adoption remains gated.
    Retain completed investigations. D19's bounded-cycle authorization is not a
    requirement to spend more cycles after a reviewable candidate is prepared.
  - **T1h - Native runtime and extension feasibility [bounded cache/PBR PASS; adoption and comparative GPU cost OPEN]:**
    1. Finish installed-source tracing before code integration. Distinguish Runtime
       module declarations from initialization/evaluation compiled into Game.
       Inspect async completion, graph serialization/shader cooking, allocation,
       native pools and cleanup. C++ graph construction is available; lack of a
       Python/MCP authoring API is not a native runtime limitation.
    2. Apply D21's order to the observed gaps. A wrapper/subclass is appropriate
       only if it can use supported native hooks; private singleton initialization
       and compiled-out internals require a scoped native patch, not access hacks.
       Start a native-node-only fixture; separately assess material/function nodes
       whose MIC mutation is Editor-only. Keep a patch manifest with exact engine
       identity and per-file reason; do not modify the launcher installation.
    3. Under D22, prove one authored 512-square graph in a cooked Win64 Development
       executable with WITH_EDITOR=0. Bound timeout/failure, validate actual output
       channels/pixels and resource retention/release. Editor/PIE/-game is only a
       diagnostic control. No ALIS map or production manifest is an input.
       D23 now demonstrates the closed three-output ALIS fixture ABI, shared cache
       consumption and tracked native RT release. This is not general graph or
       concurrent native-user support; production enrollment remains unapproved.
    4. Only after native execution works, compare returned-target retention against
       one-time native copies into stable ProjectTexture slots. Preserve public
       consumer paths/contracts; do not change ProjectMaterial/World to hide native
       allocation differences. Authenticate topology, structural inputs/seed,
       resolution and output ABI through ProjectTexture's existing identity owner.
    5. Reuse existing preflight/cache/catalog admission. One in-flight request per
       immutable identity; Ready covers the complete validated bundle with valid
       object/RHI identity. Warm requests and material-only changes generate zero
       pixels. Partial failure, invalid resources and eviction reject stale Ready.
       Verify GPU release separately from metadata eviction and quantify native
       pooled targets/intermediates; job no-cache mode alone proves neither.
    6. Compare equal-output cold CPU/GPU, passes, peak/steady/released memory,
       warm reuse, multiple identities/budget refusal and repeat lifecycles with
       the current producer. Start at 512; 1024 only if evidence warrants it.
       Keep CPU enqueue/render-thread flush separate from GPU completion. Native
       RDG usage is not proof of whole-graph aliasing or optimal residency.
    7. Review the completed changed runtime boundary once with an independent
       architect; fix concrete PATCH findings and stop at PASS for that scope.
       Recommend adoption only for demonstrated reliability/maintenance and
       measured benefit. Otherwise preserve the current producer and record the
       specific limitation. Unsupported stock behavior is not a global BLOCKER
       while D21 extension feasibility remains open. Q2a stays separate.
  - **T1g - Landscape after Q2a [BOUNDED ENGINEERING AND VISUAL PASS by D29/D30]:** reuse the approved sample on transient components
    in both cities; compare projection seams, slopes, near/far repetition and motion.
    Inspect installed native `WorldAlignedNormal`/`NormalFromHeightmap` functions
    before assembling custom projection math; verify their spaces, inputs and cost
    against the selected contract rather than assuming that native means compatible.
    Preserve geometry normals for slope/projection semantics, perturb shading only,
    and prove exact revert plus unchanged World/canonical output inventory.
    Persistent adoption still requires Q2. T0b is not a prerequisite.
- [ ] **T2 - Default prototypes [PARTIAL - D31 native aggregate/concrete bundles, water wave utility and bounded city trials; final visual acceptance and production archetypes open]:** reuse the retained consumer candidates. Add only
  necessary closed archetypes to ProjectMaterial's schema/parser/builder/reload checks,
  identity, manifest, and direct payload consumers. Textureless archetypes must model
  an absent pattern dependency truthfully; do not fabricate a pattern ID, runtime
  output, or terrain layout dependency just to satisfy the existing terrain shape.
  D26 permits independent isolated samples while soil Q2a is open; implementation
  and production adoption remain gated. Roads and buildings can share a simple opaque parent with distinct instances.
  Water gets one authenticated default; D31 tests the existing native wave texture
  and its three connected wave samples without production activation. If detail becomes
  necessary, static cell meshes need continuous world-metric coordinates; do not
  blindly reuse object-local projection or terrain-only semantic channels.
- [ ] **T3 - Visual approval:** apply candidates transiently to PIE components, not
  shared generated meshes/packages. Prove discard/reload restores original appearance.
  For soil, Q2a precedes these landscape trials. Show matched A/B views for both
  cities and get Q2 answered before persistent cutover.
- [ ] **T4 - Binding integration:** extend the existing manifest resolution for closed
  default bindings and replace obsolete road/building path restrictions and water
  placeholder validators/settings at their owners. Update direct schema, profiles,
  inspectors, verify inventories, and tests; remove obsolete local water graph ownership
  after successful authentication. Keep surface offsets/geometric settings fixed.
  Census each generated `M_ProjectWorldWater` package and its referencers. Retire
  it through the same owning transaction only after accepted consumers no longer
  reference it; include deletion and restoration in the exact allowed write set.
- [ ] **T5 - Material-only migration:** separate reference changes from geometry dirty
  decisions in the three leaf owners. First prove already-current and newly-created
  mesh cases in small isolated fixtures. Preserve geometry build data; save only changed
  material-bearing packages/inventories through the existing World transaction.
  There is no existing material-only host operation: extend only the necessary
  selection/assignment seam in `scripts/ue/world/realize_canonical_world.ps1`
  and its layer dispatch, rather than creating an independent mesh-resave writer.
  Test injected failure and exact restoration before any production cutover.
- [ ] **T6 - Adoption and close:** census the actual affected references, adopt only
  necessary changes under D3 after visual acceptance, authenticate saved/reloaded defaults,
  and prove a subsequent same-path tune changes zero World output bytes. Run focused
  owner tests, matched PIE checks, and independent R2. Update durable owners and reconcile
  material portions of retained backlog records. Report any unproved promotion boundary.

## Test-first and verification plan

### Red evidence

| Case | Kind | Baseline failure or missing capability |
|---|---|---|
| Terrain near/far/slope/hydro views | Visual acceptance | Reported oversized/flat treatment; establish fresh lit PIE control before tuning |
| Isolated soil surface and separate normal | Missing capability / visual acceptance | Current structural RGB is not a coherent soil height/normal bundle; prove flat and axis-ramp controls, then judge actual reference-matched pixels |
| Road/building/water matched views | Visual acceptance | Debug/default placeholder treatment; fresh controls use existing generated meshes |
| New default compilation and authentication | Regression guard | Current closed schema/compiler/resolver cannot admit the required defaults |
| Geometry-current mesh changes material path | Regression guard | Current leaves enter mesh-building paths; assert zero geometry rebuilds and preserved build data |
| Same-path tune after adoption | Regression guard | Must leave geometry identity and every World path/hash unchanged |
| JSON-driven live instance preview | Missing integration | MCP parameter setters exist; no ProjectMaterial-owned JSON preview operation or change-detection integration is exposed |
| Invalid live candidate or preview interruption | Regression guard | Candidate must refuse before mutation or restore exact prior overrides, bindings and dirty state |
| Persistent live saving and recovery | Missing integration | Existing production service admits commandlets only; equivalent manifest, reference, unsaved-state and recovery proof is required before a new route |
| Corrupt binding or injected migration failure | Regression guard | Invalid authority must refuse; failed mutation must restore exact pre-run path/hash set |
| Scale/coherence, cost, semantic restraint | Reviewer judgment | Judge fixed-view output and real shader stats, not an allowlist of preferred numbers |

Observe new machine-contract regressions failing before changing production behavior.
Reuse current fixtures; do not invent an automated test for aesthetic preference.

### Green evidence and traceability

| Invariant | Acceptance surface | Execution envelope | Cheapest proof | Final development proof | Stop condition |
|---|---|---|---|---|---|
| 1 | Actual geometry/build data and World identities | Isolated small fixture plus read-only existing maps | Before/after projections, hashes, and build-call assertions | Migration geometry projections equal; protected files unchanged | Any geometry/canonical/streaming change |
| 2 | Consumer dependencies and saved World bytes | Exact owner tests; same-path candidate | Existing identity/dependency tests | Zero changed World paths after tune | Appearance invalidates World |
| 3 | Serialized slots, leaf inventory, retired water material | Small fresh-process fixture transaction | New material-only branch and reference-retirement tests | Only censused references/inventory/obsolete material change; reload resolves | Mesh build, retained referencer, or unlisted mutation |
| 4 | Saved assets and restored path/hash set | Normal compiler/World wrappers, scratch mounts | Known-bad binding and post-save failure | Rejection restores exact fixture state; successful no-op rerun | Recovery or authentication gap |
| 5 | Graph/resource count and semantic inputs | Material compiler fixtures | Schema/graph tests | Shared defaults; resource count independent of city/cell count | Extra owner or fabricated semantics |
| 6 | Lit pixels and shader/GPU cost | Normal RHI PIE/game-preview; fixed settings | Paired near/far/cross-cell views | Operator accepts both cities; record shader stats and comparable GPU deltas | Clear-color pattern, seams, compile error, unjustified cost |
| 7 | Selected generated bundle, native PBR sample and operator decision | Disposable native runtime/PIE sample | Known-bad partial bundle, flat-normal/axis-ramp and independent packed-channel controls | Map board, AO/roughness controls and moving-light sample accepted at Q2a before landscape | Partial readiness, lost channels, incoherent normals, baked shadows or missing sample approval |

Use exact tests individually, selected by the actual changes, through
`scripts/ue/test/unit/iterate.ps1 -TestFilter <exact-full-test-name>`:

- `Project.Material.Generation.SurfaceRecipeContract` and `SurfaceRecipeKnownBad`
  (each with the complete `Project.Material.Generation.` prefix).
- `Project.Material.Generation.SurfaceAssetCompilerContract` and
  `Project.Material.Generation.SurfaceIdentityFirewall`.
- `Project.World.MeshTerrain.MaterialDependencyIsPathOnly`.
- Existing `Project.World.Realization.Identity.RoadMaterialChangeDirtiesRoads`
  and `Project.World.Realization.Identity.BuildingMaterialChangeDirtiesBuildings`,
  extended to distinguish reference assignment from geometry rebuilding.
- New narrowly named binding/material-only/rollback tests in the owning test files;
  record exact IDs when authored, not broad filters.

The [material host](../../scripts/ue/material/README.md) supports `-TestRoot
tmp/material/generation/<run>` and an authenticated `-LayoutReceipt`. Its standalone
transaction still requires the same-project Editor to be closed. That is a host/service
restriction, not proof that instance preview requires restarting the Editor. T0a owns
the proposed live instance path and must prove it before declaring the automatic loop
supported. Use test-root recipes/mounts for structural candidates; live trials use
unsaved objects. Direct MCP parameter changes are diagnostic, not recipe compilation
or accepted generated authority.

No Common Check, territory Matrix, canonical compile, full World Apply, cook, package,
or R5 is an ordinary visual discovery/debugging step. T1h's narrowly scoped cooked
backend proof is authorized by D22 (Q3 closed); it does not waive any production gate.
A persistent World-reference cutover must satisfy
the existing [promotion contract](../../docs/testing/world_pipeline_layers.md) and
[World transaction](../../scripts/ue/world/README.md); focused proofs do not silently
replace a mandated authority gate. If that route cannot isolate material changes or
requires broader work, stop and resolve its scope before proceeding. Final packaged
checks stay with release-plan D19-D22 after both required concerns are accepted.

## Documentation plan

- ProjectMaterial's existing material-generation doc owns supported closed archetypes,
  appearance/projection policy, saved output, and consumed identities.
- ProjectTexture's pattern doc changes only if its own structural contract changes.
- World territory contract owns default-binding/first-reference migration semantics;
  leaf/settings docs and schemas own their changed admission and inventory rules.
- Keep the repository's current documentation layout. No new architecture forest,
  duplicated backlog candidate spec, or standing link back to this todo.
- Add a plugin pitfall only for a reproduced non-trivial defect with a regression.

## Rollout and rollback

Preview state is transient. Persistent material outputs use the existing locked host
transaction and authenticated restoration until T0a establishes an equivalent supported
in-editor route. Do not default routine instance preview to an Editor restart or bypass
the current host/service exclusions. A necessary first-reference migration uses the existing
World transaction with an exact before/after census, bounded output roots, and proven
failure restoration. Restoration includes profiles/recipes and outputs that belong to
the operation, including retired water placeholder packages; preserve pre-existing user
work. Do not invent a second migration runner,
promise filesystem atomicity, or persist a shader fallback for recovery.

## Completion criteria

- Isolated soil maps and native PBR sample are accepted at Q2a before landscape trials.
- All four surface classes have operator-accepted matched PIE evidence on both cities.
- Adopted default materials are recipe-owned, compile, save/reload, and authenticate.
- Invariants 1-7 have the bounded evidence above; unexpected propagation is resolved.
- Necessary one-time reference changes are inventoried and recoverable; subsequent
  same-path tuning changes zero World packages/manifests.
- Stable docs and retained backlog records reflect their actual owners/remaining scope.
- Independent R2 accepts the implementation. This is development closure only; release
  readiness remains with the release router and its final frozen candidate.

## Review record

### 2026-10-12 - D32 diagnosis and D33 building-owner correction

- Confirmed the suspicious city building views are a geometry defect. Native
  raw SourceModel copies retain conventional-outward overlays; installed native
  build/RenderData normals and one-sided rendering follow the opposite emitted
  winding. Native Cube controls have the correct convention. Selected city
  transforms have positive unit scale. Double-sided collision and the debug
  material masked the defect; changing ProjectMaterial normal signs or enabling
  Two Sided would hide its cause. The earlier D31 concrete city interpretation
  is superseded by this diagnosis, without reopening its material architecture.
- At an authenticated 350 cm exterior wall/roof camera, Kazan's one-sided
  neutral/candidate captures see geometry about 927/933 cm away; Manhattan sees
  geometry about 3343/7000 cm away. These are rejected target captures, not proof
  of a successfully shaded exterior. Existing production building meshes remain
  untouched and wrong-facing; final city concrete acceptance is still pending.
- D33's owner-local fix emits UE-clockwise order before vertex creation, derives
  consistent outward normals and changes emitted output identity. Building-owned
  input output revision 2 also invalidates the early clean-unit skip. Canonical
  geometry, tag domains, positions/bounds and unrelated layers keep their contracts.
  Stable semantics and the incident are recorded in the World territory contract
  and pitfall 47, rather than hidden in material code.
- Exact RED `Project.World.Realization.Buildings.ExteriorFacing` reports 12 inward
  triangles and 36 overlay mismatches. GREEN requires all 12 triangles, one
  admitted building, zero facing/normal mismatches and a changed semantic digest.
  `Project.World.Realization.Buildings.ExteriorRender` passes native one-sided
  Nanite/default compilation, roof/wall depth error 0 cm, outward shader normals,
  4096-pixel readback and a rejected away-facing camera. Fresh exact InputLocality
  and TopologyAdmission tests pass. The standard Editor build passes. Independent
  architect verdict: PASS for this bounded source/fixture boundary; review closed.
- Water diagnosis authenticates top-surface rays and native depth/normal buffers
  before artistic adjustment. In the close lab, water view depth is 69.738 cm;
  the bright shallow bed appears at 122.046 cm, versus 749.753 cm for the deep
  control. Bed top heights are -30 and -390 cm. Identical water normals and optics
  produce pale shallow transmission and dark deep water. This matches the native
  Single Layer Water depth/color path; no terrain shaping is justified by the
  pale image. Both city bed/depth controls keep actual existing geometry.
- Retain the river baseline and compare RiverCalm with only RippleStrength 0.22
  versus 0.65. The calmer arm reduces harsh nearby reflection breakup; the native
  texture, three-sample averaging, world phase and optical coefficients stay the
  same. No WaterBody, foam, WPO, flow simulation, new texture set or renderer.
- Retain all four D31 asphalt/concrete source maps byte-identically. The first
  native mip-bias arm loses fine detail and is not the selected default. A second
  world-metric sample of existing Signals at 32 m introduces restrained pigment
  and roughness variation, without another normal sample. AsphaltMacro costs
  249/6/3 versus the original 238/5/3 (pixel instructions/lookups/samplers).
  Concrete and river remain 273/9/3 and 986/6/3. These are shader statistics,
  not comparative GPU time or acceptance of the 16.67 ms city target.
- Rendered lab comparisons cover 0.5/2/5/12/30 m, grazing light and correctly
  oriented native cube walls/roofs. Four actual 64-frame clips retain distinct
  frames and decode at 1280x720. Sampled motion retains coherent detail, but does
  not prove gameplay frame pacing or eliminate every grazing-angle alias.
- Each city has 12 final original/baseline/refined road/water captures, with
  authenticated native source targets and rendered-depth checks. Kazan's maximum
  central-ray depth discrepancy is 1.096 cm (3 cm diagnostic tolerance).
  Manhattan's refined per-pixel facet/projection prediction has maximum error
  0.040 cm (1 cm tolerance). The old constant-depth check rejected oblique
  context pixels; exact pixel-ray prediction replaces that misleading probe.
  Smoothed road overlays need not equal every facet; strict flat-overlay agreement
  applies to building controls, not arbitrary road triangles. No non-visible
  fallback may label a capture successful.
- Self-check keeps AsphaltMacro and RiverCalm as candidates, not visual approvals.
  Broad asphalt composition improves while granular regularity remains at grazing
  angles. RiverCalm is less harsh nearby, but native reflection patterns and city
  lighting remain visible limits. Concrete is restrained massing on a correct
  native exterior control; existing wrong-facing city meshes cannot accept it.
  Stop at a consolidated visual/production checkpoint rather than inventing more
  families, repeating accepted reviews or declaring photorealistic completion.
- Exact evidence, diagnostic controls, maps, clips and tests are under ignored
  `tmp/world/surface_presentation/texture_graph_runtime/quality_d32/`. No canonical
  compilation, city/full World regeneration, Matrix, cook or release is run.
  The eventual corrected-building update is separately authorized affected-owner
  work; it is not a dependency on terrain, roads, water or canonical regeneration.
  Material adoption, exact cooked sources and measured runtime costs remain gated.
- Final restoration compares all 477 Kazan and 388 Manhattan generated surface
  components against their pre-mutation meshes, transforms, bounds, material lists
  and exact override arrays. Both original maps reload clean with 871/750 actors
  and no dirty packages. Original Kazan is left in the background Editor. The
  temporary native autosave preference is restored to its original enabled value;
  its native setter saves settings, so no process-only/no-write claim is made.
- Final SHA-256 census preserves 10,787 protected resource/World Data and Content
  files plus CanonicalCompilation inputs/code, with zero added/removed/changed
  files. All 44 frozen BrownEarth inputs remain unchanged. An unrelated README-only
  commit advanced HEAD during verification; it is preserved without altering or
  reviewing its content. The final index is empty. This agent did not stage,
  commit, push or discard operator work. Documentation ASCII, local links and
  diff hygiene pass; transient evidence records the exact final Git snapshot.
- Independent final D32/D33 closure review: PASS, with no required correction.
  The reviewer inspected the final docs, maps/depth controls, city captures,
  restoration and preservation receipts. This closes bounded engineering only;
  it does not turn the retained visual candidates into production approvals.

### 2026-10-12 - D31 autonomous remaining-surface quality cycle

- Executed bounded native-first asphalt, concrete-massing and water experiments,
  including rejected round-grain, angular/camouflage and harsh-reflection arms.
  Frozen BrownEarth was not tuned. Public recipes, schemas, compiler, cache,
  production resources, canonical outputs and generated geometry were unchanged.
  No World regeneration, canonical compilation, Matrix, cook, release or commit.
- Native Voronoi seed positions, seeded Cellnoise and Perlin functions bake two
  512-square linear targets per opaque family: packed height/pigment/roughness
  and a separate normal. The final asphalt uses smoother variable-size grains,
  a 0.32 m tile and reduced pigment contrast; concrete uses restrained pores and
  pigment variation with signed triplanar projection. These are private source
  experiments, not a replacement bundle ABI or production cache activation.
  Dry/wet asphalt share the same targets; concrete faces share their own pair.
- Native NormalFromHeightmap consumes the selected quantized height. Independent
  wrapped differences over 262,144 texels per family report maximum normal error
  0.559 byte for asphalt and 0.548 for concrete. Flat-normal controls fail at
  44.368 and 7.232 bytes respectively. Four outputs redraw byte-identically;
  auto-mips, wrap and trilinear sampling are enabled, but every mip's contents
  and packaged material consumption are not certified by this Editor experiment.
- Water reuses Epic's existing packed wave texture and native WaterTextureWaves,
  DeriveNormalZ and Single Layer Water outputs. This is native-input reuse, not
  evidence of procedurally generated cached water or flow/shoreline geometry.
  An independent architect caught the installed function's misleading Divisor:
  it multiplies three connected samples. Literal 3 amplifies packed normals;
  weight 1/3 correctly averages them. An authenticated flat-input native
  RED/GREEN fixture measured mean RG 4.48047 versus 0.497803. Corrected rendered
  ripples are visibly coherent. Stable lessons are in ProjectMaterial pitfalls.
- Final isolated evidence includes 68 matched stills, neutral/grazing lighting,
  road views at 2/5/12/30 m and four actual 48-frame camera clips. The controlled
  two-light rig uses a fixed 25,000-lux fill; it is not city lighting. SceneCapture
  explicitly opts into Lumen GI/reflections and display-encoded output. Early
  captures without those overrides and city shots missing the selected surface
  are rejected as quality evidence. Motion is offline sampling, not frame pacing.
- Final graph statistics are asphalt 238/5/3, concrete 273/9/3 and water 986/6/3
  (pixel instructions / lookups / samplers). Native combined viewport/capture
  GPU snapshots are 6.27/6.14/6.11 ms; they do not isolate per-material cost,
  establish a speedup or accept the 16.67 ms city target. Performance remains
  INCONCLUSIVE beyond this fixture scope.
- City scouting reads native mesh triangle centroids through GeometryScript and
  terrain collision instead of assuming bounds centroids lie on visible geometry.
  One whole component per surface receives a transient override; no lifting,
  splitting or alignment correction. Opaque material graphs request Nanite usage before
  compilation and retain native targets through owned MIDs. Geometry acceptance
  and the separate alignment todo's water policy remain open.
- A diagnostic cleanup crash identified a Python native package-array reference
  retaining the old scratch world. Preflight now releases wrappers before map
  destruction. Background Editor recovery and successful subsequent map switches
  verify the fix; this was not an ordinary parameter-iteration restart. Native
  scratch autosave is not a production write or a claimed write-free experiment.
- Visual self-check: asphalt is improved but still regular at close range;
  concrete is credible restrained massing, not a detailed facade; water is a
  calm rippled prototype whose reflection and bank/context quality still need
  operator judgment. Keep the best candidates and record this bounded plateau.
  Self-check does not close Q2 or authorize persistent production adoption.
- Evidence and exact candidate controls live in ignored
  `tmp/world/surface_presentation/texture_graph_runtime/quality_d31/`.
  Exact mesh/transform/bounds/material/override restoration covers 477 Kazan and
  388 Manhattan generated surface components. Both original maps reload clean;
  asynchronous preview readiness is checked before accepting actor counts.
  Final SHA-256 census preserves 10,787 protected files and all 44 frozen soil
  files; session HEAD and operator index remain unchanged. Original Kazan is left
  open in the background. Accepted D23-D30 reviews remain closed.
- Independent architect: PASS for bounded D31 engineering after the native wave
  averaging correction and factual capture/Nanite wording fixes. Source, final
  pixels, recovery logs and receipts were independently inspected; selected-height
  normal calculations were rerun, and frozen soil plus HEAD/index were checked.
  Regular grain, concrete massing and imperfect close-water reflections remain
  visual limits. This review is closed; Q2, persistent adoption, exact cooked
  consumption and release acceptance remain open.

### 2026-10-12 - D30 approved soil freeze and remaining-surface prototypes

- Closed the D29 city visual checkpoint under the operator's scoped PASS. Kazan
  was directly reviewed; Manhattan was accepted with the report-level evidence
  limitation. BrownEarth remains compacted dry soil, not universal land cover.
  Frozen evidence covers its palette, sixteen scalar inputs, source identities
  and 44 original material/texture/source-readback files. All remain unchanged.
  Production adoption, exact cooked source/material and measured GPU cost remain
  pending. No further soil tuning is planned without a demonstrated regression.
- Reused the D26 opaque/Single Layer Water sample route in a disposable unsaved
  native map. Original cities were not bound to these new defaults. Production
  recipes, schemas, compiler, cache, engine plugins and generated geometry were
  unchanged; no World/canonical regeneration, Matrix, cook or release.
- A bounded native asphalt experiment bakes Perlin height and separate smooth
  vector-noise pigment/roughness signals into one 512-square linear target, plus
  a separate normal target. Literal Cellnoise coloring was visually rejected for
  square artifacts. No procedural noise runs in the material's per-pixel path.
  Fresh, weathered and wet MIDs share those two diagnostic targets. Two outputs
  are this experiment's choice, not a general texture-count or production ABI.
  Metallic/specular/AO remain explicit dielectric/constant modeling choices.
- The analytic-gradient normal prototype failed the selected-height comparison.
  Native `NormalFromHeightmap` now derives normals from the actual quantized
  height with wrapped one-pixel offsets. Independent forward differences over
  262,144 texels give maximum error 0.549 byte and mean 0.236 byte; the flat-normal
  control fails at 18.244 bytes. Signal channels differ, with measured pairwise
  correlations below 0.034. Both maps redraw identically with auto-mips enabled;
  this stage does not independently verify every new mip level's contents.
  World-metric XY projection and native geometry-normal reorientation affect
  shading only. No universal steep-slope/triplanar acceptance is inferred.
- The road graph reports 235 pixel instructions, five texture lookups and three
  samplers, versus D26's textureless 203/3/1 control. Source baking and implicit
  Engine samples are separate costs. No generation speedup, city frame-time or
  packaged-runtime performance claim; the 16.67 ms target and INCONCLUSIVE policy
  remain unchanged. The heavier D26 water graph still needs runtime cost proof.
- Fresh/worn asphalt albedos use Epic's measured PBR values as a reference,
  not as visual acceptance: [native PBR documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/physically-based-materials-in-unreal-engine).
  ConcreteWarm/Cool remain textureless building-massing controls, not detailed
  facades. RiverClear/Muted reuse native optical coefficients and the previously
  reviewed shared volume-output projection; they are calm-water depth/bed samples,
  not a wave/flow, geometry-alignment or packaged ray-tracing proof.
- A disposable JSON control file updates seven native MID presets; complete
  typed-name/range validation runs before mutation. An unknown-control fixture
  is rejected with all eleven instance parameters, retention slots and state
  unchanged. This is not an admitted surface recipe or a production JSON writer.
- Corrected a gray-plane framing error before the final 40 matched close/context,
  neutral/grazing captures. Native sRGB display targets and the controlled D28
  lighting/exposure pattern are used only in this fixture. Visually inspected
  final road, building and water boards; all remain prototypes awaiting selection.
- Native autosave attempted a registered scratch duplicate and refused its
  private live-target references. The final road parent is native transient;
  explicitly owned unsaved duplicates were removed without saving. No autosave
  or validator was disabled. Durable guidance is in ProjectMaterial pitfalls.
- Removed owned sample/capture actors and discarded the unsaved fixture without
  saving. Reloaded original Kazan and verified clean packages, original materials,
  geometry bounds, actor count, road/water bindings and lighting/postprocessing.
  The 10,787 protected-file census and session Git-state checks are unchanged.
  Receipts/boards live in ignored
  `tmp/world/surface_presentation/texture_graph_runtime/quality_d30/`.
- Next permitted work is focused default-material refinement/visual selection;
  production archetypes, references, persistent adoption and exact cooked
  acceptance remain at their existing Q1/Q2 boundaries. D29 review stays closed.
- Independent architect: PASS for D30's isolated ownership, selected-height
  normal controls, transient lifetime, refusal and restoration evidence. No
  required PATCH findings. This is not operator visual or production approval.

### 2026-10-11 - D29 BrownEarth transient Kazan/Manhattan trials

- Recorded the operator's candidate-specific Q2a acceptance and T1g authorization.
  Preserved all three original 512-square GranularRaised outputs, the cheaper D25
  composition, sixteen scalar parameters and the accepted linear BrownEarth palette.
  No source texture generation, production recipe/material mutation, canonical
  compilation, World regeneration, Matrix, cook, release or production activation.
- Native function inspection and rendered RED controls found that supplying the
  world-up detail normal directly flattens slopes. The first attempted native WS
  blend was rejected. The private terrain copy uses `BlendAngleCorrectedNormals`
  with geometry as base; zero-relief controls at 0, +/-30 and +60 degrees have a
  maximum geometry-component error of 0.000791. Horizontal full-relief RGB buffers
  remain identical across the measured 40x40 region. Native shader cost changes
  from 294 to 301 pixel instructions, retaining ten lookups and five samplers.
  Independent architect review accepted this bounded normal/resource boundary.
- World-XY metric projection remains unchanged; this is not triplanar, steep-face
  or universal-terrain acceptance. City captures also record native rendered
  zero/full-relief normals. Interpolated shading normals are not collision-face
  normals, and a mean over a rendered crop is not an exact same-point comparison.
- Transported preserved numeric base pixels through ignored native snapshot assets
  and reconstructed diagnostic GPU targets with native mip generation. Direct
  snapshot CPU mips differed and were rejected as selected inputs. Each city's
  thirty sampled RGB role/mip readbacks matches the original source exactly. An early
  Kazan copy failed this gate; its scout images were rejected. Explicit mip-level
  wiring, retained copy material and verified replay produced the final targets.
  This is diagnostic transport, not an authenticated production manifest, a new
  cache request or proof of packaged adoption. Native unknown-domain save warnings
  on private transport assets were not suppressed or counted as asset admission.
- Bound one existing native PreviewMeshComponent per city through `SetMaterial`.
  That native section spans the loaded territory; cameras inspect small areas.
  No mesh splitting, channel changes, actor replacement or collision rebuild.
  A native lookup in Manhattan confirms three scratch consumer MIDs share the
  same three GPU targets; the terrain macro binding uses that same Masks target.
- Final evidence contains 56 matched existing/repeat/BrownEarth/restored captures
  per city across close, 1.7 m eye height, mid, farther, canonical seam, road and
  water views; actual native viewport captures accompany the higher-resolution
  captures. Sunlit anchors were visually checked; shadowed scouts are not used as
  successful detail evidence. Native city postprocessing, atmosphere, exposure,
  light intensity and neighboring materials are preserved. Diagnostic grazing
  angles are Kazan pitch -8/yaw 148 and Manhattan pitch -15/yaw 178; the latter
  avoids the tower occlusion found in the lower-angle control. These are matched
  A/B lighting conditions, not permanent city lighting changes.
- Authenticated the seam views against each existing canonical coverage manifest:
  Kazan cells x2/y1 and x3/y1 at world X 114100 cm; Manhattan cells x0/y2 and x1/y2
  at X -46459 cm. The metric texture phase does not restart at those cell edges.
  Existing road/water presentation and geometry limitations remain separate work.
- Encoded two matched 64-frame clips per city and verified every decoded source
  frame is distinct. Native PNG transport padding was removed without changing
  decoded pixels before encoding. The offline 24 fps playback is neither gameplay
  frame pacing nor a comparative GPU benchmark; performance remains INCONCLUSIVE.
- Restored original override arrays, sun rotation, geometry bounds and actor counts.
  Native Editor actor deletion modifies the level even for transient diagnostics;
  restored bindings alone did not clear its dirty flag. Reloaded each original map
  without saving after cleanup, then verified clean packages, native original
  bindings, postprocess/light fields and all 161/132 road and 145/115 water mesh
  bindings. Deferred native preview initialization was allowed to finish before
  the final Manhattan count check. Parameter iteration needed no Editor restart.
- The final protected inventory contains 10,787 unchanged files with no additions
  or removals. Current-session HEAD and index are unchanged. Engine preview/DDC
  activity during map loading is not a World compiler/production regeneration run.
  Durable slope-normal and cleanup guidance lives in ProjectMaterial pitfalls.
- A focused independent architect follow-up accepted the demonstrated dirty-map
  recovery. The original normal/resource review remains closed; no required PATCH
  findings or new production implementation were introduced by the city trials.
- Evidence: ignored `tmp/world/surface_presentation/texture_graph_runtime/quality_d29/`
  contains normal/mip controls, city boards, native captures, clips and restoration
  receipts. Visible close relief is retained; broad cloudy/repeated composition,
  warm low-sun tint and very dark city shadows remain observations for the operator.
  Stop at the city visual checkpoint. Q2 persistent adoption, exact cooked-source
  promotion, runtime GPU benefit and final release acceptance remain open.

### 2026-10-11 - D28 native color calibration and dry-earth candidate

- Kept D27's engineering PASS closed and Q2a PATCH/open. ProjectTexture, Texture
  Graph, cache, source generation and production material/compiler/settings were
  untouched. Copied original native MID parameters, including all four texture
  bindings over the same three 512 outputs; an omitted macro binding was detected
  by the full-binding RED check and corrected before the final comparisons.
- Inspected sun color/temperature/intensity/orientation, ambient sky, fog/cloud
  properties, complete capture postprocessing and relevant native CVars. The
  atmosphere-connected grazing sun tints even a gray surface. Native real-time
  Skylight updates skip SceneCapture views; eight or 64 capture ticks alone did
  not restore comparable ambient data. Main-view redraw materially changes the
  result. Property restoration is not a claim of identical prior sky pixels.
- Used native BaseColor and float SceneColorHDR captures to isolate the source,
  then native RTF_RGBA8_SRGB for display output. The old RTF_RGBA8 target uses linear
  display gamma. Matched gray linear/display pairs differ by 73.0 neutral and
  64.52 grazing bytes on average; the native output matches the expected gamma
  transfer within 1.156/0.412 bytes. Numeric Structure/Normal/Masks encoding never
  changed. PNG codes cannot establish scene-linear clipping.
- The controlled diagnostic rig uses a white non-atmosphere sun at 100,000 lux,
  native T_GreyAmbient specified cubemap with multiplier 50,000, fixed manual
  exposure bias -15.5 and white balance 6500 K. Capture-only fog/volumetric-fog,
  atmosphere and cloud flags are disabled; bloom/vignette/lens effects are excluded.
  These are fixture settings, not a prescribed city environment or lux measurement
  of the cubemap. Native unlit gray/white recover 0.181641/0.894531 from 0.18/0.90
  inputs. Grazing gray channel spread falls from 77.87 to 5.85 display bytes.
- BrownEarth is the retained compacted dry-earth palette on the cheaper original
  graph. Linear vectors are SoilDarkColor=(0.085,0.065,0.046),
  SoilLightColor=(0.24,0.18,0.12), GrainColor=(0.30,0.26,0.20). Its measured unlit
  luminance is 0.12717; Epic's [PBR reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/physically-based-materials-in-unreal-engine)
  lists bare soil at 0.13 as guidance, not a universal
  color/soil law. CoolEarth is the darker comparison, not a new source family.
  Matched close RGB is (124.70,107.21,84.59) neutral and (72.76,55.68,37.13) grazing.
  Blue-zero pixels in the grazing ROI fall from 82.69375% with the original palette
  in this rig to 0.0125%. Flat-normal and neutral-AO controls change the original
  image by 1.558/0.145 mean bytes; neither removes its warm low-blue response.
- Final proof covers 74 native terminal captures with 16,000-pixel ROI data each,
  matched unlit invariance and exact PNG/native-byte comparisons. Both palettes
  have close and player-eye 2/5/12/30 m neutral/grazing views. The original graph
  remains 294 instructions/10 lookups/5 samplers. D27's extra distance sample is
  a retained comparison with modest visible gain, not an accepted added cost.
  No GPU speedup or city performance is claimed.
- Eighty consecutive native motion frames contain 16 static controls and 64 distinct
  moving frames. The delivered 1280x720 MP4 independently decodes all 64 moving
  frames. Readback cadence and frame differences are not gameplay/shimmer acceptance.
- Restored all 245 original material references, 16 scalar and three vector values,
  sun and Skylight settings, and refreshed restored real-time sky through 64 native
  main-view redraws. Original three output hashes remain identical; no capture actor
  or protected dirty package remains. All 10,787 protected disk files are unchanged.
  Current-turn HEAD/index are preserved; older Git receipts are historical.
- Evidence: `tmp/world/surface_presentation/texture_graph_runtime/quality_d28/`,
  including color/distance/reference/layer boards, native float reads, motion,
  exact preview inputs and restoration/protection receipts. The historical D27
  image column is labelled separately; it is not a matched-lighting palette arm.
  No new architecture boundary or independent review was required. No build, cook,
  World/canonical regeneration, Matrix, staging, commit or release was performed.
- Q2a remains the operator's visual decision. Compacted-soil appearance, terrain
  projection/city trials, exact cooked-source adoption, production recipe/binding
  authentication and runtime cost are separate remaining boundaries. Stable native
  capture/color knowledge belongs to ProjectMaterial pitfalls.

### 2026-10-11 - D27 soil preservation and material-layer candidate

- Froze the GranularRaised 512 source and its three ready outputs. No generator,
  cache or public recipe contract changed. A cloned private native Slab graph
  adds one independently scaled sample of the same Masks target, using native
  CheapContrast and the existing smooth 5-18 m gate for pigment/roughness only.
  Normal, grain, source scale and near controls remain unchanged. Tile scale is
  not a claim about aggregate or patch diameter; the source contains smaller bands.
- LayeredConservative is the retained distance candidate; LayeredBroad adds more
  conspicuous cloudy variation and is not selected. Twenty-four HD images cover
  matched neutral/grazing close views and player-height 2/5/12/30 m views, bypass
  and restored-reference controls. The conservative neutral close image is exactly
  equal to the restored reference; comparisons to the initial frame differ by
  about 0.495 RGB byte, also observed in the unchanged-reference repeat. This is
  bounded near-pixel preservation, not an assertion of global identical rendering.
- All 262,144 original normal pixels match independent wrapped-height derivatives
  within 0.559 byte; the flat replacement fails above 90 bytes. Native explicit-LOD
  reads cover all 27 non-base mip levels and 262,143 pixels. Independent box
  reduction differs by at most 1.528 bytes; the cleared-texture control fails
  by 225.519 bytes. Alpha is not admitted. Mips were consumed from the original
  live GPU targets, with no source redraw, PNG import or packaged-runtime claim.
- Eighty consecutive native Editor frames include 16 static controls and 64 unique
  moving frames. A playable clip and frame sheet record the candidate at player
  height looking 5 m ahead. Readback affects cadence; motion is inspection evidence,
  not gameplay frame pacing, automatic shimmer approval or city performance.
- The material costs 306 pixel instructions, 11 lookups and 5 samplers versus
  the 294/10/5 reference. No extra structural target or generation is needed.
  GPU impact remains unverified; retain the unchanged reference if the visual
  gain does not justify the additional sampling.
- The exact restoration probe caught Editor property assignment trimming scratch
  retention slots from 245 to one. Native SetMaterial restored all 245 original
  identities; no retained object was missing. Original scalar/vector values, three
  map hashes, selected binding and lighting are restored; no capture actor or
  protected dirty package remains. All 10,787 protected disk files are unchanged.
  Current-turn HEAD and empty operator index are preserved; prior D19 Git-state
  comparisons are historical. Stable native pin/restoration knowledge belongs
  to ProjectMaterial pitfalls.
- Independent architect: PASS for this isolated boundary, with no required
  correction. Independently recomputed normal/mip checks, matched close pixels,
  consecutive motion frames and 48 pinned proof hashes; checked native restoration,
  current Git state and actual image boards. Close this engineering review.
  Q2a remains open. The prepared compacted
  dry-earth candidate is not an accepted universal soil or an admitted production
  recipe. `preview-inputs.json` is a private parameter/hash snapshot, not compiler
  authority. Curved/terrain projection, both-city evaluation, exact cooked source,
  authenticated production recipes/bindings and measured shader cost remain open.
- Evidence: `tmp/world/surface_presentation/texture_graph_runtime/quality_d27/`,
  including approval/layer/map boards, actual mip reads, motion clip/frame sheet,
  preserved inputs and final restoration receipt. No World/canonical regeneration,
  C++ build, cook, Matrix, release, staging or commit in this pass.

### 2026-10-11 - D26 focused quality comparison and independent default proxies

- Accepted the supplied D25 review's qualifications without reopening that closed
  engineering boundary. GranularRaised remains a compacted dry-earth prototype,
  not realistic universal soil. Its source is not yet the exact cooked backend;
  Editor graph proof and instruction counts cannot certify packaged cost or adoption.
- One focused native aggregate-interior comparison produced LooseInterior and
  LooseMixed at 512, retaining the existing height -> normal -> masks contract.
  Both were visually rejected: smoother interiors and uniform stippling weakened
  relief rather than producing convincing loose particles. No further palette,
  resolution or broad native-tool survey is justified by this result.
- Six actual maps redraw deterministically. Independent wrapped-height checks
  cover 524,288 new normal pixels with maximum error 0.555 byte; flat known-bad
  controls fail above 97 bytes. These establish isolated map coherence, not realism.
- RoadDry and BuildingNeutral reuse one native textureless Substrate Slab parent
  with distinct instances. Matched BasicShape proxies show neutral massing and a
  matte road control. They are not real city roads, facades or migration evidence.
  A single road-detail trial reused all three GranularRaised targets without new
  generation or allocation. Its cloudy appearance was rejected; RoadDry remains
  the simpler prototype control, not an accepted production asphalt material.
- Native Single Layer Water initially reproduced Epic UE-384718's missing volume
  outputs during shader compilation. A small native material-output projection of
  the same albedo/extinction parameters passed compilation and rendered depth/bed
  response against the opaque-blue control. No engine, plugin or renderer settings
  were patched. The stable source/formula explanation lives in ProjectMaterial
  pitfalls; packaged and ray-tracing visual parity remain unverified.
- Native shader statistics are 203/3/1 for opaque proxies and 933/3/2 for water
  (pixel instructions/lookups/samplers); the rejected shared-texture road trial
  inherits 294/10/5. Textureless means no authored texture dependency, not zero
  engine sampling. These are not GPU timings, city performance or speedup claims.
- Paired camera/light values match and actual rendered pixels differ. No new
  motion acceptance is claimed for rejected sources. All sample/capture actors
  are removed, original lighting and GranularRaised binding are restored, and its
  three target hashes remain unchanged. Road and soil share those same targets.
  The complete dirty-package probe finds only declared scratch mounts/template.
  Disk comparison confirms all 10,787 protected files unchanged; the current
  turn's HEAD and operator-owned empty index are preserved. The older D19 index
  receipt is historical, not this turn's Git-state baseline.
- Independent architect: PASS for this isolated boundary; no required corrections
  remain. Recomputed all six hashes and normal samples, inspected matched pixels,
  restoration, shared targets, dirty-package allowlist, disk/Git state and final
  documentation. Corrected the stale MacroPatch-only Q2a direction in place.
  This review is closed; repeat it only for a concrete new defect or boundary change.
  Q2a remains PATCH. Independent default samples may continue without accepting
  soil, but landscape application, production archetypes/bindings and visual
  adoption remain gated. Reproduce an accepted exact source through the cooked
  Texture Graph backend before promotion; do not cook rejected visual candidates.
- Evidence: `tmp/world/surface_presentation/texture_graph_runtime/quality_d26/`,
  including matched soil/surface/road boards, six maps, compiler RED/GREEN evidence,
  independent pixel checks and final live-state receipt. No World/canonical
  regeneration, C++ build, cook, Matrix, release, staging or commit in this pass.

### 2026-10-10 - D25 native material/source closure

- Reused Epic HeightLerp for two aligned metric samples of the same bundle, with
  one shared blend weight and inverse-scale correction of tangent normal slopes.
  One Slab remains. The material costs 294 pixel instructions, 10 lookups and
  5 samplers, versus the retained 261/7/5 control; no GPU speedup is claimed.
- Corrected grain meaning in the new private Structure B output. Fourth-band
  sparse occupancy supplies grain; the existing native Masks B supplies independent
  pigment. Added aggregate-surface grain and angular profile controls using native
  nodes. Production recipes, ABI, runtime cache and ready outputs remain unchanged.
- GranularFine/Rugged use 1024 targets; GranularBudget uses the same source at 512.
  GranularRaised adds the same sparse grain field to height at 0.075, correcting
  grains that were colored but suppressed by the maximum of larger clump heights.
  It is a separate 512 fixture identity, not a mutation of earlier ready targets.
  Source instructions are 2,179 before and 2,181 after this two-instruction change.
  Calculated RGBA8 mip storage per exported bundle is 16 MiB versus 4 MiB, with
  one separate raw intermediate. These are format calculations, not measured total
  physical residency, native transient peak or a packaged admission proof. Prefer
  the 512 candidate unless visible benefit and runtime cost justify 1024.
- All twelve maps redraw byte-identically into their own targets. Independent wrapped
  height checks cover 2,621,440 normal pixels with maximum error 0.560 byte; flat
  replacement fails above 111 bytes. Native flat/X/Y metric-blend controls pass;
  a deliberately unscaled secondary normal fails by about 20 bytes.
- Diagnosed black UI readback from a still-connected Substrate Front Material:
  native shading-model sanitation restores Surface. Disconnecting the diagnostic
  copy's root fixes it; production Slabs stay connected. The native draw already
  ensures shader completion, so the initial timing explanation was incorrect.
- Captured matched neutral/grazing HD, player-eye 2/5/12/30 m and actual 1.5 m
  orthographic squares. Those squares cover three primary 0.5 m periods; older
  perspective images named 3x3 are aerial diagnostics, not exact metric tiles.
  Original-period high-pass correlation falls from 0.954 to 0.414 for DualCrisp
  and about 0.709 for GranularBudget. This repeat proxy does not certify realism.
- Consecutive native Editor-frame capture replaces the prior pose-only motion
  sampling. Failed dark/recovery captures are excluded. The callback captures its
  own local state, and mutation waits for its terminal receipt. Readback affects
  cadence; this is not packaged frame pacing, city performance or shimmer approval.
  Both 512 variants have 80 consecutive engine frames, including 16 static controls
  and 64 unique moving frames. The final Raised clip contains all 64 moving frames;
  static/moving mean RGB deltas are 0.482/3.967 bytes, not a shimmer verdict.
- Scratch shader defaults now reference an existing accepted ProjectTexture target
  read-only, rather than an unrecognized experimental mount. Actual soil bindings
  are unchanged. Native reference validation succeeds without changing its policy.
  The closing probe lists every dirty package and rejects anything outside the
  two scratch mounts and the exact template map, including /Game. An architect
  finding corrected its earlier incomplete /Project-only filter; the narrow rerun
  passed. No capture actor remains, original lighting is restored, and all 10,787
  protected files are unchanged. HEAD and the operator-owned empty index are preserved.
- Independent architect: PASS after the single closing-probe correction. Rechecked
  all twelve hashes, 2,621,440 normal pixels, rejected controls, consecutive frames
  and restored live/disk state. This bounded review is closed; do not repeat it
  without a concrete new defect. Visual judgment stays
  PATCH: fewer repeated rows and explicit grain are progress, but smooth clay-like
  regions and insufficient natural composition still fall short of the photographs.
  Retain GranularRaised as the next private development candidate. Neither higher
  resolution nor the added grain height establishes realistic loose soil at eye height.
  Q2a remains the operator's visual decision; no landscape binding is authorized.
- Evidence: `tmp/world/surface_presentation/texture_graph_runtime/quality_d25/`,
  including source/normal controls, actual maps, HD/metric/distance boards and
  consecutive-frame receipts. Stable diagnostic knowledge belongs to ProjectMaterial
  pitfalls. No production generation, C++ build, cook, Matrix, release, staging or
  commit in this quality closure. R5/R6 authority and the 16.67 ms policy are unchanged.

### 2026-10-10 - D24 native graded soil quality and material comparison

- Installed UE 5.8.3 source and Epic's native-noise guidance grounded four size bands,
  Voronoi seed positions, wrapped Cellnoise, rotated facets, region-dependent density
  and aggregate-edge microstructure. Corrected scratch Step input ordering and signed
  noise range before final captures. ProjectTexture/ProjectMaterial production code,
  recipes, cache and ABI remain unchanged; no new compiler or custom HLSL was added.
- LooseGraded, MixedGranular and FirmWithGrains have real live-RT renders at close,
  player-eye 2/5/12/30 m and aerial views. LooseGraded improves close granular breakup
  over D23 ClumpedEarth. Repetition, harsh scattered dark features and insufficient
  natural composition remain; the reference photographs are not matched. Q2a PATCH.
- All nine raw maps repeat byte-identically into the same RTs. Independently checked
  height-derived normals have maximum error 0.559 byte; a flat-normal substitution
  fails. PBR statistics: 261 pixel instructions, 7 lookups, 5 samplers, one Slab;
  the separate native source bake has 2,188 instructions. Neither is a GPU timing.
- Six 1920x1080 native SceneCapture views record actual MID/RT bindings, camera,
  64-lux key direction and manual exposure bias 0, without physical-camera exposure.
  Corrected positional Rotator misuse and enabled persistent capture render state:
  without it, the exposure control produced identical pixels; with it, bias +2
  changed 6,220,798 RGB values and increased mean brightness from 171.01 to 235.21.
  Restored the fixture light afterward. These controlled lighting diagnostics do not
  establish photoreal PBR acceptance. Twenty-four actual MCP camera
  samples have diagnostic MP4 playback; continuous shimmer/frame pacing is unverified.
- Additional bounded ProjectMaterial reuse test: Epic Texture_Bombing on macro pigment
  alone costs 376 instructions/10 lookups with little benefit. Corrected whole-bundle
  normal sampling/encoding with native normal rotation reduces repetition but softens
  relief: linear blending costs 546/19; common-height blending costs 570/31. Retain these
  as rejected comparison candidates, not a new default. Earlier invalid/mismatched
  normal captures are excluded. Exact transformed-normal and GPU costs are unverified.
- The private source experiment uses Structure B for pigment; the production ABI owns
  grain/pore structure there. Reconcile those semantics under ProjectTexture before any
  promotion. D23 cooked/cache proofs do not certify this new source graph automatically.
- Independent architect accepted the isolated graded engineering boundary, checked all
  nine map hashes, normal errors, capture bindings and fresh inventory, and retained the
  visual PATCH. The additional native-bombing comparison has no adoption authority.
  One narrow closing architect recheck accepted rejection of the bombing variants
  and independently verified the manual-exposure pixel control; no further review
  of this bounded stage is needed.
- Evidence: `tmp/world/surface_presentation/texture_graph_runtime/quality_d24/`, including
  `soil-d24-before-after.png`, `soil-d24-distance.png`, `soil-d24-lighting.png`,
  `soil-d24-native-bombing.png`, raw-map/repeat/binding receipts and sampled motion.
  Fresh inventory: 10,787 protected files unchanged. No World/canonical generation,
  build, cook, Matrix, city application, release, staging or commit in this quality pass.
  Final live-state receipt confirms restored fixture light, capture actor absent from
  the live level and zero dirty production packages; LooseGraded stays a scratch preview.
- Next permitted closure is focused material composition: retain granular close detail,
  solve distant repetition without blur or disproportionate sampling cost, and complete
  projected-normal and motion quality controls. No broad native survey or infrastructure rewrite.
  Stop at Q2a before landscape application; production ABI/adoption gates remain separate.

### 2026-10-10 - D23 shared native PBR backend and bounded quality cycle

- Focused reviewer follow-up: freshly rebuilt isolated Editor replayed the exact
  existing CacheReuse, BudgetAndPins, FailureLifecycle and TwoNodeGeneration tests;
  all passed. These tests declare EditorContext, so the earlier packaged Game
  no-match attempt is not regression evidence. TwoNodeGeneration used a byte-identical
  scratch copy of the existing catalog and verified nonconstant, distinct, seam-safe
  mip 0/4 signals without changing package bytes. Logs: `review-default-final-*`.
- The claim that packaged presentation only consumes imported PNGs is incorrect.
  `ProjectTextureProbePresentation.cpp` binds the live stable GPU targets directly
  to two MIDs in Game. `preview_cooked_final.py` is the separate Editor comparison.
  The narrower request for explicit mip-content and consumer recovery proof was
  valid: private diagnostics now check six live bindings, detach/clear them before
  release, export all ten mips and repeat actual rendering after eviction/regeneration.
  `review-live-consumer` passed two packaged rounds, each with six matching bindings,
  zero retained bindings on detach and zero tracked native RTs after release.
  Thirty mip readbacks matched the downsampling pyramid with maximum RGB error
  0.5 byte. A correctly sized corrupted Structure mip 4 failed at 189.75 bytes;
  the original readbacks remained accepted. All three RGB hashes stayed unchanged.
- Follow-up instrumentation changes only the disposable probe, not the public owner
  or native port. Its authenticated 778-input fingerprint is
  `4f53e6c63853fb0d1d054ee5d0983a3aef66c907616fbecff3c295eeb908c4e3`.
  Evidence: `review-live-consumer-host.json`, `review-live-consumer-mips.json`,
  `review-live-mip-control.json` and `review-default-final.json`. Source-only staging
  reused existing cooked assets. No cook, production mutation or visual adoption.
  Independent architect recheck accepted these two qualifications within the
  isolated fixture and independently checked all thirty mip readbacks. Q2a realism,
  comparative performance and production adoption remain open.
- Boundary: ProjectTexture gained an opt-in async bundle adapter; its existing
  cache owns admission, reservations, sharing, pins, readiness and eviction.
  The accepted synchronous producer remains the default. Native Texture Graph
  runs only in the private disposable Win64 Development fixture. No production
  catalog, World binding, canonical output or release authority was enrolled.
- Whole-bundle promotion copies three validated native RGB exports into stable
  linear RGBA8 slots, generates wrapped/trilinear full mips and waits for the GPU
  fence. Ready never exposes partial siblings. Default/native writers cannot
  share owned targets. Pending catalog mutation refuses promotion; teardown
  uses frozen Ready keys rather than indexing a mutable node array.
- RED/GREEN: the real cooked mutation-shutdown RED crashed on an emptied catalog
  node array. Frozen keys fixed it; the exact Game shutdown fixture passed.
  Five exact owner tests passed: AdmissionIdentity, BundleOwnership,
  BundleFailureRecovery, BundleCancellationRecovery and BundleMutationShutdown.
  Independent runtime reviews accepted the corrected owner/lifetime boundary.
  Final semantic inspection also reproduced a Ready-query descriptor gap in the
  exact recovery test: a changed descriptor refused a warm request but still
  returned Ready. The query now compares the admitted descriptor before exposing
  readiness. The cooked recovery test failed before that fix and passed after it,
  preserving the accepted resources on refusal. All five exact tests passed again
  against the final compiled candidate. Independent PBR review accepted the numeric
  ABI and material boundary, then accepted this focused readiness correction.
- Native fixes remain private: one async batch, rooted graph/task outputs,
  deferred completion cleanup, cooked material-pin reconstruction and shader
  preflight, bounded 1x1 tile settings, one-tile output buffers, and release of
  working/combined RTs at fenced scheduler quiescence. The private patch manifest
  lists 20 changed native files, per-file reasons and upstream/candidate hashes.
  This is an exclusive serialized native instance, not general concurrent graph
  support. Shared external cooked material objects are assumed immutable.
- Current compiled source closure authenticates 778 source/schema/recipe/asset/
  configuration inputs. A corrupt manifest is refused with the good closure
  unchanged. The final fingerprint is
  `c55f5c26f60bc043943275bf010bdcb8c2a7932f9ae84afd57f35d1676bce9d9`.
  Earlier certified image/lifecycle measurements used the preceding fingerprint
  `b1b80f3b0b2e7c6f03d88d7a2f0a0c52c15841917afa4cfff680eff47935eb65`.
  Final native/equal-content reruns preserved all three corresponding RGB hashes,
  exact native/default RGB equality and zero tracked native RTs after release.
  Graph-owned state is duplicated at Configure; mutation of the original graph
  afterward preserved the generated RGB hashes. A missing native kernel was
  rejected before any native start, with all output resources/accounting intact.
- Real PBR ABI: coherent height-derived Normal (maximum encoded error < 1 byte),
  independent cavity/roughness/pigment RGB Masks, flat-height and U/V controls,
  deterministic repeated pixels, 512-square dimensions, linear sampling and ten
  mips. The equal-content native/default probe matched all three RGB outputs
  exactly. Alpha is outside this RGB ABI and is not claimed equal.
- Cooked lifecycle evidence: four final cold rounds, 402 cache hits, GC before/
  during/after work, unrelated World cleanup, cancellation and explicit pending
  shutdown. All four rounds released tracked native RT allocations to zero.
  Final measured native RT peak was 7 MiB; stable mipmapped output allocation
  was 4,325,376 bytes (about 4.13 MiB), within a 32 MiB conservative reservation.
  This is texture-allocation attribution, not zero total GPU residency.
  Shutdown telemetry is a last snapshot, not post-destruction resident state.
- ProjectMaterial consumption used the existing one-Slab parent and two shared
  MID consumers. Thirty actual 1920x1080 cooked neutral/grazing and 0.5/2/5/12/30 m
  views were rendered while changing palette/relief/roughness parameters without
  another native generation. Surface sampling remains seven texture samples;
  the existing Editor material statistic was 260 pixel-shader instructions.
- Equal-content performance diagnostic: native three-kernel draw timestamps
  totaled 166 us; warm requests averaged about 35 us over 100 requests. Default
  CPU enqueue was about 3.88 ms and its whole GPU timeline about 2.36 ms. These
  GPU scopes differ: native kernels exclude copy/mips and the default timeline
  includes the complete submission interval. Comparative speedup is INCONCLUSIVE.
  Cold native preparation includes GC and Ready latency includes frame scheduling;
  neither is pure GPU execution. The isolated fixture proves no city frame target.
- Three focused visual cycles reused native nodes and the same ABI: blend irregular
  aggregates with connected regions, reduce pores/adjust grain and relief, then
  reduce a fixed microheight contribution from 0.17 to 0.025 that overwhelmed the
  aggregate outlines. ClumpedEarth is the strongest development branch. It still
  lacks varied loose grains, convincing aggregate interiors and silhouette relief
  seen in the supplied natural-soil references; broad pigment remains cloudy.
  Q2a is PATCH, not accepted realistic-soil quality. No landscape application.
- Visual evidence includes matched actual MCP views, generated maps and a playable
  24-sample one-meter camera-motion MP4 plus frame sheet. Sampled playback is not
  continuous frame-pacing/shimmer acceptance. Editor neutral/grazing captures that
  produced identical pixels were excluded from lighting proof; the cooked views
  have distinct actual light responses. One owned offscreen Editor restart was
  necessary after a duplicate scratch-asset retry blocked MCP. Unique names fix
  that retry path; ordinary numeric iteration remains live.
- Evidence root: `<repo>/tmp/world/surface_presentation/texture_graph_runtime/`.
  Authoritative fixture receipts: `certified-quality`, `certified-missing-sibling`,
  `certified-equal`, `certified-flat`, `certified-u`, `certified-v`; owner automation
  logs `final-exact-*`; cancellation/shutdown/source-snapshot logs `final-*`.
  Final-source evidence: `ready-query-green`, `ready-query-AdmissionIdentity`,
  `ready-query-BundleOwnership`, `ready-query-BundleCancellationRecovery`,
  `ready-query-BundleMutationShutdown`, `ready-query-native-green` and
  `ready-query-equal-green`. The counterexample is `ready-query-red`.
  `native-port-candidate.json` and the private patch preserve upgrade inputs.
  Follow the stable ProjectTexture rebase procedure before any later adoption.
- Protected census: 10,787 Resource/World data/content and canonical-owner files
  had zero additions/removals/byte changes. Historical D19 HEAD/index differ due
  to external commits; the verifier reports that difference rather than claiming
  unchanged staging against the stale baseline. This agent did not stage or commit.
- Next boundary: operator Q2a judgment on the exact images and remaining realism
  gap. Production backend adoption also needs a fair measured benefit and broader
  lifetime/input proof; no switch is recommended solely from this private PASS.
  Historical R5, 16.67 ms/INCONCLUSIVE policy and unpublished 3.0.0 remain intact.

### 2026-10-10 - D22 isolated cooked runtime execution and GC correction

- Built and ran one disposable Win64 Development project under
  `tmp/world/surface_presentation/texture_graph_runtime/fixture/`. It contains one
  empty map and one preauthored five-node, 512x512 graph: native Noise,
  NormalFromHeight and three Output nodes. No city, production material, manifest
  or World/canonical output is an input. Required Engine content is incidental.
- Stock packaged Game failed during cooked graph PostLoad: metadata-derived
  expression signatures were unavailable with WITH_EDITOR=0. Successive focused
  probes exposed stock texture blob construction compiled out of Game and missing
  cooked Noise/normal shader permutations. These are observed native gaps, not
  merely a descriptor-based hypothesis or grounds for abandoning D21.
- The private candidate reuses installed native bodies in eight changed files:
  expression pin signatures for preauthored graphs; engine initialization/update
  and stock texture/blob creation; portable plugin includes/shader paths; Noise
  and normal shader availability; and GC-visible async task references. No shader
  algorithm, launcher installation, public plugin or ProjectTexture contract changed.
  Material/function MIC nodes and Game graph authoring are not supported by this proof.
- Two fixture cooks were used: initial stock cook and one necessary replacement
  after confirming missing shader permutations. All later C++ and lifetime fixes
  rebuilt the fixture Game and staged with `-skipcook`; no further cook, Matrix,
  World compilation/regeneration or release was run. The ALIS Editor stayed running
  offscreen. The fixture executable also ran hidden with RenderOffscreen.
- Initial correct readbacks still ended with exit 3 because fixture teardown
  destroyed the native singleton before task world-cleanup. Calling native task
  cleanup, releasing retained references and collecting garbage before destruction
  produced a clean exit 0. A pixel receipt alone was insufficient acceptance.
- Exact lifetime RED: forced GC after task creation and before Activate crashed
  in InitRenderBatch's weak graph reference. The native task's duplicated graph
  was not reflected. Three `UPROPERTY(Transient)` declarations retain original
  graph, duplicate and output RT references through the held task. The same cooked
  test then returned valid pixels and exit 0; a repeat retained identical CRCs.
  This fixes the demonstrated graph lifetime only, not every async lifetime case.
  The exact runner was `run_packaged.ps1 -Package native-gc-green-package
  -Label native-gc-repeat2 -Render -GCBeforeActivate` under the fixture root;
  a rerun needs a fresh receipt label. Build/stage wrappers and RED package/log
  remain beside it for reproduction without a new cook.
- GREEN receipts `runtime-native-gc-green.json` and `runtime-native-gc-repeat2.json`
  report WITH_EDITOR=0, callback/readback success and three nonconstant 512x512
  outputs. RGBA CRC32 values are 172302326, 1771527761 and 172302326. Structure
  has red range 21..236; Normal has 125..130. MasksControl deliberately aliases
  Noise: it is a control output, not the accepted packed PBR mask ABI. Inspected
  PNGs show noise/normal diagnostics, not a new soil candidate or Q2a completion.
- The repeat's host receipt authenticates executable/IoStore/Pak SHA256, unchanged
  package inputs and actual exit 0. The 401.088/419.793 ms measurements include
  initialization, readback and PNG writing; they are not GPU generation duration,
  warm-cache evidence, material frame cost or city-performance acceptance.
- Source pinning covers 783 installed Source/Shaders/Content files and the exact
  authored graph, in `receipts/pinned-inputs.json` and `installed-native-tree.json`.
  This is the exact installed source, not an assertion of pristine Epic upstream.
  Device shutdown drain code is identical to that installed source; the reviewer's
  suspected missing Device.cpp patch was checked and withdrawn. Preserve the
  eight-file patch/manifest for private upgrade comparison. For a future retained
  fork, verify new installed-source hashes, review/rebase each native diff, rebuild
  Editor and Game, then repeat cooked load, output and GC regression checks before
  adoption. No Epic source was added to the public tree; license/distribution
  review is required before adoption.
- Independent architect review passed bounded cooked execution, then passed the
  focused GC RED/GREEN correction. No more review of this completed boundary is
  needed. Recursive FlushRenderingCommands warnings remain visible and require
  assessment before sustained runtime integration; clean exit does not erase them.
- Final protected inventory: 10,787 files, zero additions, removals or changed bytes.
  Resources/World Data+Content and CanonicalCompilation source stayed unchanged.
  Git HEAD/index differ from the older D19 receipt because external Git metadata
  advanced; this execution performed no staging or commit. Do not restate the
  older unchanged-index claim as proof for this run.
- Next permitted work is T1h steps 4-6: complete output retention/ABI, native pool
  residency and actual GPU release, existing ProjectTexture identity/admission,
  failure recovery and equal-output cold/warm cost comparison. These are
  UNVERIFIED. Do not install the private candidate into production, widen shader
  support speculatively or apply it to cities based on this bounded PASS.
  Stable SOTs were not rewritten because no production backend or public contract
  was adopted. Q2a remains PATCH; release approval remains with the release router.

### 2026-10-09 - Native runtime review and extension boundary

- The source envelope is UE 5.8.3, changelist 58210709, from the configured launcher
  engine's `Engine/Build/Build.version`. Texture Graph's descriptor says `1.0 Beta`,
  `IsExperimentalVersion=false`, and declares TextureGraph/TextureGraphEngine as
  Runtime modules. The review's Experimental label is not the installed flag.
- Actual execution has a deeper gap: in
  `Engine/Plugins/TextureGraph/Source/TextureGraphEngine/TextureGraphEngine.cpp`,
  `InitEngineInternal` (131; guard 138), destruction (173), stock initialization
  (313) and `Update` (352; guard 357) keep necessary work under WITH_EDITOR.
  `TextureGraphEditorModule.cpp` owns Create/Update calls at 105/123. Consequently
  the unchanged Game path cannot initialize/advance that scheduler merely by
  calling the advertised Blueprint function. This is source evidence, not a
  measured packaged crash or a BLOCKER against D21's possible extensions.
- A simple subclass cannot supply those internals: TextureGraphEngine's constructor,
  initializer and scheduler/device storage are private; Create/Update are static.
  A scoped plugin port must also address TextureGraphEngine.Build.cs:110's
  EngineDirectory-only placement guard and portable shader/plugin path resolution.
  At this source-review checkpoint no plugin fork, host-engine patch or compiled
  runtime candidate existed. The isolated D22 execution above supersedes that status.
- Stock TextureHelper::InitSolidTexture/InitStockTextures (64/108) are Editor-gated
  and require GEditor. RenderMaterial_BP.cpp's texture/scalar/vector setters
  (173/190/198) use Editor-only MIC mutations. Merely lifting the engine update
  guard would leave these additional gaps; native shader-node-only and material-
  function paths must be tested separately before choosing the retained patch.
- TG_Graph.h:140/146/303 exposes native node creation/evaluation outside the Editor
  guards. The old Python authoring limitation does not disprove C++ graph creation.
  It also does not prove cooked graph serialization, shader availability or pixels.
- TG_AsyncRenderTask.cpp:42 disables batch result caching; :203 duplicates the
  graph; :118 gathers internal RTs; :237 broadcasts after finalization/combination.
  These semantics do not provide ALIS stable slots, GPU timing or release proof.
  Device_FX.cpp:586 has a format-keyed RT pool with strong references and reuse;
  disabling batch cache does not eliminate that pool. Inspect its budget/lifetime
  and account it under ProjectTexture admission rather than adding another cache.
- Existing ALIS cache eviction is metadata removal
  (ProjectTextureRuntimeCache.cpp:103); RequestOutput writes stable export targets,
  and ReleaseTransientTargets releases owned intermediates. Neither metadata LRU
  nor D19's uncached Editor repeat establishes exported/native pooled GPU release
  or an async shared-operation integration. Those are T1h proof obligations, not
  confirmed regressions in the current bounded producer.
- Reviewer points about separate normal/shadows, runtime evidence, output identity
  and memory accounting are valid. Switching backends alone is not evidence of
  better grains, aggregate structure or faster per-frame shading. The attached
  photographs remain artistic references with different lighting/scale.
- D20 selects the native runtime investigation over the competing CC0 benchmark
  recommendation. D21 replaces automatic abandonment with bounded extension
  investigation. No package/cook/build, Editor mutation, production code/config
  change or World regeneration was performed for this source review. D22 subsequently
  closed Q3 for the isolated fixture; packaged support still requires actual execution.

### 2026-10-09 - D19 autonomous quality cycles and source limitation

- Executed three hypothesis-driven cycles in the same offscreen Editor. All work
  stayed in unsaved ProjectTextureTest/ProjectMaterialTest candidates and ignored
  `tmp/world/surface_presentation/t1h_quality_cycles/`. Production compiler code,
  recipes, manifests, runtime cache, World/canonical outputs and staging remained
  protected. Stable contracts did not change and require no documentation rewrite.
- Source diagnosis: the retained generator's radial Voronoi falloff created rounded
  isolated mounds that its normal correctly reproduced. Cycle 1 added stock periodic
  multiscale Noise to break contours/interiors, without adding custom shader code.
  FracturedAggregates improved edges but retained isolated mounds; BrokenEarth was
  too weak. Cycle 2 replaced that radial aggregate profile with a connected native
  multiscale field and retained sparse existing Voronoi grain. ConnectedEarth gave
  continuous rough structure, but its grazing response remained overly porous.
- Cycle 3 compared the same source with native SpiralBlur's generation radius
  reduced from 1.4 to 0.35 texel. It also tested installed TextureVariation using
  one coherent UV/derivative route for Structure/Normal/Masks, with random rotation
  disabled. Native dither was a paired control, not a fourth source cycle. Both
  variation arms exposed visible patch boundaries, including in a native viewport
  capture, and were rejected. Native Engine First does not require adopting a
  native function that fails the actual visual comparison.
- Selected development candidate: ConnectedEarth-Sharp, with the sharper source,
  restrained 0.45 cm shading-height control and 0.22 clump-composition pigment
  influence. Broad pigment strength remains independently controlled at 0.08 near
  and 0.30 far. Existing full mips/trilinear filtering handle normal/grain distance;
  no manual relief fade or extra normal texture was added. It retains one opaque
  Slab, constant dielectric F0 and the three-output bundle. Horizontal world-XY
  sampling/normal space is fixture-only; terrain projection remains unverified.
- Selected shader cost is 260 pixel instructions, seven pixel texture lookups,
  four samplers and 148 vertex instructions, versus DetailMacro's 258/7/4/148.
  The rejected non-dither variation arm costs 292 pixel instructions, ten lookups
  and six samplers; its additional complexity is not retained in the selection.
  Three 512-square RGBA8 full mip chains retain 4,194,300 logical bytes per bundle.
  These are uncached diagnostic outputs, not new authenticated catalogs. Prototype
  generation has four raw/filter/normal/mask draws and a raw intermediate, so it
  does not prove direct integration into the accepted three-node runtime DAG.
  Physical GPU residency, GPU milliseconds, cold completion cost and new-candidate
  cache enrollment remain unverified. CPU enqueue timings are labelled accordingly.
- `quality-reference.png` compares rendered DetailMacro/selected crops with Poly
  Haven's [Brown Mud reference](https://polyhaven.com/a/brown_mud). The reference is
  a downloaded image for visual analysis only; lighting and framing differ, and no
  reference texture was imported, rendered or adopted. `quality-close.png`,
  `quality-player-near.png`, `quality-player-far.png` and `quality-maps.png` retain
  matched cameras/lights and decoded source maps. Selected/retained captures cover
  close, 2/5/12/30 m player-eye and aerial views. Native viewport FOV is measured
  at 60 degrees; the initial assumed-90-degree repetition proxy was corrected and
  is not acceptance evidence. Measured repeat correlations still expose the half-
  metre source period, especially vertically; repetition is not declared solved.
- `quality-motion.mp4` has 48 decoded distinct frames comparing selected/DetailMacro
  at 5/12 m. Each underlying sequence uses 24 actual native MCP poses across one
  metre, at 1.7 m eye height; `quality-motion-frames.png` provides fallback stills.
  Eight-FPS presentation is not measured frame pacing. Continuous shimmer remains
  unverified; this experiment does not accept packaged/city performance.
- Five map records pass the wrapped central-difference normal check within one
  byte; a deliberately flat normal is rejected. Source pixels are identical for
  the sharp and variation arms. An exact four-draw repeat produces identical maps;
  100 MID relief changes cause zero source draws and zero changed map pixels.
  The prior DetailMacro source pixels remain unchanged. These checks establish
  diagnostic repeatability/reuse, not runtime cache readiness or authentication.
- Fresh exact Editor tests passed separately: `Project.Texture.Soil.NormalControls`,
  `Project.Texture.Runtime.CacheReuse`, `Project.Texture.Runtime.BudgetAndPins` and
  `Project.Texture.Runtime.FailureLifecycle`. NativeCandidates was not rerun:
  its source explicitly admits one run per Editor lifetime. Existing cache-policy
  tests do not certify the newly generated uncached candidates as runtime entries.
- Visual result remains PATCH. The selected branch adds readable fine relief and
  fewer isolated round pits at almost unchanged material cost. It still lacks the
  reference's irregular granular aggregate hierarchy, shows source repetition and
  can resemble rough clay/porous noise. Three cycles reached a source-quality
  plateau for this compact native-noise approach; further arbitrary variations are
  not justified. A bounded CC0 reference-based fixture comparison is a proposal
  for a separate operator decision, not an adopted source or permission for cities.
- Native inspection found zero dirty production packages, exact sibling references,
  one Slab per parent and wrapped trilinear full mips. All 10,787 protected files
  retain their inventory/bytes, HEAD and staging. No World/canonical generation,
  Matrix, C++ build, Editor restart, cook or release ran. Editor background throttle
  and autosave match their captured original settings; the same offscreen Editor
  remains open with the selected transient fixture.
- Independent architect verdict: PASS for bounded diagnostic engineering, with no
  required corrections. The reviewer inspected source/material scripts, actual
  boards and raw dither captures, independently recomputed normal error (0.559
  byte maximum) and unchanged repeated/MID map pixels, and checked the four exact
  test successes in the Editor log. This verdict does not approve aesthetics,
  persistent integration, cache enrollment, GPU cost or sloped-surface correctness.
- Supplemental viewport binding/revert captures have mean RGB deltas of 2.262/255
  for the candidate and 0.607/255 after restoration. An exploratory 10x separation
  assertion failed; without an unchanged-frame control this is diagnostic only,
  not rendered recovery acceptance. Exact map preservation and restored Editor
  preferences above remain separate proofs. Continuous shimmer also stays open.

### 2026-10-09 - D18 existing-bundle detail/macro composition

- Applied the operator's bounded material experiment in the already running
  offscreen Editor. Expected changes were unsaved ProjectMaterialTest parents/MIDs,
  fixture screenshots and the affected task records only. ProjectTexture source,
  derived maps, production material authority, World/canonical outputs and staging
  were protected. No public contract/dependency changed; no new architecture review
  was needed at this boundary.
- Three matched parents reference the same existing SoftPores Structure/Normal/Masks
  objects, full mip chains and trilinear filtering. The combined DetailMacro-MipsOnly
  preserves SoftPores decoded normal and grain routes without a manual distance
  fade. It samples existing Masks B once more at 19.7 m with independent phase,
  blends pigment strength from 0.22 to 0.82 smoothly across 5-18 m camera distance,
  and adds bounded broad roughness variation (strength 0.08). Each parent retains
  one opaque Substrate Slab. No generated texture set or production recipe changed.
- Compiled native statistics for SoftPores/MacroPatch/combined respectively are
  236/261/258 pixel instructions and 6/7/7 pixel texture lookups, with 148 vertex
  instructions, zero vertex lookups and four samplers each. Thus the combination
  costs 22 instructions and one lookup above SoftPores, and three fewer instructions
  than MacroPatch. Graph TextureSample counts are 3/4/4. Material GPU milliseconds,
  physical residency and city performance remain unverified; these counts do not
  accept the 16.67 ms release target or alter INCONCLUSIVE policy.
- Evidence root: `tmp/world/surface_presentation/t1f_detail_macro/`. Twenty-four
  native 1920x1080 captures compare all three candidates under matched neutral and
  grazing light. `player-near.png` covers 2/5 m; `player-distance.png` covers 12/30 m.
  Eye height is 1.7 m above one fixed 80 m plane; labels describe the center-ray
  ground aim distance, not every pixel's distance. The horizontal world-XY normal
  fixture does not prove terrain projection or slope seams. The white horizon
  clouds belong to the existing fixture sky, not soil composition.
- `detail-macro-motion.mp4` contains 48 decoded distinct frames, with three matched
  candidate panels at 5/12 m. Each source sequence has 24 actual native MCP poses
  across a one-metre camera path; eight-FPS playback is an evidence presentation
  choice, not measured frame pacing. `motion-frames.png` supplies numbered fallback
  stills. Continuous animation shimmer and operator playback inspection remain
  unverified; a delivered single-frame GIF cannot establish either.
- Corrected the duplicate-board interpretation: the previous local refinement and
  distance boards differ in 578,127 pixels, including image rows. What the operator
  received is unknown, so this does not refute an upload/delivery duplication.
  The replacement near/distance boards differ in 2,774,780 pixels and are distinctly
  labelled; delivery is not inferred from local file validation.
- Visual verdict remains PATCH. The combination keeps more close detail than the
  MacroPatch reference while introducing broader variation, but repeating mid-scale
  pattern, rounded aggregates and smooth/cloudy distant composition remain visible.
  It does not yet match the supplied granular soil reference. Stop at Q2a for the
  exact candidate decision; do not infer permission for city application, source
  rewriting or additional architecture from successful material compilation.
- Native checks found one Slab per parent, shared texture references, full mips,
  trilinear filtering and zero dirty production packages. Before/after decoded map
  comparisons have zero changed pixels in all three maps. All 10,787 protected
  World/resource Data/Content and canonical files retain their bytes and inventory;
  HEAD and staging remain unchanged. No C++ build, Editor restart, World generation,
  canonical compilation, Matrix, cook or release ran. Background throttle/autosave
  were restored without persisting preferences; the same background Editor remains
  open with the combined preview. Only this todo and the release router changed.

### 2026-10-09 - LooseAggregates focused refinement and material composition

- D17 continues isolated development after the operator's visual PATCH. The
  original LooseAggregates control is retained beside exactly three variants:
  SoftPores, MacroPatch and HybridCompact. Q2a remains OPEN; none is a production
  recipe, authenticated runtime catalog, city application or acceptance decision.
- Expected changed owners: disposable ProjectTexture source/derived-map previews
  and ProjectMaterial parent clones/MIDs, plus the affected task records. No
  production code, schema, dependency, recipe, generated resource or World contract
  changed. No C++ rebuild, Editor restart, independent boundary review, canonical
  compilation, World regeneration, Check/Matrix, cook or release was required.
- Native functions were inspected through the installed UE 5.8.3 material API.
  DetailTexturing uses Color/Normal samplers; WorldAlignedNormal also expects
  native Normal sampling, while this bundle is encoded linear RGBA8. They are not
  drop-in replacements for its decoding. TextureVariation exists and exposes
  shifted UVs/derivatives, but includes multiple sampling paths and optional
  rotation/dither. TextureBombing was not found in the installed engine function
  census. Native expressions are sufficient for this bounded macro comparison;
  no generic native-tool survey, custom HLSL or replacement framework was added.
  The [Epic detail-texturing guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/adding-detail-textures-to-unreal-engine-materials)
  and [texturing-function reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/texturing-material-functions-in-unreal-engine)
  support the composition techniques, not visual approval of these candidates.
- SoftPores reduces pore/grain controls, relief and cavity influence. Sparse grain
  is weighted around aggregate-mask edges using native arithmetic, coherently in
  height and the grain signal. Normal and Masks are regenerated from that same
  resulting Structure through their existing native generators. HybridCompact
  reduces loose coverage/population and adds broader smooth-region influence.
  These are explicitly uncached scratch maps; ready cached identities were not
  retargeted. Experimental allocation/residency is not a runtime-cache budget proof.
- MacroPatch uses the same source pixels as SoftPores and one additional sample
  of the existing Masks at a 19.7 m period with a different phase. It blends broad
  pigment and roughness independently of the 0.5 m grain scale. Initial subtle
  tinting still exposed the repeated grid; stronger palette composition improved
  it. A smooth 3-12 m distance transition reduces normal/grain-color detail in the
  larger views. Existing full mips and trilinear filtering remain enabled.
- All parents retain one opaque Substrate Slab and constant dielectric F0. Decoded
  XY/Z slopes are scaled and normalized; encoded RGB normals are not blended as
  colors. This larger horizontal fixture samples world XY metres and explicitly
  feeds matching world-space normals. It does not prove terrain/slope projection
  or change the production soil UV/tangent contract. Sloped World application
  remains gated, with its native projection/normal-space proof still outstanding.
- Evidence is under `tmp/world/surface_presentation/t1d_loose_refinement/`:
  `soil-refinement.png`, `soil-distance.png`, `soil-maps.png`, 24 raw 1600x1200
  captures and three 24-frame native MCP motion GIFs. Fixed lighting/cameras show
  neutral/grazing close crops, 1.5 m / 3x3 tiles, and 4/12/30 m larger planes.
  World XY coordinates preserve a 0.5 m source period at every plane size.
  `state.json` records all provisional source/material controls; scripts remain
  disposable diagnostics, not a new compiler or committed input.
- Byte-pixel normal reconstruction has maximum error 0.55-0.56 byte against
  wrapped height derivatives. The deliberately wrong flat-normal control differs
  by 81-87 bytes and is rejected. SoftPores versus MacroPatch has exactly zero
  changed source/normal/mask pixels; their visual comparison isolates material
  composition. Wrapped height edge mean steps are 0.53-1.74 bytes. A rendered
  flat-normal/neutral-AO control removes the apparent puncture shading; screenshots
  do not establish literal geometric holes or their independent pore/AO cause.
- Motion reprojection gives mean adjacent differences of 0.30/0.32/0.62 byte and
  p95 of 1/1/2 bytes for SoftPores/MacroPatch/HybridCompact. HybridCompact has fewer
  matching features (minimum eight inliers), making its registration weaker.
  Lighting, interpolation and temporal history contribute; these are diagnostics,
  not continuous 60 FPS or isolated shimmer acceptance.
- Matched native shader statistics: Original/SoftPores are 236 pixel instructions,
  six pixel lookups, zero vertex lookups and four samplers; MacroPatch/HybridCompact
  are 261 instructions, seven pixel lookups and the same four samplers. The graph
  has three versus four TextureSample expressions. The incremental matched cost
  is 25 instructions and one lookup. The earlier 243-instruction UV/tangent sample
  used a different fixture path (three vertex plus three pixel lookups); do not
  subtract it directly as if settings/projection were identical.
- Twelve local ProfileGPU attempts captured idle/update work without BasePass;
  all are excluded as material timings. Incremental material GPU time and physical
  residency remain UNVERIFIED. The earlier 6.14 ms isolated frame is historical
  diagnostic evidence only. Neither it nor this experiment accepts city cost,
  changes the 16.67 ms target or supersedes INCONCLUSIVE performance policy.
- Visual recommendation: MacroPatch is the best balance of softer relief and
  broad distribution; SoftPores still repeats, while HybridCompact loses too much
  close-up structure. Broad repetition is reduced, not eliminated. Rounded isolated
  clumps/punctures and muted near-ground grain remain visible; final-quality soil
  is not claimed. Present these exact candidates for Q2a inspection before further
  source complexity or any city adoption. No native anti-tiling layer was needed
  to produce this comparison; its benefit/cost remains conditional and unverified.
- The release router and separate alignment/road/water/building records retain
  their own gates and sequencing. Their materials are not implemented or accepted
  by this soil experiment. Production persistence, source promotion, cache/residency,
  compact-mask comparison, terrain projection and city performance remain open.
- Final inventory: all 10,787 protected World/resource Data/Content and canonical
  files retain their bytes; HEAD and index retain their initial identities. Live
  verification found zero dirty production packages. Original background throttle,
  autosave and profiler-UI settings are restored; reflected preview-setting edits
  used NEVER notification and did not persist user preferences. The same offscreen
  Editor stays running with the MacroPatch preview available. Changed tracked
  files are six task records only; no source, asset, staging or commit changed.

### 2026-10-09 - T1d native quality experiment and Q2a candidates

- The operator authorized execution under D10/D15/D16, with Q2a still blocking
  landscape application. This was an isolated quality experiment, not a production
  recipe, persistence, World, or release change. No public contract or plugin
  dependency changed; another architectural review is not needed for this boundary.
- Installed Texture Graph was already enabled and loaded. Its native factory and
  Slate palette created an unsaved graph and rendered a 1024-square Noise preview.
  Its graph creation/connection functions are C++ only in the installed headers;
  Python/MCP exposed input setters/render/export, but not graph construction.
  The bounded trial proved native authoring/rendering, not a finished multi-output
  soil graph or inferior Texture Graph quality. Adding a bridge was unnecessary
  for this round. No production plugin configuration was changed.
- Selected native material expressions at ProjectTexture's existing owner:
  VectorNoise Voronoi nearest-seed RGB/distance A, wrapped seed positions into
  Cellnoise, periodic Gradient noise, native remaps, and live MID parameters.
  Per-cell radius, presence and amplitude produce distinct compacted, loose-clump
  and gravel-rich structures. Existing owner kernels derive separate normals and
  packed cavity/roughness/pigment masks. Directional lighting is not baked into maps.
- New exact fixture `Project.Texture.Soil.NativeCandidates` freezes three distinct
  identities and generates their three-output bundles through the existing runtime.
  Each 512-square linear RGBA8 full-mip bundle uses 4,194,300 logical bytes. Each
  returned 100 warm hits with zero regeneration. Initial CPU request-to-ready times
  were 7,342 ms (cold shader compilation included), 89.9 ms and 88.3 ms; these are
  not GPU completion times or an endless-world residency proof. The fixture is
  marked for a user-instigated rendering session and requires an isolated template
  World; it retains only this fixed cohort until Editor shutdown.
- The first fixture run crashed because native material recompilation collects
  garbage and the new transient material was not yet strongly owned. Holding
  material/catalog/MID/derived objects before compilation fixed that fixture defect.
  RED crash and GREEN receipts are retained. No production behavior was implicated.
  Final project-wrapper build passed in 19.09 seconds (one test translation unit
  and its links). Fresh exact `NativeCandidates`, `Project.Material.Soil.AuthenticatedSample`
  and `Project.Texture.Soil.NormalControls` returned terminal Success in
  `editor-final-green.log`. `NativeCandidates` uses the complete `Project.Texture.Soil.` prefix.
- The user-instigated offscreen verification session initially stopped at native
  package recovery, then waited at 3 FPS due to native background throttling.
  The recovery descriptor contained exactly this experiment's seven disposable
  packages; its bytes were backed up before native auto-decline. Native reflected
  settings temporarily disabled background throttling and scratch autosave.
  Source inspection caught that the autosave property's notification saves its
  user preference; the original enabled value was restored in both settings and
  memory. Background throttling was also restored after verification. No tracked
  project configuration, foreground window, or production recovery was needed.
- Visual inspection rejected the first candidates' abrupt cell boundaries and weak
  neutral-light relief. A native `SpiralBlur-Texture` generation-only pass (2 distance
  steps, 8 radial steps, 1.4 texel distance) softened those edges. Live controls then
  reduced fine-noise dominance and varied aggregate/grain populations and palette.
  These later parameter experiments use explicitly uncached scratch RTs, preserving
  the frozen runtime cohort's identity. Their 12 additional full-mip 512 RTs consume
  16,777,200 logical bytes; this is diagnostic storage outside the runtime cache.
  Their costs/hits must not be attributed to the authenticated cached production path.
- Disposable, unsaved clones of ProjectMaterial's compiled soil parent render the
  candidate maps through its existing opaque Substrate PBR routes. All three remain
  243 pixel instructions, 3 texture samples and 4 samplers. This is a visual fixture,
  not production manifest authentication or save/reload proof. Native autosave
  refused the clones' private transient RT references; no persistent adoption is
  claimed. Production generation/rollback/single-writer guards remain unchanged.
- Evidence is under `tmp/world/surface_presentation/t1d_native/`: `soil-candidates.png`,
  `soil-maps.png`, individual 1600x1200 lighting/3x3/far captures, native map PNGs,
  and three moving-camera GIFs with 24 actual native MCP frames per candidate.
  `final-state.json` records the parameters and physical fixture UV adjustment:
  one 0.5 m tile or nine tiles over a 1.5 m plane. The near views are close crops;
  the tiling views show the entire plane. Editor simulation was also exercised.
- Native normal reconstruction against the exported height pixels has maximum
  error below 0.56 byte, including wrapped derivatives. Height edge mean steps are
  0.92-2.13 bytes. Motion reprojection has mean adjacent differences of 0.36-0.50
  bytes and 95th percentile 1 byte; interpolation/lighting/history also contribute.
  These diagnostics do not certify continuous 60 FPS packaged-runtime shimmer.
  Replaying the native experiment after the final build reproduced every compared
  height-R, normal-RGB and mask-RGB pixel exactly against the retained map board;
  `receipts/replay-map-comparison.json` records zero changed pixels. This proves
  that bounded replay, not portable production authority or all possible inputs.
- Native ProfileGPU on a forced 1600x1200 sample render reported 6.14 ms frame time;
  ordinary offscreen idle profiles contained only Slate and were excluded. This
  single fixture frame is not the World envelope or isolated material GPU cost.
  Physical GPU residency remains unmeasured. The 16.67 ms target and INCONCLUSIVE
  policy remain unchanged.
- The candidate board shows materially different relief. LooseAggregates is the
  preferred starting direction for general soil; Gravelly is a distinct coarse mix,
  and Compacted is smoother. Recognizable pigment repetition remains in 3x3 views;
  no antitiling/projection layer was added before source approval. Realistic-soil
  quality is an operator decision, not inferred from tests or these measurements.
- Protected inventory: all 10,787 World/resource Data/Content and canonical-tool
  files unchanged, index hash unchanged, HEAD unchanged, dirty production packages
  empty. No canonical compile, World generation, Matrix, cook, R5 or release ran.
  Q2a remains OPEN. Production integration of the selected native experiment,
  compact packing, real GPU residency/cost and landscape acceptance remain pending.

### 2026-10-09 - Soil visual-review fixes and distribution candidate

- Both reviewer code findings were reproduced, not inferred from the verdict.
  The exact soil integration generated a second scratch-root output/manifest while
  first-root UObjects remained loaded. It also accepted swapped Normal/Masks
  textures and cavity routed into roughness. RED logs are under
  `tmp/world/surface_presentation/soil_review_patch/receipts/regression-red.log`.
- ProjectTexture now preflights every fixture's actual mounted output file and
  clean, known loaded-package origin before policy registration or output writes.
  Read-only Validate uses the same admission. Copied accepted bytes in another
  root cannot authenticate foreign cached UObjects. The negative controls retain
  first-root bytes, make no regeneration output/manifest in the second root,
  retain copied validation bytes and remove the temporary mount on early exits.
- ProjectMaterial now verifies the connected soil texture/channel routes, including
  Normal's RGB output. The independent recheck caught a missing output-index guard
  and Validate admission parity. Both received actual RED controls, then GREEN;
  `receipts/review-red.log` records those two failures. Final recheck returned PASS
  for this local correctness boundary; no further review loop is required.
- The final incremental project-wrapper build succeeded. Fresh exact native tests
  passed: `Project.Material.Soil.AuthenticatedSample`,
  `Project.Texture.Soil.NormalControls`, and
  `Project.Texture.Generation.AssetCompilerContract`. These are this follow-up's
  current checks; the earlier nine-test record remains historical evidence.
- One source-kernel iteration retained three native outputs and their layouts.
  Patch-weighted aggregate populations, sparse grain and a weaker periodic warp
  reduce long ridges. Pigment and roughness now include variation independent of
  elevation. Pattern/material authority, native Substrate, and production host
  guards are preserved. This is disposable fixture generation, not production
  recipe enrollment or a demonstrated soil JSON hot-update extension.
- `soil-comparison.png` compares previous/new neutral and grazing samples with
  the same camera, palette, tile scale and relief. The Editor's viewport capture
  size changed with its layout; matching used real native `HighResShot 589x433`
  after settled MCP controls, without resizing either comparison arm. Individual
  maps are in `soil-maps.png`; `soil-quality-controls.png` shows the inspected
  [dry-soil reference](https://polyhaven.com/a/brown_mud_dry), native 3-by-3 tiles
  on the unscaled one-metre plane, and a distant view. Reference lighting differs
  and its pixels were not imported into the pipeline.
- Normal-zero/restored mean RGB differences are 6.18/5.24/4.15 over 421,400 pixels.
  An initial indirect-only AO control was too dark to judge. A brighter skylight
  control kept generated cavity values unchanged: AO-zero/one mean differences
  are 1.00/1.06/1.25, versus 0.67/0.70/0.88 for restored baseline variation.
  Visible AO quality remains INCONCLUSIVE. This follows
  [Epic's indirect-lighting guidance](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-global-illumination-and-reflections-in-unreal-engine),
  not an arbitrary increase of generated AO. No project render setting changed.
- Visual verdict remains PATCH: the distribution is less elongated and shows more
  patch variation, but rounded relief is still too regular and granular breakup
  remains weaker than the reference. Q2a stays open. Moving-camera shimmer, broad
  mip quality, compact-map comparison, actual GPU residency/cost and production
  adoption remain unverified. Logical residency and native consumer sample count
  are unchanged; no 16.67 ms performance PASS is claimed.
- World/canonical files and production resources remain protected; no World
  regeneration, canonical compilation, Matrix, cook or release ran. The operator's
  staged baseline is retained; fixes and documentation are unstaged. Editors were
  terminated only after explicit operator authorization, then a single offscreen
  fixture Editor resumed without a native window handle.

### 2026-10-09 - Native soil units and isolated PBR sample

- D15 continued the source-first prototype. ProjectTexture owns one closed soil
  height/clump/grain kernel plus same-height normal and packed cavity/roughness/
  pigment outputs. ProjectMaterial owns the stock opaque Substrate soil sample.
  There is no new generator framework, custom HLSL, Python texture compiler,
  texture import, persistent live writer or World consumer change.
- Authored fixtures live under each resource owner's `Data/TestFixtures/Soil/`;
  generated packages/manifests and captures live only under
  `tmp/world/surface_presentation/soil_native/`, `tmp/texture/generation/soil_*`
  and `tmp/material/generation/soil_*`. Production recipe censuses exclude them.
- Three real 1024-square linear RGBA8 outputs have independent normal, roughness
  and cavity controls. Alpha remains unused; no PBRA convention or two-texture
  optimum is claimed. Dry-soil dielectric F0 is constant 0.04. Palette, metres,
  centimetres and response weights stay with ProjectMaterial. The native sphere
  demonstrates tangent-basis behavior; only the unscaled 100 cm plane proves
  the fixture's 1 UV unit = 1 m assumption.
- Generated maps exposed interior stripes despite structural tests passing.
  Installed `Random.ush` and a native isolated 12/16-repeat control localized
  non-power-of-two wrap/hash discontinuities: maximum/median column-step ratios
  33.65 versus 1.62. The closed kernel uses power-of-two periods and full native
  Voronoi jitter. Actual height maximum mean row/column step dropped from an
  observed 10.13 / 255 column discontinuity to 5.69 / 255 over both axes, below
  the focused 6 / 255 fixture limit. This is a stripe regression, not proof of
  seam-free appearance at every mip, seed or scale.
- Native readback initially crashed when shader compilation collected unrooted
  control objects; strong ownership corrected the test. A second RED came from
  treating UE's sRGB-decoded RGBA8 float readback as raw normal data. Native byte
  readback corrected that assertion. Flat-height and known U/V ramps now verify
  encoded normals and cavity numerically against actual generated pixels.
- Independent architect recheck found partial readiness after recreating one RHI
  resource. The all-sibling regression failed exactly twice, then passed after
  warm reuse and `IsOutputReady` shared complete-bundle resource checks. All
  three dependent maps regenerate after replacement. Recheck returned PASS for
  this bounded frozen-bundle/MID boundary; that review is closed.
- Exact native targets passed: `Project.Texture.Soil.NormalControls`,
  `Project.Texture.Runtime.TwoNodeGeneration`,
  `Project.Material.Soil.AuthenticatedSample`,
  `Project.Texture.Generation.PatternRecipeContract`,
  `Project.Texture.Generation.OutputContractClosed`,
  `Project.Texture.Generation.PatternManifestContract`,
  `Project.Material.Generation.SurfaceRecipeContract`,
  `Project.Material.Generation.SurfaceRecipeKnownBad`, and
  `Project.Material.Generation.SurfaceManifestContract`. The soil integration
  authenticates exact package bytes, saves/reloads, skips unchanged generation,
  refuses malformed/budget inputs, and rejects dirty sources/disconnected Slab
  inputs. Native RED/GREEN and terminal receipts are under `receipts/`.
  Both recipe schemas validate all eight current production/soil fixture recipes.
- The soil integration also passed with its visual receipt directory absent:
  it creates that disposable output directory itself. A final incremental build
  and fresh-Editor exact test verified this setup fix; a settled native MCP
  capture re-established the same isolated sample afterwards.
- Actual native MCP images show neutral, grazing and reversed light, zero-normal
  control, restored normal, and roughness controls. `soil-lighting-board.png` and
  `soil-map-board.png` show the result; `captures/final-grazing-high.png` is a real
  1600 x 1200 UE screenshot. Zero/restored normal changes mean RGB by
  4.91/4.16/3.35 over 255,037 pixels; restored temporal differences are smaller.
  Roughness controls also visibly differ. AO-zero/AO-one differences are only
  0.21/0.19/0.16, smaller than temporal variation: visible AO contribution is
  INCONCLUSIVE here. Do not certify AO quality from its valid connection/map.
- Native maps reconstructed pixel-identically across the final compiler/schema
  identity change; PNG encoding bytes alone differed. Final texture/material
  manifests and clean content state are captured in
  `receipts/final-fixture-identity.json`. The diagnostic template map stays dirty
  in memory and was never saved. The final offscreen Editor has no native window
  handle; parameter/light controls reuse it without restart. Structural C++ edits
  required incremental builds and owned Editor reloads. Two shutdowns exposed an
  installed EaseCurveTool async-load-after-shutdown failure; source/log evidence
  names that unrelated engine path. Clean Editor shutdown is not certified.
- Protected comparison covers 10,784 baseline files: 10,780 are byte-identical;
  the four changed files are the declared resource schemas. Three added files are
  authored soil fixtures. All 6,654 World/canonical files, existing production
  resource recipes/manifests/packages, HEAD and staged content remain unchanged.
  No World Apply, canonical compile, Matrix, cook, release, staging or commit ran.
- This delivers an isolated working candidate, not realistic-soil acceptance.
  Compared with [the inspected dry-soil reference](https://polyhaven.com/a/brown_mud_dry),
  pale/clump organization remains too regular and grain breakup needs refinement.
  Q2a stays open and blocks landscape application. Compact-vs-independent-mask
  comparison, 3x3/far/mip/motion quality, production distortion policy and GPU cost
  remain open. Logical soil residency is 16,777,212 bytes; with the unchanged
  terrain fixture it is 18,175,312. No 16.67 ms performance PASS is inferred.
  Soil tuning used unsaved native MID diagnostics on JSON-compiled fixture assets;
  the terrain-only T0a command does not yet prove soil JSON hot-update support.



### 2026-10-09 - Reviewer follow-up: fixture policy lifetime and PBR packing

- Accepted the separate normal, independent roughness/cavity comparison, constant
  soil dielectric response and sample-before-landscape direction. No texture count
  is an accepted limit. Shared height need not be exported as a runtime sample.
- Confirmed the test cleanup defect in source: a clean policy is removed at entry
  but the test's final re-registration is left installed. The old exact native test
  reported Success while a live probe still found the fixture rule. That probe alone
  did not prove an entry-state delta when the rule was already present.
- Refuted the sufficiency of immediate scoped removal with an actual RED control:
  the exact fixture test failed native reference validation after removing its rule
  before next-tick save validation. Installed `UEditorValidatorSubsystem::ValidateSavedPackage`
  and `FTimerManager` explain the ordering. Do not disable or bypass these checks.
- The scoped exit now queues native latent cleanup after the queued validation.
  It checks original settings restoration, including an existing owned rule, and
  tests normal/early registration exits. The replacement incremental build passed;
  only the test translation unit and its module links rebuilt.
- Final exact `Project.Material.Preview.AuthenticatedTerrainDetailFixture` runs
  passed in clean-entry and already-registered envelopes. Native assertions export
  the full additional-rule structure. External snapshots confirm 0 -> 0 and 1 -> 1
  fixture-rule counts, unchanged restriction switches/visible domains and no dirty
  packages. Receipts are under `tmp/world/surface_presentation/fixture_policy_cleanup/receipts/`.
- Alpha/compression warnings do not prove extra residency cost for the existing
  RGBA8 targets; compare actual formats. Four-channel write correctness remains
  unproved, as do separate soil outputs, realistic soil quality and GPU cost.
- No new policy manager or texture pipeline; no World/canonical regeneration,
  Matrix, cook or release. The Git index remains operator-owned and unchanged.

### 2026-10-08 - T1c closed detail experiment and fixture safety proof

- **2026-10-09 follow-up:** A final native rerun exposed an intermittent custom
  scratch-domain validation failure after the first architect PASS. That PASS
  establishes the collision repair, not reliable native validation by itself.
  `/Temp/` was rejected because the registry skips its synchronous scans.
  `/Extra/ProjectMaterialTest/` was also rejected: an exact cold-Editor test logged
  invalid mount points and discarded dependency data. The dedicated
  `/ProjectMaterialTest/` mount is retained with a strict native domain admitting
  only self/Engine and ProjectTexture; production cannot reference it. The rule is
  removed before UObject teardown, and unchanged existing policy is tested.
  The native settings object is config-backed; this isolated route does not save
  it and must not be used alongside another policy-settings writer. The strengthened
  fixture gathers actual saved dependencies and an unloaded referencer. The
  terminal results and focused review recheck below establish this boundary.
  A non-fatal mount probe replaces an asserting path conversion found by the cold
  refusal test. Its crash required one recovery restart; the new native dependency
  also required a structural reload. Neither is an ordinary parameter iteration.
- **Follow-up verification:** All nine exact native tests passed again in the
  recovered Editor, including automatic native validation of both fixture assets.
  The exact fresh-process host unloaded-referencer test passed. It now removes
  the original instance and orphans its parent: the unloaded copied package retains
  the parent and original-instance dependencies while the unreferenced terrain
  instance is deleted. Early engine-exit cleanup prevents CDO recreation after
  UObject teardown. Removal preserves existing native policy and switches; all
  21 monitored configuration files remain byte-identical. Independent architect
  recheck returned PASS for this fixture boundary; this review loop is closed.
  Receipts: `host-unloaded-referencer-final.xml`, `policy-config-final.json` and
  the fresh `test-*.json` files under the T1c receipt directory below.
- **Reference comparison and live refinement:** Inspected Epic's detail/bump and
  Kite surface references and actual ALIS pixels. Added three disposable soil
  parameter candidates using the same authenticated current fixture, with zero
  new generated assets or graph compiles during tuning. The retained board is
  `tmp/world/surface_presentation/t1c/captures/soil-comparison-board.png`:
  production on the left, transient soil on the right, near/distant in both cities.
  Twelve baseline/candidate/revert captures use exact cameras plus consecutive
  settled-render controls; the original images still contain temporal noise.
  Earlier comparisons are historical: one Manhattan pair was rejected because
  buildings appeared after camera movement despite matching component receipts.
  The new renders supersede it. All three fixture files retain hashes and times,
  original bindings and Kazan scene identity are restored, and no packages are dirty.
  `soil-proof.json` records the controls and bounded pixel deltas after revert.
  The soil palette reduces exaggerated green/brown contrast but remains too
  uniform for visual adoption. Native LQ bump is a controlled prototype, not the
  final quality solution: Epic documents derivative block artifacts and filtering
  needs. Next T1 work must compare richer artistic fine structure and a filtered
  normal path through the existing owners, while measuring cost; Q2 stays open.
- Added `terrain_detail_metric` through the existing ProjectMaterial parser,
  schema, builder, identity and verification path. It adds independent fine scale,
  fine brightness and bounded shading height, using the same accepted ProjectTexture
  resource and Epic's installed `PerturbNormalLQ`. Geometry, collision and canonical
  channels remain unchanged. The ordinary production preview still admits only
  authenticated instance parameters; an explicit fixture command admits the trial.
- Disposable generation compiled one parent and saved/reloaded two fixture assets.
  JSON tuning then changed only transient renderer bindings in the same offscreen
  Editor. The final fixture's two package hashes, manifest hash and all three
  timestamps stayed unchanged throughout the recorded subsequent live iterations.
- Independent architect review returned PATCH for a real cached-package hazard:
  two test roots with identical IDs could authenticate one root's disk bytes while
  reusing another root's loaded UObjects. Both regression controls failed before
  the fix. Generation now checks every requested loaded origin before retargeting
  the mount; fixture preview checks loaded instance and parent origins. Dirty,
  unknown and mismatched origins refuse. The focused recheck returned PASS.
  Final source inspection also confined direct requests to an output directory
  ending in `Surfaces`; otherwise mounting its parent could write outside the
  admitted output root. The malformed-root control checks refusal before mount
  changes or generation. This stays inside the same disposable compiler boundary.
- All nine exact tests returned terminal Success: `TerrainDetailRecipe`,
  `AuthenticatedTerrainDetailFixture`, `LiveRecipeRevert`, `RefusalPreservesLiveState`,
  `SurfaceRecipeContract`, `SurfaceRecipeKnownBad`, `SurfaceAssetCompilerContract`,
  `SurfaceIdentityFirewall`, and `Project.World.MeshTerrain.MaterialDependencyIsPathOnly`.
  The material names use their existing `Project.Material.Generation` or
  `Project.Material.Preview` namespaces. Receipts retain the full paths under
  `tmp/world/surface_presentation/t1c/receipts/test-*.json`.
- The strengthened fixture checks cross-root refusal, unchanged mount/object
  identities/graph/dirty flags/bytes/binding, corrupt JSON/manifest/package refusal,
  copied native overrides, save interruption/revert and disconnected-normal refusal.
  Package corruption targets an unloaded disposable copy: the Editor holds loaded
  package files open on Windows. Failed compile attempts and invalid controls are
  not counted as passing evidence.
- Final matched near/distant groups in both cities contain 16 native captures.
  Camera/FOV equality is checked per group; close Kazan normal controls also have
  real 1280 x 720 captures. Early Manhattan captures are superseded because its
  preview section appeared asynchronously after map load. The retained `settled-*`
  Manhattan captures start after two matching identity observations; 750 actors,
  850 mesh components and the component/mesh digest remain equal after revert.
  Kazan returns to its original aerial camera, 871 actors, 975 mesh components,
  original component/mesh digest and production binding. Dirty packages are empty.
- Shader statistics are VS 153 / PS 260 / 5 pixel texture samples for the accepted
  graph versus VS 153 / PS 307 / 8 pixel texture samples for the trial. Both use
  three samplers. These are Editor shader statistics, not GPU time, packaged
  runtime evidence or certification against 16.67 ms; performance is INCONCLUSIVE.
- Inspected the actual final board and close images. The palette reduces broad
  blotches and the normal adds modest close relief; the ground still reads as
  uniform procedural noise and distant normal benefit is small. T1 and Q2 remain
  open. Motion shimmer, unshadowed Manhattan street-height detail and richer fine
  structure still need evidence. The trial is not an accepted production direction.
- The 8,094-file inventory has exactly two intended authored changes: ProjectMaterial's
  recipe and manifest schemas. The other 8,092 files are unchanged, including all
  protected World, canonical, ProjectTexture and production material recipes,
  manifests and assets. No production regeneration, World Apply, canonical compile,
  Matrix, cook, staging, commit or release was performed. Ordinary parameter
  iteration stayed live; the follow-up recovery/structural reloads are recorded above.
  The compiler source fingerprint changed; accepted scratch fixtures were generated
  through the owner against that candidate and held unchanged during parameter
  iteration. This does not authorize refreshing production artifacts.
- Final parser/schema, camera, identity, fixture and protected-inventory receipts
  live under `tmp/world/surface_presentation/t1c/receipts/`. The public manifest
  schema retains its production mount restriction; the test manifest's structure
  is checked through a production-path projection and its actual test identities
  are authenticated by the native fixture authority. No test mount was added to
  the production schema.

### 2026-10-08 - T1b matched views and reference-driven quality correction

- D11 records the operator's rejection of a scale-selection question as the next
  milestone. No candidate is visually accepted; T1 and Q2 stay open.
- Eight fixed-camera groups cover near, distant, slope and aerial views in Kazan
  and Manhattan. Each has baseline, 16 m, 8 m, 4 m and restored native captures:
  40 images, with exact camera/FOV equality checked in
  `tmp/world/surface_presentation/t1b/receipts/verification.json`.
  These native captures are 589 x 433; their comparison boards are diagnostic,
  not adequate evidence of fine ground detail or optimized performance.
- The earlier T1 Manhattan wider comparison is superseded: building population
  differed between its images. T1b uses settled preview/building population; that
  earlier pair must not support a quality decision. Obstructed/shadowed steep
  Manhattan scouts were also discarded. The selected terrain collision slopes
  are 16.86 degrees in Kazan and 13.30 degrees in Manhattan; these locate views,
  not global slope or geometry correctness measurements.
- Additional Kazan captures place the camera 180 cm above the selected preview
  collision height at the same XY. Matched baseline, muted 8 m, muted 1 m and
  restored views include actual 1280 x 720 `HighResShot` outputs under T1b
  `captures/kazan-ground-*-high.png`. The palette reduces yellow/brown contrast,
  but the surface still reads as flat color noise. Neither candidate is accepted.
  Camera equality is authenticated by the accompanying native capture receipts;
  high-resolution screenshots share that stationary camera but a different aspect.
- Inspected primary-source reference images, not only search snippets:
  [Epic's Kite assets](https://www.unrealengine.com/blog/imperfection-for-perfection?lang=en-US)
  show natural surface detail; [Epic's detail-texturing example](https://dev.epicgames.com/documentation/en-us/unreal-engine/adding-detail-textures?application_version=4.27)
  demonstrates diffuse and normal detail at close range.
  [DICE's terrain paper](https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/chapter5-andersson-terrain-rendering-in-frostbite.pdf)
  distinguishes low-frequency variation from tiled fine detail. These are quality
  and technique references, not ALIS performance benchmarks or authorization to
  import packs, add foliage, switch to Landscape, or change geometry.
- [Epic's bump documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/bump-mapping-without-tangent-space-in-unreal-engine)
  warns about derivative artifacts, procedural cost and distance aliasing. Native
  `PreparePerturbNormalHQ`, `PerturbNormalHQ`, `ComputeFilterWidth` and
  `NormalFromHeightmap` loaded in the installed Editor; their real inputs/outputs
  are recorded in `receipts/native-detail-functions.json`. Availability alone
  does not prove compatibility, quality or cost on the Mesh Terrain/Substrate graph.
- All trials used the same offscreen Editor process. Preview revert restored the
  original native binding and values; the final dirty package list is empty.
  Protected inventory verification was completed in T1c with the two authored
  schema changes listed explicitly;
  no World Apply, canonical compilation, material generation, Matrix, cook,
  release, staging or commit was invoked for these parameter/capture trials.
  The 16.67 ms target and explicit INCONCLUSIVE policy remain unchanged.

### 2026-10-08 - first local terrain candidates after T0a PASS

- Continued D10's approved local prototyping without another architectural review.
  Two candidate recipe copies use existing terrain parameters: 16 m and 4 m scales
  with the same muted palette. No graph, schema, pattern, accepted recipe, profile,
  production material package or generated reference changed.
- Inspected near and wider baseline/candidate captures on both Kazan and Manhattan
  through the same native Editor. The smaller scales introduce finer cover breakup
  and the palette reduces the broad yellow/brown treatment. This is a preliminary
  appearance judgment, not operator acceptance or a performance certification.
  Evidence lives under `tmp/world/surface_presentation/t1/captures/`; candidate
  copies live under that run's `candidates/compact/` and `candidates/fine/` roots.
- Initial captures immediately after a camera/map change were unsuitable: native
  rendering/preview initialization had not settled, and an early binding was replaced.
  Those trials were discarded. Fresh baseline/candidate captures used the initialized
  renderer; native captures and source identities remain the authority.
- Reverted candidates, returned to the original Kazan map/camera, and left the same
  offscreen Editor running with no dirty packages. T1 remains open for the visual
  decision and further controlled evaluation; Q2 is not closed by this experiment.
  The existing 16.67 ms target and INCONCLUSIVE policy are unchanged.
- T0b evaluation confirms only the existing isolated compiler save/reload/no-op
  route. A persistent live update still needs loaded-reference, single-writer and
  injected recovery proof before production use. The external host's Editor exclusion
  and the production commandlet guard were not weakened.
  Source inspection confirms that changed outputs are deleted before rebuilding,
  and saved packages are unloaded and reloaded. That lifecycle is not an in-place
  update of retained consumer references. The next isolated persistence proof must
  exercise those references and recovery through the existing material owner;
  passing the current save/reload fixture cannot close that boundary.

### 2026-10-08 - bounded T0a implementation and runtime evidence

- D10 authorizes this boundary. ProjectMaterialEditor now owns the explicit
  `project.material.preview` recipe operation and revert; no watcher or second
  material compiler is introduced. Persistent generation and production adoption
  remain separate gates. Independent completed-boundary architect review returned
  PASS with no required PATCH findings. That review is closed for transient T0a.
- The exact `Project.Material.Preview.LiveRecipeRevert` test first failed with
  the missing operation, then passed after implementation. The exact
  `Project.Material.Preview.RefusalPreservesLiveState` test passed for malformed
  JSON, unsaved baseline refusal, native MID override preservation, pre-save
  interruption, and external binding conflict. Tests run through console MCP
  dispatch, with terminal AutomationController results read from the live log.
- A disposable inherited-slot experiment reproduced an override count change
  from 0 to 1 after an apparently successful revert. The correction preserves
  inherited versus explicit slot representation; the first test now asserts
  zero added overrides and passed against the compiled correction.
- The same offscreen Editor PID stayed alive through Live Coding, JSON updates
  and revert. The first GroundColor experiment updated the parameter but not
  cold Editor pixels. Existing ProjectTexture initialized its two-node runtime
  pattern in a focused native simulation session; its Ready event preceded the
  matched Editor control/candidate captures. No compiler/World generation was used
  to initialize those disposable runtime pixels.
- Native captures at one fixed Kazan camera visibly demonstrate GroundColor
  changing to diagnostic pink, refusal retaining the last good image, and revert
  restoring the original appearance and binding. These values are not an accepted
  palette. Captures/receipts live under `tmp/world/surface_presentation/t0a/`.
  RGB mean absolute candidate deltas were 27.14/9.23/19.07 over 255,037 pixels;
  restored deltas were 1.02/0.90/0.78. Restoration is not pixel-bit-identical because
  the renderer is temporal. Binding identity and dirty package state were restored
  exactly; the final dirty package list is empty.
- All 8,094 protected World/Material/Texture Data/Content and materialized canonical
  files were byte-identical in `receipts/protected-comparison.json`. No World Apply,
  canonical compilation, Matrix, cook, release, staging or commit ran. Live Coding
  also patched previously changed World binaries; this did not rewrite generated
  World packages or execute the World pipeline.
- T0a is a manually triggered single-recipe loop. Automatic detection, persistent
  in-editor saving, matched Manhattan quality and operator visual adoption are
  not established by this result. T0b records the separate persistence boundary.
- Existing exact `Project.Material.Generation.PatternAuthority`,
  `Project.Material.Generation.SurfaceAssetCompilerContract`, and
  `Project.World.MeshTerrain.MaterialDependencyIsPathOnly` also passed in this
  Editor. The compiler test generated/saved/reloaded four isolated fixture assets,
  then skipped all four without package/manifest timestamp changes. This supports
  the existing test-mount route, not persistent live-reference updates, concurrent
  writer safety or failure rollback. Production Editor exclusion remains intact.

### 2026-10-08 - investigation and release-scope correction

- Source, schema, existing tests, stable contracts, and retained consumer research
  inspected. No runtime preview or shader/performance acceptance executed.
- D1-D6 recorded. The pasted publish-first recommendation has no operator authority.
- Q1 and Q2 remain explicit implementation/visual gates. This file is not approval.
- Independent architect R1 initially returned PATCH for historical release wording
  and the obsolete water-material retirement lifecycle; both were corrected.
- Independent architect recheck returned PASS for this planning boundary and the
  two companion documents. Implementation and visual-adoption gates remain open.

### 2026-10-08 - alignment feedback handoff clarified

- D7 recorded. Added material-independent geometry/depth classification to T0 and
  exclusions; shader tuning cannot certify or conceal a geometric defect.
- Geometrically correct shoreline appearance stays here; road/terrain sampling,
  occlusion, and topology remain in the separate investigation. No defaults, recipes,
  generated references, geometry, or runtime state changed by this documentation audit.
- Independent architect re-review returned PASS for the three-document planning
  scope, including this handoff. Implementation and visual gates remain open.

### 2026-10-08 - bounded alignment investigation timing exception

- D8 records the operator's read-only investigation request. Material implementation
  remains gated by Q1; no material algorithm or performance optimization is added
  merely because an external production used a different projection strategy.

### 2026-10-08 - native MCP and live-instance integration investigation

- D9 recorded. Installed launcher engine reports 5.8.3; native ModelContextProtocol
  and AllToolsets activation already exists. A real native HTTP initialize/discovery
  and viewport capture succeeded on the existing Kazan map; listener is loopback-only.
- Native material-parameter discovery succeeded. This proves tooling availability,
  not JSON-driven preview, runtime pattern readiness, persistent saving or recovery.
- Agent offscreen startup through the existing launcher succeeded with normal RHI,
  a zero native window handle and native viewport capture. The initial visible process
  was cleanly closed only to correct startup mode; no material iteration required restart.
- Native HTTP was exercised directly over localhost because this agent session did
  not expose the official meta-tools. Python preview setup/restoration used the existing
  Blueprint MCP console route. Do not claim the session's native connector attachment
  or JSON-to-material integration from this combined diagnostic route.
- One transient `SteepGroundColor` probe on the actual Editor preview produced visibly
  different pixels, then the original binding/appearance was restored with no dirty
  packages or asset saves. Red is a diagnostic signal, not an accepted palette.
  Evidence is under `tmp/world/material_preview/`: captures
  `rpc-011-CaptureViewport.0.png`, `rpc-018-CaptureViewport.0.png` and
  `rpc-019-CaptureViewport.0.png`; `transient-restored.json` records binding restoration.
- `world-comparison.json` records all 6,592 World Data/Content files byte-identical.
  No canonical or ProjectWorld pipeline command ran. Normal Editor startup/cache
  work is not a fresh canonical compilation or a generated-package promotion.
- A3/T0a prioritize the one-recipe proof. The Definition watcher offers regeneration;
  the material builder supports in-place instance updates, while its production service
  independently requires commandlet ownership. Preserving that guard is mandatory.
- No recipe target or persistent material authority is accepted by this investigation.
  Q1/Q2 remain open; no canonical compile, World Apply, cook, Matrix or R5 was run.
