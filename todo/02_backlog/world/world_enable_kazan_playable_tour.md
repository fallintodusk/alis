# Enable Kazan packaged playable tour

**Status:** TECHNICALLY ACCEPTED - AWAITING OPERATOR WALKTHROUGH
**Track:** P only - packaged playable tour
**Research:** Complete; R2 PASS
**Implementation:** Technically accepted; operator walkthrough remains final
**Product target:** Real menu-to-Kazan flight using the real ALIS player stack

## Goal

Add one generic `Traversal=PreviewFlight` experience policy so the packaged Kazan
prototype can be explored quickly without replacing the normal game route, pawn,
gameplay systems, collision, or World Partition authority.

Both Track P and Track V remain mandatory for the next World release. This todo owns
Track P only. The authored automated footage work remains backlog Track V and does not
block starting Track P.

## Agentic execution contract

The implementing agent owns the complete technical loop without routine operator
interruption:

- implement the accepted Track P boundary;
- write and run focused tests, build, debug failures, and run authenticated replacements;
- create Development and Shipping packages through the existing launcher-engine pipeline;
- execute RTX 4070 performance, streaming, collision, gameplay, and grounded regressions;
- collect and jointly evaluate machine-readable receipts, logs, and agentic screenshots;
- select bounded flight speed/braking from evidence rather than asking the operator;
- clean owner-created temporary data while preserving the accepted result and one `-1`;
- place the final accepted package in the existing packaging pipeline's normal Saved
  output; do not invent a parallel delivery path.

Do not pause merely to ask permission to run a test, build, package, rerun a failed gate,
capture agentic evidence, or clean disposable owner-created files. A failing focused test
is an agent-owned debug loop.

The only routine operator action is the final walkthrough of the technically accepted
packaged game. Stop earlier only when evidence requires a material scope/authority change,
such as a new pawn or movement system, generated World mutation, a changed acceptance
contract, new durable authority, or another operation that repository rules reserve for
the operator. Report the exact evidence and smallest decision in that exceptional case.

## Stable routes

- [Seven-concern selection](../01_done/world/world_research_kazan_ue58_presentation.md)
- [Accepted Track P and Track V research](../02_backlog/world/world_enable_kazan_demo_capture.md)
- [Presentation campaign](../02_backlog/world/world_plan_kazan_presentation_campaign.md)
- [ProjectLoading route](../../Plugins/Systems/ProjectLoading/README.md)
- [ProjectSinglePlay owner](../../Plugins/Gameplay/ProjectSinglePlay/README.md)
- [ProjectCharacter design](../../Plugins/Gameplay/ProjectCharacter/docs/design.md)
- [Character parity gate](../../docs/testing/character_parity.md)
- [World Partition contract](../../Plugins/World/ProjectWorld/docs/world_partition.md)

## Premise / KISS gate

The production Kazan route already boots through the normal menu, ProjectLoading,
ProjectSinglePlay, and the possessed `ADefinitionCharacter`. Native Character Movement
already provides collision-aware `MOVE_Flying`. The missing product capability is a
small policy projection plus mode-aware input, not a new pawn, movement system, World
branch, spectator, or runtime fly/walk toggle.

If implementation needs any of those rejected systems, or mutates generated World
authority, stop and return this track to focused research.

## Verified baseline and gaps

- `UKazanTerritoryExperienceDescriptor` selects the accepted Kazan map but does not
  project a traversal option into its load request.
- The real UI path is `W_MainMenu -> MenuMainComposerSubsystem ->
  AMenuPlayPlayerController -> ILoadingService`; it already preserves descriptor custom
  options through the existing loading request and travel URL.
- `ASinglePlayerGameMode::InitGame` currently parses `Mode` and
  `CharacterDefinition`, but not `Traversal`.
- `ASinglePlayerGameMode` owns post-spawn feature composition and can apply a generic
  policy through Engine `ACharacter` / `UCharacterMovementComponent` APIs. Its module
  must not gain a dependency on ProjectCharacter.
- `ADefinitionCharacter` is the production pawn. Its current movement is planar,
  Space is jump, and Ctrl starts crouch. It has no held vertical flight input or
  explicit accepted flight tuning.
- The existing World product-route gate uses scripted movement and the performance
  gate sets actor locations directly. They prove route/streaming infrastructure, not
  playable flight control.
- The accepted grounded Slice 4 route remains regression authority.

## Reviewer audit and corrections

Accepted:

- Track P is the first implementation priority because it is release-required,
  independent of the material compiler, reversible, and does not migrate geography.
- Track P and Track V remain separate mandatory release gates with no artificial
  implementation dependency.
- Landscape is a strong later visual candidate, not part of this change.

Corrected:

- `todo/00_current/` is flat by repository contract. The reviewer proposed
  `todo/00_current/world/world_enable_kazan_playable_tour.md`; this flat file is the
  correct route.
- The claimed stale unconditional "Landscape first" material rule is not present in
  the current packet. It already says the selected material-dependent concern becomes
  the first consumer. No material todo correction is required.
- Low/Medium/High effort and RTX labels are planning estimates, not measured evidence.
- A normal red focused test is not a reason to abandon Track P. Debug and repair within
  the accepted boundary. Track V becomes the temporary implementation priority only
  after a confirmed architectural violation, complete Track P rollback, and renewed
  focused research.
- Existing World gates do not replace Character parity or real input proof because
  their traversal is scripted/direct-position movement.

Latest reviewer finding:

- Accept: final Development acceptance must be one unattended cold-started packaged
  process whose correctness, real-input traversal, streaming, performance, interaction,
  logs, and screenshots share one operation identity.
- Clarify: the existing `ProjectMenuPlayAutoExperience` hook is an accepted unattended
  menu selection because it enters through MenuMain Composer, the normal menu controller,
  descriptor resolution, and ProjectLoading. It is not a direct map open. Do not add an
  Automation Driver UI click merely to replace this proved seam.
- Clarify: one acceptance receipt may be a compact composite that authenticates and
  hashes focused correctness/performance/visual artifacts from the same process. Do not
  merge separate owner responsibilities into a mega-gate merely to produce one file.
- Clarify: ALIS currently proves Automation Driver only in an Editor integration-test
  module. Epic Automation Driver and Gauntlet are optional test-side candidates, not
  selected dependencies or product architecture. Prefer the existing packaged harness
  plus native PlayerInput/Enhanced Input injection unless evidence proves it insufficient.

## Ownership and expected boundary

May change:

- Kazan descriptor: select exactly one generic `Traversal=PreviewFlight` option.
- ProjectSinglePlay: validate/default the policy and apply it on spawn and respawn.
- ProjectCharacter: generic mode-aware input plus explicit bounded flying tuning.
- Existing test owners and external packaged-proof orchestration required by Track P.
- Stable owner docs only after behavior is proved.

Must remain untouched:

- canonical Kazan inputs, generated geography, packages, manifests, fingerprints,
  `active_set.json`, collision authority, and the selected `512/1536` profile;
- ProjectWorld runtime traversal semantics;
- ProjectObject definitions, materials, cinematic implementation, and Track V;
- inventory, vitals, interaction, feature composition, animation architecture, Mutable,
  SkeletalAssembly, Motion Matching, and first-person body ownership;
- `Alis.uproject` and source-engine state.

Unexpected propagation across this boundary is a hard stop.

## Selected contract

```text
Kazan descriptor
    -> exactly one CustomOption: Traversal=PreviewFlight
    -> existing ProjectLoading serialization
    -> ProjectSinglePlay parser and session policy
    -> Engine Character Movement mode application
    -> ProjectCharacter mode-aware player input
```

Rules:

- Absent `Traversal` -> `Default`, silently.
- Wrong-case key -> naturally absent -> `Default`, silently.
- Unknown value -> `Default` plus one owner-scoped warning.
- `PreviewFlight` lasts for the selected experience and is reapplied after respawn.
- No Kazan whitelist or Kazan branch exists in ProjectSinglePlay/ProjectCharacter.
- Default experiences retain ordinary walk, jump, and crouch.
- Flight uses swept Character Movement collision; no teleport is product acceptance.
- Exact speed and braking are one explicit bounded first candidate, selected and recorded
  during implementation. Compare a second candidate only if control or streaming proof
  rejects the first.

## Implementation plan

### P0 - census and reversible baseline

- Record the exact owner files, revision, configured launcher UE identity, selected
  World authority/profile, current package identity, and accepted `-1` receipts.
- Confirm current menu writer count and every supported `Traversal` reader/writer.
- Confirm test locality before adding dependencies: ProjectLoadingTests for descriptor
  projection and ProjectIntegrationTests for cross-owner policy/character behavior.
- Create all working output below `tmp/world/playable_tour/<run-id>/`.

### P1 - red focused contracts

Add failing exact tests first:

1. Extend `ProjectLoading.DescriptorResolution.Kazan.ProductRouteProjection` to prove
   exactly one canonical Kazan option survives the real descriptor/load route.
2. Add an exact ProjectSinglePlay policy test for silent absent/wrong-case default,
   warned unknown fallback, and accepted `PreviewFlight`.
3. Add an exact spawn/respawn test proving the same policy is applied without changing
   gameplay feature composition and without a ProjectSinglePlay -> ProjectCharacter
   module dependency.
4. Add an exact `ADefinitionCharacter` input test: held Space ascends, held Ctrl
   descends, WASD stays yaw-relative, collision stays enabled, and Default preserves
   walk/jump/crouch.
5. Add a harness architecture test/census proving the selected real-input traversal path
   contains no direct transform mutation such as `SetActorLocation`, `TeleportTo`, or a
   call straight to `AddMovementInput` as its acceptance authority.

Run only one exact test at a time through the repository iteration scripts. A failing
focused test is debugging evidence, not fallback authorization.

### P2 - descriptor projection

- Project the generic option from the Kazan experience descriptor through the existing
  `BuildLoadRequest` seam.
- Preserve the single existing menu/loading route; do not add duplicate URL machinery
  or a second Kazan action.
- Make the ProjectLoading projection test green.

### P3 - ProjectSinglePlay policy

- Add the smallest generic parser/value representation owned by ProjectSinglePlay.
- Resolve missing, wrong-case, unknown, and supported values exactly as contracted.
- Apply the selected movement mode after the normal player pawn initialization and on
  respawn through Engine types only.
- Keep gameplay feature composition byte/behavior equivalent.
- Make the policy and lifecycle tests green.

### P4 - ProjectCharacter flight behavior

- Add mode-aware horizontal and held vertical input to `ADefinitionCharacter`.
- Reuse the existing Space/Ctrl actions through mode-aware handlers. While flying,
  Space/Ctrl must not simultaneously invoke jump/crouch; in Default they retain the
  existing jump/crouch behavior.
- Configure one bounded flight speed/braking candidate explicitly; do not couple it to
  Kazan or introduce a general movement framework.
- Preserve existing camera, body, animation, grounded locomotion, jump, and crouch.
- Do not pre-emptively change `UpdateRotationPolicy`, GAS movement multipliers,
  sprint/walk semantics, or animation architecture. Change one only after packaged
  evidence identifies a concrete defect owned by that surface.
- Make the focused character tests green.

### P5 - focused build and regression proof

- Build through project scripts with the configured launcher engine. Do not invoke UBT
  with custom arguments, switch to a source engine, or invalidate engine caches.
- Run every new exact test individually.
- Run the established character parity capture gate after the runtime change, including
  idle, camera yaw, locomotion, clean-path isolation, and simple animation sanity.
- Re-run the grounded Slice 4 product-route/collision/interaction authority.
- Inspect logs and agentic screenshots together; neither green automation nor visuals
  alone close the track.
- Fix in-boundary failures and run their focused replacements autonomously. Escalate only
  after the accepted owner boundary is proven insufficient.

### P6 - packaged product acceptance

Freeze one unattended and reproducible Development acceptance run on the physical
RTX 4070:

```text
cold packaged boot
-> existing automated MenuMain Kazan selection
-> descriptor -> ProjectLoading -> ProjectSinglePlay
-> possessed ADefinitionCharacter
-> normal PlayerInput/Enhanced Input path:
     look input
     hold Space -> ascend
     WASD -> centre -> dense -> perimeter/edge -> centre
     collide and slide through Character Movement
     hold Ctrl -> descend
-> same-process grounded gameplay-object interaction regression
-> World Partition unload/reload
-> screenshot and log capture
-> normal exit
-> one composite machine-readable acceptance receipt
```

- D3D12, High, 2560x1440, selected `512/1536` profile.
- Extend the existing product-route/performance process orchestration. Do not add another
  automation framework while that harness can own the run.
- The menu auto-selection hook may remain because it uses the normal product controller
  and ProjectLoading route. Direct command-line map opening remains forbidden.
- After possession, every accepted displacement must originate from key/action input
  delivered through the normal PlayerInput/Enhanced Input processing path. Native
  `APlayerController::InputKey`, Enhanced Input injection, or a platform-layer driver are
  viable test-side mechanisms; select the smallest one that proves the mapped handlers.
- Direct `SetActorLocation`, teleport, direct harness calls to `AddMovementInput`, a
  synthetic pawn, or direct invocation of flight handlers is forbidden as Track P
  traversal authority. Existing transform-based gates may remain as separate grounded
  regression/diagnostic probes but cannot contribute accepted Track P displacement.
- Continuously sample input phase/key identity, movement mode, velocity, swept
  displacement, collision response, and World Partition state so the receipt proves held
  Space/WASD/Ctrl behavior rather than merely the final coordinates.
- Begin performance sampling after product-route readiness/warmup, inside the same
  process and operation identity. Measure the surviving movement candidate across centre,
  dense, perimeter, and return. Require Frame p95 `<= 16.67 ms` and zero streaming
  failures; record p99/max, Game/Render/GPU p95, process/GPU memory, readiness,
  activation, and residency.
- Freeze the initially loaded spatial cell IDs containing the center after warmup. The
  real-input route must prove at least one of those exact IDs is unloaded at/after the
  edge and the same ID is loaded again only after the final center return. Arbitrary
  unrelated unload/reload transitions do not satisfy Track P.
- Freeze HEAD, the complete tracked/untracked source-state digest, and runtime-profile
  digest before Development packaging. Recheck them after each package and run and
  before composite publication; any drift rejects the transaction.
- The composite acceptance receipt must bind operation ID, executable/package hashes,
  revision, map/GameMode/pawn/profile identities, input-driving mechanism, correctness
  and performance receipt hashes, CSV hash, log hash, screenshot hashes, interaction,
  collision, displacement, unload/reload, exit code, and cleanup result. All bound
  artifacts must come from the same cold-started process; cross-run splicing fails closed.
- Automation Driver is currently Editor test coverage in ALIS. Use it in this packaged
  gate only if staging proves a clean test-side Development boundary and the simpler
  native input route fails an invariant. Gauntlet is optional process orchestration and
  is justified only if the existing PowerShell/runtime harness cannot reliably launch,
  monitor, time out, and collect this one session.
- Separately package Shipping from the same revision/profile and prove normal menu route,
  possession, gameplay, cook/staging identity, streaming correctness, and normal exit.
- Development and Shipping must both return exit code `0`. Development must include UE's
  clean terminal log markers. Launcher-engine Shipping emits no log, so its terminal proof
  is exit code `0` plus the authenticated same-process product receipt; when a Shipping log
  exists, its clean terminal markers remain mandatory. A written acceptance receipt never
  overrides an abnormal process exit.
- Agentic visual acceptance rejects broken/frozen body state, persistent unusable
  falling pose, camera/body clipping, missing first-person view, or collision bypass.
- Physical RTX 3060-class qualification remains release-plan `UNQUALIFIED`; never infer,
  emulate, or extrapolate it.

After all automated, packaged, performance, log, receipt, and agentic visual gates pass,
present the final Saved package to the operator for the sole routine human walkthrough.
Do not call Track P finally accepted or archive this todo until that walkthrough passes.

### P7 - durable propagation and cleanup

After acceptance only:

- Update ProjectSinglePlay docs with generic traversal policy/default behavior.
- Update ProjectCharacter design with generic mode-aware input behavior.
- Update stable testing docs only if the packaged route becomes a standing command/gate.
- Remove superseded intermediate packages, logs, screenshots, and failed-run debris from
  this owner's project-local tmp tree. Keep the current accepted receipt/visual set and at
  least the immediately previous accepted `-1` set.
- Preserve the prior runnable package at `Saved/PackageRelease/KazanPlayableTour/Previous/`
  until the operator walkthrough accepts the new `Current/`; a technical pass must not
  delete the human rollback boundary.
- Move this todo to `todo/01_done/world/`. Stable code/docs must not link to it.

## Withdrawn technical acceptance - 2026-08-26

- Operation `9204a6ef38bc4fc9a148c9cc2b00b215` is retained as diagnostic evidence only.
- Composite: `Saved/Validation/WorldRealization/playable-tour/
  9204a6ef38bc4fc9a148c9cc2b00b215/composite.json`.
- Operator package: `Saved/PackageRelease/KazanPlayableTour/Current/`.
- Launcher engine route, physical RTX 4070, D3D12, High, 2560x1440, and the
  `512/1536` runtime profile are authenticated by the same operation.
- Development returned `777003` (`CrashReporterCrashed` in installed UE 5.8) after
  requesting status `0`; the old wrapper failed to reject this abnormal exit.
- Its arbitrary-cell unload/reload counts also do not prove center-cell identity.
- Performance: Frame p95 `16.4912 ms`, p99 `22.4405 ms`, Game p95 `4.7848 ms`,
  Render p95 `16.5427 ms`, GPU p95 `10.7925 ms`, streaming failures `0`.
- Shipping returned `0`, but the old wrapper did not guard source/profile immutability
  across Development and Shipping, so the combined transaction is not accepted.
- Focused descriptor, traversal-policy, spawn/respawn, character-input, harness,
  product-progress, and character-parity gates passed. Parity run:
  `20260826_165911_493`, `5/5` scenarios.
- Screenshots remain useful visual diagnostics, but the log and process result conflict
  with the receipt. Track P is reopened until one replacement transaction is green.
- Firewall setup is a separate operator-authorized machine setup concern. It is not a
  Track P dependency, receipt, rollback surface, or single-player product gate.
- Owner tmp is empty. Immediate `-1` evidence retained at
  `Saved/Validation/WorldRealization/playable-tour/
  e7ffe02d420342498799eb4b81e581d2/`; older failed/diagnostic debris removed.
- Remaining gates: replacement technical transaction, then operator flight/camera/body
  walkthrough of the accepted Saved package.

## Replacement diagnostics - 2026-08-26

- Operation `de9ebc2626674884974329ed61e2c083` correctly rejected instead of
  publishing a false composite. The wrapper restored the previous runnable package and
  removed the failed package.
- The strengthened residency proof passed: exact center cell
  `ea0f61d1-b655-41eb-c35e-98564f1213a5` unloaded only after the real-input edge leg
  and the same cell reloaded after the final center return.
- Frame p95 was `16.9497 ms`, above the `16.67 ms` gate, while the log emitted `23,636`
  identical `ABP_WorldBodyRetarget.UpdateRetargetProfile` null-controller warnings.
- The retarget graph now gates its complete get/modify/set path behind a successful
  controller cast. Canonical character parity run `20260826_202344_406` passed `5/5`
  with zero `UpdateRetargetProfile`, Blueprint `None` access, or `LogScript` warnings.
- Repackage and rerun the exact Track P transaction to measure the corrected candidate;
  do not infer the performance delta from the focused parity result.

## Technical acceptance - 2026-08-27

- Superseded acceptance operation `6429f73c23c646ef8605b65b2ab49618` proved the
  Track P behavior before the final capture-dependency boundary correction.
- Final accepted operation: `422843d1f1c24a14aa530939d55771dc`.
- Composite: `Saved/Validation/WorldRealization/playable-tour/
  422843d1f1c24a14aa530939d55771dc/composite.json`.
- Development and Shipping returned exit code `0` from one frozen source/profile state.
- Physical RTX 4070, D3D12, High, 2560x1440: Frame p95 `14.3400 ms`, Game p95
  `5.1855 ms`, Render p95 `14.3301 ms`, GPU p95 `10.4780 ms`, streaming failures `0`.
- Real input produced `26,321` events, reached all four waypoints, traversed
  `1,520,545.83 cm`, and proved ascent, descent, collision blocking, and collision slide.
- Exact center cell `ea0f61d1-b655-41eb-c35e-98564f1213a5` unloaded after the edge leg
  and reloaded only after the final center return.
- The repeated `777003` shutdown was caused by restoring scalability from the gate
  destructor after UE had closed the object subsystem. The one-shot packaged gate now
  leaves its process-local quality override in place until process exit; the same focused
  staged route and the final authenticated transaction both exit cleanly.
- Agentic inspection confirms Development and Shipping visuals agree. It also confirms
  the accepted prototype's largest presentation defect is the green striped Landscape
  debug material, which belongs to the already-researched ProjectMaterial/Landscape path.
- `Current/` contains the accepted Shipping package, `Previous/` retains the runnable
  rollback package, and owner runtime tmp is cleaned.

## Rollback

Track P owns one complete reversible delta across descriptor projection,
ProjectSinglePlay parser/application, ProjectCharacter input/tuning, focused tests,
proof orchestration, and any proved stable-doc updates.

Rollback restores that exact set, then proves:

- Kazan/default routes resolve silently to ordinary `Default` traversal;
- grounded walk/jump/crouch, possession, gameplay features, collision, interaction,
  streaming, and character parity are green;
- generated World bytes/manifests and Track V remain unchanged.

## Completion gates

- [x] Operator approved autonomous technical execution; only final packaged walkthrough
      remains the routine human gate - 2026-08-26.
- [x] Red focused contracts existed before behavior changes.
- [x] Descriptor, policy, spawn/respawn, and input exact tests pass.
- [x] ProjectSinglePlay has no ProjectCharacter dependency.
- [x] Character parity and grounded Slice 4 regression gates pass.
- [x] One unattended real-input Development packaged run produces the accepted composite
      receipt with no teleport/direct-movement traversal authority.
- [x] Development packaged physical-RTX-4070 correctness/performance gate passes.
- [x] Shipping normal product-route/cook/streaming proof passes.
- [x] Agentic screenshots, logs, receipts, and automated gates agree.
- [ ] Final operator walkthrough of the accepted Saved package passes.
- [x] Changed scope and direct owners/consumers are semantically re-read.
- [x] Durable docs are propagated, tmp debris is cleaned, and runnable `-1` remains.
