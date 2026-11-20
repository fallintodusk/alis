# ALIS 3.0.0 lighting, visibility and optional daylight cycle

**Status:** CURRENT - P1 PASS, including both-city Development performance and protected-state restoration. P2 deferred; P3 execution handed to the approved Time/Light task. Shipping/installed R5 remains open.
**Scope:** World presentation and its runtime consumers; Terrain development is closed.
**Stable owners:** World presentation realization/contract and render configuration.

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

Make generated Kazan and Manhattan readable and attractive from normal player
views at measured rendering cost; qualify a small native geographic daylight
cycle only if its disposable proof warrants inclusion in 3.0.0.

## Authority register

### Operator decisions

- **D1 [SUPERSEDED IN PART by D2 - investigation timing only]:** Create a current
  lighting stub for 3.0.0 with old-map experience, recent presentation evidence,
  future daylight options and optimized Lumen setup; do not investigate yet,
  decide Terrain acceptance first.
  - Effect: single lighting owner and Terrain-first sequencing remain binding.
  - Reason: finish geometry before lighting work.
  - Source: operator's lighting-stub request, 2026-11-16.
- **D2 [SUPERSEDED IN PART by D3 - P1 execution only]:** Investigate the attached lighting proposal using investigate-change.
  - Supersedes: D1's no-investigation timing; Terrain development is already closed.
  - Effect: produce one reviewable implementation plan, without implementing it.
    The attached recommendations are evidence/proposals, not provider adoption.
  - Reason: not stated.
  - Source: current operator request, 2026-11-16.

- **D3:** Execute P1 implementation and verification autonomously on the qualified
  joint Terrain candidate: compare Ready-runtime EV12, a brighter fixed control
  and bounded Histogram; Local Exposure only if necessary. Implement the smallest
  successful policy and verify camera behavior, visibility, GPU cost, clean PIE
  shutdown, deterministic save/reload and protected-state restoration.
  - Supersedes: D2's investigation-only boundary for P1 only.
  - Effect: no new generic research, Terrain changes, renderer tuning, production
    promotion, P2 or P3 implementation. Report P1 PASS/PATCH with matched evidence.
  - Reason: enough native evidence exists to move to actual visual proof.
  - Source: operator's implement-approved-change request, 2026-11-16.

- **D4:** Run one bounded material-only road/concrete comparison, using existing
  ProjectMaterial/ProjectTexture contracts, Ready textures, matched near/middle/far
  views and one parameter variant per material. Adopt only an obvious improvement;
  otherwise retain defaults. Water appearance is retained by operator choice.
  - Effect: this narrowly extends D3's material hold; Terrain, Water, Histogram,
    global renderer settings and building geometry remain unchanged. No World
    regeneration, new patterns/families, facade work or new todo.
  - Source: operator's bounded tiling-comparison request, 2026-11-16.
- **D5:** Require lighting quality/performance balance and explicit performance
  acceptance, accounting for Lumen's potential cost.
  - Effect: enforce joint visual and performance assessment against the existing
    city budgets. Optional tuning may be deferred; performance acceptance may
    not be omitted or inferred from good screenshots. This does not authorize a
    renderer rewrite, budget change, P2/P3 adoption or packaging.
  - Reason: retain attractive lighting at acceptable runtime cost.
  - Source: operator's quality/performance clarification, 2026-11-17.
- **D6:** Execute the approved lighting change using implement-approved-change.
  - Effect: finish the remaining P1 correction and quality/performance checkpoint
    within the existing owner and authority boundaries. Preserve production,
    release and optional-feature gates; no new provider or renderer rewrite.
  - Source: operator's implementation request, 2026-11-17.

- **D7:** Execute the supported load-aware performance gate autonomously.
  - Effect: prepare a restoring candidate and the necessary local Development
    test package, then run the existing three-run/host-load acceptance path for
    both cities. Retain the current quality and acceptance policy; no automatic
    performance waiver, permanent production selection, Shipping certification,
    signing, publication, staging or commits.
  - Source: operator's autonomous gate execution request, 2026-11-17.

- **D8:** Implement the reviewed world-local Time/Light todo through
  implement-approved-change (2026-11-18).
  - Supersedes: D7's P3 deferral only. P1 stays accepted and P2 stays deferred.
  - Effect: [Time/Light execution](20261118-1151_world_plan_time_service_daylight_controls.md)
    owns clock, demo UI and integrated daylight; no parallel private P3 clock.
  - Reason: operator-approved separated environmental owners and playable controls.
  - Source: current operator invocation and approved revised plan.

### Operator gates

- **Q1 [OPEN - NON-BLOCKING]:** Does the optional runtime cycle merit selection
  for 3.0.0 after its native visual/performance comparison? Default: A3. This
  does not block the visibility correction or require a new question per test.

### Working assumptions

- **A1 [ACTIVE]:** Preserve static daylight for the first visibility milestone;
  compare native controls before extending the presentation contract.
- **A2 [ACTIVE]:** SunPosition is the preferred solar calculation candidate;
  its runtime owner remains conditional on the installed Day Sequence reuse check.
- **A3 [ACTIVE]:** Dynamic cycle/scalability improvements may be deferred; do not
  make them prerequisites for correcting current readability. No new multiplayer
  clock, historical civil-time service or weather requirement is assumed.
- **A4 [ACTIVE]:** The installed-source review satisfies A2's reuse check for the
  current flat solar/time scope. Prefer SunPosition with the smallest runtime
  binding; no second provider prototype unless a demonstrated native capability
  gap changes that cost comparison. Do not build a custom sequence/curve engine.

## Non-goals

New Ground/Water models, fountain detail, source acquisition, building/PCG work,
engine upgrades, weather, city-light generation, MegaLights adoption, external
clock services, production selection, source freeze, R5/R6 and Git writes.
No changing the existing ISM/HISM ray-tracing crash mitigation in this task.

## Verified evidence

| Fact / finding | Exact owner / evidence and qualification |
|---|---|
| Configured launcher engine is 5.8.3, CL 58210709 | Resolve through `scripts/config/Resolve-UEConfig.ps1`; read `<UE_PATH>/Engine/Build/Build.version`. Engine internals are read-only. |
| One native outdoor environment already exists | [Presentation Apply](../../Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationRealization.cpp): six role-owned actors, stable identities, always-loaded unbound PostProcess and fixed capture cameras. Sun 50,000 lux, Sky Light real-time capture, volumetric fog; values originate in the [shared profile](../../Plugins/World/ProjectWorldData/Data/Presentation/kazan_representative_v1.json). |
| D2 baseline: manual EV12; P1 adds bounded gameplay Histogram | `ConfigureFixedExposure` applies physical camera settings to both the volume and generated capture cameras. The [parser](../../Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationProfile.cpp) and [schema](../../Plugins/World/ProjectWorld/Data/Schemas/project_world_presentation_profile.schema.json) admit bounded gameplay Histogram; generated capture cameras retain fixed exposure. A volume-only Histogram change can be masked by camera overrides. |
| Dark actual play, brighter diagnostic comparison | `tmp/world/terrain_execute_d44/presentation-comparison-r7/comparison.json` pins matched `play-context-ev12.png` / `play-context-ev8.png` and support/camera JSON. Same actual PIE pose, Ready material pixels. EV8 improves this shaded view; no global exposure policy, new Hero proof or clean shutdown is established. The subsequent PIE teardown crashed. |
| Reflection comment is wrong | [DefaultEngine.ini](../../Config/DefaultEngine.ini) has `r.ReflectionMethod=1 ; SSR`; installed `EngineTypes.h::EReflectionMethod` is None=0, Lumen=1, ScreenSpace=2. This is configured Lumen, not proof of the effective runtime path at every quality tier. |
| Low shadow tiers are forced expensive; Medium GI needs measurement, not invention | [DefaultScalability.ini](../../Config/DefaultScalability.ini) fixes directional VSM rays/samples at 8/4 and permits Lumen GI at all tiers. Installed BaseScalability already enables Medium Lumen via `r.Lumen.FinalGatherMethod=0`; Medium reflections disable Lumen reflections in favor of SSR. Repeating Allow=1 does not by itself defeat that existing Medium gather. |
| Hardware path excludes all ISM/HISM geometry for stability | DefaultEngine records the streamed-geometry SBT/TDR incident and `r.RayTracing.Geometry.InstancedStaticMeshes=0`. Do not silently re-enable it while comparing lighting. |
| Solar API is available without a second sky bundle | [Alis.uproject](../../Alis.uproject) enables SunPosition. Installed `Runtime/SunPosition/SunPosition.uplugin` declares a runtime module; `SunPosition.h::USunPositionFunctionLibrary::GetSunPosition` accepts numeric offset/DST/date/time. It returns void and logs/returns on invalid dates; callers must validate and reject invalid/nonfinite results. No IANA-zone resolution is provided. |
| Day Sequence has been evaluated | Installed `Experimental/DaySequence/DaySequence.uplugin` and `DaySequenceActor.h` provide native runtime clock/sequence controls. The default actor adds lighting components; customized reuse adds actor/sequence bindings. Those parts are unnecessary for the present existing-actor solar calculation. This integration-cost finding, not maturity alone, supports A4 under [Native Engine First](../../docs/architecture/principles.md#native-engine-first). |
| North is negative Unreal Y | [Coordinate mapping](../../Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldCoordinateMapping.cpp): easting -> +X, northing -> -Y, metres -> centimetres. Solar azimuth, actor-forward direction and projected-grid versus true-north convergence need explicit calibration. |
| Geographic placement already has an owner | [Generated geometry](../../Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldGeneratedGeometry.cpp) persists one role/grid-bound GeoReferencing actor. Installed `GeoReferencingSystem.h` exposes geographic conversion and ENU vectors; qualify these before duplicating city coordinates or inventing a north-offset table. |
| Render labels and source-version distinction | The live reflection label and [render overview](../../docs/config/render/render.md) are corrected without changing renderer values. Local GameDefaultMap is MainMenuWorld, EditorStartupMap is City17 and auto-exposure default is off. The reviewer reports different public-main map defaults; that is a source-version difference, not a demonstrated map bug. Future evidence must pin the exact local commit, dirty file hashes and installed build. Legacy [SunSky notes](../../docs/config/render/lighting-bp-sunsky.md) remain map-specific references. |
| Local Exposure is already a native option | Installed `Scene.h` declares its settings; `Scene.cpp` initializes Bilateral with highlight/shadow contrast scales 1.0. The actual blended player settings still need readback. Do not assume Local Exposure is absent/disabled; compare one declared treatment against the measured baseline only if global exposure cannot retain both shade and highlights. |
| Moving-sun cost includes shadow invalidation | Installed `VirtualShadowMapClipmap.cpp` keys the per-light cache on LightDirection; `VirtualShadowMapCacheManager.cpp::Update` invalidates changed keys. Continuously moving lights use the uncached path. Measure native VSM statistics, shadow cost and visible motion when choosing update cadence. |
| A moon is supported, not implemented here | Installed `DirectionalLightComponent.h` supports atmosphere indices 0/1. Current World ExpectedClasses lists six roles and removes unknown generated presentation roles. One conditional Moon role needs the same ownership/identity/removal path, not an unmanaged extra light. |

**Unverified:** final Shipping/installed R5 performance, complete traced coverage,
nighttime quality and optional provider lifecycle/cooked behavior. P1 reference-
resolution, normal generation/lifecycle and D7 packaged Development checks are
recorded below; no installed or R5 certification is claimed.
No existing simulation-clock owner was found in the inspected World/runtime,
Foundation, Systems and Gameplay sources; recheck the consumed owner before adding one.

## Current architecture and source of truth

World Editor creates and owns persistent presentation actors and their profile
identity. [ProjectWorld runtime](../../Plugins/World/ProjectWorld/Source/ProjectWorld/ProjectWorld.Build.cs)
is runtime-safe and has no SunPosition dependency yet; it cannot depend on
ProjectWorldEditor. Concrete settings belong to ProjectWorldData. Existing World
transactions own saved-output mutation, recovery and adoption.

- Runtime appearance and evidence: [paired review](../../tools/World/VisualVerification/README.md#paired-geometry-and-presentation-review),
  [Ready procedural outputs](../../Plugins/Resources/ProjectTexture/docs/pattern_generation.md#contract).
- Proof depth: [World test layers](../../docs/testing/world_pipeline_layers.md).
- Budgets: [Kazan](../../Plugins/World/ProjectWorldData/Data/Profiles/Budgets/kazan_territory_v1.budget.json)
  and [Manhattan](../../Plugins/World/ProjectWorldData/Data/Profiles/Budgets/manhattan_showcase_v1.budget.json).
  Retain High/reference-hardware p95 16.67 ms and explicit INCONCLUSIVE policy.
- Runtime/lifecycle routes: [World host](../../scripts/ue/world/README.md),
  [serialized Editor/shutdown rules](../../docs/ue_engine/mcp_editor_control.md).

Expected implementation CHANGED: presentation profile/schema/parser/realization,
affected capture/runtime consumers and tests; renderer config only for an adopted
measured option. Optional cycle may add the smallest runtime binding/native dependency.
UNTOUCHED: canonical/source identities, Terrain/Water/structures and materials,
regional Hero evidence, shared ISM crash guard, generated geometry and release gates.
Unexpected propagation requires boundary review, not broader regeneration.

## Problem and root cause

The retained actual-play view demonstrates insufficient shaded-ground readability;
EV8 is diagnostic evidence of exposure sensitivity, not a proven two-city solution.
Cold Editor missing material pixels were a separate execution-envelope artifact.
The presentation contract cannot express adaptation or geographic runtime time;
it writes a static sun and manual exposure. Renderer comments/documentation also
misdescribe current configuration. Performance/quality gains from changing these
settings remain unmeasured.

## Decision

### Premise / KISS gate

Correct visibility through the existing presentation owner first. Use native
live controls for comparisons, not repeated C++ rebuilds. Keep the six outdoor
roles as the baseline; add no service, process, port, provider registry or
environment framework. Generic lighting/provider research is complete. Further
inspection is limited to an exact runtime failure, not a new documentation sweep.
The static milestone knowingly gives up automatic time-of-day changes.

Quality and performance are a joint acceptance gate. Preserve the existing High
reference baseline, actual output/internal resolution and upscaler settings for
paired comparisons in both cities. Record total frame p95, Game/Render/GPU time
and memory against the linked city budgets; a GPU-only result or capped capture
cannot establish frame-budget acceptance. If the baseline already misses the
budget, identify the responsible owner and retain an explicit unresolved result;
a small lighting delta does not waive the overall target. Local visual progress
can still be reported without claiming installed-game/R5 performance acceptance.

Attribute lighting cost before tuning: Lumen GI/reflections, VSM shadows,
Sky Light capture, clouds/fog and exposure/postprocess where measurable. Compare
warm settled views and moving shade/bright transitions; keep cache/startup work
separate. If daylight is selected, include moving-sun and time-transition cost.
Change one measured contributor at a time and retain matched normal-render
images. Reject a speed gain that relies on unreadable shade/night, lost important
highlights, materially worse reflections, unstable indirect light or shadow
stepping. No silent resolution/tier reduction or crash-guard bypass to pass.

| Viable option | Parts added / avoided | Recommendation |
|---|---|---|
| Qualified static exposure | Existing profile/actors only; no runtime clock | First milestone; sufficient if it solves present readability |
| SunPosition plus thin runtime binding | One opt-in world-scoped consumer/time state, existing plugin dependency; no custom astronomy or sky bundle | Recommended P3 proposal (A4); runtime evidence still required |
| Customized Day Sequence | Native actor/time/sequence binding plus needed assets; can avoid custom clock/curve code | Evaluated alternative, no separate prototype by default. Revisit for a demonstrated sequence/modifier/synchronization requirement before implementing an equivalent |

Keep one time source and one owner of light/exposure updates. Prefer a fixed UTC
calendar plus native elapsed game-time scale/pause for the isolated prototype;
calling the solar API with offset 0/DST false avoids inventing civil-zone rules.
Use the existing georeference for geographic position and true-north conversion,
or a verified derived serialized binding when runtime access requires it. Do not
maintain a second independent per-city latitude/longitude/north authority.
If local civil controls are selected, validate an explicit offset/DST policy,
including double-DST avoidance. Do not derive civil time or weather from longitude.
Do not add GameState replication or cross-world persistence for hypothetical use;
if an actual adoption consumer requires them, identify its existing authority and
qualify that boundary before claiming multiplayer or persistent-time support.

## Required invariants

1. Accepted canonical heights, geometry/collision and unselected actors stay unchanged.
2. Generated roles remain unique, always loaded where required, deterministically
   regenerated; authored lights remain protected. The six-role baseline permits
   one optional Moon only if required by the accepted night candidate, integrated
   in the same role lifecycle with disable/removal and cost proof. No default
   SunSky/Day Sequence environment stacked over the World-owned one.
3. Gameplay exposure and explicit deterministic captures consume one declared
   policy. Manual camera overrides cannot silently defeat an adaptive gameplay mode.
4. Runtime code has no Editor dependency. Missing/ambiguous roles or invalid
   date/location fail observably before updates; no partially advanced or stale result.
5. Settings/quality comparisons retain the ISM/HISM guard and shared High baseline;
   no permanent global renderer downgrade without measured and visual acceptance.
6. Only actual Ready Game/PIE/GamePreview outputs certify appearance. Diagnostics,
   numeric luminance proxies and cold Editor images cannot certify player readability.
7. Optional day/night must demonstrate dawn/noon/sunset/night and continuous
   transitions, not merely rotate a constant-intensity sun below the horizon.
8. Local iteration remains reversible and measured; no candidate promotion,
   full-city generation or packaged release claimed from this task's local proof.
9. Optional P2 tuning does not make performance acceptance optional. Report visual
   quality and performance separately, and require both for an unqualified
   lighting acceptance claim. Final installed-game certification remains R5.

## Implementation tasks

- [x] **P1 baseline:** Authenticate the verified joint Ground/Water/Pier candidate,
  not the restored older production base. In one restoring saved-candidate replay,
  pin the local commit, dirty-file hashes, active canonical/World identity and
  installed engine build. Record both-city player controls: sunlit ground, deep
  shade, Water/shore and skyline, plus a covered location only if already available.
  Bind input hashes,
  camera/FOV/resolution, actual Ready pixels, effective CVars/quality and exposure
  readback, including actual Local Exposure and player/capture camera overrides.
  Reuse D46 images as diagnostics, not a new full baseline. Separate
  Game/CPU, Render and GPU pass cost, frame p95 and memory at the budget envelope.
- [x] **P1 comparison:** One bounded batch: retained EV12, one less-dark fixed
  control informed by EV8, and bounded native Histogram. Keep materials, geometry,
  sun and renderer path fixed. Inspect highlight/shadow detail and a moving
  shade-to-sun transition; record settling time/flicker and costs. Tune native
  controls first; do not assume globally setting EV8 fixes two cities.
- [x] **P1 conditional Local Exposure - not required by inspected views:** Only if fixed and bounded Histogram cannot
  retain shade and highlights together, compare one native Local Exposure treatment
  with that same measured global policy. Hold other settings/poses constant; test
  moving edges, halos, lost contrast/flicker, highlights and GPU cost. This is one
  conditional A/B, not a new provider, broad tuning matrix or claim of adoption.
- [x] **P1 implementation:** Select the smallest successful policy. If fixed
  exposure suffices, avoid adding unused adaptation fields. If Histogram wins,
  extend the single presentation schema/parser/realization and all actual camera
  consumers together, with explicit deterministic capture behavior. Update real
  profiles/fixtures/tests in one pass, without obsolete compatibility scaffolding.
  Persist Local Exposure controls only if its conditional comparison wins. Add a
  permanent regression only for the demonstrated contract failure.
- [x] **P1 checkpoint:** Return matched normal player screenshots, transition
  footage, readbacks, dirty outputs and cost before expanding. If unclear, allow
  one focused owner fix/replacement batch, then report the exact unresolved owner.
  No repeated source/geography research or open-ended brightness search.
- [x] **P1 final cold check / performance disposition:** Verify the configured
  cache offset in a cold native process and retain the existing exposure
  save/reload/no-op regression. The local Editor frame budget is missed in both
  controls; do not infer an installed-game result or lower Lumen quality from it.
  Retain the existing real-runtime/R5 performance obligation.
- [x] **P1 packaged Development performance:** D7 runs the supported load-aware
  three-run campaign in both cities on one authenticated package. Both pooled
  results pass the base budget without a host allowance. Preserve raw samples,
  correctness, envelope, package/source identity and normal-exit evidence. Final
  Shipping/installed R5 remains separate; restore the protected World state.
- [x] **Annotation correction:** Reflection labels and stale duplicated render
  configuration are corrected in this review patch; functional values unchanged.
- [ ] **P2 conditional scaling:** Compare one renderer dimension at a time: effective HW/SW tracing with coverage verified, installed
  Medium GI/reflection behavior, lower VSM tiers, then cloud/volumetric-fog cost
  only where measured material. Preserve High appearance and the crash guard.
  No full Cartesian benchmark, untested CVar recipe or mandatory scaling project
  before visibility can close. Adopt/defer each option explicitly.
- [ ] **P3 optional native cycle:** Use the evaluated SunPosition/thin-binding
  proposal (A4). Reopen provider comparison only for a concrete unmet capability;
  no new clock/curve framework or routine second prototype. Prove one disposable Kazan
  setup: opt-in runtime owner, native solar math, explicit north alignment and
  deterministic set-time/pause/rate. Bind location/north to the existing georeference;
  use project-owned data, not legacy City17 BP or a second city-coordinate list.
  Actual night appearance/intensity/exposure must be qualified, not inferred.
  If night needs a Moon, add at most one role-owned atmosphere light with a
  distinct index, checked identity, no-op/save/reload and disabled-role cleanup;
  do not imply astronomical moon accuracy or add city-wide night lights.
- [ ] **P3 consumers:** Check equivalent UTC/local-offset inputs, date rollover,
  negative-Y north/cardinal light direction, frozen/repeated time, sunrise/noon/
  sunset/night and smooth progression. Verify Manhattan with its own location,
  runtime startup/map switch/teardown, save/reload of authored initial settings,
  disabled fallback and missing/duplicate-role refusal. Compare a frozen sun with
  native continuous and bounded-cadence updates; record VSM cached/rendered pages,
  shadow GPU time, p95, visible stepping and accelerated-time movement. Per-frame
  light rotation is not the default; choose cadence from measured quality/cost,
  retaining correct shadow invalidation. Disable debug visualizers for final timing.
  Measure clouds and GPU Sky Light capture; no manual RecaptureSky per tick.
- [ ] **Close:** Record accepted visibility and adopt/defer optional P2/P3, migrate
  durable knowledge to owners below, review locality and restore scratch state.
  Production selection and installed/R5 proof remain separate authorized work.

## Test-first and verification plan

| Case / proof type | RED or baseline | Required GREEN |
|---|---|---|
| Player visibility - acceptance | Matched Ready-runtime EV12 deep shade is unreadable | Same normal player poses show navigable contact/detail without washed-out sunlight/sky; inspect motion and exposure recovery. Luminance/clipping measurements are supporting proxies, not a human visual PASS. |
| Exposure contract - permanent regression if changed | Current parser refuses an adaptive policy; current ActorLifecycle assumes manual volume/cameras | Faithful profile covers accepted mode, invalid/nonfinite/inverted bounds and actual camera/volume policy; explicit fixed capture remains deterministic. Keep tests coarse by failure class, not one test per scalar. |
| Conditional Local Exposure - acceptance | Global comparisons cannot retain both shade/highlight detail | One matched native A/B preserves both without halos/flicker or an unqualified cost; no permanent scalar-value test just for tuning. |
| Moving-sun cost - acceptance if selected | Static sun does not exercise VSM light-direction invalidation | Frozen/continuous/cadenced measurements and visible smooth shadows justify the selected update rate; correct invalidation retained. |
| Runtime solar/binding - feature regression if selected | No registered runtime controller/time behavior exists | Real Epic calculation plus independent cardinal/north placement expectations, bad-date/role refusal with no mutation, pause/rate/rollover and enabled/disabled lifecycle. Validate finite provider results. |
| Quality/performance - joint acceptance | Actual inherited/overridden CVar readback and pinned High cost | Both-city matched quality passes and total-frame/Game/Render/GPU/memory measurements satisfy the existing budget at the same declared resolution/upscaler/tier. Attribute Lumen and other lighting costs before tuning. A changed tracing path requires its own scene-coverage proof; P1's unchanged guarded renderer is not new tracing-coverage certification. A capped capture or unresolved frame budget stays INCONCLUSIVE. Installed-game certification remains R5. |
| Locality/recovery - permanent guard where needed | Existing presentation ActorLifecycle preserves geography and stable roles | Changed presentation only, deterministic no-op/save/reload and refusal/recovery; no canonical or geometric regeneration. If Moon is selected, prove role addition, duplicate refusal, disable/removal and unchanged authored lights. Recheck only consumers of changed identities. |
| Appearance/shutdown - real route | Cold textures and prior PIE teardown failure invalidate substitute evidence | Actual Ready runtime captures and normal player transition, serialized safe lane, PIE ended before host exit, clean native shutdown and exact protected-state restoration. |

Reuse [presentation tests](../../Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/Tests/ProjectWorldPresentationTests.cpp)
and the [capture reader tests](../../tools/World/VisualVerification/tests/test_verify_capture.py).
Initial exact native checks after implementation, through the project wrapper:

```powershell
scripts/ue/test/unit/iterate.ps1 -TestFilter Project.World.Realization.Presentation.ProfileContract
scripts/ue/test/unit/iterate.ps1 -TestFilter Project.World.Realization.Presentation.ActorLifecycle
scripts/ue/test/unit/iterate.ps1 -TestFilter Project.World.VisualEvidence.RenderModeContract
```

Add only missing focused runtime tests when P3 is selected. Build with
`scripts/ue/standalone/build.ps1`; no direct Engine Build.bat. Use small fixture
L0/L1 and affected normal-RHI local views first. Expensive gates follow the linked
layer contract and existing initial-plus-one-replacement budget. Actual cooked
module loading, final installed visibility/performance and human walkthrough
remain required for adopted release behavior under R5, not proven here.

## Documentation plan

- World owns runtime/presentation role binding, identity, opt-in time behavior and
  capture-vs-gameplay exposure semantics: update its existing territory contract
  and architecture view only for implemented changes.
- Render configuration owns effective renderer/scalability policy and old-map
  scope. The overview now links actual config owners instead of copying an old
  INI; legacy notes remain scoped references. Future adopted behavior updates only
  its current owner; do not copy concrete profile values into prose.
- VisualVerification owns actual Ready runtime/evidence interpretation; update
  only a changed capture contract or a demonstrated new trap. Keep future-only
  state and investigation receipts here, not in stable current-state docs.
- No stable owner, source, schema or test may link back to this todo. The release
  plan keeps its existing task route; no second lighting todo/router is needed.

## Rollout and rollback

Compare native live parameters on disposable/transactionally restored worlds;
keep profile/config/generated-asset and canonical inventories before/after.
Adopt saved presentation only through the existing World owner with explicit
candidate selection. Keep High/static controls for rejection. Do not save runtime
clock mutations as authored defaults, bypass the safe lane, or disable the ISM
mitigation. A rejected comparison restores its exact inputs and owned artifacts.

## Completion criteria

P1 has inspected matched Ready player views in both cities, transition/cost proof,
focused regression/realization checks, clean runtime teardown and unchanged
protected geography. P2/P3 each have an explicit adopt/defer disposition; deferred
work requires no gratuitous prototype, while adoption needs measurements. Selected
P3 has runtime-safe/native reuse, identity and time/orientation/lifecycle proof.
No qualitative acceptance is inferred from a receipt status or screenshot count.
Durable knowledge resides with its owner; no false production or R5 acceptance.
Under D5, attractive images alone cannot close performance acceptance, and a
faster frame cannot justify an unaccepted quality loss. Optional P2/P3 can be
deferred while retaining the existing release performance obligation.

## Review record

### 2026-11-16 - proposal investigated against local 5.8.3

- D2 supersedes D1's timing clause. Only this todo changes; staged work is preserved.
- Confirmed native outdoor ownership, manual exposure/camera coupling, reflection
  comment error, existing Medium path and runtime SunPosition API. Refuted the
  direct-Kazan default-map claim and maturity-only rejection of Day Sequence.
- No broad tier rewrite, custom clock or runtime provider is selected by research.
- Existing `test_verify_capture.py`: 13 tests PASS in 0.204 s; this proves capture
  reader contracts, not current lighting, performance or native lifecycle.
- No Editor, build, generation, source acquisition or packaged run performed.

### 2026-11-16 - external review reconciled with local source

- Accepted conditional Local Exposure, explicit moving-sun/VSM costs and a
  conditional ownership-safe Moon. Native references are settled; remaining
  work is authenticated actual play, readbacks, bounded comparisons and small fixes.
- A4 records the simpler native integration recommendation; no routine Day Sequence
  prototype. This is not a claim it cannot reuse the current actors or a maturity
  waiver for Native Engine First. New native capability needs still trigger that rule.
- Narrowed the earlier default-map refutation: public main may differ from this
  checkout. Local values are verified; remote-main settings were not re-fetched.
  Future receipts pin exact source/config/build rather than declaring one universally current.
- Corrected the live reflection comment without changing its numeric value and
  replaced the obsolete render-config duplicate with links to current owners.
  No functional renderer/exposure setting, runtime provider or production authority changed.
- Verification: INI assignments unchanged; documentation ASCII, links/TOC and
  focused diff checks pass. HEAD and staged-entry identities preserved. No runtime
  test was repeated for annotation-only changes; native lighting remains unverified.

Official sources checked alongside installed code:

- [Lumen performance and Medium paths](https://dev.epicgames.com/documentation/unreal-engine/lumen-performance-guide-for-unreal-engine?lang=en-US).
- [SunPosition native API](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/SunPosition/USunPositionFunctionLibrary).
- [Day Sequence native time-of-day options](https://dev.epicgames.com/documentation/unreal-engine/day-sequence-time-of-day-plugin-for-unreal-engine?lang=en-US).
- [Auto exposure controls](https://dev.epicgames.com/documentation/unreal-engine/auto-exposure-in-unreal-engine?lang=en-US).
- [Sky Light GPU capture/time slicing](https://dev.epicgames.com/documentation/unreal-engine/sky-lights-in-unreal-engine?lang=en-US).
- [VSM light-cache invalidation](https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine).
- [Atmospheric Sun/Moon light slots](https://dev.epicgames.com/documentation/en-us/unreal-engine/sky-atmosphere-component-in-unreal-engine).

Online pages can advance beyond the installed engine; qualify properties/symbols
against resolved engine source at implementation time. Web failures on some direct
API/versioned URLs were resolved through official indexed pages; no third-party
performance result is treated as ALIS evidence.

### 2026-11-16 - P1 implementation and bounded native proof

- Same authenticated joint saved World: `tmp/world/lighting_p1/native-r2/run-result.json`.
  Twenty-four matched normal Hero views cover four poses per city and three arms.
  EV12 loses shaded detail; EV8 washes out the Pier waterfront and Kazan banks.
  Bounded Histogram is the leading policy; no additional Local Exposure treatment.
- Actual Hero camera method override is false. Generated normal/mask/structure
  pixels are populated. The supported Manhattan movement/turn covers 8.06 m;
  footage: `tmp/world/lighting_p1/native-r2/manhattan-histogram-transition.mp4`.
- The native optional policy, fixed capture distinction and fixed-policy restoration
  are implemented through the existing presentation owner/schema. Source profile
  adoption is held for the reference-resolution checkpoint; no generated city saved.
- Schema RED: the previous closed schema rejected the demonstrated policy. GREEN:
  accepted policy plus five invalid/missing/unknown-field controls. Native
  HistogramContract and HistogramLifecycle each pass; disk save/reload, nine-role
  no-op, fixed restoration and authored-content preservation use a small fixture
  derived from the existing presentation input. The presentation gate passed 8/8.
  Receipts: `tmp/world/lighting_p1/native-tests/`. Builds: 73.54 s, then 27.56 s
  for the test-only viewport command. No broad World generation or packaging.
- First timing is diagnostic only: actual PNGs are 1298x460 (Manhattan) and
  1294x457 (Kazan). `r.SetRes` changed system-resolution metadata, not the
  embedded PIE render target. Do not use these measurements as a 1440p PASS.
  The existing uncooked viewport helper now has a test-only command; one bounded
  two-arm/two-pose-per-city confirmation runs under `tmp/world/lighting_p1/native-r3/`.
- Both initial city PIE sessions ended before a clean native exit (0). The
  restoring wrapper proved original World/Canonical identities and removed its
  marker. The original rejected source-target receipt and production holds remain.
  The first admission attempt failed with an unidentified PermissionError before
  rendering; the diagnostic replacement succeeded without a Canonical code change.

### 2026-11-16 - P1 reference-resolution confirmation and normal integration

- `tmp/world/lighting_p1/native-r3/run-result.json`: eight original 2560x1440
  images with matched actual Hero eye/FOV/rotation, two controls at two poses per
  city. High GI/reflections/shadows=2, screen percentage 87, dynamic resolution
  off, D3D12 SM6 on RTX 4070; ISM/HISM ray-tracing guard remains 0.
- Histogram GPU p95: 8.386-11.358 ms across these views. Warm Pier A-B-A frame
  p95: EV12 14.41/15.58 ms, Histogram 16.13 ms; postprocess p95 0.292/0.285 ms
  versus 0.321 ms. `native-r3/performance-summary.json` retains 180 finite frames
  per sample. Total frame-budget acceptance remains INCONCLUSIVE: other Editor
  views exceed 16.67 ms in both policies. No installed/R5 FPS PASS or budget waiver.
- Native movement covers 9.51 m on supported Pier geometry. The diagnostic MP4
  preserves actual intervals for 17 full-resolution samples; readback stalls mean
  it is not FPS evidence. Keep the first batch's denser 40-sample clip alongside it.
- The viewport helper rejected no-PIE use and produced independently checked PNG
  dimensions after PIE. Both city PIE sessions ended before native exit 0.
  Protected World/Canonical identities match and the recovery marker is removed.
- `tmp/world/lighting_p1/normal-route-r2/integration.json`: existing synthetic twin
  through normal World CLI, fixed -> Histogram -> no-op. All 26 geometric output
  files remain byte-identical; no-op preserves the entire generated inventory.
  Source fixture and all 182 original generated files restore exactly. A preceding
  collector-only failure read `sha256` instead of manifest `digest`; it restored
  before policy mutation and introduced no production-code fix.
- Wrapper costs: 86.07/86.44/85.92 s; native generation: 2.067/2.747/0.784 s.
  This proves output locality, not complete avoidance of preparation/startup.
  No preparation optimization was implemented; do not broaden this task into it.
- Selected the proven policy in the existing shared two-city source profile; fixed
  captures remain explicit. No runtime controller, second environment or global
  quality rewrite. Saved production cities and canonical authority are unpromoted.
- Native presentation gate: 8/8 PASS; the exact RenderModeContract also PASS.
  Standing semantics/traps are in the render/MCP owners. P1 checkpoint stays open
  for image review and inconclusive total frame-budget disposition. P2/P3 remain
  unstarted; original rejected source-target receipt and R2/R5 holds remain intact.

### D4 material-only visual pass

- Closed within its bounded scope: asphalt ADOPT, concrete RETAIN. One contrast
  variant per material; no new pattern, shader family, facade, World regeneration
  or renderer tuning. Source recipes and accepted manifest remain material-owned.
- `tmp/world/material_tiling/saved-kazan/authenticated-comparison.json`: 24 native
  2560x1440 images, six matched pose pairs at 2/12/45 m horizontal offsets, plus
  BaseColor/WorldNormal controls and restored baselines. Ready 512-square outputs use
  wrap and trilinear filters. Selected components are recorded; rejected waterfront
  scouts are not acceptance evidence.
- Asphalt's macro-disabled BaseColor control removes broad repeating blotches.
  Adopted only `MacroContrast` 0.30 -> 0.10; detail scale, macro scale, relief,
  roughness and texture inputs stay unchanged. Concrete's 0.65 -> 0.20 trial does
  not clearly remove its visible albedo pattern; its source/assets remain exact.
- `saved-kazan/metrics-summary.json`: six 180-frame captures at High, 87% screen
  percentage. Asphalt GPU p95: baseline 9.6068 / candidate 9.6148 / restored
  baseline 9.7169 ms. Editor comparison only; no installed-game certification.
- `saved-kazan/motion-authenticated.json`: four native asynchronous 30-fps-capped
  clips, 358 captured frames; Slate interval p95 33.88-33.97 ms. Capture converts
  the 10-bit viewport to 8-bit PNG and downsamples to 1280x720. This is motion
  review evidence, not uninstrumented frame-budget acceptance.
- `adopt/Current/host.receipt.json`: accepted normal material generation, 1 output,
  9 skips, 0 shader compiles. `final-validate/Current/host.receipt.json`: accepted,
  0 outputs, 10 skips. Only the asphalt package/manifest record changes.
- `adopted-kazan/default-loaded.json` and `default-road.png`: cold actual saved
  road uses the accepted constant instance at 0.10, with Ready existing texture
  pixels and no preview MID. Both rendering sessions end PIE before native exit 0.
- `final-state.json`: 3,723 protected files compared; only the intended asphalt
  package differs. World Content/Canonical/manifests, Concrete, Water, ProjectTexture
  and Histogram source stay unchanged; HEAD/index unchanged; no recovery pending.
- Initial setup was unnecessarily broad: a candidate activation replayed canonical
  corrections, then an agent-started Blueprint commandlet blocked native startup
  and recovery. `recovery.log` proves original identities restored and marker
  removal. No second activation was attempted. The successful pass opens the saved
  city directly; the MCP owner now documents the commandlet-mode trap.
- Remaining at that historical checkpoint (resolved by the cache-range proof below):
  native motion includes the cached-Lumen/real-time-sky
  exposure-clipping warning around EV 12.6, with cached-lighting pre-exposure 4.0.
  Record and resolve with the existing lighting owner before declaring P1 closed;
  no Histogram or global-CVar adjustment is authorized by this material pass.
  The original P1 total-frame INCONCLUSIVE disposition and P2/P3 gates remain.
- Durable comparison guidance: ProjectMaterial's live-preview evaluation policy.
  No Terrain reopening, new task, staging, commits or release promotion.


### 2026-11-17 - cached-lighting range and joint quality/performance checkpoint

- D6 resumes P1; no P2/P3 adoption. Evidence:
  `tmp/world/lighting_acceptance/native-r2/`. The first replay refused before
  rendering because its scratch harness demanded a historical whole-plugin
  digest despite changed presentation inputs. It restored exactly. The replacement
  authenticates the current presentation separately, preserves the exact 9,826-entry
  generated payload/current ownership audit, and retains before/after plugin equality.
  Do not reuse the historical whole-plugin assertion as current input admission.
- Native Temporal Upscaler diagnostics show 2228x1253 input -> 2560x1440 output,
  TSR High, in both cities. Rounded displayed pre-exposure is 0.000146/0.000150
  in Manhattan and 0.000086 in Kazan: view exposure about EV12.7/13.5.
  Installed `EyeAdaptation::WriteWarnings` bounds explain the defect: offset 4
  supports [-8,12], while tested offset 8 supports [-4,16]. These observations
  are transcribed from native overlays, not raw GPU exposure readbacks;
  `range-observations.json` pins the images and calculation.
- Adopted only the cache offset in DefaultEngine.ini. Histogram bounds, Local
  Exposure, High rendering settings, the ISM guard, materials and geometry remain.
  Normal captures retain shaded detail and bright context. All inspected active
  Hero exposure override flags are false; PP readbacks match the current profile.
- Eight matched normal views, 600 uncapped frames each, zero measured camera/FOV
  differences. `performance-summary.json` retains Game/Render/GPU and scoped pass
  costs; CSV pass timings do not establish a complete additive async Lumen budget.

| Pose | GPU p95 old -> candidate (ms) | Editor frame p95 old -> candidate (ms) |
|---|---|---|
| Manhattan fountain shade | 8.646 -> 8.940 | 19.777 -> 26.291 |
| Manhattan Pier waterfront | 11.028 -> 10.667 | 25.686 -> 20.972 |
| Kazan bank ground | 9.494 -> 9.654 | 18.825 -> 22.910 |
| Kazan bank skyline | 11.751 -> 11.414 | 20.045 -> 18.974 |

- No consistent GPU regression demonstrated; total Editor frame budget is missed
  in both controls. Real-runtime/installed acceptance remains INCONCLUSIVE, not a
  performance PASS. No quality reduction or budget change adopted. Sampled process
  memory is diagnostic: `memory-summary.json` records working set, private bytes
  and dedicated GPU usage separately; it does not certify packaged peak memory.
- Native asynchronous Hero capture: 180 frames, 195/195 grounded samples, about
  8.14 m movement with a camera turn. `hero-histogram-transition.mp4` is a 30-fps
  capped visual recording, excluded from performance measurements.
- Verification: 105 World contract files and 13 capture-reader tests PASS. The
  exact native `Project.World.Realization.Presentation.HistogramLifecycle` passes
  with save/reload, nine-role no-op, fixed restoration and authored preservation.
  `cold-native-r2/tests.log` proves startup value 8 from SystemSettingsIni, one
  discovered/started/completed test, 1/1 required marker and native/wrapper exit 0
  (35.9 s). The initial cold native test passed, but its outer shell stripped
  quotes from the required marker; that refused wrapper result is retained.
- Normal rendered Editor exit 0 after all PIE sessions ended; outer recovery
  proves original identities restored and removes its marker. `final-state.json`
  compares 256 Material/Texture/config files: only DefaultEngine.ini differs.
  No World assets are promoted; original rejected geographic target and R5 holds
  remain. HEAD and staged content are unchanged; raw index stat metadata refreshed.
- Durable cache-range semantics: `docs/config/render/render.md`. No diagram or
  schema change: the existing project configuration and presentation owners remain.

### 2026-11-17 - D7 packaging preflight correction

- Initial Development packaging stopped before gameplay: aggregate variation helpers
  collided with projection helpers in the unity translation unit (C2084/C4459).
  The outer operation restored original World/Canonical identities and cleared its marker.
- Scoped the private variation helpers inside their material owner. Standard Editor
  build PASS (28.33 s); the original failing unity input and UBT arguments also pass
  a syntax compile, independently of adaptive exclusion. Native asphalt (41.9 s) and
  triplanar (38.0 s) regressions each discover/start/complete exactly one passing test.
- Existing generator reconciled all ten shared-fingerprint surfaces: ten generated,
  five shader compiles; repeat zero generated, ten skipped, zero compiles, identical
  output/manifest digests. Recipe and texture dependencies remain unchanged. Evidence:
  `tmp/world/lighting_performance/build-regression/`. No artistic changes adopted.
- Durable diagnosis: ProjectMaterial [unity-build pitfall](../../Plugins/Resources/ProjectMaterial/docs/pitfalls.md#aggregate-private-helpers-collide-in-unity-builds).
  One replacement package campaign follows in `tmp/world/lighting_performance-r2/`.
  Performance remains unverified until its authenticated gameplay aggregate passes.


### 2026-11-17 - D7 packaged Development performance

- PASS for the restoring candidate, not production selection or R5. Evidence:
  `tmp/world/lighting_performance-r2/{result.json,package.json,prepared-inputs.json}`;
  Kazan `Kazan/performance-aggregate.json`; Manhattan
  `Saved/Validation/WorldRealization/manhattan-showcase/3462a75c669645eca26553b526450d1a/manhattan-release-performance.json`.
  The prepared World snapshot retains the qualified Ground/Water/Pier outputs
  with current presentation and material bindings; the source/package/executable
  identities are recorded in those receipts.
- Existing World Apply updates nine presentation actors per city and two shared
  Manhattan identity actors while retiring its mixed input binding. Both native
  inventories report zero dirty geometry units; no roads, buildings, Water,
  Terrain, vegetation or structures are regenerated. The existing Terrain post-
  build/inventory route still runs; both builder logs show zero built sections
  and complete reuse. This proves geometry reuse, not sparse total preparation.
  No geographic acceptance receipt is relabelled by this lighting proof.
- One current Development package, three predetermined runs per city, six stopped-
  game host-load windows per city, 89,657 pooled raw frames. RTX 4070/D3D12, High,
  actual game viewport 2560x1440, uncapped envelope, dynamic resolution disabled.
  All six native receipts and normal exits pass; zero streaming failures.

| City | Frames | Frame p95 / p99 (ms) | Game / Render / GPU p95 (ms) | Allowed p95 (ms) |
|---|---|---|---|---|
| Kazan | 42,419 | 14.997 / 18.227 | 7.295 / 14.965 / 10.179 | 16.670 |
| Manhattan | 47,238 | 12.995 / 14.651 | 6.176 / 12.981 / 9.445 | 16.670 |

- Lowest host windows: Kazan CPU 8.0%, GPU 4.67%; Manhattan CPU 10.0%, GPU 3.67%.
  Both receive 0% allowance. Peak process physical/GPU-local memory: Kazan
  3,438,637,056 / 3,733,565,440 bytes; Manhattan 2,217,185,280 / 3,763,068,928 bytes.
  GPU time does not demonstrate a Lumen budget blocker at this static High baseline;
  no tracing, shadow, cloud, resolution or quality downgrade is adopted.
- The context PNGs are separate 1920x1080 SceneCaptures, not main player-camera
  lighting comparisons. Native timing reads the actual game viewport size. The
  matched Ready player views and motion proof remain the P1 appearance evidence;
  interpretation belongs to the World host's screenshot/performance contract.
- Verification cost is separate from runtime cost: per-file Git/LFS source-state
  authentication repeatedly takes about ten minutes in this changed candidate.
  The owner is `scripts/ue/package/prepare_release.py::source_state_digest`, not
  Lumen or the shoreline constructor. No source-identity optimization or guard
  bypass is implemented here. Canonical admission/reversal remains a distinct cost.
- P2/P3 remain deferred under D7; moving-sun/night cost and appearance have not
  been measured. The existing Manhattan Shipping-performance watch remains open:
  Development PASS cannot settle a Shipping-only symptom. R5 still needs its
  own frozen source, selected candidate, installed-game proof and operator review.
- Outer operation `ad249e88c96749c6be3bd909eecc3208` exits 0, proves original
  World/Canonical identities, and removes its recovery marker. No Unreal/game
  process remains. `closure.json` records final selectors and evidence identities;
  `evidence-authentication.json` rechecks 30 retained artifact files and shared
  source/package/executable pins. HEAD and staged content remain unchanged.
