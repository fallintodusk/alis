# Post Process Setup Guide

## Overview

Configure Post Process settings via World Settings to fix dark corners through exposure, color grading, and ambient occlusion adjustments.

---

## Where to Configure

### Recommended: World Settings

1. Open `City17_Persistent_WP.umap`
2. Window -> World Settings
3. Find **Default Post Processing Settings** section
4. **Enable Override** checkbox for each parameter you change

**Why:** Applies globally to entire World Partition, survives streaming cell changes.

### Alternative: Post Process Volume

1. Place Post Process Volume in Persistent Level
2. Set `Unbound = True`
3. Set `Priority = 1` (higher than other PPVs)
4. Configure parameters in Details

**Use only if:** World Settings doesn't work for your setup.

---

## Exposure Configuration (CRITICAL!)

**Goal:** Disable Auto Exposure, set fixed EV for consistent brightness.

### Parameters

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| **Metering Mode** | `Manual` | Disables Auto Exposure completely |
| **Exposure Compensation** | `0.5` | +0.5 EV brightening |
| **Min EV100** | `8.5` | Fixed exposure lower bound |
| **Max EV100** | `8.5` | Fixed exposure upper bound (same as min!) |
| **Apply Physical Camera Exposure** | `False` | Don't use camera exposure |

**Why Min = Max:**
- Auto Exposure adapts by varying EV between min and max
- Setting both to `8.5` -> fixed exposure
- No brightness jumps when moving indoor<->outdoor

**EV 8.5 meaning:**
- EV 8.0 = brighter (better for interiors)
- EV 9.0 = darker (more realistic exteriors)
- EV 8.5 = balance between both

**Test range:** 8.0-9.0, adjust based on preference.

---

## Tone Mapping

**Goal:** Configure ACES Filmic tonemapper for cinematic look.

### Parameters

| Parameter | Value |
|-----------|-------|
| **Tone Curve Amount** | `1.0` |
| **Slope** | `0.88` |
| **Toe** | `0.55` |
| **Shoulder** | `0.26` |
| **Black Clip** | `0.0` |
| **White Clip** | `0.04` |

**Note:** Standard ACES values, no need to change unless specific artistic direction.

---

## Color Grading (KEY SETTINGS!)

**Goal:** Brighten shadows and overall scene without touching highlights.

### Global

| Parameter | Value (R, G, B, A) | Effect |
|-----------|-------------------|--------|
| **Global Gamma** | `(1.08, 1.08, 1.08, 1.0)` | +8% overall brightening, especially shadows |
| **Global Gain** | `(1.0, 1.0, 1.0, 1.0)` | No change (base multiplier) |

**How to set in UE:**
1. Expand Color Grading -> Global
2. Global Gamma -> R=1.08, G=1.08, B=1.08, A=1.0

### Shadows (CRITICAL!)

| Parameter | Value (R, G, B, A) | Effect |
|-----------|-------------------|--------|
| **Shadows Gain** | `(1.15, 1.15, 1.15, 1.0)` | **KEY!** +15% brightness in dark areas |
| **Shadows Gamma** | `(1.05, 1.05, 1.05, 1.0)` | Additional +5% shadow brightening |

**Why Shadows Gain = 1.15:**
- Directly multiplies shadow pixel values by 1.15
- 15% brighter dark corners
- Doesn't affect midtones/highlights
- Maintains contrast

**Test range:** 1.10-1.25 (higher = brighter shadows)

---

## Ambient Occlusion (Reduce Darkening)

**Goal:** Reduce AO darkening in corners and crevices.

### Parameters

| Parameter | Value | Default | Effect |
|-----------|-------|---------|--------|
| **Intensity** | **`0.5`** | 1.0 | **KEY!** -50% AO darkening |
| **Radius** | `80.0` | 100.0 | Slightly smaller search radius |
| **Quality** | `50` | 100 | Performance/quality balance |
| **Static Fraction** | `0.0` | - | Don't use static AO (dynamic lighting) |

**Why Intensity = 0.5:**
- AO darkens corners/crevices to simulate occlusion
- At 1.0 -> too aggressive darkening
- At 0.5 -> natural occlusion without over-darkening
- Works well with Lumen GI

**Test range:** 0.3-0.7 (lower = less darkening)

---

## Additional Settings

### Motion Blur (Optional)

**Recommendation:** Keep disabled for gameplay clarity.

| Parameter | Value |
|-----------|-------|
| **Motion Blur Amount** | `0.0` |

### Bloom (Optional)

**Recommendation:** Low or off for realistic lighting.

| Parameter | Value |
|-----------|-------|
| **Bloom Intensity** | `0.0` or `0.1` |

---

## Testing in PIE

### Quick Console Commands

Open console (` key):

```
; Check exposure fixed
r.DefaultFeature.AutoExposure 0
r.EyeAdaptationQuality 0

; Temporarily adjust AO
r.AmbientOcclusionIntensity 0.3  ; Test lower
r.AmbientOcclusionIntensity 0.7  ; Test higher

; Visualize AO
r.VisualizeAO 1  ; See AO only
r.VisualizeAO 0  ; Back to normal

; Check tonemapper
r.Tonemapper.GrainQuantization 0  ; Disable grain
```

### Verification

1. Launch PIE
2. Navigate to dark corner
3. Open console -> `r.VisualizeAO 1`
4. Check if AO is too strong (very dark areas)
5. If yes -> decrease AO Intensity
6. `r.VisualizeAO 0` -> back to normal

---

## Iteration Guide

**If corners still too dark:**

| Parameter | Current | Increase to | Effect |
|----------|---------|-------------|--------|
| Shadows Gain | 1.15 | 1.25 | +10% shadow brightness |
| Global Gamma | 1.08 | 1.12 | +4% overall brightness |
| Decrease AO Intensity | 0.5 | 0.3 | -20% AO darkening |
| Decrease Exposure EV | 8.5 | 8.0 | +0.5 EV brighter |

**If too bright (washed out):**

| Parameter | Current | Decrease to | Effect |
|----------|---------|-------------|--------|
| Shadows Gain | 1.15 | 1.05 | -10% shadow brightness |
| Global Gamma | 1.08 | 1.0 | No gamma adjustment |
| Increase AO Intensity | 0.5 | 0.7 | +20% AO darkening |
| Increase Exposure EV | 8.5 | 9.0 | -0.5 EV darker |

**If lack of contrast:**

| Parameter | Current | Adjust to | Effect |
|----------|---------|-----------|--------|
| AO Intensity | 0.5 | 0.7 | More depth in corners |
| Shadows Gamma | 1.05 | 1.0 | Less shadow lift |

---

## Saving Changes

### World Settings
- File -> Save Current Level (Ctrl+S)
- Settings saved in .umap

### Post Process Volume
- Select PPV -> File -> Save Current Level
- Settings saved in .umap

---

## Verification

1. Close editor
2. Re-open City17_Persistent_WP.umap
3. Window -> World Settings
4. Check Default Post Processing Settings:
   - Exposure Min/Max EV = 8.5/8.5 [OK]
   - Shadows Gain = (1.15, 1.15, 1.15, 1.0) [OK]
   - AO Intensity = 0.5 [OK]
5. Launch PIE -> corners brighter [OK]

---

## Common Issues

### Exposure still adapting (brightness jumps)

**Cause:** Auto Exposure not fully disabled

**Fix:**
1. Check DefaultEngine.ini: `r.DefaultFeature.AutoExposure=False`
2. Restart editor after .ini change
3. World Settings -> Metering Mode = Manual
4. Min EV = Max EV = 8.5

### Settings not applying

**Cause:** Override checkboxes not enabled

**Fix:**
1. World Settings -> Default Post Processing
2. Each parameter group has "Override" checkbox
3. Enable for: Exposure, Color Grading, AO
4. Change parameters
5. Save level

### AO still too strong

**Cause:** Multiple PPVs with conflicting settings

**Fix:**
1. World Outliner -> search "PostProcess"
2. Check all PPV priorities
3. Delete or disable conflicting PPVs
4. Use World Settings instead (recommended)

### Changes work in PIE but not in build

**Cause:** Some Post Process settings behave differently in packaged builds

**Fix:**
1. Package project (Development build)
2. Test in build
3. Adjust parameters slightly higher/lower
4. Re-package and test

---

## Performance Check

**After applying settings:**

1. PIE -> Press ` -> type `Stat FPS`
2. `Stat Unit` -> check GPU time
3. `Stat RHI` -> check render stats

**Target for RTX 3060:**
- FPS: 60+ stable
- GPU time: <16.6ms
- Frame time: <16.6ms

**If FPS low (<50):**
- AO Quality: 50 -> 25
- Disable Motion Blur (if enabled)
- Check [Troubleshooting guide](./lighting-troubleshooting.md)

---

## Next Steps

After configuring Post Process:

1. Test in PIE
2. Check dark corners -> should be 40-60% brighter
3. If issues -> [Troubleshooting](./lighting-troubleshooting.md)
4. Save everything (Ctrl+S)

---

## Back to Index

[<- Return to Lighting Setup Guide](./lighting-setup-editor.md)
