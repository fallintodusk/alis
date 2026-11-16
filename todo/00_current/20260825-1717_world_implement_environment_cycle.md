# ALIS 3.0.0 lighting and daylight qualification

Status: CURRENT - STUB / QUEUED. Terrain development disposition is complete;
lighting investigation and implementation have not started.
No new lighting research, provider selection, tuning or implementation is authorized
by this stub. This is the existing environment-cycle task, moved from backlog and
broadened as the single lighting/daylight owner.

## Goal

Qualify a KISS presentation for generated Kazan and Manhattan: readable shaded
streets, credible daylight and night appearance, and measured rendering cost.
Retain the existing environment owner and runtime-safe control boundary.

## Retained evidence and limits

- Legacy City17 SunSky, exposure, persistent lighting and streaming experience:
  [SunSky setup](../../docs/config/render/lighting-bp-sunsky.md),
  [Editor lighting setup](../../docs/config/render/lighting-setup-editor.md),
  [troubleshooting](../../docs/config/render/lighting-troubleshooting.md).
  These are historical references to qualify against the installed engine and
  generated worlds, not settings to copy blindly.
- Generated worlds already have static outdoor actors, but no selected reusable
  runtime day/night provider. Existing SunPosition and Day Sequence are future
  native candidates; compare the installed capabilities before choosing a custom
  controller. A runtime controller must not depend on ProjectWorldEditor.
- The source-bound fountain comparison is retained at
  `tmp/world/terrain_execute_d44/presentation-comparison-r7/`, especially
  `comparison.json`, `pattern-before.json`, `pattern-runtime.json` and the matched
  `play-context-ev12.png` / `play-context-ev8.png` with their camera/support JSON.
  Cold Editor output textures were empty; PIE generated actual Ready pixels.
  Original EV12 was too dark in play mode; disposable EV8 improved readability.
  EV8 was not selected for production or qualified in both cities/sunlit areas.
- The live observation crashed in PIE teardown after capture. Files authenticate,
  but this is not a clean native, Hero or packaged acceptance run. Protected
  geography, assets, profiles, HEAD and index were restored unchanged.
- Readiness, capture-envelope and shutdown lessons belong to the stable owners
  below. Do not repeat Ground-source acquisition, height corrections or a full
  city regeneration to diagnose presentation.

## Future investigation steps

- [ ] When this task is authorized for investigation, establish representative shadowed
  and sunlit controls in both cities, with actual Ready runtime materials.
- [ ] Separate exposure, material response and illumination. Compare a bounded
  exposure policy; do not apply EV8 globally from one shaded fountain image.
- [ ] Qualify Lumen GI/reflections, hardware/software paths, shadows, Sky Light
  capture, atmosphere/cloud/fog and scalability against the actual runtime.
  Measure GPU/render cost, memory, streaming transitions and the 16.67 ms target
  before changing settings. Preserve explicit INCONCLUSIVE handling.
- [ ] Compare the existing approach with current native daylight providers:
  geographic Sun, date/time/north, controllable clock, frozen-time evidence,
  morning/noon/sunset/night and continuous transitions. Reuse outdoor actors.
- [ ] Propose one small Kazan prototype, then affected Manhattan checks. Require
  readable play, save/reload, normal startup and clean native shutdown.
- [ ] Record adopt/defer for 3.0.0. Day/night remains a preferred investigation;
  weather presets are optional later work. No weather simulation, new city-light
  infrastructure, PCG work, engine upgrade or general environment framework.

## Stable owners

- [World presentation realization](../../Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationRealization.cpp)
  and [territory presentation data](../../Plugins/World/ProjectWorldData/Data/Presentation/kazan_representative_v1.json).
- [Terrain/object fidelity and separation](../../Plugins/World/ProjectWorld/docs/territory_contract.md#terrain-and-feature-separation).
- [Paired visual review](../../tools/World/VisualVerification/README.md#paired-geometry-and-presentation-review)
  and [runtime procedural outputs](../../Plugins/Resources/ProjectTexture/docs/pattern_generation.md#contract).
- [Editor execution/shutdown](../../docs/ue_engine/mcp_editor_control.md#ue-581-map-switch-and-shutdown-boundaries),
  [render configuration](../../docs/config/render/README.md),
  [test layers](../../docs/testing/world_pipeline_layers.md)
  and [native reuse](../../docs/architecture/principles.md#native-engine-first).
