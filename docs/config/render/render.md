# Render configuration owners

Renderer values and map defaults are owned by configuration, not a copied INI
inside documentation. Pin the exact source revision and dirty-file hashes when
comparing this checkout with a public revision or an installed candidate.

## Current configuration

| Concern | Owner |
|---|---|
| Renderer project settings, stability guards and entry-map defaults | [DefaultEngine.ini](../../../Config/DefaultEngine.ini) |
| Quality-tier overrides | [DefaultScalability.ini](../../../Config/DefaultScalability.ini) |
| Generated-world outdoor settings | [Presentation profile](../../../Plugins/World/ProjectWorldData/Data/Presentation/kazan_representative_v1.json) |
| Generated actors, PostProcess and capture-camera realization | [World presentation implementation](../../../Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldPresentationRealization.cpp) |
| Native lighting parameters and runtime Sun application | [ProjectLight](../../../Plugins/World/ProjectLight/README.md) |
| Installed engine selection | [Engine configuration](../../../scripts/config/ue_path.conf) and its resolver; machine overrides remain local |

`r.ReflectionMethod=1` selects Lumen reflections; `2` selects standalone SSR
(`EngineTypes.h::EReflectionMethod`). The effective runtime path also depends on
scalability and view settings. A project setting or an SSR quality value alone
does not establish which reflection path a player view actually uses.

Generated World presentation bakes declared lighting settings. Optional runtime
activation follows [World's cooked daylight binding](../../../Plugins/World/ProjectWorld/docs/daylight_composition.md).
Its
[profile contract](../../../Plugins/World/ProjectWorld/Data/Schemas/project_world_presentation_profile.schema.json)
supports fixed physical exposure and optional bounded native Histogram for
gameplay. The generated unbound PostProcess volume owns gameplay exposure;
generated capture cameras explicitly retain the profile's fixed exposure.
Cooked native actors carry these settings without a runtime Editor dependency.
Global auto-exposure defaults alone do not establish the blended player-camera
policy; inspect active camera override flags and actual Ready player views.

The profile expresses Histogram limits in EV100. Realization converts them for
the configured native exposure-unit convention. Changing that convention requires
an explicit presentation-output migration; unchanged-profile no-op does not
convert saved settings. Fixed captures are
deterministic diagnostics; adaptive gameplay needs settled views and a moving
shade/bright-scene comparison. Measure frame and GPU costs at the actual image
resolution; a configured resolution alone does not establish the rendered size.

## Cached lighting and gameplay exposure

`r.EyeAdaptation.CachedLightingPreExposure` in the project configuration owns the
fixed precision/range offset for cached Lumen and real-time Sky Light lighting.
Gameplay brightness and adaptation remain with the World PostProcess policy;
the cache offset is not an exposure-compensation control or a quality tier.

A valid Histogram policy can still exceed the cached-lighting range. This is
distinct from clipped highlights in the final image. The installed renderer's
`PostProcessEyeAdaptation.cpp::EyeAdaptation::WriteWarnings` compares the actual
view pre-exposure with that range. Qualify changes with the native Temporal
Upscaler visualization's pre-exposure and input/output dimensions, then inspect
matched normal player views and motion. Diagnostic views do not certify appearance.
Do not suppress the warning or reduce lighting quality as a substitute for fixing
an established range mismatch.

Keep one project configuration owner rather than competing per-world or per-tick
writers of this global setting. Check a cold process readback after an offset
change. A future night policy must qualify its darker endpoint as well as daylight;
the static-world comparison does not establish day/night or installed-game acceptance.

## Wall-lighting diagnostics

Use the same Ready player pose, time, image dimensions, exposure and quality tier
as the reported defect. A darker replay that hides the pattern is not a fix.
Compare native Unlit, Lighting Only, Detail Lighting and World Normal before
changing a material. Inspect Lumen Scene, Surface Cache and Geometry Normals
separately; visible-mesh normals do not prove tracing-representation agreement.
Pink Surface Cache regions establish missing coverage, not the cause of every
bright patch. Temporarily disable diffuse GI and screen traces separately,
restore each control, and retain normal-render controls.
Compare a cold fixed-time view with a day-to-night seek and a later view at the
same UTC: indirect-light and exposure history can outlast texture readiness.
Do not accept transient daylight left on night walls as the night preset's
visibility. A longer capture wait corrects evidence; it does not eliminate the
runtime transient or qualify continuously accelerated time.

For an active post-seek artifact, `vis Lumen.ScreenProbeGather.DiffuseIndirect`
shows the native diffuse-indirect contribution; `vis 0` disables that diagnostic.
Test `r.Lumen.ScreenProbeGather.RadianceCache 0` before the lighting change and
compare with its enabled control, then restore the original value. Removing a
patch pattern while retaining stale overall illumination is not convergence.
Lumen Scene and Final Gather update speeds control different stages; failure
of one does not reject the other. In the installed renderer,
`r.Lumen.ScreenProbeGather.Temporal.ClearHistoryEveryFrame 1` disables history
reuse for debugging; `0` restores normal reuse. Confirm this in
`LumenScreenProbeGather.cpp`, rather than copying a forum reply's reversed value.
These global switches and Surface Cache resets are diagnostics, not runtime policies.
A demonstrated Scene-budget correction belongs to
[ProjectLight's bounded world policy](../../../Plugins/World/ProjectLight/docs/architecture/structure.md#update-lifecycle-and-performance).
Large instantaneous seeks and continuous accelerated time need separate evidence.

Generated massing uses ordinary static-mesh components, so the project's ISM
ray-tracing exclusion does not directly exclude those walls. Preserve that
documented stability guard. Hardware tracing, mesh distance fields and Surface
Cache cards are different representations. In the installed engine,
`StaticMesh.cpp` and `DistanceFieldAtlas.cpp` can schedule card generation even
with distance-field resolution zero. Read actual card data after asynchronous builds;
raising `MaxLumenMeshCards` does not guarantee additional cards or coverage.
Qualify a disposable rebuilt cell before changing its producer or saved output.
See Epic's [Lumen technical details](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-technical-details-in-unreal-engine)
and [native view modes](https://dev.epicgames.com/documentation/en-us/unreal-engine/viewport-modes-in-unreal-engine),
with the installed engine source authoritative for exact behavior.

## Map-specific references and evidence

The [City17 SunSky guide](lighting-bp-sunsky.md),
[Editor setup](lighting-setup-editor.md),
[post-process notes](lighting-postprocess.md) and
[troubleshooting](lighting-troubleshooting.md) describe a legacy map-specific
setup. Their suggested lux, skylight and exposure values are not current
settings or acceptance evidence for generated Kazan/Manhattan. The
[fix history](lighting-fix-summary.md) records that historical work.

Use [paired visual review](../../../tools/World/VisualVerification/README.md#paired-geometry-and-presentation-review)
and [actual Ready procedural outputs](../../../Plugins/Resources/ProjectTexture/docs/pattern_generation.md#contract)
to distinguish material, lighting and geometry issues. Runtime readbacks and
normal player views decide current appearance; a cold Editor image does not.
