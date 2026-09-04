# Present World environment

**Status:** RESEARCH COMPLETE / IMPLEMENTATION NOT SELECTED
**Scope:** Universal generated-World environment/day-night capability; Kazan is the first
performance and product fixture, not the owner or a runtime branch.
**R1:** PASS - 2026-08-25
**Existing owner:** ProjectWorld presentation contract; each territory data owner holds
its concrete activation/configuration (ProjectWorldData for Kazan)
**Selected future owner:** ProjectWorldEnvironment runtime integration, if prioritized

**Stable routes:** [plugin boundaries](../../../docs/architecture/plugin_rules.md),
[World proof layers](../../../docs/testing/world_pipeline_layers.md), and
[producer fingerprints](../../../scripts/ue/world/generator_fingerprint.ps1).
**Kazan first-fixture evidence:**
[accepted Slice 4 runtime evidence](../../01_done/world/world_validate_kazan_territory_slice_4_runtime.md).

## Product outcome

Give generated territories a genuine runtime day/night cycle with coherent sky,
light, atmosphere, and time-of-day presentation without changing geography or
waiving the physical RTX 4070 1440p/60 performance target.

Runtime day/night is an essential ALIS realistic-world capability. The current
static presentation remains the deterministic control and operational fallback,
but it cannot satisfy or close the future environment implementation. If Epic
DaySequence fails, keep each territory usable through that fallback while investigating a
smaller dynamic provider; do not silently remove the product requirement.

## Verified baseline

- [ProjectWorld](../../../Plugins/World/ProjectWorld/README.md) owns the implemented
  presentation schema, loader, material projection, and generated actor identities.
- [Kazan presentation data](../../../Plugins/World/ProjectWorldData/Data/Presentation/kazan_representative_v1.json)
  owns concrete material references, static sun/sky/fog/exposure values, and viewpoints.
- Apply creates one movable Sun, Sky Atmosphere, real-time Sky Light, volumetric
  Height Fog, Volumetric Cloud, fixed-exposure Post Process, and named cameras.
- Lumen, Nanite, Virtual Shadow Maps, ray tracing, and disabled global auto exposure
  are project-wide configuration and are already inside the accepted Kazan baseline.
- SunPosition is project-enabled but is not consumed by the current Kazan realization.
- Installed UE 5.8.1 CL `56057345` marks Day Sequence Experimental. Its stock
  `ASunMoonDaySequenceActor` self-registers and owns sun, moon, atmosphere,
  real-time Sky Light, optional fog/cloud, a sky sphere, and `DSCA_24hr` content.
- Day Sequence defaults to a five-minute cycle and a zero update interval. Local
  source makes `SequenceUpdateInterval` protected with no public setter, so a clean
  configurable integration needs at least a tiny derived provider actor; directly
  spawning the stock class is not enough for the proposed performance controls.
- The accepted `512/1536` packaged Development receipt recorded `14.999 ms`
  overall frame p95, `17.773 ms` p99, `14.938 ms` render p95, and `10.295 ms`
  GPU p95. Dense-centre frame p95 was `15.969 ms`, leaving only `0.701 ms`
  before the `16.67 ms` budget. No dynamic-light cost may be assumed affordable.

## Reviewer decision audit

### Accepted

- Epic Day Sequence is the primary implementation direction before custom
  sun/moon/clock code.
- Day Sequence is an Experimental runtime dependency and must not enter reusable
  `ProjectWorld` core.
- Existing generated environment actors remain the deterministic fallback.
- Presentation time must not become scenario, vitals, save, NPC, or survival time.
- The combined presentation schema/parser must stay unchanged: it participates in
  map, presentation, Landscape, road, and building producer fingerprints.
- Moving a directional light can invalidate Virtual Shadow Map cache pages. Native
  update intervals are a candidate control, not proof that the cost is safe.
- Celestial Vault, engine changes, a custom clock, and immediate custom sequence
  assets are outside the first experiment.

### Corrected or rejected

- **The previous packet under-scoped the requirement.** The operator now explicitly
  requires genuine runtime day/night for the realistic ALIS world. E0 and E1 are
  controls/diagnostics, not selectable product outcomes.
- **The implementation is not proven tiny.** Provider switching, private generated
  role identities, startup/teardown, cook closure, exposure, and performance remain
  real integration work.
- **A reusable plugin must not "recognize Kazan" in code.** Concrete map/provider
  selection belongs to ProjectWorldData configuration; reusable code consumes it.
- **Do not add either plugin to `Alis.uproject`.** Stable plugin architecture forbids
  new project-plugin registration there. Descriptor dependencies own activation.
- **ProjectWorld cannot remain literally untouched.** Generated presentation-role
  prefixes are currently private and duplicated. An isolated provider needs one
  minimal public read-only identity query; it must not copy private strings.
- **Stock DaySequence is one bundled Epic technique.** It does not isolate the cost
  of its moon, sky sphere, real-time Sky Light, fog, cloud, sequence evaluation, and
  moving lights. Treat the stock bundle as one candidate; do not claim per-feature
  attribution without a separate controlled candidate.
- **Shipping is correctness/cook evidence, not the current FPS envelope.** The
  launcher Shipping route does not expose the native CSV profiling used by the
  accepted harness. Physical 4070 performance remains packaged Development;
  selected behavior still gets a separate Shipping product-route proof.
- **Static fallback is not a terminal acceptance result.** A DaySequence stop
  condition stops that provider attempt and returns the concern to dynamic-provider
  research. Static presentation only keeps the product operable during recovery.
- **The funnel still needs a small temporal visual diagnostic before E3.** A still
  capture cannot decide whether `0.50 s` updates visibly step. Use a short live MCP
  observation or authenticated time series; reserve the full MCP audit for the survivor.

## Selected research direction

Select genuine runtime DaySequence through a **bounded performance funnel**.

```text
E0 current static paired control
        +
E2 moving DaySequence at 0.50 s
        |
        +-> dense-centre + fixed-time captures fail
        |       |
        |       +-> E1 fixed DaySequence diagnostic
        |       +-> identify provider vs motion/VSM cost
        |       +-> optimize one measured bottleneck and repeat focused proof
        |
        +-> focused proof passes
                |
                +-> temporal diagnostic is smooth -> full product traversal
                |
                +-> visible stepping -> E3 at 0.25 s
                                      -> repeat dense-centre + temporal proof
                                      -> survivor gets full product traversal
```

E0 and every executed DaySequence candidate use the same warmup, binary, machine,
driver, D3D12 High 2560x1440 envelope, and `512/1536` runtime profile. Dynamic
candidates also use the same initial time and cycle rate. E0 is rerun in the same
session; the Slice 4 receipt is historical reference, not a paired baseline.

Selection order:

1. a moving candidate is mandatory; E0 and E1 cannot win;
2. correctness, cook, ownership, locality, and frame p95 hard gates;
3. smooth visible progression plus readable dawn/noon/dusk/night;
4. frame p99/max, Render/GPU/Game p95 deltas, memory, package size, and churn;
5. KISS: prefer E2 over E3 when both are smooth and pass.

Do not preplan another interval. If E3 still cannot satisfy both cadence and
performance, use its evidence for a new focused decision. If stock DaySequence
cannot satisfy the requirement after measured architecture-local optimization,
stop that implementation and revise this research for a leaner dynamic provider.

## Ownership and plugin hierarchy

```text
Plugins/World/
  ProjectWorld/
    generic generated presentation identity query only

  Environment/
    ProjectWorldEnvironment/
      reusable runtime provider integration
      Epic DaySequence dependency
      provider lifecycle and focused tests
      README and BuildUnit ownership

  ProjectWorldData/
    concrete Kazan map/provider configuration
    dependency on ProjectWorldEnvironment only if the candidate is selected
```

`ProjectWorldEnvironment` is justified because it quarantines an Experimental
runtime dependency from reusable ProjectWorld. It is `Project.World.Environment`,
disabled by default, and depends on `ProjectWorld` plus Epic `DaySequence`. ProjectWorldData
enables it through its plugin descriptor and owns the concrete map selection through
a packaged config entry. No Kazan token, path, profile ID, or tuning value belongs in
the reusable module.

The minimal ProjectWorld change is a public read-only query for generated
presentation role/profile/hash identity. Do not change the presentation generator,
profile parser, schema, existing tags, generated packages, or producer fingerprints.
If the query cannot be added without changing a byte producer, that is
ARCHITECTURAL RED and requires a new R1 decision before implementation continues.

Adding the plugin also updates the authoritative World layout in
`docs/architecture/plugin_rules.md`, its normal plugin README and `BuildUnit.yaml`,
and the existing C4 model/relationship/view routes that enumerate World plugins.
Update `docs/architecture/source_of_truth.md` only where its current plugin index
requires the new owner. Do not create a second architecture document.

### Change-locality declaration

```text
owning black box:
    runtime environment provider integration

public contract:
    ProjectWorld read-only generated presentation identity
    plus ProjectWorldData-owned map/provider configuration

expected components CHANGED if selected:
    Plugins/World/Environment/ProjectWorldEnvironment
    its .uplugin, module, README, tests, and BuildUnit.yaml
    minimal ProjectWorld public identity query and focused tests
    ProjectWorldData plugin dependency and concrete environment config
    focused environment acceptance automation
    existing packaged performance runner/gate only where identity is required
    docs/architecture/plugin_rules.md authoritative World layout
    current C4 model/relationship/view routes that enumerate World plugins
    docs/architecture/source_of_truth.md plugin index if required

expected components UNTOUCHED:
    Alis.uproject
    ProjectWorld presentation schema/parser/generator and producer fingerprints
    canonical geography and all six generated layers
    generated map packages and 512/1536 runtime policy
    terrain, water, roads, vegetation, buildings, gameplay placement
    ProjectLoading, ProjectSinglePlay, character, gameplay/save time
    ProjectPCG, ProjectCinematic, HLOD, engine source
```

## Provider lifecycle contract

- Start only for an exact ProjectWorldData-owned map configuration.
- Inspect generated roles through the public ProjectWorld query and require exactly
  one current Sun, SkyAtmosphere, SkyLight, HeightFog, and VolumetricCloud fallback.
- Spawn the provider deferred, configure deterministic initial time/update mode,
  validate all required components, then switch providers atomically before the first
  accepted rendered frame.
- Deactivate only those five fallback roles. Keep PostProcess and capture cameras.
- Require exactly one DaySequence subsystem provider; reject ambiguous self-registration.
- On partial startup failure, destroy the candidate and restore every saved fallback
  state. On world cleanup, remove delegates/timers/provider references without leaks.
- Disabling/removing the plugin restores the unchanged generated fallback on next boot.
- Do not expose another generalized ALIS time service. Capture controls are provider-
  local and presentation-only.

## One-variable performance protocol

- Run paired E0/E2 dense-centre first. E2 is the first product candidate; E1 is not
  run unless E2 fails and attribution is needed.
- Freeze E2 at `06:00`, `12:00`, `18:00`, and `00:00` for matching fixed-camera
  samples and captures. Capture E0 once in its accepted static state.
- Treat those fixed-time captures as readability gates. If the existing fixed EV100
  makes a time unusable, add one explicit provider-local exposure candidate after E2;
  change no interval simultaneously, then repeat the fixed captures and dense probe.
  A broad exposure framework or ProjectWorld generator change requires a new R1.
- If E2 fails, compare E1 fixed against E0/E2. E1 failure identifies provider/bundle
  overhead; E1 pass with E2 failure identifies playback, moving-light, or VSM cost.
- Optimize only the measured owner. Use native DaySequence timing and VSM diagnostics
  before considering provider component changes. Do not reclaim budget through an
  unrelated blanket Lumen, Nanite, foliage, resolution, or quality downgrade.
- After one focused optimization, repeat only the dense-centre proof it invalidated.
  Repeated or cross-owner failure routes to scientific debugging and a new R1 boundary.
- If E2 passes dense-centre, inspect temporal cadence with a short authenticated MCP/PIE
  observation or time series. Run E3 `0.25 s` only when E2 visibly steps, then repeat
  dense-centre and the same temporal diagnostic.
- Run the full centre/diagonal/perimeter/backtrack/high-speed product traversal only
  for the surviving moving candidate, from the same start time and cycle rate.
- Require dense-centre and then overall **and every full route** frame p95
  `<= 16.67 ms`, zero streaming failures, the physical RTX 4070, D3D12, High,
  2560x1440, and at least the existing sample floor. Record Frame p95/p99/max,
  Game/Render/GPU p95, memory, package delta, readiness, cell transitions, driver,
  executable hash, candidate ID, initial time, cycle rate, and update interval.
- Treat Virtual Shadow Map invalidation views/stats and Unreal Insights as diagnostic
  only. They explain a failure but do not replace the packaged gate.
- Animated weather/cloud policy, Sky Light capture policy, Lumen, Nanite, and fog
  quality are separate future candidates. Exposure is admitted here only when the
  fixed-time evidence proves it necessary, and remains a one-variable comparison.

The runner should package one Development binary where possible and switch only the
non-Shipping candidate mode by validated command line. This isolates runtime behavior;
measure the cook/package delta separately against the last accepted static artifact.
The selected production mode comes from ProjectWorldData config. The funnel owns one
paired dense probe plus one replacement after a focused fix; only its survivor consumes
the existing full traversal and release gates.

## Proof traceability

| Invariant | Acceptance surface | Cheapest proof | Final proof | Stop condition |
|---|---|---|---|---|
| Config selects no wrong map | parsed ProjectWorldData entry | parser test with wrong/duplicate maps | packaged real menu -> Kazan receipt with candidate ID | wrong or ambiguous activation |
| Exactly one provider contributes | runtime actors/components and DaySequence subsystem | synthetic world lifecycle test | packaged ownership receipt sampled during traversal | duplicate/missing provider |
| Fallback is fail-safe | saved component states | forced spawn/config failure test | disable/relaunch proof returns to E0 | fallback not restored |
| Genuine cycle is present | provider clock and visible environment state | set/freeze/resume/advance automation | packaged moving-provider receipt plus temporal visual evidence | implementation cannot close on E0/E1 |
| Time controls are local and correct | provider clock state | dependency and state automation | packaged advance/set/freeze/resume receipt | gameplay clock dependency or drift |
| Generated authority is untouched | six layer manifests/artifacts plus map packages | pre/post hash test | authority audit with byte-identical hashes | any generated/package mutation |
| Performance remains accepted | real rendered product route | paired dense-centre probe | survivor-only packaged Development traversal on physical 4070 | optimize/research until dynamic candidate and 60 FPS both pass |
| Cook and product route work | staged Shipping artifact | cook-reference validation | Shipping menu -> ProjectLoading -> Kazan correctness | missing class/content or route regression |
| Visual result is usable | authenticated time/viewpoint captures | image freshness/dimension checks | agent MCP inspection plus operator/reviewer judgment | black/stale/broken or no meaningful gain |

## Automated, MCP, and human evidence order

1. Run exact L0/L1 lifecycle, config, provider, rollback, and receipt tests.
2. Build/package through launcher-engine project scripts; never build engine source.
3. After focused tests pass, reviewer performs R2 before expensive rendered gates.
4. Run paired E0/E2 packaged Development dense-centre and E2 fixed-time captures.
5. On E2 performance failure, run E1 only for attribution, optimize one measured
   bottleneck, and repeat the invalidated dense probe.
6. On E2 performance success, run a short MCP/PIE temporal diagnostic. Run E3 and
   repeat dense/temporal proof only when E2 visibly steps.
7. Run the full existing packaged traversal only for the surviving moving candidate.
8. Perform the final live Unreal MCP audit on the actual Kazan product world: query
   provider count/class, fallback component state, time advance/freeze/resume, exact
   viewpoint transforms, and `06:00/12:00/18:00/00:00` captures.
9. Cross-check MCP evidence against candidate ID, configured time, logs, and packaged
   receipts. MCP/Editor FPS remains DIAGNOSTIC / NON-AUTHORITATIVE.
10. Operator confirms presentation, then run one selected Shipping product-route/cook
    proof and the final unchanged-authority audit.

Automated receipts prove contracts and performance. MCP proves that the live editor
state visually matches those contracts. Human review decides whether the visual gain
is worth the measured cost. None substitutes for another.

## Ordered future implementation actions

1. Start only after this research receives R1 PASS and is selected from all seven
   concerns. Add failing config/identity/provider lifecycle tests and a forced-fallback test.
2. Add the read-only ProjectWorld presentation identity query without touching its
   generator or profile contract.
3. Create `Plugins/World/Environment/ProjectWorldEnvironment` with a tiny derived
   `ASunMoonDaySequenceActor` used only to expose validated provider settings. Do not
   create custom sequence assets or celestial logic.
4. Add ProjectWorldData-owned Kazan configuration and descriptor dependency. Do not
   edit `Alis.uproject`.
5. Add the plugin README and BuildUnit; update the authoritative plugin layout and
   existing C4/source-of-truth routes that enumerate World plugins.
6. Implement atomic activation, state restoration, teardown, and observable logs.
7. Extend existing acceptance surfaces with environment candidate identity; add only
   the focused environment behavior/capture gate that the existing harness cannot own.
8. Run focused L0/L1 and request R2 before any rendered candidate gate.
9. Execute E0/E2 through the funnel; use E1 only for failure attribution and E3 only
   for proven visible stepping. Optimize one measured bottleneck at a time.
10. Run full performance, final MCP, Shipping product route, locality audit, and owner
    cleanup only for the surviving moving candidate.

## Scratch and rollback

The environment runner owns `tmp/world/environment_candidates/<run-id>/`. In `finally`
it must stop child processes and delete its package copies, snapshots, transient config,
CSV staging, and non-selected screenshots on both pass and failure. Retain only small
review receipts and selected evidence under `Saved/Validation/WorldEnvironment/`; do
not create a new long-lived rollback copy. Finish through the existing bounded
`cleanup_workspace.ps1 -Apply`, which must not delete unrelated owners' scratch.

Rollback is plugin/config removal plus relaunch. Canonical data, generated layers,
map packages, and the existing static presentation remain byte-identical.

## Stop conditions

- A dynamic candidate that shows no coherent visible day/night progression is rejected.
- A p95 or streaming failure blocks that candidate and triggers focused attribution
  and optimization; it does not waive either day/night or the 60 FPS target.
- Unreliable DaySequence cook/Shipping stops the DaySequence provider attempt and
  returns this concern to research for another dynamic provider.
- Provider startup/teardown crashes, leaks, or cannot restore fallback atomically.
- Integration requires hardcoded Kazan logic, engine modification, generated-map
  mutation, presentation-schema expansion, or broad ProjectWorld refactoring.
- A required optimization crosses an owner declared untouched. Stop and revise R1.

Static E0 remains operational rollback for every stop condition, but no static or
fixed-time state may be promoted as completion of the essential runtime cycle.

## Research close-out

- [x] Reviewer produced an environment-only proposal and UE option comparison.
- [x] Agent verified installed UE 5.8.1 source, current ALIS owners, performance
  evidence, fingerprint locality, plugin rules, and test envelopes.
- [x] Record the bounded candidate design, hierarchy, ordered actions, rollback,
  cleanup, and exact automated/MCP/performance evidence.
- [x] Same reviewer rechecks the corrected requirement, ownership, and candidate
  tournament as R1 and returns PASS/PATCH/BLOCKER.
- [x] Set `RESEARCH COMPLETE / IMPLEMENTATION NOT SELECTED`; implement nothing.
- [x] Keep this todo in backlog after research. Move it to current only if selected
  for implementation, and to done only after implementation and verification pass.
