# ProjectTexture runtime pattern DAG and terrain cache proof

**Status:** DONE - positive result, merged into the Terrain Material v2 task, which owns every remaining item; the direct-vs-cache frame delta stays unverified

**Scope:** ProjectTexture runtime structural-pattern generation and bounded GPU reuse,
ProjectMaterial consumption, and one production Mesh Terrain comparison

**Stable documentation owner:**
[ProjectTexture README](../../../Plugins/Resources/ProjectTexture/README.md),
[pattern generation](../../../Plugins/Resources/ProjectTexture/docs/pattern_generation.md), and
[surface generation](../../../Plugins/Resources/ProjectMaterial/docs/material_generation.md)

## Contents

- Goal
- Authority register
- Non-goals
- Verified evidence
- Current architecture and source of truth
- Problem and root cause
- Decision
- Required invariants
- Implementation tasks
- Test-first and verification plan
- Documentation plan
- Rollout and rollback
- Completion criteria
- Review record

## Goal

After the current universal material task reaches PASS, build and prove the first
production-looking, fully generated terrain appearance through the smallest reusable
runtime texture foundation that can reduce repeated structural shader work without
moving authority into generated pixels or multiplying state by terrain section, cell,
city, object, color, or material instance.

The intended flow is:

```text
ProjectTexture recipe authority
        -> deterministic structural node graph
        -> bounded runtime generation and reuse
        -> ProjectTexture-owned sampled output seam
        -> ProjectMaterial appearance and semantic composition
        -> production Mesh Terrain material
```

Terrain is the first production comparison, not the owner of the cache. Direct
evaluation was the equivalent-quality control; the bounded native cache/projection
won the product comparison and is now the sole production arm.

## Current state and next steps

ProjectTexture pattern schema v3 / compiler v5 owns an authenticated two-node native DAG, one stable sampled
Structure output, and the runtime cache lifecycle. The selected 512x512 RGBA8 output
has a complete mip chain, Wrap addressing, 1,398,100 resident bytes, and a measured
two-node generation peak of 2,796,200 bytes. One writer, explicit
generating/ready/failed state, pins, pre-allocation budget refusal, exact accounting,
and unpinned LRU eviction keep resource ownership bounded. Requests from 1, 10, and
100 consumers converge on one ready structural identity.

ProjectMaterial consumes only the stable pattern ID and sampled output. Terrain uses
`world_metric_triplanar_2d`; the generated movable cube uses
`object_metric_triplanar_2d`. The current `surface_structure_rgb_v1` ABI is
`R=MacroVariation`, `G=CoverPatch`, `B=GroundDetail`; alpha is undefined and unused.
The basis combines medium and high-frequency structure into RGB before the child derives
the exported output. The runtime GPU regression proves all three declared channels vary
and remain pairwise distinguishable at mip 0 and mip 4. ProjectWorld has no pattern
compiler, cache call, per-section MID, or runtime texture ownership.

The temporary equivalent direct arm was packaged and captured, then removed. The
cached Development package was 4,836,032 bytes smaller, distant structure was smoother
through real mips, and the terrain no longer evaluates four procedural noises per
visible pixel. The official performance route was run against both arms with identical
settings, but UE 5.8 crashed before sampling both runs with the same CSV-profiler
assertion. Therefore frame/GPU timing is unverified; this common instrument failure does
not justify production material workarounds.

The pre-correction Shipping route passed 19 exact tests, but the new per-channel
regression correctly fails `DetailHeight`; that package is no longer final acceptance.
`M_ProjectTerrain` and `M_ProjectMetricSurface` cooked for SM5 and SM6; archive and IoStore checks passed;
the 2,063,109,020-byte package passed Kazan, Manhattan, ground-detail, and
moved/rotated-cube fixed views. The Manhattan frame is only a terrain-presence sanity
view. The ground-detail frame proves stable metric projection and broad structure, not
final photoreal microdetail. A post-RGB close-ground Development frame remains required
before current appearance acceptance. Independent R2 and repair or replacement of the
shared timing instrument also remain before formal PASS.

## Authority register

### Operator decisions

- **D1** Investigate a proper texture foundation now and continue the terrain material
  through that foundation.
  - Effect: ProjectTexture runtime structure and the production terrain consumer are
    one evidence chain; an isolated texture demo alone cannot close the task.
  - Reason: "we need now investigate proper texture foundation and move on with
    landscape material with it."
  - Date/source: 2026-09-28, current request.
- **D2** The material/texture system must be universal, flexible, black-boxed,
  decoupled, and scalable rather than terrain-specific.
  - Effect: ProjectTexture owns reusable structure and runtime resource lifecycle;
    ProjectMaterial owns appearance; ProjectWorld and Mesh Terrain do not acquire
    pattern generation or cache ownership.
  - Reason: future world rebuilding must not create another tightly coupled pipeline.
  - Date/source: 2026-09-27 and 2026-09-28 operator direction.
- **D3** Minimize physical asset multiplication and prefer bounded runtime calculation
  and reuse when it wins the measured total system.
  - Effect: generated pixels are runtime-only; small descriptor/kernel assets may
    scale with exported structural patterns, but outputs may not scale with consumers
    or geography. Moving unlimited bytes from disk to VRAM is not a win.
  - Reason: one city must not grow toward terabytes of repeated content.
  - Date/source: 2026-09-27 operator direction and the current material task D17.
- **D4** Epic Experimental/Beta technology is admissible when it is a credible future
  path with a present measured advantage and a real packaged proof.
  - Effect: labels do not reject a candidate, but public API shape, marketing, or a
    Runtime module label does not promote it either.
  - Reason: use next-generation engine technology where it materially advances ALIS.
  - Date/source: 2026-09-27 operator direction and the current material task D16.
- **D5** Keep this change KISS.
  - Effect: prove generation, shared terrain consumption, and a measured win in that
    order. Do not build a general cache, new loading framework, custom virtual texture,
    or alternate renderer before its preceding seam passes.
  - Reason: explicit operator direction during the material architecture review.
  - Date/source: 2026-09-27.

### Operator gates

- **Q1 [CLOSED - PASS]:** one stable ProjectTexture-owned output is sampled by every
  compiled Mesh Terrain section in packaged ALIS with no per-section MID, ProjectWorld
  dependency, or new renderer path.
- **Q2 [CLOSED for production promotion, timing caveat retained]:** the honest
  triplanar cached path preserves production terrain quality, improves distant
  filtering, reduces the paired Development package by 4,836,032 bytes, bounds runtime
  memory independently of consumer/world population, and removes four repeated noises
  per visible terrain pixel. Frame/GPU timing remains unverified because the official
  collector crashes identically for both arms before sampling; no timing win is claimed.

### Working assumptions

- **A1 [RESOLVED]:** This work ran inside the Terrain Material v2 task, which remains the single
  owner of the initiative's open items.
- **A2 [ACTIVE]:** The first shared-binding candidate is one ProjectTexture-owned
  cooked `UTextureRenderTarget2D` output-slot descriptor for the exported terrain
  pattern. Its package stores configuration and identity, not generated pixels; the
  runtime fills the referenced resource before terrain is shown. Intermediate DAG
  nodes remain transient and pooled.
- **A3 [ACTIVE]:** The first fixture uses the smallest linear format and resolution that
  can discriminate seams, mip behavior, reuse, and quality. No 2048 or 4K default is
  assumed.
- **A4 [ACTIVE]:** Prewarm begins inside the ProjectTexture runtime lifecycle. A generic
  ProjectLoading contribution seam is considered only if the normal packaged route
  proves that ProjectTexture cannot become ready before the first terrain frame.
- **A5 [ACTIVE]:** If the cached output changes the current
  `world_metric_3d`/`object_metric_3d` meaning, it is a new projection adapter and must
  be named and compared honestly; it cannot silently claim parity with direct 3D
  evaluation.
- **A6 [ACTIVE]:** A first failed candidate does not close the task. A shared-slot
  lifecycle failure receives one exact native correction; a projection/quality failure
  receives one deliberate production refinement. If the bounded replacement still
  fails Q1/Q2, the candidate is removed rather than retained as unused production
  architecture. The accepted direct graph remains production and the task closes with
  the negative evidence.

## Non-goals

- UE Landscape compatibility or City17 material repair. Production Kazan and Manhattan
  use Mesh Terrain; "landscape material" in D1 means the terrain appearance domain.
- CanonicalCompilation semantics, terrain channels, the shared MPD, World geography,
  or Mesh Terrain topology changes.
- Per-section MIDs, per-cell textures, per-city atlases, or World-owned cache calls.
- Runtime Texture Graph on UE 5.8.3.
- RVT, Mesh Partition Material Cache, a custom `IVirtualTexture` producer, RDG/compute
  generation, custom block compression, or a disk cache.
- Nanite Assemblies, GPU PCG, FastGeo, object generation, HLOD, or general geometry
  cardinality policy. Those are unrelated owners and were scope noise in the supplied
  proposal.
- A general material-layer system or migration of Water, roads, buildings, vegetation,
  decals, wood, metal, or City17 content.
- Keeping schema v2 compatibility after an accepted DAG cutover. ALIS uses a one-pass
  current-only public architecture.

## Verified evidence

### Verified facts

- **E1 - Current ownership:**
  [ProjectTexture README](../../../Plugins/Resources/ProjectTexture/README.md) owns
  reusable structural patterns. ProjectMaterial owns tint, roughness, wetness, metric
  scale, projection policy, and semantic composition. Runtime renderer pages are
  disposable; recipes and accepted manifests remain authority.
- **E2 - Runtime capability gap:** `ProjectTexture.uplugin` already contains a Runtime
  module, but `Source/ProjectTexture` currently has only module startup/shutdown code.
  There is no runtime recipe catalog, request API, resource owner, cache, budget,
  readiness state, or telemetry.
- **E3 - Current recipe shape:** `pattern-recipe.schema.json` v2 describes one flat
  `terrain_structure_native` function with seed and three scale ratios. The accepted
  manifest resolves `project_terrain_structure` to
  `/ProjectTexture/Patterns/Terrain/MF_ProjectTerrainStructure` and authenticates its
  package. No parent graph or runtime output layout exists.
- **E4 - Current dimensional contract:**
  [pattern generation](../../../Plugins/Resources/ProjectTexture/docs/pattern_generation.md)
  requires a three-component `PositionMeters` input and publishes three normalized
  signals. `ProjectTexturePatternBuilder.cpp::BuildSignal` evaluates native 3D noise
  with `bTiling=false`. The current function is continuous direct 3D evaluation, not a
  seamless 2D tile.
- **E5 - Current production policy:**
  [surface generation](../../../Plugins/Resources/ProjectMaterial/docs/material_generation.md)
  selects direct evaluation and defines `world_metric_3d` and `object_metric_3d` as
  direct metric 3D coordinates, not triplanar projection. Runtime cache promotion
  requires a measured win.
- **E6 - Native render-target path:** installed UE 5.8.3
  `UTextureRenderTarget2D` is a regular `UTexture`/`MCT_Texture2D`, exposes runtime
  size/format/addressing, and supports `bAutoGenerateMips`. Its `Serialize` override
  serializes object/version properties only; GPU pixels live in
  `FTextureRenderTarget2DResource`. Epic API:
  https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UTextureRenderTarget2D
- **E7 - Native one-shot generation:** installed UE 5.8.3
  `UKismetRenderingLibrary::DrawMaterialToRenderTarget` is a Runtime Engine API. It
  renders a material quad and calls `UpdateResourceImmediate(false)` so automatic mips
  are generated. Epic warns that repeated draws to one target should use the Canvas
  begin/end route instead:
  https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UKismetRenderingLibrary/DrawMaterialToRenderTarget
- **E8 - Ordinary dynamic binding exists but is consumer-local:**
  `UMaterialInstanceDynamic::SetTextureParameterValue` can bind a runtime texture to a
  normal primitive MID. This is sufficient for an isolated cube fixture, not by itself
  for production Mesh Terrain.
- **E9 - Mesh Terrain material realization:** installed UE 5.8.3
  `MeshPartitionEditorComponent.cpp:547-549` creates one
  `UMaterialInstanceConstant` inside each compiled-section package and parents it to
  the MPD material. `MeshPartitionCompiledSection.cpp:419-427` writes its channel
  texture with the Editor-only MIC setter. Generated static meshes retain that
  material interface. A normal runtime texture parameter therefore has no single
  shared terrain MID to update.
- **E10 - Stable render-target descriptor candidate:** a material may reference one
  cooked `UTextureRenderTarget2D` asset directly, while runtime code draws into the
  same loaded object. Source E6 shows that this packages descriptor properties rather
  than generated pixel source data. This removes the per-section binding problem if a
  packaged proof confirms initialization, ordering, mip generation, and package
  stability.
- **E11 - Material Parameter Collections are not a texture route:** installed UE 5.8.3
  `UMaterialParameterCollection` contains only scalar and vector parameters, and
  `UKismetMaterialLibrary` exposes only scalar/vector setters. It cannot publish a
  runtime texture globally.
- **E12 - Texture Collections are not the default KISS route:** installed UE 5.8.3
  marks `UTextureCollection` Experimental. `TextureFromCollection` requires SM6;
  `UTextureCollection::CreateResource` returns no resource when bindless is disabled
  or unsupported. ALIS has no project bindless override, while the installed Windows
  defaults are `Disabled` for PCD3D_SM5 and `RayTracing` for PCD3D_SM6. ALIS currently
  cooks both SM5 and SM6. A global bindless change is therefore a separate risky
  experiment, not a prerequisite for the first proof.
- **E13 - Virtual Texture Collection does not directly accept the proposed RT:**
  installed UE 5.8.3 `FVirtualTextureCollectionResource::InitRHI` accepts resources
  exposing `FVirtualTexture2DResource` or `FTexture2DResource`; a
  `UTextureRenderTarget2D` exposes `FTextureRenderTarget2DResource` and takes the
  `Invalid resource` branch. `UVirtualTextureAdapter` exists, but Epic documents it as
  memory-inefficient and unsuitable for performance use. API:
  https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UVirtualTextureCollection
- **E14 - Lower-level VT is real but not small:** Epic documents `IVirtualTexture` as a
  tile producer for streaming or runtime compositing, but implementers own render-thread
  page request/produce/finalize behavior. It is a credible future escape hatch, not a
  fallback inside this task:
  https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/RenderCore/IVirtualTexture
- **E15 - Existing loading boundary:** ProjectLoading warmup currently loads declared
  Primary Assets, queues shader PSOs, and touches traditional streaming levels. It has
  no neutral runtime-generation contributor contract. ProjectTexture depends only on
  ProjectCore; adding ProjectTexture -> ProjectLoading would invert the category
  direction.
- **E16 - Full RT memory is material:** a linear RGBA8 1024 square target is 4 MiB at
  mip 0 and about 5.33 MiB with a full mip chain; two are about 10.67 MiB. At 2048 the
  corresponding values are 16 MiB and about 21.33 MiB per target. All render-target
  mips are resident, so the implementation must account exact allocated formats and
  mips rather than call the resource a free disk saving.
- **E17 - Current task is not closed:** the active material todo still requires the
  direct-graph performance capture, packaged movable-cube proof, accepted terrain
  views, and independent R2. This task must not mutate its architecture or create a
  second active implementation slice.
- **E18 - Native tiled noise is a bounded candidate, not a blanket guarantee:** installed
  UE 5.8.3 exposes `bTiling` and `RepeatSize`; Epic describes them as useful when baking
  seamless wrapping textures. Installed editor rules allow tiling for Gradient ALU,
  Value ALU, Gradient Texture, and Voronoi ALU, which includes the current direct
  kernel's Gradient ALU and Value ALU modes. Tiled computational noise is materially
  more expensive per level, so runtime generation time and first-view readiness remain
  measured costs even when the shader executes once per generated output. Epic docs:
  https://dev.epicgames.com/documentation/unreal-engine/utility-material-expressions-in-unreal-engine
- **E19 - Mip generation has independent edge policy:** installed
  `UTextureRenderTarget2D` defaults `MipsAddressU/V` to Clamp and passes those values to
  `FGenerateMips`; ordinary sampling uses `AddressX/Y`. A repeatable output therefore
  needs explicit Wrap policy for both sampling and mip generation plus tests below mip
  0. Epic API:
  https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/Engine/UTextureRenderTarget2D
- **E20 - Native world-aligned projection exists:** installed UE 5.8.3 contains the
  `WorldAlignedTexture` material function. Epic documents its XYZ output, world-unit
  sizing, projection blending, and world-coordinate swimming on animated objects:
  https://dev.epicgames.com/documentation/unreal-engine/texturing-material-functions-in-unreal-engine
- **E21 - Canvas RT is not another binding primitive:** installed
  `UCanvasRenderTarget2D` derives from `UTextureRenderTarget2D`. Its update path handles
  canvas drawing, deferred-clear removal, flush, and resolve. It is a valid bounded
  lifecycle fallback when ordinary draw/update ordering is the reproduced defect; it
  cannot by itself repair a material-reference, package, or shared-binding failure.
- **E22 - Native de-tiling is available but not free:** installed UE 5.8.3 contains the
  `Texture_Bombing` material function. It remains conditional because its extra samples
  must beat the cheaper first refinement: a second transformed sample of the same
  cached resource.

### Inferences

- **I1:** E6-E10 make a ProjectTexture-owned stable render-target output slot the
  smallest plausible shared Mesh Terrain binding. It must remain a candidate until a
  packaged normal-route proof; source shape cannot prove first-frame ordering.
- **I2:** E4-E5 mean that baking the current function into one 2D RT cannot honestly be
  described as identical direct evaluation. The cached arm needs an explicit 2D
  world-XY or triplanar projection contract, seam test, and slope/oblique visual
  comparison.
- **I3:** E9 rules out per-section MID assignment as the universal solution: it scales
  with realization sections, crosses into World/MeshTerrain ownership, and recreates
  the coupling this architecture removed.
- **I4:** E12-E13 make Texture Collection an optional later comparison only if the
  stable output slot fails for a concrete reason. It is not a cleaner assumed route on
  the current SM5/SM6 target matrix.
- **I5:** E15 means "reuse the loading route" is not currently a zero-cost call. First
  prove whether ProjectTexture's own game-instance/world lifecycle can finish before
  the first terrain frame; introduce a generic loading readiness seam only from a
  reproduced timing failure.
- **I6:** E18-E20 select `world_metric_triplanar_2d` as the first honest cached terrain
  adapter. It should compose the installed native world-aligned behavior where that
  packages cleanly, or reproduce the same documented projection math with native
  material nodes behind ProjectMaterial's adapter. The direct arm remains
  `world_metric_3d`; neither name aliases the other.
- **I7:** A reusable output ABI is a small named bundle, not a promise that every future
  pattern has exactly one texture. The first implementation still stays minimal: one
  required linear Structure slot, with a Physical slot admitted only by close-view and
  measured sampling evidence.

### Assumptions / unverified areas

- A cooked render-target descriptor can be filled early enough on the normal
  menu -> ProjectLoading -> Kazan route without a visible clear frame or a blocking
  render-thread stall.
- The current Substrate terrain graph can sample the output-slot RT correctly on both
  PCD3D_SM5 and PCD3D_SM6 after it is filled.
- A 2D cached projection can preserve acceptable terrain quality and improve the
  representative direct 3D graph. This is deliberately unclaimed until Q2.
- Runtime resource release/recreation and garbage collection are safe for unpinned
  transient intermediates under the selected subsystem lifetime.

### Refuted

- **"Runtime RT APIs prove the terrain architecture."** Refuted by E9 and E10. They
  prove generation and sampling primitives, not a shared Mesh Terrain binding or
  first-frame readiness.
- **"A single baked 2D pattern is equivalent to the current pattern."** Refuted by E4
  and E5. The current kernel is non-tiling direct 3D noise.
- **"Texture Collection or Virtual Texture Collection cleanly accepts runtime RTs on
  current ALIS targets."** Refuted as a default by E12-E13. Plain collections require
  SM6/bindless support; virtual collections reject the RT resource class.
- **"A Material Parameter Collection can publish the texture."** Refuted by E11.
- **"Build the full DAG/LRU/loading integration before the proof."** Refuted by D5 and
  the two independent unknowns in Q1-Q2. The binding/projection proof can reject the
  production premise first.
- **"Mesh Partition Material Cache is the ProjectTexture cache."** Refuted by current
  ownership. Material Cache stores final material evaluation per Mesh Partition;
  ProjectTexture would own reusable structural inputs shared across consumers.
- **"Nanite Assemblies and GPU PCG rules belong in this texture TODO."** Refuted by
  subsystem ownership and current scope. Their general reuse idea is compatible, but
  adding geometry policy here would duplicate unrelated owners without changing Q1 or
  Q2.
- **"No hand painting means remove or bypass the canonical hand overlay."** Refuted by
  the active material task D13 and the World ownership boundary. This prototype must
  require no hand-authored texture, generated-asset edit, or city-specific appearance
  parameter; it still consumes final canonical surface weights after the existing
  authored terrain correction layer.
- **"CanvasRenderTarget2D is a second shared-binding solution."** Refuted by E21. It is
  the same render-target texture family with a different update lifecycle, so it is
  useful only when that lifecycle is the demonstrated failure.
- **"Expensive tiled noise is automatically acceptable because it runs once."**
  Refuted by E18. One-time generation removes steady per-pixel repetition, but cold
  generation and first-view hitch remain product costs and must fit the measured gate.

## Current architecture and source of truth

```text
ProjectTexture JSON recipe + accepted manifest        authoritative structure
        -> generated material function                disposable editor projection
        -> ProjectMaterial recipe/compiler            appearance owner
        -> generated Substrate parent/instance        sampled by shared Mesh Terrain MPD
        -> compiled-section MICs/static meshes         Mesh Terrain realization

CanonicalCompilation -> ProjectWorld -> ProjectWorldMeshTerrain
        terrain facts      realization     MPD/channel adapter

ProjectWorld -X-> ProjectTexture runtime cache
ProjectWorld -X-> ProjectMaterialEditor
```

Stable owners:

- Repository authority and generated-projection rule:
  [architecture principles](../../../docs/architecture/principles.md).
- ProjectTexture recipe/function authority:
  [ProjectTexture README](../../../Plugins/Resources/ProjectTexture/README.md) and
  [pattern generation](../../../Plugins/Resources/ProjectTexture/docs/pattern_generation.md).
- ProjectMaterial appearance, projection, and direct/cache policy:
  [ProjectMaterial README](../../../Plugins/Resources/ProjectMaterial/README.md) and
  [surface generation](../../../Plugins/Resources/ProjectMaterial/docs/material_generation.md).
- Terrain semantics and realization boundary:
  `Plugins/World/ProjectWorld/docs/territory_contract.md` and
  `Plugins/World/ProjectWorldMeshTerrain/README.md`.

Expected changed components after approval:

- ProjectTexture recipe/manifest/compiler only after the isolated seam passes.
- ProjectTexture Runtime module for the selected lifecycle, cache, and telemetry.
- A bounded set of ProjectTexture generation/output descriptor assets.
- ProjectMaterial Editor compiler and terrain graph consumer adapter.
- Focused tests and the stable owners named above.

Expected untouched components:

- CanonicalCompilation, ProjectWorld geography, terrain-surface semantics, hand
  overlays, World transaction code, Mesh Terrain topology, channel ABI, and shared MPD.
- City17, Water, roads, vegetation, buildings, object generation, and release layout.

Unexpected propagation into an untouched owner stops implementation and returns the
candidate to architecture review.

## Problem and root cause

ProjectTexture currently authenticates reusable structural functions but owns no
packaged runtime resource lifecycle. Expensive structural work is therefore evaluated
inside each material invocation. The proposed runtime cache cannot simply be added:

1. Mesh Terrain bakes per-section MICs, so ordinary transient texture parameters lack
   one shared runtime binding point.
2. The accepted structural ABI is direct metric 3D and non-tiling; a full 2D RT changes
   projection and repetition behavior.
3. Full RTs are fully resident GPU allocations, so storage savings can become
   unbounded VRAM without an explicit budget and pin/eviction policy.
4. The existing loading phase has no generic generation/readiness contributor, so
   first-frame prewarm cannot be assumed.

The root capability gap is therefore not "we need an LRU." It is the absence of one
consumer-neutral runtime output seam whose dimensional contract, binding, readiness,
and cost are proven through the real terrain consumer.

## Decision

Use a proof-first, slot-backed design. Do not change the durable recipe schema or build
the general cache until the two smallest packaged gates pass.

### Gate 1 - native generation and shared binding

Build one isolated ProjectTexture fixture:

```text
node A -> transient linear RT with mips
node B samples A -> ProjectTexture-owned stable Structure output-slot RT with mips
fixture material directly references the Structure slot asset
```

The output-slot UAsset is a replaceable descriptor only. No generated pixel source is
saved, exported, or cooked. A runtime owner fills the loaded resource once. The same
fixture is first sampled by an ordinary movable cube, then by the generated terrain
parent without changing ProjectWorld or compiled-section materials.

Tileable slots explicitly use Wrap for `AddressX/Y` and `MipsAddressU/V`. Gate 1 passes
only in a packaged Development run on the existing target matrix, with one generation
of A/B, stable output package bytes before/after runtime, seam-safe mip 0 and lower
mips, no clear/fallback frame at the terrain view, and no per-section material
creation. If ordinary RT draw/update ordering fails, diagnose it and try one exact
lifecycle correction; `UCanvasRenderTarget2D` is admissible only for that failure class.

### Gate 2 - projection and value

Add one explicitly named `world_metric_triplanar_2d` cached projection candidate. Use
the installed native world-aligned XYZ behavior where practical behind ProjectMaterial's
adapter; if direct function composition does not package through the generated graph,
use the equivalent native-node projection and record the reason. Do not add a terrain-
specific world-XY/cliff branch. Future movable objects need an honest
`object_metric_triplanar_2d` adapter to avoid world-coordinate swimming, but that is
not implemented in this terrain slice.

Generate the tile with one installed tiling-capable native noise mode and a bounded
`RepeatSize`; do not assume every Noise/VectorNoise mode tiles. The production pattern
must also prove a real two-node DAG:

```text
tileable_base -> natural_ground_structure -> exported Structure slot -> terrain
```

The exported ABI is a named structural bundle. Its minimal first candidate is:

```text
Structure (linear RGBA)
  R MacroVariation
  G CoverPatch
  B GroundStructure
  A DetailHeight

Physical (optional only when close-view evidence requires it)
  normal/roughness structural information, with exact named channels
```

`CoverPatch` is a visual structure signal, not canonical grass or land-cover truth.
ProjectTexture owns no final colors. ProjectMaterial maps the signals plus canonical
`ground`/`hydro_transition` weights and continuous slope to grass-like cover, soil,
exposed earth/clay/mineral, wet tint, roughness, and normal response. The prototype
uses no imported terrain texture, hand-edited generated asset, or city-specific
parameter. It does not bypass the canonical authored terrain overlay.

Gate 2 passes only if identical Kazan and Manhattan views show believable natural
terrain rather than a colored noise/debug surface: grass-like cover mixed with soil on
flatter ground, progressively less cover and more exposed earth on steeper ground,
darker/wetter hydro transitions, macro breakup, medium patch structure, and close
physical detail. It must also pass steep/oblique projection, metric scale, seam,
lower-mip, and far/aerial repetition review.

If repetition is visible, the one bounded quality refinement is a second transformed
sample of the same cached Structure resource at an incommensurate metric scale/offset
or rotation. Native Texture Bombing is considered only if that remains insufficient
and measurement justifies its sample cost.

The direct comparison must implement equivalent production appearance rather than let
the current cheaper three-signal graph win by doing less work. Positive promotion
requires production quality plus at least one concrete scalable benefit, such as lower
steady shader work, reused expensive computation, or bounded package/resource
cardinality. The result must report cold generation, first-view hitch, warm CPU/GPU
frame cost, resident bytes, shader/PSO count, cook/package delta, and generated asset
count. Regressions may trade against benefits only inside accepted budgets; the cached
arm need not win every metric.

### Durable design only after both gates

If both gates pass:

- ProjectTexture recipe v3 becomes a closed acyclic node graph. Each node identity is
  derived from algorithm, normalized structural parameters, seed, ordered parent
  identities, output dimensional/layout contract, resolution class, compiler
  fingerprint, and UE Major.Minor compatibility.
- The editor compiler topologically validates the graph and emits one authenticated
  cooked runtime catalog plus stable named output-slot descriptors only for exported
  bundle members. Intermediate pixels and RTs remain transient.
- The ProjectTexture runtime owner resolves the catalog, generates missing parents,
  pools compatible intermediate RTs, publishes only after render completion, and
  exposes request/pin/release plus readiness and telemetry.
- The hard budget counts actual output/intermediate format, dimensions, and mip chain.
  Pinned entries are never evicted. Admission fails before allocation when pins would
  exceed the budget. Unpinned entries use the smallest proven LRU; no timer or polling
  loop is added.
- ProjectMaterial continues to name only `pattern_id`. Its editor compiler resolves
  the accepted named output ABI, slots, and projection ABI; runtime code does not parse
  JSON or load either Editor module.
- The normal product route prewarms the finite terrain pattern set through
  ProjectTexture's lifecycle. Modify ProjectLoading only if a reproduced first-frame
  ordering failure proves that a readiness wait is required; any seam is generic and
  dependency-correct, never ProjectTexture -> ProjectLoading.

### Premise / KISS gate

ProjectTexture already owns structural identity and has an empty Runtime module, so it
is the only new lifecycle owner. The chosen first candidate adds:

- one runtime owner in the existing module;
- one two-node packaged fixture;
- one required stable Structure output-slot descriptor for the exported terrain
  pattern; and
- one ProjectMaterial sampling adapter after the fixture passes.

It initially gives up arbitrary runtime rebinding and page-granular virtualization.
That is deliberate. It removes a bindless platform change, per-section MIDs, custom
VT producer, disk cache, new plugin, and new loading framework from the first proof.

### Alternatives considered

| Alternative | Decision | Evidence-based reason |
|---|---|---|
| Keep direct 3D evaluation | Control and rollback | Current accepted path; wins automatically if Q1/Q2 do not discriminate in favor of caching. |
| Stable ProjectTexture RT output slot | First candidate | Uses ordinary texture sampling on SM5/SM6, gives all section MICs the same referenced object, and cooks descriptor state rather than pixels. Packaged readiness remains unverified. |
| Per-section runtime MIDs | Reject | O(sections), crosses the World/MeshTerrain boundary, and has no single owner for lifecycle or release. |
| Plain `UTextureCollection` | Defer | Experimental, SM6-only material expression, bindless-dependent, and mismatched with the current SM5/SM6 acceptance matrix. |
| `UVirtualTextureCollection` | Reject for RT input | Its installed resource path rejects `FTextureRenderTarget2DResource`; the adapter workaround is documented as inefficient. |
| Material Parameter Collection | Reject | Scalar/vector only; cannot carry a texture. |
| Runtime Virtual Texture | Out of scope | World-space cross-primitive composition owner, not a reusable structural DAG cache. |
| Mesh Partition Material Cache | Separate layer | Caches final section material evaluation, not reusable ProjectTexture structure. |
| Custom `IVirtualTexture`/RDG/compute | Future escape hatch | Credible for page generation, but adds render-thread producer/finalizer and shader infrastructure before a full-RT limitation is measured. |
| Runtime Texture Graph | Reject on UE 5.8.3 | Installed-source investigation in the active material task found editor-only engine initialization and disabled async cache behavior. |
| Saved generated `UTexture2D` assets | Reject | Reintroduces generated pixel packages/cook growth and turns regeneration into content mutation. |

## Required invariants

1. JSON recipes and accepted manifests remain structural authority. Generated functions,
   catalogs, descriptors, pixels, and cache entries are reproducible projections.
2. ProjectTexture owns structural node identity, dependencies, generation, memory,
   readiness, and cache observability. ProjectMaterial owns appearance and projection.
3. CanonicalCompilation, ProjectWorld, and ProjectWorldMeshTerrain do not call the
   runtime cache and do not gain ProjectTexture/ProjectMaterial editor dependencies.
4. Generated resource and asset cost scales with unique reusable structure and its
   named exported slots plus active intermediates, never with world population,
   consumers, sections, cells, cities, colors, transforms, or cosmetic instances.
5. Packaged targets parse no authoring JSON and load no Editor module.
6. Node graphs are acyclic, parent-complete, deterministically ordered, and fail closed
   on unknown algorithms, fields, layouts, formats, dimensions, or identity mismatch.
7. A normalized no-op reuses the same entry. A structural change invalidates exactly
   the changed node and descendants; appearance-only changes invalidate none.
8. One generation is published only after its render work and mip chain are complete.
   Concurrent requests cannot publish duplicate writers for one identity.
9. The hard GPU budget is checked before allocation. Pinned resources are not evicted;
   budget exhaustion is observable and leaves existing ready entries unchanged.
10. Runtime generation performs no normal-path CPU pixel readback, texture export,
    `ConstructTexture2D`, package save, disk cache write, or network operation.
11. The accepted output is seam-safe for its declared projection and retains truthful
    metric scale. A 2D adapter never claims to be the current direct 3D contract.
12. The output-slot package and World/MeshTerrain packages remain byte-stable during
    runtime generation and same-path appearance tuning.
13. Dedicated Server creates no render resources and loads no rendering-only runtime
    catalog; its dependency/build boundary remains clean where the installed toolchain
    can prove it.
14. Failure, cancellation, travel, and shutdown release transient resources without
    stale callbacks or a second writer. A visible production material is never silently
    treated as ready after generation failure.

## Implementation tasks

- [x] **T0 - Admission and R1:** keep this task in backlog until the active material
  task reaches PASS. Obtain independent architecture review of Q1/Q2, A2, the explicit
  2D-versus-3D correction, ownership, and the stop-on-no-win rule. Do not implement on
  reviewer PATCH/BLOCKER.
- [x] **T1 - Test the native seam:** add the smallest isolated two-node packaged fixture
  in ProjectTexture. Use a transient parent RT, one stable Structure output-slot RT
  descriptor, automatic mips, Wrap sampling/mip addressing, explicit counters, and an
  ordinary fixture material. Prove package bytes and Content remain unchanged after
  runtime. On a reproduced update-order failure, make one exact correction and use
  Canvas RT only when its lifecycle specifically addresses that failure.
- [x] **T2 - Prove the real shared consumer:** make a candidate ProjectMaterial terrain
  graph reference the same output slot at its stable path. Package through the normal
  Kazan route and prove every section sees the filled resource without ProjectWorld
  changes, per-section MIDs, a bindless config change, or a first-view clear frame.
  If Q1 still fails after T1's bounded exact correction, return to architecture review
  and remove the candidate rather than adding a per-section or World-owned workaround.
- [x] **T3 - Prove projection:** create one closed tileable 2D structural algorithm and
  the `world_metric_triplanar_2d` projection ABI. Prove mip-0/lower-mip continuity,
  metric scale, production appearance, and far-view repetition against an equivalent-
  quality direct graph. If quality fails, apply exactly one evidence-led refinement,
  preferring a second transformed sample of the same resource; then return to
  architecture review if Q2 still cannot pass.
- [x] **T4 - Add durable DAG contracts only after T1-T3 pass:** replace schema v2 in one
  pass with the closed v3 node graph; add cycle/missing-parent/layout/identity checks,
  topological compilation, accepted-manifest coverage, and one authenticated cooked
  runtime catalog. Define a small named output bundle with required Structure and no
  Physical slot unless T3 evidence requires it. Keep `pattern_id` as ProjectMaterial's
  only recipe coupling. Make the accepted terrain pattern itself reuse a cached parent;
  the synthetic fixture is not the only DAG proof.
- [x] **T5 - Add the bounded runtime owner:** implement request/pin/release, one-writer
  state, exact resident-byte accounting, compatible intermediate pooling, unpinned LRU,
  render completion, failure/shutdown behavior, and structured telemetry in the existing
  ProjectTexture Runtime module. Do not add another plugin.
- [x] **T6 - Prove scale and invalidation:** run 1/10/100-consumer reuse, parent/child/
  sibling invalidation, no-op identity, budget refusal, pin/eviction/regeneration,
  travel, and rendering-disabled controls. Known-bad controls must make each instrument
  reject.
- [x] **T7 - Normal-route prewarm:** first prove ProjectTexture's own lifecycle on the
  packaged menu -> ProjectLoading -> Kazan route. If it misses the first terrain frame,
  capture that failure and return the smallest generic readiness seam for review before
  modifying ProjectCore/ProjectLoading. Do not add polling.
- [ ] **T8 - Production A/B:** compare the authenticated direct and cached terrain arms
  under identical package, camera, RHI, scalability, and sampling envelopes. Record
  quality, cold generation, readiness, hitch, warm CPU/GPU frame cost, resident bytes,
  shader/PSO count, package/cook bytes, asset count, and World/package locality. Promote
  only if Q2 passes. Capture an identical paired board for player-height flat ground,
  close detail, steep/oblique terrain, hydro transition, aerial/far repetition, and one
  Manhattan sanity view in addition to Kazan. Package, quality, shader, resource,
  cardinality, and locality evidence is complete; frame/GPU timing remains unverified
  because the official collector crashes identically before sampling both arms. The
  original close-detail framing intersected geometry and is excluded; a replacement
  Shipping ground-detail frame proves projection and broad structure but not final
  photoreal microdetail.
- [ ] **T9 - Close current architecture:** remove the losing arm and all probe-only
  assets/code/config; regenerate accepted authorities once; prove the immediate rerun is
  zero-write; update only the stable owners whose behavior changed; run independent R2.
  The direct arm, probe-only code/assets/config, and stale v2 compatibility are removed;
  regeneration/no-op and stable-doc updates are complete. Independent R2 remains.

## Test-first and verification plan

### Red evidence

| Case | Kind | Wrong behavior captured before production code |
|---|---|---|
| `Project.Texture.Runtime.GraphContract` | Permanent regression guard | Current v2 parser cannot represent parents, output layout, or resolution class; cycle/missing-parent cases have no owner. |
| `Project.Texture.Runtime.StructuralIdentity` | Permanent regression guard | Parent identities and runtime output contract do not participate in identity/invalidation. |
| `Project.Texture.Runtime.CacheReuse` | Permanent regression guard | No runtime owner exists; repeated requests cannot converge on one ready entry. |
| `Project.Texture.Runtime.BudgetAndPins` | Permanent regression guard | No pre-allocation budget refusal, pin protection, or bounded eviction exists. |
| `Project.Texture.Runtime.FailureLifecycle` | Permanent regression guard | No one-writer failure, cancellation, travel, or shutdown contract exists. |
| Two-node packaged fixture | Acceptance | No packaged process generates A once, generates B from A once, waits for completion, and samples B with working mips. |
| Stable-slot package control | Acceptance | No proof shows runtime fill leaves the output-slot package, cook payload, and repository bytes unchanged. |
| Non-tiling edge known-bad | Acceptance control | Baking the current `bTiling=false` noise to a repeated 2D RT exposes an edge mismatch; the seam detector must reject it before testing a tileable candidate. |
| Clamp-mip known-bad | Acceptance control | A crafted mip-0 tile with matching boundary texels but deliberately different edge neighborhoods is generated with Clamp mip addressing; the lower-mip seam control must reject it, proving the test observes mip contamination rather than only the base level. |
| Shared Mesh Terrain binding | Acceptance | No proof shows all compiled sections sample one filled runtime slot without runtime MIC multiplication or World changes. |
| First-view timing control | Acceptance control | Deliberately delay generation; the normal-route check must reject a clear/default terrain frame and report not-ready rather than pass on a later screenshot. |
| 1/10/100 consumer control | Acceptance | No evidence shows identical structural requests keep one entry and one generation count. |
| Descendant invalidation control | Acceptance | No evidence shows a parent change regenerates descendants while an unrelated sibling remains warm. |
| Production DAG counters | Acceptance | No evidence shows `tileable_base` and `natural_ground_structure` each generate once while all terrain consumers sample the exported descendant and the unpinned parent can return to the pool. |
| Direct-versus-cached terrain | Acceptance | No cached arm exists, and the requested total-system outcome has not moved. |
| Ownership review | Reviewer-checked judgement | Confirm names, dependency direction, projection truthfulness, absence of per-section/per-cell state, and no unrelated geometry policy. |

Each permanent guard is added and observed failing through the exact single-test route
before its production behavior is implemented:

```powershell
scripts/ue/test/unit/iterate.ps1 -TestFilter <exact-full-test-name>
```

### Green evidence

Run narrow checks first, then one authenticated acceptance package:

1. The exact five `Project.Texture.Runtime.*` tests above, one at a time.
2. Existing owner guards, one at a time:
   - `Project.Texture.Generation.PatternRecipeContract`
   - `Project.Texture.Generation.PatternManifestContract`
   - `Project.Texture.Generation.AssetCompilerContract`
   - `Project.Material.Generation.PatternAuthority`
   - `Project.Material.Generation.SurfaceRecipeContract`
   - `Project.Material.Generation.SurfaceAssetCompilerContract`
   - `Project.Material.MeshTerrain.LayoutReceipt`
3. `scripts/ue/standalone/build.ps1` after reflected headers/module dependencies settle.
4. The existing schema validator route from `scripts/ue/check/README.md`.
5. A Development candidate package through:

```powershell
scripts/ue/package/package_release.ps1 `
  -ClientConfig Development `
  -OutputDir tmp/material/runtime-pattern-cache/package `
  -RequiredCookMap /ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory
```

6. The packaged fixed-view runner from `scripts/ue/world/README.md`, extended only with
   ProjectTexture readiness/generation telemetry owned by the fixture. Capture an
   identical paired direct/cached board: player-height flat ground, close detail,
   steep/oblique terrain, hydro transition, aerial/far repetition, and Manhattan sanity
   in addition to Kazan. Include the known-bad delayed-generation view.
7. One direct/cached performance tournament using the existing authenticated Kazan
   performance collector. Use identical package inputs except the reviewed arm switch;
   record raw samples, not screenshot timing.
8. IoStore inspection and before/after file/package hashes prove that no generated
   pixel payload, test mount, or runtime mutation entered the release package or World.
9. Rendering-disabled/Game/Client dependency checks and the documented Server static
   boundary. Do not claim a Server binary on the launcher engine, which refuses Server
   targets before compiling project code.
10. Fresh Shipping cook/package only after the candidate wins Q2 and production state
    is regenerated. Confirm PCD3D_SM5 and PCD3D_SM6 compile and run the existing Kazan
    fixed-view/product route.

The expensive-gate budget is one initial candidate run plus one replacement after a
necessary in-scope fix. Discovery uses the isolated fixture and exact tests, not the
full package loop.

## Documentation plan

- **Authoritative stable owner:**
  `Plugins/Resources/ProjectTexture/README.md` and
  `Plugins/Resources/ProjectTexture/docs/pattern_generation.md`.
- **Consumer owner:**
  `Plugins/Resources/ProjectMaterial/docs/material_generation.md`.
- **Router / TOC update:** audit `docs/architecture/structure.md` and the two plugin
  READMEs; add links only when the accepted runtime owner creates a new stable document.
- **Content to add or change only after Q2 passes:** DAG identity/dependencies,
  dimensional/output ABI, runtime catalog, stable output-slot meaning, cache lifecycle,
  budget/pins/eviction, readiness, telemetry, failure behavior, and the ProjectMaterial
  consumption boundary.
- **Architecture principles:** the existing canonical-authority -> replaceable
  projection rule already owns the broad principle. After positive proof, add the
  concise cardinality rule if it is still absent: generated resource cost scales with
  unique reusable structure, not world population or consumer count. Do not add
  MeshTerrain-, Nanite-, PCG-, or experiment-specific policy there.
- **World/MeshTerrain docs:** no change is expected. If the adapter remains untouched,
  do not add cache details merely because terrain was the proof consumer.
- **Duplication avoided:** routers link to ProjectTexture/ProjectMaterial owners; they
  do not restate node schemas or cache policy. Stable docs, code, comments, tests, and
  configuration never link to this todo.
- **Negative result:** if Q1 or Q2 fails, stable docs retain direct evaluation and need
  only a correction if this investigation exposed an inaccurate current statement.
  Experimental history remains here, not in current architecture docs.

## Rollout and rollback

- T1-T3 use isolated fixture assets and a reversible candidate material arm. They do
  not replace accepted manifests, production material paths, or World packages.
- Production promotion happens once, at the same ProjectMaterial object path, only
  after Q2. ProjectWorld is not regenerated for appearance-only content.
- Keep the accepted direct package and material authority until the cached Shipping
  package and normal product route pass. Rollback restores the prior ProjectTexture and
  ProjectMaterial accepted manifests/assets through their existing generation
  transactions; no Landscape or legacy compatibility is introduced.
- A runtime generation failure fails readiness observably. It does not write packages,
  silently claim cache readiness, or mutate canonical/World state.
- If the shared slot, projection, or performance premise fails, remove the entire
  candidate and close on direct evaluation. Do not keep dead schema fields, config,
  services, or fallback writers.

## Completion criteria

This todo may reach PASS with either a positive or a bounded negative measured
conclusion. One first-attempt failure is not sufficient negative evidence.

Positive PASS requires:

- the active material task and this task's independent R1 both pass before implementation;
- the packaged two-node fixture generates each node once, reuses its parent, publishes
  after completion, and samples working mips;
- the minimal named output bundle, beginning with the required Structure slot, feeds
  all production terrain sections with no per-section MID, World code change, bindless
  requirement, saved pixels, or first-view clear frame;
- the accepted recipe is an acyclic deterministic DAG whose normalized no-op,
  descendant invalidation, and 1/10/100-consumer controls discriminate correctly;
- resident bytes never exceed the hard budget; pin, refusal, eviction, regeneration,
  failure, travel, and shutdown behavior are observed;
- the `world_metric_triplanar_2d` cached projection passes the paired flat, close,
  steep/oblique, hydro, aerial/far repetition, and Manhattan/Kazan review at accepted
  metric scale, including mip-0 and lower-mip seams;
- the accepted terrain reads as believable generated natural ground, not a colored
  procedural test surface, without imported terrain textures, city-specific appearance
  parameters, hand-edited generated assets, or invented canonical grass/rock facts;
- authenticated A/B evidence proves production quality and a concrete scalable
  total-system benefit over an equivalent-quality direct implementation while every
  reported cost stays inside its accepted budget;
- Development and final Shipping packages pass current SM5/SM6 and product-route gates,
  and package/World locality and output-slot byte stability are proved;
- stable docs describe only the accepted current design and independent R2 returns PASS.

Negative PASS requires:

- the rejecting Q1 or Q2 experiment, its known-bad control, and the one bounded exact
  correction or quality refinement are reproducible;
- all probe-only code/assets/config and any unused contract are removed;
- direct evaluation and current accepted authorities remain green and byte/locality
  expectations are restored; and
- stable docs make no unsupported runtime-cache claim.

Neither result may claim broader object/material/VT scalability than the tested terrain
and isolated DAG evidence proves.

## Review record

### 2026-09-28 - initial investigation and reviewer proposal correction

- **Trigger:** D1 requested a proper texture foundation before continuing the terrain
  material; the supplied proposal recommended a full runtime DAG, bounded RT cache,
  loading prewarm, and terrain cutover.
- **Root cause:** the proposal correctly identified the missing reusable runtime layer,
  but treated RT generation as if it also solved Mesh Terrain binding and treated one
  2D cached texture as equivalent to the current non-tiling direct 3D contract. It also
  assumed Texture Collections and loading integration were ready seams and mixed
  unrelated Nanite/PCG policy into the texture task.
- **Fix:** the plan now gates durable architecture on (1) a stable shared output-slot
  proof and (2) an honest projection/performance win. It selects a cooked RT descriptor
  as the smallest source-supported candidate, defers bindless/VT/loading framework work,
  keeps direct evaluation as the default, and removes unrelated geometry scope.
- **Verification:** read current ProjectTexture/ProjectMaterial/ProjectLoading/World
  owners; inspected installed UE 5.8.3 render-target, material-instance, Texture
  Collection, Virtual Texture Collection, virtual-texture, shader-platform, and Mesh
  Partition sources; checked Epic 5.8 primary API documentation. No production code,
  asset, config, shader, build, cook, package, or external state changed.
- **Authority:** D1-D5 recorded; Q1-Q2 opened; A1-A6 recorded. This todo requires
  independent review and is not implementation authorization.

### 2026-09-28 - production terrain acceptance refinement

- **Trigger:** reviewer accepted the current v3 producer/consumer boundary but found
  that this follow-up could close as a cache-only experiment without proving the
  requested terrain product.
- **Finding:** the plan left projection open, treated one output slot as the apparent
  whole ABI, checked seams without explicit lower-mip policy, compared against a
  cheaper direct graph, and allowed immediate negative closure. Those gaps could reject
  the scalable path for unequal work or accept a technically reusable but visibly weak
  result.
- **Fix:** select `world_metric_triplanar_2d`, a minimal named Structure bundle, a real
  production DAG, installed tiling-capable noise, Wrap sampling/mip policy, paired
  natural-terrain views, equivalent-quality A/B, bounded de-tiling, and one correction
  attempt before negative closure. Preserve the canonical hand overlay and keep Canvas
  RT limited to its actual update-lifecycle failure class.
- **Verification:** checked current ProjectTexture/ProjectMaterial code and docs,
  active-task decisions, installed UE 5.8.3 Noise, RenderTarget2D,
  CanvasRenderTarget2D, WorldAlignedTexture, and Texture_Bombing implementations/assets,
  and Epic 5.8 primary documentation. No production code, asset, config, shader, build,
  cook, package, or external state changed.

### 2026-09-28 - implementation and production selection (R2 REQUIRED)

- **Implemented:** schema/manifest v3, deterministic topological DAG validation,
  authenticated runtime catalog, a two-node native GPU graph, one stable output slot,
  full mips, Wrap addressing, one-writer lifecycle, pins, exact bytes, budget refusal,
  unpinned LRU, failure/shutdown cleanup, and normal-route prewarm all live in
  ProjectTexture. ProjectMaterial owns the sampled triplanar adapters and appearance.
- **Scale proof:** 1, 10, and 100 consumers converge on one 1,398,100-byte ready output;
  the two-node generation peak is 2,796,200 bytes. Cycle, missing-parent, descendant
  identity, byte stability, mip-0/lower-mip seams, budget, pin, eviction, failure, and
  one-writer controls pass.
- **A/B result:** the equivalent direct arm evaluated four native noises per visible
  pixel. Cached rendering preserved the green/brown terrain read, was smoother at
  distance through mips, and reduced the paired Development payload from 2,286,926,409
  to 2,282,090,377 bytes. The direct arm and every probe-only artifact were removed.
- **Performance limitation:** the authenticated packaged performance collector crashed
  both arms before sampling with the same UE 5.8 CSV-profiler assertion. This rejects
  the instrument, not either material arm. CPU/GPU frame, hitch, and cold-generation
  timing remain unverified and no number is claimed.
- **Final route:** 19 exact tests passed. Immediate pattern and surface regeneration
  reruns were zero-write. The current Shipping package compiled the generated terrain
  and object materials for SM5 and SM6, passed full cook, archive, IoStore, and source
  identity checks, and totaled 2,063,109,020 bytes. Kazan, Manhattan, and the
  moved/rotated cube produced accepted fixed-view receipts. The Manhattan view is a
  technical sanity frame. The original close-detail frame intersected geometry and is
  excluded; its replacement proves stable projection and broad ground structure, not
  final photoreal microdetail.
- **Review finding:** the prior requirement to select direct evaluation merely because
  frame timing is unavailable would overrule the other discriminating product evidence
  and is rejected. Conversely, claiming a measured timing win would also be incorrect.
  Independent R2 and repair/replacement of the common timing collector remain.

Material changes to goal, decision, scope, invariants, or completion criteria require
renewed review before implementation continues.

### 2026-09-28 - per-channel readback correction (PATCH)

- **Accepted reviewer finding:** an overall packed-range check was insufficient evidence
  for four named Structure channels. The GPU test now requires R, G, B, and A to vary
  independently at mip 0 and mip 4.
- **Red evidence:** the restored production Canvas path passes RGB structure and seam
  checks, but channel 3 is constant at both mips. This explains why close terrain lacks
  the promised `DetailHeight` breakup.
- **Falsified fixes:** UI/translucent produced an empty target; Surface/translucent also
  produced an empty target. Both experiments were removed. Production generation was
  restored to UI/alpha-composite and the accepted asset regenerated.
- **Required fix:** choose one truthful current-only contract: transport four real GPU
  channels through a proven native path, or reduce/repack the ABI and update every
  consumer, manifest, test, document, A/B arm, package, and visual proof. Do not weaken
  the new per-channel regression.

### 2026-09-28 - Canvas source audit and RGB contract update

- **Native-path conclusion:** installed UE 5.8 `FCanvas::SetWriteDestinationAlpha` is read only by
  the texture/tile alpha-blend overload. `UCanvas::K2_DrawMaterial` submits a material tile and
  does not consult that flag, so the suggested setter cannot repair material alpha transport here.
- **Implemented and regenerated:** ProjectTexture now exports `surface_structure_rgb_v1`; parent
  and child use RGB only, ProjectMaterial consumes `GroundDetail` from blue, and generator opacity
  is constant. One RGBA8 resource remains physically allocated, but its alpha lane is outside the
  public contract. No World data was regenerated.
- **Evidence:** AlisEditor Development build succeeded; the runtime two-node GPU regression passed
  1/1 with RGB range, pairwise distinction, wrap, mip-0/mip-4, cache-reuse, and package-byte checks;
  pattern regeneration no-op skipped 1/1 and surface no-op skipped 4/4 with zero shader compiles;
  JSON validation passed 434 schemas.
- **Open:** close-ground Development visual, timing evidence, and independent R2. The existing
  VisualVerification planner requires Landscape descriptors and does not authenticate the production
  Mesh Terrain runtime view; do not treat its old screenshots as post-change evidence.

### 2026-09-30 - closure (merged)

- **Outcome:** positive. T0-T7 and the T9 cleanup are complete; ProjectTexture's runtime cache is
  the sole production arm. The Terrain Material v2 task records the post-RGB close-ground and
  final Development views, the 600-frame native Kazan measurement of the cached path, the
  packaged cold/warm CPU request-to-Ready proof, and an independent final delta R2 over the same
  code.
- **Not proven:** the direct-vs-cache frame/GPU delta. The direct arm was removed after the paired
  A/B, so no timing win is claimed and none will be measured.
- **Remaining owner:** the Terrain Material v2 task owns the public developer-payload declaration
  of ProjectTexture's pattern authority and all candidate proof. No work stays scheduled here.
