# Fix Strafe Artifacts (Striations During Fast Camera Turns)

## Problem

Visible striations/banding on walls and floors during fast camera turns in dimly lit rooms.
Reproduced on multiple maps. Not observed when standing still.

## Root Cause (Confirmed)

**TSR (Temporal Super Resolution, r.AntiAliasingMethod=4)** is the cause.
TSR's shading rejection algorithm is overly aggressive during fast camera rotations —
it rejects nearly 100% of temporal history (confirmed via `r.TSR.Visualize 2`, entire screen turns pink).
Without accumulated history, the raw single-frame result is exposed as visible banding/striations.
Especially visible in dark interiors where LDR quantization errors are most noticeable.

## Current Mitigation (Partial Fix)

These settings **reduce** striations significantly but do not fully eliminate them:

```ini
; TSR Flickering — increase parallax velocity tolerance for fast camera turns
r.TSR.ShadingRejection.Flickering.MaxParallaxVelocity=100
r.TSR.ShadingRejection.Flickering.Period=3

; TSR Resurrection — recover rejected history faster
r.TSR.Resurrection.PersistentFrameCount=16
r.TSR.Resurrection.PersistentFrameInterval=3
```

| Setting | Default | Tuned | Effect |
|---|---|---|---|
| `Flickering.MaxParallaxVelocity` | 10 | 100 | Reduced striations during fast turns |
| `Flickering.Period` | 2 | 3 | Slightly better temporal stability |
| `Resurrection.PersistentFrameCount` | 2 | 16 | More history frames saved for recovery |
| `Resurrection.PersistentFrameInterval` | 31 | 3 | History recovered faster after rejection |


## TSR CVars Tested — Full Log

### Settings That Helped

| Command | Default | Tested | Result |
|---|---|---|---|
| `r.TSR.ShadingRejection.Flickering.MaxParallaxVelocity` | 10 | 100 | **Improved** — reduced striations |
| `r.TSR.ShadingRejection.Flickering.MaxParallaxVelocity` | 10 | 200 | **Improved** but ghosting on lit surfaces |
| `r.TSR.ShadingRejection.Flickering.Period` | 2 | 3 | **Slightly improved** |
| `r.TSR.ShadingRejection.Flickering.Period` | 2 | 10 | **Worse** — ghosting on lit surfaces |
| `r.TSR.Resurrection.PersistentFrameCount` | 2 | 8 | **Improved** — faster history recovery |
| `r.TSR.Resurrection.PersistentFrameCount` | 2 | 16 | **Testing** |
| `r.TSR.Resurrection.PersistentFrameInterval` | 31 | 7 | **Improved** |
| `r.TSR.Resurrection.PersistentFrameInterval` | 31 | 3 | **Testing** |

### Settings That Did NOT Help

#### History & Temporal

| Command | Default | Tested | Result |
|---|---|---|---|
| `r.TSR.History.R11G11B10` | 1 | 0 | No change |
| `r.TSR.History.SampleCount` | 16 | 32 | No change |
| `r.TSR.History.UpdateQuality` | default | 3 | No change |
| `r.TSR.History.ScreenPercentage` | 200 | already 200 | N/A |

#### Velocity & Reprojection

| Command | Default | Tested | Result |
|---|---|---|---|
| `r.TSR.Velocity.WeightClampingPixelSpeed` | 1 | 4 | No change |
| `r.TSR.Velocity.Extrapolation` | default | 1 | No change |
| `r.TSR.Velocity.HoleFill` | default | 1 | No change |
| `r.TSR.ReprojectionField` | default | 0 | No change |
| `r.TSR.ReprojectionField.AntiAliasPixelSpeed` | 0.125 | 2 | No change |

#### Rejection & Exposure

| Command | Default | Tested | Result |
|---|---|---|---|
| `r.TSR.ShadingRejection.ExposureOffset` | 4 | 1 | Striations remain |
| `r.TSR.ShadingRejection.ExposureOffset` | 4 | 3 | Striations remain |
| `r.TSR.ShadingRejection.ExposureOffset` | 4 | 4+ | **Ghosting** in dark areas |
| `r.TSR.ShadingRejection.SampleCount` | 2 | 4 | **Worse** — more ghosting |
| `r.TSR.ShadingRejection.Flickering.FrameRateCap` | 60 | 30 | No change |
| `r.TSR.ShadingRejection.Flickering.AdjustToFrameRate` | 1 | 0 | No change |

#### Other

| Command | Default | Tested | Result |
|---|---|---|---|
| `r.TSR.AsyncCompute` | 2 | 0 | No change |
| `r.TSR.16BitVALU` | default | 0 | No change |
| `r.TSR.LensDistortion` | 1 | 0 | No change |
| `r.ScreenPercentage` | 100 | 85 | No change |

### Nanite Test

| Command | Tested | Result |
|---|---|---|
| `r.Nanite.Enable` | 0 | Striations remain (not Nanite-related) |

## Lumen CVars Tested (Not the Cause)

| Command | Default | Tested | Result |
|---|---|---|---|
| `r.Lumen.ScreenProbeGather.ScreenTraces` | 1 | 0 | No change |
| `r.Lumen.Reflections.ScreenTraces` | 1 | 0 | No change |
| `r.Lumen.ScreenProbeGather.Temporal.MaxFramesAccumulated` | ~6-8 | 20 | No change |
| `r.Lumen.ScreenProbeGather.Temporal.DistanceThreshold` | default | 30 | No change |
| `r.Lumen.ScreenProbeGather.MaxRayIntensity` | 10 | 2 | No change |
| `r.Lumen.ScreenProbeGather.DownsampleFactor` | 16 | 8 | **Worse** — coarse probe artifacts |

## Diagnostic Evidence

- `r.TSR.Visualize 2` (rejection mask): nearly entire screen turns pink during fast rotation = TSR rejects ~100% of history
- `r.TSR.ShadingRejection.ExposureOffset`: lowering threshold keeps striations, raising causes ghosting — no sweet spot exists
- Problem reproduces on multiple maps, not scene-specific
- Especially visible in dark interiors where LDR quantization errors are most pronounced

## Next Steps

- Continue tuning Resurrection settings (PersistentFrameCount/Interval) for best balance
- Monitor for ghosting side effects from current mitigation settings
- File Epic bug report at [issues.unrealengine.com](https://issues.unrealengine.com/) — TSR rejection during fast camera rotation, UE 5.7.4, RTX 3060
- Watch for TSR fixes in future UE patches

## References

- [TSR feedback thread (Epic Forums)](https://forums.unrealengine.com/t/tsr-feedback-thread/883977)
- [TSR degraded since 5.3+ (Epic Forums)](https://forums.unrealengine.com/t/why-tsr-5-1-worked-and-in-5-5-it-is-terrible/2224795)
- [TSR FAQ (Epic Docs)](https://dev.epicgames.com/documentation/en-us/unreal-engine/temporal-super-resolution-frequently-asked-questions-for-unreal-engine)
- [UE 5.7 known rendering issues](https://dev.epicgames.com/community/learning/knowledge-base/j2yV/unreal-engine-ue-5-5-x-most-common-rendering-issues)

<details>
<summary>Original Lumen-based analysis (outdated, click to expand)</summary>

### 1. Low-Precision Irradiance Buffer (Striations — Primary)

Lumen stores screen probe irradiance in R11G11B10_FLOAT (5-6 bits mantissa per channel).
In dark ranges, quantization steps become visible as hard color bands.
Standing still — temporal accumulation masks it. Fast turns — temporal history invalidated, raw quantized result exposed.

- **Current:** `r.Lumen.ScreenProbeGather.IrradianceFormat` at default (1 = R11G11B10)
- **Fix:** Set to `0` (RGBA16F, 16-bit float) — doubles precision, eliminates dark-scene banding
- **Cost:** ~0.2-0.3ms GPU
- **Source:** Daniel Wright (Epic) confirmed as known issue

### 2. Screen-Space Traces Failing During Fast Turns (Streaks)

Screen traces compare against previous frame's depth — completely wrong after sharp rotation.
Probes return invalid/black results, creating dark streaks aligned with probe grid.
Redundant since Hardware Ray Tracing is already enabled.

- **Current:** Screen traces enabled by default
- **Fix:** `r.Lumen.ScreenProbeGather.ScreenTraces=0`
- **Cost:** None (HWRT handles all traces)

### 3. Aggressive Temporal History Rejection (GI Popping)

Fast camera turns cause depth mismatches that reject accumulated history.
Recovery takes several frames, during which low-precision artifacts are fully visible.

- **Current:** `MaxFramesAccumulated` at default (~6-8)
- **Fix:** `r.Lumen.ScreenProbeGather.Temporal.MaxFramesAccumulated=20` and `r.Lumen.ScreenProbeGather.Temporal.DistanceThreshold=30`
- **Cost:** Minimal; slight ghosting on very fast movement

### 4. Tonemapper Dithering Disabled (Compounds Banding)

Removes blue-noise dithering that masks 8-bit output quantization.
Compounds the R11G11B10 issue even after temporal recovery.

- **Current:** `r.Tonemapper.GrainQuantization=0` (DefaultEngine.ini:116)
- **Fix:** Set to `1` (Epic default)
- **Cost:** Zero

### 5. Mesh SDF Tracing Disabled (Light Leak)

Without per-mesh distance field tracing, Lumen uses only coarse Global Distance Field.
Misses thin walls, allows sky/ambient light to bleed into enclosed rooms.

- **Current:** `r.Lumen.TraceMeshSDFs=0` (DefaultEngine.ini:80)
- **Fix:** Set to `1`
- **Cost:** ~5-10% GPU, essential for interiors

</details>
