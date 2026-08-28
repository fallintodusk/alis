# Stabilize the combined Kazan release candidate

**Status:** IMPLEMENTATION IN PROGRESS - R1 PASS
**Active flag:** F6 - Kazan same-character PreviewFlight scale proof
**Baseline:** operator-selected integration commit `46a9459bd1`
**Scope:** evidence isolation and bounded correctness closure after the combined
Track P, Track V, ProjectMaterial, Landscape, generic World, survival, and
workstation setup integration
**Stable documentation owners:**
`Plugins/Gameplay/ProjectSinglePlay/docs/scenario_orchestration.md` and
`Plugins/Editor/ProjectCinematic/README.md`

## Goal

Keep the useful combined implementation without pretending that one green package
independently accepts every owner. Treat the selected commit as a recoverable `C0`
integration baseline, then re-accredit one owner/contract at a time through small
follow-up diffs and focused receipts.

The remaining player-facing result is governed by D1, D3, and D5: Old City 17 owns
the introduction to mental/survival systems; Kazan reuses the real character and
those systems but starts with the existing `PreviewFlight` traversal enabled so the
player can inspect the reconstructed territory manually. The survival cache scenario
remains explicit technical evidence and is not the normal Kazan objective or tutorial.

This todo does not authorize history rewriting, destructive reset, another framework,
or reimplementation of accepted features. D4 requires R1 review of this todo diff
before production implementation resumes.

## Authority register

### Operator decisions

- **D1** The player-facing legacy map name remains `Old City 17`, and Old City 17
  remains the first-map introduction for the mental and other survival systems.
  - Effect: Kazan must not duplicate that onboarding role.
  - Reason: preserve the existing product progression and map identity.
  - Date/source: operator clarification, 2026-08-28.
- **D2 [SUPERSEDED by D5]** Kazan reuses the same real character and ALIS systems, but its
  player-facing emphasis is an automated flight that reveals the volume of the
  reconstructed world.
  - Effect: the normal Kazan product route is a same-character scale showcase,
    not a grounded survival tutorial.
  - Reason: make the generated territory itself the Kazan proof.
  - Date/source: operator clarification and confirmation, 2026-08-28.
- **D3** The cache/pouch/water/ration/shelter flow remains useful as automated
  technical proof but is not player-facing Kazan onboarding.
  - Effect: normal Kazan must not select or present `UrbanSurvivalProofV1`;
    explicit validation may still select it through the real product route.
  - Reason: avoid duplicating Old City 17 and keep the Kazan story clear.
  - Date/source: operator confirmation of the corrected product split, 2026-08-28.
- **D4** Review this todo diff before implementation, then apply valid reviewer
  corrections and implement/re-verify the remaining intent.
  - Effect: this revision is R1 input only and authorizes no production change.
  - Reason: review the corrected boundary before changing the accepted Candidate.
  - Date/source: current operator request, 2026-08-28.
- **D5** Use the existing flight capability through two states, enabled or disabled,
  instead of building a scripted automatic tour merely to show Kazan's scale.
  - Supersedes: D2's scripted automatic-tour interpretation while retaining its
    same-character and world-volume intent.
  - Effect: F6 selects the existing `PreviewFlight` policy for normal Kazan and
    keeps automatic route/timing choreography out of scope.
  - Reason: follow the product goal without overengineering functionality the
    repository already provides through traversal selection.
  - Date/source: operator correction after R1 review, 2026-08-28.
- **D6** Proceed immediately with the R1-approved D5/F6 implementation and valid
  reviewer corrections; prepare and test F5 verification now, but execute the final
  Track V capture only after the operator accepts the exact Shipping Candidate.
  - Effect: closes the implementation gate, resolves A4-A5 for this release, and
    preserves one final operator walkthrough before F5 execution.
  - Feasibility correction: verified packaged-gate code subsequently rejected A4's
    grounded-survival premise. D6 still authorizes the valid implementation but does
    not override repository evidence.
  - Reason: operator requested autonomous implementation after reviewing R1 PASS.
  - Date/source: current operator request, 2026-08-28.

### Operator gates

- **Q1 [CLOSED by D6]:** Does a later release need a player-visible in-session
  fly/walk toggle? - default while open: A4; it does not gate the current Kazan
  scale proof.

### Working assumptions

- **A1 [REJECTED by D5]:** After the automated route, control stays with the same possessed
  character in manual `PreviewFlight`; v1 adds no runtime fly/walk toggle.
- **A2 [REJECTED by D5]:** The packaged tour retains the existing gameplay camera so the
  product proof stays truthful to the real character. Authored external-camera
  composition remains Track V's Editor-only responsibility.
- **A3 [REJECTED by D5]:** Concrete tour timing and route ownership are selected by R1 from
  current owners and evidence; neither the test gate nor ProjectCinematic may become
  packaged product architecture merely because it already moves a camera/character.
- **A4 [REJECTED by evidence]:** For this KISS release, enabled/disabled are load-time session
  selections through the existing `Traversal` option. Normal Kazan selects enabled;
  explicit grounded validation selects disabled. No runtime toggle or second menu
  selection is added unless Q1 is later closed by a new operator decision. The
  accepted survival packaged driver requires flight-specific Space/Ctrl travel,
  cruise-height, braking, descent-collision, and slide evidence.
- **A5 [RESOLVED by D6]:** Normal Kazan `PreviewFlight` retains the existing gameplay camera
  so the package proves the real character. Authored external-camera composition
  remains Track V's Editor-only responsibility.
- **A6 [ACTIVE]:** Normal Kazan explicitly selects `PreviewFlight` without a scenario.
  Technical survival explicitly selects both `UrbanSurvivalProofV1` and
  `PreviewFlight` because its accepted real-input driver is flight-based. Ordinary
  experiences still select disabled flight by omitting `Traversal`; no policy is
  inferred from another policy.

## Reviewer finding disposition

| Finding | Decision | Required action |
|---|---|---|
| Keep normal Kazan as grounded survival | Superseded by D3 and D5 | Make normal Kazan start in the existing same-character `PreviewFlight` mode. Keep survival as explicit grounded technical evidence only. |
| Freeze a new `Tour=KazanScaleTourV1` JSON contract and lifecycle | Rejected by D5 | The operator wants flight enabled, not scripted movement. Do not add route data, a runner, lifecycle, timeout, or handoff machinery. |
| Explicitly suppress flight when technical survival is selected | Rejected by verified gate code | The accepted scenario driver is flight-based and tests flight descent/slide. Explicit automation must select both Scenario and PreviewFlight; neither policy may be inherited or coupled inside ProjectSinglePlay. |
| Distinguish automatic movement from real keyboard input | Accepted principle; automatic movement removed | Normal Kazan and the packaged acceptance use the same pawn's normal Enhanced Input and Character Movement. No runtime automation drives the player. |
| Mechanically bind Track V to the accepted package | Accepted | F5 recomputes and compares the canonical package/executable identities against the F6 acceptance receipt before rendering. |
| Add a tour-specific presentation component | Rejected for D5 scope | There is no tour lifecycle. The existing local presentation already shows `PreviewFlight` controls even when no scenario is active; do not add a parallel component for identical behavior. |
| Reuse the World performance gate as product code | Rejected | It remains test authority under explicit flags, not packaged gameplay architecture. |
| Put packaged flight behavior in ProjectCinematic | Rejected by A5 and D5 | ProjectCinematic remains Editor-only Track V authority and cannot become a Shipping gameplay dependency. |
| Split the combined commit/history | Superseded by operator decision | Do not rewrite `C0`. Re-accredit each owner from this baseline with isolated follow-up source and evidence. |
| Water changed during the combined work | Accepted fact; cause not yet proven | Audit Water independently. Do not attribute it to Landscape or restore bytes blindly until authority history identifies the owning cause. |
| Material orphan parent, compiler fingerprint, SHA vectors | Accepted | Fixed regression-first after `C0`; run the full isolated material transaction before re-acceptance. |
| Scenario runtime parser weaker than schema | Accepted | Fixed regression-first after `C0`; retain strict runtime tests. |
| Survival p95 proves grounded performance | Rejected claim | The number is a PreviewFlight mechanics/streaming envelope. It supports D5 flight performance but does not prove grounded survival UX. |
| Generic World authority fixes are hidden feature details | Accepted evidence concern | Keep the code, but re-accredit semantic fingerprint and authored-overlay persistence through their own owner gates. |
| Firewall is release acceptance | Rejected | It is independent workstation setup, uninstalled unless an operator completes UAC, and never a package dependency. |

## Verified post-baseline facts

- `C0` exists and is cleanly recoverable in Git. No backup patch or worktree is
  needed to protect already committed bytes.
- The normal Kazan descriptor selects `Scenario=UrbanSurvivalProofV1`; it does not
  force `Traversal`. A stale ProjectLoading test was corrected to the selected
  product contract.
- The accepted Candidate therefore starts normal Kazan grounded with the survival
  scenario. `Launch_Kazan_PreviewFlight.cmd` already selects the desired manual
  `PreviewFlight` behavior through the real menu/loading route. The remaining D3/D5
  gap is product selection, technical-survival suppression, and final package truth;
  no movement feature is missing.
- `ProjectSinglePlayTraversal` already owns exactly two session states: absent or
  unknown resolves to `Default`, while case-sensitive `PreviewFlight` applies native
  `MOVE_Flying` after normal pawn initialization. `ADefinitionCharacter` already
  maps mouse/WASD/Space/Ctrl according to Character Movement mode.
- No player-facing runtime fly/walk toggle exists in current production code. The
  World gates change movement modes only under explicit test flags; they are not a
  product switch.
- `ProjectSinglePlay/Data/ModeOverrides.json` owns gameplay/difficulty feature
  configuration, not traversal. Adding flight there or inventing a second JSON
  experience format would duplicate the existing `Traversal` authority.
- `USinglePlayScenarioPresentationComponent` already displays the manual flight
  controls whenever `PreviewFlight` is active, independently of scenario activity.
- The existing packaged performance gate already drives the possessed
  `ADefinitionCharacter` through real input handling, Character Movement collision,
  and centre-edge-centre streaming, but it runs only under explicit test flags and
  cannot be exposed as product lifecycle code.
- ProjectCinematic owns authored Sequencer/MRQ execution and is excluded from packaged
  gameplay. Its release request is still bound to an older playable-tour receipt, so
  F5 must wait for the corrected final Candidate rather than driving F6 runtime.
- The first post-baseline launcher-engine build exposed two unity-only symbol
  collisions in ProjectWorld and ProjectSinglePlayClient. Both have minimal
  owner-local naming fixes; the replacement incremental build passes.
- Material parent-authority, compiler identity/SHA vectors, strict scenario profile,
  and Kazan descriptor projection tests pass after observed red failures.
- Water active authority moved from generation 11 at pre-`C0` Track P commit
  `4a3cb53714` to generation 15 in `C0`. The Water material digest changed from
  `8dbfb392...` to `c12a86d0...`.
- Water generations 13 and 14 record the same generator fingerprint and input
  identities but different material digests (`7c9d0696...` versus `c12a86d0...`).
  That is an idempotence/authority question requiring isolated diagnosis; the
  combined diff alone does not prove Landscape caused it.
- The rebuilt ProjectMaterialEditor candidate embeds compiler fingerprint
  `95f170cc...`, while the tracked accepted manifest still records `5eb76a6a...`.
  Until F1 regeneration and authentication pass, the implementation is a fixed
  candidate, generated Material authority is stale/pending re-accreditation, and
  Landscape K1 final acceptance remains blocked.

## Ordered stabilization flags

Only one unchecked flag may be promoted to `todo/00_current/` at a time.

### F0 - Immediate correctness closure

**Status:** PASS - focused correctness and replacement gates green

- [x] Reject instance recipes whose parent is absent from the current declared
  recipe set, even when leftover package bytes exist.
- [x] Derive compiler fingerprint from the sorted non-test compiler source/schema
  set at build time.
- [x] Add empty, `abc`, and multi-block SHA-256 vectors.
- [x] Enforce exact scenario schema identity, version, and root fields at runtime.
- [x] Enforce scenario IDs as exact ASCII `[A-Za-z][A-Za-z0-9_]*` identities
  before filesystem path construction.
- [x] Correct the stale Kazan descriptor projection test.
- [x] Fix the two unity-only naming collisions found by a real incremental build.

### F1 - ProjectMaterial isolated re-accreditation

**Status:** PASS - isolated owner authority re-accredited on 2026-08-28

- [x] Run the complete ProjectMaterial C++ gate: 12/12 passed.
- [x] Align the v1 `SlopeContrast` schema/parser/builder range to `[0.01, 16]`
  with exact boundary tests.
- [x] Include `ProjectMaterialEditor.Build.cs` in compiler fingerprint input.
- [x] Run repository inline-schema validation after the bound change.
- [x] Run the host transaction test.
- [x] Prove production validation is non-mutating and the accepted output set has
  no parent without a current parent recipe.
- [x] Prove source/schema fingerprint changes invalidate identity after rebuild,
  while test-only changes do not.
- [x] Prove the promoted `accepted.material-manifest.json` compiler fingerprint
  exactly equals the fingerprint embedded in the rebuilt compiler process that
  performed regeneration.
- [x] Re-run fresh process generation, zero-write idempotence, injected rollback,
  owner cleanup, and package/runtime census without invoking World realization.
- [x] Keep `ProjectMaterial/BuildUnit.yaml` as the last-successful-source-build
  baseline. The current ChangeScanner reports `ProjectMaterial` dirty and an
  authenticated dry run leaves the stored descriptor unchanged. Refreshing it
  during this launcher-only owner gate would falsely mark an unbuilt source-release
  unit clean; the next successful BuildService source-release pipeline owns refresh.
- [x] Keep BuildUnit scopes exact: code covers `Source`, `Config`, `Build.cs`, and
  the `.uplugin`; content covers `Content`. Material schemas, recipes, and manifests
  under `Data` are authenticated by the Material transaction/compiler fingerprint
  and are not falsely claimed as BuildUnit code/content coverage.
- [x] Restore the material todo to `DONE` only after this flag passes.

Accepted receipt summary:

- compiler fingerprint and both manifest records:
  `6ddb657ca77288a2e598eca8eb74b4ef58852ea12888874f7f63760f15e4151a`;
- C++ gate: 12/12; host transaction gate: 4/4;
- schema-only edits invalidate the launcher makefile and fingerprint, while
  test-only source does not change compiler identity;
- first production regeneration wrote two assets with one shader compile; the
  unchanged rerun wrote zero assets, compiled zero shaders, and preserved exact
  path/hash/mtime authority;
- all 635 ProjectWorldData generated/manifest files remained exact;
- Shipping IoStore contains both generated Terrain assets and excludes the Editor
  compiler, recipes, schemas, accepted manifest, and BuildUnit metadata;
- `tmp/material` and package census/cook/stage working outputs were owner-cleaned.

### F2 - Generic ProjectWorld semantic fingerprint

- [x] Identify the exact standing contract changed by the D3/provenance fix.
- [x] Run only its synthetic/full/incremental/rejected focused tests.
- [x] Prove the source change does not mutate generated packages outside the exact
  dirty scope.
- [x] Publish an owner-local receipt; do not claim Landscape or survival ownership.

**Status:** PASS - evidence/fingerprint owner re-accredited on 2026-08-28

Owner-local receipt:

- standing contract: `ProjectWorld.Input=<compile receipt>` is compile provenance,
  not D3 realized-product semantics; the semantic-evidence serializer is evidence
  code and belongs to no generated-byte producer fingerprint;
- `generator_fingerprint.Tests.ps1`: 4/4, including exact producer locality, true
  shared invalidation, catalog-extension stability, and unknown-producer rejection;
- `Project.World.Realization.Presentation.SemanticFingerprintIgnoresInputProvenance`:
  1/1; changed compile provenance preserves D3 while changed cell ownership moves it;
- `Project.World.Realization.Presentation.ActorLifecycle`: 1/1; incremental update
  and clean reconstruction converge on the same semantic fingerprint;
- exact pre/post hash, mtime, length, and path census: all 1,527 ProjectWorldData
  generated, external-generated-actor, and manifest files remained unchanged;
- no production realization or package promotion ran, and no Landscape or survival
  acceptance is inferred from this receipt.

### F3 - Generic authored-overlay persistence

- [x] Re-run generic overlay external-package persistence and reload tests.
- [x] Prove byte/anchor preservation across accepted regeneration.
- [x] Prove the fix works for synthetic fixtures without Kazan scenario behavior.
- [x] Snapshot the exact `-1` external-actor package, inject a later owning-operation
  failure after successful self-save, and prove rejection restores exact `-1` bytes,
  leaves active authority unchanged, and passes the next read-only audit. Reuse the
  existing transaction rollback mechanism rather than inventing another one.
- [x] Publish an owner-local receipt before survival relies on it as accepted
  generic authority.

**F3 receipt - PASS 2026-08-28:**

- synthetic-only owner: `ProjectWorldTestData`; Kazan data, scenario code, durable
  manifests, and ProjectWorldData generated bytes remained untouched;
- `Project.World.Authored.Anchor.ActorNoOp`: 1/1 through the exact launcher-engine
  test route;
- generated-content transaction Pester gate: 15/15;
- cold wrapper/commandlet integration accepted first Apply and cold reload; the
  stable `persistence_marker` package stayed at the same path, 4,920 bytes, and
  SHA-256 `6a7761d69c9d9afca9bb54f85dec3da06fe23fda7b1d65f49a1b2fd2fa156a76`;
- a 15 -> 20 degree TestData-only anchor change produced exactly one real
  `self_saved_actor_mutations`, then the owning-operation fault injection rejected
  with `test-after-self-save` and the existing transaction reported `rolled_back`;
- rejection restored the exact external-actor package and complete external tree,
  kept transient and durable active authority unchanged, and the next read-only
  audit accepted all 7 scopes and 31 byte-identical artifacts;
- accepted evidence:
  `Saved/Validation/WorldRealization/authored-overlay-persistence/59ec8d96049b4c60be5fec25c3eb1060/summary.json`;
- the outer snapshot restored tracked TestData bytes, failed evidence runs were
  pruned by the owner, and `tmp/world/authored_overlay_persistence` plus transaction
  scratch are empty.

### F4 - Water and Landscape locality

**Status:** PASS - current Water/Landscape authority re-accredited on 2026-08-28

- [x] Reconstruct Water generation 11 -> 15 authority history from immutable
  manifests and operation receipts.
- [x] Explain or reproduce the generation 13 -> 14 same-input material digest
  change. Classify it as: an owned semantic change; an unnecessary same-semantics
  rewrite; or independent clean-rebuild byte variability from Unreal package metadata.
  Treat unexplained non-idempotence as a blocker, but do not require independent
  destructive rebuild bytes to match unless the standing contract requires it.
- [x] Prove the primary invariant: same accepted inputs plus an intact accepted output
  performs zero writes.
- [x] Select the correct current Water authority from evidence; never restore old
  bytes merely because they are older.
- [x] Run Landscape K0/K1 from the accepted Material baseline and assert exact
  Water, Roads, Vegetation, Buildings, gameplay, and authored-overlay locality.
- [x] Mark Landscape technically accepted only after the isolated K1 receipt and
  visual comparison pass.

**F4 receipt - PASS 2026-08-28:**

- Water generation 11 -> 15 was reconstructed from immutable manifests. Generations
  11 -> 12 and 14 -> 15 were metadata-only. Generations 12 -> 13 and 13 -> 14 changed
  only `M_ProjectWorldWater.uasset`; all 290 other Water artifacts stayed exact.
- Generations 13 and 14 have the same inputs, generator fingerprint, normalized layer
  contract, and Water semantic identity `5cfa3c79...`. A current-code isolated clean
  reconstruction reproduced a different Water package digest with the same semantic
  identity, classifying the change as independent Unreal package metadata variability,
  not a geography or presentation-semantic change.
- The same isolated lifecycle run proved the primary invariant: the immediate no-op
  wrote zero Terrain, Water, Roads, Vegetation, Buildings, gameplay, authored-overlay,
  presentation, or manifest artifacts. Accepted receipt:
  `Saved/Validation/WorldRealization/3a-territory-isolation/5c0f153378c7426f941310dea88b7a08/summary.json`.
- Exact current survival validation accepted the F1 Terrain parent/MIC and changed no
  generated byte. Receipt:
  `Saved/Validation/WorldRealization/F4/current-survival-validate.json`.
- The first Territory replacement exposed one stale F3 consumer: the synthetic profile
  still expected zero authored anchors after F3 added one persistent marker. The profile
  now expects exactly one resolved/placed marker, its package bytes are authenticated as
  Matrix input, and focused regression tests pass.
- One replacement common Check and every selected Matrix share the same accepted Check
  identity: `check-20260828T095846Z`, P0 `run-20260828T100409Z`, Representative
  `run-20260828T101107Z`, and Territory `run-20260828T101909Z`.
- The Matrix chain proved exact no-op, road-only locality, one-cell incremental locality,
  rejected-apply rollback, clean reconstruction convergence, and unchanged authored
  package bytes across all six generated layers.
- A metadata-only migration re-accredited all 10 active scopes against current producer
  fingerprints without changing any stable manifest payload or generated artifact.
  Operation `25ab72c29c98409db2b7189ac4e43729` moved active authority from
  `0ec3be3e...` to `45d5ae23...`; the post-audit accepted 1,373 byte-identical artifacts,
  zero ownership gaps, and zero unresolved consumers. Receipts:
  `Saved/Validation/WorldAuthority/f4-current-fingerprint-migration.json` and
  `Saved/Validation/WorldAuthority/f4-post-migration-authority.json`.
- The correct Water payload remains the generation-15 artifact set; current authority is
  generation 16 only because its producer fingerprint metadata was re-accredited. No old
  Water byte was restored.
- Fresh current K1 visual evidence authenticated nine 1920x1080 fixed views. Direct pixel
  comparison against the same pre-K1 views proves the alternating debug grid is removed;
  Terrain continuity, relief, roads, and buildings remain aligned with no visible holes or
  displaced quadrants. Receipt:
  `Saved/Validation/WorldVisual/f4-landscape-k1-20260828T104840Z/capture.json`.
- The accepted F1 Terrain parent/MIC hashes are `bb3309a6...` and `cedada1d...`. The prior
  paired packaged K0/K1 measurement remains valid evidence for the frozen material design:
  Frame p95 `+0.057 ms`, GPU p95 `+0.103 ms`. The final combined Development package after
  F7 owns source-state performance revalidation.
- The World cleanup owner removed visual scratch, completed transaction scratch, and test
  scratch. Receipt: `Saved/Validation/WorldCleanup/20260828_134952.json`.

### F5 - Track V final release truth

- [ ] Implement and test the F5 receipt/path/hash/source verification during F6.
  Do not execute the final Track V capture until F6 technical acceptance and the
  operator's acceptance of the exact final Shipping PreviewFlight Candidate. The
  historical grounded-survival Candidate and old playable-tour receipt are not Track
  V truth authority.
- [ ] Reuse the existing Track P package owner:
  `Saved/PackageRelease/KazanPlayableTour/Candidate`. Do not introduce another
  Kazan package root merely because scripted movement was removed by D5.
- [ ] Verify the owner-local F6 acceptance receipt binds the exact Candidate path,
  package-tree hash, Shipping executable path/hash, source state/revision,
  runtime-profile/World authority, F6 operation, and operator product decision.
- [ ] Change the Track V request/schema/tests from stale `.../Current` input to the
  accepted `.../Candidate` contract. Update `kazan_release_v1.json` away from the
  old playable-tour composite.
- [ ] Canonicalize the supplied package path inside the expected package owner,
  recompute its tree and Shipping executable hashes, and compare them with the F6
  acceptance receipt. Also compare source/release identity. Refuse before render
  if any identity differs; do not copy hashes from a receipt while accepting an
  independently supplied path.
- [ ] Re-run staged/IoStore capture-only census on that accepted release package.
- [ ] Rerender the authored capture and bind it to the same accepted Kazan PreviewFlight
  release transaction, recording both release and Editor/MRQ execution identities.
- [ ] Retain Current plus one Previous capture; owner-clean all disposable render
  scratch.

### F6 - Kazan same-character PreviewFlight scale proof

**Status:** IMPLEMENTED - TECHNICAL ACCEPTANCE PENDING

#### Corrected acceptance story

Per D1, D3, and D5, the two maps have different product jobs:

```text
Old City 17
-> introduction to the mental/survival systems
-> grounded gameplay learning

Kazan
-> same possessed ADefinitionCharacter and composed ALIS systems
-> PreviewFlight is enabled automatically by the normal experience selection
-> player manually uses mouse/WASD/Space/Ctrl to inspect territory volume
```

Normal Kazan must not show cache/pouch/water/ration/shelter instructions, markers,
failure, or restart behavior. `UrbanSurvivalProofV1` remains an explicit real-route
automation profile that proves interaction, inventory, equipment, item use, vitals,
and terminal scenario behavior; it does not define the Kazan product narrative.
There is no scripted route, waypoint/timing JSON, automatic pawn movement, completion
handoff, or tour failure lifecycle under D5.

The two traversal states already exist and remain one authority:

```text
Traversal=PreviewFlight
-> enabled
-> native MOVE_Flying after normal pawn initialization

Traversal absent
-> disabled/default
-> normal grounded Character Movement
```

The implementation-time automation projection adds
`ProjectMenuPlayAutoScenario=UrbanSurvivalProofV1` beside the existing explicit
`ProjectMenuPlayAutoTraversal=PreviewFlight`. This preserves the accepted flight-based
technical gate after normal Kazan stops selecting the scenario. Neither policy is
inferred from the other, and no visible second menu choice or in-session toggle is
added.

#### Ownership and locality for R1

```text
owning black box:
  Kazan product selection -> KazanTerritory experience descriptor
  generic flight application -> ProjectSinglePlay traversal policy
  explicit technical projection -> ProjectMenuPlay automated experience seam

public contract:
  KazanTerritory experience descriptor custom options
  -> ProjectLoading URL projection
  -> possessed ADefinitionCharacter
  -> existing PreviewFlight Character Movement

expected components CHANGED after R1 PASS:
  Source/Alis KazanTerritory experience projection
  ProjectMenuPlay narrow automated Scenario/Traversal projection
  focused descriptor/menu/loading tests and packaged candidate runner
  Track V request/schema/wrapper/tests under F5
  stable ProjectSinglePlay docs after behavior is proved

expected components UNTOUCHED:
  current accepted Old City 17 descriptor/content/onboarding/display/internal identity
  ProjectSinglePlay traversal resolver/application behavior
  ProjectCharacter movement/animation/GAS architecture unless focused evidence finds a defect
  canonical Kazan inputs, generated packages, manifests, World Partition profile, and materials
  UrbanSurvivalProofV1 state machine/profile semantics
  ProjectCinematic Editor-only and Shipping-exclusion boundary
  ProjectSinglePlay mode JSON/schema and menu UI
```

Editing an UNTOUCHED owner is ARCHITECTURAL RED and returns the packet to R1. A
flight selection may not be placed in ProjectWorldData merely because it is selected
for Kazan; WorldData owns geography, not experience or traversal policy.

#### Premise / KISS gate

Observable outcome: selecting Kazan normally possesses the real character with native
flight already active, shows the existing controls hint, and immediately lets the
player inspect the whole territory. The existing descriptor custom option and
ProjectSinglePlay traversal policy already own this behavior. The implementation
changes product selection, not movement architecture.

Compared with the previous auto-tour proposal, D5 removes a Tour option, JSON schema,
profile, route data, runner, readiness signal, timeout/failure state, automated input,
handoff, and tour presentation. It adds no pawn, GameMode, camera director, runtime
ProjectCinematic dependency, World generator, menu branch, or fly/walk framework.

No new JSON is justified. `ModeOverrides.json` owns gameplay/difficulty features, and
the established `Traversal` option already owns flight selection. The knowingly
dropped v1 capabilities are scripted movement, runtime flight/ground switching,
agent-generated camera choreography, branching route logic, and a second Kazan
tutorial. Track V retains authored cinematic composition only.

Materially viable alternatives for reviewer evaluation:

| Option | Decision entering R1 | Reason |
|---|---|---|
| Normal Kazan descriptor selects existing `PreviewFlight` | Selected by D5 | Reuses the complete proven movement path and changes only experience selection. |
| Add a Tour JSON/runner | Reject | Scripted movement is not required and adds multiple new authorities/lifecycles. |
| Put traversal in `ModeOverrides.json` or a new experience JSON framework | Reject | It conflates gameplay mode with traversal or duplicates the existing option authority. |
| Add a runtime toggle or two menu choices now | Reject under D6 | Current scale proof needs one clear default; either choice expands UI/input policy without present evidence. |
| Expose `FProjectWorldProductPerformanceGate` as gameplay | Reject | It is flagged acceptance machinery and receipt authority, not a player lifecycle owner. |
| Package ProjectCinematic/its MRQ sequence as gameplay | Reject | It violates the Editor-only capture boundary and does not prove the real character. |
| Direct transforms, teleport, or a synthetic pawn | Reject | They do not demonstrate the same character, native movement, or collision. |

#### Reviewer questions to close at R1

- [x] Confirm D5 eliminates every Tour/route/lifecycle implementation seam and that
  the existing descriptor -> loading -> traversal application path is sufficient.
- [x] Confirm no visible or runtime fly/walk toggle is required now. A4's additional
  grounded-survival premise was rejected by verified packaged-gate code.
- [x] Confirm explicit automation selects both `UrbanSurvivalProofV1` and
  `PreviewFlight` in ProjectMenuPlay without coupling Scenario and Traversal inside
  ProjectSinglePlay.
- [x] Confirm current `USinglePlayScenarioPresentationComponent` behavior is enough
  for the existing PreviewFlight controls notice in this bounded release; a naming/SRP
  refactor is not an F6 correctness requirement.
- [x] Confirm the accepted package owner remains
  `Saved/PackageRelease/KazanPlayableTour/Candidate` despite removal of autopilot.
- [x] Confirm that Track V remains authored Editor/MRQ footage and later binds to
  the final accepted PreviewFlight Candidate through the exact F5 identity checks.

#### Required invariants

1. The normal menu `KazanTerritory` route selects `Traversal=PreviewFlight` and
   does not select `Scenario=UrbanSurvivalProofV1`.
2. The normal route possesses the normal `ADefinitionCharacter`; gameplay features,
   Enhanced Input, native Character Movement collision, and streaming remain active.
3. The player, not runtime choreography, moves the pawn through mouse/WASD/Space/Ctrl.
   There is no Tour option, automatic movement, transform mutation, or synthetic pawn.
4. Explicit technical survival selects its scenario and `PreviewFlight` through the
   real product route because its accepted driver is flight-based. The two policies
   are projected independently and never inferred from one another.
5. Absent traversal resolves silently to grounded `Default`; exact `PreviewFlight`
   enables flight; unknown values retain their existing fail-closed warning behavior.
6. F6 makes zero further change to the current accepted Old City 17 descriptor,
   content, onboarding, display state, or internal City17 identity.
7. Canonical/generated Kazan geography and active World authority remain exact;
   this is experience selection only.
8. Physical RTX 4070 Development acceptance remains High, 2560x1440, D3D12, selected
   `512/1536`, Frame p95 `<= 16.67 ms`, and zero streaming failures. Shipping proves
   cook, normal product route, manual flight controls, and release identity separately.
9. Track V is rebound/rendered only after the operator accepts the exact final
   Shipping PreviewFlight Candidate and F5 independently verifies its bytes.

#### Test-first and verification plan

| Invariant | Red/cheapest proof | Final acceptance surface | Stop condition |
|---|---|---|---|
| Normal Kazan selects flight, not scenario | Extend exact `ProjectLoading.DescriptorResolution.Kazan.ProductRouteProjection`; current source fails because it emits Scenario and no Traversal | Shipping menu selection records the projected URL and possessed flying character | Scenario present or flight absent blocks publication |
| Same real character and manual input | Retain focused traversal/input tests; add no tour lifecycle test | One cold packaged Development process records possession, real mapped input displacement, collision, and centre-edge-centre streaming | Synthetic pawn, automatic movement, transform movement, or lost composition returns to R1 |
| Technical survival remains explicit | Add exact ProjectMenuPlay automation-projection red: starting from the Kazan descriptor, selected Scenario and PreviewFlight are both explicit | Existing Development and Shipping success/failure receipts retain flight-specific real-input/collision evidence | Missing Scenario, inherited-only selection, or lost flight evidence blocks F6 |
| Flight controls are clear | Characterize existing traversal presentation with no active scenario | Shipping normal route shows flight controls and accepts mouse/WASD/Space/Ctrl immediately | Missing/misleading hint or dead input blocks F6 |
| Old City 17 gets no F6 delta | Existing label/descriptor assertions plus current-baseline pre/post path/hash census | Packaged menu shows `OLD CITY 17`; F6 City17 owner census is exact | Label regression or any F6 City17 change blocks F6 |
| World authority stays frozen | Static changed-path classification and pre/post generated/manifests census | Candidate binds the same active World/profile identities and zero World writes | Any generated byte/manifest change returns to its owner |
| 4070 performance/streaming | Existing real-input product instrumentation observes the normal PreviewFlight selection; it is never packaged as gameplay | Authenticated Development receipt at the frozen hardware/settings budget | Gate failure blocks Shipping Candidate |
| Final footage tells the same truth | F5 request/census checks against the accepted Candidate | MRQ receipt and human clip comparison bound to exact package hashes | Stale package binding or misleading footage blocks release |

Red cases are written and observed before production edits. During iteration, use only
exact L0/L1 tests. One coherent replacement Development/Shipping package is the L4
exit gate after R2; broad suites and repeated cooks are not the debug loop.

#### Ordered implementation tasks after R1 PASS

- [x] Record R1 PASS, A4's evidence rejection, A5 resolution, and A6 in this todo.
  Add no Tour implementation.
- [x] Add and observe the three product-route reds: normal descriptor selection,
  explicit technical scenario projection, and exact F5 package binding.
- [x] Change the Kazan descriptor from `Scenario=UrbanSurvivalProofV1` to the existing
  `Traversal=PreviewFlight` custom option.
- [x] Extend the existing ProjectMenuPlay automation seam narrowly so technical
  survival adds its Scenario beside its existing explicit
  `ProjectMenuPlayAutoTraversal=PreviewFlight`. Do not infer either policy from the
  other inside ProjectSinglePlay.
- [x] Preserve the existing PreviewFlight controls notice and scenario messages;
  add no new presentation component unless a focused red proves current behavior
  fails, and remove no scenario test coverage.
- [x] Implement F5's receipt/path/hash verification and update its request/schema/tests.
- [x] Run exact descriptor, menu projection, traversal/input, scenario, cinematic
  wrapper, and governance tests; run the launcher incremental build and R2 diff review.
- [x] Diagnose the first Development package rejection without weakening the gate.
  Product route, possessed character, real input, collision, interaction, streaming,
  and correctness all passed; only Frame p95 rejected at `18.680 ms`. The same current
  bytes then passed a non-publishing standalone reproduction at `16.392 ms`, while
  draw calls, primitives, memory, route distance, and GPU cost remained comparable.
  This is run-level host/package variance, not evidence for a gameplay or World rewrite.
- [x] Disable unused UE network messaging for the unattended package acceptance process
  with `-NoMessaging`, matching the proven survival runner. A focused architecture red
  failed before the runner change and passed afterward. This avoids the recurring
  Windows Firewall prompt without changing normal packaged gameplay behavior.
- [x] Route completed playable-tour diagnostics through the existing World cleanup owner;
  no diagnostic package or CSV scratch survives under `tmp/` after evidence inspection.
- [ ] Run one final Development plus Shipping package transaction, unattended
  real-input acceptance, performance/streaming/collision evidence, visual screenshots,
  package census, and owner cleanup. Retain Candidate plus exactly one
  PreviousCandidate.
- [ ] Stop for the operator's packaged Kazan PreviewFlight walkthrough. On PASS, write the
  owner-local acceptance receipt and proceed to F5 against that exact package.

#### Historical grounded-survival Candidate

The following receipt remains accepted technical evidence but is superseded as the
player-facing F6 product decision by D3/D5. It must not be promoted as the final Kazan
release Candidate:

- Composite receipt:
  `Saved/Validation/Gameplay/KazanSurvival/a011add2b35145f8aef529fc80c74feb/composite.json`.
- Exact source state SHA-256:
  `8f482c65bcce7dbfc08207bfb4bc9c1ae29de900c37234dae392289b7b7a6df9`.
  Runtime-profile SHA-256:
  `d48a455af7769d8288889a5d24015278c4229dccee979b224dd8f0f9fd792f87`.
- Development and Shipping package/build/cook/stage/archive checks passed through
  the configured launcher UE 5.8 engine. Automated success and failure/restart
  product routes passed in both configurations.
- Development PreviewFlight instrumentation accepted Frame p95 `15.714 ms`, GPU
  p95 `9.949 ms`, zero streaming failures, a complete center unload/reload
  cycle, collision-blocked descent, collision slide, and `23,346` real input
  events. This remains mechanics/performance evidence, not grounded UX proof.
- Fresh Shipping Candidate:
  `Saved/PackageRelease/KazanSurvival/Candidate/Windows/Alis.exe`.
  Candidate tree SHA-256:
  `3736867d26fd1c3edf47c4ad7fd52b7142d39c1732143c853100b56663c672ab`.
  Shipping executable SHA-256:
  `a241d852395007452c1af84e332c597249e7ea40852445d4e2e1149ffd139307`.
- The authenticated Candidate tree includes
  `Saved/PackageRelease/KazanSurvival/Candidate/Launch_Kazan_PreviewFlight.cmd`
  for explicit manual whole-world inspection. The normal `Alis.exe` route remains
  grounded survival and is the red characterization; the helper already demonstrates
  the D5 movement behavior but is not the corrected normal product endpoint.
- The prior Candidate was retained as `PreviousCandidate`; owner runtime scratch
  was removed.

### F7 - Independent bugfix and workstation patches

- [x] Re-accredit the empty-equip-slot `INDEX_NONE` fix with its owner test.
- [x] Re-accredit the retarget-profile Blueprint fix with character parity and
  warning-count evidence.
- [x] Keep firewall installation separate. UAC installation and owner-rule
  verification are workstation operations, not release gates.

**F7 receipt - PASS (2026-08-28):**

- `ProjectIntegrationTests.UI.Framework.Inventory.EquipSlots.EmptySlotsUseCanonicalSentinel`
  passed 1/1. It directly proves all eight empty equipment projections use the
  canonical `INDEX_NONE` sentinel instead of valid instance id `0`.
- Canonical character parity RunId `20260828_105629_438` passed all five expected
  scenarios with complete capture, timeline, and summary artifacts under
  `Saved/Validation/CharacterDebug/`.
- The exact test log SHA-256 is
  `c4dfea2c0a3b14f294118b23535ff642691f539eefb33c982774623d07b4f661`.
  Its case-insensitive warning census is zero `UpdateRetargetProfile`, zero
  `Accessed None`, zero `Blueprint Runtime Error`, and zero
  `Blueprint Runtime Warning` occurrences.
- No F7 source or asset correction was required: the selected baseline fixes are
  correctly owner-local and pass their current regression authorities.
- Firewall rule installation remains an optional workstation operation requiring
  its own elevated owner action. It is not package content and did not gate F7.

## Completion

This stabilization todo closes only when F1 through F7 have individual evidence,
the final package/capture is bound to the selected source state, owner scratch is
clean, and the operator has accepted the same-character manual Kazan PreviewFlight
scale walkthrough in the exact final Shipping Candidate. Historical combined and
grounded-survival receipts remain useful diagnostics but are not substituted for an
owner flag's required isolated receipt or the corrected F6 product decision. After
F1-F4, F6, and applicable F7 code fixes, rebuild the final Development and Shipping
package from the selected source. Stop for the operator at that one final Shipping
checkpoint. On PASS, write the owner-local F6 acceptance receipt, then close F5 by
rendering and binding Track V to that exact accepted release transaction. The
gameplay runner retains its `Candidate`/`PreviousCandidate` contract; Track V
independently promotes `Current`/`Previous`. Only then record final release
promotion.

## Review record

- 2026-08-28 - First R1 returned PATCH on the scripted-tour contract, technical
  scenario suppression, input authority, Track V package binding, and Old City 17
  baseline wording.
- 2026-08-28 - D5 added and supersedes D2's scripted-tour interpretation; A1-A3
  rejected; A4-A5 added. F6 reduced to the existing two-state Traversal contract.
- 2026-08-28 - R1 PASS. D6 added; A4-A5 resolved; implementation authorized. F5
  verification code may land before the operator gate, while capture execution waits.
- 2026-08-28 - Verified scenario packaged-gate code rejected A4's grounded-survival
  premise. A6 preserves the accepted flight-based technical gate through explicit,
  independent Scenario and Traversal selection; the product D5 path is unchanged.
- **Next review scope:** R2 implementation diff and focused evidence before the one
  Development/Shipping acceptance transaction.
