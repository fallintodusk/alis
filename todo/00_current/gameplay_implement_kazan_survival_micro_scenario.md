# Implement the Kazan survival micro-scenario

**Status:** TECHNICAL MECHANICS ACCEPTED - GROUNDED PRODUCT WALKTHROUGH PENDING
**Scope:** Slice 5 single-player scenario orchestration, protected Kazan anchors,
existing inventory/vitals consumption, and packaged acceptance
**Stable documentation owner:**
`Plugins/Gameplay/ProjectSinglePlay/docs/scenario_orchestration.md`, with spatial
rules remaining in `Plugins/World/ProjectWorld/docs/territory_generation.md`

## Goal

Make the normal Kazan menu experience answer "what is ALIS?" within minutes:
the real grounded `ADefinitionCharacter` must read an urgent survival state,
find and interact with a small supply cache, make one visible carrying choice,
use existing consumables to improve a real vital, reach a nearby shelter, and
receive an unambiguous success or failure followed by a clean restart.

All technical acceptance is agentic. The operator sees only the technically
accepted package in `Saved/PackageRelease/KazanSurvival/Candidate` and performs
the final feel/clarity walkthrough. Automation does not promote Candidate to a
final product decision.

## Non-goals

- No combat, dialogue, crafting, multiplayer, Mind/quest integration, AI, or
  new map.
- No new inventory, vitals, interaction, loading, save, or UI framework.
- No Kazan branch inside generic gameplay mechanics.
- No scenario logic in ProjectWorld, ProjectWorldData, a Level Blueprint, or
  generated geography.
- No second menu/loading/GameMode route and no direct map opening as acceptance.
- No redesign of global survival balance beyond the smallest qualified item or
  container data needed to make one honest carrying choice.
- No concurrent ProjectMaterial, environment, road, water, vegetation, or
  building implementation.
- No operator-dependent test step before the final walkthrough.

## Verified evidence

### Verified facts

1. The product route is already accepted through the real menu, ProjectLoading,
   `/Script/ProjectSinglePlay.SinglePlayerGameMode`, and the possessed
   `/Script/ProjectCharacter.DefinitionCharacter`. The accepted operation is
   `kazan_playable_tour_422843d1f1c24a14aa530939d55771dc`; its composite is
   under `Saved/Validation/WorldRealization/playable-tour/422843d1f1c24a14aa530939d55771dc/`.
2. `UKazanTerritoryExperienceDescriptor::BuildLoadRequest` currently forces
   `Traversal=PreviewFlight`. That is useful for the accepted tour but is not a
   grounded survival experience. The automated menu route currently adds only
   `game` and `Mode`, so removing the descriptor default without one
   automation-owned request override would make the accepted flight proof
   impossible to rerun.
3. `ASinglePlayerGameMode` already owns URL policy parsing, feature
   initialization after possession, condition-depletion handling, and
   `ReloadCurrentExperience()`. The implementation file is 601 lines, so the
   scenario responsibility should be added in sibling policy/runner files
   rather than expanding the GameMode into another responsibility.
4. ProjectSinglePlay is documented as a thin, deterministic, world-agnostic
   orchestrator. Its runtime module depends on ProjectCore abstractions and has
   no hard dependency on ProjectInventory, ProjectVitals, ProjectGAS, or
   ProjectWorld.
5. `UProjectVitalsComponent::Start()` calls `ApplyConfig()` after the ASC is
   available. `vitals_config.json` therefore overrides constructor defaults and
   starts the current player at hydration `0.1 / 3.0`, calories `25 / 2500`,
   fatigue `70 / 100`, and condition `75 / 100`. The starting survival pressure
   already exists; the scenario does not need to invent or directly write it.
6. Vitals owns all `State.*` tag updates and threshold/debuff behavior. The
   generic character begins in Empty hydration/calorie bands, and consuming
   existing items applies real GAS `SetByCaller` magnitudes through inventory.
7. `IInventoryReadOnly` already exposes item/container snapshots,
   weight/volume, `ContainsItem`, and `OnInventoryViewChanged`. The concrete
   inventory component exposes `OnItemUsed`, but its own contract labels that
   event UI/audio/telemetry-only; gameplay scenario logic must not bind to the
   concrete ProjectInventory class.
8. ProjectCore has no read-only vitals snapshot interface. The existing
   `IVitalsEventsSource` projects damage and death only. Exact scenario proof of
   hydration recovery therefore needs one narrow read projection instead of a
   ProjectSinglePlay -> ProjectVitals/ProjectGAS dependency.
9. Naked inventory is two real 2x2 hand grids. Their weight/volume limits are
   currently zero (unlimited), so the current data cannot honestly prove that
   weight or volume forced a choice. The backpack definition selects the Back
   equip slot but has no owner-local `containerGrants`; its current 6x6 capacity
   comes from an explicitly fallback-only component grant.
10. Existing item data is sufficient to exercise the real use path, but is not
    uniformly presentation-ready for capacity tuning. `WaterBottleBig` adds
    `0.5` hydration, which produces exactly `0.6 / 3.0 = 20%` from the accepted
    start. Vitals exits Empty only above 20%, so that item cannot prove the
    promised one-use transition. The selected reusable `EmergencyWater`
    candidate adds `0.75` hydration in one consumed use and produces at least
    `0.85 / 3.0 = 28.33%` before normal drain.
11. Kazan already realizes three water pickup actors from
    `Data/GameplayPlacement/kazan_territory_v1.json`. They prove the generated
    gameplay layer and interaction, but they are generated geography data, not
    protected scenario hero actors.
12. The authored-overlay contract already protects coordinate/feature-anchored
    packages from generated-layer replacement. It owns spatial placement only;
    the stable World roadmap explicitly assigns objective state, item
    requirements, success/failure, and feature activation to ProjectSinglePlay
    or existing feature data.
13. ProjectSinglePlayClient already owns local input/UI glue and depends on
    ProjectUI. `UProjectToastSubsystem` is available, so a short objective/status
    presentation does not justify another HUD framework.
14. The accepted package pipeline preserves `Candidate` plus
    `PreviousCandidate` when a prior candidate exists, and the
    World/cinematic owner cleanup scripts already remove disposable operation
    scratch while preserving accepted evidence and one rollback generation.

### Inferences

1. A generic data-driven scenario policy plus two tiny protected authored packages is
   smaller and safer than a quest framework, a Kazan GameMode, or new feature
   plugin.
2. The strongest short release proof is not another free-roam tour. It is one
   grounded loop that visibly connects the real city, interaction, inventory
   capacity, item consumption, vitals, and a terminal outcome.
3. The existing three generated water pickups may remain as normal world
   objects, but the deterministic scenario cache and shelter must have separate
   protected identities so regeneration and automated acceptance can prove
   them exactly.
4. The current critical starting state is already urgent enough. Adding a
   scenario timer or direct attribute mutation would be redundant and less
   honest unless packaged observation disproves the premise.

### Assumptions / unverified areas

1. A protected authored sublevel can hold the selected existing pickup actors
   and Engine-native semantic anchor actors without requiring a new runtime
   dependency from ProjectWorldData. Implementation must prove this through
   asset-reference and cook census before accepting the topology.
2. Existing toast hosting remains visible during normal Kazan play. Verify in a
   packaged characterization before selecting it; if it is not available,
   project scenario progress through the smallest existing ProjectUI layer
   contract, not a new framework.
3. Cache contents, item dimensions, and pouch limits are frozen below. Only the
   two anchor coordinates/distances remain implementation-time evidence
   selections, bounded to a cache reachable in 30-60 seconds and a shelter
   another 60-90 seconds away. They are not permission to rebalance unrelated
   objects.

## Current architecture and source of truth

- Product outcome: [`todo/00_current/00_focus.md`](../../00_current/00_focus.md),
  Track B.
- Slice routing and owner split:
  [`world_generate_kazan_territory_roadmap.md`](../world/world_generate_kazan_territory_roadmap.md),
  Slice 5.
- Single-player owner:
  [`ProjectSinglePlay/README.md`](../../../Plugins/Gameplay/ProjectSinglePlay/README.md).
- Inventory behavior:
  [`ProjectInventory/docs/design_vision.md`](../../../Plugins/Features/ProjectInventory/docs/design_vision.md).
- World regeneration and authored anchors:
  [`territory_generation.md`](../../../Plugins/World/ProjectWorld/docs/territory_generation.md).
- Character behavior:
  [`ProjectCharacter/docs/design.md`](../../../Plugins/Gameplay/ProjectCharacter/docs/design.md).
- Loading route:
  [`ProjectLoading/README.md`](../../../Plugins/Systems/ProjectLoading/README.md).

Responsibility flow:

```text
UKazanTerritoryExperienceDescriptor
    selects Scenario=UrbanSurvivalProofV1 through the existing load request
        -> ProjectLoading keeps the existing map/GameMode route
        -> ProjectSinglePlay resolves generic scenario policy/data
        -> existing ProjectCore read contracts observe inventory/vitals
        -> ProjectSinglePlayClient presents short progress through ProjectUI

ProjectWorldData protected authored overlays
    own one cache package/anchor and one shelter package/anchor
    plus Kazan coordinates only
        -> no objective graph or state transitions

ProjectObject
    owns concrete reusable item dimensions, pouch grant, and cache composition
        -> no scenario identity or Kazan knowledge
```

## Problem and root cause

Kazan is technically playable and filmable, but the package still demonstrates
navigation rather than the ALIS survival identity. The mechanics are already
present; the missing capability is a small orchestration contract that makes
their relationship legible and terminal.

The gap cannot be closed by simply placing more pickups. The current default
route is flight, generated water placements are not protected scenario
authority, ProjectSinglePlay has no scenario selection/state runner, there is
no abstract read-only vitals projection, and current item/container data does
not yet create an honest weight/volume boundary.

## Decision

Implement one generic optional scenario policy named
`UrbanSurvivalProofV1` and select it from the existing Kazan experience. The
same Kazan descriptor stops forcing `Traversal=PreviewFlight`, so normal menu
play is grounded. The already-accepted `PreviewFlight` policy remains reusable
when an explicit tour operation requests it; it is not deleted or replaced.

The scenario behavior is frozen as:

```text
normal game cold start
    -> menu selects Kazan
    -> ProjectLoading
    -> grounded ADefinitionCharacter
    -> urgent existing vitals state is visible
    -> player reaches and interacts with protected supply cache
    -> player equips/uses/moves qualified items through real inventory UI/input
    -> one exact pouch weight limit forces one optional-item choice
    -> real consumable use raises hydration across at least one Vitals-owned
       state boundary
    -> player reaches protected shelter with EmergencyRation
    -> success

failure route:
    reach shelter without the required recovery/supply condition
    -> explicit failure
    -> clean restart through the existing experience loader
```

ProjectSinglePlay owns only generic scenario selection, deterministic step
state, and completion/failure orchestration. Put the loader and runner in
sibling source files. The strict schema and reusable
`UrbanSurvivalProofV1` profile live under `ProjectSinglePlay/Data/Scenarios/`,
which the existing Build.cs stages as UFS. The Kazan descriptor selects only
that stable profile ID. The profile contains no Kazan identity or coordinates
and may use semantic step kinds only:

- reach a named authored anchor;
- observe one of the profile's explicit required item IDs through
  `IInventoryReadOnly`;
- observe a Vitals-owned state or normalized value through a narrow new
  ProjectCore `IVitalsReadOnly` contract;
- complete or fail and request the existing clean restart.

The profile must not contain coordinates, material/mesh paths, direct ASC
access, direct inventory mutations, Kazan branches, or generic graph language
beyond the step kinds required by this one loop.

ProjectWorldData owns two small protected authored packages: one cache package
and one shelter package. Each has one overlay record and one canonical anchor,
matching the frozen v1 overlay schema and Level Instance realization. Use
existing object definitions for visible cache contents. Prefer Engine-native
anchor actors with stable owner-scoped semantic tags if their package/cook
evidence is clean; add no gameplay class merely to hold a location.

Before authoring the cache, add only the reusable ProjectObject definitions
used by this proof. The exact V1 layout is frozen:

```text
EmergencyPouch (equipped to Back)
    Item.Container.Backpack: 2x4, 1.0 kg, 3.0 L, 8 cells
EmergencyWater: 2x2, 0.8 kg, 0.75 L, hydration +0.75, consumed
EmergencyRation: 2x2, 0.6 kg, 1.0 L, exact shelter requirement
EmergencyMedkit: 2x2, 0.6 kg, 1.5 L
CompactPryBar: 2x2, 0.8 kg, 0.8 L
```

The real-input route equips `EmergencyPouch`, puts `EmergencyWater` in the
left hand and `EmergencyRation` in the right hand, moves `EmergencyMedkit` into
the pouch, then attempts `CompactPryBar` in the other free pouch cells. The
move must be rejected as `TargetWeightExceeded` because
`0.6 + 0.8 = 1.4 > 1.0 kg`. Both `2x2` hand grids are fully occupied at that
instant, so unlimited hand weight and aggregate capacity cannot bypass the
proof. The player chooses medkit or pry bar. V1 success checks the exact
`ObjectDefinition:EmergencyRation`, not a new supply taxonomy.

All five carried definitions are general concrete resources with no Kazan or
scenario identity and may reuse existing qualified meshes. One reusable
`EmergencySupplyCache` LootContainer definition reuses a qualified duffle mesh
and seeds exactly one of each item above, with no random loot profile. If this
exact owner-local layout does not produce the named rejection, stop instead of
changing global hand/component balance or encoding a scenario-only capacity
rule.

Presentation uses three short messages through existing
ProjectSinglePlayClient/ProjectUI plumbing:

```text
SEVERE DEHYDRATION - Search the emergency bag. Reach the shelter with one food ration.
HYDRATION STABILIZED - Take one food ration to the shelter.
SHELTER SECURED - Hydrated. One ration delivered.
```

The failure path shows `SHELTER FAILED - Water and one ration are still
required. Restarting...` for 2-3 seconds before loader-owned restart. Runtime
scenario logic exposes neutral state; it never depends on ProjectUI. The target
play time is 2-4 minutes, with no timer, minimap, voiceover, or long tutorial.

### Premise / KISS gate

Existing owners already provide loading, player lifecycle, interaction,
inventory, consumable GAS effects, vitals pressure/state, UI layers, spatial
anchors, restart, packaging, and real-input automation. This change adds:

- one optional URL policy and one bounded scenario profile;
- one small ProjectSinglePlay runner lifecycle;
- one read-only Vitals projection in ProjectCore/ProjectVitals;
- two protected authored packages with one semantic location each;
- owner-local qualification of only the items/container used by the proof;
- one packaged scenario driver/receipt composed from existing gate machinery.

It does not add a service, process, port, protocol, quest framework, second
GameMode, second loading route, or new UI system. The knowingly omitted
capability is arbitrary branching/multi-scenario quest authoring.

If implementation requires a general quest graph, direct feature dependency,
new World generator/layer, Level Blueprint logic, or another loading route,
stop and return to research.

### Alternatives considered

1. **Keep PreviewFlight as the menu default and run the scenario in flight.**
   Rejected: it weakens collision, urgency, and survival feel and is not the
   product behavior Slice 5 must prove.
2. **Add a second Kazan scenario experience.** Rejected: it duplicates product
   routing and lets the accepted menu path remain a tour instead of the game.
3. **Put Kazan objective logic in ProjectWorld/ProjectWorldData or a Level
   Blueprint.** Rejected: it couples regenerated geography to gameplay state
   and violates the frozen Slice 5 owner split.
4. **Use ProjectMind/quest tags.** Rejected: Mind is explicitly out of release
   scope and would add a second owner for a one-loop proof.
5. **Bind ProjectSinglePlay to `UProjectInventoryComponent`,
   `UProjectVitalsComponent`, or GAS attributes.** Rejected: existing DIP
   boundaries prove a narrow ProjectCore projection is the scalable seam.
6. **Use only the three generated water pickups.** Rejected as final scenario
   authority: they are useful ambient objects but do not provide a protected,
   deterministic cache/shelter transaction or a carrying choice.
7. **Hard-code capacity or starting vitals in the scenario.** Rejected: feature
   owners already own those mechanics/data. Scenario orchestration observes
   consequences; it does not counterfeit them.

## Required invariants

1. The real menu -> descriptor -> ProjectLoading -> existing Kazan map and
   `SinglePlayerGameMode` route remains the only product route.
2. Normal Kazan scenario play is grounded. `PreviewFlight` remains an explicit,
   generic opt-in policy and all its focused regressions stay green. The
   existing automated menu path accepts one test-only
   `ProjectMenuPlayAutoTraversal=PreviewFlight` command-line value and adds it
   to the already-resolved `FLoadRequest`; it must not create another
   experience, loading route, direct-map route, or runtime movement switch.
3. Missing `Scenario`, including wrong-case keys, means no scenario and logs no
   warning. Unknown values fail closed to no scenario with one owner-scoped
   warning. Supported values are case-sensitive.
4. The ProjectSinglePlay runtime module contains no Kazan identity, coordinates,
   or hard ProjectWorld, ProjectInventory, ProjectVitals, ProjectGAS, or
   ProjectUI dependency. ProjectSinglePlayClient may keep its existing
   ProjectSinglePlay/ProjectUI presentation dependencies.
5. ProjectWorld and ProjectWorldData contain no objective state, item rules,
   success/failure, or restart logic.
6. ProjectVitals remains the only writer of Vitals `State.*` tags. The scenario
   uses a read-only projection and cannot set vitals or tags directly.
7. Inventory mutations occur only through existing interaction/UI/command
   authority. The scenario and test harness do not call `RequestAddItem`,
   `RequestUseItem`, or concrete inventory internals to manufacture success.
8. The carrying choice is the frozen `EmergencyMedkit` versus `CompactPryBar`
   pouch equation. Acceptance proves the named per-container weight rejection
   while both `2x2` hands are occupied. A message or aggregate unlimited
   capacity value is not evidence.
9. Scenario cache composition is deterministic. Random loot profiles are not
   terminal acceptance authority.
10. Cache and shelter are separate protected authored packages and overlay
    records, each with exactly one canonical anchor. Regeneration must preserve
    both package bytes and re-resolve both anchors within declared tolerances.
11. Generated terrain, road, water, vegetation, building, and existing gameplay
    layer bytes remain unchanged by scenario implementation.
12. Presentation uses existing ProjectUI ownership and stays optional to the
    server-safe scenario state machine.
13. Failure and success are terminal, machine-readable states. Restart uses
    `ILoadingService` through `ReloadCurrentExperience()`; no direct map reload.
14. Development performance remains physical RTX 4070, D3D12, High, 2560x1440,
    `512/1536`, Frame p95 `<= 16.67 ms`, and zero streaming failures.
15. Development and Shipping acceptance use the same source-state hash and
    authenticated package transaction. No source/config/data may change between
    the two runs.
16. All agent-created scratch stays under `tmp/gameplay/kazan_survival/` and is
    removed by its owner script. Accepted package/evidence retains `Candidate`
    plus at most one `PreviousCandidate` rollback generation.
17. An ordinary red test is an agent-owned debugging signal, not permission to
    abandon this design. Stop only for a material owner/scope/authority change.

## Implementation tasks

- [x] Characterize the current grounded Kazan route, starting vitals, inventory
  containers, toast hosting, existing pickup interaction, and clean restart in
  one packaged Development run before production edits.
- [x] Add focused red tests for Scenario option parsing, grounded Kazan
  selection, supported profile loading, unknown/absent behavior, deterministic
  step transitions, terminal restart, and forbidden direct dependencies.
- [x] Add the minimal `IVitalsReadOnly` snapshot/state contract to ProjectCore,
  implement it in ProjectVitals, and prove ProjectSinglePlay consumes only the
  abstraction. Do not broaden `IVitalsEventsSource` into a mixed contract.
- [x] Add a small Scenario policy/loader/runner in new ProjectSinglePlay sibling
  files. Put the strict schema and reusable profile under
  `ProjectSinglePlay/Data/Scenarios/`; keep them free of Kazan identity and
  coordinates.
- [x] Project neutral scenario progress to ProjectSinglePlayClient and the
  existing ProjectUI notification layer without adding runtime UI dependency.
- [x] Qualify the smallest scenario item set. Correct only demonstrably wrong
  dimensions used by the proof and add the exact reusable definitions and
  pouch `containerGrants` frozen above. Validate and regenerate through
  ProjectObject's existing idempotent definition pipeline.
- [x] Create separate deterministic cache and shelter authored packages and
  register each through its own canonical protected-overlay record/anchor.
  Re-run anchor resolution and byte-preservation evidence.
- [x] Change the existing Kazan descriptor to select
  `Scenario=UrbanSurvivalProofV1` and cease forcing PreviewFlight. Preserve an
  explicit opt-in Track P flight operation through the existing automated menu
  request seam and its focused regressions.
- [x] Extend the existing packaged-runtime harness with two real-input routes:
  one success and one failure/restart. Reuse its menu selection, interaction,
  performance, screenshot, source-freeze, and receipt machinery.
- [x] Run focused tests and the exact end-of-slice Development/Shipping package
  gate. Debug and rerun replacements autonomously within the gate budget.
- [x] Add the owner cleanup script/route for `tmp/gameplay/kazan_survival/`, run
  it, and retain accepted `Candidate` plus at most one `PreviousCandidate`.
- [x] Update stable ProjectSinglePlay scenario documentation, World anchor docs
  only if their existing contract needs clarification, testing routes, and the
  Slice 5/router status after technical acceptance.
- [x] Re-read the final affected code/data/docs and compare the final diff to
  this todo and all direct owners/consumers before requesting the operator
  walkthrough.

## Test-first and verification plan

### Red evidence

Write and observe each named failure before production changes:

| Red case | Production-shaped input | Wrong behavior captured today |
|---|---|---|
| `ProjectSinglePlay.Scenario.Policy` | absent, wrong-case, unknown, supported `Scenario` URL values | no scenario policy exists |
| `ProjectSinglePlay.Scenario.ProfileSchema` | real proposed JSON plus malformed/unknown step kinds | no strict scenario profile contract exists |
| `ProjectSinglePlay.Scenario.StateMachine` | mock inventory/vitals/anchor observations | no deterministic terminal state owner exists |
| `ProjectCore.Vitals.ReadOnlyProjection` | real/mocked Vitals provider | consumers cannot read normalized Vitals state without concrete dependency |
| `ProjectObject.Data.ScenarioCapacity` | exact selected item/container definitions | current accepted data does not force the declared weight/volume choice |
| `ProjectWorld.AuthoredOverlay.KazanSurvival` | real proposed cache/shelter anchors | no protected scenario package/anchors exist |
| `ProjectIntegrationTests.Kazan.SurvivalSuccess` | cold packaged menu route with real input | Kazan opens in PreviewFlight and has no terminal survival loop |
| `ProjectIntegrationTests.Kazan.SurvivalFailureRestart` | cold packaged menu route, shelter reached without requirements | no explicit failure or loader-owned clean restart exists |

Static red assertions also reject Kazan strings in ProjectSinglePlay, scenario
logic under ProjectWorld/Data, direct ProjectInventory/ProjectVitals/ProjectGAS
dependencies, Level Blueprint scenario logic, direct-map travel, synthetic pawn,
teleport/direct transform, and harness inventory/vitals mutation.

### Green evidence

1. Re-run every red test as the exact green set.
2. Run ProjectSinglePlay policy/registry tests and existing Track P traversal
   tests to prove explicit PreviewFlight remains valid while normal Kazan is
   grounded.
3. Run focused ProjectCore, ProjectVitals, ProjectInventory, ProjectObject data,
   interaction, loading, character-parity, and authored-overlay tests touched by
   the new seam.
4. Build through the launcher-engine project scripts only. Never call custom
   Build.bat arguments or source-build the Engine.
5. Run one cold packaged Development success operation using real menu
   selection and real input. Its composite receipt must bind: source state,
   executable/package hashes, map/GameMode/pawn, grounded movement, cache and
   shelter anchor identities, actual interactions, inventory before/after and
   capacity rejection, Vitals before/after and crossed state, terminal success,
   streaming, performance, logs, and screenshots from the same process.
6. Run one cold packaged Development failure operation that reaches shelter
   without the required state, proves terminal failure, loader-owned restart,
   fresh scenario state, and normal exit.
7. With source state still frozen, package and run Shipping through the same
   real product route. Prove cook/product correctness and both terminal paths;
   Development remains the instrumented performance authority.
8. Run a geography immutability census plus protected overlay byte and
   anchor-resolution comparison before/after regeneration.
9. Run the owner cleanup and verify no owner-created UE/game process, loose temp
   package, or stale operation directory remains.
10. Perform agentic screenshot review against the logs/receipts. Only after all
    technical gates pass is the final package handed to the operator.

## Documentation plan

- **Authoritative stable owner:**
  `Plugins/Gameplay/ProjectSinglePlay/docs/scenario_orchestration.md` for generic
  policy/profile/state/presentation boundaries; existing
  `Plugins/World/ProjectWorld/docs/territory_generation.md` remains authority for
  protected spatial anchors.
- **Router / TOC update:** `Plugins/Gameplay/ProjectSinglePlay/README.md` links
  the new scenario doc; World docs link only their stable owner if needed.
- **Content to add or change:** supported option semantics, generic step kinds,
  ownership/dependency boundaries, terminal/restart lifecycle, presentation
  projection, deterministic data/cook requirements, and test/receipt routes.
- **Duplication avoided:** inventory, vitals, loading, character, and World
  behavior remain in their current stable owners. Stable docs/code/tests/config
  never reference this todo.

## Rollout and rollback

Implementation is one clean candidate transaction:

1. Freeze source/config/data and all generated geography hashes.
2. Build the new ProjectSinglePlay policy/contracts, qualified ProjectObject
   data, and isolated protected overlay candidate.
3. Rotate an existing `Candidate` to `PreviousCandidate` before publishing a
   replacement. Absence of a prior candidate is valid.
4. Publish `Candidate` only after Development success/failure, performance,
   regeneration, and Shipping gates pass from the same frozen source state.
5. On rejection, restore the previous Kazan descriptor, ProjectSinglePlay/Core/
   Vitals/Object files, protected overlay registration/package, and package
   `Candidate` from `PreviousCandidate` when present. Generated geography is
   never a rollback surface.
6. The owner cleanup removes all unaccepted operation scratch and keeps the
   rejected diagnostic receipt only when it is needed to explain the decision.

## Completion criteria

`PASS` requires all of the following:

- Normal menu Kazan is grounded and runs the one accepted scenario with the
  real character and existing gameplay stack.
- A real capacity rule forces one visible decision; a real consumable crosses a
  Vitals-owned state boundary; success and failure/restart both work.
- ProjectSinglePlay remains world-agnostic and feature-agnostic through audited
  dependencies and source scans.
- Scenario locations survive full World regeneration byte-identically and
  resolve within their canonical tolerances; generated geography is unchanged.
- Focused tests, launcher Editor/game build, Development performance, Shipping
  product/cook, source freeze, screenshots/logs, and composite receipts are
  green and mutually authenticated.
- Owner temp is clean; `Candidate` and at most one `PreviousCandidate` are
  retained.
- Stable docs own every durable contract and contain no todo references.
- The mechanics-accepted Candidate is in the existing Saved release path. The
  remaining operator action is the grounded walkthrough of reachability,
  clarity, pacing, feel, and fun.

## Technical acceptance evidence - 2026-08-27

- Accepted operation:
  `kazan_survival_2d3eb44a5fb1412487b89d0360c52527`.
- Composite receipt:
  `Saved/Validation/Gameplay/KazanSurvival/2d3eb44a5fb1412487b89d0360c52527/composite.json`.
- Published candidate: `Saved/PackageRelease/KazanSurvival/Candidate`, tree
  SHA-256 `f53b99727f737a03a0da82b8b6a4b541cf03dfa036b0d2b530898a1f87f55208`.
  No prior candidate existed, so `PreviousCandidate` is absent.
- Development RTX 4070 High 2560x1440 D3D12 with the explicit
  `PreviewFlight` automation override: Frame p95 `15.663 ms`, GPU p95
  `10.810 ms`, zero streaming failures, and the same center cell was observed,
  unloaded, and reloaded through 22,904 simulated real-input events. This is a
  mechanics/streaming performance envelope, not grounded route-performance or
  on-foot usability proof.
- Development and Shipping both proved menu -> ProjectLoading -> real character,
  success, failure, loader-owned restart, inventory capacity rejection, item
  use, hydration recovery, carried ration, and normal exit.
- Generated-authority audit verified all 1,373 tracked artifacts byte-identical.
  Its separate repository-wide fingerprint check still reports older P0 and
  representative map scopes accepted by the prior `map:v1` producer. The
  territory scope used by this candidate is current; the older scope migration
  is not hidden or attributed to this scenario.
- Owner runtime scratch was removed. Close-out edits after the receipt are
  documentation-only; the receipt remains bound to the exact packaged source
  state recorded as `31fdce686932d99eb08cab113fe8a9c2ae744763348958138f0d5dd21035d84b`.

## Review record

- 2026-08-27: Initial investigation packet created from the accepted Slice 4,
  Track P, Track V, presentation research, architecture audit, and release-value
  audit. No scenario implementation was started during investigation.
- 2026-08-27: Independent architecture audit required two v1 authored-overlay
  packages, a ProjectSinglePlay-owned reusable profile, and a named
  per-container rejection while both hands are occupied. Corrected.
- 2026-08-27: Independent release-value audit selected this loop over further
  material work, then required the Track P flight override seam, valid one-use
  hydration arithmetic, and an exact non-bypassable capacity equation.
  Corrected; R1 PASS.
- 2026-08-27: The implementation reviewer correctly required a frozen source
  transaction, candidate rotation, and exact center-cell unload/reload proof.
  All were implemented. Its firewall concern applied to an earlier unsafe
  draft, not the final separately requested ALIS-owned Domain/Private setup;
  the package gate itself uses `-NoMessaging` and has no firewall dependency.
- 2026-08-27: Architecture and release-value audits found no new subsystem was
  justified. The bounded scenario composition remained the highest-impact KISS
  release step and passed the full unattended package transaction above.
- 2026-08-27: Post-integration review confirmed that the default scenario was an
  explicit product selection, not a silent Track P regression. It also found
  that the automated performance route used PreviewFlight. Technical mechanics
  remain accepted; grounded reachability, clarity, pacing, and feel remain open
  for the operator walkthrough. Cross-concern re-accreditation is routed through
  `../02_backlog/world/world_stabilize_kazan_combined_release_candidate.md`.
- Material changes to the grounded/default route, scenario behavior, ownership,
  capacity premise, or terminal acceptance require renewed review before
  implementation.
