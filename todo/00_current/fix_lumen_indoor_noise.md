# Fix Indoor Lumen Noise + Dynamic Shadow Smear (Apartment Interiors, City17)

Status: open. Research-grounded plan after 2026-05-15 MCP verification + three
parallel web research passes (AAA UE5 case studies, VSM dynamic shadow smear
deep dive, indoor lighting authoring conventions).

## Two distinct phenomena (do not mix)

A previous tuning session mixed them in one CVar campaign and converged on
nothing. They have different root causes and different fix tracks.

**A. Static Lumen GI grain on opaque indoor surfaces** (the original noise
complaint). Visible on plaster walls, wooden doors, radiator, cabinet
panels. Per-frame ray noise that TSR temporal accumulation partially hides.

**B. Lighting smear / "shleif" / ghost trail when dynamic shadow casters move**
(doors swinging open, NPCs walking, vehicles). Old shadow position fades
over ~30-60 frames. Found via `showflag.DynamicShadows 0` -> smear vanishes.

## Industry reframe (AAA UE5 case study survey, 2023-2026)

Surveyed ten shipped titles: Hellblade II, STALKER 2, Black Myth Wukong,
Robocop Rogue City, Layers of Fear (UE5), Silent Hill 2 Remake, Talos
Principle 2, The Casting of Frank Stone, Fortnite, Lyra.

- **Software Lumen + VSM + TSR is the universal AAA UE5 stack.** All ten
  ship this. ALIS is not tech-stack-misconfigured.
- **STALKER 2 is the closest profile to ALIS** (first-person, post-apoc,
  indoor/outdoor, doors, NPCs) and **shipped with visible Lumen + shadow
  artifacts** (flashlight no shadows, NPC shadow gaps, GI popping,
  light leakage). GSC accepted them as costs.
- **AAA pattern: treat indoor noise as CONTENT DEBT, not an engine bug.**
  Hellblade II, Robocop, Silent Hill 2 Remake all manage residual Lumen
  noise via deliberate direct-light "anchors" in every interior (i.e.
  ensure each room has enough direct light that Lumen has signal to
  converge on).
- **No surveyed AAA title retreated to baked GI.**
- **Black Myth Wukong's optional path-tracing mode** is the only "escape
  hatch" shipped (NVIDIA partnership) and is gameplay-cost-prohibitive
  except as a graphics-flagship feature.
- **MegaLights itself ships noisy in 5.7** (Beta). Not a silver bullet.
- **VSM dynamic shadow lag is tribal knowledge**, not Epic-documented as
  a clean fix recipe.

Bottom line: nothing about ALIS's noise/smear profile is uniquely broken.
We are on the AAA frontier and need the AAA approach (content discipline
+ surgical CVars + per-actor authoring), not the trial-and-error CVar
campaign approach.

---

## Phenomenon B - root cause confirmed (engine regression)

**Known UE 5.7.0 VSM regression.** Confirmed Epic forum threads:

- [Constant invalidation of VSM dynamic shadows in 5.7.0](https://forums.unrealengine.com/t/constant-invalidation-of-vsm-dynamic-shadows-in-5-7-0/2686534)
- [5.7 Directional light is invalidating all VSM caches](https://forums.unrealengine.com/t/unreal-engine-5-7-directional-light-is-invalidating-all-vsm-caches/2674483)

Not present in 5.6.1. Epic targeting fix in **5.7.3**.

Amplified in ALIS by:

- Project's hot SMRT tuning -
  [`Config/DefaultScalability.ini`](../../Config/DefaultScalability.ini)
  lines 98-116:
  ```
  r.Shadow.Virtual.SMRT.RayCountDirectional=8
  r.Shadow.Virtual.SMRT.SamplesPerRayDirectional=4
  ```
  Roughly 2x engine default. Higher samples thicken the temporal denoiser
  tail; cache-invalidation lag becomes a longer-visible trail.
- Movable shadow casters (doors) using default `ShadowCacheInvalidationBehavior`
  rather than `Rigid` - the cleaner-invalidation mode designed for this case.
- Possible unrelated invalidators flooding the page pool (5.7 water
  rendering bug; Nanite skeletal meshes per UE-328823; foliage WPO).
- TSR `Resurrection.PersistentFrameCount=16` (set for "strafe artifact" fix
  in [`DefaultEngine.ini`](../../Config/DefaultEngine.ini) line 37)
  preserves old TSR history longer than default - composes with the VSM
  lag to make the trail more visible.

### Track B1 - passive (PREFERRED)

Wait for **UE 5.7.3+**. Engine fix, not ours. Re-evaluate after upgrade.
Most cost-efficient path.

Action: subscribe to UE release notes; on next launcher patch, re-test the
City17 door-open scenario before any other Phase B work.

### Track B2 - per-actor invalidation behavior (low-risk authoring)

Set component property `Shadow Cache Invalidation Behavior = Rigid` on every
movable shadow caster: doors, vehicles, NPC skel meshes where mobility is
movable. Per
[EShadowCacheInvalidationBehavior API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/EShadowCacheInvalidationBehavior).

Caveat: Nanite skeletal meshes ignore `Rigid` (UE-328823 class bug). If
affected actors are Nanite skeletal, convert to non-Nanite skeletal until
the bug is fixed.

Catalogue work, not CVar work. Best done by content owners with a sweep
list.

### Track B3 - surgical CVar reduction (defers some "soft shadow" quality)

Drop SMRT tuning in
[`Config/DefaultScalability.ini`](../../Config/DefaultScalability.ini)
lines 98-116:

```ini
r.Shadow.Virtual.SMRT.RayCountDirectional=4         ; was 8
r.Shadow.Virtual.SMRT.SamplesPerRayDirectional=2    ; was 4
```

Net: halves the temporal denoiser tail. Shadows become slightly harder /
less penumbra-soft, which is the project's original "soft shadow quality"
preference. This is a TRADE-OFF, not a free win - document it in the SOT
[`docs/config/render/lighting-bp-sunsky.md`](../../docs/config/render/lighting-bp-sunsky.md).

### Track B4 - hybrid (last resort, GPU-budget cost)

Re-enable ray-traced shadows for the directional light only (RT shadows
have no page-cache lag):

```ini
r.RayTracing.Shadows=True
```

Per-light in BP_SunSky: disable `Cast Ray Traced Shadows` on lights that
should keep VSM. Cost ~1-2 ms on mid-tier RT GPUs. Only justified if Track
B1+B2+B3 prove insufficient.

### Track B5 - audit ambient invalidators

Use `r.Shadow.Virtual.Cache.DrawInvalidatingBounds 1` in-editor. If many
non-door bounds are invalidating per frame:

- Check water materials for shadow casting (5.7 bug; disable on the main
  pass)
- Audit Nanite skeletal meshes (UE-328823)
- Check foliage WPO Disable Distance is set on small props
- Set `World Position Offset Disable Distance` on far foliage

---

## Phenomenon A - static GI grain (content-debt strategy)

The static grain is per-frame Lumen ScreenProbeGather noise + AO grain
(verified via `showflag.GlobalIllumination 0` bisect: noise gone on
opaque surfaces; concrete floor showed residual "slow dark spots playing"
which is AO grain).

Per the AAA cohort, the durable fix is **content authoring**, not Lumen
quality CVars (which cost GPU and reach diminishing returns).

### Track A1 - apartment mesh split (HIGHEST IMPACT, content side)

The #1 cause of "Lumen looks bad in interiors" per the agent research
(Lumen SIGGRAPH 2022 + Epic Community KB on wall light leak): single-mesh
apartment walls do not generate clean Lumen scene cards behind interior
faces, and have surface-cache leak through thin / non-closed geometry.

Action:

- Split apartment walls / floors / ceilings into SEPARATE meshes.
- Wall thickness >= 10 cm (per Lumen SIGGRAPH paper).
- Verify with `r.VisualizeMeshDistanceFields 1` that each interior wall
  has a closed Distance Field.
- DF Resolution Scale 1.5-2.0 on thin walls / problem architectural pieces.
- Two-Sided Distance Field generation ONLY for thin / non-thickenable
  pieces (curtains, signs).

Owner: level / environment art. This is multi-asset work, not a CVar.

### Track A2 - SkyLight: stop the cubemap-recapture treadmill

The SunSky_C actor has `SkyLight.SourceType = SLS_CapturedScene` with Real
Time Capture ON (verified via MCP, project default per stock UE SunSky BP).
Per agent 3: production AAA UE5 interiors capture once (Stationary,
non-RTC). RTC has measurable per-frame cost + recapture latency which
amplifies lag artifacts on geometry change.

Action on BP_SunSky SkyLight component:

| Property | Live | Proposed | Reason |
| --- | --- | --- | --- |
| `Real Time Capture` | True | **False** | stop per-frame cubemap rebuild + recapture lag |
| `bLowerHemisphereIsBlack` | False | **True** | hard-stop ground leak (color already 0,0,0) |
| `Intensity` | 1.0 | **0.3-0.5** | indoor-dominant - reduce raw skylight contribution, replace with practical window-Rect lights per Track A3 |

After change, hit "Recapture Scene" once to bake the cubemap.

### Track A3 - direct-light anchors (the AAA pattern)

Per Hellblade II / Robocop / Silent Hill 2 Remake / Layers of Fear /
William Faucher tutorials: every interior should have a deliberate direct
light "anchor" so Lumen has signal to converge on. Per apartment room:

| Light type | Count | Placement | Temp | Intensity |
| --- | --- | --- | --- | --- |
| Rect Light (window key) | 1 per window | inside window plane, oriented inward | 5500-6500 K (daylight) | 5000-10000 cd/m^2 |
| Rect Light (window fill) | 1 per window | wider, dimmer, behind the key | 5500-6500 K | half of key |
| Rect Light (ceiling fixture) | 1 per ceiling lamp | facing down | 2700-3200 K (tungsten) | ~800-1600 lm equivalent |
| Spot Light (sconce / under-cabinet) | as needed | per fixture | 2700-3200 K | low |

Per-light setting: enable `Use MegaLights` flag once UE 5.7 MegaLights
artifacts stabilize (5.7.2 / 5.7.3). For now, place but leave off; the
project has `r.MegaLights.EnableForProject=True` and PPV `bMegaLights=True`
already, no project-side switch needed.

Owner: lighting / environment art.

### Track A4 - god rays via volumetric fog (not atmosphere)

Per agent 3: god rays / window-light bleed in interiors should come from
**Exponential Height Fog with Volumetric Fog enabled** plus `Volumetric
Scattering Intensity > 0` on the window Rect Lights. NOT from
SunSky atmospheric scattering (which is for exteriors).

Action: add an ExponentialHeightFog actor to apartment maps (if not
present), enable Volumetric Fog, set window Rect Lights' Volumetric
Scattering Intensity to taste.

### Track A5 - material roughness floor

Lumen reflections produce visible noise when Roughness drops below ~0.15.
Indoor metals (radiator, door handles): keep Roughness >= 0.25.
Glass: prefer Masked or Dithered blend mode for non-hero windows; reserve
Translucent for hero glass that needs refraction.

Owner: material authoring. Sweep `Plugins/Resources/ProjectObject/.../`
and the M_S_House materials visible in the screenshots.

### Track A6 - PPV override cleanup

The live `PPV_Global` PPV has several Lumen overrides flagged ON but set
to engine-default values (e.g. `LumenSceneDetail = 1.0`,
`LumenReflectionQuality = 1.0`). These are clutter and confuse future
debugging (we already burned an hour on it). Either set meaningfully (2.0
each, slight quality boost, modest GPU cost) OR clear the `bOverride_*`
flags.

Additionally, defer this from the SkyLight scope:

- `LumenSkylightLeaking = 0.02` -> consider 0.0 (Lumen's own screen-space
  skylight leak knob; unusually high and defeats Track A2's hemisphere
  gate). Note this was tried during experimentation and did NOT
  individually clean noise - keep as belt-and-braces with A2, not a
  standalone fix.

### Track A7 - small CVar pack (only after A1-A6 land)

Once content fixes land, the residual noise floor is what AAA accepts. If
the project still wants narrower static-grain reduction, the following
CVars are SAFE (verified during 2026-05-15 experimentation - do NOT cause
visible dynamic-shadow smear):

```ini
; Add under [/Script/Engine.RendererSettings] in DefaultEngine.ini
r.Lumen.ScreenProbeGather.MaxRayIntensity=4
r.Lumen.ScreenProbeGather.ShortRangeAO.HardwareRayTracing=1
r.Lumen.Reflections.RoughnessFadeLength=0.6
```

CVars that **must NOT ship** (verified to cause smear / ghosting in
experimentation):

```ini
; DO NOT ENABLE - dynamic ghosting and trails on interactive scenes
; r.Lumen.ScreenProbeGather.Temporal.MaxFramesAccumulated >= 24
; r.Lumen.Reflections.Temporal.MaxFramesAccumulated >= 16
```

For per-PPV settings (better-targeted than global): adjust on PPV_Global
or per-room PPVs:

- `LumenMaxTraceDistance`: live 100000 cm is too high for apartments;
  reasonable values 5000-10000. Affects ray scope, NOT ray noise rate.
  Light gain, not the main fix.
- `LumenSceneViewDistance`: live 20000 cm; can go to 8000-10000 for
  apartment maps.

These are perf trims, not noise fixes. Apply only after Tracks A1-A6.

---

## Live state SOT (verified via MCP on 2026-05-15)

Actors:

- `BP_SunSky` (label) = `/SunPosition/SunSky.SunSky_C` (stock Engine SunSky
  plugin, NOT `BP_SunSky_Child` as
  [`docs/config/render/lighting-bp-sunsky.md`](../../docs/config/render/lighting-bp-sunsky.md)
  claims).
- `PPV_Global` PostProcessVolume, `bUnbound=true`.

Key component values:

| Property | Live | SOT doc claims | Truth |
| --- | --- | --- | --- |
| SkyLight Intensity | 1.0 | 3.0 | doc wrong |
| SkyLight bLowerHemisphereIsBlack | false | not recorded | doc gap |
| SkyLight LowerHemisphereColor | RGB(0,0,0) | RGB(30,35,40) | doc wrong |
| DirectionalLight Intensity | 45000 | "9.0 lux" | doc wrong (legacy units) |
| DirectionalLight IndirectLightingIntensity | 1.0 | 1.2 | doc wrong |
| DirectionalLight ShadowAmount | 1.0 | 0.85 | doc wrong |

PPV Lumen overrides currently active (all `bOverride_*=True`):

| Property | Live | Note |
| --- | --- | --- |
| `LumenMaxTraceDistance` | 100000 | unusually high for apartments |
| `LumenSceneViewDistance` | 20000 | engine default |
| `LumenSceneDetail` | 1.0 | override but default value |
| `LumenSceneLightingQuality` | 1.0 | override but default value |
| `LumenFinalGatherQuality` | 2.0 | raised |
| `LumenReflectionQuality` | 1.0 | override but default value |
| `LumenSkylightLeaking` | 0.02 | unusually high - see Track A6 |
| `LumenDiffuseColorBoost` | 1.1 | +10% bounce |
| `ReflectionMethod` | Lumen | PPV overrides `r.ReflectionMethod=1` (SSR) in ini |
| `bMegaLights` | True | MegaLights enabled at PPV |
| `MotionBlurAmount` | 0.4 | active despite `r.MotionBlurQuality=0` |
| `BloomMethod / BloomIntensity` | BM_FFT / 0.25 | active despite `r.BloomQuality=0` |

## SOT doc fixes (co-land in same PR, separate commit)

- [`docs/config/render/lighting-bp-sunsky.md`](../../docs/config/render/lighting-bp-sunsky.md)
  - Correct actor class to stock SunSky plugin (`SunSky_C`), not `BP_SunSky_Child`.
  - Correct SkyLight Intensity to live 1.0 (or document the Track A2 change once made).
  - Correct LowerHemisphereColor to RGB(0,0,0).
  - Add `bLowerHemisphereIsBlack` row (currently undocumented).
  - Correct DirectionalLight Intensity / IndirectLightingIntensity / ShadowAmount to engine defaults.

- [`docs/config/render/lighting-postprocess.md`](../../docs/config/render/lighting-postprocess.md)
  - Correct exposure mechanism to physical-camera (Shutter 100, ISO 300, f/16),
    not Min/Max EV 8.5.
  - Correct ColorGamma global to live 1.0.
  - Correct AmbientOcclusionRadius to live 180.
  - Document the existing PPV Lumen overrides (so the next reader does not
    assume they are unset).

## What we learned during experimentation (compressed, not a recipe)

A live tuning session on 2026-05-15 worked through several CVar batches.
Recorded for future-author benefit. **Do not act on this section as a
plan** - the real plan is the Tracks above.

Ruled out as causes (the noise / smear persisted):

- PPV `LumenMaxTraceDistance` 100000 -> 5000
- PPV `LumenSceneViewDistance` 20000 -> 6000
- PPV `LumenSkylightLeaking` 0.02 -> 0.0
- SkyLight `bLowerHemisphereIsBlack` false -> true (helps skylight leak
  but not the dominant noise)
- `r.Lumen.ScreenProbeGather.Temporal.MaxFramesAccumulated` at 8 / 16 / 24 / 32
- `r.Lumen.Reflections.Temporal.MaxFramesAccumulated` at 4 / 8 / 16 / 24
- `r.TSR.Resurrection.PersistentFrameCount` 16 -> 0
- `r.TSR.History.SampleCount` 8 -> 4
- `showflag.MotionBlur 0`
- `r.Shadow.Virtual.SMRT.RayCountDirectional` 8 -> 2
- `r.Shadow.Virtual.SMRT.SamplesPerRayDirectional` 4 -> 2

Isolation wins:

- `showflag.Translucency 0` did NOT remove noise -> ruled out translucency.
- `r.AntiAliasingMethod 0` (TSR off) -> noise WORSE -> TSR was hiding per-
  frame noise, not generating it.
- `showflag.GlobalIllumination 0` -> noise gone on most opaque surfaces;
  residual "slow dark spots playing" on concrete floor -> AO grain.
- `showflag.DynamicShadows 0` -> SMEAR GONE -> isolated B to dynamic shadows.

Insight: A (static GI grain) and B (dynamic shadow smear) are SEPARATE
problems. Mixing them in one CVar campaign converged on nothing. They
have separate fix tracks (above).

## Files touched on approval (Phenomenon A + B combined)

1. Apartment maps - Track A1 (mesh split). Multi-asset content work, owner: env art.
2. `BP_SunSky` SkyLight component in `City17_Persistent_WP` - Track A2.
3. Apartment maps - Track A3 (Rect / Spot light placement).
4. Apartment maps - Track A4 (ExponentialHeightFog + Volumetric Fog).
5. M_S_House and equivalent material assets - Track A5 (roughness floor).
6. `PPV_Global` in `City17_Persistent_WP` - Track A6 cleanup.
7. [`Config/DefaultEngine.ini`](../../Config/DefaultEngine.ini) - Track A7
   (3 CVars under `[/Script/Engine.RendererSettings]`).
8. [`Config/DefaultScalability.ini`](../../Config/DefaultScalability.ini) -
   Track B3 (`RayCountDirectional 8 -> 4`, `SamplesPerRayDirectional 4 -> 2`).
9. Movable shadow casters across the project (doors, NPCs, vehicles) -
   Track B2 (`Shadow Cache Invalidation Behavior = Rigid`).
10. [`docs/config/render/lighting-bp-sunsky.md`](../../docs/config/render/lighting-bp-sunsky.md) - SOT doc fix.
11. [`docs/config/render/lighting-postprocess.md`](../../docs/config/render/lighting-postprocess.md) - SOT doc fix.

PR shape:

```text
commit 1: fix(rendering): indoor Lumen content + VSM tuning (Track A2/A6/A7 + B3)
commit 2: chore(content): per-actor Rigid shadow invalidation on movable casters (Track B2)
commit 3: docs(rendering): update stale lighting SOT docs to match live MCP state
```

Larger asset-side work (Tracks A1, A3, A4, A5, B5) lands in separate
content commits owned by env / lighting / material art.

## Coordination with in-flight todos

- [`improve_foliage_lumen_quality.md`](improve_foliage_lumen_quality.md):
  shares the `r.Lumen.TraceMeshSDFs` decision. NOT touched in this todo.
  Track that decision there.
- [`restore_lost_foliage_instances.md`](restore_lost_foliage_instances.md):
  unrelated. No overlap.

## Honest "do nothing" option

Per the AAA survey, STALKER 2 shipped with visible Lumen + shadow
artifacts. ALIS could too. If the dev-time cost of Tracks A1-A6 (multi-
month content work) is not worth the quality delta, the project can:

- Land Tracks A2 + A6 + A7 (cheap CVar / scene work, days of work).
- Wait for UE 5.7.3 to fix Phenomenon B (Track B1).
- Defer A1 / A3 / A4 / A5 indefinitely.
- Ship with residual noise on par with STALKER 2.

This is a legitimate choice, not a failure mode. Document the decision.

## References

### AAA case study sources

- [Digital Foundry on Hellblade II](https://www.eurogamer.net/digitalfoundry-2024-senuas-saga-hellblade-2-is-a-defining-moment-in-the-evolution-of-real-time-graphics)
- [Lumen and VSM in Hellblade 2 (technical review)](https://medium.com/@shinsoj/technical-review-lumen-and-vsm-in-hellblade-2-925d17f65e04)
- [Epic interview: Croteam on UE5 for Talos Principle 2](https://www.unrealengine.com/en-US/developer-interviews/inside-croteam-s-transition-from-in-house-tech-to-ue5-for-the-talos-principle-2)
- [Epic interview: STALKER 2 and UE5](https://www.unrealengine.com/en-US/developer-interviews/balancing-nostalgia-with-innovation-in-s-t-a-l-k-e-r-2-heart-of-chornobyl)
- [PC Gamer: STALKER 2 performance analysis](https://www.pcgamer.com/hardware/stalker-2-heart-of-chornobyl-performance-analysis-everyone-gets-ray-tracing-but-the-entry-fee-is-high/)
- [Epic interview: Layers of Fear and Lumen](https://wccftech.com/layers-of-fear-tech-qa-anshar-studios-talks-shipping-the-first-ue5-third-party-game-with-lumen-on-spotlight/)
- [NVIDIA: Black Myth Wukong full ray tracing](https://www.nvidia.com/en-us/geforce/news/gfecnt/20248/black-myth-wukong-full-ray-tracing-dlss-3/)
- [Digital Foundry / GamingBolt: Silent Hill 2 Remake](https://gamingbolt.com/silent-hill-2-remake-graphics-analysis-pushing-unreal-engine-5-to-its-limits)
- [Digital Foundry: RoboCop Rogue City](https://www.neogaf.com/threads/digital-foundry-robocop-rogue-city-df-tech-review-unreal-engine-5-shines-on-ps5-xbox-series-x-s.1663497/)

### VSM dynamic shadow smear sources

- [Epic forum - Constant invalidation of VSM dynamic shadows in 5.7.0](https://forums.unrealengine.com/t/constant-invalidation-of-vsm-dynamic-shadows-in-5-7-0/2686534)
- [Epic forum - UE 5.7 Directional light is invalidating all VSM caches](https://forums.unrealengine.com/t/unreal-engine-5-7-directional-light-is-invalidating-all-vsm-caches/2674483)
- [Epic forum - VSM Shadow Cache Invalidation Behaviour (UE5.4)](https://forums.unrealengine.com/t/vsm-shadow-cache-invalidation-behaviour-and-separate-static-caching-in-ue5-4/1927473)
- [Epic forum - Nanite Skeletal Meshes Invalidate VSM (UE-328823)](https://forums.unrealengine.com/t/nanite-skeletal-meshes-invalidate-vsm/2666444)
- [Epic forum - Persistent Lumen Ghosting/Smearing 5.7.4](https://forums.unrealengine.com/t/title-persistent-lumen-gi-ghosting-smearing-on-moving-objects-despite-velocity-and-cvar-adjustments-unreal-engine-version-5-7-4/2717636)
- [Virtual Shadow Maps - UE 5.7 docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine)
- [StraySpark - VSM Optimization for Open Worlds in UE5.7](https://www.strayspark.studio/blog/virtual-shadow-map-optimization-open-worlds-ue5-7)
- [Epic tech blog - Virtual Shadow Maps in Fortnite Battle Royale Chapter 4](https://www.unrealengine.com/en-US/tech-blog/virtual-shadow-maps-in-fortnite-battle-royale-chapter-4)
- [EShadowCacheInvalidationBehavior API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/EShadowCacheInvalidationBehavior)

### Indoor lighting authoring sources

- [William Faucher - Lighting Interiors in UE5](https://www.youtube.com/watch?v=0GYyHDuaPcg)
- [William Faucher - Things To Know About Lumen](https://will_faucher.artstation.com/projects/lxvJ6o)
- [William Faucher - UE5 Render Settings Gumroad](https://williamfaucher.gumroad.com/l/WilliamsUE5RenderGuide)
- [Epic Community KB - Lumen leak through walls](https://dev.epicgames.com/community/learning/knowledge-base/15Gl/unreal-engine-why-does-my-lighting-leak-through-walls-with-lumen)
- [Epic Docs - Sky Lights UE 5.7](https://dev.epicgames.com/documentation/en-us/unreal-engine/sky-lights-in-unreal-engine)
- [Epic Docs - Post Process Volumes UE 5.7](https://dev.epicgames.com/documentation/en-us/unreal-engine/add-post-process-volumes)
- [Lumen SIGGRAPH 2022 - Wright et al.](https://advances.realtimerendering.com/s2022/SIGGRAPH2022-Advances-Lumen-Wright%20et%20al.pdf)
- [Magnopus - Photography principles for UE lighting](https://www.magnopus.com/blog/lighting-in-unreal-with-photography-principles)
- [NVIDIA - UE5 Raytracing Guideline](https://dlss.download.nvidia.com/uebinarypackages/Documentation/UE5+Raytracing+Guideline+v5.4.pdf)
- [StraySpark - UE5 Lumen 60fps Optimization](https://www.strayspark.studio/blog/ue5-lumen-optimization-60fps)
- [Hyperdense - Lumen for indies](https://medium.com/@sarah.hyperdense/lumen-lighting-for-indies-good-results-without-melting-your-gpu-517cfe83c6c7)
- [Tom Looman - UE 5.7 Performance Highlights](https://tomlooman.com/unreal-engine-5-7-performance-highlights/)

## History

- 2026-05-15: todo opened (initial planning).
- 2026-05-15: MCP verification - SOT docs found stale; live PPV / SkyLight values captured.
- 2026-05-15: Live tuning session - Phase 2 PPV trims + Phase 3a hemisphere
  gate applied, no improvement, reverted. Phase 1 temporal CVars applied,
  reduced static grain BUT introduced dynamic-shadow smear, reverted.
- 2026-05-15: Bisect via `showflag.*` isolated two distinct phenomena
  (A static grain via GI, B smear via dynamic shadows).
- 2026-05-15: Three parallel research agents (AAA case studies, VSM smear
  deep dive, indoor authoring conventions) returned findings.
- 2026-05-15: Full doc rewrite to research-grounded plan. Removed
  experimental phase numbering; replaced with Tracks A1-A7 (content debt)
  and B1-B5 (engine regression). Confirmed: ALIS's tech stack is the AAA
  consensus; door smear is a known UE 5.7.0 regression with Epic 5.7.3
  fix targeted.
