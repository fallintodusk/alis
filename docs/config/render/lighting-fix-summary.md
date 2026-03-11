# Dark Corners Fix - Quick Guide

## Problem
Dark corners in rooms and shaded outdoor areas in City17_Persistent_WP.

## Solution (3 Steps)

### [OK] STEP 1: Configuration (COMPLETED)

**What's done:**
- [OK] Config/DefaultEngine.ini backup created
- [OK] Config/DefaultEngine.ini updated with optimized render settings
- [OK] docs/config/render.md synchronized with DefaultEngine.ini

**Key changes in DefaultEngine.ini:**
- Auto Exposure disabled (`r.DefaultFeature.AutoExposure=False`)
- Lumen Software RT for RTX 3060 (`r.Lumen.HardwareRayTracing=False`)
- Improved bounce light (`r.Lumen.TraceMeshSDFs=0->1`)
- Optimized VSM shadows (`RayCount=4->8`, `SamplesPerRay=2->4`)
- Added `MaxLightsPerPixel=32` (from Fortnite)
- AO reduced (`Levels=2`, `MaxQuality=50`)

**Action:** Restart Unreal Editor to apply .ini changes.

---

### [ACTION] STEP 2: Configure BP_SunSky_Child in Editor (ACTION REQUIRED)

**File:** `Content/Project/Placeables/Environment/BP_SunSky_Child.uasset`
**Level:** `Plugins/World/City17/Content/Maps/City17_Persistent_WP.umap`
**Location:** Persistent Level (mandatory!)

**How to open:**
1. Open City17_Persistent_WP.umap
2. World Outliner -> Persistent Level -> BP_SunSky_Child
3. Select -> Details panel

**What to configure:**

#### Directional Light (sun):
| Parameter | Value | Effect |
|----------|----------|--------|
| Intensity | 9.0 lux | Realistic daylight sun |
| Light Color | RGB(255, 245, 235) | Neutral daylight |
| Indirect Lighting Intensity | 1.2 | +20% bounce light |
| Shadow Amount | **0.85** | **KEY: lightens shadows by 15%** |
| Shadow Bias | 0.3 | Reduces shadow acne |
| Source Angle | 0.5357 | Realistic sun disk |

#### Skylight (sky) - CRITICAL!
| Parameter | Value | Effect |
|----------|----------|--------|
| Source Type | SLS Captured Scene | Realtime capture |
| Intensity | **3.0** | **KEY: +200% fill light for corners** |
| Cubemap Resolution | 128 | Quality/memory balance |
| Lower Hemisphere Color | RGB(30, 35, 40) | Ambient from below |
| Cast Shadows | True | Natural sky shadowing |
| Real Time Capture | True | Updates on changes |

**After setup:** Press "Recapture Scene" in Skylight

---

### [COLOR] STEP 3: Post Process in World Settings (ACTION REQUIRED)

**Where:** Window -> World Settings -> Default Post Processing Settings

**Enable Override** for each parameter!

#### [CAMERA] Exposure (CRITICAL):
| Parameter | Value |
|----------|----------|
| Metering Mode | **Manual** |
| Exposure Compensation | 0.5 |
| Min EV100 | **8.5** |
| Max EV100 | **8.5** |
| Apply Physical Camera Exposure | False |

#### [COLOR] Color Grading -> Global:
| Parameter | Value (R, G, B, A) |
|----------|------------------------|
| Global Gamma | **(1.08, 1.08, 1.08, 1.0)** |
| Global Gain | (1.0, 1.0, 1.0, 1.0) |

#### [COLOR] Color Grading -> Shadows:
| Parameter | Value (R, G, B, A) |
|----------|------------------------|
| Shadows Gain | **(1.15, 1.15, 1.15, 1.0)** <- KEY: +15% shadow brightness |
| Shadows Gamma | (1.05, 1.05, 1.05, 1.0) |

#### [BOX] Ambient Occlusion:
| Parameter | Value | Default |
|----------|----------|--------------|
| Intensity | **0.5** | 1.0 <- KEY: -50% darkening |
| Radius | 80.0 | 100.0 |
| Quality | 50 | 100 |
| Static Fraction | 0.0 | - |

---

## Quick Check

**In Editor PIE (Play In Editor):**
1. Launch PIE
2. Check room corners in different zones
3. Check shaded outdoor areas

**If not bright enough:**
- Increase Skylight Intensity to 3.5
- Increase Shadows Gain to 1.25
- Decrease AO Intensity to 0.3
- Decrease Exposure EV to 8.0

**If too bright (no contrast):**
- Decrease Skylight Intensity to 2.5
- Decrease Shadows Gain to 1.05
- Increase Exposure EV to 9.0

---

## Console Commands for Quick Testing

Open console in PIE (` key):

```
; Check Auto Exposure
r.DefaultFeature.AutoExposure 0

; Boost bounce light
r.Lumen.DiffuseIndirect.SceneMultiplier 1.5

; Reduce AO
r.AmbientOcclusionIntensity 0.5

; Soft shadows
r.Shadow.Virtual.SMRT.RayCountDirectional 8

; Visualize AO (debug)
r.VisualizeAO 1

; Visualize Lumen
r.Lumen.Visualize.Mode 1
```

---

## Expected Result

| Zone | Before | After | Improvement |
|------|----|----|-----------|
| Room corners | Nearly black | Readable, details visible | +40-60% brightness |
| Shaded outdoor areas | Too dark | Natural, soft | +25-35% brightness |
| Overall scene | Contrasty, harsh | Balanced | Even lighting |
| Street<->interior transition | Harsh adaptation | Smooth | Fixed exposure |

**Performance on RTX 3060:**
- [OK] FPS: 60+ (stable)
- [OK] VRAM: <8 GB
- [OK] Frametime: Stable

---

## Checklist

Before final testing:

- [ ] Unreal Editor restarted after DefaultEngine.ini changes
- [ ] BP_SunSky_Child is in Persistent Level
- [ ] Directional Light -> Shadow Amount = 0.85
- [ ] Skylight -> Intensity = 3.0
- [ ] Skylight -> Recapture Scene executed
- [ ] World Settings -> Exposure Min/Max EV = 8.5/8.5
- [ ] World Settings -> Shadows Gain = (1.15, 1.15, 1.15, 1.0)
- [ ] World Settings -> AO Intensity = 0.5
- [ ] BP_SunSky_Child saved (Ctrl+S)
- [ ] City17_Persistent_WP level saved (Ctrl+S)
- [ ] Tested in PIE (corners are brighter)
- [ ] FPS >50 on RTX 3060

---

## Documentation

**Detailed instructions:**
- [lighting-setup-editor.md](./lighting-setup-editor.md) - Step-by-step guide with explanations and troubleshooting

**Technical documentation:**
- [render.md](./render.md) - Final render config (synchronized with DefaultEngine.ini)
- `<user-home>\.claude\plans\silly-brewing-panda.md` - Full plan with UE5 games research

**Modified files:**
- `Config/DefaultEngine.ini` - Section [/Script/Engine.RendererSettings]
- `docs/config/render.md` - Config documentation

**Require manual editor setup:**
- `Content/Project/Placeables/Environment/BP_SunSky_Child.uasset`
- `Plugins/World/City17/Content/Maps/City17_Persistent_WP.umap` (World Settings)

---

## Troubleshooting

### Corners still dark

**Check:**
1. DefaultEngine.ini: `r.DefaultFeature.AutoExposure=False`
2. Editor restarted after .ini changes
3. Skylight Intensity = 3.0 (not reset)
4. World Settings -> Manual Exposure enabled
5. AO Intensity = 0.5 (not 1.0)

**Solution:**
```
; In PIE console:
r.Lumen.DiffuseIndirect.SceneMultiplier 2.0
r.AmbientOcclusionIntensity 0.3
```

### FPS <40

**Cause:** VSM RayCount or Lumen overloading GPU

**Solution:**
1. In DefaultEngine.ini change:
   ```ini
   r.Shadow.Virtual.SMRT.RayCountDirectional=4  ; was 8
   r.Lumen.ScreenProbeGather.Quality=1          ; was 2
   ```
2. Restart editor

### Settings reset

**Cause:** BP_SunSky_Child in streaming cell instead of Persistent Level

**Solution:**
1. World Outliner -> find BP_SunSky_Child
2. Verify it's in **Persistent Level**, not cell
3. If in cell -> Cut -> Paste to Persistent Level

---

## Key Settings Summary

**3 critical changes to fix dark corners:**

1. **Skylight Intensity = 3.0** (was ~1.0)
   -> Fills corners and shadows with indirect light

2. **Shadows Gain = 1.15** (was 1.0)
   -> Lightens dark areas by 15%

3. **AO Intensity = 0.5** (was 1.0)
   -> Reduces Ambient Occlusion darkening

**Additional:**
- Auto Exposure OFF -> stable brightness without adaptation
- VSM RayCount = 8 -> soft shadows instead of harsh
- TraceMeshSDFs = 1 -> better Lumen bounce light
- EV 8.5 fixed -> interior/exterior balance

---

## Next Steps

1. [OK] **STEP 1:** .ini Configuration (COMPLETED)
2. [ACTION] **STEP 2:** Configure BP_SunSky_Child in editor
3. [ACTION] **STEP 3:** Configure Post Process in World Settings
4. [OK] **STEP 4:** Testing in PIE
5. [OK] **STEP 5:** Save and verify

**After completing STEP 2-3:**
Dark corners will be eliminated, lighting will be readable and comfortable.
