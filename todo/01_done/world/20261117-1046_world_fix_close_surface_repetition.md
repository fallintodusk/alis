# Reduce close-range road material repetition

**Status:** DONE - candidate 3 locally adopted, verified and accepted by final implementation review; 3/3 visual models used

**Scope:** ProjectMaterial / ProjectTexture; local road appearance  
**Stable documentation owners:** [material generation](../../../Plugins/Resources/ProjectMaterial/docs/material_generation.md), [pattern generation](../../../Plugins/Resources/ProjectTexture/docs/pattern_generation.md)

## Contents

- [Goal and authority](#goal)
- [Current failure and next discriminator](#current-failure-and-next-discriminator)
- [Evidence and ownership](#verified-evidence)
- [Decision and invariants](#decision)
- [Implementation and verification](#implementation-tasks)
- [Handoff and completion](#documentation-plan)
- [Result and evidence](#result-and-evidence)

## Goal

Remove player-visible repeating bands and motifs on roads while retaining the accepted distant appearance, useful near detail, stable motion and rendering cost. A different grain arrangement does not satisfy this goal if the same repeating bands remain. Establish detection on the operator-rejected output before selecting another fix.

Acceptance means perceptually natural material appearance at actual gameplay distances, not mathematically eliminating repetition from a finite texture.

Closed handoff: the final implementation review accepts the integrated compiler,
closed verifier, cold default images and retained receipts below. No fourth model,
source-forensics restart or building implementation is required for this slice.
Release source freeze, installed-game performance and R5 remain separate gates.

## Authority register

### Operator decisions

- **D1 [SUPERSEDED IN PART by D3 - planning-only execution restriction]:** Add a current todo with proposed investigation, tests and fix.
  - Effect: planning only; implementation remains subject to approval.
  - Reason: close-range tiles remain visible after the distant-road improvement.
  - Date/source: operator request, 2026-11-17.
- **D2 [SUPERSEDED IN PART by D3 - restriction on executing future material trials]:** Critically evaluate the supplied review and apply valid in-scope code/documentation corrections.
  - Effect: correct this plan against actual owners; reviewer approval does not authorize executing the future material trials.
  - Reason: not stated.
  - Date/source: operator's review-and-fix request, 2026-11-17.
- **D3 [SUPERSEDED IN PART by D6 - candidate ceiling only]:** Execute the reviewed todo autonomously through implement-approved-change, preserving the accepted road baseline and untouched owners, with one diagnostic batch and at most two candidate fixes.
  - Effect: supersedes the planning-only execution restrictions in D1/D2; their historical scope and reasons remain unchanged. No staging, commits, publication or release promotion.
  - Reason: revised D2 plan accepted for implementation.
  - Date/source: operator's explicit skill invocation and approved-plan request, 2026-11-17.
- **D4:** Investigate the repeating patterns highlighted on both comparison images; research techniques, first establishing how to detect the remaining tiling.
  - Effect: prior visual PASS/DONE is withdrawn. Diagnose the rejected output before further behavior changes. Preserve D3's one-of-two model-attempt count; this research does not reset or extend the model budget.
  - Reason: the main visible pattern remains in the reported result.
  - Date/source: operator's marked before/after screenshot and correction, 2026-11-17.
- **D5:** Try to improve the road appearance based on the measured evidence and understanding.
  - Effect: continue autonomously from the reopened diagnosis into the remaining bounded fix after localizing the failing contribution; D3's two-candidate ceiling remains unchanged.
  - Reason: not stated.
  - Date/source: operator's continuation request, 2026-11-17.
- **D6:** Allow one additional isolated native sampling candidate, starting with shared TextureVariation while retaining roughness variation, grain and normal detail.
  - Supersedes: D3's two-candidate ceiling only; the total limit is now three. No World regeneration, release promotion or Git operations.
  - Effect: permits the third isolated model and closes Q1. Original roughness variation must be retained in this comparison; production defaults remain unchanged.
  - Reason: not stated.
  - Date/source: operator's explicit reply to the budget-extension question, 2026-11-17.
- **D7:** Critically evaluate the candidate-3 review and fix valid code/documentation findings.
  - Effect: authorizes integration and verification of the already-tested candidate and resolution of the retained source disposition, not another visual model. The operator's preceding visual assessment prefers candidate 3; measurable residual recurrence alone is not a rejection criterion. No release promotion or Git operations; unrelated output changes remain a scope boundary.
  - Reason: not stated.
  - Date/source: operator's review-and-fix request, 2026-11-17.
- **D8:** Allow bounded 10-surface reconciliation after the native road checks pass.
  - Effect: supersedes the road-only generated-output restriction for the existing full surface generator. Reconcile all ten surface records and verify unrelated material behavior remains unchanged. No World regeneration, release promotion or Git operations; the three-model limit is unchanged.
  - Reason: shared compiler fingerprint invalidates all surface identities.
  - Date/source: operator's explicit reply to the reconciliation scope question, 2026-11-17.
- **D9:** Evaluate the final PASS review and apply valid code/documentation corrections.
  - Effect: verify the review against current files and evidence, then close the task under its existing completion criterion. No new lighting/daylight implementation, renderer tuning or release certification is included.
  - Reason: not stated.
  - Date/source: operator's final review-and-fix request, 2026-11-17.

### Operator gates

Engineering receipts alone do not establish visual acceptance. The accepted
combined default retains the 0.012 source warp and native shared phase variation;
the warp alone is not claimed to solve tiling. Candidate 2's roughness override
was disposable and restored. Accepted roughness variation remains 0.18.

D6's additional isolated candidate has been tested and restored. All three
model slots are used. D7 authorizes integration of that candidate; no fourth
model or release promotion is authorized. The integrated result is now qualified
and accepted by final review; residual recurrence alone does not reopen the task.

Locality preflight: `ProjectMaterialEditor.Build.cs` fingerprints all compiler
sources and schemas. `ComputeArtifactSemanticIdentity` includes that global
fingerprint for every recipe, and `IsAcceptedSkip` requires exact identity.
Changing only the road graph therefore invalidates all ten production surface
records. The current wrapper has no selected-road merge mode. D8 authorizes one
bounded reconciliation through that owner after isolated proof. Preserve unrelated
behavior and retain compiler authentication; no fingerprint suppression or manual
manifest migration is permitted.

- **Q1 [CLOSED by D6]:** Authorize one additional isolated native sampling candidate, or stop experiments with current defaults retained? D3's two-candidate ceiling is exhausted. This gates a third model only, not evidence preservation or cleanup.

### Working assumptions

- **A1 [CLOSED]:** Road first; concrete received only affected-consumer regression checks, with no appearance change.
- **A2 [RESOLVED by D3]:** Proposed budget: one diagnostic/control batch, then at most two candidate fixes total, selected from the diagnosis. Stop early on clear success; otherwise retain defaults and record the residual defect. This is a proposed implementation bound, not an existing operator limit.

## Non-goals

Water changes, Terrain correction, Histogram/exposure changes, building facades, new material families, a second PBR layer, general texture framework, global renderer tuning, World regeneration, source activation, packaging or release acceptance.

## Current failure and next discriminator

This section preserves the earlier diagnostic history. D7/D8 selected and
integrated the visually preferred result; the optional unresolved source-period
questions below do not block that disposition.

**Do not repeat the prior acceptance error:** changing a source tile's grains does not break repetition of that tile, and neither source hashes nor shader cost establish natural appearance. The operator-rejected lit output is a mandatory known-bad case. Previous source/manifest/native lifecycle checks remain engineering evidence only.

Offline inspection of the original native PNGs confirms repeated directional structure before and after. A luminance residual, windowed 2D autocorrelation and overlap-normalized patch correlation find the same horizontal recurrence peaks in three road-only screen strips:

| Native screen strip (x=1024, width=768, height=192) | Repeat offset before | Repeat offset rejected-after | Patch correlation before / after |
| --- | --- | --- | --- |
| y=200 | 37 px | 37 px | 0.350 / 0.336 |
| y=328 | 43 px | 43 px | 0.327 / 0.318 |
| y=456 | 50 px | 50 px | 0.265 / 0.248 |

These are diagnostic screen-space measurements, not a universal pass threshold or a measured world-space tile size. Perspective changes the spacing with depth. The same peaks and their harmonics remain; the prior warp edit does not solve the reported visual defect.

`tmp/world/material_repetition/detection/` contains `probe.py`, `controls.py`, `screen-recurrence.json`, `channels.json`, `controls.json` and `recurrence-before-after.png`. The scripts run with the already-installed Python 3.11 NumPy/Pillow environment; no package was installed. Controls detect known-period stripes and their low-contrast variant, distinguish white and matched-contrast correlated nonperiodic noise negatives, classify a flat input as insufficient signal, and expose lost fine detail in a deliberately blurred image. A separate held-out road region at x=1920, y=400 retains a 52 px peak before and after (patch correlation 0.287 / 0.277); see `holdout-controls.json`. This calibrates the instrument on those cases; it does not prove aesthetic correctness or a general production gate. No tracked test may depend on this scratch evidence.

The 37 px recurrence is stronger in Lit than in the existing BaseColor/WorldNormal captures. Those offline channel comparisons alone did not localize the contributor. The subsequent native ablations below identify the dominant roughness contribution; the source-versus-footprint/sampling mechanism still needs qualification. The warp changes height/pigment, while roughness/macro B bytes were unchanged. The earlier claim that raw grains fully classified the visible failure is retracted.

- [X] Preserve and measure both rejected outputs; demonstrate recurrence survives the previous fix. No new material/source changes or Unreal generation for this offline pass.
- [ ] Extend the measured controls/held-out ROI checks to additional camera angles and perspective-aware or fixed world-metric views. Retain original normal-render images; contrast-enhanced residuals and correlation plots are diagnostic only. Search multiple offsets/scales so replacing one grid with another cannot pass. Flat/blurred/lost-detail output must never count as success.
- [X] Use the existing native scalar preview for one cheap Lit ablation batch: constant roughness (`RoughnessVariation=0`, `MacroRoughness=0`) versus normal-off (`ReliefStrength=0`), plus baseline and rendered revert. A flat-channel diagnostic that hides bands localizes a contributor; it is not an adopted material. Reuse the saved map/known camera/Ready textures and the documented optional-bridge opt-out; no World replay, full-city test or source regeneration.
- [ ] Tie the surviving bands to source/tile versus footprint/filtering using a fixed plane or authenticated world-coordinate projection and camera/scale controls. Do not assume the measured screen period equals the 0.32 m source period.
- [X] Test the remaining approved scalar mitigation and D6's one additional native sampling candidate. Both reduce recurrence without eliminating the reported structure; neither was adopted. Preserve original sources and strict production graph verification. The current evidence does not qualify a world-space period or an alternative production graph.

Research references: [periodicity from autocorrelation peaks](https://doi.org/10.1016/S0167-8655(97)00030-5), [Epic's native bombing notes and normal handling](https://forums.unrealengine.com/t/texture-bombing/65652), [Practical Real-Time Hex-Tiling](https://jcgt.org/published/0011/03/05/paper-lowres.pdf), and the authors' [histogram-preserving tiling/blending demonstration](https://unity-grenoble.github.io/website/demo/2020/10/16/demo-histogram-preserving-blend-synthesis.html). Hex/stochastic blending offers a researched fallback direction, not a selected ALIS implementation; blending can lose contrast and needs coordinated normal treatment. Prefer a proven installed engine route before custom code.

### Attempt 2: evidence-led roughness mitigation, not adopted

One serialized native saved-Kazan session produced twelve Lit diagnostic captures: baseline, constant roughness, normal-off, flat pigment, all-flat and revert, including top-down and grazing-angle controls. At the first known-bad strip, baseline recurrence correlation was 0.348; constant roughness 0.080; normal-off 0.346. This localizes the dominant contribution to roughness response rather than normal relief. These ablations are not candidate replacements.

The second fix candidate changed only `RoughnessVariation` from 0.18 to 0.045 in a disposable authenticated MID preview. Broad roughness, pigment, relief, source textures, projection, filtering, Water, Terrain, concrete and Histogram were unchanged. The original material was restored after evaluation.

| Known-bad ROI | Baseline correlation | Candidate 2 correlation | Restored correlation | Retained fine-frequency image RMS |
| --- | --- | --- | --- | --- |
| x=1024, y=200; lag 37 px | 0.343 | 0.149 | 0.358 | 91.1% |
| x=1024, y=328; lag 43 px | 0.324 | 0.131 | 0.344 | 95.6% |
| x=1024, y=456; lag 50 px | 0.242 | 0.091 | 0.260 | 97.3% |
| Held-out x=1920, y=400; lag 52 px | 0.277 | 0.098 | 0.295 | 96.1% |

The candidate weakens the bands without globally blurring the surface, but recurrence at the old offsets remains. Fine-frequency RMS is a proxy, not an assertion that all perceived detail is preserved. Do not declare anti-tiling PASS or promote this override based on lower correlation alone. Both model slots are now used; another fix requires Q1 rather than relabeling a new method as the same attempt.

Evidence under `tmp/world/material_repetition/iteration2/`:

- `receipts/ablation.json` and `ablation-analysis.json`: matched Lit controls, Ready textures and selected bindings.
- `receipts/candidate2-images-authenticated.json`: nine 2560x1440 captures; four matched near/middle/far/top-down pairs and a rendered baseline restore, with actual runtime roughness parameter values.
- `receipts/candidate2-analysis.json`: three original strips plus the held-out ROI, old-lag correlation, new peak search and fine-detail proxy.
- `receipts/candidate2-metrics.json`: five 180-frame samples. Near GPU p95 8.5624 -> 8.4496 ms; far 11.2304 -> 11.1551 ms; near restore 8.4385 ms. Shader graph unchanged. These Editor samples do not certify the 16.67 ms whole-game target.
- `receipts/candidate2-motion-authenticated.json`: matched 90-frame native grazing-motion captures for baseline/candidate, with the normal capture cap, not performance claims.
- `captures/candidate2-close-comparison.png`: normal-render 1:1 crops, baseline left/candidate right. Full near/far captures and both motion clips remain alongside it.

No C++ change, source/material regeneration or new unit test was needed for this non-adopted scalar preview. Prior owner authentication tests still describe unchanged code; the fresh visual evidence remains PATCH. D6 subsequently authorized attempt 3 below. Do not spend another model slot on grain warp or another amplitude-only adjustment.

### Attempt 3: shared native sampling, integrated under D7/D8

D6's one additional model used the installed UE 5.8.3 `TextureVariation`
function on an unsaved duplicate of the existing aggregate parent. Its single
Shifted UVs output feeds both fine Signals and Normal samples. Both use the
function's clean DDX/DDY outputs with derivative sampling; the broad macro
sample remains unchanged. Rotation/scale, temporal dithering and HQ edge
comparison are disabled. Other native function defaults are retained. Original
`RoughnessVariation=0.18`, relief, palette, grain, world scale and source textures
are retained. The duplicate binds only an unsaved copy of the selected actual
road mesh and transform. This is exploratory rendering evidence, not an
authenticated compiler output. Production verification was not modified.

| Known-bad ROI | Baseline correlation | Candidate 3 correlation | Restored correlation | Fine-frequency RMS fraction |
| --- | --- | --- | --- | --- |
| x=1024, y=200; lag 37 px | 0.348 | 0.146 | 0.334 | 98.8% |
| x=1024, y=328; lag 43 px | 0.337 | 0.146 | 0.328 | 100.4% |
| x=1024, y=456; lag 50 px | 0.252 | 0.114 | 0.251 | 98.5% |
| Held-out x=1920, y=400; lag 52 px | 0.283 | 0.134 | 0.279 | 98.4% |

Historical agent disposition before the operator's subsequent visual preference
and D7: the long bands become less coherent, but peaks remain at nearly the same
spacing. The peak search includes other directions/offsets; this is not a
single-lag or universal aesthetic gate. Fine-frequency RMS is an image proxy,
not proof of normal fidelity or natural appearance. The candidate is PATCH,
not an anti-tiling PASS, and no production material is adopted.

Evidence under `tmp/world/material_repetition/iteration3/`:

- `receipts/native-function.json`, `graph-probe.json`, `function-routes.json`
  and `candidate3-graph.json`: installed function pins/defaults, shared routes,
  sampling modes, original roughness and zero final compile errors. Intermediate
  wiring compiled before DDX was connected and produced missing-DDX warnings;
  the completed graph compiled without errors. Setup API corrections are not
  additional model attempts.
- `receipts/candidate3.json` and `candidate3-images-authenticated.json`: fifteen
  2560x1440 Ready-runtime captures, matching numeric camera poses, Lit at near,
  middle, far and steep downward angles, WorldNormal/Roughness/BaseColor pairs,
  and rendered baseline restoration. The camera clamps the downward view to
  -79 degrees; the historical `topdown` key does not mean an orthographic view.
- `receipts/candidate3-analysis.json`: original strips, held-out region,
  strongest recurrence search and retained-detail proxy. Source/tile physical
  period and fixed world-metric reproduction remain unqualified.
- `receipts/candidate3-metrics.json`: eleven 180-frame samples. Lit near GPU
  p95 8.6071 -> 8.5938 ms, far 10.3430 -> 10.1351 ms, restored near 8.1515 ms.
  These noisy Editor comparisons do not prove a speedup or packaged performance.
  Native shader statistics increase from 242 to 276 pixel instructions, 6 to 9
  reported pixel texture lookups and 3 to 5 samplers.
- `receipts/candidate3-motion-authenticated.json`: 89 baseline and 90 candidate
  saved frames of a native 1 m grazing-camera movement. Ninety were requested
  per arm; report actual saved counts. Clips use the 30 fps capture cap and
  are not performance evidence. No temporal dither is enabled; full temporal
  artifact acceptance remains unproven.
- `captures/candidate3-close-comparison.png`, `candidate3-far-comparison.png`,
  `candidate3-normal-comparison.png` and `candidate3-roughness-comparison.png`:
  baseline left / native candidate right, 1:1 crops with no contrast adjustment.
  Full source images and baseline/candidate motion MP4s remain alongside them.
- `receipts/final.json`, `final-pie-ended.json`, `session/native-exit.json`:
  PIE ended, no fixture actor remained, native exit 0. Direct comparison of
  all 3,756 censused data/output files before/after found zero changes. World
  packages, canonical data, Material/Texture outputs and manifests were retained.
  No source acquisition, regeneration, production compiler edit or Git mutation.

The experiment budget stops at 3/3. The following earlier source-forensics
recommendation is optional under D7, not a prerequisite to integrating the
preferred result: distinguish directional
structure within the roughness source from repetition of its whole tile using
raw B-channel repeats and a qualified fixed-metric view. Do not repeat grain
warp, amplitude-only mitigation or assume a shared non-rotating shift removes
source-internal structure. That is a proposed next discriminator, not approval
for another experiment. Adjacent-section, generated-graph authentication and
save/reload adoption tests are deferred because this visual candidate failed;
unchanged-owner tests from attempt 1 are not proof for this experimental graph.
The D7 integration evidence below supersedes those deferred engineering checks;
it does not spend a fourth model slot.

## Verified evidence

- The operator likes the improved distant road and reports regular close detail. The supplied screenshot alone does not establish a UV seam, source lattice or filtering defect.
- The [asphalt recipe](../../../Plugins/Resources/ProjectMaterial/Data/SurfaceRecipes/Aggregate/MI_ProjectAsphalt_Default.surface.json) uses 0.32 m detail, 32 m macro variation and the adopted `MacroContrast=0.10`; keep that improvement as the baseline.
- [Aggregate projection](../../../Plugins/Resources/ProjectMaterial/Source/ProjectMaterialEditor/Private/ProjectMaterialAggregateSurface.cpp) samples Signals and Normal in world XY. Existing macro variation does not randomize each fine-detail repeat.
- The [road pattern](../../../Plugins/Resources/ProjectTexture/Data/Patterns/Aggregate/project_road_aggregate.pattern.json) produces a periodic 512x512 Signals/Normal bundle. Its [builder](../../../Plugins/Resources/ProjectTexture/Source/ProjectTextureEditor/Private/ProjectTextureAggregateBuilder.cpp) creates wrapped outputs with automatic mips and explicit trilinear filtering.
- Road source controls include `FineFrequency=96` and `MicroFrequency=192`; attempt 1 changed `WarpAmplitude` from 0.003 to the current working 0.012. Their periodic structure is a possible cause, not proof of a visible lattice.
- Installed UE 5.8.3 includes `TextureVariation`, `Texture_Bombing` and the beta BaseMaterial `MF_Coordinate_CellBombing`. Availability does not prove visual suitability or affordable shader cost.
- Installed `TextureVariation.uasset` describes shifted UVs, optional temporal dithering, height-based selection and explicit derivatives from unmodified UVs. Its rotation warning supports starting without rotation; serialized descriptions do not establish the final compiled graph or quality.
- `ProjectMaterialAggregateSurface::Verify` admits two Signals plus one Normal sample for world XY, or six Signals plus three Normal samples for triplanar projection, and checks connected routes. Three versus nine expression nodes is not a measured GPU-cost ratio. A structural trial must preserve authentication of its selected graph rather than bypass these checks.
- UE source routes explicit texture filtering through `TextureLODSettings.cpp::GetSamplerFilter`. `HLSLMaterialTranslator.cpp` can use analytic gradients for mip-bias sampling: the enum/header comment alone cannot prove the effective anisotropy of this graph.
- Historical soil trials exposed seams or softened relief with UV variation/bombing; they do not prove asphalt will fail. Previous road mip-bias softening was rejected. Do not repeat a generic blur trial.
- Existing scratch evidence under `tmp/world/material_tiling/` can help reproduce the camera setup; it is optional diagnostic history, never a tracked fixture dependency.

Primary references: Epic [texturing functions](https://dev.epicgames.com/documentation/unreal-engine/texturing-material-functions-in-unreal-engine), [mip sampling modes](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/ETextureMipValueMode), and [native Texture Bombing discussion](https://forums.unrealengine.com/t/texture-bombing/65652).

## Current architecture and source of truth

ProjectTexture owns generated source structure, filtering and the shared Signals/Normal output contract. ProjectMaterial owns projection, sampling, shading and surface recipes. World consumes their authenticated assets; it does not own this correction.

Use existing [material generation and isolated fixtures](../../../Plugins/Resources/ProjectMaterial/docs/material_generation.md), [aggregate bundle](../../../Plugins/Resources/ProjectTexture/docs/pattern_generation.md#aggregate-bundle), [material wrappers](../../../scripts/ue/material/README.md) and [native editor lane](../../../docs/ue_engine/mcp_editor_control.md). Scratch belongs under `tmp/world/material_repetition/`, with separate captures and receipts.

## Problem and root cause

**PARTIALLY LOCALIZED:** the native Lit controls identify fine roughness response as the dominant contributor. Reducing its amplitude mitigates the bands, but visible periodic structure and old-lag correlation remain. The remaining source/footprint/sampling mechanism is not fully qualified. Raw grain changes and lower shader cost do not establish a fix. Close camera distance does not guarantee mip 0; texture mip selection is independent of mesh LOD.

## Decision

Classify the defect on one faithful road patch, then change its smallest existing owner. Another macro tint layer is not the default remedy for a repeated fine pattern. If whole-tile repetition is confirmed, `TextureVariation` is the preferred first native comparison, not a proven winner.

Use its coordinated shifted UVs and clean-UV derivatives for Signals and Normal, initially without random rotation or flipping. Where height-based selection/blending is used, derive shared decisions from the existing Signals height so channels do not select different surface fragments. Verify decoding against ALIS's linear encoded Normal contract. Inspect temporal artifacts under the actual anti-aliasing mode.

### Premise / KISS gate

Retain the existing two-output aggregate bundle and generation lifecycle. Prefer a source-control adjustment or local sampling correction when it resolves the measured defect. Only compare native UV variation if whole-tile repetition remains the demonstrated cause. Add no service, manager, cache, plugin architecture or additional PBR bundle. Accept that a finite tiled source cannot promise zero repetition at every scale.

### Alternatives considered

- **Source-pattern controls:** simplest for a grid within the source; compare only implicated existing frequency, warp or occupancy controls, regenerating the coordinated pair. This cannot eliminate whole-tile periodicity.
- **Local filtering/gradient correction:** suitable for demonstrated angle-dependent aliasing; does not randomize a recognizable source motif.
- **Native UV variation:** prefer `TextureVariation` for the first whole-tile comparison. Compare `Texture_Bombing` only if a simpler candidate fails and A2 has a candidate remaining; inspect native normal handling and actual shader cost. Do not apply either globally.
- **Distance-based blending:** conditional only if the remaining defect demonstrably depends on distance. The accepted far-road improvement alone does not justify another blend or detail layer.
- **Extra randomization texture:** consider only if the chosen native route requires it and measured benefit justifies its memory, sampling and ownership cost.

## Required invariants

1. Preserve Water, Terrain, exposure, canonical/source selection and unmodified World outputs. No World Apply, candidate activation, Matrix or city rebuild for material discovery.
2. Preserve distant road improvement, near grain, normal response and plausible roughness/color. No global mip bias, mip disabling or texture-group changes to hide the symptom.
3. Keep world-space continuity across adjacent road sections and World-cell boundaries. Coordinate Signals/Normal UVs and selection/blend weights; rotated/flipped normals require correct reorientation and coherent derivatives. Leave triplanar defaults unchanged unless an affected-consumer regression requires a correction.
4. Retain authenticated generation, deterministic identities, shared runtime outputs, save/reload, warm no-op and refusal-without-mutation behavior.
5. Use one serialized rendering Editor lane. Confirm the expected rendering Editor is ready before MCP status calls; a NullRHI commandlet cannot provide visual evidence. Do not bypass session safety checks.

## Implementation tasks

- [X] **Baseline:** record selected component/material, effective recipe, texture Ready state, resolution, quality and exposure. Use a saved road or faithful isolated plane, without World regeneration. Capture top-down and player-eye/grazing views at near, medium and far distances plus a short matched motion segment.
- [X] **Source/channel controls:** inspect a raw 2x2 repeat of Signals and Normal; compare BaseColor, WorldNormal, flat-relief and macro-disabled controls. A positive control must visibly change the intended surface. Reject wrong framing, unready textures or cached mismatched views.
- [X] **Classify:** separate source-internal structure from whole-tile repeats and angle-dependent filtering. Verify effective runtime sampler/compiled sampling where filtering is implicated; do not infer it only from asset enums.
- [X] **Selected fix:** the operator prefers candidate 3 and D7 authorizes its integration. Residual correlation alone is not grounds for another artistic experiment; no fourth model is authorized.
- [X] **Regression:** extend existing production-shaped fixtures only for changed machine-provable behavior. Demonstrate failure before fixing production logic; reuse existing lifecycle tests rather than adding scalar-value mirror tests. For a graph change, update its recipe/compiler identity and connected-route authentication together; prove a mismatched or tampered variant refuses. Keep the unselected graph authenticated, not permissively accepted by relaxing sample-count checks.
- [X] **Native comparison:** capture matched baseline/candidate/restored near and far views, grazing-angle motion, shader lookups/instructions/samplers and repeated GPU timings. Adopt only a clear visual improvement without an unexplained cost increase.
- [X] **Adoption:** candidate 3 reproduced through the compiler, saved/reloaded, locally adopted under D8 and cold-loaded as the actual default. Owner authentication, no-op, controlled recovery, adjoining sections and actual Hero walking are recorded below.
- [X] **Bounded handoff:** preserve the comparisons, measurements, cleanup and stable-owner lessons. Final implementation review accepted the slice; no additional artistic model or release claim.

## Test-first and verification plan

### Red evidence

| Case | Current wrong behavior / proof to establish |
| --- | --- |
| Close, top-down and grazing road | Reproduce the reported regular pattern in matched normal rendering; isolated views classify its cause. |
| Motion and distance | Establish whether the pattern crawls/aliases and retain the accepted distant baseline. |
| Source/sampling contract, if changed | Add a focused failing assertion for the demonstrated contract gap, not subjective attractiveness. |
| Shared projection, if changed | Reproduce any channel misalignment, normal rotation error or seam on a small boundary fixture. |
| Adjacent road sections | Keep continuous sampling across separately generated sections, including a cell boundary. |
| New graph variant, if needed | Reject wrong UV/gradient/normal routes and mismatched recipe identities after native save/reload. |

### Green evidence

- Repeat the same visual cases and controls; operator/reviewer judges appearance from matched normal-render images. Hashes and shader statistics alone do not prove visual quality or GPU cost.
- Reuse `Project.Material.Asphalt.AuthenticatedSample` for material realization/authentication. If source generation or filtering changes, also use `Project.Texture.Aggregate.AuthenticatedSource` and extend its relevant contract assertion where needed.
- Dispatch each exact test separately through `scripts/ue/test/unit/iterate.ps1 -TestFilter <exact-name>`; use a real rendering RHI for image evidence. See [testing layers](../../../docs/testing/world_pipeline_layers.md).
- Report frame/GPU distributions with the same resolution, scalability, camera and settled exposure, retaining the 16.67 ms frame target as context. If the baseline misses it, classify the incremental material cost separately; an isolated patch cannot certify whole-game performance. Do not claim installed-game/R5 performance from an Editor sample.
- Verify rendered revert and clean PIE shutdown, then cold-load the adopted default without a preview MID. Confirm protected assets, source selection and Git index remain unchanged.
- Recipe-only changes need applicable parsing/authentication and native consumer proof; code changes additionally need focused automated regression and the documented build route. No full-city acceptance gate for this isolated task.

## Documentation plan

Update only the applicable stable material/pattern owner with the selected mechanism, measured limits and reproducible checks. Add a pitfall only for a verified reusable failure. Existing routers suffice; no new documentation hub, duplicate lighting plan or stable-doc link to this todo.

## Rollout and rollback

Preview or generate under existing isolated fixture mounts first. Promote only reviewed affected recipe/asset/manifest outputs through the existing generation wrapper. Restore a rejected preview or generation through its owner lifecycle, preserving unrelated dirty work. New production authority, release promotion and Git operations remain outside this task.

## Completion criteria

- Cause classified; one bounded fix adopted with matched visual/runtime evidence, or candidates rejected with accepted defaults retained and an explicit residual disposition. Retention alone does not mean the visual defect was fixed.
- Required invariants and affected consumer checks hold; report exact changed outputs and actual investigation/regeneration costs.
- Verified durable knowledge lives with its owner. Move this task to done only after implementation review accepts the outcome; otherwise record the remaining decision or scoped follow-up.

## Review record

- 2026-11-17 final review: PASS. Current source, the ten package SHA-256 values,
  one consistent compiler fingerprint, retained no-op/validation identities,
  90-frame final walking capture, 104/104 grounded samples and clean native exits
  agree with the submitted evidence. No new runtime tests or renderer changes
  were required for documentation closure. The reviewer could not independently
  execute the native tests; that limitation remains recorded rather than becoming
  a new native run or release certification.
- Filtering qualification: Epic's mip-mode documentation describes API support,
  not this material's effective filter. Aggregate outputs retain trilinear
  filtering. The installed translator's analytic-derivative branch can emit
  gradient sampling for MipBias, so "MipBias always disables anisotropy" is too
  broad, and adopting derivatives is not proof that anisotropy was enabled.
  `UTextureLODSettings::GetSamplerFilter` and
  `FHLSLMaterialTranslator::TextureSample` own those distinct decisions. The
  material SOT records this qualification; the accepted result relies on shared
  shifted coordinates, clean gradients and rendered evidence. Lighting's existing
  exposure-clipping observation and optional daylight decision remain in their
  current owner; this closure does not authorize executing them.
- 2026-11-17: plan prepared from repository/installed-engine inspection and primary references. Root cause and proposed fixes remain unverified. No new runtime/material implementation is claimed by this planning change.
- 2026-11-17 review correction: verified source controls, native function descriptions and three/nine sample-node admission in actual code. Made non-rotating `TextureVariation` the first conditional whole-tile trial and bombing the bounded fallback; strengthened shared selection, boundary and graph-authentication proof. The best visual technique and comparative GPU cost remain unverified. The reviewer's optional concrete appearance trial is deferred under A1; shared-owner regression remains required. No production defect was demonstrated that requires changing the current graph before these trials.
- 2026-11-17 implementation: one diagnostic batch and one model candidate used. Source-only correction selected; shader branches, concrete changes and new graph regressions were unnecessary. Final integration review checked generated manifests, source identities, normal alignment, locality and stable documentation. Independent artistic judgment can use the matched captures below; no installed-game or release PASS is claimed.
- 2026-11-17 operator rejection (D4): visible diagonal repetition remains in both reported images. Previous visual PASS and DONE were premature and are withdrawn. Offline recurrence probes and controls confirm the surviving pattern; the task returned to current. Preserve engineering evidence but do not reuse it as artistic acceptance. No additional source/material behavior was changed during this investigation.

## Result and evidence

### D7/D8 integration and local adoption

The existing XY aggregate owner now binds one installed non-rotating native
TextureVariation call to both fine Signals and Normal samples. Clean derivatives
are shared; the existing mip-bias control scales them coherently. The macro
sample and triplanar builder remain unchanged. The private aggregate variation
helper owns construction and verification; no new schema, material family,
generic sampler framework or second generator was introduced. The compiler's
existing source fingerprint authenticates the changed implementation.

Source disposition: retain the tested 0.012 road warp as part of this combined
source/material result. It is not claimed to fix tiling alone. The source recipe,
catalog and texture manifest were unchanged during integration. Roughness
variation stays 0.18, relief stays 0.7 and the effective road macro contrast stays
0.10. Remaining statistical recurrence is not an automatic rejection under the
operator's visual preference and D7.

The review's F1-F3 findings were valid. Its road-only regeneration instruction
was not executable through the current shared compiler fingerprint and complete
manifest reconciliation. D8 explicitly authorized all ten surface outputs;
the implementation neither bypasses authentication nor adds a selective writer.

Evidence is under `tmp/world/material_repetition/integration/`:

| Check | Result | Evidence |
| --- | --- | --- |
| Missing native route, RED | Saved graph contained 0 rather than 1 native variation call | `receipts/test-red-native.log` |
| UV-swizzle admission, RED | One-axis normal UV mask was wrongly accepted | `receipts/swizzle-red-native.log` |
| Editor build | PASS through the project build wrapper | `receipts/build-final.log` |
| `Project.Material.Asphalt.AuthenticatedSample` | PASS after both regressions; wrong UVs/swizzles, swapped derivatives, wrong normal/height textures, altered settings and rotation refuse | `receipts/test-final-native.log` |
| `Project.Material.Aggregate.TriplanarContract` | PASS; untouched projection behavior retained | `receipts/test-triplanar-native.log` |
| Isolated generation and no-op | 2 generated / 1 parent compile, then 0 generated / 2 skipped | `receipts/generate/`, `receipts/noop/` |
| Injected post-save failure | Rejected; exact prior fixture packages/manifest restored; restored validation accepted | `receipts/recovery.json`, `receipts/failed-apply/`, `receipts/validate-restored/` |
| Authorised production reconciliation | 10 generated / 5 parent compiles | `receipts/production-adopt/Current/` |
| Fresh production no-op and Validate | 0 generated / 10 skipped; identical output/manifest digests | `receipts/production-noop/Current/`, `receipts/production-validate/Current/` |
| Cold default consumption | 17 road sections bind the generated constant default, no preview MID | `production-native/receipts/production.json` |
| Boundary | Two actual adjoining sections inspected; shared-vertex maximum delta 0.00394 cm, no new visible phase break | `native/receipts/adjacent-sections.json` |
| Normal positive control | Flat-relief control changes the actual normal buffer; mean absolute difference 0.222 RGB codes, max 3 | `native/receipts/normal-control-analysis.json` |
| Actual Hero walking | Final default: 90 saved frames, 104/104 grounded samples, 16.89 m sampled displacement | `production-native/receipts/walking-summary.json`, `production-native/captures/final-default-walking.mp4` |
| Restoration | Both rendering sessions exit 0; PIE absent and no fixture actor remains | both native session/cleanup receipts |
| Locality | Only ten Material packages and their accepted manifest changed among 3,756 censused data/output files | `receipts/production-delta.json`, `receipts/final.json` |

The first integration build needed test-local naming corrections. A later native
input convenience API was not exported by the installed Engine; the verifier
uses the public expression outputs instead. One test accidentally launched after
that failed link and exercised stale binaries; its `swizzle-stale-*` logs are not
GREEN or new RED evidence. The final build and final cold test above supersede
those setup failures. The actual regression REDs are the two explicit native
assertion failures in the table.

The matched integrated fixture closely reproduces the preferred prototype:
old-lag correlations are 0.145, 0.149, 0.109 and 0.128, versus prototype 0.146,
0.146, 0.114 and 0.134. These are diagnostic proxies, not aesthetic thresholds.
Source-period attribution remains optional; no additional model was tried.

Shader statistics are 279 pixel instructions, 9 reported lookups and 5 samplers,
versus 242/6/3 before. The three instructions above the experimental graph retain
the existing mip-bias control. Matched fixture GPU p95 is 7.7579 -> 7.9344 ms near
and 9.6050 -> 9.5586 ms far; these noisy samples establish no speedup. Final cold
all-default GPU p95 is 8.8635 ms near and 10.2672 ms far. Those are separate
session measurements, not a paired whole-world comparison. Editor total-frame
p95 exceeds 16.67 ms; installed-game/R5 performance remains unverified.

All ten recipe identities and consumed source identities are unchanged by
reconciliation, including Water's native-resource hashes. Fresh concrete views
retain its original parameters and contain no variation call. Water/Terrain
behavior and recipes are retained; their packages were rewritten under D8.
Canonical/World outputs, Histogram and Texture outputs are byte-unchanged. Concrete's
pre-existing repetition is not fixed or newly accepted by this road change.

Final visual handoff: `production-native/captures/final-close-before-after.png`
and `final-far-before-after.png` show saved baseline left / cold generated default
right, at 1:1 crop scale. Full cold road views, concrete near/far views and final
walking footage remain alongside them. The boundary and normal-control captures
are in `native/captures/`. All scratch paths are execution evidence, never tracked
test inputs. Stable ownership and verification lessons are in the material SOT.

The isolated walking script originally labelled a requested 60 cm/s speed as an
effective cap. Its retained positions show about 16.9 m travel; the annotation was
corrected without changing sensor traces. The final default run uses native
gameplay speed policy directly. Capture rate caps are not FPS measurements.

No World generation, new pattern, building change, staging, commit, publication
or release promotion was performed by the agent. Final implementation review is
PASS; the original completion criterion is satisfied and the task is closed.

### Historical attempt-1 record

**Prior attempt record, superseded for visual acceptance by D4.** Preserve the measured engineering facts below; withdraw the interpretation that this completed anti-tiling. The working 0.012 warp/source assets have not been reverted or changed during this diagnosis. Current failing evidence and unchecked work are above.

- **Fix:** Road `WarpAmplitude` 0.003 -> 0.012 through the existing ProjectTexture recipe. Frequencies, tile scale, filtering, material graph and accepted `MacroContrast=0.10` remain unchanged. Stronger existing placement warp reduces the aligned fine-grain rows; no native stochastic sampler or additional shading layer was needed. One of two candidate slots used; stopped on the first useful result.
- **RED:** `tmp/world/material_repetition/receipts/baseline.json` authenticates ten 2560x1440 lit/channel/control captures and Ready outputs. `captures/baseline-signals-2x2.png` shows source-internal rows. Macro-disabled BaseColor retains the detail pattern; the flat-relief WorldNormal control visibly removes the bump on the selected road. This is visual red evidence, not an invented scalar-value unit failure.
- **Candidate:** `receipts/candidate1-images-authenticated.json` retains twelve matched captures across near, middle, far, top-down, BaseColor and WorldNormal. The fixture used an unsaved copy of the actual selected road mesh/transform. Shader statistics stayed at 242 pixel instructions, six reported texture samples and three samplers. A normal unsaved Editor actor was needed for PIE duplication; a transient-only actor was absent in the game world. Setup corrections did not consume additional model attempts.
- **Channel locality:** `receipts/source-locality.json` compares all 262,144 source pixels: height and pigment changed, all roughness/macro B bytes stayed identical, and the derived Normal changed coherently. Both cold adopted outputs match the candidate's complete RGB pixels exactly. The shared source, unchanged world-XY graph and native wrap contract retain section continuity without per-cell random state.
- **Native adoption:** `receipts/adopted.json` records six cold, normal-route captures, no preview MID, Ready Signals/Normal and seventeen saved road sections sharing the accepted constant instance. `receipts/adopted-motion-authenticated.json` retains a 90-frame grazing-motion clip; the candidate comparison also retains matched 90-frame baseline/candidate clips. Native viewport size was 2560x1440, High settings, 87% screen percentage, TSR and unchanged Histogram policy. Motion clips use the documented native 30 fps capture cap and are not FPS benchmarks.
- **Performance:** `receipts/trial1-metrics.json` holds eight 180-frame samples. Far GPU p95 was 10.1278 ms baseline versus 10.2277 ms candidate; middle was 10.2798 versus 10.1524 ms. Cold adopted far GPU p95 was 10.2336 ms. These measurements and unchanged shader work show no demonstrated material GPU regression. Editor total-frame p95 exceeded 16.67 ms already in baseline; whole-game performance remains outside this pass.
- **Protection:** `receipts/final.json` checks 3,756 protected paths. Only the Road recipe, its 56 KB catalog and the Texture/Material accepted manifests changed. All World packages/canonical/manifests, concrete, Water, Terrain, exposure, material packages and other texture bundles stayed byte-identical. No World generation, source activation, C++ change, staging or commit occurred.
- **Shutdown:** the candidate session hit the documented optional McpAutomationBridge pure-virtual shutdown failure. Its images remain diagnostic, not a successful process receipt. The replacement cold session disabled only that optional bridge for the process, retained BlueprintMCP/rendering, ended PIE, removed no production actor and exited 0. `session-final/native-exit.json`, `receipts/final-pie-ended.json` and unchanged final hashes prove cleanup. Earlier helper-commandlet startup blocked the generation wrapper before writes; only the agent-started helper was stopped, with no process guard bypass.

Exact verification commands and retained receipts (all paths below are under `tmp/world/material_repetition/`):

| Check | Result | Evidence |
| --- | --- | --- |
| `scripts/ue/test/unit/iterate.ps1 -TestFilter Project.Texture.Aggregate.AuthenticatedSource -CompileMode None -TimeoutSeconds 300` | 1/1 PASS; 84.5 s | `receipts/tests/texture-authentication.log` |
| `scripts/ue/test/unit/iterate.ps1 -TestFilter Project.Material.Asphalt.AuthenticatedSample -CompileMode None -TimeoutSeconds 300` | 1/1 PASS; 72.5 s | `receipts/tests/material-authentication.log` |
| `scripts/ue/texture/run_pattern_generation.ps1 -Mode Regenerate -EvidencePath <evidence>` | 1 generated / 3 skipped | `receipts/adopt-pattern/Current/host.receipt.json` |
| Same texture command, fresh process | 0 generated / 4 skipped; identical output/manifest digests | `receipts/noop-pattern/Current/host.receipt.json` |
| `scripts/ue/material/run_surface_generation.ps1 -Mode Regenerate -LayoutReceipt <existing-layout> -EvidencePath <evidence>` | 0 generated / 10 skipped; provenance refresh only | `receipts/adopt-material/Current/host.receipt.json` |
| Same surface wrapper with `-Mode Validate` | 10 validated, zero compiles; normal exit | `receipts/validate-material/Current/host.receipt.json` |

The four generation/validation commandlet log spans were 13.657, 12.484, 13.431 and 12.391 seconds respectively; these exclude host/startup overhead. Zero surface rebuilds/compiles does not assert that the source generator needed no shader/DDC work. Existing focused tests cover connected controls, source tampering/refusal, cache reuse and preview restoration. No new scalar-mirroring test was added for this recipe-only visual fix. Native owner parsing/authentication and cold rendering qualify the actual production recipe.

Visual handoff: `captures/final-close-before-after.png` and `captures/final-normal-before-after.png` show baseline left / adopted right, with identical 1024x512 crops at x=1024, y=200 and no resampling. Full views remain in `captures/trial1-*-baseline-lit.png` and `captures/adopted-*-lit.png`; `captures/road-adopted.mp4` is the final cold motion clip. Raw images, metrics and scripts are optional diagnostic history, not clean-checkout test inputs.

Historical remaining defect after attempt 1: the operator-rejected gameplay view
still showed repeated bands. D7/D8 supersede that disposition with the integrated
preferred result above. Installed-game performance and R5 remain their existing
release owners' work; final implementation review accepted this slice. No further
source or artistic investigation is required here.
