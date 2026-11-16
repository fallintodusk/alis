# Terrain and Water alignment

**Status:** DONE - Terrain/Water development scope accepted; production selection
and final release certification remain separate.

## Closure disposition

The operator accepts the fountain as temporary prototype object context and
requests no further fountain/Terrain investigation. Detailed fountain geometry
will be reconstructed separately using final Terrain height/support evidence.
This is a fidelity decision, not a claim that the approximate basin or its inferred
Water level is surveyed or that only material defects ever remain.

- [x] Coherent Ground, bounded shoreline and separately adapted Pier qualified.
- [x] Actual saved-city Hero, native collision/reload, dependency/no-op/recovery,
  partition and both-city uncooked consumer/tour proof retained on the candidate.
- [x] Fountain context disposition accepted by the operator (D48); no Ground
  change, basin reconstruction or extra model iteration required for this scope.
- [x] Durable object/Terrain semantics are owned by the territory contract.
  Lighting has its current stub; influence, building extension and measured
  preparation cost remain scoped backlog work. Fountain detail follows ordinary
  object reconstruction, without a separate fountain task.
- [x] Close the development task at 3/10 physical models. Do not rerun rejected
  models or unchanged native proof to close a documentation/fidelity decision.

The original top-level target receipt remains REJECTED for the final-reader
schema mismatch. Its focused repair and separate NONPRODUCING technical
reinspection pass; this closure does not mint a replacement target receipt.
Production canonical/World selection remains restored to its original state.
No candidate promotion, fresh packaged proof, R5/R6, final release approval or
Git writes follow from this closure. The release plan owns those remaining gates.
Exposure/readability remains a presentation concern; EV8 is diagnostic and has
not been adopted. Lighting research and object implementation have not started.

The sections below preserve the historical execution record, including prior
blockers, conditional unchecked options and now-superseded next steps. Resume a
new selected concern from its current/backlog owner and stable contracts, not
from an old instruction in this completed record. Reopen Ground only on new
evidence of a Ground or required-support defect.

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
- [Bounded execution strategy](#bounded-execution-strategy)
- [Acceptance checkpoint](#acceptance-checkpoint)
- [Fountain product decision](#fountain-product-decision)
- [Source-model research handoff](#source-model-research-handoff)
- [Remaining implementation](#remaining-implementation)
- [Verification and completion](#verification-and-completion)
- [Review disposition and retained evidence](#review-disposition-and-retained-evidence)

## Current owners

Read the row needed for the current boundary. These owners describe implemented
behavior. This task retains acceptance steps, operator decisions and execution
history; optional extensions belong to their scoped owner backlogs.

| Need | Source of truth |
|---|---|
| Terrain, Water, object separation and existing authored adjustment | [Territory contract](../../../Plugins/World/ProjectWorld/docs/territory_contract.md#terrain-and-feature-separation) and [consumption view](../../../Plugins/World/ProjectWorld/docs/architecture/diagrams/terrain_features.md) |
| Current correction capabilities and ownership | [Refinement structure](../../../tools/World/CanonicalCompilation/refinement/docs/architecture/structure.md) and [main view](../../../tools/World/CanonicalCompilation/refinement/docs/architecture/diagrams/main.md) |
| Supported preparation, refusal, identities and current reuse cost | [Combined preparation](../../../tools/World/CanonicalCompilation/terrain_refinement.md#combined-feature-preparation) and [sequence view](../../../tools/World/CanonicalCompilation/refinement/docs/architecture/diagrams/candidate_preparation.md) |
| Source roles, observation/model distinction and qualified Ground | [Ground evidence contract](../../../tools/World/SourceIngestion/supplemental/ground_products.md) |
| Provider product selection, known artifacts and source-model comparison | [Elevation source suitability](../../../tools/World/SourceIngestion/elevation_sources.md) |
| Native terrain/cook/scene fixtures and replay | [Mesh Terrain verification](../../../Plugins/World/ProjectWorldMeshTerrain/README.md) |
| Small-test routes and gate escalation | [Focused refinement tests](../../../tools/World/CanonicalCompilation/refinement/README.md#focused-contract-tests) and [pipeline layers](../../../docs/testing/world_pipeline_layers.md) |
| Actual selected Apply, dependencies and recovery | [World transaction owner](../../../scripts/ue/world/README.md) |
| Modular building prototype, supported shapes, kit and Terrain refusal | [Assembly architecture](../../../Plugins/World/ProjectBuildingAssembly/docs/architecture/structure.md) and [fast/native tests](../../../Plugins/World/ProjectBuildingAssembly/Tests/README.md) |

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

- **D37 [SUPERSEDED IN PART by D38 - Ground/L2 execution budget]** Resolve Manhattan Ground/fountain,
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

- **D38 [SUPERSEDED IN PART by D39 - additional source qualification]** Complete the Terrain/Water milestone autonomously through both-city L2,
  retaining valid verified improvements for 3.0.0.
  - Supersedes: D37's exhausted Ground-investigation and excluded-L2 timing for
    this closure run; its retained evidence and Terrain-first invariants survive.
  - Scope: ONE feasibility pass using cached 2017 Ground evidence over the existing
    Manhattan canonical domain; the smallest supported integration if viable;
    actual Pier25/Kazan Hero checks; then isolated/restoring Manhattan/Kazan L2
    after required local acceptance. Reuse existing owners, tests and valid proof.
  - Stop: if the Ground model or another required geographic issue is unsupported,
    report exact evidence and a recommended product decision. No repeated domain
    expansion, new source acquisition, invented observations, silent exclusions or
    promotion of rejected/unverified candidates.
  - Closure: preserve accepted improvements, migrate implemented knowledge,
    backlog only optional work and move this task to done only when required
    criteria pass. R5, final operator acceptance and publication remain separate;
    no staging, commits or other Git writes are authorized.
  - Source: operator's explicit "Yes - complete Terrain through L2" response,
    2026-11-12. Reason: close Terrain before moving to other 3.0.0 work.

- **D39** Keep the existing Manhattan canonical domain; perform ONE metadata-first
  qualification pass with at most 2,000,000,000 bytes of targeted public bare-earth
  raster downloads. Use existing ingestion/canonical ownership and only needed
  tiles. This investigation budget is not a permanent architecture limit.
  - Supersedes D38's source-acquisition exclusion for this bounded pass only.
    No repeated downloads, domain expansion, arbitrary blending, invented missing
    elevations, generic source framework or automatic geographic acceptance.
  - If support remains insufficient, stop this source investigation and recommend
    an explicit Ground/context/Water scope decision with affected consumer routes.
    Continue independently authorized Pier25/Kazan gameplay work.
  - D38's supported integration, isolated/restoring L2 after local acceptance and
    closure mandate survive. R5, production promotion, final operator acceptance,
    staging, commits and publication remain outside scope.
  - Source: operator's explicit "Keep domain; qualify missing Ground coverage"
    response, 2026-11-12.

- **D40 [ALIGNMENT CHECK COMPLETE; CONSTRUCTION NOT ENTERED]** Qualify ONE bounded explicitly inferred shoreline bulkhead/seawall
  candidate at the affected Pier25 coast, rather than a generic quay.
  - First compare the actual landward edge with Hudson River Park Trust's 2019
    Pier25/Pier26 survey, NYC's 2017 shoreline and authenticated Ground/Water.
    Distinguish shoreline bulkhead, pile-supported pier deck and an erroneous
    Water boundary. If the comparison contradicts the inferred edge, stop with
    that exact conflict instead of constructing an unsupported wall.
  - Preserve qualified Ground and existing Water authority. A vertical retaining
    structure is separate inferred geometry, never a steep Terrain substitute or
    an invalid face forced through the constructed-surface XY height field.
    Reuse the smallest suitable owner; no surveyed wall heights, bathymetry,
    universal shoreline classifier or generic modeling framework is implied.
  - If supported, prove continuity, exposed rendering, collision, actual Hero
    access, save/reload and affected native behavior. Reuse unchanged fountain
    evidence; both-city L2 follows acceptable local dispositions.
  - No additional raster downloads, geographic expansion, automatic production
    acceptance, release/Git authority or weakened existing gates.
  - Source: operator's explicit bulkhead clarification to Q7, 2026-11-12.

- **D42 [BOUNDED REVIEW/FIX PASS COMPLETE; GEOGRAPHY DEFERRED]** The operator's
  2026-11-13 autonomous audit/fix and "backlog if needed" request authorizes local
  engineering decisions and deferral without pretending unfinished geography
  passed. Compare one reversible, explicitly inferred shoreline adjustment;
  repair demonstrated effective-input admission and preserve a cheap portable
  failure input. This does not accept inferred geography, promote production
  authority, authorize more source acquisition or waive D38's acceptance gates.
  - The comparison consumes this pass's single shoreline trial. It tests an
    18 m westward core edge with a 60 m taper at each end, using the retained
    adjacent-line context as modeling guidance, not a surveyed root location.
  - Defer the remaining local shore-construction/composition implementation in
    this same task. Keep all required release cases and L2 open. Do not repeat
    old-scene gameplay tests, source research or full worlds to rediscover it.

- **D43 [REVIEW CORRECTIONS APPLIED; TERRAIN OPEN]** The operator's 2026-11-13
  review/fix request (`72eee77e`) resumes this required task in `00_current`.
  This supersedes D42's backlog scheduling, not its engineering evidence or
  protected-state rules. Keep D38's local-then-restoring-L2 sequence and all
  release cases. A reviewer cannot independently grant scope or geographic
  acceptance; D42's earlier "backlog if needed" authorization was real.
  - Reuse the tracked input and existing sparse geometry operation before
    proposing another operation. Preserve pre-Water source Ground as evidence;
    do not label a lowered dry transition as unchanged observed Ground.
  - The literal all-dry-preserved/all-wet-lowered condition is infeasible for a
    continuous field when their limiting heights differ. A local correction must
    explicitly qualify its finite dry transition, or a separate retaining edge
    with appropriate surface/topology ownership. Neither is implemented or
    geographically accepted by this review. Do not invent a surveyed wall.

- **D44 [EXECUTION ACTIVE]** The operator approved the
  player-impact-first strategy and requested it be recorded, 2026-11-13. The
  [execution strategy](#bounded-execution-strategy) owns its checkpoint, absolute
  ceiling, reviewer conditions and final dispositions. This is a bounded new
  local correction budget, not permission to repeat rejected models unchanged.
  - Adopt the review's source-aware finite transition and Pier landing adaptation
    as the first technical target. Inference is permitted and must stay explicit;
    preserve source Ground outside its declared transition. The prior impossible
    all-dry-preserved interpretation is not an acceptance requirement.
  - A separate retaining edge is a conditional alternative if the slope model
    cannot provide credible physical support. Its attempts share the same budget;
    it does not bypass D40's source/role distinctions or require surveyed claims.
  - D38/D43's active required cases, canonical ownership and local-then-restoring
    L2 order remain. A tolerable local known issue can be backlogged under the
    conditions below; no automatic R2/R5 waiver, promotion, publication, new
    source campaign, building/PCG work, staging or commits are authorized.

- **D45 [DIAGNOSTICS AND FIDELITY DIRECTION]** The operator requested explicit
  presentation diagnostics, an overall Terrain/building source-alignment check,
  approximate-construction tolerance, and researched future object-driven Terrain
  adjustment with PCG, 2026-11-16. Diagnostic test/capture owners and their stable
  contracts may change. Canonical algorithms, generated city assets, production
  presentation profiles and release/Git authority stay untouched. No physical
  model is added to D44's 3/10 count. Current fidelity and the future integration
  direction belong to [Terrain/feature separation](../../../Plugins/World/ProjectWorld/docs/territory_contract.md#terrain-and-feature-separation);
  diagnostic procedure belongs to [paired visual review](../../../tools/World/VisualVerification/README.md#paired-geometry-and-presentation-review).

- **D46 [COMPARISON EXECUTED; NOT PROMOTED]** The operator approved the focused
  source-bound fountain comparison and requested screenshots for independent
  review, 2026-11-16. Replay the saved candidate reversibly, identify the owning
  defect, compare one small presentation correction and preserve the existing
  geographic, production and release boundaries. Only diagnostic/capture owners
  and their docs change; no new physical model or source campaign is authorized.

- **D47 [LIGHTING STUB; GEOMETRY DECISION FIRST]** The operator requested one
  current lighting/daylight task for 3.0.0, retaining old-map and recent evidence,
  with no investigation now. Move the existing environment owner to current;
  queue exposure, native daylight-provider and measured Lumen work there.
  Keep completed Ground/Pier/collision proof and decide Terrain closure
  separately from lighting. The engineering assessment remains in the acceptance
  checkpoint; this entry does not select a new lighting or geographic model.

- **D48 [CLOSURE - PROTOTYPE FOUNTAIN CONTEXT ACCEPTED]** The operator agrees
  that the fountain is prototype knowledge and its real reconstruction will use
  Terrain height evidence; no further investigation is needed. Close this
  development task, update its durable owner and scoped backlog handoffs.
  This supersedes the outstanding fountain-fidelity decision and D44's active
  correction loop. It does not select production authority, adopt EV8, accept
  the whole release or implement future object/PCG work.
  - Source: operator's explicit fountain disposition and todo/docs request,
    2026-11-16.

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

- **Q6 [RESOLVED by D39]:** retain the 210-cell domain and qualify additional
  Ground coverage within the explicit investigation budget. A revised geographic
  scope remains a fallback requiring a separate decision if this pass fails.

- **Q7 [RESOLVED by D40]:** one inferred shoreline bulkhead candidate is selected
  conditionally on the cheap local alignment check. This does not accept its
  geographic placement, elevations or final geometry. Exact D39 witnesses and
  the failed simple-clipping control remain below.

No generic operator question blocks the architectural investigation. D40 owns the
current conditional model check. Raise only
a concrete residual whose acceptable product consequence cannot be determined
within D31, with evidence and a recommended choice.

## Required release-case dispositions

Development dispositions are complete. The release plan retains exact-source
selection, normal presentation, installed-runtime and final human acceptance.
Detailed object reconstruction is not a Terrain development prerequisite.

| Case | Closed development disposition | Retained evidence / limit |
|---|---|---|
| Manhattan Pier25/coastal connections | Integrated model 3, separate adapted Pier and required contact routes retained | Current integrated checkpoint and iteration ledger; no blanket acceptance of every OSM way or untested corridor |
| Manhattan regional Ground/fountain | Coherent 210-cell Ground near 19 m retained; fountain accepted as prototype object context by D48 | Current integrated checkpoint and presentation comparison; detailed basin reconstruction deferred, no surveyed rim/Water claim |
| Kazan bank/Water | Retained supported bank and actual saved-map Hero/native consumers accepted within their tested scope | Current integrated checkpoint; neighboring submerged mapped footway is contextual and remains unaccepted as a playable route |

## Bounded execution strategy

**Budget:** checkpoint after five model iterations; absolute ceiling ten. At the
checkpoint, continue only if independent review confirms measured geometry or
gameplay improvement and identifies a credible bounded next change. Otherwise
stop the local correction loop and perform the final player-impact assessment.
There is no extension beyond ten without an explicit new operator budget.
Stop earlier when a correction is accepted locally or a decisive physical blocker
is established; unused attempts are not a quota for additional polish.

**Counter at closure:** 3/10 model iterations consumed (third integrated native PASS; fountain context disposed by D48). Ground/Water baseline,
joint native field, regional Pier player-height views and actual Hero contacts passed. D39-D43
are retained prior evidence, not attempts in this new budget. Never reset the
counter for a new agent, reviewer, session, model type or verification failure.

The same model also passes the regional saved-scene Hero run over the complete
retained 648.914660 m corridor in both directions: 15,645 walking frames,
1.55 m minimum clearance above visible Water, 381.8 s. Receipt:
`tmp/world/terrain_execute_d44/retained-corridor-gameplay/gameplay.json`.
This requalifies that corridor in its regional fixture. It is not the actual
saved product map's corridor proof. The actual product-map check below covers
the five explicitly declared contact routes; neither certifies every OSM way.

### Current integrated checkpoint

Common `check-20261114T153300Z` and immutable-base Matrix
`run-20261114T155736Z` PASS for their frozen inputs. Selected target
`refinement-20261114T164549Z` completed an accepted native transport, all five
transaction phases and the entire requested consumer envelope. Selected native
Ground result is `f082b27f781b01100700d97af555b843f0d6c779ae86f0e3e204944acd56751c`;
World is `b2c9f55d8a1058e1dc9a562ae65c219c7babacbbc76aa47e97eca3a11b341873`.
Both render variants have 201,810 matching ordinary samples and 2,308 matching
refined-cell triangles, zero mismatches; maximum refined error is 0.000047245 m.
Cold no-op has zero actor changes and zero dirty units. Actual saved-map Hero:
Manhattan five bidirectional routes / 5,081 walking frames / 1.55 m minimum
visible-Water support; Kazan one bidirectional bank route / 1,767 frames. Both
foreign-floor controls, saved projections, partition audits and actual uncooked
Both-tour PASS. Both city processes closed normally; this is not packaged or
frame-budget certification.

The original Python result remains REJECTED solely for a final-reader schema
mistake: full Apply has `layer_inventories`, without scoped `execution_layer_ids`.
Faithful fixture RED/GREEN and 42 focused reader/consumer/completion tests PASS;
the malformed optional-map control also has RED/GREEN. A separate NONPRODUCING
reinspection binds historical Common/Matrix/request/transport, all nested native,
Hero, tour and image pins, and permits only the reader-function body plus two
named test changes. It PASSes without replaying native generation. It does not
mint an accepted top-level target, current Common, release or publication proof.
Evidence: `tmp/world/terrain_execute_d44/reader-reinspection-r5/` and
`tmp/world/end_to_end_validation/refinement-20261114T164549Z/`.

Supplemental observation of those exact saved bytes independently confirms the
two Pier contacts look continuous at Terrain-first fidelity. Original scene
actors and lighting are retained. The old fountain camera stood above the pool
center and looked beyond it; normal darkness still prevents rim qualification.
Explicit shadow/context controls are diagnostic only. The corrected dry-rim
normal/native views confirm the readability failure. Both eyes stand 170 cm
above actual dry Ground, about 24 cm outside the authenticated Water edge, with
FOV 75 and downward pitch -78/-65 degrees. The native host exited normally with
zero infrastructure errors and all 210 Terrain subjects drawn; its green capture
authenticates the images, not their visual quality. Do not change Terrain heights
or acquire sources to solve this presentation case. First observation restored
all 3,553 expected protected files, HEAD and index exactly and removed its marker.
Evidence: `tmp/world/terrain_execute_d44/viewport-diagnostic-r5/`.

The final native rim evidence and explicit `not_visually_qualified` judgment are
in `tmp/world/terrain_execute_d44/viewport-diagnostic-r6/`. Exact source feature
is `alis:osm:way:394395237`, cell `grid_36784852dea14f43:x2:y2`, level 19 m.
The frozen [presentation profile](../../../Plugins/World/ProjectWorldData/Data/Presentation/kazan_representative_v1.json)
uses fixed EV12. The exact cause within presentation/material/shadow context is
not proven; changing intentional exposure policy is not an authorized Terrain
height correction. D45 authorizes paired geometry/presentation diagnostics and
source-alignment review, not a selected production exposure change. Evidence for
the intentional presentation-policy question remains with `world_implement_environment_cycle`;
a day/night provider is not a prerequisite. Neither deferral nor a diagnostic
shadow-disabled image waives the required R2 visual criterion. This was the
pre-D48 hold; D48 resolves prototype fidelity while retaining release presentation.
Do not restart rejected Ground/shore models to avoid a presentation decision.
Ordinary existing-owner fault localization remains authorized; the pending
choice concerns intentional presentation-policy changes or required fidelity,
not permission for routine diagnostics.
Both observation owners have now restored all 3,553 expected protected files,
HEAD and index exactly, removed their markers and left no Editor running.
Final read-only transport reinspection still PASSes; documentation checks pass
for 23 changed/new documents and 342 relative links. The physical count remains
3/10; presentation observations and receipt repairs do not consume that budget.

D45 verification uses `tmp/world/visual_contract_update/`: the diagnostic-only
capture regression is RED/GREEN; native plan-mode parsing and a four-mode Engine
template capture PASS. The latter verifies capture behavior, not Manhattan visual
acceptance. Source control networks for both cities, effective-volume regressions
and final-Ground native building placement are checked without regenerating cities.
The full Manhattan canonical inventory has 109,486 logical buildings and 118,060
effective volumes: 115,285 source-tag heights, 566 floor estimates, 2,209 defaults.
These are provenance counts, not survey accuracy. The fountain ray's nearest
volume is from neighbouring relation `2100721`, part `145427088`, tagged 148 m;
its boundary is about 0.14 m from the recorded native hit. This is a local
correspondence, not an independent certificate of its real shape or every saved
building. Resume with paired views of the exact selected candidate; do not repeat
Ground acquisition or infer current-geography acceptance from template images.
Final checks: 21 Visual Verification, 10 building-volume and 6 geodetic Python
tests PASS; exact native `RenderModeContract` and `GroundFinalAnchor` PASS
(anchor error 0 m), Editor build PASS, four-mode template receipt independently
authenticated and images inspected. All 3,553 protected files, HEAD and index are
unchanged. No Editor remains running. This closes the D45 test/docs change,
not the fountain's normal-view qualification or Terrain acceptance.

### Presentation comparison checkpoint

D46 evidence: `tmp/world/terrain_execute_d44/presentation-comparison-r7/`.
Both 12-view native image sets authenticate; the original quiet capture exits
normally. Cold Editor pattern RGB is zero. Actual PIE generates Ready texture
outputs (normal sample byte RGB `190,149,236`); the resulting normal buffer changes
without changing Ground or its material graph. This refutes the apparent
horizontal-normal production defect inferred from the cold Editor alone.
Original EV12 remains too dark in PIE. One in-memory EV8 comparison (ISO 100,
f/4, shutter reciprocal 16 instead of 256) makes the contact inspectable. Matched
PIE screenshots are `play-context-ev12.png` / `play-context-ev8.png`, with pose and
compiled collision support in the matching JSON files. The wider eye is 170 cm
above actual compiled Ground; all geometry, material recipes, Sun and shadows
are retained. This is simulation-camera evidence, not a new Hero or package gate.

The exposed surface is still an approximate reflective Water patch with an
unfinished-looking edge; do not claim visual acceptance or invent a Terrain fix
from that image. Static EV8 is not selected for the shared production profile or
qualified in both cities. The live observation exited abnormally after capture
in `UExternalDataLayerEngineSubsystem::Get` during PIE teardown. Images remain
diagnostic, not a clean native host PASS. End play before a future exiting capture.
The outer owner restored all 3,553 protected files, HEAD/index and original
selection exactly and removed its marker; no Editor remains. Physical count
stays 3/10. The uniform-diagnostic-frame regression is RED/GREEN; 22 visual Python
tests and the Editor build PASS. No production/Git/release promotion occurred.

The final local geometry question is now bounded by
`tmp/world/lighting_todo_handoff/fountain-boundary-height.json`. Sampling 190
boundary positions against the retained grid triangles puts Water about
0.067-0.147 m above adjoining Ground. The grid query agrees with the actual
compiled-floor witness within 1e-6 m. Water level 19 m is marked
`low_confidence_point_fallback` with one source sample. This may be a missing
raised fountain-basin representation rather than bad Ground; it is not proven
to be a material-only effect or an incorrect surveyed Water level.

D48 disposes the raised edge as temporary fountain context. Retain the completed
Ground/Pier/collision proof; reconstruct the future object from final Terrain
support through ordinary object generation. Lighting/daylight/Lumen work stays in its current
stub without reopening Ground engineering. No new investigation is required.

### Integration history

The following records are historical and do not replace the checkpoint above.

Integration prerequisites exposed stale verify identities and diagnostic tests
registered without required inputs. Fix those owners and the cold execution
envelope before another Common Check; do not rebuild physical models or count
these verification repairs as iterations. Native baseline records are changed
only after independent projection comparison, never to hide geometry drift.

Current integration checkpoint: public selected-input admission PASS, 782.82 s,
`tmp/world/terrain_execute_d44/selected-inputs.json` needs re-admission after the
validation-only repair below. Common Check `check-20261113T180941Z` passed all
eight suites (340 canonical, 229 E2E, 329 lifecycle, 149 native) for the prior
profile/code; it is now stale. Immutable-base Matrix `run-20261113T183607Z`
completed both cities' native legs and restored all 3,552 original protected
files, then correctly rejected `layer_metrics_mismatch`. Do not repeat it with
the old expectations. Its log is `tmp/world/terrain_execute_d44/immutable-base-matrix.log`.
Kazan's retained bridge/confluence child produces 11,946 Water / 629,842 Road
triangles in both cold generations, with zero writes on no-op/locality. The old
profile predates that child. Four unconditional zero-valued assembly diagnostics
were also absent from all three building expectations. Independent review
confirmed these exact metadata corrections; no geometry or selected authority
changed. Diagnostic attribution: `tmp/world/terrain_execute_d44/kazan-metric-attribution.json`.
The existing exact layer predicates now also run immediately after the first
native receipt. RED dispatched an expensive projection for an already mismatched
receipt; GREEN rejects before any later dispatch and restores the original map,
absent second map and protected neighbor (0.02 s). Final comparisons remain strict.
Replacement Check `check-20261113T193829Z` PASS (231 E2E and all other suites),
input re-admission PASS (776.87 s). Matrix `run-20261113T200249Z` completed
both cities' native legs, then rejected an obsolete final reader predicate:
Map comparison 2 was required to equal 1. Exact restoration PASS (3,552 files,
HEAD and index unchanged, declared source-profile correction retained).
The reader now requires Map 1/2 and Building massing 2/1 independently. Existing
saved-projection tests give RED/GREEN and retain collision-drift/old/new revision
refusals. Diagnostic replay of both cities' full layer and saved Terrain/World
comparisons PASS: `tmp/world/terrain_execute_d44/matrix-native-recheck.json`.
This is not an accepted Matrix. Final refusals now retain completed execution
evidence without admitting it; fresh Check/Matrix are required. No model or
source authority changed (still 3/10). An isolated check-drift test's broad JSON
mock was corrected without changing its guard.
Fresh Check `check-20261113T210413Z` passed Canonical 340, E2E 232, lifecycle
329, environment/source/architecture and Twin observers, then timed out in the
fixed native host at 180 s. Its log has 98 successful completions and active
saved-consumer work at cutoff, after about 63 s startup. Protected restoration
PASS. The bounded aggregate host cap is corrected to 360 s (5 focused Pester
tests RED/GREEN); no test, assertion, timeout cleanup or failure was waived.
Direct complete cold suite PASS: 149/149, 200.4 s, zero failures, exact protected
state preserved (`tmp/world/terrain_execute_d44/native-cold-budget-r1/`). The
fresh Check `check-20261113T214737Z` PASS: all eight suites, including native
149/149 (122.1 s). Pre-Matrix protected check PASS. The sequenced run in
`tmp/world/terrain_execute_d44/continue_acceptance.ps1` owns the acceptance sequence.
Matrix `run-20261113T221804Z` PASS, with exact generated-root restoration
and protected 3,552-file/HEAD/index checks. Target `refinement-20261113T230816Z`
REJECTED at retained Kazan navigation, then restored. Retained-map probes localize
external-nav reconciliation, load-time tile defaults and obsolete full-refresh
compatibility; fixes have focused native/PowerShell proof and independent review.
Probe `nav-base-probe-r3` now commits Kazan base Apply, then refuses Manhattan's
missing Structures manifest before mutation. It restores the original protected
inventory exactly, apart from the separately pinned inert child archive retained
by canonical preparation. No canonical pointer was promoted.

Current integration fixes also add qualified temporary missing-layer preparation
inside the existing restoring owner, and actual saved-city Hero walking. The normal
Manhattan tour is kilometres from Pier25, so it cannot substitute for those contacts.
Guard/recovery tests PASS; saved-product execution remains pending. Existing regional
Hero regression PASS again, 149.8 s / 5 routes / 5,081 walking frames. Current code
requires a fresh Check and matching base Matrix after the retained-base discriminator
passes. Do not reuse the earlier receipts as proof of these new paths.
No additional physical model was attempted; the count remains 3/10.
Fresh Check `check-20261114T024156Z` PASS (340 canonical, 234 E2E, 355 lifecycle,
151 native and all other suites). Base Matrix `run-20261114T030642Z` PASS for
both cities; exact restoration and independent receipt review PASS. These are
historical after the audit-only fix below, not current-code acceptance.
Selected target `refinement-20261114T035531Z` admitted the same candidate and
passed both normal base transactions plus the strict 15-scope cold audit.
Selected native Apply PASS: all 210 cells, six layers and two Pier structures,
546.31 s. Its postbuild audit then REFUSED actual refined coverage: ordinary
201,810 samples match, but all 2,308 refined-cell triangles fail complete-field
comparison, whose first failure reports overlap.
Fallback refinement was not executed after the first refusal; zero fallback
counts are not a PASS. Original authority/files restored exactly; HEAD/index
unchanged, with only the already declared profile edit and inert archive retained.
Independent source review localized the audit's duplicate RVT component, not a
new physical model defect. Actual native `AddStaticMesh` reproduces the same
overlap RED in 33.6 s; primary-inventory selection and independent variant checks
GREEN in 34.0 s, including genuine duplicate-primary controls. Geometry and
fidelity tolerances are unchanged. Current input admission `selected-inputs-r6.json`
still has identical reader/profile identities. Refreshed Check
`check-20261114T060103Z` PASS: all eight suites, including 152 native tests in
113.7 s, the unchanged Twin baseline, 340 canonical, 234 E2E and 355 lifecycle
tests. Restoring base Matrix `run-20261114T062541Z` PASS for both cities with
independent receipt review and exact restoration. Selected target
`refinement-20261114T071546Z` proved both actual saved visual fields: 2,308
refined triangles per variant and 201,810 ordinary samples, zero mismatches.
Post-CAS controlled failure and prepared-base recovery passed; normal publication
also completed. The outer host had already rejected after 7,200 s while its
delegated worker continued; no saved gameplay/streaming acceptance followed.
Existing recovery restored all 3,553 expected files, HEAD and index exactly and
removed the marker. The actual process-tree timeout regression is RED; the shared
Job supervisor is GREEN (four real-process tests, seven existing Pester controls).
Compiled Job code now participates in Common identity. The composed selected-only
budget is separate from unchanged native deadlines. Fresh input admission,
Check/Matrix and selected-consumer acceptance are required; earlier receipts
remain historical evidence. Still 3/10 physical models.
The scoped native-worker retirement repair passed 10 process controls and the
actual 152-test cold suite (114.8 s). Fresh Check `check-20261114T105947Z`
PASS (340 canonical, 239 E2E, 355 lifecycle, 152 native); base Matrix
`run-20261114T112514Z` PASS with independent 19-receipt review and exact restore.
Selected target `refinement-20261114T121356Z` passed real post-CAS recovery,
normal publication, cold no-op (all actor counters and dirty units zero), both
saved projections, both partition audits, and actual saved-city Hero walking:
Manhattan five bidirectional routes / 5,081 frames / 1.55 m visible-Water
clearance; Kazan 1,767 frames / accepted bank route. Foreign-floor controls PASS.
It then REJECTED capture path resolution: the public script prepended the repo
to an already absolute plan. No captures or Both-tour acceptance followed.
Exact protected 3,553-file/HEAD/index restoration PASS; marker removed. The
shared capture resolver has a faithful absolute-path RED and GREEN, and is now
used before mutation. Preserve these native receipts as bounded evidence;
the current contract cannot upgrade a rejected prefix to an accepted target.
Fresh Check/Matrix/target are required after this script fix. Still 3/10 models;
do not repeat source research or change the passing geometry.
Request the complete selected consumer branch with the current authenticated
runtime vantage plan, including explicit saved gameplay references for both cities.
The default publication/recovery-only target cannot close city gameplay or
streaming. See the acceptance owner's selected-refinement contract.

1. **Player baseline first.** Bind the exact Ground/Water/Pier candidate and its
   hashes, scene, normal presentation and player camera. Capture the access and
   shoreline from actual player height; audit default spawn and nearby ordinary
   routes, then perform a short support/movement check. Distinguish the current
   saved World, an old regional scene and the new Ground candidate explicitly.
   A defect in an unselected fixture does not prove shipping-player exposure.
   Use the existing regional scene/capture owners; do not rebuild a full city to
   obtain this baseline. Missing evidence is UNVERIFIED, not a reason to invent
   impact or begin another source campaign.
2. **One physical hypothesis per iteration.** Prefer a bounded inferred slope
   and adapted landing using existing canonical mechanisms. Reuse the tracked
   pre-Water input, dry/wet controls and closed-neighbor checks. Prove the complete
   transition and outgoing paths, source preservation outside it, semantic weights,
   Water support and independent constructed ownership. Add only a demonstrated
   missing owner capability after a credible offline result. A decorative apron,
   unqualified vertical Terrain face or repeated shoreline shift is not a fix.
3. **Count and review every attempted model.** One iteration includes one bounded
   physical change, its focused checks, measurements and reviewer disposition.
   Rejected offline candidates count. Code edits, test invocations and screenshots
   are not separate iterations. Compare like-for-like source departure, dry support,
   contact errors, outer closure, grades and player results as applicable; keep
   observed source and inferred geometry distinct. More documentation, diagnostic
   tooling or green tests alone do not justify extending the budget. A tooling
   obstacle must be recorded and bounded, not turned into an open-ended workstream.
4. **Inspect the actual result.** Run only affected native scene checks when the
   candidate is ready; keep Unreal serialized. At the stop checkpoint, present
   normal-render player-perspective before/after views, short actual-Hero support
   evidence, exact input/output identities, changed outputs and restoration.
   Include a Ground-only diagnostic view where a structure could conceal damage.
   A rejected model remains rejected; show the authenticated baseline or last
   valid saved candidate when a replacement cannot be admitted. Mark untested
   properties explicitly. Screenshots alone cannot waive collision or route failures.
5. **Make one explicit disposition.** Use the table below and stop local model
   experiments. Preserve the tiny replay, failed hypotheses, exact camera/scene
   receipts and reopening condition. If local acceptance passes, compose the
   accepted Ground/Water/Pier through existing owners, verify changed consumers,
   then run the required restoring Manhattan/Kazan L2. Full-city checks are final
   gates, not per-iteration debugging. The existing expensive-gate budget still
   applies; a later geometry change consumes a model iteration and cannot silently
   exceed the ceiling.

| Local disposition | Required evidence and consequence |
|---|---|
| Correction accepted | Complete physical/visual local proof on the new candidate; proceed to integration and both-city L2. Local PASS alone does not close Terrain or release. |
| Tolerable known issue -> backlog | Player-level appearance is acceptable and the defect is noncritical or demonstrably contained outside required spawn/traversal. Record exact affected area, candidate, consumers/routes, limitations and replay. Keep main Terrain active for other required proof; any change to an existing required geographic release case still needs an explicit release-scope decision. |
| Release blocker | Broken default spawn, required traversal/support, falling through geometry, or a serious uncontained defect. Report the exact physical counterexample and recommended product decision; do not hide it, silently exclude it or continue past the budget. |

Record one row per attempted candidate here, including refused candidates; no
separate tracking service or parallel task is needed.

| Iteration | Hypothesis / model and input-output identities | Measured result / player evidence | Reviewer progress and next decision |
|---|---|---|---|
| 1 - REJECTED | Fixed Water, 5 m source-aware bank with finite north/south blend. D39 v2 input `20cbb8d9fb1d993cf07876f52625c9b2fb9efa8067029919ffee1fab5ee33f66`; `tmp/world/terrain_execute_d44/iteration1-r2/report.json` | 35 quads, 2,428 triangles, 6.20 s; unchanged exterior/seams, but near-collinear interpolation produced invalid gradient ~1.92e10. No canonical/native admission. Baseline `baseline/native` PASS 141.3 s and `baseline/view` PASS 76 s are Ground/Water-only, not joint Pier acceptance. | Reject nonlinear nodal blend; next use an exact piecewise-planar envelope. Numerical mesh retries remain this same model attempt. |
| 2 - REJECTED JOINT MODEL | Exact affine bank, same Ground/Water; `tmp/world/terrain_execute_d44/iteration2-r2/report.json`; adapted 18 m north and 5 m south approaches | 35 quads, 456 triangles, maximum gradient 0.60744, closed exterior/seams, 1.74 s construction. Complete joint cover exposed a 0.001597 m deck penetration at `[583380.08291,4508173.20263]`; witness in `iteration2-r2/pier-contact-counterexample.json`. No native admission. | Retain bank construction; change only the northern final approach in attempt 3. |
| 3 - LOCAL PASS | Same bank; final 5 m north contact transition with 0.025 m allowance. Joint result `f082b27f781b01100700d97af555b843f0d6c779ae86f0e3e204944acd56751c`; prepared authority `b1fb98234ecd94d313882738d0ca36bd2a670a8ebdc4643350bfabc033866d6b` | 35 quads/456 bank triangles, complete dry/wet cover and closed seams; regular Ground artifacts unchanged from D39. Native 4,230 faces x3 variants, max error 0.000047245 m, PASS 80.7 s; settled views 64.7 s plus close contacts 66.2 s. Actual Hero: 5 bidirectional affected/source and explicitly inferred cap routes, 5,081 walking frames, 1.55 m minimum support above visible Water, PASS 151.3 s. Evidence: `tmp/world/terrain_execute_d44/{joint-input/publication-report.json,joint-native-r2/sections.json,joint-view-r2/,contact-view/,joint-gameplay/gameplay.json}`. Preparation 255.22 s; deterministic authority replay 416.20 s. | Independent engineering review PASS within bounded scope. Proceed to normal six-layer profile, restoring both-city Matrix and selected-child recovery/no-op. No city, streaming or release acceptance yet. |

## Acceptance checkpoint

Use the existing [deferred-case protocol](../../../docs/agents/scientific_debugging.md#resuming-a-deferred-investigation).
The evidence index below retains exact inputs, rejected witnesses and proof
limits. This checkpoint selects where work resumes, without another source search.

| Case / retained result | Resume action and reopening condition | Stop boundary |
|---|---|---|
| Fountain: coherent Ground/native fidelity PASS; D48 accepts the sampled 7-15 cm raised Water boundary as prototype object context. | No further Terrain investigation. Future object reconstruction uses final Terrain support; lighting has its separate current stub. | No surveyed basin claim, new walkable-support admission, EV8 adoption or production promotion. Final release proof remains separate. |
| Pier25: integrated physical, two contact views and actual saved-city Hero PASS. | Retain the current model and five saved-city contact routes. The 648.9 m regional corridor is a separate bounded fixture, not an actual-city corridor claim. | No repeated unchanged source/deck work or automatic acceptance of every OSM way. Native city consumers and streaming already passed on this candidate. |
| Kazan: actual saved-city supported bank Hero and native city consumers PASS. | Retain the unchanged qualified child and exact accepted interval. The neighboring submerged mapped feature is contextual, not an admitted playable route; requalify only if a future consumer selects it as support. | Do not restart bank fitting, bathymetry or constant-stage experiments, or extend the interval silently. |

- [x] Bind the reusable Hero/movement helpers to the authenticated saved regional
  candidates and explicit routes/support. Integration and actual retained-scene
  traversal are implemented:
  see [gameplay applicability](../../../Plugins/World/ProjectWorldMeshTerrain/Tests/README.md#gameplay-applicability).
  Pier25 full access/both shore joins and the supported Kazan bank were walked by
  the actual Hero. The contact-envelope calibration correction passes; retain
  navigation/streaming and changed-baseline proof as separately identified checks;
  their integrated native executions now pass in the current checkpoint.
  A reconstructed generic mesh scene is not the same acceptance surface.
- [x] Retain and hash-check the current evidence. If scratch scenes/receipts are
  later missing, mark that proof UNVERIFIED and reconstruct only its affected
  owner; do not invent results, substitute source epochs or infer that a new
  geographic model is required.
- [x] Qualify the complete local shore transition, unaffected exterior traces
  and separate adapted Pier access, then execute restoring native both-city
  consumers. D44 model 3 and the current checkpoint retain those proofs.
- [x] Dispose the fountain's small raised Water edge as admitted temporary object
  context (D48). Retain later normal-presentation requirements in the release
  plan; source coverage and Terrain development are closed for this scope.

Optional preparation-cost, Terrain-influence and building backlogs retain their
own measured/consumer triggers and pause conditions. They are not alternate ways
to reopen the completed fountain investigation. Gameplay cost is measured below
for the retained routes. D48 closes this development scope; final release
certification remains separate.

### Historical D39/D40 witnesses

These rejected-model witnesses precede D44's authorized bounded dry transition.
They preserve evidence, not current restart instructions or a requirement to
build a wall. The current checkpoint above governs resumption.

**Conditional D39 geometric witness; D40 changes its interpretation.** The saved R3 deck has 80 distinct
vertices; its four dry terminal vertices lose 1.59-3.11 m between qualified
pre-Water PWA Ground and the final coarse Water-conditioned field. At the inland
contact, preserving source-resampled Ground and keeping the exact mapped Water
boundary at inferred stage requires a bank grade of at least 57.93 m/m over
0.05345 m. This is a retaining-edge interpretation, not a gentle bank or a
surveyed coastline. The existing simple exact-footprint clipping comparison does
not fix the contacts and does not close its retained outer trace. Do not promote
that candidate, lower the deck to conceal erased land, add another source search,
or bypass the single-valued Terrain/outer-edge guards.

**D40 alignment disposition: UNVERIFIED; no wall built.** The proposed OSM edge
is `[583366.48,4508116.95] -> [583375.54,4508178.23]`, about 62 m long. NYC's
2017 NIR-not-tidally-coordinated lines do not observe that root segment itself.
The nearest retained line positions are 5.40/7.28/18.75/18.13 m from the four dry
contacts; these are largely endpoint distances, not a continuous wall survey.
Five adjacent northern quay sections put the current Water edge 17.61-18.39 m
east of the NYC line, versus only 0.28-0.42 m at four southern control sections.
The 2019 drawing labels nearby bulkheads and separate pier faces,
but its shoreline positions are approximate imagery-derived context. It does
not locate a hidden wall under the pier root.

These facts do not prove OSM wrong or exclude a hidden bulkhead/apron. They do
prevent treating the current Water-mask edge as the independently located wall.
The 57.93 m/m bound above is conditional on that mask, not physical evidence that
a wall belongs there. D40's placement prerequisite is unmet; no vertical-wall
producer, Terrain restoration or Water move is justified by this check.

Reopen with a qualified local shore-role/position interpretation, or an explicitly
accepted authored shoreline layout that distinguishes bulkhead, apron and Water.
Recommend settling that bounded XY relationship before another height correction;
retain the existing Water stage unless separately qualified. Use the captured
contacts and existing canonical/native owners. Do not repeat Ground acquisition,
the fountain proof, the refused clipping model or a full-city run to discover
the same gap. Both-city L2 remains gated by acceptable local dispositions.

## Fountain product decision

**Current disposition: accepted as prototype object context (D48).**
The coherent Ground candidate has completed its development physical checks.
The operator does not require detailed fountain reconstruction or further
investigation for Terrain closure. The fountain/plaza is not geographically
excluded. Future object construction uses final Terrain height/support and must
qualify its basin and contained Water separately. Presentation and final
production/release selection retain their own gates.

The table below preserves earlier decision inputs, not an outstanding request
for source acquisition, an exclusion decision or another Ground model.

| Decision input | Verified state / consequence |
|---|---|
| Required concern | Fountain/plaza/access around 1345 Avenue of the Americas remains required. The diagnostic basin bounds are `[586193.44, 4512906.20, 586224.06, 4512954.64]` in canonical projected metres. The 15 m access probe and larger study domains are diagnostic, not an operator-approved playable or exclusion boundary. |
| Retained production defect / new candidate | At `(586198.6, 4512911)`, retained Terrain/Water are approximately 168.28/168.3 m. D39's coherent candidate has physical Terrain 18.8987 m and Water 19.0 m, with native fidelity, collision, reload and normal-presentation PIE evidence. Source processing agreement does not prove present-day geographic accuracy. |
| Rejected alternatives | The 5.4747 km2 authored comparison and 5.9157 km2 Ground-first candidate have unacceptable joins; the latter introduces 47 new >100% faces and a 98.45-degree crease. Exact witnesses, source support and reproduction scripts are retained below. Unknown stencils remain unknown. Neither rejected model is selected; D44's coherent candidate is tested separately. |
| D38 coherent-base feasibility | Five authenticated NOAA 9307 objects do not supply a complete Ground field for the unchanged 210-cell domain. Of 202,710 original-grid nodes, 104,787 are finite and 97,923 unknown; 64 cells are complete, 66 partial and 80 entirely unknown. Fountain Ground is locally finite at about 20.215 m, but this does not qualify a whole-world replacement or a new Water stage. |
| D39 source qualification | Two metadata-selected USGS seamless 1/3 arc-second bare-earth products cover every one of the 202,710 core and 1,806 halo stations with complete original-pixel bilinear support. New raster acquisition totals 661,900,704 bytes. This closes the measured source-coverage gap; source admission and Terrain/Water quality remain separate. |
| Retain as required | D39 selects bounded source qualification without excluding geography. Integrate only a qualified coherent Ground-before-Water model; diagnose strict finalization refusals against the original-input control. Do not silently retain roof-influenced DSM, fill unknowns or accept a Water-conditioning policy from finite samples alone. |
| Explicitly defer from development | Requires an operator-defined excluded area/access scope and affected consumer/spawn/route audit. Until then, no exclusion is assumed. Other local gameplay and city acceptance still have to pass. |
| Release consequence | A development-only exclusion does not waive the existing 3.0.0 R2 concern or R5 admission. Any release-scope change is a separate operator decision. |

D48 supersedes the earlier hold on prototype fountain fidelity. Do not relabel
rejected Ground models as accepted or invent grades; retain the verified coherent
Ground and defer only detailed object reconstruction, not a geographic region.

## Source-model research handoff

This is the retained source-selection record. D44 has already admitted the
coherent candidate for restoring native verification; it is not a direction to
repeat feasibility, acquisition or model preparation. The complete D39 field
uses the two qualified USGS products below. NYC/NOAA 2017 data remain retained
comparisons/local references, not a claim that their cached coverage supplies
the entire coherent field. Permanent production selection remains protected.

Documentation research and D38's cached-only pass are complete. D39's additional
USGS evidence closes the full-domain sampling gap; it has not accepted a Terrain
replacement. The
[source owner](../../../tools/World/SourceIngestion/elevation_sources.md) retains the
verified NYC/Copernicus product distinctions, bridge-artifact treatment, frame and
epoch cautions, 2010 rights condition and limits of an independent 3DEP check.
D38/D39 own this closure run and its stop conditions; no new model is admitted.

The retained cached mosaic already uses five objects from NOAA's 2017
topobathymetric bare-earth dataset 9307; "try the 2017 DEM" alone is not a new
premise. Their identity with NYC's named Base Bare Earth product is not
established. The exact input inventory and support evidence are indexed under
the single outward comparison below.
That comparison retained DSM boundary heights. A proposed coherent Ground base
must explicitly change that modeling constraint and define its playable domain.
The old cache gap and new provider-field support are measured separately below;
neither alone establishes an acceptable Ground/Water baseline.

- [x] Verify provider documentation and existing implementation limits. D39 adds
  closed coherent Ground replacement through existing canonical refinement,
  without a generic NYC adapter or priority selector. Source admission and
  geographic acceptance remain distinct.
- [x] Run D38's ONE read-only L0 feasibility check using retained 2017 evidence for the
  [Manhattan canonical domain](../../../Plugins/World/ProjectWorldData/Data/Profiles/CanonicalCompilation/manhattan_showcase_v1.compile.json).
  Inventory exact NOAA/NYC product lineage; check valid samples, gaps, frames,
  resampling, representative dry streets, Water interfaces and required outer
  boundaries. Test the coherent Ground premise, not another internal DSM splice.
  Result: that cache cannot support the complete baseline. Its source-acquisition
  stop was subsequently superseded by D39; do not rerun the unchanged cache test.
- [x] Perform D39's metadata-first targeted acquisition and native support check.
  Preserve the full domain, exact source objects, frame operation, complete raw
  kernels/masks and independent numerical control. Evidence below.
- [x] Integrate the qualified coherent pre-Water model through existing owners,
  with original-input Water/finalization controls, exact no-op/frozen replay and
  selected native fountain fidelity/appearance. Model quality is still open.
- [ ] Conditional on a future implementation or missing regression consuming the
  rejected join: preserve its smallest faithful input and offline replay as a
  tracked fixture, retaining source provenance, masks and necessary neighbors.
  Reuse the Madison fixture where applicable; it does not reproduce the broader
  fountain join. This is not a prerequisite for pausing or source feasibility.
  A committed replay must not depend on tmp inputs or a new generic framework.
- [x] Route public-checkout agents through the existing World quick start to the
  tracked skill, agent/dev SOT and task lifecycle. Root AGENTS.md exclusion alone
  does not prove knowledge loss. Local mirror-rule checks cover these routes;
  actual remote publication remains unverified and is not part of this work.

## Remaining implementation

D34's [authored dry Terrain](../../../tools/World/CanonicalCompilation/terrain_refinement.md#authored-dry-terrain)
workflow is implemented; the [Madison fixture](../../../Plugins/World/ProjectWorldTestData/Data/Fixtures/madison_dry_ground/README.md)
and [native owner](../../../Plugins/World/ProjectWorldMeshTerrain/README.md) own its
regressions and replay. This does not accept the radial +5.1 m model: six faces
steepen and the largest incident/external crease grows 15.3 -> 25.0 degrees.
The complete-cell native proof passed in 107.3 s, including saved/reloaded geometry
and 19 collision points. Retained evidence is indexed below.

D35/D36's implemented boundaries and unsupported cases now belong to the Assembly
owners above. Complete geography acceptance and T2 optimization remain separate
from this registered building lifecycle proof.

### T1/T3 - Preserve completed correction evidence

- [x] Finish the one authorized outward Ground-first comparison and refuse its
  unsuitable candidate before native work. That D37 premise is closed; D39's
  coherent whole-domain base is a different authorized model. Earlier local
  splices remain unselected and must not restart.
- [x] D39's coherent model reuses existing
  [Ground qualification](../../../tools/World/SourceIngestion/supplemental/ground_products.md)
  and [coherent canonical construction](../../../tools/World/CanonicalCompilation/terrain_refinement.md#coherent-qualified-ground-base).
  This is complete original-domain replacement, not regional pre-Water selection.
  The old fixture's assumed 20.2 m stage is not imported; production refusals stay.
- [x] Complete Pier25's combined two-cell Terrain/Water/deck native scene and full
  shore-cap runtime views, including independent local retain review.
  Projection, registered producer, isolated Terrain/restored combined physics and
  replay contracts belong to the [regional test owner](../../../Plugins/World/ProjectWorldMeshTerrain/Tests/README.md).
  Retain this as a local inferred candidate with the bounded Hero proofs above;
  changed dependencies and geographic admission stay open.
- [ ] For a demonstrated acceptance failure, capture/reduce the actual owner input
  into a permanent faithful RED fixture before repairing that owner. Preserve
  coordinates, source roles, joins and necessary neighbors. Extend an operation
  or dependent composition only if that input proves it necessary.
- [ ] If a selected candidate needs it, independently qualify extra conditioning of Water
  `alis:osm:way:452617299`. Historical base finalization requires new lowering
  (six nodes, up to 13.7 m), with only low-confidence point-fallback support.
  The nearest edited Madison node is 522 m away. Retaining unchanged Water in
  a dry-disjoint child neither admits these cuts nor makes this the sole next job.

### T2 - Completed optimization and deferred cost work

- [x] Remove the demonstrated repeated raw-geometry work in Water dirty selection
  using its existing prepared-geometry owner. Exact same real fit and dirty scope:
  409.096 -> 24.645 s, 39 selected features, 38 changed functions, 16 cells.
  RED: 90,168 segment-distance calls for one disjoint footprint. GREEN: 27 focused
  Water/Pier tests, 16.617 s; independent review PASS. This does not localize base
  parsing, avoid the complete Water prepass, or accept conditioning policy.
Remaining authenticated-read/copy performance work is moved to the
CanonicalCompilation preparation-cost backlog. It is not an acceptance blocker
without a measured verification obstacle; do not optimize speculatively here.

### T4/T5 - Product acceptance and subsequent milestone

- [x] Retain the rejected radial model's per-face/crease evidence above. It is
  unselected and must not restart the geographic investigation.
- [x] Verify affected native Water, placements, navigation, partition/streaming
  and actual saved-city Hero consumers. The current integrated checkpoint binds
  their exact inputs and outputs; regional fixtures alone are not city proof.
- [x] Run isolated/restoring native Kazan and Manhattan milestones and measure
  costs. The current checkpoint preserves the original rejected reader result
  and separate authenticated reinspection; it is not fresh release certification.
- [x] Prove coherent Ground, Water and separate Pier structures in one admitted
  canonical/World candidate through the registered six-layer Manhattan profile,
  transaction, failed-commit recovery and cold no-op. Preserve qualified temporary
  missing-layer enrollment and stale-support guards; permanent production
  enrollment is not authorized by a restoring candidate proof.
- [x] Complete the fountain disposition: D48 accepts prototype object context
  using the retained source-bound evidence. Detailed reconstruction is deferred.
  Release presentation, final operator acceptance and promotion stay separate.
- [x] Repair effective-input admission without dropping Kazan's selected child.
  The existing immutable route now authenticates base plus carried child, and
  binds the actual effective World input. Real two-city admission preflight PASS.
  This is not Matrix acceptance; `refinement-target` still needs current Common
  Check and the matching Matrix. The frozen reader cannot mint that proof.
  See the [acceptance owner](../../../tools/World/EndToEndValidation/README.md#immutable-artifact-consumer-acceptance).
  Do not advance the true slice base or treat a target receipt as a full Matrix.

Conditional Terrain-influence work is moved to its World consumer backlog.
Its ownership direction remains in the territory contract; no influence API or
general scenario framework is implemented by this acceptance checkpoint.

### Building follow-ups - paused by D37

The five extension steps are retained in the World backlog entry
`world_extend_building_assembly`. Resume only after the accepted Terrain/Water
baseline and operator direction. Current contracts remain with the
[Assembly owner](../../../Plugins/World/ProjectBuildingAssembly/docs/architecture/structure.md).

## Verification and completion

For each new case: faithful RED -> smallest owner fix -> focused GREEN -> affected
consumer checks -> authenticated replay. Run ordinary iterations through the
linked small fixtures; full-city Check/Matrix/cook belongs to milestones.

The three development case dispositions are complete as recorded above. This
record does not certify every mapped object, select the candidate permanently
or replace the release plan's remaining presentation, packaged and human gates.

## Review disposition and retained evidence

- D44 review disposition: the supplied D43 review is sound on the physical
  height conflict, finite-transition option, landing adaptation and missing
  integrated/L2 proof. No demonstrated defect requires another production-code
  patch to the diagnostic implementation. Its instruction to continue until
  acceptance is bounded by the operator's five-iteration checkpoint and absolute
  ten-iteration ceiling above. Reviewer preference cannot waive that ceiling or
  require a new structural producer before an actual model demonstrates the need.
  The current change records strategy only; no screenshot, player-impact result,
  new geometry, iteration or acceptance is claimed.

- D43 (`72eee77e`): the BACKLOG/CURRENT inconsistency is valid and repaired;
  this required task and release links are restored to `00_current`. D42 did not
  claim Terrain accepted, and the operator explicitly allowed backlog, so the
  move was not an unauthorized waiver. Resuming the release priority still
  requires the missing new-scene and both-city proofs; task location proves none.
  - Existing sparse `_quad` lowers shoreline vertices on dry pieces as well as
    wet pieces. The new `--sparse` route on the tracked shore fixture starts
    from the captured pre-Water affine Ground, keeps Source/deck/modeled heights
    distinct, integrates dry-only cut volume and checks retained outer edges.
    It is a diagnostic of a refused construction, not a new correction writer.
  - `tmp/world/terrain_review_d43/sparse-comparison.json`: 15 original quads /
    58 triangles; about 1.3 s. About 3,206.94 m3 is removed from 7,637.39 m2 of
    dry pieces. Source departures at the four contacts are 1.359/1.484/3.085/
    3.087 m. Three distinct exterior height positions and two new exterior
    subdivisions refuse. D39's six height witnesses were incidences at three
    unique XY nodes, not six distinct positions.
  - Source limiting heights at the nearest Water boundary are
    0.6063/0.7316/2.1059/2.0956 m against the unchanged -1 m wet bound. Continuity
    cannot preserve both traces. The 57.93 m/m cap-to-water bound is conditional
    model evidence, not a surveyed wall or a universal slope threshold. The
    review's literal dry-preservation requirement needs this qualification.
  - Three focused diagnostic tests and eight existing Terrain/surface-refinement
    tests PASS in 8.27 s including launch. `focused-regressions-final.json` pins
    the exact modules. A matching
    low-Ground/low-outer control removes dry height loss, but correctly retains
    neighbor-subdivision refusals; it does not pass old deck contacts or accept
    a World. The first control assertion incorrectly expected topology closure
    from equal heights; corrected that assertion without relaxing geometry.
    Default no-native replay remains about 4.7 ms. All prior geometry/acceptance
    proofs remain scoped to their original inputs; no new city/Unreal run occurs.
    A wet-only control lowers three affine half-quads by 1,350 m3 while dry loss
    remains zero. Deliberately substituting total wet+dry loss for dry loss makes
    that test fail as expected; the actual observer passes. These tests diagnose
    refused geometry and the observer, not Terrain closure.

- D42 (`2c862aa1`): the engineering-tool limitation is confirmed and fixed in
  the existing E2E owner, without a new mode or validator framework. Base history
  and effective child identities remain distinct; carried archives, consumed
  result hashes, enrollment/completion bindings and reader/schema dependencies
  are checked. Fresh target candidates still require current producers, and
  replacing an already selected target child refuses. Production pointers stay
  unchanged. Evidence: `tmp/world/terrain_unblock_d42/`.
  - Five initial focused tests fail for the expected admission, result-family
    and reader-fingerprint reasons, then seven focused tests pass. Existing
    consumer/refinement/enrollment/completion regressions and real-base pin
    refusals pass. One budget-test mock lacked its real operation/schema fields;
    repaired that mock and reran its affected Matrix module. Exact logs/costs
    are retained under the evidence root; none is a city L2 receipt.
  - The actual selected Kazan/Manhattan immutable admission plan passes in
    82.94 s. It retains Kazan's child and selects restoring L2; no L3/L4 work or
    geography acceptance follows. This removes D41's tooling limitation, not
    the physical shore/composition requirements.
    The final explicit effective-input snapshot passes in 76.93 s and records
    both original base identities plus Kazan's actual child result separately;
    `effective-selection-receipt.json` verifies unchanged production pointers.
  - `shore/report.json`: one 18 m inferred westward shoreline comparison in
    3.79 s, unchanged -1 m Water stage, valid native polygon partition, exact
    original-field control. Only three core nodes change. Four actual saved
    cap positions remain 0.899/0.893/1.829/1.812 m from their deck contacts even
    17-20 m inland of the inferred edge. REFUSED before Unreal; no publication.
    This does not prove every authored shore model impossible. It establishes
    that an inferred edge choice alone does not fix coarse support erosion.
  - Portable captured replay:
    [shore fixture](../../../tools/World/CanonicalCompilation/tests/pier_shore_fixture.py).
    It retains a 63-node Ground halo, clipped original/authored Water, the four
    contacts, exact original-core control and source/model hashes in about
    7.5 KB. Replay takes about 3.5 ms with no cache, network, GDAL or Unreal.
    Its REFUSED diagnostic is a starting input, never an accepted World.
    Changing a captured dry Ground node is observed to fail the original-field
    control; restoring that input exactly reproduces the comparison.
  - Next: use that input for one local, explicitly inferred shore field with
    complete landing/bank/perimeter and incoming path checks. Preserve single
    Terrain/Water authority, outer traces and explicit source/inference roles.
    Add only its demonstrated typed admission and Ground/structure composition
    after offline success, then native changed-scene Hero/visual/reload and L2.
    A wide top-only apron hides the inland hole; near-vertical Ground is not a
    supported wall. Do not call either a KISS solution merely because it walks.
  - Verification covers 124 distinct focused tests across the new effective-input
    tests, real-base pins, candidate/target, completion/enrollment, Matrix,
    dispatch/planning and scope modules. The initially failing budget mock is
    repaired; its entire affected Matrix module passes on rerun. No Common
    Check, full city Matrix or new-scene Hero result is claimed. ASCII, Python
    syntax, schemas, local links/anchors and diff checks pass. All 3,123 protected
    files, 641 Ground candidate outputs, HEAD and staged bytes stay unchanged.

- D41 review/fix request (`b4608ef0`): F1 shoreline and F2 combined acceptance
  gaps are valid. D40's unqualified wall location remains a stop, not permission
  to adopt a different inferred layout. No further source campaign, promotion,
  packaging or Git operations are authorized by the reviewer's embedded handoff.
  Full upstream replay is required when certifying fresh producer output; it is
  not universally required for unchanged authenticated artifact consumption.
  The existing consumer controller nevertheless cannot accept the current
  two-city effective selection, as the selected-child preflight demonstrates.
  L2 restoring verification, L3 durable authority and L4 packaging remain separate.
  - Final-oracle full-route reruns: `tmp/world/terrain_review_d41/gameplay/`.
    Pier 648.914660 m both ways: 15,645 walking frames / 560.9 s; Kazan supported
    73.114284 m bank both ways: 1,767 frames / 144.6 s. Actual Hero, owning-floor
    contact, capsule/support, Water clearance, unsupported falling controls,
    runtime census and PIE cleanup PASS. `postflight.json` verifies all 21/14
    saved files and exact route/input/native pins unchanged. These are retained
    OLD scenes, not acceptance of the new Ground or complete city Worlds.
  - Selected Manhattan consumer audit: normal authored overlay has zero overlays
    and zero active surface-snap anchors. The sabotage profile's one missing-
    feature anchor is a deliberate refusal control, not a required placement.
    All three selected geodetic controls explicitly have unknown Z. Runtime
    spawn/route and ordinary Building/Vegetation placement sample finite final
    geometry without requiring source vertical accuracy. Unknown accuracy remains
    unknown; no selected authored placement is newly blocked by it. This does not
    prove spawn, support or changed-baseline gameplay acceptance.
  - The immutable-base test fixture copied current effective authority pointers
    while retaining only base archives, and its relative schema link resolved to
    a fixture copy rather than the executing schema owner. Corrected only this
    private fixture to pin real base lineage and the actual owned schema.
    Production pointers and selected-child refusal remain unchanged. Observed
    positive-test RED 1.569 s -> GREEN 125.474 s; all six affected module tests
    PASS in 246.45 s including the selected-child and tampered-input refusals.
    ASCII, local links/anchors, Python syntax and diff checks PASS. All 3,123
    protected files, 641 candidate outputs, HEAD and staged state are unchanged.
    Verification and
    exact input pins are retained under `tmp/world/terrain_review_d41/audit/`.

- D40 alignment check: `tmp/world/terrain_closure_d40/shoreline/` retains exact
  Socrata metadata, the 24,492,817-byte NYC vector ZIP, its original State Plane
  NAD83 US-foot reference, the projected local linework and 6,526,594-byte HRP
  survey PDF. This is vector/document acquisition, not another DEM download or
  geographic expansion. `alignment-report.json` pins source bytes, all four
  actual R3 dry contacts, their closest line positions and proposed OSM segment.
  Native GIS overlay/metrics take 0.87 s; `alignment.png` shows Water/deck/lines.
  `section-alignment.json` adds the nine observed adjacent sections and southern
  control. It does not invent a line across the unobserved root gap.
  - `survey-page42-pdfium.png` is the inspected complete survey map. Windows'
    PDF renderer omitted most map content; its nearly blank output is not source
    evidence. Scratch-only PDFium reader 5.15.2, wheel SHA
    `74d1bace34fb01c660d0b64344d5a4b7d276942b7c6623c88ab59ea21c5acfbd`,
    correctly renders the retained PDF. No project dependency was introduced.
  - Independent architectural/placement review: the current constructed-surface
    field correctly excludes vertical walls. A future qualified disposable wall
    can reuse native StaticMesh/persistence rather than a new volume framework;
    canonical Ground restoration must precede it. Placement remains UNVERIFIED:
    neither nearby bulkhead context nor endpoint proximity locates the hidden
    root wall. No wall, Terrain edit, Water change, native run or city L2 follows.
    D39 fountain/source/native and retained Hero evidence stays bounded and valid.
  - `final-verification.json`: 54 pre-existing files preserved, all 641 current
    canonical outputs and 14 saved fountain inputs unchanged, current producer
    request still valid, 3,123 protected paths unchanged, HEAD/index preserved.
    Documentation ASCII, local-link and diff checks pass. No staging or commits.
- D39 source/model evidence: `tmp/world/terrain_closure_d39/`. Metadata selected
  USGS 1/3-arc-second `n41w074_20260917` and `n41w075_20261028`; these are tile
  distribution versions, not survey epochs. Acquisition was 661,900,704 bytes in
  33.96 s. Exact URL/size/SHA and two-store retention are recorded under `source/`.
  No further raster acquisition or domain expansion occurred.
  - `qualification/{report.json,independent-support.json}`: all 202,710 core and
    1,806 halo stations have complete original four-pixel/mask support. Reversing
    source order changes no samples. Independent Z processing differs by at most
    0.000288 m; this does not establish geographic accuracy or observed bathymetry.
  - Final source admission:
    `tmp/world/ground_products/terrain_closure_d39_v2/admission.json`, SHA
    `ab355ec96c347d5586fa1c4dedee95d7e2d21053224dc9c88a25764d9eeac9cc`.
    Admission/read/no-op: 10.14/2.14/7.02 s; 19 files unchanged. Earlier receipt
    is superseded by metadata refusal strengthening. Source tests: 20 PASS;
    four existing Ground consumer modules: 30 PASS.
  - `old-water-control/report.json`: all 319 original Water heights/counts replay
    exactly from original `water_fit` in 53.61 s, including 906 flow stations.
    Coherent diagnostic changes 318 functions, preserving geometry/IDs/membership
    and 225,533 non-Water records. Fountain interpolation is 18.9975 m and inferred
    Water 19.0 m. `fountain-provider-probe.json` corrects an obsolete diagnostic
    ID lookup; the older comparison report's null label is not missing geometry.
  - `conditioning-comparison/report.json`: one old/new census took 544.20 s.
    Additional support nodes: 5,116 old / 5,062 new; max cuts: 29.5 / 16.4 m.
    Reported distances use closest same-level candidates, not necessarily the
    actual support owner. Strict refusal occurs at identical old/new witnesses.
    One inferred scratch candidate freezes 16.4 m cut / 42.43 m support limits
    (ceil of the 30 m quad diagonal). No production policy is accepted or limits
    raised on failure. This policy remains unaccepted; L2 is gated by model quality.
  - Non-mocked canonical v1 processing completed in 359.04 s with 641 outputs,
    without production promotion. Independent review found an unqualified generic
    filter-scale claim; its focused RED is preserved in
    `ground-scale-provenance-red.log`. The claim is removed and seven focused
    constructor/publisher tests pass. Current v2 completes in 352.64 s with 641
    outputs, result SHA
    `20cbb8d9fb1d993cf07876f52625c9b2fb9efa8067029919ffee1fab5ee33f66`.
    `canonical-candidate-verification-v2.json`: 210 cells, 391 shared edges with
    zero mismatches, 225,533 unchanged non-Water records, 318 refitted Water
    functions and 147 byte-unchanged feature shards. Real no-op: 75.63 s / all 641
    outputs unchanged. Public-source substitution controls refuse.
  - `authority-candidate-v2-result.json`: fresh exact replay and isolated frozen
    authority PASS in 542.31 s. Production pointer remains unchanged. This is
    processing evidence, not city or geographic admission.
  - `native-ground-consumer-result.json`: focused native known/unknown accuracy
    and 19 resealed invalid-input controls PASS. Real fountain `native-v3` PASS
    in 86.9 s: three variants retain 1,922 faces through fresh reload, zero
    mismatches, maximum field error 3.56e-15 m; 1,922 rays/capsules and 19 extra
    support/disabled/height-fault controls. All 501 input/DLL pins unchanged.
    `fountain-native/runtime-view-v1`: normal-profile PIE observer PASS in 72.5 s,
    three settled views and unchanged saved inputs. Images inspected; the
    selected region lacks object geometry and does not establish city acceptance.
  - `changed-geometry.json`: complete 403,620-face conditioning comparison in
    6.47 s; 22,810 changed faces, 7,468 steepened and no newly >100% faces.
    Maximum changed gradient 0.7614 -> 0.9852. All 38,180 incident/external pairs
    examined: largest final crease 51.41 degrees; largest increase
    3.94 -> 38.87 degrees. These are geometric diagnostics, not player thresholds.
  - `saved-pier-contact-comparison.json`: actual saved R3's 80 vertices, all four
    dry terminal vertices measured in 0.26 s; current final Ground is -1 m at
    each, while source PWA is 0.5905/0.7158/2.1072/2.0962 m. Earlier
    `pier-old-cap-*.json` use closely related fixture points; this actual R3
    comparison supersedes them for consumer invalidation.
    `precision-shore/report.json`: one 1.79 s, 15-quad/56-face exact-clipping
    diagnostic leaves contacts at -0.769..-0.990 m and six outer-node witnesses
    with up to 3 m mismatch. No authority was published or guard relaxed.
- D39 gameplay: `tmp/world/terrain_closure_d39/gameplay/`. Actual configured Hero,
  ordinary CharacterMovement, owned live collision/Water and unsupported-position
  falling controls pass. Full Pier 648.914660 m both ways: 15,645 walking frames /
  454 s; inland-cap crossing: 301 frames / 181 s. Supported Kazan 73.114 m bank
  both ways: 1,767 frames / 269.5 s. All 21 Pier / 14 Kazan saved inputs unchanged.
  Neighboring submerged footway is unaccepted route context, not a demonstrated
  Ground defect. Native capsule/junction checker regression and four permanent
  offline route tests pass. Nav, streaming and changed-baseline/city L2 remain open.
  - Independent review found the first envelope mixed buried Ground slopes into
    deck tolerances. `gameplay/envelope_review/`: focused RED 26.9 s / GREEN
    28.4 s; actual floor impact now matches admitted top support and only the
    owning layer determines the capsule envelope. A flat deck above 30 m/m
    buried Ground retains a 2.41 cm allowance and refuses a 10 cm gap; captured
    Kazan junction control remains valid. Fresh actual-Hero Pier entry/inland
    checks PASS in 114.8 s (554 frames, 206 Ground / 348 deck); Kazan 8 m PASS in
    86.8 s (205 Ground frames). All 21/14 saved files unchanged. The full earlier
    traversals and raw measurements are historical evidence, not a full-route
    rerun of the added impact-owner guard. Independent bounded review PASS.

- D38 cached-only feasibility: `tmp/world/terrain_closure_d38/feasibility/`
  contains `assess_ground.py` and `report.json`. The final census took 22.46 s,
  including 10.32 s authenticating five raw products, five headers and all 638
  canonical outputs. The unaccepted instrument run took 21.09 s; both census
  executions total 43.54 s. Original domain bounds in EPSG:32618 metres are
  `[578653.41, 4504812.45, 592603.41, 4517832.45]`, with a 466 x 435 node lattice.
  Only 102,965 of 201,810 quads have four finite coarse corners; finite corners
  alone still do not establish complete original-pixel support. Source objects,
  hashes, processing recipe and frame identity remain in the retained inventory
  under the earlier source evidence below. The accepted candidate did not change.
  - Exact source NoData witnesses: mapped building probes `way/1016498833` at
    `(589621.160, 4515586.785)` and `way/1016498832` at
    `(589707.665, 4515536.205)`, both in `x5:y5`, each decode four original
    NoData pixels in the cached source mosaic. They are support witnesses, not
    proof of a failed playable route or current physical ground beneath buildings.
  - Other near-shore probes have finite original pixels but insufficient coarse
    neighbors. Do not label every coarse missing query an absent raw point or
    every unsupported Road a dry-ground defect; the initial nearest Road probes
    included Lincoln Tunnel. NoData, outside-domain and Float32 sentinel controls
    are explicit. `report-instrument-red.json` retains an unaccepted diagnostic
    whose direct comparison to the abbreviated header sentinel was corrected by
    Float32 canonicalization before accepting original-pixel missing counts.
  - Source/frame processing and the fountain's approximately 20.215 m reference
    are not a new geographic or Water-level admission. No source acquisition,
    production-code change, geographic model generation or city L2 occurred.
    Hero integration remains open; the requested unsupported-model stop applies.

- History recheck traced the 36 task-changing commits since 2026-10-08 and the
  retained 4,706-line task at `abb597aa8`. It distinguishes real implementation
  progress from unresolved product acceptance: `c8c3dd345` changed only the task
  despite its broad "failures resolved" title; `661fe4821` explicitly changed
  priorities to Terrain-first; authored correction, Water dependency and runtime
  fixes in `dba6a4645`, `64205f529` and `8a51eca39` addressed distinct boundaries.
  Historical task lines 2061-2075 reject one old outer-quad/0.019 continuation;
  2402-2429 distinguish crop-order and centroid-law failures from a legitimate
  east-trace input correction; 3214-3220 explain why arbitrary collars or city
  runs cannot repair a missing provider/exterior model contract. These are
  scoped results, not proof that every Ground model fails. The compact handoff
  above preserves what may reopen a case and its first cheap check; old B-model
  and detailed-object obligations are not silently reactivated after D31/D32.

- Approved knowledge-ownership corrections moved the five conditional building
  steps to their paused owner backlog, clarified the legacy authored-Landscape
  task and indexed the verified replay/collision lessons in the existing pitfalls.
  Operator decisions, failed comparisons and release obligations remain here;
  no gameplay, geographic or release acceptance follows from this migration.

- Reviewer `6d317dee` is accepted on stopping research, preserving local proofs
  and keeping the main baseline/release concerns open. Its proposed D38 label
  does not create a new operator mandate or authorize city L2. The claim that
  existing gameplay routes directly finish these regional scenes is corrected:
  reusable helpers exist, but a saved-candidate route/support integration is
  missing. No measured completion-time claim is justified. No production-code
  defect was demonstrated by this review. Detailed coastal art or higher accuracy
  may be optional only beyond the required credible/playable baseline; the
  fountain defect is not optional polish. Existing Kazan walkthrough backlog
  remains the owner of its later full-candidate walkthrough; no duplicate task
  is created. Skill -> territory generation -> architecture -> Mesh Terrain -> regional Tests
  reaches the implemented proofs and their gameplay limits without skill changes.

- D37 keeps all three geographic cases and both-city L2 OPEN. Evidence root:
  `tmp/world/terrain_baseline_d37/`. Current inputs reproduce the RC0 active
  hashes and 638 Manhattan / 641 Kazan outputs. Protected geometry/authority,
  HEAD/index remain unchanged. The current milestone started from the clean
  baseline recorded in `milestone_v1/baseline.json`; its final protected-state
  verification in `milestone_v1/final-verification.json` confirms all 3,123
  protected files and the index remain unchanged.
  Implemented observation commands and limits belong to the
  [regional test owner](../../../Plugins/World/ProjectWorldMeshTerrain/Tests/README.md).

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
  [compiler owner](../../../tools/World/CanonicalCompilation/README.md).
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
  by [Assembly pitfalls](../../../Plugins/World/ProjectBuildingAssembly/docs/pitfalls.md).
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
