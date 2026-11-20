# Offline world time, geographic daylight and demo controls

**Status:** PATCH - -4.5 EV retained; iteration 05 implements and proves bounded native Scene-lighting recovery for the reproduced Manhattan seek transient. Settled-night visual approval, cooked negatives and D11 release performance remain open; retained Manhattan performance measurements remain REJECTED. Dynamic opt-ins stay disabled; production maps are not promoted.
**Scope:** World-local ProjectTime (clock and optional demo UI) and ProjectLight
(scene lighting/atmospheric rendering), integrated for 3.0.0. Weather is deferred.
**Stable owners:** [plugin boundaries](../../docs/architecture/plugin_rules.md),
[World](../../Plugins/World/ProjectWorld/docs/territory_generation.md),
[UI](../../Plugins/UI/ProjectUI/docs/README.md),
[Time](../../Plugins/World/ProjectTime/README.md) and
[Light](../../Plugins/World/ProjectLight/README.md).

## Contents

- [Goal](#goal)
- [Authority register](#authority-register)
- [Non-goals](#non-goals)
- [Verified evidence](#verified-evidence)
- [Current architecture and source of truth](#current-architecture-and-source-of-truth)
- [Problem and root cause](#problem-and-root-cause)
- [Decision](#decision)
- [Required invariants](#required-invariants)
- [Implementation tasks](#implementation-tasks)
- [Test-first and verification plan](#test-first-and-verification-plan)
- [Documentation plan](#documentation-plan)
- [Rollout and rollback](#rollout-and-rollback)
- [Completion criteria](#completion-criteria)
- [Review record](#review-record)

## Goal

At startup, show the real current time and corresponding daylight for the loaded
territory using the PC clock offline. Let players preview morning, evening and
other times through draggable demo controls, then return to PC-synchronized time.
Complete day/night integration in this slice, not a second clock-only milestone.

## Authority register

### Operator decisions

- **D1:** Default time is real time for the geographic zone, synchronized with
  the PC rather than an online source.
  - Effect: computer-sourced UTC plus territory civil timezone; no network clock.
  - Reason: not stated.
  - Source: operator request, 2026-11-18.
- **D2:** Investigate a separate ProjectTime owner, proposed near ProjectWorld,
  and ownership of draggable in-game time/speed controls for a 3.0.0 demo.
  - Effect: assess the proposed placement and UI boundary; the suggested folder
    and UI plugin are proposals to evaluate, not imposed architecture.
  - Reason: intuitive ownership and letting players see different times of day.
  - Source: operator request, 2026-11-18.
- **D3:** Use investigate-change and architect to prepare a new todo and thoughts.
  - Effect: this plan is the only repository edit; no implementation or adoption.
  - Reason: careful architectural evaluation before realization.
  - Source: operator request and invoked skills, 2026-11-18.
- **D4:** Keep ProjectTime scoped to World time; keep its optional controls in the
  same plugin rather than creating a generic Systems clock and separate UI plugin.
  - Effect: one plugin near ProjectWorld; internal UI separation is permitted.
  - Reason: current consumers are environmental; hypothetical reuse is insufficient.
  - Source: operator clarification and acceptance, 2026-11-18.
- **D5:** Keep the feature available in the game, with demo controls permitted by
  gameplay mode/experience and unavailable in normal single-player. Do not focus
  on removing it from final Shipping now.
  - Effect: default-deny runtime override policy, enforced below UI; no blanket
    Shipping exclusion. Demo and normal experiences may coexist in one build.
  - Reason: allow a demo map without making normal gameplay easily toggleable.
  - Source: operator clarification and acceptance, 2026-11-18.
- **D6:** Update this todo with the agreed simplifications and assess day/night
  ownership, time notifications and an efficient update cadence.
  - Effect: revise this plan only; minute-based ticking is a hypothesis to verify,
    not an accepted interval. No feature implementation is authorized here.
  - Reason: coherent ownership and efficient sky/daylight response.
  - Source: operator request, 2026-11-18.
- **D7:** Use ProjectLight for scene lighting and atmospheric appearance; keep
  environmental time and its optional UI in ProjectTime.
  - Effect: replaces A5's combined time/lighting owner. ProjectLight owns Sun/sky,
    cloud/fog rendering and gameplay exposure; World delegates lighting setup
    through its existing transaction. D4 still keeps Time UI inside ProjectTime.
  - Reason: a narrower lighting black box without exterior-object ownership.
  - Source: operator confirmation of the lighting boundary, 2026-11-18.
- **D8:** Future weather owns weather state and activates effects by publishing
  conditions to ProjectLight and the appropriate material/effect controllers.
  Weather implementation is not current scope.
  - Effect: record the future dependency only. Rendering/material/effect owners
    apply conditions; weather does not become another writer of their internals.
  - Reason: decoupled weather-state control.
  - Source: operator request to mark this boundary, 2026-11-18.
- **D9:** Implement the approved todo through implement-approved-change.
  - Supersedes: D3/D6's planning-only execution restriction; their design history
    stays intact. D1-D8 retain their design constraints.
  - Effect: local implementation and required verification are authorized;
    production promotion, Git staging/commits and release remain separate.
  - Reason: proceed from reviewed plan to concrete implementation.
  - Source: operator invocation, 2026-11-18.

- **D10:** Execute one additional bounded Manhattan verification campaign.
  - Effect: package the corrected Development-only capture setup; run Manhattan
    cooked UI/daylight proof and its three existing performance traversals.
    Retain Kazan evidence and restore generated World, Canonical selection and
    runtime inputs. No optional-controller redesign or default adoption.
  - Supersedes: the exhausted campaign budget for this one continuation only.
  - Reason: operator authorized the additional verification after clarification.
  - Source: operator "yes do please it now", 2026-11-19.

- **D11:** Keep performance acceptance for final release preparation; check actual
  day/night screenshots and live UI now, with a compact review folder per iteration.
  - Effect: do not repeat the contended performance campaign during visual work.
    Final release preparation retains matched-load, moving-light cost and both-city
    budget checks. Prior REJECTED results stay rejected; no waiver or default adoption.
    Use `tmp/release/3.0.0/time-light/iteration-<nn>/` for review images, with raw
    logs/receipts in its `evidence/` child.
  - Reason: separate user-visible qualification from final integrated performance.
  - Source: current operator request, 2026-11-19.

- **D12:** Execute the reviewer's bounded intermediate night comparison and select
  and qualify the best night-exposure setting. Quick evidence-backed improvements
  within the existing owners are allowed.
  - Effect: compare -4.5 against retained -3.5/-5.5 evidence at the same two city
    poses; retain the accepted priority fix and keep dynamic opt-ins disabled.
    Performance and cooked-negative obligations remain open. No new astronomy,
    starfield, renderer tuning, production selection or release promotion.
  - Reason: improve night navigation without making the survival setting too bright.
  - Source: current operator request, 2026-11-19.

- **D13:** Attempt a quick native correction for the reported wall artifacts;
  critically check the review against local source and preserve -4.5 EV.
  - Effect: isolated material/lighting/representation diagnostics and the smallest
    proven local fix are authorized. No speculative rendering rewrite, Terrain
    changes, production promotion or extra performance campaign.
  - Reason: resolve the visible defect without undoing the accepted night policy.
  - Source: current operator request, 2026-11-20.

- **D14:** Evaluate the Lumen-history review and fix supported in-scope findings.
  - Effect: bounded native cache diagnostics and a proven local runtime correction
    under D13; no renderer rewrite, World regeneration or production adoption.
    D11 retains final performance acceptance as a release-preparation obligation.
  - Reason: resolve the demonstrated transient using installed native controls.
  - Source: current operator request, 2026-11-20.

### Operator gates

- **Q1 [OPEN - NON-BLOCKING]:** Approve the placement, world scope and compact UI
  defaults in A1-A3, or revise them during review. They do not block this plan.
  - Update: D4/D5 settle placement and control policy; A1 is rejected. Only the
    detailed UX defaults in A3 remain reviewable, without an implementation gate.
- **Q2 [DEFERRED - outside current local-only scope]:** Is network-shared calendar time needed for 3.0.0?
  Default A2 keeps the proof local/single-player. Enabling it in a networked
  experience requires resolving authority first; local PC clocks are not a
  replicated multiplayer clock.
  - Disposition: no answer or operator interruption is required for this slice.
    Networked worlds remain unsupported; a future networking task must establish
    authority. This is an agent scope disposition under A2, not a new decision.

### Working assumptions

- **A1 [REJECTED by evidence; replaced by D4]:** Recommend `Plugins/Systems/ProjectTime` for reusable calendar
  time and `Plugins/UI/ProjectTimeUI` for its presentation. World owns the solar
  adapter and territory data. Placement remains subject to review under Q1.
- **A2 [ACTIVE]:** Mutable demo state is world-scoped, session-only and initially
  enabled only in admitted local/single-player demo worlds. New worlds/restarts
  default to D1; travel does not silently carry a demo offset into another city.
- **A3 [ACTIVE]:** Initial panel: territory-local date/time and Live/Demo label,
  preview offset slider (-12 to +12 elapsed hours), speed slider (0 to 60x),
  and Reset to PC time. Scrubbing enters paused Demo; explicit speed resumes it.
  These are reviewable UX defaults, not operator-prescribed ranges.
- **A4 [REJECTED by cadence review; replaced by A7]:** Live sampling observes a PC clock correction or resume by the
  next one-second refresh. Demo uses monotonic elapsed time, not OS-clock jumps.
  Solar update cadence is measured independently; one-second UI refresh does
  not prescribe one-second shadow updates.
- **A5 [REJECTED by D7]:** ProjectTime's runtime module owns clock and day/night behavior;
  its optional client UI module owns controls. World supplies geographic context
  and existing generated actor bindings. No new generic service facade or bus.
- **A6 [ACTIVE]:** Use native world-scoped delegates for time discontinuities,
  mode/rate changes and policy changes; one bounded visual-update timer samples
  current time. Select its interval from measured Live and accelerated-demo proof.
- **A7 [ACTIVE]:** Live reads sample fresh PC UTC on demand. Visible UI can refresh
  once per second; daylight uses A6. No separate one-second background clock poll
  or arbitrary correction SLA. Resume/explicit commands request a fresh apply.

## Non-goals

NTP, internet weather, automatic timezone-boundary GIS, a timezone database of our
own, a scheduler/event-calendar framework, multiplayer time replication, saved
time travel, OS-clock changes, global time dilation, automatic AI/vitals/animation
retiming, new city lights, Terrain/material changes, source acquisition, release
promotion, staging or commits. Logs and save-file metadata keep their real-clock
semantics; this is not a repository-wide replacement of FDateTime calls.
Shipping feature stripping, anti-tamper infrastructure, a weather simulation,
star catalog and astronomical Moon reconstruction are outside this slice.

## Verified evidence

| Evidence | Consequence |
|---|---|
| [Plugin categories](../../docs/architecture/plugin_rules.md) put reusable runtime services in Systems; World owns geographic realization. | D4 narrows the component to environmental time; Systems placement is not justified by hypothetical reuse. |
| No world/calendar clock contract found in ProjectCore, Systems or World. [Vitals](../../Plugins/Gameplay/ProjectVitals/Source/ProjectVitals/Private/ProjectVitalsComponent.cpp) uses elapsed DeltaTime; [Mind](../../Plugins/Gameplay/ProjectMind/Source/ProjectMind/Private/Services/MindServiceImpl.cpp) uses monotonic cooldowns and real timestamps. | No demonstrated gameplay calendar consumer justifies a general time service. Preserve those clocks. |
| [Service locator](../../Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/ProjectServiceLocator.h) is process-global and keyed by interface, without world scope. | Registering one mutable clock there would leak demo state across PIE worlds. |
| Installed WorldSubsystem.cpp admits Game, Editor and PIE by default; preview worlds are already excluded. | Override runtime support explicitly for admitted standalone Game/PIE contexts. Automatic runtime controllers must not mutate Editor realization worlds. |
| [World runtime](../../Plugins/World/ProjectWorld/Source/ProjectWorld/ProjectWorld.Build.cs) has no SunPosition dependency; [presentation realization](../../Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationRealization.cpp) already owns Sun, Sky Light, atmosphere, clouds, fog, PostProcess and capture roles. Its Apply requires canonical cells. | D7 extracts lighting setup/control into ProjectLight, usable without Terrain. World retains generated identity/transaction and capture viewpoints; no duplicate environment. |
| [Presentation data](../../Plugins/World/ProjectWorldData/Data/Presentation/kazan_representative_v1.json) is shared; [runtime data](../../Plugins/World/ProjectWorldData/Data/Runtime/kazan_territory_512_1536_v1.json) is territory-specific and its [schema](../../Plugins/World/ProjectWorld/Data/Schemas/project_world_runtime_profile.schema.json) is closed. | Zone identity belongs with territory runtime context, not duplicated lighting profiles or code tables. |
| [Runtime-profile parser](../../Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldRuntimeProfile.cpp) is Editor-only. [Georeference realization](../../Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldGeneratedGeometry.cpp) already persists a unique generated GeoReferencing actor with stable identity and non-spatial loading. | Serialize validated time context on that actor; JSON fields alone are not a cooked runtime handoff. Preserve GeoReferencing as coordinate authority. |
| Installed SunPosition is Runtime and exposes GetSunPosition; the native API takes date, coordinates and numeric zone/DST inputs. | Supply canonical UTC with offset 0/DST false and existing geographic coordinates; civil formatting must not alter the solar instant. |
| [Coordinate mapping](../../Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldCoordinateMapping.cpp) maps easting to +X and northing to -Y. Installed GeoReferencing provides EngineToGeographic and ENU vectors. | Qualify true north and projected-grid convergence; do not copy the old SunSky north offset. |
| Live City17 MCP inspection found stock /SunPosition/SunSky, 45,000-lux Sun, Sky Light realtime capture false, coordinates 45/-73 and October 2019. Its Tick checks supplied time changes; it does not advance a clock. | The legacy map is reference evidence, not a ready calendar service or authoritative city configuration. Raw inspection: tmp/world/daylight_readiness/20261118/legacy-live.json. |
| [Pause service](../../Plugins/UI/ProjectMenuGame/Source/ProjectMenuGame/Private/ProjectGameMenuService.cpp) explicitly pauses gameplay. [ProjectHUD](../../Plugins/UI/ProjectHUD/README.md) owns slot containers; [ProjectUI MVVM](../../Plugins/UI/ProjectUI/docs/ui_mvvm.md) owns shared presentation mechanics. Native Slider is already registered by ProjectUI. | Use a non-pausing domain panel; avoid putting clock behavior in HUD, settings persistence or the pause service. |
| [Experience definition](../../Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Experience/ProjectExperienceDefinition.h) and its [schema](../../Plugins/Resources/ProjectExperienceData/Data/Schemas/experience.schema.json) have no time-control permission. [Loading](../../Plugins/Systems/ProjectLoading/Source/ProjectLoading/Private/Experience/ProjectExperienceDefinitionDescriptor.cpp) forwards traversal through URL options. | Extend existing composition with one policy; do not mistake a caller-supplied URL flag for authorization. |
| [UI discovery](../../Plugins/UI/ProjectUI/Source/ProjectUI/Private/Subsystems/ProjectUIRegistrySubsystem.cpp) scans enabled plugins' Data/ui_definitions.json. Installed UE module descriptors support target/configuration filtering. | Same-plugin client UI is feasible, but absent modules/data must not leave broken registrations. Normal modes deny commands even when UI code is present. |
| [Settings UI staging](../../Plugins/UI/ProjectSettingsUI/Source/ProjectSettingsUI/ProjectSettingsUI.Build.cs) calls StageDataDir and declares Data files as UFS RuntimeDependencies. [Staging validator](../../scripts/ue/check/governance/validate_plugin_data_staging.py) discovers literal GetPluginDataDir(TEXT("X")) calls. | Stage Time UI JSON explicitly and prove cooked discovery. Dynamic plugin-name discovery can escape that validator; a green source scan is insufficient. |
| Installed DateTime.cpp selects PlatformUtcNow or EstimatedUtcNow; estimated UTC may rebase every 600 seconds. ICUText.cpp/ICUCulture.cpp use native timezone formatters. DefaultGame.ini selects InternationalizationPreset=All. | Verify fresh-PC sampling and packaged timezone data. Do not assume formatted output validates an unknown zone. |
| Static P1 Development proof: Kazan frame p95 14.997 ms, Manhattan 12.995 ms, both against 16.670 ms with zero host allowance. | Moving-sun/night cost remains unproved; static success is not dynamic acceptance. Evidence: tmp/world/lighting_performance-r2/. |
| [Producer identity](../../scripts/ue/world/generator_fingerprint.ps1) uses declared output/pipeline revisions and data inputs; its [regressions](../../scripts/ue/world/test/generator_fingerprint.Tests.ps1) reject blanket invalidation from code-only edits. | Bump only owners whose output changes; do not use a global pipeline bump to force regeneration. |

Primary references: [PC UTC](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Core/FDateTime/UtcNow),
[UTC-to-zone formatting](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/FText/AsDate),
[UE ICU support](https://dev.epicgames.com/documentation/unreal-engine/localization-overview-for-unreal-engine),
[ICU timezone rules](https://unicode-org.github.io/icu/userguide/datetime/timezone/),
[VSM invalidation](https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine).
Installed DaySequenceActor exposes SequenceUpdateInterval and an update delegate;
[Epic's Day Sequence guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/day-sequence-time-of-day-plugin-for-unreal-engine)
also separates visual application cadence from time progression. This supports
measuring cadence, not restarting provider selection: retain native SunPosition.
Resolve installed-source paths through the engine resolver; do not hardcode them.

## Current architecture and source of truth

The clock, zone context and demo UI described here are not implemented.
[World presentation](../../docs/config/render/render.md) currently supplies static
daylight and bounded gameplay Histogram; generated capture cameras stay fixed.

| Proposed owner | Owns | Does not own |
|---|---|---|
| ProjectTime runtime, under Plugins/World | World-scoped environmental clock, civil-time projection, time/control notifications and override refusal | Lighting, geographic authority, UMG, gameplay simulation clocks |
| ProjectCore time-view contract | Minimal world-context UTC snapshot/notification boundary shared by provider and consumer | Mutable global clock, new service registry or lighting policy |
| ProjectTimeDemoUI module, inside ProjectTime | Optional ViewModel, panel and validated intents through the plugin's public API | Clock advancement, direct light writes, permission grants |
| ProjectLight, under Plugins/World | Scene-lighting setup and day/night control; Sun, ambient sky, atmosphere/sphere, fog/cloud rendering and gameplay exposure | Calendar, weather-state decisions, exterior objects, global renderer/scalability configuration |
| ProjectWorld + ProjectWorldData | Geographic/zone context, generated identity/transaction, capture viewpoints and lighting-preset selection | Duplicated lighting implementation, a second runtime light writer or clock |
| Existing gameplay/experience composition | One default-deny time-control policy for the active world | Widgets, light updates, a general permission framework |
| Existing ProjectUI / ProjectHUD | Layout/layers/input/MVVM; minimal launcher-slot composition if needed | Time-domain behavior |

The accepted static-lighting record remains historical evidence. This new plan
replaces its unimplemented fixed-date/private-clock P3 proposal with D1/D2/D4/D5; do not
implement two clocks or maintain duplicate P3 checklists. At implementation handoff,
finalize that record's P1 scope and retain only genuinely separate optional work.
Only this new todo is edited during the present investigation (D3).

## Problem and root cause

Sun calculation, clock authority and UI are different responsibilities. A SunSky
actor alone supplies neither the requested PC-synchronized world calendar nor
shared demo commands. PC local time would give both cities the operator's zone;
longitude alone cannot identify political timezone or DST rules. Putting controls
inside the pausing menu would also obscure the live preview D2 seeks.

## Decision

### Premise / KISS gate

Follow D4/D5/D7: ProjectTime owns clock and optional client UI; ProjectLight owns
the existing scene-lighting responsibility extracted from World. Reuse UE clock,
ICU, SunPosition, world-subsystem lifecycle, delegates and ProjectUI primitives.
World delegates lighting setup within its transaction; ProjectLight is the single
runtime lighting writer. No generic registry, message
bus, scheduling framework or separate UI plugin. Give up automatic timezone GIS,
multiplayer synchronization and persistent demo sessions.

### Alternatives considered

| Option | Added / avoided | Judgment |
|---|---|---|
| Keep all behavior inside ProjectWorld | Avoids one plugin, extends the existing component | Viable smaller alternative, but D4 selects a distinct cohesive environmental-time owner. |
| ProjectTime with its UI, plus ProjectLight | Two cohesive World plugins; one focused extraction of existing lighting code | D7 replaces A5. Removes terrain-dependent lighting setup and avoids a competing controller. |
| Systems clock plus separate UI plugin | More cross-component contracts/lifecycles for hypothetical reuse | Earlier A1 rejected; current gameplay does not require this generalization. |

### Clock and zone behavior

- Store one authoritative UTC instant. Live evaluates current PC UTC; do not use
  PC-local Now() and then apply the territory offset again. Honor A7 using the
  native fresh platform UTC path where estimated UtcNow cannot meet it.
- Demo anchors UTC to monotonic elapsed time. Re-anchor at the currently displayed
  instant before changing rate, so dragging speed does not jump the calendar.
  Rate zero pauses the calendar only. Reset discards overrides and samples PC UTC.
- WorldData supplies an explicit named zone, initially Europe/Moscow or
  America/New_York for the respective territories. Use bundled ICU rules, including
  DST. Validate IDs; empty/invalid IDs must not silently become the PC zone or GMT.
  Zone changes affect civil display, not UTC or solar position.
- A3's scrubber selects elapsed UTC offsets from a gesture-start anchor and shows
  the resulting local date/time. It is not a naive 0-24 local-hour inversion;
  spring gaps and autumn repeated hours remain unambiguous UTC instants. Show the
  zone/offset alongside local time. A local-calendar picker is outside this slice.
- Use world-owned state through native UWorldSubsystem lifecycle. Existing global
  service discovery is unnecessary for internal clock/UI communication. Do not
  register mutable global time or introduce a facade/second world registry.
- Runtime activation explicitly supports standalone Game/PIE after validated
  context is ready. Editor, EditorPreview, GamePreview, inactive and networked
  worlds cannot start clock-driven mutations. This does not prohibit deliberate
  World Editor realization or explicit diagnostic setup. Teardown/end-play clears
  timers, subscriptions and pending applies; no callbacks target the next world.

### Cooked handoff and module direction

- Proposed handoff: one serialized, runtime ProjectTime context component on
  World's existing generated, always-loaded GeoReferencing actor. World validates
  the zone then persists the zone/profile identity and binds to that actor's
  coordinate authority. No second coordinate table, source JSON reader in runtime,
  transient-only handoff or additional context actor. Prove component persistence
  and cooked availability before enabling real-city consumption.
- Include that component in the existing World identity, semantic/inventory and
  recovery evidence. A copied profile hash alone is not authentication; normal
  generated-output and cooked-payload verification must cover the serialized data.
  Missing, duplicate, stale or invalid context refuses dynamic activation.
- Follow [dependency rules](../../docs/architecture/plugin_rules.md): ProjectTime
  and ProjectLight depend on a minimal ProjectCore time-view contract; Time
  implements it and Light consumes it through world-scoped binding. No default
  ProjectLight -> ProjectTime concrete dependency, global registration or bus.
  Time's own UI module can use its public API within the same plugin.
- Concrete World composition may depend on Time/Light types it constructs or
  owns. World Editor retains actor spawning, stable GUIDs/tags, always-loaded
  flags, volume-brush creation, capture cameras, save/transaction and recovery.
  ProjectLight owns the public lighting configuration/binding contract, parameter
  policy and runtime updates; no canonical bundle or WorldEditor private headers.
- Keep UActorFactoryBoxVolume/UnrealEd in the existing World Editor adapter.
  Runtime Time/Light have no UI/UnrealEd/WorldEditor dependency; Time also has no
  Light dependency. UI depends on Time and ProjectUI. Add a Light Editor module
  only if a demonstrated setup dependency cannot remain in that adapter. Verify
  Build.cs and descriptor graphs are acyclic, including non-Editor game targets.

### Mode control and day/night integration

- D5 is one world-scoped, default-deny policy resolved by existing gameplay/
  experience composition. It is not a UI preference or raw travel-option grant.
  ProjectTime validates every seek/rate/reset request; UI merely reflects access.
  Unknown/revoked permission clears overrides, closes controls and returns to Live.
  Demo and ordinary single-player must work in the same cooked executable.
- Treat the PC clock as D1's input, not an anti-cheat guarantee. Refusing ordinary
  override paths does not prevent a user changing their OS clock or executable.
- D7 includes native day/night integration in this todo. World supplies
  georeference-derived coordinates/north and generated identity/transaction;
  ProjectLight owns lighting policy/application and runtime control; World's thin
  Editor adapter owns native actor/brush creation. Preserve existing native
  actors and stable identities initially. No second SunSky bundle,
  per-frame actor scans or new global renderer/cache-CVar writer.
- Use one native world-scoped time-change delegate, not independent broadcasts to
  every sky component. ProjectLight consumes current UTC and applies the
  existing Sun/atmosphere/ambient policy coherently. SkyAtmosphere follows its
  atmosphere Sun through UE's existing rendering behavior; UI updates separately.
  D8's future weather remains deferred. New stars or
  a Moon are not prerequisites unless a bounded night-visibility test requires a
  small existing-owner addition; no custom celestial simulation.
- The clock computes time from its anchor on demand; it need not accumulate
  minute ticks. Notify discontinuities/rate/policy changes, apply the initial
  state immediately, and coalesce dragging to at most one visual apply per frame.
  Use one native timer for ongoing visual updates; paused Demo needs no repeated
  identical light writes. Always sample current time, not replay missed ticks.
- A6 leaves cadence unselected. Compare a real-minute interval against shorter
  intervals in Live; test accelerated Demo separately. A real minute at 60x is
  an entire simulated hour, so fixed minute updates are not a general solution.
  Measure shadow stepping, horizon transitions, VSM invalidations and update-frame
  spikes. No every-frame light interpolation disguised as low-frequency updates,
  forced stale shadow caches or per-tick manual Sky Light recapture.

### Deferred weather boundary

D8 is future direction, not a weather implementation task or a requirement to
invent a weather schema, bus, controller plugin or material layer now:

- Weather owns selection and evolution of conditions, such as foggy, overcast or
  rainy, and publishes the current state when it changes. Consumers must be able
  to obtain the current state when attaching; missed events must not mean stale
  defaults. Choose the actual contract/cadence when weather work is authorized.
- ProjectLight maps conditions to its owned lighting, fog and cloud rendering.
  The runtime material controller maps relevant conditions to its owned material
  response; effect owners activate their effects. Identify the existing runtime
  material owner then; do not assume a texture/material compiler is that owner.
- Weather does not directly mutate foreign lights, fog components or materials.
  Each mutable output keeps one writer, combining time, base preset and relevant
  conditions. Reuse native delegates or existing contracts; no global bus or
  per-frame weather broadcast. World-scoped subscriptions detach on teardown.
- Until weather exists, ProjectLight uses the accepted base atmosphere preset.
  Rain/snow, wet materials, weather UI, forecasts and simulation are all deferred.

## Required invariants

1. D1 works offline; host timezone changes do not alter a territory's civil zone.
   An incorrect PC clock is not corrected or represented as independently verified.
2. One clock per supported world; UI and solar consumer observe the same UTC.
   Foreign/stale contexts refuse. World teardown removes state and subscriptions.
   Editor/preview worlds receive no automatic runtime mutation; standalone Game/
   PIE activation requires the serialized context, not Editor-only profile JSON.
3. Demo affects only opted-in calendar consumers. Physics, animations, vitals,
   log timestamps and save chronology remain unchanged; no global time dilation.
4. Invalid dates, rates, IDs, provider state or actor bindings cannot partially
   mutate presentation. Missing context disables controls and retains observable
   static fallback, rather than inventing local time, location or lighting.
5. Runtime modules have no Editor dependency; ProjectTime runtime has no UI or
   lighting dependency; ProjectLight has no ProjectWorld implementation dependency.
   UI sends semantic commands; only the
   active composition grants D5 permission. Unknown policy refuses overrides.
6. Current Terrain, Water, structures, materials, authored actors and canonical
   authority are preserved. Identity reconciliation follows existing owners.
7. Native shadow invalidation remains correct. Report run p95/p99 separately from
   event-aligned update/recovery frames and worst observed stalls; rare events can
   escape both percentiles. Static percentile budget difference is not guaranteed
   per-update headroom. Keep the existing 16.67 ms/load-aware acceptance policy.
8. R2 source-selection, R5 Shipping/installed proof and final operator acceptance
   remain separate. A prototype, this plan and a review PASS do not promote a city.
9. World generation owns spawning/identity/recovery; ProjectLight owns policy/day/night
   writes. Teardown clears bindings; permission revocation clears Demo while
   preserving automatic Live daylight through the same controller.

## Implementation tasks

- [X] **T0 - focused native preflight:** prove bundled zone formatting/validation
  for both IDs and invalid input, fresh PC UTC semantics and the existing UI
  definition/layer entry route. Inspect only these capability gaps, not another
  lighting/provider survey. Verify D7's extraction/composition seam; Q2 is deferred.
  Hand off the old P1/P3 record without duplicating work. Fix its P1 tracing-scope wording,
  not the accepted gameplay.
- [X] **T1 - clock and fast proof:** introduce the one World-local ProjectTime
  plugin, minimal Core time-view contract and deterministic clock-model tests.
  Implement explicit world-type/standalone admission and lifecycle guards. Use
  sampled UTC/monotonic inputs for tests; never sleep or change OS time. Exercise
  absent-provider refusal at the real consumer boundary before enabling it.
- [ ] **T2 - context and access:** extend owning runtime schema/parser/data and
  realization for zone/bindings. Implement the serialized context component on
  the existing GeoReferencing actor; enroll it in identity, no-op, semantic and
  recovery checks. Prove both zones and invalid/absent context in a cooked fixture,
  with no source JSON available. Extend existing gameplay/experience composition
  with D5's one default-deny policy; validate the actual resolved mode/context,
  not a raw user-provided URL flag. Test direct API refusal, revocation and travel.
  Migrate actual fixtures/consumers once; no compatibility or permission framework.
- [ ] **T3 - lighting extraction and day/night:** introduce ProjectLight, move
  lighting parameter policy/application out of World's presentation implementation.
  Retain spawning, volume brushes, capture viewpoints, identity, transaction and
  surface material resolution in World. Pass only the public config/binding
  contract to Light; runtime updates consume the Core time view. Prove no UnrealEd
  dependency in a non-Editor build and lighting in a small scene without canonical
  Terrain; preserve generated actor identities, cleanup/no-op and authored actors.
  Integrate native SunPosition using ProjectTime UTC and geographic context.
  Add scoped change notifications and one
  measured visual-update timer. Compare Live/demo cadence per A6; no generic bus.
  Qualify dawn/noon/dusk/night, Histogram and cached-lighting range. Add at most
  one lifecycle-owned Moon only if the night proof requires it; no streetlight
  generation or claim of astronomical Moon reconstruction.
- [X] **T4 - demo UI:** add ProjectTimeDemoUI inside the same plugin: small
  JSON/MVVM panel, native sliders, labels and reset. Reuse the existing non-pausing
  layer/input host. Add only a
  thin HUD launcher slot or existing navigation entry as needed; HUD/menu code
  owns no time logic. Consume Escape/focus correctly, restore input on close,
  support keyboard adjustment, and prevent model refresh from reissuing commands.
  Keep module/data registration valid without client UI; D5, not build flavor,
  controls availability. No Shipping stripping task or user-toggleable permission.
  Stage definitions/layout JSON via the existing UI RuntimeDependencies pattern;
  inspect staged payload and exercise actual cooked discovery. The static staging
  scanner alone cannot prove dynamically discovered plugin data is present.
- [X] **T5 - local proof:** first use small worlds and faithful native fixtures,
  then one disposable qualified Kazan scene. Prove lifecycle, refusal/no mutation,
  clock continuity and actual dragging with visible lighting. Closing the panel
  retains Demo until Reset or world teardown; configuration stays unsaved.
- [ ] **T6 - bounded adoption:** after local success, verify Manhattan and
  dependency-local saved realization, no-op and corruption repair. Use explicit
  map/presentation output revisions as required; inspect shared profile/runtime
  identity reconciliation before claiming locality. Keep the global pipeline and
  Material/Terrain producers unchanged absent a demonstrated consumed change.
- [ ] **T7 - acceptance and knowledge:** run the matrix below, migrate durable
  contracts/diagrams/pitfalls, inspect final diff and restore protected state.
  Adopt dynamic defaults only after the visual and performance proof is accepted.

## Test-first and verification plan

### Red and green matrix

Feature acceptance baseline is the observed static World with no calendar service
or demo panel. Add permanent behavioral guards at the new contract seam before
implementing the corresponding behavior; demonstrate relevant known-bad controls.
Do not encode source-text allowlists or fake a failing metric to manufacture RED.

| Case / proof | RED / known-bad control | Required GREEN |
|---|---|---|
| Live PC source - permanent | Constant time or PC-local time treated as UTC | Fake PC UTC corrections observed on next query/consumer refresh; resume requests fresh apply; host-zone change has no effect; offline startup. |
| Demo arithmetic - permanent | Scaling epoch time or unanchored rate change jumps time | Exact anchor continuity, zero rate, bounded finite rates, reset to fresh PC UTC, leap/day/year rollover and overflow refusal. |
| Zone rules - permanent/native | Fixed New York offset or invalid-ID fallback | At 12:00 UTC, Moscow 15:00; New York 07:00 in January and 08:00 in July. Native DST transitions, invalid/empty IDs refuse, packaged ICU agrees. |
| World isolation - permanent | Global locator holds mutable clock shared by two worlds | Two PIE/test worlds can have independent Demo states; stale/foreign context and teardown cannot change another world. |
| World activation - permanent/native | Default subsystem admits Editor; runtime callback edits realization world | Standalone Game/PIE only after ready; Editor and preview scenes remain unchanged without timers/listeners; travel/end-play/deinitialize cancel stale work. |
| Cooked context - permanent + cooked | Zone exists only in Editor JSON or a transient component | One persistent non-spatial georeference-bound component, covered by World evidence; cooked UTC/zone/solar context works without source JSON; missing/duplicate/corrupt context refuses. |
| Mode control - permanent + cooked | Hidden panel but direct API/URL grants override in normal play | Same executable: demo permits; normal/missing policy refuses every mutation without state change; policy revocation/travel clears Demo and restores Live. |
| UI commands/input - permanent + acceptance | Programmatic refresh causes command loops; pause menu freezes preview | Mouse drag and keyboard reach the model once, labels track real state, non-pausing scene updates, Reset works, focus/delegates clean up. |
| UI staging - permanent + cooked | Editor discovery passes with unstaged definitions/layout | Client module declares UFS data staging; packaged definition, layout and ViewModel load through ProjectUI; deliberately missing payload fails the proof rather than passing the literal-path scanner. |
| Consumer refusal - permanent | Bad provider/date/duplicate light partially rotates a Sun | Validate before any write; unchanged static settings on refusal; precise error retained, no private fallback clock. |
| Solar direction/lifecycle - permanent/native | Copied legacy location or inverted north; duplicate controller after reload | Both-city independent cardinal expectations, georeference alignment, one writer, enable/disable, no-op, save/reload, travel and static fallback. |
| Lighting boundary - permanent/native | Lighting setup requires canonical Terrain or duplicates World's lights | ProjectLight works in a tiny scene without terrain data; World delegates setup; generated identity/no-op/cleanup, fixed captures and authored actors retain their existing proofs. |
| Appearance - acceptance | Rotating a constant Sun alone produces unreadable night | Ready main-player views and motion at dawn/noon/dusk/night; readable shade/night, retained highlights, no exposure flicker/cache warning. Human visual review required. |
| Dynamic cost - acceptance | Sparse stalls vanish from p95/p99 | Same scene/pose/resolution: frozen vs continuous vs timed Sun; report UTC/rate, event count, duration, VSM/Sky Light costs, Game/Render/GPU/frame p95/p99, memory and event-aligned worst observed apply/recovery frames. |
| Notifications/cadence - permanent + motion | A real-minute timer freezes an accelerated Sun; callbacks survive teardown | Latest UTC on first apply/seek/reset/resume; bounded callbacks, no catch-up replay or duplicate unchanged writes; clock stays correct at every cadence, visible motion and update-frame spikes qualify the chosen interval. |
| Real route/recovery - acceptance | Editor proof or SceneCapture used as installed/UI acceptance | Cooked default Live clock and real UI interaction, both-city routes, serialized native processes and exact protected-state restoration. No R5 claim from local proof. |

### Execution envelope

- Implemented exact focused names: `Project.Time.Clock.ModesAndContinuity`,
  `Project.Time.Clock.WorldIsolation`, `Project.Time.CivilZone.OfflineRules`,
  `Project.Time.Controls.ModePolicy`, `Project.Time.DemoUI.Controls.Session`, and
  `Project.World.Realization.Presentation.DaylightLifecycle`. Their results are
  recorded below; names alone are not proof.
- Use `scripts/ue/test/unit/iterate.ps1 -TestFilter <exact-name>` and the supported
  cold runner when startup/cook evidence is required. Build through
  `scripts/ue/standalone/build.ps1`. No broad dev filters or source regeneration.
- Preserve small fixed-clock/native-zone fixtures independent of tmp/. Use the
  real service/Widget command path, not copies of clock math in tests.
- Preserve High, actual viewport/internal resolution and crash guard. Sun math is
  not the expected GPU cost; measure VSM invalidation and Sky Light updates.
  ProjectLight's controller reads current time at a measured cadence; no
  interpolation that quietly rotates the light every frame and defeats the cache comparison.
- For each actual light apply, mark operation and engine-frame identity. Retain
  the preceding frame, apply frame and a bounded recovery window covering render/
  GPU pipeline latency; report worst observed frame cost and recovery duration.
  Include several complete real-cadence cycles, Live and accelerated Demo, with
  event counts/durations. Do not infer rare-event safety from pooled percentiles.
  Validate the measurement with a known-bad sparse-stall control that p95/p99 miss;
  name missing GPU attribution or incomplete recovery as a proof gap. Use native
  tracing/stat tools, disable diagnostic visualizations for timed acceptance, and
  avoid a new telemetry framework. The static ~1.67 ms p95 difference is not a
  per-event allowance or proof of spare GPU time.
- Under D11, defer the affected both-city packaged performance gate to final
  release preparation after visual qualification. Do not rerun it during this pass;
  its eventual execution remains
  under the existing load-aware policy. Reuse static P1 as historical control,
  not certification of dynamic defaults. Follow the initial-plus-one-replacement
  expensive-gate rule; attribute harness/source hashing separately from rendering.
- Keep final installed/Shipping R5 and its source freeze separate. The accepted
  static-lighting package does not certify the clock, UI, night policy or moving Sun.

## Documentation plan

- New owners: `Plugins/World/ProjectTime/docs/architecture/structure.md` and
  `Plugins/World/ProjectLight/docs/architecture/structure.md`, with their main
  diagrams and World README routing. Time owns its optional UI internally;
  no separate Systems/UI plugin docs or speculative weather documentation tree.
- ProjectTime owns time/control API and notifications; ProjectLight owns lighting
  setup, atmospheric rendering and runtime day/night semantics;
  gameplay composition owns D5 policy, World owns geographic/zone binding,
  generated lifecycle and locality. Update existing World architecture/
  territory docs and render/MCP evidence guidance only for changed contracts.
- Core owns the thin cross-plugin time-view contract; World owns cooked context
  realization and identity/integrity validation; Time owns runtime context consumption. Document
  world admission, UI data staging and event-aligned performance proof at those
  owners when implemented. Do not claim those contracts are already realized.
- Data/schema own timezone IDs and configurable UI/clock values. Docs state
  semantics, not copied profile fields, defaults or shader settings.
- Move proven UTC/local double-conversion, DST, world-scope, feedback-loop and
  night-cache, URL-permission and travel-state traps into the responsible pitfalls
  with executable regressions.
  Keep rejected experiments and test costs here. No stable doc links to this todo.

## Rollout and rollback

Start with disposable opt-in context and retained static fallback. Make no city
authority changes for clock/UI iteration. Saved outputs use the existing World
transaction/manifest/recovery path; disabling the feature removes its owned runtime
binding/optional Moon and restores declared static settings. UI unbinds without
changing saved preferences or another world's clock. Preserve source identities
and exact evidence before any later adoption or R5 selection.

## Completion criteria

D1/D4/D5/D7 work in both territories; active assumptions have accepted or explicitly
revised dispositions (A1/A4/A5 stay rejected). D8 remains recorded deferred work,
with no weather implementation added. Clock, controls and native
day/night are integrated together. Live/Demo/Reset, zone rules, mode-policy
refusal/revocation, isolation, input and notification paths pass;
actual UI controls and playable night are visually accepted. Under D11, final
release preparation must separately prove dynamic performance meets the existing
budget without hidden quality loss; this visual slice cannot certify it. Geometry/material inputs
are preserved, dependency-local work and total costs are reported, stable owners
are updated and protected state is restored. No network/shared-time or R5 claim
is inferred. Stop after this approved slice; no weather or generic time framework.

## Review record

### 2026-11-20 - native history correction, iteration 05

- The reviewer is right to distinguish Scene Lighting from Final Gather speed.
  Native diffuse-indirect visualization reproduces the wall patches. Radiance
  cache off before the jump smooths their pattern but retains stale brightness;
  it is not a sufficient correction. Scene speed 4 clears the early afterglow.
  Installed 5.8.3 source confirms ClearHistoryEveryFrame=1 clears history; 0 is
  normal. No temporal-clear trial was needed after the Scene candidate succeeded.
- Implemented temporary recovery in the existing ProjectLight runtime controller.
  Its [lifecycle owner](../../Plugins/World/ProjectLight/docs/architecture/structure.md)
  and [pitfall](../../Plugins/World/ProjectLight/docs/pitfalls.md) own the durable
  behavior; [World diagnostics](../../Plugins/World/ProjectWorld/docs/daylight_composition.md)
  owns the real-clock test route. No additional manager, dependency or data knob.
- `Project.Time.Integration.LightForwardPriority`: expected RED (24.8 s) on the
  missing large-seek recovery, GREEN (28.7 s), final expanded GREEN (26.8 s).
  Covers preserved authored budgets including speed 8, repeated seeks, Stop,
  unchanged notifications, small 60x steps and timer-only clock reads.
  `Project.World.Realization.Presentation.DaylightLifecycle` PASS (25.1 s).
  Editor build PASS (16.84 s final); Alis Development build PASS (52.61 s).
  Invalid history-arm invocation refuses as expected (27.2 s) before creating
  the requested proof directory; protected Config/WorldData remain unchanged.
- Opt-in `Project.World.Realization.Presentation.DaylightHistory`: primary PASS
  483.9 s, isolated Scene trial PASS 323.4 s, actual runtime recovery PASS 320.8 s.
  Actual Manhattan Hero, camera and UTC retained; absolute jump checkpoints
  5/15/75 s, native diffuse visualization and frame CSVs recorded. The runtime
  final camera consumes Scene speed 4 at 5 s and the original override/value is
  restored by 75 s. No permanent cache disable, history reset or exposure change.
- Fixed wall ROI (500,250)-(840,550), unmodified 8-bit screenshots: runtime
  baseline 15 s mean 75.14 and MAE against its own held reference 72.46; corrected
  15 s mean 1.99 and MAE 1.44. This measures stale wall illumination, not temporal
  noise or player visibility. The sampled wall matches dark settled night by
  15 s; the 5 s image still has afterglow. This is not an instantaneous reset.
- A separate short actual-clock 60x sunset reaches its paused endpoint and holds
  the exact same UTC for 75 s. Wall ROI endpoint-versus-held MAE 0.051/255.
  This is one transition at one stationary Manhattan pose, not a full-cycle or
  both-city gameplay claim. Sun, sky and road screenshots remain for human review.
- Rendered source boundary: retained read-only r3 World snapshot, current native
  runtime code, producer revision 4. No current-source generation, current frozen
  acceptance or R5 is inferred. Original 2560x1440 screenshots use native High
  internal resolution scaling; they do not prove native-resolution GPU acceptance.
- Runtime 15 s phase GPU p95 10.04 -> 10.98 ms, frame p95 35.41 -> 38.82 ms.
  Phase statistics omit boundary seconds/visualizer frames; untrimmed captures
  and hitches remain in raw CSV and analysis. Different lighting energy, Editor
  contention and capture overhead prevent clean incremental cost attribution.
  This is NOT a performance PASS; D11 remains open, including recovery events.
- Evidence: `tmp/release/3.0.0/time-light/iteration-05/evidence/`; compact matched
  boards and originals in sibling `review/`. Production Config/WorldData untouched;
  all four Runtime dynamic opt-ins remain false. Next: review settled-night
  appearance, then remaining cooked negatives; final performance/R5 stays deferred.

### 2026-11-20 - wall-artifact investigation, iteration 04

- D13 retains -4.5 EV. The reviewer's reflection-comment finding is stale for
  this checkout: `DefaultEngine.ini` already correctly identifies Lumen=1 and
  standalone SSR=2. No reflection-method or ISM stability-guard change is needed.
- The actual source cell has Nanite, ordinary static-mesh components, no mesh
  distance-field resolution and nine generated Lumen cards. Its Surface Cache
  view has missing coverage. This supports a representation concern but does
  not alone establish the original bright-patch cause.
- Reject the disposable Max Lumen Mesh Cards 12 -> 64 candidate: actual count
  remains nine, native rebuild costs 6199.7 ms and coverage does not improve.
  Do not repeat this budget-only change or regenerate city meshes for it.
- Source-cell diagnostics PASS (365.8 s); cards-only diagnostics PASS (232.8 s).
  The simplified scene is diagnostic. A read-only saved-scene replay produced
  all views but failed test-owned PIE-reference cleanup; that fixture bug was
  corrected. Normal Hero/loading-route diagnostics then PASS (270.6 s), including
  clean shutdown and restored session controls. Compile failures are not RED.
- Those 1080p replays are darker and do not reproduce the original large bright
  patches. Separate GI-off and screen-trace-off comparisons do not prove a fix.
  Missing external-actor warnings in the frozen snapshot and its old map
  revision prevent treating these images as current-source city acceptance.
- The controlled 1440p history proof PASS (225.1 s): a noon -> midnight jump
  reproduces the original blotches at about 15 s, then they clear by about 75 s.
  The final phase's filename says `60s`, but its timer starts after the 15 s
  capture: do not misreport the cumulative time as 60 s. On the same flat wall
  ROI, mean 8-bit luminance is 69.48 at 15 s, 3.05 at 75 s, and 2.57 in cold
  night. This identifies a history-dependent transient, not a particular cache.
- Reject native Final Gather Lighting Update Speed 4 as a quick fix: the same
  noon/night trial still shows blotches at 15 s (164.7 s diagnostic run PASS).
  Do not adopt a sustained rendering-budget increase for this result.
- Fix the existing VisualOnly capture path to wait at least 75 s after a seek,
  with a capture-count-based timeout. Keep cadence/performance intervals intact.
  This repairs false visual evidence, not the runtime jump. Earlier night-floor
  choice remains -4.5, but settled-night player visibility needs fresh review:
  earlier bright walls included transient lighting. Keep default quality intact.
- Corrected exact player/UI proof PASS (144.6 s): 2560x1440 scene, actual 1920x1080
  Slate UI, grounded Hero at the retained pose, -4.5 EV and midnight, Reset and
  clean PIE shutdown. Editor build PASS (10.24 s); ASCII/local links, diff
  whitespace and machine-local-path checks PASS. All four runtime opt-ins remain
  false; WorldData/Config diff is empty. No agent staging or commits.
- The temporary diagnostic C++ fixture is retained only in scratch evidence;
  it is not added as a permanent test requiring untracked snapshots. Normal
  source test: `Project.World.Realization.Presentation.DaylightPlayer`.
- Resume at the reproduced early post-seek window, not the clean late view:
  isolate GI/screen tracing/reflections there and compare with continuous 60x
  time before selecting another correction. The existing cold-night GI switches
  do not identify the transient's cache. No broad mesh-generation or lighting
  architecture work is justified by these results.
- Installed renderer range analysis also finds cached-lighting offset 8 has a
  nominal EV range [-4,16], while the new night floor admits -4.5. Offset 7 would
  cover [-5,15], including day maximum 14. The isolated trial does not fix the
  walls; keep this separate from artistic exposure and qualify actual view
  pre-exposure before any configuration adoption.
- Evidence: `tmp/release/3.0.0/time-light/iteration-04/evidence/`. Durable method:
  [wall-lighting diagnostics](../../docs/config/render/render.md#wall-lighting-diagnostics).
  No new Terrain/material/geometry authority or accepted runtime wall correction.

### 2026-11-19 - bounded intermediate night selection, iteration 03

- D12 executed: one new -4.5 EV candidate at the same midnight poses. Fresh exact
  player proof PASS in Kazan (160.1 s) and Manhattan (104.8 s). Retained -3.5 and
  -5.5 controls were reused with unchanged image hashes; both native camera poses,
  civil times and lighting inputs match. Separate runs do not isolate every cloud
  or wall-shading difference as an exposure effect. Both actual UI originals were
  inspected; no new UI or pointer-routing implementation was introduced.
- Select -4.5 for the prototype night policy: more readable sampled roads than
  baseline, less bright Kazan sky than -5.5. The sky remains artistic and blue;
  shaded walls still depend on lighting/material presentation. Do not claim AAA,
  astronomical accuracy, whole-route night acceptance or actual GPU exposure.
- Native final-view configuration records Histogram minimum 0.044 and histogram
  minimum -6.5. The existing Light default is updated; dynamic activation is not.
  Map output revision advances to 4 for projected/saved Light settings. Existing
  fingerprint checks confirm only `map:v1` changes, eight other producer identities
  are unchanged. Current saved city packages require normal source-backed release
  generation/qualification, not a manual identity refresh.
- Exact Light policy acceptance: one expected RED assertion against the old
  floor (42.5 s), GREEN with the selected default (42.5 s), retaining priority
  checks and the day exposure bound. Exact daylight lifecycle PASS (43.8 s):
  stale policy repairs only the context owner; complete policy survives actual
  disk reload. The capture extension builds (12.16 s); Editor policy build PASS
  (58.01 s); non-Editor Development build PASS (52.91 s). Initial capture build had
  a local misplaced test statement, corrected
  before any native proof; it is not RED evidence or another model iteration.
- Outer operation `046d8925532e4307b5dd96e220e9a302` completed `restored` before
  default/revision adoption. Selected capture candidate audit accepted; original
  World bytes, selector and runtime-profile bytes restored, with baseline audit
  failures unchanged. All four opt-ins remain disabled; no staging or commits.
- Handoff: `tmp/release/3.0.0/time-light/iteration-03/` contains two boards, eight
  original PNGs and a compact reviewer ZIP, including priority implementation/test
  source. Native capture costs above exclude protected validation/restoration.
  This closes the bounded comparison, not T3/T6/T7 or final R5. Keep cooked negative
  cases, human visual review and D11 performance/Live attribution open. No stars,
  astronomy, new lights, renderer tuning, Terrain or material changes.

### 2026-11-19 - native priority correction and visual iteration 02

- Reviewer priority finding CONFIRMED. Installed UE
  `Renderer/Private/LightGridInjection.cpp` selects a single forward light for
  Water/translucency/fog using priority and then colored brightness; equal top
  priorities trigger the observed warning. A permanent Sun preference is not the
  right runtime policy for the supplied night source.
- Existing ProjectLight controller now selects by native colored brightness and
  sets distinct forward priorities only when their native values differ. No new
  actor, registry, cached authority, timer, dependency or serialized field. Static
  presentation, clock/UI contracts, source geometry and materials are unchanged.
- Exact `Project.Time.Integration.LightForwardPriority`: four expected RED
  failures (48.4 s), then GREEN (39.9 s). It uses the actual native controller and
  Core read-only view, covering day, zero-Sun night, two active twilight sources,
  dominance reversal, paused no-op and no-fill admission. Initial double World
  initialization was a fixture error (50.9 s), corrected before the valid RED.
  Editor runtime-fix build PASS (14.37 s); final visual-harness build PASS (11.90 s).
- Reviewer corrections: `TimeOfDay_Default` is an installed template `.umap`, not
  a ready-to-assign star material. Engine `SkyAtmosphere.usf` renders disk indices
  0 and 1; absence in a ground-facing view does not establish a missing Moon.
  Existing fill remains explicitly artistic, without lunar phases or ephemeris.
  USNO's [2026 phase data](https://aa.usno.navy.mil/api/moon/phases/year?year=2026)
  confirms March 19 New Moon; no full-Moon realism claim is appropriate here.
- Retain SunPosition/Time/Light and generated ownership. Celestial Vault is
  installed Beta but its preconfigured environment is not a small replacement;
  accurate astronomy remains deferred. A star dome is optional, needs a verified
  material and normal World identity/cooked-reference proof, and is not an R2 gate.
- Iteration folder: `tmp/release/3.0.0/time-light/iteration-02/`. The existing
  player proof separates scene-only images from one UI image. Its opt-in
  diagnostic compares matched midnight baseline/fill-off/lower-EV-floor views,
  a cloud-free look toward the existing night source and native solar-relative
  twilight. It reads configured final view exposure rather than claiming GPU
  adaptation measurements. No visual preset is adopted by the test.
- Fresh exact player proof: Kazan five-condition scenes PASS (177 s), Manhattan
  five-condition scenes/UI PASS (149.8 s); targeted Kazan UI repair PASS (89.6 s).
  Native scrub/speed/Reset handlers and grounded support pass. Matched midnight
  baseline/fill-off/lower-floor poses are identical within each city. Scene PNGs
  are 2560x1440; separate native UI-window originals are 1920x1080. These are PIE
  visual proofs, not new cooked or performance acceptance. Non-Editor Development
  build PASS (42.26 s); final targeted-UI Editor build PASS (13.21 s).
- Visual disposition: lowering the existing night floor by 2 EV improves road
  readability in both sampled views without increasing the 0.2 lux fill. Fill-off
  is almost black. Recommend image 03 for review against baseline image 01;
  keep defaults unchanged until the visual decision. Solar-relative twilight is
  18:16 GMT+3 in Kazan and 19:22 EDT in Manhattan for this fixture date.
- Existing native disk is visible in Kazan's cloud-free upward view; Manhattan's
  upward view is obstructed by buildings. Do not claim both views prove Moon
  visibility. Read-only native material inspection qualified no drop-in star
  option; texture inventory alone does not exclude procedural star behavior.
  Both UI originals were visually checked: correct player window, legible controls,
  still a large panel. Clean scene captures exclude it.
- First window's Kazan scenes remain valid; its global UI screenshot selected an
  empty Editor viewport and is rejected. An empty optional PowerShell argument
  prevented the first Manhattan launch. One corrected replacement captured only
  missing Manhattan views and targeted Kazan UI. Native capture now explicitly
  selects the real panel's Slate window; the empty-target pixel check is a sanity
  refusal, not semantic visual acceptance. Both windows use existing restoration.
- Compact handoff: iteration 02 contains two labeled boards, twelve byte-identical
  original PNGs and a reviewer ZIP. Raw logs and repair failures stay in its
  evidence child. Native test durations exclude authenticated candidate setup and
  protected-state restoration. Night-preset approval, cooked negatives, final D11
  performance/Live attribution and production adoption remain separate obligations.
- Both outer operations completed `restored`; selected candidate audits accepted,
  original baseline audit failures unchanged. Runtime-profile bytes, original
  Canonical selector and generated World outputs restored; all four opt-ins remain
  disabled. No fresh R5 acceptance, staging, commits or production promotion.

### 2026-11-19 - visual/UI iteration 01 and performance handoff

- Registration review is accepted; no new defect in that correction was found.
  The reviewer's performance-first sequence is superseded by operator D11,
  without changing the measured REJECTED results or final release limits.
- Extended the existing `DaylightPlayer` test, not a new capture service. Optional
  street placement selects actual blocking Road support near the preview spawn,
  requires the grounded Hero and records the unchanged camera for every phase.
  The real city widget's scrub/speed/Reset handlers are checked before captures;
  `VisualOnly` skips the cadence/performance comparison and still verifies Reset
  reaches the native Light consumer. World owns this test-only change; runtime
  Light, renderer quality, materials and Terrain algorithms are unchanged.
- Final incremental Editor build PASS (12.52 s). Exact
  `Project.World.Realization.Presentation.DaylightPlayer` PASS in Kazan (150.5 s)
  and Manhattan (147.3 s). Separate exact `Project.Time.DemoUI.GestureRouting`
  PASS (76.5 s) covers actual pointer drag, keyboard adjustment, Reset click and
  Escape; do not confuse handler broadcasts with pointer input evidence.
- Review set: `tmp/release/3.0.0/time-light/iteration-01/`. Two labeled boards
  compare retained cooked overviews (2560x1440) with eight fresh grounded PIE
  player/UI captures (1920x1080, including native preview chrome). The 16 original
  PNG copies are byte-identical; no brightening/color correction. Captures use
  06:00, 12:00, 18:00 and 00:00 local; retained Manhattan cooked evening is 19:00.
- Visual disposition PATCH: daylight/evening and panel text are readable. Kazan's
  midnight foreground remains hard to distinguish; Manhattan's 06:00 is nearly
  black, while midnight walls/path are more readable. Different times are not
  evidence of a history/cache bug. Operator/reviewer visual approval remains open.
- New focused finding: Kazan evening shows UE's equal-priority directional-light
  warning. `ProjectLightSettings.cpp` configures Sun/night fill without assigning
  forward priorities; installed UE `LightGridInjection.cpp` detects equal top
  priorities and falls back to brightness for forward/Water/fog lighting. Next
  iteration should qualify an explicit selection in the existing Light owner,
  with its smallest regression, before arbitrary exposure changes. Do not hide
  the warning or create another lighting manager. Original ray-tracing resident
  memory warnings are preserved for final release qualification under D11.
- Reused the pinned prepared World through the existing restoring envelope.
  Restoration PASS: selector/runtime bytes restored, all four opt-ins disabled,
  generated World worktree unchanged, recovery marker removed. Selected visual
  candidate audit was accepted. Baseline strict audit remains REJECTED before
  and after for identical stale producer fingerprints; do not call that repaired.
  Final restoration evidence is recorded in the review pack; no production
  adoption, package rebuild, performance campaign, staging or commit is authorized
  by this visual iteration. The expensive source/candidate qualification belongs
  to preparation cost, not light-update cost; keep optimization in the existing
  [preparation backlog](../02_backlog/world/20261112-1259_world_reduce_authenticated_preparation_cost.md).

### 2026-11-19 - registration recovery review correction

- Reviewer registration finding CONFIRMED and fixed in the existing World Editor
  realization owner. No-op now requires registered Context and Controller as
  well as the existing Night check. Replay configures and registers valid
  existing instances; failed registration refuses instead of reporting success.
  No new runtime lifecycle, binding mechanism, actor or module dependency.
- Extended `Project.World.Realization.Presentation.DaylightLifecycle` rather than
  introducing another scene: unregister each component, replay the same profile,
  retain instance/identity/policy, record one owner repair and then zero-write
  replay. The real runtime provider binds repaired Context; repaired Controller
  applies daylight and consumes a notification through an actual world tick.
- RED: exact test completes with four expected failures on the original code:
  Context/Controller each report zero repaired owners and remain unregistered
  (41.7 s). GREEN: same exact test passes after the fix (37.9 s); Editor rebuild
  PASS (32.04 s). Receipts: `tmp/world/time_light/registration-red-complete/`,
  `registration-green/` and the corresponding build logs.
- The first fixture attempt crashed before completion: full ticking exposed the
  existing minimal GameInstance's missing reciprocal world-context link in
  native NetworkPrediction tracing. It is not counted as RED. A concrete
  test-only GameInstance links that existing context without starting unrelated
  game services; no production lifecycle abstraction was added.
- Reviewer Manhattan timeout row is stale: the authorized r7 cooked 1440p proof
  already passed. Do not repeat that campaign. Retain its performance REJECTED
  result under contention and the very dark night images; neither this Editor
  registration fix nor the old Kazan PASS certifies those open criteria.
- Standing no-op/recovery semantics and the verified registration trap live in
  the owning World composition/pitfalls docs. No Terrain/material/data/profile
  change, World regeneration, staging, commit, default adoption or publication.


### 2026-11-19 - authorized Manhattan continuation

- D10 execution is complete. Retain the functional PASS and performance
  REJECTED dispositions separately; this task remains current. No default
  adoption, geographic promotion, new World generation or dependency redesign.
- Fresh Development package/archive verification PASS. The previously rejected
  1080p capture is replaced by an actual 2560x1440 cooked proof: PC UTC, New York
  civil labels (including March GMT-4), real staged UI/widget commands, all four
  phases, continuous 60x sunset, Reset, configured-policy revocation and normal
  exit. All ten original PNGs have verified dimensions and retained hashes.
  The resolution fix therefore has observed cooked GREEN.
- Agent inspection: noon and twilight are readable from the fixed aerial camera;
  the panel is legible. Night/pre-dawn show mostly dark silhouettes. This is not
  accepted player-eye night visibility, operator approval or proof that street
  routes are usable. No brightness/renderer/geometry changes were made here.
- Manhattan's three 60x performance traversals all pass Hero/collision/streaming
  correctness with zero streaming failures, but fail the existing budget:

  | Run | Samples | Frame p95 ms | Render p95 ms | GPU p95 ms |
  |---|---:|---:|---:|---:|
  | 1 | 9585 | 25.0325 | 25.0224 | 10.5143 |
  | 2 | 8243 | 29.2990 | 29.1942 | 10.5234 |
  | 3 | 9182 | 25.9940 | 25.9626 | 10.1534 |

  Aggregate: REJECTED, 27010 samples. Base budget 16.67 ms; the existing
  load-aware policy applied its capped 10% allowance, giving 18.337 ms. Host CPU
  windows were 30.67-70.33%, GPU windows 7.33-16%; the aggregate's selected CPU
  load is 30.67%. Concurrent non-test CPU activity was observed; it was not
  stopped. These results establish a miss in that envelope, not a daylight-only
  regression or GPU saturation. The existing policy was not relaxed.
- Required frozen-vs-moving comparison: after full restoration, one diagnostic
  was run with the same authenticated package, runtime identity, noon anchor,
  route and High/1440p envelope, but rate 0. No new package, Canonical selection,
  source edits or map writes. Correctness PASS, 9952 samples, exactly one paused
  light apply. Frame p95 24.1208 ms, render 24.0745 ms, GPU 10.3438 ms; native
  performance remains REJECTED. Host CPU was 51% before / 57.67% after. Pausing
  continuous Sun updates did not recover budget. This single observation does
  not certify performance, establish pre-feature cost, or isolate update cost
  from changing shadow direction and host contention.
- Next smallest performance step: use the retained frozen package for matched
  paused/moving observations under comparable, quieter host load, then inspect
  native render-thread CPU work if the miss remains. Do not start another
  package, Terrain investigation, global Lumen tuning or controller redesign
  merely because this contended timing series failed. Native CSV diagnostics
  retain render/resource/visibility categories without claiming exclusive cause.
- Optional-controller/soft-reference composition remains a separate architecture
  question. This verification changed no dependency or binding mechanism.
- Temporary r6 startup failed before packaging because the Manhattan-only wrapper
  omitted the Kazan description import that defines shared package/source helpers.
  It fully restored. The corrected r7 wrapper retains that read-only import and
  executes only Manhattan; helper/cook-target preflight passed before execution.
  No native failure or model attempt is inferred from that wrapper error.
- Restoration PASS for both wrappers: owner identities match, original Canonical
  selector and runtime-input bytes restored, marker removed. All four time
  profiles are disabled. No Alis/Editor processes remain. No source/package
  integrity failure was reported; no Git writes were performed by this run.
  An external commit advanced HEAD from 2cb9da59b to b26ec8c62 during preparation
  and consumed the existing staged change. Preserve that state. The package
  records actual source revision b26ec8c62 and content-state hash b05f93751bd8;
  the full binding remains in its package receipt.
- Evidence: `tmp/world/time_light/city-candidate-window-r7/` (package binding,
  cooked captures/integrity/inspection, three receipts and rejected aggregate),
  `tmp/world/time_light/manhattan-frozen-control-r1/` (diagnostic only), and
  `tmp/world/time_light/continuation-restoration-check.json`. Retain r5 Kazan
  acceptance unchanged. Preparation/source hashing dominated wall time; its
  optimization belongs to the existing authenticated-preparation backlog.


### 2026-11-18 - current checkpoint and bounded continuation

- Implementation is reviewable; this task stays current. T2/T3/T6/T7 remain open
  for their remaining cooked-negative, cadence, visual and adoption evidence.
  Do not restart the architecture, Terrain, material or native timezone research.
- Replacement Development package/archive checks PASS. Kazan's three actual
  1440p High, 60x daylight traversals and their existing aggregate are ACCEPTED:

  | Run | Samples | Frame p95 ms | GPU p95 ms |
  |---|---:|---:|---:|
  | 1 | 16111 | 12.7153 | 9.5738 |
  | 2 | 16022 | 12.7192 | 9.6204 |
  | 3 | 16218 | 12.7123 | 9.5600 |

  All 48351 samples belong to the source/package-bound product route. Aggregate
  budget is 16.67 ms with zero load allowance; host-before/after windows are
  sampled before fingerprinting. Collision/Hero/streaming correctness passed.
  This qualifies that Kazan route, not Manhattan, every night pose or Shipping.
- Manhattan's first cooked UI capture is REJECTED: `ready-or-capture-timeout`.
  Log ordering proves command-line 2560x1440 was replaced by save-load graphics
  settings at 1920x1080 before the probe started. The readiness fix only waited;
  it did not establish the UI-only capture envelope after load. No Manhattan
  dynamic traversal ran. This is not a measured daylight FPS regression.
- The explicit Development-only UI probe now requests its one-shot native
  capture size after the admitted world/pawn is ready, then validates realized
  size. Non-Editor Development rebuild PASS (30.79 s). Cooked GREEN for this
  correction is not yet established. The ordinary save/settings owner is unchanged.
  Resume Manhattan cooked capture and its three product-route runs only; retain
  the prior rejected receipt and Kazan results. Do not regenerate either world
  or repeat mathematical/static characterization tests without a new reason.
- The initial-plus-one-replacement expensive campaign budget has been spent.
  Further packaging/runtime campaign needs a bounded scope decision under the
  existing gate policy. A local diagnostic fix/build is not a new accepted run.
- Event-window analysis uses actual Kazan cooked native frame IDs: 20 Live
  applies in 20.00 s and 578 accelerated applies in 60.02 s; 19/577 complete
  preceding-through-apply+5 windows. Live worst window: frame 20.57 ms, GPU
  13.35 ms maximum across windows. Accelerated GPU maximum: 14.67 ms. Its
  592.13 ms frame maximum overlaps a screenshot request by two frames; the
  frozen phase also has a 537.73 ms boundary/capture stall. Retain these raw
  maxima, but do not attribute them to VSM or certify performance from this
  capture-contaminated probe. Separate traversal is the Kazan performance proof.
  Five-frame windows do not establish complete cache recovery or exclusive GPU
  attribution. A 1000-frame sparse-stall control has p95/p99 10 ms while its
  event window detects the injected 100 ms stall.
- Still required before adoption: Manhattan cooked proof/performance; clean
  event/recovery attribution and cadence disposition; normal-mode, absent UI
  payload and invalid cooked-context refusal controls; player-eye night review.
  Live one-minute comparison was not run. Do not silently mark these passed.
- Runtime authoring inputs were found enabled after a prior restoring window:
  generated World/Canonical restoration alone did not cover those source files.
  The two task-owned flags were returned to their original disabled bytes. The
  replacement parent now owns persisted backups, a finally restore and exact
  byte comparison for both inputs. The earlier leak's cause is not established.
- Final replacement recovery PASS: `identities_match`, prior Canonical selector
  restored and operation marker removed. Both Runtime input hashes match the
  disabled baseline byte-for-byte; all four declared profiles are disabled.
  Restored World retains its pre-operation stale-fingerprint audit disposition;
  this is exact restoration, not a new accepted generated target. Index unchanged.
- Final static checks: 27 Markdown files have valid local links and ASCII text;
  15 JSON/descriptor files parse, nine relative-schema documents validate, and
  the full diff whitespace check is clean. Prefix governance passes 1573 files.
- Evidence: `tmp/world/time_light/city-candidate-window-r5/` contains package,
  Kazan aggregate/children and rejected Manhattan output. The retained r3 Kazan
  cooked images/UI receipt remain valid within that build's stated scope.
  `city-candidate-window-r3/cooked-event-window-diagnostics.json` records the
  offline window analysis. No new accepted L2, R5 or production selection.


### 2026-11-18 - implementation evidence

- Both-city actual Editor player probes PASS: Kazan 188.2 s, Manhattan 179 s.
  Paused/1x/60x intervals recorded 0/14/149 light writes in each city, with no
  multiple applies between adjacent sampled frames. These retain the 1080p
  screenshot limitation and do not certify the frame budget. The PIE bright
  patches and resident-geometry warning were not reproduced in the Kazan cooked
  views; their cause is not established and no global renderer tuning was made.
- Two normal map updates PASS (86.1/111.4 s commandlet time): zero created or
  removed actors, 13 updated actors per map, zero road/building sections. Fresh
  strict generated-authority audit PASS. Only the two map/bootstrap artifact
  sets and producer fingerprints changed; the other 13 artifact sets match the
  retained qualified payload. All 15 scope receipts were reconciled, so their
  receipt hashes must not be called unchanged.
- Development package/archive checks PASS. Kazan cooked clock/UI probe PASS:
  actual staged layouts, PC UTC, civil-time checks, widget command, Reset,
  configured-policy revocation and ten original 2560x1440 views, including a
  continuous sunset at 60x. Aerial night is uniformly dark with visible outlines;
  this does not certify street-level night play or final art/R5 acceptance.
- First dynamic traversal was REJECTED before performance sampling: the new
  diagnostic probe checked 1440p while saved graphics settings were briefly
  1080p, before the existing performance owner established its envelope. The
  Development-only fix delegates traversal resolution admission to that owner
  and makes UI capture wait for realized size. Non-Editor rebuild PASS (41.9 s).
  Replacement execution remains pending; this is not an observed FPS failure.
- Fingerprinting the selected source through existing Git/LFS filters took
  roughly 7-12 minutes per check. This is preparation cost, not renderer cost.
  The replacement reuses the accepted prepared World snapshot rather than
  repeating map generation. Host-after measurement moves before fingerprinting;
  the earlier wrapper placed it too late and supplied no accepted performance
  result. Existing source/package integrity checks remain in place.
- Native gates now retain seven clock/zone/UI/solar/policy checks (7/7 PASS),
  actual Slate mouse/keyboard gesture routing (PASS, 65.8 s), and the confirmed
  arrival policy regression (PASS). The latter reproduces the ordering found
  by the real-city probe: pipeline completion can precede ServerTravel arrival.
  Both event orders, foreign-world and unnamed-request refusal are covered;
  successful-load permission no longer records the departing world.
- Night persistence/lifecycle PASS (48.4 s): visible Sun-owned night fill,
  actual map save/reload and runtime composition, opt-out, no-op repeated opt-out
  and re-enablement. Destroyed components remain findable by UObject name until
  collection; realization excludes invalid retired objects.
- First Kazan city probe passed functional commands but failed visual inspection:
  the UI recipe used the wrong background field and midnight was unreadable.
  Both are retained as failed evidence. The replacement stopped at permission
  admission before captures, exposing the loading-arrival race above. No city
  appearance acceptance is claimed from these runs. Screenshots were 1920x1080
  despite reported logical viewport dimensions; they are diagnostic, not 1440p
  performance acceptance. Candidate World/Canonical state was fully restored.
- The bounded night correction uses the existing Sun actor, an artistic
  non-shadowing fill and a lower Histogram floor. Its rendered acceptance remains
  pending. No additional context actor, weather system or renderer CVar change.
- The Development-only Time UI runtime probe exercises real staged definitions,
  widget commands, PC UTC, civil labels, four day phases, frozen/1x/60x sampling,
  reset and configured policy revocation. It never grants permission and has no
  Light dependency. Actual cooked execution remains pending until recorded below.
- Reviewer watchpoints implemented: World runtime owns exact-world composition;
  context uses identity/integrity terminology, not cryptographic authenticity.
  No new context actor, registry, generic bus, weather or LightEditor module.
- Feature RED: `Project.Time.Clock.ModesAndContinuity` executed against the
  initial Live-only implementation and failed 10 expected Demo/continuity checks.
  Compilation errors are not counted as RED.
- Clock/zone/world/UI/solar native checks: initial five-test gate PASS (45.9 s).
  Expanded controls gate: five PASS, one fixture timer assertion FAIL; native
  timers activate pending entries after a frame, so the refusal check was moved
  to a real multi-frame latent fixture. Exact paused-revocation check PASS
  (49.5 s), without UI refresh or a clock read triggering revocation.
- Native widget construction and actual slider/reset delegates PASS. The real
  single-player mode-policy test passes configured default denial, explicit
  permission, exact successful-load world, URL rejection and in-progress travel
  refusal. This is not actual mouse/focus or cooked UI acceptance.
- `Project.World.Realization.Presentation.DaylightLifecycle` PASS (47.8 s,
  1/1 required runtime-binding marker): zero Terrain cells, no-op, corruption
  repair, malformed identity refusal, actual disk save/unload/reload and native
  Game-world startup through the concrete subsystem. Fixture startup was aligned
  with Engine LoadMap's preload world type and active-level initialization.
- Existing rendered `Presentation.ActorLifecycle` characterization PASS before
  extraction (121 s) and after extraction (91.1 s). Authored actor/identity,
  fixed capture and static exposure behavior retain their existing coverage.
- `GenerateDefinitions -type=ExperienceDefinition`: two generated, zero failures;
  only the existing configured demo definitions grant controls. Ordinary modes
  default to denial. Runtime profiles remain disabled outside the restoring
  native candidate window.
- Map output revision advances for serialized time/light context. Pipeline,
  Material and Terrain producer revisions remain unchanged. Actual locality and
  any necessary baseline reconciliation must be read from the candidate receipt;
  no fresh L2/R5 or dynamic rendering PASS is inferred from unit tests.
- Evidence root: `tmp/world/time_light/`. Relevant receipts: `red-clock`,
  `green-native-fixtures`, `native-controls-proof`, `paused-revocation-proof`,
  `final-context-proof`, `static-regression-proof` and definition-generation logs.
  The owner discovery/staging scanner passes but does not enumerate ProjectTime's
  dynamically discovered JSON; cooked payload inspection remains required.

### 2026-11-18 - architectural investigation

- Clock/backend separation is justified by D1/D2; Systems placement and a domain
  UI adapter are recommendations, not existing implementation or approved paths.
- Inspected category policy, Core locator, World presentation/runtime/identity,
  UI MVVM/layers/sliders, pause behavior and installed UE UTC/ICU APIs. Native
  timezone support is available; invalid-ID admission and cooked behavior still
  require T0. No feature tests or native gameplay reruns were performed.
- Earlier live legacy MCP inspection is retained as evidence; it exited normally
  and changed no source assets. No repeated legacy inspection is required.
- Only this REVIEW REQUIRED todo is created. Existing lighting work, production
  code, configs, assets, stable docs, index and commits remain untouched.

### 2026-11-18 - operator-directed simplification

- D4/D5 replace the generalized Systems/separate-UI recommendation with one
  World-local plugin and mode-controlled overrides. A5 proposes keeping native
  day/night control inside it while retaining World's environment generation.
- Inspected actual Vitals/Mind time use, experience schema/loading, mode ownership,
  UI auto-discovery, installed module filtering and Day Sequence cadence API.
  No additional gameplay calendar consumer or override-permission field exists.
- D6 changes this todo only. Cadence, integrated night appearance and GPU cost
  remain unverified; no feature implementation or runtime tests were performed.

### 2026-11-18 - lighting ownership and deferred weather

- D7 replaces A5's combined runtime owner with ProjectLight. Native Epic rigs
  group lighting/atmosphere; World's existing lighting implementation also handles
  canonical admission, identity and capture cameras. Extract only the lighting
  responsibility, preserving existing transaction and lifecycle proofs.
- D8 records a future weather-state owner and separate lighting/material/effect
  application. No weather code, effects, messages or material changes are planned
  in this slice. Only this todo changed; implementation remains unstarted.

### 2026-11-18 - integration-contract review

- F1-F6 accepted as concrete plan refinements: explicit world admission, cooked
  context, Editor/runtime setup boundary, staged UI, resolved mode permission and
  event-aligned rendering cost. F5 was already required; its real-route proof is
  now more explicit. Native defaults admit Editor, but already exclude preview.
- F7 accepted for acyclicity, corrected for repository policy: a Light consumer
  uses Core's time-view contract rather than directly depending on Time concrete
  APIs. Direct dependencies remain valid for composition that constructs/owns a
  type and for Time's internal UI module. No generic service abstraction added.
- Keep D1-D8 unchanged. Q2 is deferred under local-only A2, with no new operator
  gate or invented networking decision. Reject treating static p95 headroom as a
  guaranteed per-update allowance. Weather and final Shipping R5 remain separate.
- Inspected installed WorldSubsystem/VSM source, runtime-profile/georeference
  realization, UI staging/discovery and scanner, gameplay URL parsing and category
  dependency policy. Documentation checks only; native feature proof is pending.
