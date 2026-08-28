# Single-player scenario orchestration

## Purpose

ProjectSinglePlay may orchestrate a short, deterministic objective from existing
gameplay capabilities. It owns scenario selection, profile validation, phase
transitions, terminal state, and restart requests. It does not implement the
inventory, vitals, interaction, loading, character, UI, or World systems it
observes.

The first supported profile is `UrbanSurvivalProofV1`:

```text
severe dehydration
-> reach an emergency cache
-> encounter a real capacity rejection
-> use water through inventory
-> carry one ration
-> reach shelter
-> success or failure
```

## Ownership

| Owner | Responsibility |
|---|---|
| `ProjectSinglePlay` | Generic policy, strict profile loader, state machine, runner, terminal restart request |
| `ProjectSinglePlayClient` | Local toast projection and packaged real-input acceptance driver |
| `ProjectCore` | Read-only capability interfaces such as `IVitalsReadOnly` |
| Feature plugins | Inventory, Vitals, interaction, loading, and character behavior |
| `ProjectObject` | Reusable item and container definitions |
| `ProjectWorldData` | Protected authored cache/shelter content and concrete Kazan selection |
| `ProjectWorld` | Generic authored-overlay realization and World runtime evidence |

ProjectSinglePlay remains world-agnostic. No Kazan ID, coordinate, object
definition, or generated-world dependency belongs in its runtime scenario code.

## Selection contract

The travel option is case-sensitive:

```text
Scenario=UrbanSurvivalProofV1 -> select the supported profile
absent or wrong-case key      -> no scenario, silently
unknown value                 -> no scenario, owner-scoped warning
```

Profiles live under `Data/Scenarios/`, validate against
`Data/Schemas/single_play_scenario.schema.json`, and use a safe owner-local ID as
their filename. Runtime parsing requires the exact owner-relative `$schema`, schema
version `2`, and the exact admitted root-field set. The loader rejects malformed JSON,
unknown fields, unsupported schema identity/version, unsupported step order, unsafe
IDs, invalid thresholds, and invalid restart delays. Scenario IDs use the exact ASCII
grammar `[A-Za-z][A-Za-z0-9_]*`; this is enforced before any filesystem path is built.

## Runtime lifecycle

The runner observes existing interfaces and tagged authored actors. The supported
phase sequence is:

```text
Inactive
-> SearchCache
-> RecoverHydrationAndCarryItem
-> ReachShelter
-> Succeeded or Failed
```

The runner never inserts inventory items, changes hydration, moves the pawn, or
teleports the player. Those state changes come from the normal gameplay stack.
Failure requests a bounded delayed restart from `SinglePlayerGameMode`, which
re-enters the existing loading service and preserves the effective experience,
mode, scenario, traversal, and character selection. Character identity is written
to the travel URL as its primary asset name because `:` is not URL-safe there.

The client projects each actionable phase through the existing ProjectUI toast
service. Cache discovery has its own recovery instruction because this is the point
where the player must understand hands-only storage, pouch equipment, item transfer,
and water use. Actionable messages remain visible long enough to read; terminal
messages stay shorter. It adds no second HUD or scenario widget framework.

Explicit `PreviewFlight` sessions suppress survival instructions and show the actual
inspection controls instead: mouse look, WASD, Space to rise, and Left Ctrl to
descend. Normal Kazan selects `PreviewFlight` to present territory scale and does not
expose a runtime fly/walk toggle.

## World and data boundary

Scenario locations use separate protected authored overlay packages. Each overlay
has one canonical anchor record and is realized as a generated Level Instance
reference. Regeneration may replace generated geography but must preserve the
authored packages byte-for-byte and re-resolve their anchors within the accepted
tolerance. The standing spatial contract is owned by
`ProjectWorld/docs/territory_contract.md`.

The technical Kazan validation request selects the generic scenario ID. Generic
ProjectSinglePlay code knows only actor tags and profile values; it never owns
geographic identity.

## Acceptance

The release gate is `scripts/ue/gameplay/test/run_kazan_survival_proof.ps1`.
It owns one source-frozen transaction:

```text
Development package
-> real menu and ProjectLoading route
-> success scenario through real key and Slate input
-> input-driven playable tour and performance capture
-> failure and loader-owned restart
-> Shipping package from the same source state
-> Shipping success and failure routes
-> publish Candidate
```

Development owns instrumented performance. Shipping proves the real cook,
product route, interaction stack, terminal outcomes, and restart. Both stages
must bind to one frozen source-state hash. The composite receipt records the
separately owned evidence without replacing those subsystem receipts.

The harness must drive normal input and UI paths. It may not manufacture
success through direct transforms, direct inventory mutation, or direct Vitals
mutation. The explicit `PreviewFlight` test override remains a generic travel
policy. The survival runner explicitly selects both its scenario and that traversal;
neither option implies the other.

That automation override authenticates scenario mechanics, streaming, collision, and
the measured performance envelope. It does not turn the technical survival scenario
into the normal Kazan narrative or prove player-facing flight feel and scale clarity.
Those product qualities remain an explicit operator walkthrough gate before promotion.

Normal Kazan explicitly selects the packaged `PreviewFlight` capability and omits
`Scenario`. Ordinary experiences that omit `Traversal` still resolve silently to
grounded `Default`. The technical survival operation explicitly selects
`Scenario=UrbanSurvivalProofV1` and `Traversal=PreviewFlight` because its accepted
real-input driver measures flight-specific descent and slide behavior.

Accepted packages are published to `Saved/PackageRelease/KazanSurvival/Candidate`.
If a candidate already exists, the runner rotates it to `PreviousCandidate`.
The Candidate includes `Launch_Kazan_PreviewFlight.cmd` as a discoverable operator
entry point for the explicit inspection policy. The launcher still uses the real
menu selection and ProjectLoading route and is authenticated by the Candidate tree
digest.
Only an operator walkthrough promotes the product decision; test automation
does not rename a candidate to `Current`.

All disposable runner state is owner-scoped under
`tmp/gameplay/kazan_survival/`. The runner removes that state after acceptance
or rejection while retaining authenticated evidence under `Saved/Validation`.

## Acceptance invariants

- The normal Kazan route is menu -> ProjectLoading -> possessed
  `ADefinitionCharacter`; direct-map travel and synthetic pawns are invalid.
- Scenario and traversal are independent request policies. The technical survival
  operation explicitly selects both; normal Kazan selects only `PreviewFlight`.
- Inventory capacity, item use, Vitals recovery, interaction, collision, and
  restart are executed by their existing owners.
- Success requires real hydration recovery and the exact configured carried
  item. Failure and restart are separately proven.
- Development acceptance uses a physical GPU and records frame, game, render,
  GPU, memory, input, collision, and World Partition evidence from one process.
- Scenario overlays are protected authored content. Regeneration may replace
  generated geography but may not mutate their package bytes or anchor meaning.
