# Enable Kazan release capture and playable tour

**Status:** TRACK P TECHNICALLY ACCEPTED; TRACK V INTEGRATED CANDIDATE - FINAL RELEASE TRUTH REBIND PENDING
**R1:** PASS - 2026-08-26
**Owners:** Track P - Kazan descriptor selection, ProjectSinglePlay policy,
ProjectCharacter behavior; Track V - ProjectCinematic authored capture and render
**Campaign:** [Kazan presentation research](world_plan_kazan_presentation_campaign.md)

## Operator decision

The next World release requires both outcomes:

1. **Playable proof:** a person enters Kazan through the normal packaged-product
   route and explores it with the real ALIS player and gameplay systems.
2. **Automated footage:** an agent or operator runs one stable invocation to render
   repeatable, clean Kazan video from shots authored once in Sequencer. Automation
   does not synthesize camera paths or replace gameplay performance for recorded takes.

Neither outcome substitutes for the other. Keep them as separate tracks with
separate owners, gates, and rollback.

Research is complete. Track P is technically accepted. Track V is an integrated
candidate whose final release/package truth binding and operator walkthrough remain.
Track V evidence remains in the existing
[cinematic capture pipeline](../../00_current/cinematic_capture_pipeline.md); this
umbrella packet remains the release-gate authority and does not duplicate that task.

## Technical acceptance evidence - 2026-08-27

- Track P composite: `Saved/Validation/WorldRealization/playable-tour/
  422843d1f1c24a14aa530939d55771dc/composite.json`.
- Track P runnable package: `Saved/PackageRelease/KazanPlayableTour/Current`, with
  exact `Previous` retained.
- Track V receipt: `Saved/CinematicRelease/Kazan/Current/receipt.json`.
- Track V master: `Saved/CinematicRelease/Kazan/Current/KazanRelease_v1.mov`, with
  exact `Previous` retained.
- Track V package census: `Saved/Validation/CinematicRelease/PackageAudit/
  422843d1f1c24a14aa530939d55771dc.json`.
- Automated receipts, logs, screenshots/contact sheet, and cleanup agree. Human review
  remains deliberately separate and cannot be inferred from the technical receipts.

## Premise / KISS gate

The need is a real packaged tour plus quick repeatable footage. Reuse the production
`ADefinitionCharacter` and native Character Movement for the tour; reuse authored
ProjectCinematic/Sequencer/MRQ shots for footage. This adds no pawn, movement plugin,
camera planner, spline generator, capture subsystem, service, or protocol. V1 knowingly
drops runtime walk/fly switching and automatic shot choreography because neither is
required for the two operator-visible outcomes.

## Stable routes

- Product outcomes: [Current product focus](../../00_current/00_focus.md)
- World realization contract:
  [Territory generation](../../../Plugins/World/ProjectWorld/docs/territory_generation.md)
- Gameplay character contract:
  [ProjectCharacter design](../../../Plugins/Gameplay/ProjectCharacter/docs/design.md)
- Single-player orchestration:
  [ProjectSinglePlay](../../../Plugins/Gameplay/ProjectSinglePlay/README.md)
- Existing visual evidence tooling:
  [VisualVerification](../../../tools/World/VisualVerification/README.md)
- Existing cinematic implementation task:
  [Cinematic capture pipeline](../../00_current/cinematic_capture_pipeline.md)
- Completed baseline proof:
  [Slice 4 runtime acceptance](../../01_done/world/world_validate_kazan_territory_slice_4_runtime.md)

The campaign router and current research orchestrator record this umbrella concern as
two owner-scoped outcomes: packaged tour and release capture. This routing does not
create a new plugin or duplicate the existing cinematic implementation task:

```text
Playable tour
    -> Kazan descriptor selection
    -> ProjectSinglePlay policy
    -> ProjectCharacter behavior

Release capture
    -> ProjectCinematic authored capture and render
```

Both entries may link this umbrella packet. VisualVerification remains evidence
infrastructure, not a Track V implementation owner. They remain two outcomes inside
one of the campaign's seven research concerns; routing clarity does not create an
eighth concern.

## Verified local baseline

### Product route and pawn

- Slice 4 already proves the packaged menu -> ProjectLoading -> Kazan ->
  ProjectSinglePlay route, possession, grounded movement, collision, gameplay
  interaction, World Partition unload/reload, and the selected `512/1536`
  runtime profile. The new playable tour must not replace those gates.
- Production spawning resolves `ObjectDefinition:Hero` through ProjectObjectSpawn
  and produces `ADefinitionCharacter`. The reviewer's `AProjectCharacter` name is
  stale and must not be used as the implementation or acceptance type.
- `ADefinitionCharacter` owns the first-person camera, Enhanced Input, Character
  Movement, GAS, vitals, and ordinary player behavior. Current controls bind WASD,
  mouse, Space jump, Ctrl crouch, Shift sprint, and CapsLock walk.
- Current `Move()` deliberately uses controller yaw and strips pitch. There is no
  playable flight policy, vertical flight input, or flight-specific braking proof.

### Existing policy seam

- `FSinglePlayModeConfig` does not currently expose traversal policy. Duplicating
  the Beginner/Medium/Hardcore feature composition into a tour-only mode would
  add avoidable configuration drift.
- `UProjectExperienceDescriptorBase::BuildLoadRequest` is virtual, while the Kazan
  descriptor currently overrides only asset scanning. It can add a canonical
  `Traversal=PreviewFlight` option without another configuration system.
- [InitialExperienceLoader](../../../Plugins/Systems/ProjectLoading/Source/ProjectLoading/Private/Experience/InitialExperienceLoader.cpp)
  resets the request and calls the descriptor. The real product-menu path then
  [adds only `game` and `Mode`](../../../Plugins/Gameplay/ProjectMenuPlay/Source/ProjectMenuPlay/Private/MenuPlayPlayerController.cpp),
  so current HEAD contains no later traversal writer on that path.
- [ProjectLoadingTravelURL](../../../Plugins/Systems/ProjectLoading/Source/ProjectLoading/Private/ProjectLoadingTravelURL.cpp)
  serializes all other `CustomOptions` and places `game` last. Travel therefore
  carries the descriptor option into destination `Options` unchanged.
- `FLoadRequest::Validate` does not validate `CustomOptions`, and
  [SinglePlayerGameMode](../../../Plugins/Gameplay/ProjectSinglePlay/Source/ProjectSinglePlay/Private/SinglePlayerGameMode.cpp)
  currently parses only `Mode` and `CharacterDefinition`. A closed traversal parser
  is the missing endpoint; the packet must not describe the full feature as implemented.
- Exact-key `TMap::Add` replaces an earlier value, while UE 5.8
  `UGameplayStatics::ParseOption` returns the first exact-case URL pair. The focused
  product-route test must prove exactly one canonical `Traversal` entry survives.
  ProjectSinglePlay accepts only `Default` or `PreviewFlight`. Absence resolves
  silently to `Default`; a wrong-case key naturally behaves as silent absence; an
  unknown value fails closed to `Default` with an owner-scoped warning. Do not add
  duplicate-URL policing, a generic option framework, or a world-specific branch
  without a supported producer that demonstrates the need.

### Character Movement evidence

- UE 5.8 Character Movement already supplies `MOVE_Flying` and collision-aware
  `PhysFlying` movement through swept movement, impact handling, stepping, and
  sliding. No new pawn, movement component, Mover migration, or noclip mode is
  justified for the first proof.
- The Slice 4 route gates do set the possessed real character to `MOVE_Flying`,
  but then reposition it with unswept `SetActorLocation`. They prove scripted
  streaming traversal with the real pawn, not player input, collision-preserving
  flight, acceleration/braking, or a usable speed.
- Character Movement defaults are not acceptance: `MaxFlySpeed` defaults to
  600 cm/s and flying braking defaults to zero. The implementation needs an
  explicit bounded policy and packaged evidence that the player neither drifts
  uncontrollably nor outruns the selected streaming profile.

### Existing footage owners

- VisualVerification already owns authenticated static world-evidence capture
  through its wrapper command and transient SceneCapture path. Its README contains
  a stale output-tree note saying screenshot capture is not implemented; research
  must trust the executable path and tests, then correct the stable doc when the
  implementation concern is opened.
- ProjectCinematic already owns CineCamera, LevelSequence, Take Recorder, Movie
  Render Queue, gameplay-interaction capture, and World Partition preparation.
  Extending that owner is preferable to a second World demo or capture subsystem.
- ProjectCinematic describes itself as Editor-only, yet its descriptor globally
  enables a Runtime module and the project enables the plugin. Its Build.cs comment
  relies on a plugin-level Editor type that the descriptor does not declare.
  Therefore zero packaged cost is not proved and must be closed before release.
- The active cinematic pipeline has unresolved interaction/highlight evidence and
  an over-1000-line subsystem decomposition obligation. This packet must reconcile
  with that work rather than duplicate it.
- Existing cinematic preset scripts refer to the stale
  `/Script/ProjectSinglePlay.AlisCinematicGameMode` path. The current generic class
  is `ACinematicGameMode` in ProjectCinematic. The implementation track must repair
  the scripts and keep reusable code free of new `Alis*` names.

## Review verdict

| Reviewer point | Verdict | Evidence-based correction |
| --- | --- | --- |
| Build the real gameplay player proof | Accept | It is required for the next World release. |
| Reframe this as the current Slice 4 decision | Reject | Slice 4 is already accepted and archived; these are new release-presentation requirements. |
| Replace automated capture with playable flight | Reject | The operator requires both deliverables; one cannot prove the other. |
| Demote automated footage to an optional decision | Reject | The operator explicitly requires quick automated footage for this World release; narrow its mechanics, not its priority. |
| Freeze Track P before Track V | Accept reviewer correction | Both are release gates, but the campaign/operator selects implementation timing after all research closes. |
| Detect arbitrary duplicate traversal URL options | Accept reviewer correction | The supported `TMap` route has one writer; prove that source shape and use the native exact-key parser. |
| Restrict PreviewFlight to Kazan in gameplay code | Reject | The descriptor selects policy; ProjectSinglePlay validates a reusable generic policy without a world whitelist. |
| Warn when Traversal is absent | Accept reviewer correction | Absence is the normal default-experience path and resolves silently to `Default`; only an unknown value warns. |
| Make PreviewFlight switchable during the session | Reject | V1 flight lasts for the selected experience; default experiences prove ordinary traversal independently. |
| Automate camera choreography | Reject | Sequencer owns shots authored once; automation repeats execution and render only. |
| Require cross-run pixel or file identity | Reject | Determinism is structural; hashes identify one run unless pixel determinism is separately proved. |
| Verify the complete CustomOptions seam | Accept | Current HEAD proves transport but lacks validation and the destination traversal parser; focused tests close that gap. |
| Add a packaged visual character gate | Accept | Mechanical flight proof cannot detect a catastrophic body, animation, or camera defect. |
| Use `AProjectCharacter` | Reject | The production pawn is `ADefinitionCharacter`. |
| Use native `MOVE_Flying` | Accept | It is the smallest existing collision-aware traversal primitive. |
| Slice 4 already implements playable flight | Reject | Its harness uses unswept direct relocation after changing movement mode. |
| Keep ordinary gameplay features active | Accept | The tour is the normal player plus a non-diegetic traversal permission. |
| Reuse Space/Ctrl for vertical movement | Accept with proof | Existing handlers must become mode-aware and held-input behavior needs tests. |
| Put experience policy in ProjectSinglePlay | Accept with correction | Carry a generic descriptor option through the existing load request; do not duplicate a whole mode. |
| Put flight mechanics in ProjectWorld/Kazan | Reject | World owns geography and realization, not player mechanics. |
| Keep collision enabled | Accept | Noclip would weaken the playable proof and add another semantic mode. |
| Defer cinematic work beyond the next release | Reject | Automated footage is independently required for this release. |
| Reuse ProjectCinematic/Sequencer/MRQ | Accept | It is the existing capture owner; first close its target and script defects. |
| Add MRG, rail, or crane now | Reject | Add only after a demonstrated shot or MRQ limitation requires it. |
| Exact fly speed is already known | Reject | Slice 4 proves the streaming profile, not a player-control speed. |

## Selected architecture

### Track P - packaged playable proof

```text
Kazan experience descriptor
    -> generic CustomOption: Traversal=PreviewFlight
    -> existing ProjectLoading request
    -> ProjectSinglePlay validates and applies experience policy
    -> existing possessed ADefinitionCharacter
    -> existing UCharacterMovementComponent MOVE_Flying
```

Ownership:

- Kazan descriptor selects the generic experience policy only.
- ProjectLoading transports the existing option unchanged.
- ProjectSinglePlay owns validation, defaulting, spawn/respawn application, and
  session-lifetime policy. `PreviewFlight` remains active for that experience;
  there is no v1 runtime toggle, walk/fly restoration state machine, or Kazan
  whitelist. Any future descriptor may select the same generic policy.
- ProjectCharacter owns generic input behavior while its Character Movement mode
  is flying: horizontal yaw-relative WASD, Space ascend, Ctrl descend, existing
  mouse look, and unchanged default jump/crouch behavior outside flight.
- ProjectWorld and ProjectWorldData remain untouched by flight mechanics.

Do not add a new pawn, component, plugin, duplicated difficulty mode, Kazan branch,
DebugCamera dependency, SpectatorPawn, noclip, 6-DOF controls, or Mover migration.

### Track V - automated release footage

```text
stable capture request
    -> ProjectCinematic editor capture owner
    -> durable, operator-authored LevelSequence + CineCamera shot authority
    -> existing World Partition preparation
    -> operator/player performance + Take Recorder for required gameplay takes
    -> MRQ output
    -> deterministic receipt + cleaned temporary workspace
```

Ownership:

- ProjectCinematic owns cinematic authoring, execution, rendering, and receipts.
- VisualVerification remains the technical still-image evidence owner; do not turn
  cinematic footage into automated world-correctness authority.
- A future agentic skill may wrap the stable command only after the command and
  result schema are proven. The skill orchestrates; it does not own shot or render
  behavior.
- Unreal MCP may open assets, invoke the stable capture entry point, and inspect
  results during development. It is optional control-plane automation, not a
  package dependency or acceptance authority.
- World plugins expose no cinematic API and gain no capture dependency.

The automated boundary ends at repeatable validation, execution, rendering, receipt,
and cleanup of authored shots. No autonomous camera planner, generated rail/spline,
agent-selected composition, or requirement to automate a human gameplay performance
is part of v1.

For this release, prefer a local Editor MRQ workflow. Select either Editor-only
modules or the smallest evidence-required editor/runtime split after proving what
separate-process MRQ loads. A remote or packaged render worker is deferred unless
the operator workflow proves that Editor capture cannot satisfy it. The invariant
is mandatory: packaged game receipts must exclude capture modules and dependencies.

## Expected change boundary

### Track P may change

- Kazan experience descriptor selection of a generic traversal option.
- ProjectSinglePlay option validation and spawn/respawn policy application.
- ProjectCharacter mode-aware movement input and bounded flight configuration.
- Focused unit/integration tests for those owners.

### Track V may change

- ProjectCinematic module/target boundary, current capture implementation, scripts,
  tests, and stable cinematic documentation.
- Existing VisualVerification documentation only where executable behavior proves
  the current text stale.
- A thin stable automation wrapper after the underlying cinematic command exists.

### Both tracks leave untouched

- Canonical Kazan inputs and all generated geography.
- Territory manifests, fingerprints, World Partition profile, collision authority,
  gameplay feature composition, and grounded Slice 4 acceptance.
- ProjectWorld runtime semantics except consumption by existing streaming behavior.
- ProjectObject definitions and material/presentation research.

Unexpected changes to any untouched owner stop the implementation and return it to
its owning contract before continuing.

## Acceptance design

### Track P focused proof

Add exact tests for:

1. Existing default experience descriptors emit no `Traversal` option; absence
   resolves to `Default` and retains ordinary walk/jump/crouch behavior.
2. The Kazan descriptor selects exactly one canonical `Traversal=PreviewFlight`
   entry; the real MenuPlay additions preserve it, ProjectLoading serializes it,
   and the destination parser resolves it without another supported writer.
3. Initial spawn and respawn apply the same selected policy to the possessed
   `ADefinitionCharacter` without changing feature composition.
4. In flight, held Space ascends and held Ctrl descends through Character Movement;
   WASD remains yaw-relative and collision remains enabled.
5. Unknown traversal values fail closed to `Default` with an owner-scoped warning.
   Missing or wrong-case keys behave as absence and resolve silently to `Default`.
   The source-shaped product-route test proves its sole canonical value. This is
   correctness provenance, not a security or anti-cheat boundary; arbitrary duplicate
   URL construction is outside v1.

Then run the packaged product proof on the physical RTX 4070:

```text
normal game boot
-> menu Kazan selection
-> ProjectLoading
-> ProjectSinglePlay
-> real ADefinitionCharacter possessed
-> inventory/vitals/interaction remain operational
-> ascend, horizontal traverse, collide/slide, descend
-> descend beside and interact with a gameplay object
-> centre -> edge -> centre unload/reload without holes or failures
-> selected 512/1536 profile remains authoritative
-> exit normally
```

Performance authority is an instrumented packaged Development run from the same
revision and World authority as the product candidate. Run it on the physical RTX
4070 with D3D12, High, 2560x1440, and the selected `512/1536` profile. The exact
speed/braking candidate that survives the focused control proof must execute the
centre, dense, perimeter, and return route. Its machine-readable receipt records
Frame p95/p99/max, Game/Render/GPU p95, peak process and GPU memory, readiness,
activation/residency evidence, and streaming failures. Acceptance requires Frame
p95 `<= 16.67 ms` and zero streaming failures; p99/max and the other domains remain
diagnostic unless an existing stable budget says otherwise.

Shipping remains a separate cook, menu-to-Kazan, gameplay, streaming-correctness,
and release-identity proof because the current Shipping target has no CSV profiler.
Do not infer Development performance from Shipping or weaken the physical-adapter
gate. Both packages must come from the same selected candidate revision/profile.

The packaged human gate also rejects a catastrophic flight presentation defect:
broken/frozen body state, persistent falling pose that makes the public proof unusable,
camera/body clipping, or loss of the expected first-person view. Do not add flight
animations pre-emptively; repair only an observed failure.

Use a small bounded speed/braking comparison only if the first candidate violates
control quality or streaming correctness. Measure the surviving packaged candidate;
do not infer a safe value from the Slice 4 scripted route.

The existing grounded Slice 4 tests remain regression authority and must stay green.
Playable flight does not replace terrain, road, building, or interaction collision
proof.

### Track V focused proof

Add exact tests for:

1. The stable capture request resolves the intended Kazan map, sequence, camera,
   preset, output root, and overwrite policy.
2. Missing map/sequence/camera/preset fails before capture with owner-scoped logs.
3. Repeating the same request preserves structural determinism and does not mutate
   the source map: same map, LevelSequence, camera bindings/cuts, render config and
   Game Overrides, frame range, shot trajectory, engine/project revision fields,
   and output contract.
   Cross-run pixel hashes or encoded-file identity are not required unless separately
   demonstrated. Per-run hashes remain useful receipt and integrity evidence.
4. A short smoke render contains the intended camera cut, non-empty frames, and a
   receipt recording the actual capture executable/configuration, engine/project
   revision, map, sequence, preset, resolved render configuration and Game Overrides,
   frame range, resolution, output files, duration, and bounded-resource evidence.
5. World Partition preparation covers the shot route without treating the render as
   a substitute for streaming acceptance.
6. `Alis` and `AlisClient` staged-file plus IoStore censuses cover loose binaries,
   descriptors, archive content, and discovered transitives. They reject payload
   proven capture-only from ProjectCinematic, Takes, MovieRenderPipeline,
   MoviePipelineMaskRenderPass, AppleProResMedia, or a discovered dependency. Module
   type or plugin name alone is not proof; classify the forbidden set from reference
   and staging evidence before changing descriptors or targets.
7. The final receipt binds to the accepted release transaction: map, active World
   authority/profile IDs and hashes, project revision, acceptance operation, Shipping
   executable SHA-256, IoStore receipt SHA-256, and the downloadable archive names and
   SHA-256 entries from signed `SHA256SUMS.txt`. It records the Editor/MRQ execution
   identity separately and never claims the Shipping executable performed the render.
   If Track V lands first, repeat this binding against the final accepted release.

The resolved MRQ configuration is machine-readable and compared with the accepted
packaged presentation. V1 may not silently force better scalability, LOD, or streaming
than that product. MRQ has no `16.67 ms` frame-time gate; Track P owns real-time truth.
Track V instead requires completion within its declared request timeout and disk
preflight, no OOM, complete/non-blank/non-stale frames, no World Partition holes, and
recorded peak memory plus scratch/output bytes. Select numeric resource ceilings from
focused evidence during implementation rather than inventing them in research.

The human visual gate compares the rendered clip with the packaged RTX 4070 game:
no materially misleading quality override, obvious streaming hole, UI leakage,
wrong camera, missing interaction beat, or stale world generation.

## Implementation dependencies and release gates

All seven concerns are closed. Track P, Landscape K1, and Track V are technically
accepted. Their operator visual walkthroughs remain; no further implementation action
is open inside this packet.

Before the World release, both must be accepted:

- **Track P:** the packaged playable proof passes its focused, grounded-regression,
  streaming, gameplay, performance, and human visual gates.
- **Track V:** ProjectCinematic shipping exclusion is proved, stale script/class
  routing is repaired, and one structurally repeatable authored Kazan sequence passes
  its render, receipt, cleanup, and human truth-comparison gates.

There is no hard Track P -> Track V implementation dependency. If Track V lands first,
rerun its final visual/truth comparison against the accepted packaged Track P build
before release. Add an agentic wrapper or MCP-assisted invocation only if it removes
repeated operator work from the proven workflow. Run focused tests after each change,
then one replacement packaged acceptance after fixes; broad gates are not the debug loop.

The existing current cinematic pipeline task should inherit Track V if it can cover
these actions cleanly. Do not open a duplicate cinematic implementation todo merely
because this research packet belongs to the World presentation campaign.

## Rollback and temporary-file ownership

- Track P owns one complete reversible delta: Kazan descriptor selection plus every
  candidate parser/applicator/test/config change in ProjectSinglePlay and every
  mode-aware input/test/config change in ProjectCharacter. Rollback restores all of
  those owner files, then re-proves silent `Default` resolution and ordinary grounded
  walk/jump/crouch. It does not regenerate World bytes or manifests.
- Track V owns one complete reversible delta across the ProjectCinematic descriptor,
  module/build boundary, source, scripts, sequence/preset assets, wrapper, receipt
  contract, and package-census expectations. Rollback restores the prior accepted
  invocation and staged footprint; it never edits generated Kazan geography.
- Every script or capture owner creates its scratch output under a named
  `tmp/cinematic/<component>/<run-id>/` or `tmp/world/<component>/<run-id>/`
  directory.
- Each owner removes intermediate frames, caches, logs, and failed-run debris that
  are no longer needed. Keep only the explicitly promoted clip and compact receipt.
- Cleanup must validate its exact project-local target and preserve at least the
  immediately previous accepted artifact so the operator has a `-1` comparison and
  rollback surface.

## Stable documentation propagation after implementation

Move durable knowledge only when proved:

- ProjectSinglePlay docs: generic traversal-policy ownership and default behavior.
- ProjectCharacter design: mode-aware input contract, not Kazan-specific policy.
- ProjectCinematic docs: true target boundary, stable command, receipt, cleanup, and
  shot-authority contract.
- VisualVerification README: executable still-capture capability if the current
  output-tree statement is confirmed stale.
- Testing docs: exact packaged playable and editor-capture routes only if they become
  standing gates.

Stable docs and code must never link back to this transient todo.

## Research close-out

R1 evaluated both tracks without choosing one as a replacement for the other:

- [x] Rechecked the `CustomOptions` transport chain, missing closed parser, and
  owner boundaries against current HEAD.
- [x] Bounded ProjectCinematic module selection by local MRQ evidence and packaged
  capture-only payload exclusion.
- [x] Routed existing cinematic commands/tests into Track V without duplication.
- [x] Rejected duplicate ProjectCharacter, ProjectSinglePlay, ProjectCinematic,
  VisualVerification, or World ownership.
- [x] Kept exact speeds, shots, presets, and automation technology evidence-selected.
- [x] Preserved both owner-scoped outcomes and the seven-concern campaign count.

Reviewer plus agent verification is complete. This packet remains in backlog until both
requirements are implemented and accepted, or until two promoted owner-specific todos
explicitly inherit every open action. Research completion alone never moves it to done.
