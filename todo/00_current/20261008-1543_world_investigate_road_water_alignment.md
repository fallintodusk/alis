# Terrain and Water alignment

**Status:** CURRENT - D37 Terrain/Water completion is active; TERRAIN BASELINE
NOT ACCEPTED. D34's correction workflow and D35/D36 building proofs remain valid
within their scopes. Building expansion and deep PCG research are paused. The
radial model and all three geographic cases remain OPEN; no production geography
is admitted.

## Goal

Produce plausible, continuous Mesh Terrain and Water plus approximate tagged
feature envelopes, ready for separate future object generation. Correct repeatable
terrain defects automatically through small tested rules; retain an explicit
authored correction input for the last mile. Make ordinary iterations local and
fast, then validate complete Kazan and Manhattan generation at milestones.

## Contents

- [Current owners](#current-owners)
- [Authority register](#authority-register)
- [Required release-case dispositions](#required-release-case-dispositions)
- [Remaining implementation](#remaining-implementation)
- [Verification and completion](#verification-and-completion)
- [Review disposition and retained evidence](#review-disposition-and-retained-evidence)

## Current owners

Read the row needed for the current boundary. These owners describe implemented
behavior; proposed hierarchy, caches and influence APIs remain in this task.

| Need | Source of truth |
|---|---|
| Terrain, Water, object separation and existing authored adjustment | [Territory contract](../../Plugins/World/ProjectWorld/docs/territory_contract.md#terrain-and-feature-separation) and [consumption view](../../Plugins/World/ProjectWorld/docs/architecture/diagrams/terrain_features.md) |
| Current correction capabilities and ownership | [Refinement structure](../../tools/World/CanonicalCompilation/refinement/docs/architecture/structure.md) and [main view](../../tools/World/CanonicalCompilation/refinement/docs/architecture/diagrams/main.md) |
| Supported preparation, refusal, identities and current reuse cost | [Combined preparation](../../tools/World/CanonicalCompilation/terrain_refinement.md#combined-feature-preparation) and [sequence view](../../tools/World/CanonicalCompilation/refinement/docs/architecture/diagrams/candidate_preparation.md) |
| Source roles, observation/model distinction and qualified Ground | [Ground evidence contract](../../tools/World/SourceIngestion/supplemental/ground_products.md) |
| Native terrain/cook/scene fixtures and replay | [Mesh Terrain verification](../../Plugins/World/ProjectWorldMeshTerrain/README.md) |
| Small-test routes and gate escalation | [Focused refinement tests](../../tools/World/CanonicalCompilation/refinement/README.md#focused-contract-tests) and [pipeline layers](../../docs/testing/world_pipeline_layers.md) |
| Actual selected Apply, dependencies and recovery | [World transaction owner](../../scripts/ue/world/README.md) |
| Modular building prototype, supported shapes, kit and Terrain refusal | [Assembly architecture](../../Plugins/World/ProjectBuildingAssembly/docs/architecture/structure.md) and [fast/native tests](../../Plugins/World/ProjectBuildingAssembly/Tests/README.md) |

## Authority register

D1-D30, Q1-Q5 and A1-A5 keep their IDs and original wording in the immutable
historical task at commit `abb597aa8c8f24549e29e4bcb5f462fc890e0017`. They are not
renumbered or duplicated into living SOTs. Load an older statement only when its
scope matters:

```text
git show abb597aa8c8f24549e29e4bcb5f462fc890e0017:todo/00_current/20261008-1543_world_investigate_road_water_alignment.md
```

Verified 2026-11-10: this commit is an ancestor of the current branch; its task
blob `06781ec004d7b823cb6f31f1d98219adf0fb34b0` contains all 4,706 original lines,
matching the pre-cleanup content hash. This is retained history, not a tmp input.

Binding history still includes D1's presentation/geography separation, D13's
disposable sparse representation, D21's extensibility/fast tests/locality goal,
and D23/D25's autonomous owner-local engineering. D30's completed model/scene
evidence is bounded and disposable; B9 is not mandatory production geometry. Existing production,
release and Git-operation boundaries survive unless explicitly superseded.

- **D31 [CLARIFIED IN PART by D32 - first implementation scope]** Make Mesh Terrain and Water the current product target. Treat mapped
  buildings, bridges, roads and trees as separate temporary context/volumes/tags;
  their detailed procedural replacements belong to separate future scopes.
  Choose a flexible hierarchy that diagnoses a terrain defect, applies an
  appropriate supported correction, or exposes a residual for another qualified
  operation or manual adjustment. Future generators may request terrain influence
  through an explicit interface; their realized meshes must not become terrain
  authority. Plausible terrain without gaps or weird geometry is the product
  criterion, with local automatic fixes and a last-mile authored adjustment path.
  Keep small faithful regressions, dependency-local iteration and intuitive SOT
  docs/diagrams; remove the execution diary from the active todo.
  - Supersedes in part: D27/D29/D30's immediate priority on completing/admitting
    the particular Pier25 structural assembly and B candidate. It also changes
    the terrain-first application of Q2/Q3/Q5 as recorded below. Their historical
    observations, protected-state constraints and bounded proofs remain valid.
  - Current pass: investigate web/code/docs with several agents, evaluate the
    supplied review, update correct SOT owners and this task, and fix demonstrated
    in-scope issues. Unimplemented behavior is recorded below, not presented as
    a current capability. No new city publication, generation campaign, staging,
    commit, remote write or release operation follows from reviewer wording.
  - Reason: focus on reusable terrain generation and reduce slow, noisy iterations.
  - Source: operator's terrain-first/manual-influence and documentation request,
    2026-11-10.

- **D32** Adopt the base-world clarification: credible playable Terrain + Water +
  approximate tagged envelopes precede detailed procedural objects. Start with
  retain adequate base / qualified local correction / explicit manual-unresolved
  disposition, rather than a universal chain framework. Local layout intents
  precede canonical correction and final realization; PCG is a future optional
  consumer, not a Mesh Terrain writer. Approximation does not waive the accepted
  physical/gameplay requirements of a route actually admitted as playable.
  Preserve proven regressions and scope tests to changed properties and fidelity.
  - Clarifies/supersedes in part: D31's first-version breadth; its future extension,
    ownership, fast-test and locality goals remain. No production rollout follows.
  - Source: operator's agreement with the architectural-target clarification,
    2026-11-10. Proposed adapters remain implementation tasks, not current behavior.

- **D33** Implement the first bounded T1-T3 real-case proof with existing
  correction mechanisms, a permanent faithful regression and measured local work.
  Classify Terrain, Water and unsupported source interpretations before choosing
  a fix. Keep native verification focused and preserve production/release gates;
  no generic framework or unmeasured optimization follows.
  - Source: operator's implement-approved-change request with the next-case
    direction, 2026-11-10.

- **D34** Complete one actual authenticated Terrain-correction candidate through
  the existing selected-child path; repair the D33 quality observer and classify
  unrelated Water admission independently. A demonstrated missing owner-local
  capability may be implemented with independent review. Prove original/final
  identity, complete affected output, seams, cold/no-op equality, measured work,
  saved/reloaded Mesh Terrain, actual collision and a representative perspective.
  Perform a read-only building-readiness audit; no building implementation,
  generic framework, city publication, full-city regeneration or Git writes.
  - Source: operator's attached autonomous D34 mandate, 2026-11-10.

- **D35 [COMPLETE for isolated prototype]** Begin the one-building modular prototype
  after focused preflight. Reuse existing canonical footprint/effective-volume
  and Terrain contracts, generate an original tiny test kit if useful, and prove
  deterministic layout, selected-only blockout replacement, support/refusal and
  local regeneration with fast tests plus isolated native evidence. Act
  autonomously; use architectural review for blockers and official Epic research
  for current PCG/terrain capabilities. Backlog narrow unsupported cases rather
  than introducing a generic system. Future generators may request canonical
  terrain adaptation; they do not gain another Terrain writer. City publication,
  release geography and Git staging/commits remain outside this scope.
  - Source: operator's explicit implement-approved-change and KISS direction,
    2026-11-10. Supersedes D34's building read-only limit for this prototype only.

### Current gate dispositions

- **D37 [ACTIVE - Terrain/Water first]** Resolve Manhattan Ground/fountain,
  Pier25/coastal connections and Kazan bank/Water at terrain-first game-world
  fidelity. Recover exact current inputs and evidence; classify retain / correct /
  unresolved, prove the smallest faithful L0 case, then affected native L1 geometry,
  Water contacts, collision, views, deterministic replay and recovery. Obtain
  independent review at each accepted boundary.
  - After all three local cases pass, run existing isolated/restoring Manhattan
    and Kazan L2 verification on authenticated inputs, including actual routes,
    streaming and visual acceptance. Reuse only still-valid receipts. No L3
    promotion, L4 packaging, publication or Git writes are authorized.
  - Preserve existing uncommitted D36 work. Do not expand buildings, start deep
    PCG/SIGGRAPH research, create influence frameworks or optimize speculatively.
    Measured performance work is optional unless it prevents this verification.
  - Reject the provisional +5.1 m Madison radial model if it worsens the admitted
    shape. Do not invent observed heights or Water levels to obtain a pass.
  - Keep this task current until the actual development baseline is accepted;
    release R2/R5 and operator gates remain separate and explicit.
  - Source: operator's second attached Terrain-first mandate, superseding the
    first attachment's immediate closure/building/PCG direction. Its reused D36
    label is recorded here as D37 to preserve the completed D36 identity.
  - Operator clarification (2026-11-11): compare one bounded disposable authored
    Ground transition after checking raw source, frame and processing. This is
    diagnostic authority, not geographic approval. Keep unknown stencils unknown;
    reject cliffs and unsupported Water relationships. Continue Pier25 and Kazan
    independently. Backlog requires evidence that an unsupported area is outside
    the agreed playable baseline with no affected consumer; no such exclusion has
    been established for the required fountain concern.
  - Operator clarification (2026-11-11): compare the proposed broader neighborhood
    regrade within at most 5.4747 km2. Retain supported Ground in the core, an
    explicitly authored border, exact outer Terrain trace and protected unknown
    stencils. This authorizes a disposable comparison, not geographic acceptance
    or production promotion.
  - Operator clarification (2026-11-12): authorize ONE further outward diagnostic
    Ground-first expansion using existing cached and qualified evidence. Screen
    source support and joins cheaply, choose the smallest plausible domain, and
    stop this investigation if that expansion cannot support a credible join.
    No repeated enlargement or arbitrary street grades. Agents choose technical
    methods, local corrections and L0/L1 checks within this budget without asking
    again. Geographic acceptance, production/release authority and budget changes
    remain operator decisions. Continue independent Pier25/Kazan work; this run
    excludes full-city L2, buildings, PCG research, staging and commits.

- **D36 [COMPLETE for disposable registered lifecycle]** Integrate selected modular buildings through
  the ordinary World profile, producer, manifest and transaction lifecycle in a
  disposable test profile. Start with one, then 3-5 supported buildings after its
  lifecycle passes. One buildings projection owns massing suppression and admitted
  assemblies together; no independently selectable replacement manager. Reuse the
  original kit and native shared asset ownership. Prove cold/no-op, changed-input
  locality, removal and failed-apply recovery; measure actual work. Keep Terrain
  concerns separate. No complex geometry, PCG adapter, full-city generation,
  publication, release, staging or commits.
  - Source: operator's implement-approved-change request adopting the D36
    integration direction, 2026-11-10. The operator's committed D35 baseline and
    Git index remain unchanged by this work.
  - Boundary: World building editor, its optional selection contract, native kit
    realization and focused fixtures change. Canonical data/producers, Terrain,
    other layer implementations and production outputs stay untouched.
  - Proof: small host/schema checks -> exact native projection test -> existing
    wrapper/manifest transaction on a tiny confined TestData fixture. No broad
    Check/Matrix/city run is authorized by this disposable integration scope.
  - Adjacent boundary correction: the first native producer saved an admitted
    assembly, but Mesh Terrain's post-apply audit rejected the wrapper-supported
    tmp evidence root and the transaction restored prior state. Reuse World's
    public root guard and promote its existing input-isolation helper for Audit
    and Receipt; Terrain geometry remains untouched. Reserve `tmp/world/inputs/`
    from evidence writes for consumed scratch selection/kit files. Independent
    architectural review accepted this bounded fix; prove refusal preserves bytes.

- **Q1:** remains closed by D8 for bounded correctness. **Q4:** remains closed
  by D13 for the disposable sparse representation, not arbitrary new admission.
- **Q2 [TERRAIN ARCHITECTURE NOT BLOCKED by D31]:** enough evidence exists to
  design/test the correction hierarchy. Detailed civil-road grade policy is a
  future road concern unless a selected terrain correction actually needs it.
  Select and challenge scenario-specific engineering limits; do not impose a
  universal road crossfall or submarine slope law on all terrain.
- **Q3 [SEPARATION DIRECTION RESOLVED by D31]:** object volumes and tags are
  context, not precise built geometry. Preserve source crossing/role uncertainty;
  terrain acceptance does not wait for detailed procedural bridge/building
  reconstruction. A relationship actually consumed by terrain still needs an
  explicit interpretation, inference or unresolved/manual disposition.
- **Q5 [PRODUCT CRITERION SET by D31; MEASURED ACCEPTANCE OPEN]:** judge plausible,
  playable Terrain/Water rather than require surveyed seabed/object reconstruction.
  T5 must turn that criterion into reproducible views and physical checks. Keep
  strict finite geometry, coverage, shared edges, cooking, identity, provenance
  and refusal/no-mutation checks. A visual pass alone cannot establish collision
  support; declared inference must not be relabeled an observation.

No generic operator question blocks the architectural investigation. Raise only
a concrete residual whose acceptable product consequence cannot be determined
within D31, with evidence and a recommended choice.

## Required release-case dispositions

The [release router](00_release_3.0.0.md#r2---materials-and-required-visual-concerns)
still requires these geography concerns. Detailed object deferral does not close
them. Each must become accepted at current terrain-first fidelity, explicitly
deferred by an authorized revised release decision, or remain unresolved with its
release consequence. An unresolved/manual marker alone is not a release waiver.

| Case | Current status | Required terrain-first acceptance | Existing evidence reference | Release consequence while open |
|---|---|---|---|---|
| Manhattan Pier25/coastal connections | OPEN for gameplay/admission; combined two-cell local inferred-access fixture retained after native reload, physical contact and full-cap runtime views | Continuous plausible Terrain/Water joins and physical support for any route admitted as playable; detailed bridge reconstruction is separate | Current candidate and combined-scene evidence below | R2 concern remains open; blocks final R5 admission |
| Manhattan regional Ground/fountain | OPEN; source-level DSM/Ground mismatch confirmed; authored and single outward Ground-first comparisons REFUSED | Qualify the selected Ground extent and required joins; resolve fountain footprint/level, Water/Ground contact and affected access at current fidelity | [Madison fixture](../../Plugins/World/ProjectWorldTestData/Data/Fixtures/madison_dry_ground/README.md) and the D37 source/comparison evidence below | R2 concern remains open; blocks final R5 admission |
| Kazan bank/Water | OPEN for gameplay/L2; local retain candidate has verified compiled-runtime appearance | Preserve the demonstrated continuous bank/Water contact; prove remaining required consumers at their own boundary | Historical task RC0/RC3 and the D37 regional evidence below | R2 concern remains open; blocks final R5 admission |

## Remaining implementation

D34's [authored dry Terrain](../../tools/World/CanonicalCompilation/terrain_refinement.md#authored-dry-terrain)
workflow is implemented; the [Madison fixture](../../Plugins/World/ProjectWorldTestData/Data/Fixtures/madison_dry_ground/README.md)
and [native owner](../../Plugins/World/ProjectWorldMeshTerrain/README.md) own its
regressions and replay. This does not accept the radial +5.1 m model: six faces
steepen and the largest incident/external crease grows 15.3 -> 25.0 degrees.
The complete-cell native proof passed in 107.3 s, including saved/reloaded geometry
and 19 collision points. Retained evidence is indexed below.

T2's measured baseline remains: four nodes/nine quads in one of 210 cells;
636 existing outputs unchanged. Preparation 55.10 s includes 45.49 s for 631
authenticated reads/validations, 0.744 s geometry and 0.987 s copying 694,507,646
bytes. Cold/no-op took 54.22/55.04 s. Selection/materialization took
213.10/102.78 s. Geometry locality is not sparse total I/O or raw-source replay.

D35/D36's implemented boundaries and unsupported cases now belong to the Assembly
owners above. Complete geography acceptance and T2 optimization remain separate
from this registered building lifecycle proof.

### T1/T3 - Next real correction, without a universal chain

- [ ] Qualify regional Ground support before selecting another height patch.
  The authenticated 1345 Avenue of the Americas basin neighborhood has Terrain
  168.2766 m / Water 168.3 m at `(586198.6, 4512911)`, while the retained
  provider Ground model is approximately 20 m. This is a regional DSM/Ground
  role conflict, not a two-quad basin correction. Establish complete source,
  core/halo and exterior support; then use qualified Ground before Water fitting
  and refit affected dependent Water groups. Terrain-dependent Water reuse is
  now corrected and regression-tested in CanonicalCompilation. Regional
  pre-Water Ground selection is still unimplemented; supplemental product
  admission already exists. Do not import the fixture's assumed
  20.2 m Water stage or relax original-edge/civil-support refusals.
  The small collar and bounded neighborhood comparisons below are unselected;
  do not repeat them or force them through dry-only Water refusals.
- [ ] Resolve the bounded comparison's product boundary before implementing its
  Ground selection. Complete sampled support exists around both basins, but the
  tested 5.4747 km2 joins remain unaccepted: simple borders introduce sharp outer
  creases; retaining the original edge slopes creates nine new >100% faces.
  The ONE authorized outward Ground-first experiment is now consumed. Screening
  one additional 930 m cell in each direction (17.6391 km2 fixed study) selected
  a 5.9157 km2 certified domain. Source-only interior with the unchanged outer
  trace still introduces 47 new >100% faces and a 98.45-degree local crease.
  No unsupported quad or boundary node changes. Refuse this candidate before
  Water/native work; stop the fountain investigation rather than expand again or
  invent street grades. This does not prove every possible Ground model fails.
  Reuse
  [supplemental Ground admission](../../tools/World/SourceIngestion/supplemental/ground_products.md)
  and [pure Ground construction](../../tools/World/CanonicalCompilation/terrain_refinement.md#raster-ground-construction).
  The ordinary mosaic only stitches compatible Copernicus components; conflicting
  overlaps refuse, and pure Ground output remains unpublished. The missing link
  is qualified regional Ground into canonical pre-Water Terrain, not another
  admission service or publisher. If no complete supported domain is available,
  retain the exact unresolved scope and release consequence before introducing
  a new contract. Current evidence does not justify excluding the fountain from
  the playable baseline or promoting any inferred geography.
- [ ] Select the next smallest demonstrated product defect among the three cases
  above. Identify Terrain, Water, approximate-object or unsupported-source ownership
  before selecting retain / supported correction / manual-unresolved disposition.
- [x] Complete Pier25's combined two-cell Terrain/Water/deck native scene and full
  shore-cap runtime views, including independent local retain review.
  Projection, registered producer, isolated Terrain/restored combined physics and
  replay contracts belong to the [regional test owner](../../Plugins/World/ProjectWorldMeshTerrain/Tests/README.md).
  Retain this as a local inferred candidate; Hero and geographic admission stay open.
- [ ] Capture/reduce the actual failing owner input and pinned support into a
  permanent faithful RED fixture, then repair that owner. Keep coordinates,
  source roles, joins and required neighbors; no scratch dependency in tests.
- [ ] Add a new scenario or dependent composition only when that real input proves
  existing operations insufficient. Keep one final Terrain writer; no generic
  classifier, scenario engine or PCG influence API upfront.
- [ ] Independently qualify any additional conditioning of Water
  `alis:osm:way:452617299`. Historical base finalization requires new lowering
  (six nodes, up to 13.7 m), with only low-confidence point-fallback support.
  The nearest edited Madison node is 522 m away. Retaining unchanged Water in
  a dry-disjoint child neither admits these cuts nor makes this the sole next job.

### T2 - Measured preparation cost (deferred unless blocking D37)

- [x] Remove the demonstrated repeated raw-geometry work in Water dirty selection
  using its existing prepared-geometry owner. Exact same real fit and dirty scope:
  409.096 -> 24.645 s, 39 selected features, 38 changed functions, 16 cells.
  RED: 90,168 segment-distance calls for one disjoint footprint. GREEN: 27 focused
  Water/Pier tests, 16.617 s; independent review PASS. This does not localize base
  parsing, avoid the complete Water prepass, or accept conditioning policy.
- [ ] Localize the measured 45.49 s authenticated parsing/validation cost further
  on the same real case before choosing a preparation optimization.
  D34 observes complete-base reads and copying, not generally sparse preparation.
  Geometry is restricted to four nodes/nine quads; do not confuse that with total
  I/O or consumer regeneration. Preserve authentication, cold/no-op equality and
  failure recovery when changing the measured bottleneck.
- [ ] Optimize only that demonstrated boundary through its existing owner and one
  declaration of consumed dependencies. Reuse accepted unchanged results; prove
  stale/corrupt-input refusal, moved/removed support and exact dependency closure
  for any new reuse path. No second cache authority or generic scheduler.

### T4/T5 - Product acceptance and subsequent milestone

- [ ] Judge the radial model separately with its per-face/crease/source-departure
  evidence and actual view. Keep it provisional unless it is appropriate for the
  admitted product role; do not approve it from four improved source comparisons.
- [ ] Verify any selected native Water, route, placement, partition/streaming or
  navigation contract when it actually changes. Reuse unaffected proofs. D34's
  isolated Terrain scene does not establish Hero routes, Water presentation,
  packaged behavior or complete geographic acceptance.
- [ ] After local/profile closure, run complete isolated/restoring Kazan and
  Manhattan milestones, inspect playable Terrain/Water and measure costs. Distill
  each failure back to its owner fixture instead of debugging full worlds daily.

Future multi-generator influence, editor sculpt UI and joint/ordered correction
adapters remain conditional on a demonstrated consumer. Their accepted ownership
direction stays in the territory contract; they are not current capabilities.

### Building follow-ups - paused by D37

Resume only after the accepted Terrain/Water baseline and operator direction.

- [ ] When a chosen real feature requires it, add one faithful regression for
  its unsupported footprint, cross-cell/multivolume support or Terrain span.
  Terrain changes still go through canonical correction; no second writer.
- [ ] Before general kit intake, replace the original-test kit reader's
  10 x 10 x 6 m probe with target-independent kit admission. Retain strict types
  and qualify dimensions against the actual selected input.
- [ ] Choose a bounded real neighborhood and qualify its actual footprints and
  support before production/playable admission. D36's source-shaped synthetic
  fixture proves lifecycle behavior, not city geography, art variety or streaming.
- [ ] Qualify material-reference-only adoption for modular ISMs when the first
  consumer needs it; current selection explicitly refuses that route.
- [ ] Measure selected-cell reclassification/massing before adding reuse;
  unchanged outputs do not imply skipped validation or sparse total I/O. Reuse
  the existing shared kit package ownership; no speculative cache or scheduler.

## Verification and completion

For each new case: faithful RED -> smallest owner fix -> focused GREEN -> affected
consumer checks -> authenticated replay. Run ordinary iterations through the
linked small fixtures; full-city Check/Matrix/cook belongs to milestones.

The three release cases remain OPEN with their consequences above. A workflow
proof, provisional model or manual marker does not waive them. City selection,
durable World publication, release packaging and Git writes remain outside D34-D36.

## Review disposition and retained evidence

- D37 keeps all three geographic cases and both-city L2 OPEN. Evidence root:
  `tmp/world/terrain_baseline_d37/`. Current inputs reproduce the RC0 active
  hashes and 638 Manhattan / 641 Kazan outputs. Protected geometry/authority,
  HEAD/index remain unchanged. The current milestone started from the clean
  baseline recorded in `milestone_v1/baseline.json`; its final protected-state
  verification in `milestone_v1/final-verification.json` confirms all 3,123
  protected files and the index remain unchanged.
  Implemented observation commands and limits belong to the
  [regional test owner](../../Plugins/World/ProjectWorldMeshTerrain/Tests/README.md).

| D37 boundary | Retained evidence under that root | Disposition / next constraint |
|---|---|---|
| Current regional fields | `current-inputs.json`, Kazan `kazan-native-built-v5/sections.json`, Madison/fountain/Pier25 `*-native-built-v1/sections.json`, Pier25 v2 | 1,922 faces per initial/reloaded variant; 79-93 s. Regional Water preparation, collision and reload proof only; earlier gray/dark captures do not accept geography. |
| Focused controls | `outside-control-*`, `tiny-outside-*`, probe-isolation and split-intersection logs | Observed RED/GREEN for missing/extra/tiny faces and leaked probe packages; final focused checks 18-20 s. `CanonicalAuthoredDry` still passes in 61.7 s. |
| Source roles and processing | `transition_v1/source/report.json`, producer `qualify_transition_sources.py` | All 1,024 fountain DSM nodes reproduce exactly. Both 1,156-node Ground windows agree with independent point-scale native warps within 9.5e-7 m. Elevated-building influence is supported, not an exact roof height. Existing admitted raster product covers Madison; new windows have processing/support evidence, not supplementary/geographic admission. |
| Ground exterior / unknowns | `ground-exterior.json`, `ground-support-v1/report.json`, `milestone_v1/{source-support/report.json,regional-domain.json}` | All 258 unknowns reproduced and mapped inside Water, at least 1,767 m from the fountain; no complete Ground grid emitted. Finite search: 3,716,856 rectangles, 3,104,904 with full sampled support/halo; best outer mismatch 65.968 m. This does not rule out larger/nonrectangular or authored models. No unknowns filled. |
| One complete-footprint authored collar | `transition_v1/comparison_complete/report.json`, `comparison.png`, producer `compare_fountain_transition.py` | REFUSED in 1.59 s: 108 nodes / 130 quads, unchanged outer trace, zero unknowns consumed. Gradient 170.87% -> 262.56%, crease 35.41 -> 67.51 degrees, 41 newly >100% faces. Diagnostic Water quantiles independently compute 20.2 m for both basins, with low-confidence inference; no fixture stage imported. Dry-only construction correctly refuses Water. No native run of this rejected candidate. |
| Approved bounded neighborhood comparison | `milestone_v1/{fountain-comparison.json,fountain-comparisons.png}` | 3.0267 km2 supported core, at most 5.4747 km2 total; zero unknowns filled. Fixed crossing-Water support removes genuine exterior changes. Linear border: maximum grade 84.9%, outer crease 66.0 degrees. Native GDAL-smoothed border: 92.1%, outer crease 77.5 degrees. Edge-slope-preserving alternatives: outer derivative unchanged, nine new >100% faces. No model selected, no new Ground operation or native fountain run. These trials do not prove every bounded model impossible. |
| Single outward Ground-first comparison | `expanded_ground_v1/{boundary-screen.json,candidate/report.json,comparison.png,join-witness.png,review/candidate-review.json}` | Fixed 17.6391 km2 study uses cached, certified point/support evidence; 3,038 of 19,599 quads remain uncertified. Finite grid-boundary screen 1.25 s, validated against exhaustive tiny controls, bounds best possible maximum boundary departure at 27.075 m. Its one 5.9157 km2 candidate takes 1.83 s: no unsupported quad or outer node changed, but 47 new >100% faces and 98.45-degree crease. Independent reconstruction confirms all 47 are mixed Ground/retained joins; mapped West 31st Street intersects the worst crease with directional Terrain grades 15-17% -> 109-113% (not native Road/pawn evidence). There are 660 m of frontier against 15 retained uncertified quads near XY 587083..587323 / 4511052..4511472. REFUSED before Water/native; expansion budget consumed, investigation stopped. Neither the grid-boundary bound nor this refusal proves all possible models impossible. |
| Historical low Ground and actual routes | `milestone_v1/{historical-ground/report.json,review/local-routes-v2.json,review/local-routes-v4.json}` | Independent 49-node point warp agrees within 8.99e-7 m. Low cluster is reproduced source data under a current building; historical excavation is supported context, not proved current Ground. Exact nominal route strips have complete coverage and missing-face controls; selected West 36th longitudinal grade changes 7.61% -> 22.36/22.72%. Pit footway 43.78% -> 69.43%. Grades are geometric proxies, not pawn failure or universal rejection thresholds. |
| Water comparison and strict policy | `milestone_v1/{conditioning-delta.json,neighborhood-v3/water-delta.json,review/water-dirty-scope-r2-v1.json}` | Whole-cell replay has unrelated legacy cuts; counterfactual separates them. Fixed-exterior comparison changes zero exterior/protected nodes relative to the original-input replay. Of 484 measured conditioning requests in the smoothed diagnostic, 421 have the same final height as control; 63 differ, at most 2.0 m cut / 37.565 m support distance. Diagnostic limits are UNACCEPTED; production still refuses without qualified policy. No blanket 23 m cut approval. |
| Madison alternatives | `madison-node-comparison.json`, `madison-qualified-comparison.json` | Source agreement alone is insufficient: 15-node fit improves 3.766 -> 0.117 m mean departure but steepens faces/creases. These alternatives remain unselected. |
| Kazan bank/contact | `native-contacts.json` | 60 x 60 m ROI: approximately 731 m2 exposed / 1,199 m2 Terrain-occluded Water; max departure 7.413 m. Occlusion is not a proved bank defect. Keep the complete flowing function, resolve useful view and connectivity before retain/correct. |
| Kazan capture isolation | `transition_v1/kazan-variant-{red,green}` and corresponding logs | RED: two co-resident visuals (142.4 s). GREEN: one Nanite visual / one suppressed fallback, restored state, all three 1,922-face fields (93.0 s). Brighter image remains synchronous diagnostic; no stable-frame World lighting/geographic acceptance. Do not infer winding from signed-normal PNGs. |
| Kazan settled regional views | `milestone_v1/kazan-capture-v5/{capture.json,presentation.json,screenshots/}` | Three authenticated 1280x720 views with configured lighting/exposure show continuous contact. The route draws Editor preview sections, not saved compiled actors. Speckles remain unclassified; this does not close compiled appearance, shoreline access or L2. Independent review retains geometry as candidate. |
| Kazan actual compiled runtime view | `milestone_v1/{kazan-runtime-view-v4/,final-verification.json}` | Exact PIE observer PASS in 42.1 s: two compiled sections, one drawn Nanite component, one ordinary Water actor, three distinct settled frames, actual 50,000 lux / EV12, refusal controls and unchanged saved inputs. Independent local retain PASS: continuous banks/contact, no demonstrated gap; preview speckles absent. Hero, full-world Water and L2 remain open. Sun rotation regression RED/GREEN and Editor lifecycle PASS. |
| Pier25 first connection | `pier25_connection_v1/{measure.py,input.json,report.json}` | About 1 s: complete 7.75 x 2 m corridor, two Ground quads/four faces, eight historical Upper faces; missing-face control refuses. Entry mismatch 2.892-2.916 m compares current Ground with UNSELECTED historical structure, not a proved production Terrain gap. Observed-pier requirements are the wrong mechanism for inference; its attempted call borrowed another Road's parent and is only a fixture applicability control. Declared import supports inferred geometry, but this short layout lacks role/grade qualification; its existing civil limits are not universal Terrain rules. |
| Pier25 current inferred access | `milestone_v1/{pier-route.json,pier-model-v2/,pier-native-final/}` | Full 637 m mapped path: complete 2 m Terrain cover, mostly below regional Water. Separate 0.8 m deck uses qualified fixed-pier context, 2 m entry inset, 25 m ramp and 12 m mapped inland extension; maximum grade 1.953%. Preparation 60.676 s changes four metadata/feature outputs, preserves 634 including all Terrain/Water. Standalone native lifecycle PASS; combined proof follows below. The deck remains authored inference, not surveyed geography or the complete pier surface. |
| Pier25 combined local fixture | `pier_joint_v1/{preparation/l0.json,native_r3/sections.json,joint_readback.json,runtime_view_caps/,review/final-review.json}` | Two complete original cells, 644 authenticated candidate outputs, all 210 Terrain and seven selected Water features unchanged. Native PASS 86.2 s: all three variants/reload have 3,844 exact faces, 13 external actors reload, all 80 deck-face own-component collision probes pass. Actual native affine measurement 1.3 s: minimum deck/Water clearance 0.196333556 m, full-cap Ground error <4.58e-7 m. Cap-framed PIE PASS 85.9 s: two compiled sections, one drawn Nanite component, two ordinary Water and two structural actors, three ready frames/views, actual 50,000 lux/EV12, 21 saved inputs unchanged. Independent RETAIN covers both entire cap joins and continuous corridor; Hero, navigation, full-world Water, complete pier and geographic admission remain unproved. |

- Compiler dependency fix: a raster-only change previously retained stale Water
  elevations. Focused cold/incremental RED (3.289 s), then three new tests GREEN
  (5.017 s); combined 18 Water regressions GREEN (23.159 s). Polygon nodes, actual
  fallback support outside footprints, flowing groups/halos, unchanged neighbors
  and quantized/no-op reuse are covered. Evidence: `reviewer_075f8593/` logs.
  Cold/incremental final Terrain core, halo, semantics and edge hashes also match.
  Output invalidation is dependency-local; the existing complete target-relevant
  Water prepass still runs when feature compilation is needed. Fully local
  fitting work is not implemented or claimed.
  Instrumented fixture execution (`reviewer_075f8593/execution-cost.json`):
  incremental 1.774 s versus cold 1.145 s, no-op 0.107 s. One feature cell changes;
  both Terrain cells rebuild for this single-raster fixture. The Water prepass
  still consumes all five Water records, including the unchanged neighbor. This
  is correctness evidence, not a demonstrated speedup or city-scale benchmark.
  The implemented contract/test route is in the
  [compiler owner](../../tools/World/CanonicalCompilation/README.md).
- Source inventory first: cached NOAA 9307 NYC2017 bare-earth/hydroflattened
  tiles 020/025/026/029/030 already informed the source and exterior probes above;
  they are not a new lead. Other 2017 variants are not thereby qualified.
  No NYC2010 seamless DEM was found in the inspected World source cache. Before
  acquisition, qualify its distribution/use conditions and a bounded retrieval
  route: [NYC metadata](https://github.com/CityOfNewYork/nyc-geo-metadata/blob/main/Metadata/Metadata_DigitalElevationModel.md)
  identifies interpolated 2010 Ground, NAVD88 feet and a DEP approval condition;
  the [public dataset](https://data.cityofnewyork.us/City-Government/1-foot-Digital-Elevation-Model-DEM-/dpc8-z3jc)
  advertises bulk ZIPs, not qualified local tiles. Neither product existence nor
  interpolation proves the 258 unknown stencils or exterior joins resolved.
- Next: regional Ground integration remains unimplemented. Dependent Water
  invalidation alone is not a Ground replacement. Do not repeat the rejected
  collar, radial Madison model or force
  a dry patch through Water refusal. The required fountain/plaza concern has not
  been shown outside the playable baseline; it remains R2/R5-blocking.
- Continue Kazan's useful shoreline observation and Pier25's explicit approximate
  access layout/role independently. Road-profile omission and a far-away runtime
  start do not waive the coastal concern. No geographic case is accepted here.
- D36 integrates the existing `project_building_massing:v2` layer (output revision
  3), with one complete admitted replacement union and existing transaction/
  recovery ownership. Three buildings contain 207 instances referencing four
  shared meshes in one kit package. Cold reconstruction, no-op, kit change,
  post-save rollback and complete deselection passed through the normal wrapper.
  The seven-building/two-cell fixture is synthetic, preserving the captured
  first-building dimensions and an unselected cross-cell neighbor.
- D36 measured kit-only regeneration: 29.28 s wrapper / 0.385 s native; one owner
  cell and one assembly package changed, no massing triangles or kit assets were
  rewritten. Cold reconstruction took 111.91 s; final removal took 90.82 s /
  0.657 s native, including engine startup/shutdown and normal safety checks.
  Native time alone is not the complete iteration cost.
- D36 regressions cover stale selection removal after planning (observed RED,
  then GREEN), complete batch exclusion, building-height/physical-support
  identity, authored refusal and evidence/input isolation. Native lifecycle
  failures exposed temporary-map saving, absent clean outputs and loaded/unused
  package retirement; fixes and replay belong to the linked Assembly owners.
- D36 evidence: `tmp/world/building_lifecycle_d36/proof_c/lifecycle/receipt.json`,
  immutable scope manifests, individual native results and `runs.json`; retained
  outcomes distinguish accepted replacements from overwritten historical
  metadata. Replays now refuse evidence overwrites. Final outer
  content restoration digests match. The producer remains opt-in; production
  profiles, canonical inputs, authored content and all three release cases stay
  unchanged. No Git staging/commit or city publication occurred.
- The official massing baseline changed only output revision 2 -> 3; its four
  projection records stayed identical. Fresh saved/reloaded comparison and
  saved-output ownership sabotage passed. Final checks used an isolated local
  filesystem DDC after shared Zen startup failed; this does not qualify that
  shared service. Exact logs are `massing-compare-local-headless/tests.log` and
  `massing-known-bad/tests.log` under `tmp/world/building_lifecycle_d36/`.
- D35 proves real canonical building `alis:osm:way:250440683`: one blockout
  replaced by 69 original wall/window/floor/roof instances; 2,268 unrelated
  emissions unchanged. Full physical support covers 83.776631 m2, elevations
  20.326640..20.379169 m. Native no-op, kit change, drift repair, authored/unowned
  refusal, one-sided collision and save/reload passed. No Terrain edit or city
  layer registration occurred.
- D35 proof: host production-core regression PASS (1.65 s initially; final
  10.31 s including compile under native load); native editor build PASS
  (33.02 s); exact `CanonicalPrototype` PASS (136.5 s); exact `SavedRender` PASS
  (177.9 s). Initial missing-feature acceptance and story/slab-gap regressions
  were observed RED before implementation/fix. Native shell orientation,
  light-root composition and map-collection defects gained checks and are owned
  by [Assembly pitfalls](../../Plugins/World/ProjectBuildingAssembly/docs/pitfalls.md).
- D35 receipts: `tmp/world/building_assembly_d35/run_v3/native/receipt.json`,
  `native-log-v3/tests.log`, `saved_view_v3/receipt.json` and
  `saved_view_v3/building.png`; the neutral saved candidate is
  `saved_view_v3/scene/NeutralBuilding.umap` under the same root. These authenticate
  isolated generation/presentation, not production, gameplay routes or packaging.
  `final-checks.json` records ASCII/link/schema/Python checks, unchanged 3,123
  protected paths, HEAD and staged patch. Independent final review PASS covered
  source, receipts and images; the reviewer did not rerun Unreal.
- D34 reviewer findings accepted: the global maximum hid local steepening;
  unrelated Water qualification is independent of a dry Terrain correction;
  typed geometry injection did not establish real selected-candidate integration.
  The supplied slope-table correction (2.4 -> 15.1%, not 15.7%) is confirmed.
- Frozen authored-only authority now also rejects a resealed feature/Water change;
  this was a genuine RED discovered during implementation review. Existing mixed
  operations retain their own semantics. No geographic quality waiver was added.
- D30 retained disposable model evidence remains indexed at
  `tmp/world/geographic_model_d30/architect/d30-b9-composite-r2-v1.json` and
  `b9-floating-clearance-r2-v1.json`. B9 is not mandatory production geometry;
  the original diary remains in reachable Git history.
- D34 execution receipts: `tmp/world/terrain_case_d34/candidate-final.json`,
  `before-geometry.json`, `after-geometry.json`, `review/per-face-review.json`,
  `water/classification.json`, `building/readiness.json`,
  `native-log-v4/tests.log` and `native-v4/sections.json` / `player-eye.png`.
  These are retained evidence, never clean-checkout inputs or production authority.
