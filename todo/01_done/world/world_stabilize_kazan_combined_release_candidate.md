# Stabilize the combined Kazan release candidate

**Status:** DONE - F6 MACHINE AND OPERATOR ACCEPTED; TRACK V TRANSFERRED
**Active flag:** None
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
- **D7** Accept the exact final Shipping Kazan Candidate and close stabilization at
  the playable-product boundary. Do not execute Track V as part of this task; transfer
  future agent-authored video work to its dedicated backlog owner.
  - Effect: the generated operator-acceptance receipt owns the exact accepted product
    identity. This todo records the decision and routing only.
  - Reason: Kazan product proof is complete; future cinematography is a separate outcome.
  - Date/source: operator PASS and scope decision, 2026-09-02.

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
- **A7 [CLASSIFIED]:** Reviewer-raised visual scale uncertainty was not evidence of a
  global coordinate or Building realization defect. Focused unit tests and live-product
  metrology prove metre-to-centimetre conversion, unit actor/component transforms, and
  ordinary character/body/camera dimensions. The two pinned landmark controls instead
  expose a generic `building:part` admission/representation limitation: taller part-only
  source ways are absent from current `nwr/building` canonical authority. F6 may proceed
  to its final Candidate after the focused source changes pass; Track V still waits for
  operator acceptance. No canonical/generated World mutation or territory regeneration
  is authorized by this classification.

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
| Treat the visual concern as proof of a global scale defect | Rejected as unverified | Measure the consumed coordinate, mesh, actor, character, and camera surfaces first. Preserve the current package as a visual control; do not rebuild World while classifying. |
| Authenticate only dirty-tree state before Track V | Accepted defect | F5 must also compare the current `git rev-parse HEAD` exactly with the operator-accepted `source_revision`; two clean revisions can otherwise share the same empty-diff digest. |
| Require per-feature generated actor bounds for landmarks | Narrowed to the actual representation | Buildings are realized as combined cell-local StaticMesh actors, not one actor per OSM feature. Prove synthetic per-feature dimensions through the real builder, unit scale on generated cell actors/components, and local source-to-canonical landmark semantics; do not invent feature actors for diagnostics. |
| Reuse held Shift for fast manual PreviewFlight | Accepted, evidence-gated | Reuse the existing Sprint action. Grounded sprint stays unchanged; flying boost is owner-local Character Movement tuning and never drives the performance acceptance route. |

The reviewer's claim that an operator observation had already established a hard scale
defect is rejected as unsupported by the recorded operator evidence. The visual question
was worth measuring, but it did not authorize treating a hypothesis as a result.

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

**Status:** CONTRACT PASS - EXECUTION DEFERRED AND TRANSFERRED

- [x] Implement and test the F5 receipt/path/hash/source verification during F6.
  Do not execute the final Track V capture until F6 technical acceptance and the
  operator's acceptance of the exact final Shipping PreviewFlight Candidate. The
  historical grounded-survival Candidate and old playable-tour receipt are not Track
  V truth authority.
- [x] Reuse the existing Track P package owner:
  `Saved/PackageRelease/KazanPlayableTour/Candidate`. Do not introduce another
  Kazan package root merely because scripted movement was removed by D5.
- [x] Verify the owner-local F6 acceptance receipt binds the exact Candidate path,
  package-tree hash, Shipping executable path/hash, source state/revision,
  runtime-profile/World authority, F6 operation, and operator product decision.
- [x] Change the Track V request/schema/tests from stale `.../Current` input to the
  accepted `.../Candidate` contract. Update `kazan_release_v1.json` away from the
  old playable-tour composite.
- [x] Canonicalize the supplied package path inside the expected package owner,
  recompute its tree and Shipping executable hashes, and compare them with the F6
  acceptance receipt. Also compare source/release identity. Refuse before render
  if any identity differs; do not copy hashes from a receipt while accepting an
  independently supplied path.
- [x] Preserve exact source identity in the eventual Track V receipt itself: record
  both `source_revision` and the dirty/untracked source-state digest after checking
  both before Editor launch and after capture.
- [x] Transfer future staged/IoStore census, authored capture, deterministic render,
  receipt binding, promotion, and cleanup to the dedicated cinematic backlog todo.
  No Track V render was executed during stabilization.

### F6 - Kazan same-character PreviewFlight scale proof

**Status:** PASS - MACHINE ACCEPTANCE AND OPERATOR WALKTHROUGH COMPLETE

#### 2026-08-31 operator walkthrough rejection - submerged Water

The prior fixed-camera Water proof covered one visible surface but did not prove global
Terrain/Water vertical order. The operator found the Kazanka area behind the Kremlin
covered by green Landscape even though Water actors exist there. That Candidate is
rejected and cannot become the F6 or Track V truth source.

The active canonical bundle contains the expected Water geometry. A global diagnostic
over the exact active authority sampled 54,076 Landscape points inside canonical Water
footprints and found 4,736 (`8.758 percent`) where raw DEM Terrain is above canonical
Water. Worst clearance is `-11.25 m`. Kazanka group `kazan_kazanka` alone has 1,925 of
6,821 submerged samples and a `-9.35 m` minimum. This is not missing GIS, material
failure, or only coplanar z-fighting. Raising all Water by the worst error would visibly
float the rivers and is rejected.

The bounded correction keeps canonical Terrain and Water byte-identical. ProjectWorld's
generated Landscape projection consumes ordered selectors `terrain`, `water` and lowers
only Landscape samples inside exact Water footprints to
`min(canonical_terrain_z, canonical_water_surface_z)`. The Water mesh remains flat or
monotonic at canonical Water Z plus the existing `0.25 m` presentation offset. This
adopts the native/industry hydro-conditioning principle without enabling the Experimental
Epic Water plugin, introducing WaterBody/Landmass authority, painting Water into the
Landscape material, or adding a Kazan override.

Expected changed components are the ProjectWorld Landscape projection, Terrain
verification, typed realization tuple/schema/profiles, composite cell input identity,
producer fingerprint, focused tests, stable World docs, generated Landscape proxies,
and declared generated dependants selected by the existing layer DAG. Canonical bytes,
Water surface functions/elevations/geometry, Water material, World Partition budgets,
ProjectMaterial, gameplay, menu, Old City 17, and Track V remain untouched.

- [x] Authenticate the active canonical bundle and prove the affected Kazanka Water
  polygon/surface group exists.
- [x] Measure Terrain-versus-Water clearance globally rather than extrapolating from one
  screenshot or one fixed camera.
- [x] Implement the owner-local Min-style Landscape projection, ordered Terrain/Water
  selector tuple, Water-sensitive cell identity, and generated-proxy semantic migration.
- [ ] Pass focused projection, profile, dirty-locality, generator-fingerprint, schema,
  native Water, and Landscape verification tests against the rebuilt Editor.
- [ ] Realize Kazan through the normal immutable owner transaction; prove exact canonical
  bytes, expected layer-DAG propagation, immediate zero-write idempotence, authenticated
  generated authority, and owner-clean scratch.
- [ ] Prove global post-projection vertical order and inspect the exact rebuilt Kazanka
  and Volga Water surfaces from above using process-local Editor/package screenshots.
- [ ] Rebuild the coherent Development/Shipping Candidate and rerun the exact Shipping
  Water temporal/product gate before returning to the operator walkthrough.

#### 2026-08-31 pre-freeze Water and performance audit

The Terrain inventory and the direct Landscape realization helper previously encoded the
same ordered `terrain`/`water` tuple in two functions. The shared composite canonical-input
serializer is now the single owner used by both paths. The exact incremental-inventory test
proves byte-identical identities for a dry cell, a referenced/cross-cell Water cell, and an
owned polygon-Water cell through the real inventory builder.

Current canonical Water contains 115 admitted surfaces: 81 polygons and 34 ribbons. The
accepted canonical compiler already rejects polygon sample overlap and ribbon/polygon
overlap; every current ribbon reports zero polygon overlap. A bounded read-only check of all
current ribbon footprints found nine continuous confluence/touch intersections but zero
Landscape samples belonging to two different ribbon surface groups. Therefore the current
Landscape `min` projection does not hide incompatible Water heights, and no new overlap
policy or runtime Water system is justified.

The two Development package failures are classified as Render/RHI scheduling variance, not
a generated-content topology regression. Against the latest accepted same-machine/profile
run, actor counts, Landscape proxies, draw calls, primitives, route points, streaming-cell
counts, memory, and GPU p95 remained effectively equal. The narrow replacement measured
Frame/Render p95 `16.997/16.992 ms` and GPU p95 `9.751 ms`; the largest deltas were Render
visibility wait and Game event-wait time. The frozen `16.67 ms` threshold remains unchanged.
There will be no random retry: the next unprofiled run is the one final coherent transaction,
and another miss returns a blocker.

The existing Shipping Water proof now targets canonical Kazanka relation
`alis:osm:relation:7493502` at Unreal XY `[0, -80000]` cm, inside
`grid_413718bc833994e5:x1:y1` immediately north of the Spasskaya control. This replaces the
unrelated reservoir viewpoint so the final exact PNG directly covers the operator-observed
absence. The proof owner and camera mechanism are unchanged.

#### 2026-08-29 operator walkthrough rejection

The authenticated Candidate passed its machine gate but failed the real product
walkthrough. It is rejected and must not be used as the F6 acceptance or Track V
truth source. The operator observed four bounded defects:

1. Escape does not open the in-game menu.
2. PreviewFlight can enter generated Building massing and become trapped.
3. The product spawn does not begin at the Spasskaya Tower/Kremlin control.
4. Generated Water does not read clearly; the prototype needs an immediately
   visible blue surface.

The first winding correction made Water visible, but the next walkthrough exposed a
second independent defect: standing Water and the Landscape beneath it can both be
exactly `49.5 m`. The resulting coplanar triangles z-fight and the reflective placeholder
drifts over repeated rendering states. The first fixed-camera pair is an observed red:
`0.0023753111` blue-classification flip ratio and `2.2916834376` mean blue-channel
delta, above the Water-specific gates `0.001` and `1.5`.

The corrective ownership boundary supersedes the earlier F6 source freeze only for
these defects:

```text
ProjectMenuGame
-> pause overlay and game-menu service

ProjectSinglePlayClient
-> existing Escape Enhanced Input action
-> resolves the game-menu service through ProjectCore DIP

ProjectWorld territory runtime profile
-> data-owned product overview spawn at the engine georeference origin
-> that origin is the rounded projection of the pinned Spasskaya control

ProjectWorld Building realization
-> double-sided complex collision for closed generated massing
-> owned semantic migration forces the generated collision bytes to update

ProjectWorld Water realization/inventory
-> profile-owned 0.25 m realization separation above unchanged canonical Water Z
-> time-invariant opaque blue prototype material
-> owned material semantic migration prevents a stale clean early-out
```

The exact Spasskaya coordinate is inside its admitted Building footprint. The
PreviewFlight PlayerStart therefore keeps the landmark horizontal anchor but uses a
profile-owned height above terrain so the pawn begins outside collision and the first
view communicates territory scale. This is not a landmark geometry override.

Expected changed components are ProjectMenuGame, the existing SinglePlay client input
projection, ProjectWorld runtime/building/water owners plus the bounded 2026-08-31
Landscape projection correction, their focused tests, the three territory runtime
candidates, and generated authority produced by the normal realization transaction.
Declared dependants may be rewritten only through the existing layer DAG. Old City 17,
canonical Kazan Terrain/Water bytes, survival scenario behavior, flight tuning, and
Track V remain untouched. Unexpected propagation outside this boundary aborts the
realization transaction.

- [x] Add one Escape-owned pause overlay with Resume, Main Menu, and Quit; Escape must
  close it while paused and no direct map travel may bypass ProjectLoading.
- [x] Prove the Escape mapping, visible pause state, ProjectUI definition, and second
  Escape resume through the real packaged input driver. The Main Menu button is not
  claimed as automated acceptance: its production callback is source-audited to build
  `MainMenuWorld` through `ILoadingService` and call `StartLoad`, with no direct map
  travel; the completed operator walkthrough owns its product-UI acceptance.
- [x] Add a territory-product overview spawn contract anchored to the engine
  georeference origin, with explicit height/yaw/pitch in each territory candidate.
- [x] Prove the realized PlayerStart is horizontally at the Spasskaya-derived origin,
  above terrain/building massing, always loaded, and leaves bounded-route profiles
  unchanged.
- [x] Make generated Building triangle collision double-sided and migrate its semantic
  identity so unchanged geometry cannot preserve stale one-sided cooked collision.
- [x] Prove the persistent production Building component is complex-as-simple and
  double-sided, and blocks character-sized `ECC_Pawn` capsule sweeps plus ray queries
  across the same known wall from outside-to-inside and inside-to-outside without
  changing mesh dimensions. The regression control rebuilds that body setup with
  double-sided geometry disabled and proves one ray-query direction is culled before
  restoring and rebuilding the accepted double-sided state.
- [x] Reproduce Water instability from one fixed camera and add a durable repeated-pose
  pixel gate with synthetic stable, classification-flip, and color-drift tests.
- [x] Raise realized Water vertices by the profile-owned `0.25 m` separation while
  keeping actor placement and canonical Water elevation exact.
- [x] Replace the temporary reflective material with a time-invariant opaque blue
  placeholder, migrate its owned semantic identity, and keep no-collision/non-Nanite.
- [x] Prove the historical fixed-camera Water pair passes the temporal gate. That earlier
  material/offset transaction left Terrain, Roads, and canonical authority exact; it did
  not establish the global layer-order proof now required above.
- [ ] Bind the same Water proof to exact Shipping pixels without taking over the
  operator's desktop. The existing ProjectWorld product acceptance owner must drive the
  possessed `ADefinitionCharacter` through process-local simulated input, pause at the
  accepted Water viewpoint, write a game-owned identical-pose pair under the Shipping
  run, and pass the existing temporal verifier unchanged. OS-global `SendInput`, Editor,
  MCP, direct transforms, and a second pixel verifier are not acceptance evidence.
- [ ] Re-run the coherent Development/Shipping package gate against this final source
  and generated authority; prior Main Menu -> Kazan MCP/input/camera evidence remains
  regression evidence but cannot authenticate the rebuilt Candidate.
- **Receipt-owned final package gate:** retain the replacement Candidate plus one
  PreviousCandidate, owner-clean disposable runtime scratch, and bind the exact final
  source state. The authenticated composite, not a post-build edit to this todo, owns
  PASS/FAIL for that gate.
- [ ] Operator walkthrough of that exact Shipping Candidate. F5 remains blocked until
  the Candidate receives operator PASS.

#### 2026-08-30 Water stability receipt

- The observed red pair is preserved under
  `Saved/Validation/WorldRealization/water_stability_20260830/visual/before/`.
  It reports `0.0023753111` Water/ground classification flips and `2.2916834376/255`
  mean Water-channel drift, rejecting both `0.001` and `1.5/255` gates.
- The canonical standing reservoir and its underlying terrain both resolve to
  `49.5 m`; the correction changes no canonical byte. The realization profile owns a
  `0.25 m` positive presentation separation, applied to Water mesh vertices while the
  geospatial actor origin remains exact.
- The temporary Water graph is three constants only: blue BaseColor, Roughness `1.0`,
  and Specular `0.0`; it is opaque Default Lit, non-Nanite, and no-collision. A tested
  Unlit alternative was rejected because auto-exposure made the large surface nearly
  black.
- Final Apply dirtied Water plus its declared Vegetation consumer only. Terrain, Roads,
  Buildings, gameplay, and canonical authority remained exact. Immediate repeat Apply
  reported zero dirty units for all six layers and the same semantic fingerprint.
- Final generated-authority audit accepted all 10 scopes and verified 1,373 artifacts
  byte-identical. Receipts are under
  `Saved/Validation/WorldRealization/water_stability_20260830/`.
- The authenticated final fixed-camera pair is under `visual/after/`. It reports
  `0.0008190766` classification flips and `1.3113507693/255` mean channel drift, passing
  both Water-specific temporal gates. Human inspection confirms continuous bright blue
  Water with no broad blue/green depth-interference triangles.

#### Scale classification receipt

No generated World package, canonical bundle, active manifest, or realization profile
was changed during this audit.

- `Project.World.Realization.CanonicalCoordinatesRoundTrip` proves independent `+1.000 m`
  East/North/vertical deltas become `+100/-100/+100 cm`, in addition to inverse
  round-trip coverage.
- `Project.World.Realization.Buildings.Dimensions` sends real `10 m x 10 m` fixtures at
  `2/9/27 m` through `ProjectWorldBuildingMeshBuilder`; measured bounds are
  `1000 x 1000 x 200/900/2700 cm`. The persistent-layer regression separately proves
  unit generated actor and StaticMeshComponent scale.
- Live MCP inspection entered Kazan through MainMenu -> ProjectLoading, possessed the
  real `ADefinitionCharacter`, and recorded actor/camera/skeletal-component unit scales,
  capsule `23/88 cm`, camera FOV `90`, visible body bounds, and three loaded combined
  Building cell actors with unit actor/component scale in
  `Saved/Validation/WorldScale/current_kazan_mcp_20260829.json`. The same live MCP
  receipt binds the FOV-90 game-viewport screenshot, real W-driven velocity, and the
  held-Shift `5x` values plus exact release restoration; it remains diagnostic evidence
  rather than a substitute for the packaged gate or operator walkthrough.
- The pinned source/canonical audit is authenticated in
  `Saved/Validation/WorldScale/kazan_landmark_source_chain.json`. The two admitted base
  ways are `12 m` and `8 m`; the same pinned source contains related part-only volumes
  up to `58 m`, but the active source selector is `nwr/building`. Canonical centroid
  separation is `237.6768 m`, equivalent to `23,767.68 cm`, so horizontal scale is sane.
- The independent geodetic proof was rerun from
  `Data/Controls/kazan_territory_v1.control.json`, not from realized UTM points. The
  accepted control-network receipt uses the pinned PROJ transform for raw
  `EPSG:4326 -> EPSG:32639` forward/inverse placement and separate WGS84 ellipsoid
  geodesic math for distance/scale. Its three non-collinear controls are Spasskaya,
  technical northwest, and technical southeast. Maximum forward/inverse errors are
  `0.0000649239/0.0000649047 m`, RMS forward error is `0.0000374838 m`, maximum
  pairwise error is `0.0000648191 m`, maximum corner error is `0.0000000294 m`, and
  the 5.58 km UTM/geodesic scale delta is `1.31397723099 m <= 1.50 m`.
  `test_control_network.py`, the `controls` operation, and the full `authority`
  operation all passed. The receipt SHA-256
  `650273ac6aa096ad2ee638fb72f203d7f5335c8a2eb1ea7cf16391403c1862fc`
  matches the exact entry authenticated by
  `Data/Canonical/kazan_territory_v1/active.json` and its immutable authority ZIP.
  No duplicate CRS implementation is justified.
- Per-feature runtime bounds are intentionally not claimed: one generated actor/mesh
  combines many cell features. Adding diagnostic feature actors would change the
  representation being tested. Builder fixtures plus live cell transform checks prove
  the actual boundaries without creating a parallel World model.
- `ProjectIntegrationTests.Character.Traversal.ModeAwareInput` drives the existing
  Enhanced Input actions and measured approximately `4,800.915 cm/s` before Shift and
  `25,765.477 cm/s` during the boost window, with configured flight maxima
  `12,000 -> 60,000 cm/s`. It also proves grounded Shift remains Sprint and every tested
  release, movement-mode-exit, and unpossession paths restore the exact prior flying
  values. `EndPlay` invokes the same idempotent cleanup defensively before teardown;
  no post-teardown consumer is claimed as a separate acceptance surface.

Classification is case C from the reviewer packet: consumed coordinate and realization
scale are correct; selected skyline controls are low because source semantics are omitted
before canonical compilation. That limitation is routed to the existing Buildings packet
and durable territory contract. It is not patched with landmark constants and does not
block this honest prototype blockout Candidate.

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

expected components CHANGED after R1 PASS and the bounded operator-rejected Candidate correction:
  Source/Alis KazanTerritory experience projection
  ProjectMenuPlay narrow automated Scenario/Traversal projection
  ProjectCharacter owner-local PreviewFlight boost through the existing Sprint action
  ProjectSinglePlay existing PreviewFlight controls text
  ProjectMenuGame pause overlay plus ProjectCore service contract and SinglePlay Escape projection
  ProjectWorld runtime-profile spawn, Building collision, and Water material realization owners
  generated Building/Water/map authority plus dependency-driven Vegetation exclusion reaccreditation
  focused descriptor/menu/loading tests and packaged candidate runner
  Track V request/schema/wrapper/tests under F5
  stable ProjectSinglePlay docs after behavior is proved

expected components UNTOUCHED:
  current accepted Old City 17 descriptor/content/onboarding/display/internal identity
  ProjectSinglePlay traversal resolver/application behavior
  ProjectCharacter animation/GAS/rotation architecture and grounded movement tuning
  canonical Kazan inputs, Water surface authority, road geometry policy, and World Partition streaming budgets
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
7. Canonical Kazan Terrain, Water, and road authority remain exact. The generated
   Landscape is the deterministic Terrain-plus-Water projection; its bounded correction
   and declared layer-DAG dependants advance only through the normal immutable manifest
   transaction, immediate idempotence, and read-only authority audit.
8. Physical RTX 4070 Development acceptance remains High, 2560x1440, D3D12, selected
   `512/1536`, Frame p95 `<= 16.67 ms`, and zero streaming failures. Shipping proves
   cook, normal product route, manual flight controls, and release identity separately.
9. Track V is rebound/rendered only after the operator accepts the exact final
   Shipping PreviewFlight Candidate and F5 independently verifies its bytes.
10. Held Shift while flying selects the bounded `5x` overview candidate and scales only
    active flying speed/acceleration/braking; release, unpossession, or leaving flight
    restores the exact previous values. `EndPlay` defensively invokes the same cleanup
    before teardown. Grounded Sprint remains unchanged.
11. Core unit conversion, real Building builder dimensions, live character/body/camera
    scale, and generated Building cell transforms remain covered without regenerating
    World content.

#### Test-first and verification plan

| Invariant | Red/cheapest proof | Final acceptance surface | Stop condition |
|---|---|---|---|
| Normal Kazan selects flight, not scenario | Extend exact `ProjectLoading.DescriptorResolution.Kazan.ProductRouteProjection`; current source fails because it emits Scenario and no Traversal | Shipping menu selection records the projected URL and possessed flying character | Scenario present or flight absent blocks publication |
| Same real character and manual input | Retain focused traversal/input tests; add no tour lifecycle test | One cold packaged Development process records possession, real mapped input displacement, collision, and centre-edge-centre streaming | Synthetic pawn, automatic movement, transform movement, or lost composition returns to R1 |
| Technical survival remains explicit | Add exact ProjectMenuPlay automation-projection red: starting from the Kazan descriptor, selected Scenario and PreviewFlight are both explicit | Existing Development and Shipping success/failure receipts retain flight-specific real-input/collision evidence | Missing Scenario, inherited-only selection, or lost flight evidence blocks F6 |
| Flight controls are clear | Characterize existing traversal presentation with no active scenario | Shipping normal route shows flight controls and accepts mouse/WASD/Space/Ctrl immediately | Missing/misleading hint or dead input blocks F6 |
| Old City 17 gets no F6 delta | Existing label/descriptor assertions plus current-baseline pre/post path/hash census | Packaged menu shows `OLD CITY 17`; F6 City17 owner census is exact | Label regression or any F6 City17 change blocks F6 |
| World correction remains owner-local | Static changed-path classification plus transaction idempotence and pre/post generated/manifests census | Candidate binds the accepted active World/profile identities; post-package authority audit is read-only and exact | Unowned propagation, canonical drift, unexpected generated-layer propagation, stale fingerprints, or package-time World writes block publication |
| Coordinate/Building scale is real | Exact independent unit-delta and production-builder dimensional tests | Live product-route MCP receipt proves character/camera/cell actor transforms; local source-chain receipt classifies landmark semantics | Unit/builder transform failure blocks packaging; omitted source parts return to Buildings research, never ad hoc repair |
| Fast overview is bounded | Extend exact real-input Character traversal test through grounded, flying boost, release, mode exit, and unpossession | Operator inspects the final Shipping Candidate; base-speed route remains performance authority | Stuck tuning, grounded regression, or demonstrable streaming holes rejects `5x` and permits one `4x` fallback |
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
- [x] Classify the scale question without World mutation: independent coordinate deltas,
  real builder dimensions, live product-route character/camera/cell transforms, and two
  pinned landmark source chains all pass their applicable boundaries. Route the omitted
  `building:part` limitation to Buildings instead of hardcoding landmarks.
- [x] Reuse held Shift for the owner-local `5x` PreviewFlight overview candidate, scale
  active flight acceleration/braking with speed, prove release/mode-exit/unpossession
  restoration, retain defensive teardown cleanup, preserve grounded sprint, and update
  the existing controls notice.
- [x] Close F5's clean-revision hole by comparing current `git rev-parse HEAD` with the
  accepted Candidate `source_revision` before Editor launch and after capture; retain the
  dirty-tree digest as the independent uncommitted-state check.
- **Receipt-owned final package gate:** after the last tracked R2 correction, run one
  final Development plus Shipping package transaction, unattended
  real-input acceptance, performance/streaming/collision evidence, visual screenshots,
  package census, and owner cleanup. Retain Candidate plus exactly one
  PreviousCandidate inside that runner-owned pair. The latest authenticated composite
  is the sole PASS/FAIL authority for this gate; do not edit this tracked file merely
  to copy its post-build result.
- [x] Before that transaction cooks, authenticate the Shipping Water viewpoint against
  the exact compile result selected by the active Water manifest. Require the configured
  XY to remain inside the expected canonical Water feature, record its canonical cell and
  feature identity, and bound the runtime pawn-to-target XY error in the composite. The
  unchanged temporal verifier owns stability; agent inspection of the exact Shipping PNG
  owns the semantic claim that its blue pixels are the visible Water plane.
- [x] Preserve the existing F5 source-identity seam: `source_revision` owns exact HEAD,
  while `source_state_sha256` owns the binary tracked diff plus sorted untracked
  path/content-hash entries relative to that HEAD. Prove the World runner and
  ProjectCinematic calculate the same digest, prove tracked and untracked byte changes
  move it, and prove a different clean revision remains a separate revision failure.
- [x] Keep the Shipping Water gate's anonymous-namespace symbols file-distinct so the
  launcher unity build cannot merge them with another ProjectWorld translation unit.
  This is a naming-only build correction; it does not change Water behavior or evidence.
- [x] The operator passed the exact packaged Kazan PreviewFlight walkthrough. The
  owner-local generated acceptance receipt is
  `Saved/Validation/WorldRealization/playable-tour/Candidate/operator-acceptance.json`.
  Track V was deliberately deferred rather than executed from this task.

The exact final operation id, package hashes, measurements, and screenshots remain in the
owner-generated `Saved/Validation/WorldRealization/playable-tour/<run-id>/composite.json`.
Do not copy post-build receipt values into tracked files before operator acceptance: that
would change the source-state digest bound by the Candidate. A source edit or commit after
this gate requires rebuilding/rebinding before F5.

#### Final transaction blocker - 2026-09-01

- Run `fcf91a33603d43ff95e4d64241d33f0f` passed the complete Development
  product route at Frame p95 `13.627 ms`, Frame p99 `16.462 ms`, Game p95
  `5.412 ms`, Render p95 `13.644 ms`, GPU p95 `9.572 ms`, and zero streaming
  failures. Development and Shipping cook/archive both passed.
- Shipping product-route correctness passed, but the Shipping Water receipt was
  rejected before composite publication. The new Kazanka target is only about
  `550.29 m` of real input travel from the packaged spawn, while the runner still
  requires the old target's absolute `1000 m` displacement. The target itself was
  reached within the declared `250 m` arrival radius (`192.87 m` XY error).
- Applying the unchanged temporal verifier to the exact Shipping PNG pair also
  rejected it: `322807` blue-union pixels, `8589` classification flips,
  flip ratio `0.0266072` versus the `0.001` limit, and mean color delta `0.146113`.
  The current full-frame blue classifier includes the blue sky and does not isolate
  canonical Water pixels, so this result cannot yet distinguish Water instability
  from non-Water temporal/capture variance.
- Smallest safe next correction: make Shipping travel acceptance target-relative
  instead of retaining an unrelated absolute-distance threshold, and make the
  temporal proof measure a fixed canonical-Water frame from one persistent,
  fixed capture state. Do not weaken the numerical temporal gate or infer PASS from
  visual similarity alone.
- [x] Replace the stale `1000 m` Shipping assertion with a target-relative contract:
  actual start-to-target distance, the retained `250 m` arrival radius, bounded final
  target error, and accumulated real-input travel at least distance minus radius.
- [x] Keep one transient `SceneCapture2D` and render target alive across both images.
  Anchor UE's built-in top-down orthographic `SCS_BaseColor` capture to the authenticated
  canonical Water XY. This keeps real Water/Landscape depth while excluding sky and
  lighting noise, so no ROI, second verifier, debug material, or special Water renderer
  is required.
- [x] Focused correction evidence: launcher-engine incremental build passed;
  `ProjectWorld.PlayableTour.ShippingWater.TargetRelativeTravel` passed `1/1`; existing
  Water temporal tests passed `3/3` with unchanged thresholds. The first exact base-color
  Shipping pair then exposed a verifier defect: UE preserved valid blue RGB under alpha
  zero, but the verifier's GDI+ copy alpha-composited it to black. The byte-preserving
  reader now passes the same exact pair with `2018266` blue pixels, zero flips, and zero
  color drift; the existing stable-blue test was converted to the UE zero-alpha regression
  instead of adding redundant test coverage. Final coherent Shipping runtime remains
  pending.
- The failed runner restored the prior Candidate and cleaned its runtime workspace.
  No composite was published, Track V was not started, and the operator must not
  review this failed package as the final Candidate.
- The accepted `f9e6293ffa97447b85163df09caf82e9` replacement proved the BaseColor
  geometry/occlusion pair, but its normal Shipping `product-route.png` frames the city
  and only small canal patches, not the authenticated Kazanka target. BaseColor is not
  the normal lit/tone-mapped player surface. Keep that pair unchanged and add exactly one
  target-centered perspective `SCS_FinalColorLDR` `water-product.png` with normal show
  flags. The composite must bind its path/hash/source/camera and the existing canonical
  cell/feature. Agent inspection, not a self-asserted receipt boolean or another pixel
  verifier, owns the semantic visual verdict.
- The focused FinalColor implementation compiled through the launcher engine; the exact
  target-relative C++ test passed `1/1`, the unchanged temporal tests passed `3/3`, and
  World/Cinematic source identity passed `2/2`. Transaction
  `311cd7275386420bacd2c1b5ec4b9ab3` then stopped before Shipping: every Development
  correctness/input/collision/streaming invariant passed with zero streaming failures,
  but Frame/Render p95 measured `16.903/16.897 ms` against the frozen `16.670 ms` limit
  (`9.538 ms` GPU p95). The FinalColor path executes only in the later Shipping Water
  process and cannot explain this miss. The runner restored the prior Candidate with
  Shipping executable hash `c38f76a4ae74986d1709b1efb120465776ae8432e4bd2819d9f652159ede2fb2`,
  removed all owner scratch, and published no composite. Do not random-retry, weaken the
  gate, or treat the prior Candidate as evidence for the unexecuted FinalColor path.
- The rejected run's exact Development log proves its in-process gate observed
  `r.VSync=0` and `t.MaxFPS=0`; therefore the ambiguous launcher argument did not establish
  a VSync root cause. Installed UE 5.8 source nevertheless proves `-VSync=0` is ignored by
  boolean `FParse::Param`, while `-novsync` is the documented/implemented flag. All
  packaged performance owners now use `-novsync`.
- The Development gate now establishes and fail-closed authenticates the actual uncapped
  envelope before sampling: VSync/max FPS, smoothing, fixed frame rate/time step,
  benchmarking, dynamic-resolution mode/status, native resolution, High quality, D3D12,
  and the physical adapter. Focused RED/GREEN Pester evidence passed `2/2`; the launcher
  incremental build passed; and
  `ProjectWorld.PlayableTour.PerformanceEnvelope.Uncapped` passed `1/1`. No Kazan content,
  Water, Landscape, runtime profile, or `16.67 ms` threshold changed.
- The authenticated transaction
  `a05251d984a744ba94d1e08a56448c2a` completed the launcher Development
  build/cook/package and all correctness, real-input, collision, and streaming gates, but
  rejected before Shipping at Frame p95 `17.086 ms` and Render p95 `17.049 ms`. Game p95
  was `8.088 ms`, GPU p95 was `9.478 ms`, streaming failures were zero, and the receipt
  authenticated the uncapped pacing envelope. The runner restored the prior Candidate,
  published no composite, and did not execute the Shipping FinalColor Water proof.
- Two non-publishing runs of the exact same staged Development executable
  (`a3942e462e7bfa54635f0f2752fef3689b4189e92f04b1e4f33114cd4dd6a576`)
  produced Frame/Render/GPU p95 values of `13.248/13.259/9.290 ms` and
  `16.754/16.703/9.456 ms`. This proves the one-run performance result is not repeatable;
  the passing diagnostic is not acceptance evidence and the rejected diagnostic is not
  permission to optimize Kazan content.
- The bounded identical-byte CSV comparison localizes the variance to render submission
  and host scheduling. The rejected diagnostic increased
  `DrawSceneCommand_StartDelay` p95 from `9.059` to `12.389 ms` and render-thread
  visibility wait p95 from `4.458` to `7.401 ms`, while draw-call p95 remained
  `196` versus `197`, primitive p95 remained `330571` versus `332467`, and GPU p95 moved
  by only `0.165 ms`. No product-content, World-profile, Water, Landscape, material, or
  `16.67 ms` threshold change is authorized by this evidence. This established the need
  for the bounded evidence-owner correction below; Track V remains blocked behind the
  final accepted Shipping Candidate.
- Reviewer follow-up correctly identified the remaining evidence-owner defect: missing
  JSON properties could cast to zero/false, and one fixed three-execution population is
  preferable to a single noisy run. The literal proposal to pool the existing native UE
  CSV was corrected: retained runs prove that file contains six extra numeric rows and
  does not exactly reproduce the C++ receipt percentile. `ProjectWorld` now emits a
  separate exact collector-sample projection while retaining native CSV diagnostics.
- Every shared envelope consumer now requires all eleven properties before value checks.
  Playable-tour Development builds once, executes exactly three predetermined child runs,
  accepts only individual Frame-p95-only rejections for aggregation, and fails immediately
  on every crash, identity, product, input, collision, interaction, streaming, envelope,
  sample, or source failure. The runner pools all exact raw frames with the same C++
  nearest-rank algorithm; no child p95 average/median, best-run selection, slow-frame
  removal, retry count, scheduler tuning, or product-content change exists.
- Focused RED was observed before the shared helper existed. GREEN evidence is Pester
  `12/12`, launcher incremental build PASS, and
  `ProjectWorld.PlayableTour.PerformanceSamples.ExactProjection` `1/1` PASS. The original
  performance gate source was split only at its configuration SRP seam and remains below
  the repository's 1000-line guardrail. The next expensive action is exactly one coherent
  release transaction under this frozen harness.
- The first coherent transaction built and archived Development successfully, then its
  first child exposed a harness-only package-identity defect. The product log proves
  Orchestrator created its normal runtime state below
  `Windows/Alis/LocalAppData`; a focused replacement then proved UE also writes normal
  `GameUserSettings.ini` state below `Windows/Alis/Saved`. After the same symptom
  repeated, a per-file before/after forensic diff against the already-staged package
  identified the complete five-file delta: two Orchestrator state files, two UE game
  state files, and `Windows/Engine/Saved/Config/Windows/Manifest.ini`; no payload file
  changed. Package evidence now excludes only those three exact runtime-state roots,
  records the digest scope, and still rejects executable/pak/packaged-config/content
  changes. The focused replacement test is included in the `12/12` Pester pass.
  No World content, runtime profile, material, or performance budget changed. One
  replacement coherent transaction is now authorized by this concrete harness fix.
- The next complete Development population proved the revised statistical contract:
  all three real product-route children accepted with Frame p95
  `13.905/13.118/13.915 ms`, GPU p95 `9.508/9.185/9.538 ms`, sample counts
  `15,158/16,189/15,142`, zero streaming failures, and pooled Frame/GPU p95
  `13.643/9.461 ms` across `46,489` exact collector frames. A PowerShell binding error
  then stopped before Shipping because the accepted aggregate's intentionally empty
  `acceptance_reason` was passed as the assertion message. The assertion now supplies a
  fixed non-empty diagnostic prefix; no evidence, threshold, or product behavior changed.
- The subsequent source-bound population again completed all three real product routes.
  Child Frame p95 was `13.544/16.137/17.228 ms`, GPU p95 was
  `9.219/9.429/9.746 ms`, every child had zero streaming failures, and the third child's
  only rejection was `performance_hard_gate_failed`. UE logged a requested process status
  10 but Windows reported normal exit 0, which exposed duplicated child-outcome logic that
  incorrectly refused the allowed slow child before aggregation. One shared classifier
  now retains authenticated Frame-only misses with observed normal exit 0 or 10, records
  the actual code, and still rejects abnormal exits or any non-performance error. Offline
  replay of the exact `42,511` retained frames passes pooled Frame/GPU p95 at
  `15.862/9.549 ms`; focused Pester is `12/12`. Shipping was not started by that failed
  operation, and its prior Candidate was restored.

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

F1 through F7 have owner evidence, machine-side F6 passed, owner scratch is clean,
and the operator accepted the same-character manual Kazan PreviewFlight walkthrough
in the exact final Shipping Candidate. The generated operator-acceptance receipt owns
the exact product identity. Historical combined and grounded-survival receipts remain
diagnostic provenance rather than substitutes for the final product decision.

By D7, Track V execution is not a stabilization completion requirement. It was not
executed and is transferred to the dedicated future cinematic workflow todo. That
future owner must preserve ProjectCinematic's exact Candidate/source binding and may
select the frozen Candidate only from its accepted source revision or deliberately bind
a later accepted Candidate. No further Kazan stabilization work remains.

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
- 2026-08-29 - R2 returned one bounded pre-operator PATCH: preserve exact HEAD in the
  final Track V receipt, prove or narrow flight lifecycle restoration, reconcile stale
  F5/F6 status, and freeze correct future `building:part` no-double-volume semantics.
- 2026-08-29 - The four operator-rejected Candidate defects were closed through their
  existing owners: pause menu, Spasskaya overview spawn, double-sided Building
  collision, and visible blue Water. Focused automation, normal World realization,
  immediate idempotence, live MCP/camera inspection, and read-only authority audit are
  green. The final source-bound Development/Shipping composite is the remaining
  machine authority and must be produced after this last tracked reconciliation.
- 2026-09-02 - Final machine operation and Water/product evidence passed. The operator
  passed the exact Shipping Candidate; the owner-local acceptance receipt was generated
  and authenticated against the frozen Candidate/composite identity. D7 transferred
  Track V without execution and closed this stabilization task.
