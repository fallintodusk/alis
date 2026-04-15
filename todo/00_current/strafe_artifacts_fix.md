# Fix Lumen Striations & Light Leak in Dark Interiors

## Problem

Visible striations/banding on walls and floors during fast camera turns in dimly lit rooms.
Light glare/leak in enclosed spaces that should be dark.
Not observed outdoors in bright light or when standing still.

## Root Causes

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

## Config Changes (DefaultEngine.ini)

All changes go after line 82 (`r.Lumen.ScreenProbeGather.RadianceCache=1`), except where noted:

```ini
; Line 80: change 0 to 1
r.Lumen.TraceMeshSDFs=1

; Line 116: change 0 to 1
r.Tonemapper.GrainQuantization=1

; Add after line 82:
r.Lumen.ScreenProbeGather.IrradianceFormat=0
r.Lumen.ScreenProbeGather.Temporal.MaxFramesAccumulated=20
r.Lumen.ScreenProbeGather.Temporal.DistanceThreshold=30
r.Lumen.ScreenProbeGather.ScreenTraces=0
r.Lumen.Reflections.ScreenTraces=0
```

## Testing Order (console ~ key, test before committing)

1. `r.Lumen.ScreenProbeGather.IrradianceFormat 0` — turn sharply in dark room, striations should disappear
2. `r.Tonemapper.GrainQuantization 1` — residual banding on dark walls gone
3. `r.Lumen.ScreenProbeGather.ScreenTraces 0` — dark streaks during fast turns reduced
4. `r.Lumen.ScreenProbeGather.Temporal.MaxFramesAccumulated 20` — smoother GI transitions
5. `r.Lumen.TraceMeshSDFs 1` — light leak in enclosed rooms reduced
6. `stat gpu` — verify performance acceptable

Step 1 alone should fix the majority of the issue.
