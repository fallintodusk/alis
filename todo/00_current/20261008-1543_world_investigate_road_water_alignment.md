# Road and Water Alignment

**Status:** CURRENT - geographic reconciliation remains open; remaining implementation
plan REVIEW REQUIRED. Documentation cleanup only under D20.
**Goal:** coherent, playable geographic relationships in Kazan and Manhattan,
with dependency-local changes and truthful canonical-to-runtime evidence.

## Contents

- [Documentation owners](#documentation-owners)
- [Current state and next steps](#current-state-and-next-steps)
- [Remaining steps](#remaining-steps)
- [Proof traceability](#proof-traceability)
- [Authority register](#authority-register)

## Documentation owners

| Need | SOT or owned view |
|---|---|
| Refinement boundary and supported capabilities | [Refinement structure](../../tools/World/CanonicalCompilation/refinement/docs/architecture/structure.md) and [main view](../../tools/World/CanonicalCompilation/refinement/docs/architecture/diagrams/main.md) |
| Model semantics, admission and selection | [Refinement contract](../../tools/World/CanonicalCompilation/terrain_refinement.md) |
| Qualified supplemental Ground and source preservation | [Ground product contract](../../tools/World/SourceIngestion/supplemental/ground_products.md), [source view](../../tools/World/SourceIngestion/supplemental/docs/architecture/diagrams/main.md) and [retention](../../tools/World/SourceIngestion/retention/README.md) |
| Final-surface consumers | [Territory contract](../../Plugins/World/ProjectWorld/docs/territory_contract.md) and [World view](../../Plugins/World/ProjectWorld/docs/architecture/diagrams/main.md) |
| Input and target acceptance | [Immutable/refinement acceptance](../../tools/World/EndToEndValidation/README.md#immutable-artifact-consumer-acceptance) and [frozen evidence](../../tools/World/EndToEndValidation/frozen_evidence.md) |
| Selected Apply and recovery | [World host](../../scripts/ue/world/README.md) |
| Development and gate scope | [World pipeline layers](../../docs/testing/world_pipeline_layers.md) and [checkpoint policy](../../docs/agents/canonical.md#checkpoint-scope-and-gate-escalation) |
| Product-route visuals and runtime | [Visual verification](../../tools/World/VisualVerification/README.md) and [World runtime operation](../../scripts/ue/world/README.md) |

## Current state and next steps

Do not repeat accepted tests with unchanged inputs. The bounded evidence below
records task state; implementation semantics remain with the owners above.
Execution history is retained in Git. No authority, city output or source pin
changes as part of this cleanup.

| Accepted boundary | Retained evidence and reuse limit |
|---|---|
| Immutable canonical/source recovery separation (D56) | Current acceptance/retention SOTs above; this does not certify a changed base compiler or unavailable historical raw-source replay |
| Two-city Water revision-5 rollout (D58) | `tmp/world/completion_d58/architect/water5-postrollout.json` records the original canonical ZIP identities and rollout receipts: one changed Water mesh/actor pair per city, all 145 Kazan/115 Manhattan nonempty Water outputs replayed, cold no-op. Dependent Vegetation wrote nothing; unrelated layers/materials/base data stayed unchanged. Not D59's later selected child, Q5 geography or final packaged acceptance |
| Source-qualified Kazan bridge/drain rollout (D59) | `tmp/world/geographic_reconciliation_d59/rollout-selected-v1/`, `architect/completed-selected-rollout-and-reader-review.json`; transaction `1d33c88f8ffe47669800348becdcb001`, three Road/Water pairs, cold no-op; no Terrain/Manhattan adoption |
| D60 fresh-selection origin and Ground construction/query fixes | `tmp/world/geographic_reconciliation_d60/architect/feature-binding-fb1-recheck.json`; source/typed proof does not select regional geography |
| D60 Ground Road/Building source consumption and bounded scene | Same run's `architect/ground-civil-native-consumer-final-review.json`, `source/park-comparison/complete_road_basin_scene.json` and `editor/complete-scene-preview/`; exact `Road.GroundFinalSurface` / `Building.GroundFinalAnchor` source output and sampled transient collision passed; no saved/compiled, capsule, navigation, streaming, packaged or regional-adoption proof |
| D60 observed-basin/CDT join | Same run's `architect/observed-basin-join-final-review.json` and `editor/observed-basin-preview/`; bounded join/source/native-ray proof only, not accepted geographic or visual quality |
| D60 pure constructed Road and typed endpoint support | Same run's `architect/observed-constructed-surface-implementation-review.json`, `architect/constructed-surface-native-review.json`, `tests/native-constructed-real/` and `tests/protected-constructed-final.log`; not production parser admission, saved collision or playable joins |
| Accepted presentation, winding and earlier Road/Water rollouts | Preserve adopted materials and unrelated geometry. Prior rollout counts do not define the new affected closure; final changed-candidate artifact proof remains separate |

Retain these discriminators rather than restarting investigation:

- **Pier25:** relation `20784500`, four qualified members, `floating=no`,
  `start_date=2010`, no layer tag. Pier26 relation `20784501` has zero overlap
  with the four complete Road widths; its 2020 opening is not Pier25 evidence.
- One ribbon (`1171058928`) has pure/native source proof, about 0.5899 maximum
  centerline grade and unresolved player quality. Neighboring inferred widths
  `1236753481`/`1236753479` leave the parent by about 0.002992/0.008548 m2;
  bridge `1171058927` lacks qualified metric deck support. Incident joins remain open.
- The retained COPC `20170504_980200.copc.laz` is 77,948,603 bytes, SHA-256
  `927b6af830dbbebdd9ebf8fc1cde5c4e37858cc0a1feaa9978c440c1c01c3809`.
  Local coverage does not support the entire approximately 12,592 m2 parent.
  Full source/processing evidence is under the D60 run's
  `source/manhattan-steep-road-context/`; reuse the corrected
  `pier-parent-20784500/qualification-r2.json`.
- **Regional Ground/fountain:** narrow collars and fixed park blends fail
  surrounding Road quality; the broader 81-Road screen exposes grades about
  0.747-1.090. The 47.8 km2 comparison is unselected. Source gaps/conflicting
  observations and the multi-level entrance remain explicit unknowns.
  Reuse the D60 run's `source/manhattan-land-region/fountain-land-component-report.json`
  and corrected `gap-attribution-v2.json`; the unsuffixed attribution used the wrong
  domain. The qualified land selector supplies geography, not source elevation.
- **Kazan:** the approximately 112,182.579 m2 dry-bank modification and unsupported
  sheer-bank candidate are rejected. Preserve accepted terrain pending a defensible
  source-shaped policy. Historical 45/six/seven/21 contact counts cover different
  domains/states and must not be summed or treated as a current global census.
- **Identity gap:** fixture serialization omits constructed endpoint relation and
  supporting-model identity; Road consumption already includes them. Production
  parsing still rejects that model. If selected, RC4 corrects the distinction before
  authoritative verification/admission; it is not a stale production-output finding.

## Remaining steps

All RC steps are unchecked future work. Q2/Q3/Q5 remain their specific gates;
this documentation request does not approve model quality or production changes.
RC1-RC3 and independent release preparation may progress separately once authorized;
serialize Editor mutations and transactions. Use focused tests until slice exit.

### Delegated work and retained decisions

For a separately authorized implementation session, apply the
[existing authority register](#authority-register) once to the selected model:

- **Q2:** D17/D18 permit source investigation, relationship qualification and
  supported deterministic modeling. They do not silently approve a new road-design
  law. Q2 still gates grading prototypes/changes needing unaccepted crossfall,
  longitudinal, corridor or cut/fill policy. Prepare one measured decision if that
  policy is required; continue independent approved work instead of asking per feature.
- **Q3:** source-context admission/qualification is authorized by D17. Resolve
  supported relationships through evidence; unknown metric heights/connectivity
  remain unknown. Escalate only an irreducible ambiguity outside that authority.
- **Q5:** conservative source/quality limits supported by evidence and product
  constraints are delegated by D14 and release D25, with independent architect
  challenge. Unsupported geographic-risk acceptance or widened unsupported limits
  remain operator decisions. Unknown data never grants permission.

Critical path: RC0 -> required RC1/RC2/RC3 branches -> selected RC4 contract -> RC5
scene -> RC6/RC7 acceptance/transaction -> RC8 changed-property validation ->
RC9/RC10 concern handoff -> release-owned R5 -> operator approval -> R6.
This is a dependency guide. An unused optional model is not mandatory, but omitting
it cannot waive an affected release-critical feature or required quality proof.

### RC0 - Authenticate the actual candidate and remainder

- [ ] Record the true slice base/path set, current base/child selection, source and
  processing pins, accepted receipts and protected output/index inventory.
- [ ] Produce one current feature/cell/relationship/severity inventory, reconciling
  old Water counts and identifying unchanged reusable proof. Use the acceptance SOT.
- [ ] Select the release-critical branches and required model variants from that
  inventory; record why any optional prototype is unused without excluding a real defect.

### RC1 - Complete platform, bridge and land/path relationships

- [ ] Qualify the complete supported parent/domain, holes, incident widths, land
  attachments and underlying Water. Preserve topology, source epochs and gaps.
- [ ] Compare supported surface modeling with explicitly identified, bounded
  game-world grade inference from qualified context/endpoints. Resolve tiny inferred-
  width departures from evidence. No parent expansion, Water-as-Ground, fabricated
  surveyed height or feature-ID rules. Apply the delegated-work distinction above;
  resolve Q2 if the chosen candidate introduces its unaccepted grading policy.
- [ ] Measure every join's full-width grade, cross-slope, steps, clearance and
  continuity, including the bridge; select only a Q2/Q3/Q5-compliant connected model.

### RC2 - Select regional Ground and fountain quality separately

- [ ] Compare coherent regional composite versus broader qualified Ground, using
  complete source frontiers and affected civil consumers. Keep unknowns explicit;
  do not automatically adopt the 47.8 km2 candidate or publish a diagnostic crop.
- [ ] Use existing adaptive/shared-edge capabilities; measure source departure,
  seams, gradients, cut/fill, civil contacts and output cost against rejected controls.
- [ ] Resolve joint fountain footprint, level/spill, Ground and civil access with
  explicit observed/inferred provenance and an accepted Q5 quality table.

### RC3 - Complete Kazan bank/Water and residual crossing classes

- [ ] Trace each current failure through canonical, producer, saved, compiled and
  collision surfaces; separate numerical errors from conflicting source functions.
- [ ] Resolve supported contacts/coverage/holes and protected road/dam/quay/bank
  relationships through the existing refinement owner. No snapping unequal heights,
  invented connectivity or revived broad cut. Measure the complete affected domain.
- [ ] Record any remaining irreducible case with actual visual, collision/access and
  gameplay consequences. A critical unresolved failure keeps geographic acceptance open.

### RC4 - Review and authenticate the selected general contract

- [ ] Obtain independent architect review of rules, source/model provenance, epochs,
  joins, gates and dependency closure before new production admission.
- [ ] Verify existing child API/schema/publication and fresh-selection coverage for
  the selected model; extend only consumed gaps. Bind feature origin, complete
  evidence/support, policy, base and native identities. No Water-only Ground provenance.
- [ ] Only if the selected solution uses constructed endpoints, add proposed exact test
  `Project.World.Realization.Verify.ConstructedEndpointIdentity`: same geometry,
  changed valid endpoint/support identity must invalidate verification. Correct
  `ProjectWorldVerifyIdentity` in its owner; keep default identities and refusal
  state unchanged before authoritative verification/admission. Use existing tests;
  retain the guard as known unused work if this model is not selected.
- [ ] Extend parser/schema admission only for selected production variants, after
  focused RED/GREEN, stale/unknown-input refusal and direct consumer proof; review
  the completed boundary. Do not implement an unused diagnostic model for completeness.

### RC5 - Prove complete representative neighborhoods

- [ ] Use actual source-shaped final Terrain, Water, ordinary/constructed/elevated
  Roads, all incident joins and nearby Buildings/placements in disposable scenes.
- [ ] Prove saved/reloaded meshes, Nanite, fallback, collision, full coverage,
  shared-cell seams and float conversion with missing-face/wrong-join controls.
- [ ] Exercise real possessed-character walking/contact, steps, sliding/falling,
  shore access, navigation and streaming return; ray hits alone do not pass.
- [ ] Inspect matched player-eye/near/distant/grazing views and continuous motion
  with accepted materials. Cover the separate fountain/regional and Kazan scenes;
  one Pier25 scene cannot certify those classes. Prove cleanup and review integration.

### RC6 - Accept the exact affected closure and recovery

- [ ] Follow the existing planner from the true slice base. Explain each changed
  unit and consumer identity; preserve unrelated units and source/base lineage.
- [ ] Follow the current planner's selected regression gates and modes, plus the
  chosen publication operation's mandatory target/recovery acceptance. Reuse valid
  evidence; do not prescribe Check/Matrix by habit or bypass operation guards.
  Actual source/base-compiler changes retain their producing proof.
- [ ] Prove paired canonical/World failure recovery, exact restoration and cold
  replay through the existing transaction; no alternate publisher or bypassed gate.

### RC7 - Roll out only the qualified dependency closure

- [ ] Confirm durable-operation authority and quality dispositions; publish the
  accepted child and affected owners through the existing selected transaction.
- [ ] Compare actual writes to the accepted census. Preserve unrelated geometry,
  materials and inputs; reevaluate placements only for changed consumed surfaces.
- [ ] Cold-audit both cities and prove no-op without producer replay or a required
  temporary selection file. Stop unexpected propagation and retain recovery proof.

### RC8 - Accept changed-city gameplay and runtime

- [ ] First prove saved-city uncooked movement/contact, access, visuals and streaming
  on affected routes in both cities with unchanged-region controls.
- [ ] Specify changed-property packaged consumer/collision/resource-lifetime and
  frame/GPU/CPU, Water-pass, memory/streaming requirements for the release owner.
  R5 owns the single final frozen cook/package and certification; RC8 does not
  schedule another city package. Reuse prior exact-artifact proof only where valid.
- [ ] Complete focused uncooked/available-artifact validation and inspect actual
  pixels. Report bounded concern acceptance separately from release-owned artifact/
  human gates; busy-host INCONCLUSIVE or Editor proof is not packaged performance PASS.

### RC9 - Hand off exact release inputs

- [ ] Supply accepted geography/consumer evidence, canonical/resource identities,
  changed scope, residuals and required new artifact checks to the release owner.
  Final R5 execution stays in that owner's checklist and package SOT.
- [ ] Preserve historical release receipts. Do not stage/commit, freeze or run release
  commands during this documentation pass.

### RC10 - Close the concern accurately

- [ ] Resolve applicable Q2/Q3/Q5 dispositions, obtain final independent integration
  review, inspect the final diff and migrate verified standing facts to existing SOTs.
- [ ] Close only the concern properties actually proved; retain any release-owned
  artifact gate explicitly. Move this task to done when its acceptance closes and
  update the release router. A required failure cannot be hidden in a new backlog.

## Proof traceability

One table binds future evidence to the actual surface/envelope. Retain D60 only
for unchanged properties. Exact build/RHI/flags and input identities must accompany
each new receipt; required final artifact proof may be coordinated with release R5.

| Invariant | Acceptance surface | Execution envelope | Cheapest proof | Final proof | Stop condition |
|---|---|---|---|---|---|
| 1 - one canonical authority | Selected canonical child and final consumer query/geometry | Pinned Python/native execution, then Win64 Development Editor normal-RHI realization | Stale/competing-source refusal; retained D60 feature binding | RC4 source-bound replay and RC5/RC7 selected authority-to-saved-output agreement | Competing authority blocks admission |
| 2 - geometry independent of presentation | Actual Road/Water/Terrain surfaces versus presentation | Same authenticated source, saved native geometry and normal-RHI city scene | Same-XY height/offset chain with wrong-height control | RC5/RC8 geometric and rendered agreement with unchanged accepted materials | Material-height masking or unexplained mismatch blocks geography acceptance |
| 3 - qualified source meaning and coverage | Full qualified footprint, source roles/epochs and endpoint context | Pinned admitted source bytes and CanonicalCompilation refinement; no implicit live data | Existing D60 support/coverage tests plus missing-parent/source-gap controls | RC1-RC3 complete-domain qualification and RC4 authenticated admission | Unsupported coverage or fabricated provenance blocks model selection |
| 4 - protected state | Canonical/generated protected path/hash set and Git index | D19 read-only documentation pass; future reversible L0-L2 fixtures | Before/after inventory | RC0/RC5/RC6 exact protected inventory; RC7 only the approved write set changes | Unexpected mutation stops the operation |
| 5 - saved geometry and collision | Saved source meshes, Nanite/fallback, physics shapes and streamed seams | Win64 Development Editor normal RHI, save/reload and actual PIE; final packaged counterpart owned by R5 | Missing-face/wrong-join controls and continuous triangle comparisons | RC5 saved realization; RC8 supplies changed-property requirements for R5 packaged collision/streaming proof, retaining Water no-collision policy | Ray-only, vertex-only or wrong-envelope evidence blocks the named boundary |
| 6 - complete consumed identity and bounded closure | Native consumed identities, fixture identity, planner closure and output cost | Exact Python/UE fixtures; planner-selected and operation-required acceptance envelopes | RC4 changed-role/hash RED if constructed variant selected; support/neighbor mutation and unchanged-default controls | RC4 identity proof for selected variants plus RC6 census and RC7 exact actual writes/cold no-op | Stale acceptance identity or unexplained fanout blocks publication |
| 7 - deterministic publication and recovery | Paired canonical/World pointers and persisted owner outputs | Existing locked selected-refinement target with closed project Editors; cold reader after restart | Post-pointer and saved-child failure injection | RC6 exact restore and RC7 cold audit/no-op without temporary-selection or producer dependency | Incomplete recovery or refusal writes block rollout |
| 8 - identified derived model and fidelity | Identified modeled surface and all final-surface consumers | Qualified refinement model, typed production parser and normal-RHI saved realization | Retained D60 constructor/typed controls; new inference/provenance and parser negative controls | RC4 independently reviewed contract and RC5 source-to-realized fidelity with acyclic declared dependencies | Unidentified inference, stale support or hidden adapter correction blocks admission |
| 9 - whole-neighborhood quality and contact | Complete neighborhood, incident joins and possessed-character contact | RC5 disposable full scene; RC8 actual saved-city product route, real character/capsule/CharacterMovement | Known steep/bad-join/hole controls; full-width quality measurements | RC1-RC3 accepted model-quality table and RC5/RC8 walking, shore access, streaming return and player-eye motion in each distinct scene | Failed quality/contact or unresolved critical geography blocks geographic release acceptance |
| 10 - packaged runtime and performance | Exact packaged live resources, pixels, frame cost, collision and residency | Profile-authenticated Win64 Development Game and required Shipping route, normal RHI, both cities and qualified performance envelope | RC8 receipt freshness and changed-property validation/requirements | Release-owned R5 both-city gameplay/render/performance and changed lifetime proof; target/policy retained | Missing/wrong-envelope evidence, correctness failure or unaccepted budget result blocks final artifact readiness |
| 11 - frozen unsigned artifact identity | Installed unsigned artifacts, payload/chunks and machine acceptance receipts | Frozen full-SHA source route through unsigned release coordinator and real installed product | RC9 authenticates handoff inputs; release owner runs preflight and integrity controls | Release-owned R5 frozen-source machine/artifact result and final independent review; RC9 does not execute it | Mixed identities or failed release evidence block R5, not the already-proved bounded concern |
| 12 - human acceptance and publication authority | Operator acceptance and truthful release-router lifecycle | Actual operator walkthrough/decision and separately authorized R6 environment | Read authority register and compare closure wording | RC10 explicit technical-versus-human handoff; release closure only under its retained completion contract | Missing human authority leaves the named gate open; never auto-publish |

**Documentation review evidence:** the earlier cleanup PASS was conversation-only;
no standalone receipt was retained. The independent planning recheck is PASS for
this pre-hygiene snapshot SHA-256:
`c361418c9e8d924d479467199b9f10973c28f3cab3343b623530a81a3db31211`.
Local disposable evidence path:
`tmp/world/documentation_cleanup_3_0_0/reviewer_patch/architect-review.json`.
The receipt may be removed; permanent availability is not claimed. This reference-
only hygiene grants no implementation, runtime or release acceptance.

<details>
<summary>Retained operator decisions and gates</summary>

## Authority register

### Operator decisions

- **D1** Keep geometric alignment and layering separate from material presentation.
  - Reason: its implementation design is not decided yet.
  - Source: operator request, 2026-10-08.
- **D2 [SUPERSEDED IN PART by D5 - read-only investigation timing]** Create a current investigation todo now; agree on the material todo first,
  then conduct this dedicated investigation separately.
  - Reason: not stated.
  - Source: operator request, 2026-10-08.
- **D3 [SUPERSEDED IN PART by D5 - bounded read-only editor evidence]** This step is planning/documentation only; do not implement or run expensive
  verification. Development later uses owner-local tests, scratch previews, and
  focused integration.
  - Reason: avoid repeatedly rebuilding/cooking/regenerating the World for debugging.
  - Source: operator request, 2026-10-08.
- **D4** Audit the attached alignment review against actual code, architecture,
  project goals, and previous agreements; fix these todos and explicitly refute
  incorrect reviewer points.
  - Effect: revise this existing investigation, its presentation handoff, and release
    routing. This source audit does not approve an alignment implementation or
    supersede D2/D3's dedicated-investigation sequence and documentation boundary.
  - Reason: not stated.
  - Source: operator follow-up, 2026-10-08.
- **D5 [SUPERSEDED IN PART by D8 - Q1 implementation]** Update the todo and run the read-only editor investigation now.
  - Supersedes: D2's requirement to wait for material agreement before this bounded
    read-only investigation, and D3's documentation-only limit for this step.
  - Effect: source/candidate comparison and bounded editor geometry/visual evidence
    are authorized now. No implementation, saved asset changes, generation, broad
    gates, or release work is authorized; Q1 remains open.
  - Reason: not stated.
  - Source: operator answer to the scope question, 2026-10-08.
- **D6 [SUPERSEDED IN PART by D8 - Q1 implementation]** Continue investigation and prepare the existing todo for comprehensive
  realization, evaluating the supplied investigation-stage review.
  - Effect: additional owner-local read-only diagnosis and this implementation
    proposal; D5's no-implementation/no-generation boundary remains binding.
    Analytical controls of existing behavior are permitted; candidate geometry
    prototypes and changed algorithms remain behind Q1.
  - Reason: not stated.
  - Source: operator follow-up, 2026-10-08.
- **D7** Critically evaluate the attached simplification review against code,
  architecture, goals and prior agreements; apply valid in-scope corrections and
  explicitly refute incorrect points.
  - Effect: update this existing planning artifact. No Q1 implementation choice or
    asset mutation is approved by the review's proposed Q1-Q3 answers.
  - Reason: not stated.
  - Source: operator follow-up, 2026-10-08.
- **D8** Implement the accepted todo through implement-approved-change for the
  bounded Q1 correctness slice: small RED/GREEN fixtures and owner-local road
  conformity, consumed-input identity and water coverage.
  - Supersedes: D5/D6's no-implementation boundary for this bounded slice only.
  - Effect: Q2 grading and Q3 ambiguous crossing reconstruction remain deferred;
    persistent terrain mutation requires accepted water distortion/source-quality
    limits. No production geography promotion, whole-World rebuild or release work.
  - Reason: not stated.
  - Source: operator skill invocation and accepted alignment todo, 2026-10-08.
- **D9** Inspect reported widespread z-fighting through live Editor MCP and apply
  valid in-scope code/documentation fixes after reviewing D39 feedback.
  - Effect: bounded duplicate/intersection diagnosis and Water-owned constant-plane
    composition with focused fixtures. Preserve accepted materials, canonical inputs
    and city output authority. No new grading, crossing law, terrain-conditioning
    limits, persistent geometry rollout or broad pipeline is inferred.
  - Source: operator follow-up, 2026-10-13.

- **D10** Investigate terrain flicker with short PIE camera movement and fix the
  demonstrated owner-local defect.
  - Effect: reversible near/far rendering controls, Mesh Terrain authoring/runtime
    lifecycle correction and focused native proof. Preserve accepted material
    recipes, all canonical and city output bytes, and the separate Water rollout,
    nonconstant crossing and terrain-conditioning gates.
  - Source: operator follow-up with D40 reviewer attachment, 2026-10-13.

- **D11** Execute the remaining work in one coordinated pass toward moving the
  completed concerns to done.
  - Effect: investigate the retained stub, implement independently justified
    owner-local correctness, prepare measured shoreline alternatives and verify
    the actual consumers. No missing numeric conditioning/source-quality or
    ambiguous-crossing choice is inferred; do not close unresolved criteria by
    changing their meaning. Preserve accepted art and unrelated generated owners.
  - Source: operator "do all remaining in one pass to move todo to done",
    2026-10-15. Supersedes the preceding stub-only timing for this work.

- **D12** Investigate constrained canonical terrain refinement; preserve
  low-confidence cases until their policy is resolved.
  - Effect: investigate the canonical representation, native implementation
    capabilities, bounded influence and exact consumer/neighbor closure. This
    selects the investigation direction, not numeric distortion limits, source-
    quality promotion or persistent terrain mutation. D8's quality boundary stays.
  - Reason: not stated.
  - Source: operator's explicit answer to the shoreline-direction question,
    2026-10-15.

- **D13** Approve Q4's disposable implementation of the sparse canonical terrain
  representation and affected query contract in one-quad/two-cell fixtures.
  - Effect: implement and verify the reviewed D12 boundary. Existing city assets,
    production canonical authority and low-confidence cases stay unchanged. Q5's
    production quality decision remains open.
  - Reason: not stated.
  - Source: operator's explicit answer "Approve disposable implementation",
    2026-10-15.

- **D14 [SUPERSEDED IN PART by D19 - this pass's implementation timing]** Complete this concern under the release router's D25 FULL GO.
  - Effect: investigate the real source-shaped Q5 candidates, implement necessary
    canonical/Water/consumer corrections, and perform gated dependency-local
    production transactions and packaged acceptance autonomously. Q4 stays closed.
    The release router owns the technical-quality delegation, independent review,
    preserved operator/performance boundaries and final unsigned certification.
    Unresolved source/crossing risks require evidence and explicit disposition;
    neither an exclusion nor a reviewer PASS manufactures release acceptance.
  - Source: operator's FULL GO attachment and autonomous completion request,
    2026-10-16; see [release authority](00_release_3.0.0.md#authority-register).

Release inclusion, the historical R5, final frozen R5/R6, and unchanged performance
policy are owned by release-plan D19-D22, not this investigation.

- **D15** Implement the release router's D26 dependency and recovery correction.
  - Effect: authenticate immutable accepted canonical consumption separately from
    fresh source/base compilation; retain Q4's child identity and correctness
    proof, strict upstream replay and unchanged geographic selection. Preserve
    accepted city outputs, source pins and Q5 limits. Complete the bounded exact
    raw search and durable retention/recovery contract before replacement pins.
    A scoped Water release-exception proposal remains evidence, not acceptance;
    the operator retains the final risk decision.
  - Source: operator's source-recovery reinvestigation and architecture correction,
    2026-10-16; [release authority](00_release_3.0.0.md#authority-register).

- **D16 [SUPERSEDED IN PART by D19 - this pass's implementation timing]** Resume full completion under release-plan D27. Reuse D56's accepted
  dependency/source boundary and its receipts. Perform the gated revision-5
  Water transaction, actual city consequence checks and independent packaged
  verification. Preserve low-confidence cases, the rejected bank decision and
  the operator's final geographic-risk authority. Do not stop at another
  intermediate PASS or create a duplicate concern.
  - Source: operator's resume-and-complete request, 2026-10-16;
    [release authority](00_release_3.0.0.md#authority-register).

- **D17 [SUPERSEDED IN PART by D19 - this pass's implementation timing]** Keep the geographic release hold temporarily and continue the original
  FULL GO through source-aware geographic surface reconciliation. Do not accept
  a blanket exception or stop engineering at the hold.
  - Effect: audit the four measured defect classes and their surviving source
    semantics; inspect native UE and established terrain methods; prepare this
    existing todo through investigate-change; obtain independent architect review
    at the canonical/consumer boundary before broad implementation. Implement
    justified general rules, focused fixtures and real-city verification, then
    deploy only affected outputs through the existing acceptance/transaction route.
    Continue independent packaging, collision and performance work. D58's frozen
    proof remains bounded to its recorded candidate until consumed inputs change.
  - Constraints: preserve original sources, provenance, accepted geography,
    materials and Git state during investigation/candidates. Prefer explicit
    evidence, supported topology/geometry, then defensible bounded and identified
    model inference. Never invent elevations/connectivity or unsafe cut/fill
    limits; do not silently reinterpret missing historical tags. No parallel
    terrain authority, generic solver framework without demonstrated need,
    repeated full-city development regeneration or premature publication.
  - Supersedes: D16 review record's proposed hold-versus-waiver next step. Q5
    remains open; escalate only a specific ambiguity demonstrated irreducible
    from available evidence and accepted engineering constraints.
  - Reason: procedural generation should resolve defensible geographic
    relationships automatically rather than request a decision for every crossing.
  - Source: operator's Q5 answer and stages A-E, 2026-10-16.

- **D18 [SUPERSEDED IN PART by D19 - this pass's implementation timing]** Continue FULL GO by evaluating and applying valid source-model review
  findings, preserving the accepted bounded Ground proofs.
  - Effect: compare an evidence-backed regional Ground/retained-terrain transition
    with broader qualified Ground adoption and any explicitly permitted unsupported
    disposition. Identified deterministic interpolation is a game-world model,
    not surveyed elevation. The historical DSM is immutable input, not a blanket
    requirement to freeze erroneous civil grades or force a tiny matching seam.
    Obtain independent architect review before selecting a regional policy/contract;
    implement and measure through the existing canonical refinement and consumers.
    Audit original-feature binding at its actual acceptance owner and keep generated
    test evidence outside public source. Continue actual geography, transaction and
    technical release verification; no intermediate PASS closes the whole concern.
  - Constraints: D14/D17 quality, source, dependency-local and operator boundaries
    remain. No automatic 47.8 km2 replacement, fabricated waterline, unknown-source
    fill, arbitrary cut/grade relaxation, parallel authority or public R6 action.
  - Source: operator's "do bro fully" request to evaluate/apply the attached review,
    2026-10-17. Technical findings still require repository and measured evidence.

- **D19 [SUPERSEDED IN PART by D20 - documentation destinations]** Add the complete decomposed remaining geographic/release-closure work to
  this existing todo using investigate-change and architect; do not implement now.
  - Effect: this pass edits only this todo and performs read-only repository/plan
    inspection. The new release-closure checklist remains REVIEW REQUIRED until
    operator approval; reviewer execution wording grants no authority.
  - Supersedes: D14/D16/D17/D18 implementation timing for this planning pass only;
    their accepted work, long-term goal, quality constraints and release gates remain.
  - Reason: prepare complete steps for faster future closure.
  - Source: operator's "add only in todo now" request, 2026-10-18.

- **D20** Clean the active 3.0.0 todos to actionable steps linked to hierarchical
  SOT documentation and diagrams, and obtain independent architect review.
  - Effect: documentation maintenance only; update existing documentation owners
    and routes where needed. Preserve accepted work, decision IDs, source pins,
    operator gates and Git state. No implementation, generation or release run.
  - Supersedes: D19's single-todo destination restriction for this documentation
    cleanup only; its no-implementation boundary remains.
  - Source: operator's active-todo and hierarchical-documentation cleanup request,
    2026-10-18. Reason: remove redundancy and put knowledge with its proper owner.

### Operator gates

- **Q1 [CLOSED by D8 - bounded correctness only]:** What implementation should be approved after the dedicated
  investigation identifies the defect and owning boundary? Gates implementation only.
  No alignment design is endorsed by creating this file.
- **Q2 [OPEN - BLOCKING for road grading]:** What derived road crossfall/longitudinal
  grade, maximum cut/fill, corridor/transition extent, and canonical surface contract
  should be accepted? Existing source heights/widths do not supply a road-design law.
  Gates grading prototypes and production changes, not the read-only diagnosis.
- **Q3 [ADMISSION INVESTIGATION AUTHORIZED by D17; ambiguous metric resolution OPEN]:** Should available source crossing
  metadata be admitted through the existing canonical/typed contracts, or should
  ambiguous crossings be explicitly excluded/refused? Do not invent ordinary
  junctions, bridge clearance, or tunnel elevations from XY overlap alone.
- **Q4 [CLOSED by D13 - disposable implementation only]:** Approve the versioned sparse
  final-surface representation, bounded query contract and disposable canonical/
  adapter experiment proposed below? D12 selects investigation, not this contract
  change. Existing Q1 correctness approval does not approve a representation migration.
- **Q5 [OPEN - TECHNICAL SELECTION DELEGATED by D14/D25]:** Which source-accuracy
  classes and numeric maximum cut/fill, changed dry-bank extent, bank gradient,
  continuity/clearance and topology-cost limits are acceptable for each admitted
  use? This records D8's retained production-quality boundary; Q4 fixtures cannot
  supply production acceptance. D12 excludes low-confidence cases until resolved.
  Supported conservative limits may now be selected and challenged independently
  under D14/D25; an unsupported residual quality/risk choice remains a human gate.

Q2/Q3 do not block Q1-approved independent road-consumption/identity and water-
coverage fixtures outside ambiguous crossings. Water terrain mutation has its own
source-quality and distortion acceptance before persistent promotion; it must not
borrow a road grade law. The following are proposals, not closed gates:

| Gate | Proposed resolution for operator review |
|---|---|
| Q1 | Accepted by D8 for staged owner-local correctness fixtures: road full-surface consumption and bounded tangent/neighbor identity, plus independent canonical water coverage. Representative integration and policy acceptance remain required before persistent production output or a representation migration. |
| Q2 | Keep grading optional and separate. A named paved-road visual fixture may compare about 2% normal crossfall only after explicit prototype/input approval. Accept road-class/source-quality-specific longitudinal, transition and cut/fill limits before persistent grading; report infeasible segments. |
| Q3 | Admit available bridge/tunnel/layer/ford distinctions through existing source/canonical/typed seams. Preserve authenticated node connectivity only where present; otherwise keep connectivity unknown. Exclude/report ambiguous features from automatic merging/grading while unrelated correctness proceeds. |

The roughly 2% reference comes from [FHWA's tangent-pavement cross-slope guidance](https://highways.dot.gov/field-offices/missouri/interchange-design-promptlist).
It is not an ALIS surveyed fact, a universal road-class rule, or proof that an admitted
road is paved. [OSM's layer guidance](https://wiki.openstreetmap.org/wiki/Key:layer)
describes crossing order, not metric elevation; coincident XY is not shared-node identity.

### Working assumptions

- **A1 [ACTIVE]:** The reported concerns can be diagnosed using existing maps and a
  small representative fixture before any production geography is regenerated.
- **A2 [ACTIVE]:** Surface color/reflection may make an intersection look wrong;
  final attribution must compare geometry/depth with lit presentation.
- **A3 [SUPERSEDED IN PART by D8]:** Conformity is approved for Q1 owner-local
  fixtures. Bounded feature-driven shaping remains conditional on Q2 and is not
  an automatic second phase.
- **A4 [ACTIVE - proposal, not authority]:** A single versioned final-surface
  artifact per refined cell, using the pinned GEOS native kernel and complete-quad
  query domains, is the smallest viable D12 experiment. It remains conditional on
  Q4 and the refusal/locality proofs below; numeric production policy stays Q5.

</details>
