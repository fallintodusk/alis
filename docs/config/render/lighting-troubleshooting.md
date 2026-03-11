# Lighting Troubleshooting Guide

## Quick Diagnostics

### Problem: Corners still dark after applying all settings

**Check list:**
1. DefaultEngine.ini -> `r.DefaultFeature.AutoExposure=False` [OK]
2. Editor restarted after .ini changes [OK]
3. BP_SunSky_Child -> Skylight Intensity = 3.0 [OK]
4. World Settings -> Exposure Min/Max EV = 8.5 [OK]
5. World Settings -> Shadows Gain = 1.15 [OK]
6. World Settings -> AO Intensity = 0.5 [OK]

**Solutions:**

| Issue | Solution |
|-------|----------|
| Skylight reset to 1.0 | BP_SunSky_Child in streaming cell -> move to Persistent Level |
| Settings not applying | Enable Override checkboxes in World Settings |
| AO too strong | Decrease to 0.3, check for multiple PPVs |
| Auto Exposure still on | Verify .ini change, restart editor |

**Emergency fix (console commands):**
```
r.DefaultFeature.AutoExposure 0
r.Lumen.DiffuseIndirect.SceneMultiplier 2.0
r.AmbientOcclusionIntensity 0.3
```

---

### Problem: FPS too low (<40)

**Causes:**
1. VSM RayCount too high (8+)
2. Lumen overloading GPU
3. Too many light sources
4. High AO quality

**Solutions:**

**Quick fix (console):**
```
; Reduce VSM quality
r.Shadow.Virtual.SMRT.RayCountDirectional 4
r.Shadow.Virtual.SMRT.SamplesPerRayDirectional 2

; Reduce Lumen quality
r.Lumen.ScreenProbeGather.Quality 1
r.Lumen.TraceMeshSDFs 0  ; WARNING: lose bounce light!

; Reduce AO
r.AmbientOcclusionLevels 1
```

**Permanent fix:**

1. Edit DefaultEngine.ini:
   ```ini
   r.Shadow.Virtual.SMRT.RayCountDirectional=4  ; was 8
   r.Lumen.ScreenProbeGather.Quality=1          ; was 2
   ```

2. World Settings -> AO Quality = 25 (was 50)

3. Check light count:
   - Use MegaLights for many sources
   - Reduce overlapping lights

**Trade-offs:**
- RayCount 8->4: Harsher shadows, but +10-15 FPS
- TraceMeshSDFs 1->0: Less bounce light, but +5-10 FPS
- AO 50->25: Slightly less detail, but +3-5 FPS

---

### Problem: Settings reset on editor restart

**Cause:** BP_SunSky_Child or PPV in streaming cell instead of Persistent Level

**Solution:**

1. World Outliner -> top-right filter -> select **Persistent Level**
2. Find BP_SunSky_Child
3. If not in Persistent Level:
   - Right-click -> Cut
   - Switch to Persistent Level
   - Right-click -> Paste
4. Delete instances in streaming cells
5. Save level

**Verification:**
1. Close editor
2. Re-open level
3. World Outliner -> Persistent Level -> BP_SunSky_Child exists
4. Check settings preserved

---

### Problem: Harsh brightness jumps between zones

**Cause:** Auto Exposure still enabled

**Check:**

1. DefaultEngine.ini:
   ```ini
   r.DefaultFeature.AutoExposure=False
   r.EyeAdaptationQuality=0
   ```

2. World Settings -> Exposure -> Metering Mode = Manual

3. Console (in PIE):
   ```
   r.DefaultFeature.AutoExposure
   ; Should return: 0
   ```

**Fix:**
1. Verify .ini change saved
2. Restart editor (MUST restart after .ini change!)
3. Re-check World Settings

---

### Problem: Changes work in PIE but not in packaged build

**Cause:** Some settings behave differently in builds vs PIE

**Solution:**

1. Package Development build (not Shipping)
2. Test in build
3. Compare to PIE

**Common differences:**
- Exposure may be slightly different
- AO may appear stronger
- Shadows may be slightly darker

**Adjustment:**
- If build darker: Decrease Exposure EV to 8.0
- If build brighter: Increase Exposure EV to 9.0
- Re-package and test

---

## Advanced Diagnostics

### Visualize Lighting Components

**Console commands:**

```
; Show only diffuse light
r.LightingOverride 0

; Show only specular
r.LightingOverride 1

; Show only indirect diffuse (Lumen GI)
r.LightingOverride 2

; Show only indirect specular (reflections)
r.LightingOverride 3

; Back to normal
r.LightingOverride -1

; Visualize AO
r.VisualizeAO 1

; Visualize Lumen Scene
r.Lumen.Visualize.Mode 1  ; Overview
r.Lumen.Visualize.Mode 2  ; Surface cache
```

**How to use:**
1. Launch PIE
2. Navigate to dark corner
3. Open console -> `r.VisualizeAO 1`
4. If very dark -> AO too strong
5. Decrease AO Intensity to 0.3

---

### Check Lumen GI

**Console:**
```
; Boost indirect light temporarily
r.Lumen.DiffuseIndirect.SceneMultiplier 1.5

; If corners become brighter -> Lumen working
; If no change -> check TraceMeshSDFs enabled
```

**Verify TraceMeshSDFs:**
```
r.Lumen.TraceMeshSDFs
; Should return: 1
```

**If returns 0:**
1. Edit DefaultEngine.ini -> `r.Lumen.TraceMeshSDFs=1`
2. Restart editor

---

### Check VSM Shadows

**Console:**
```
; Visualize VSM
r.Shadow.Virtual.Visualize 1

; Show shadow LOD
r.Shadow.Virtual.Visualize 2
```

**Check soft shadows:**
```
r.Shadow.Virtual.SMRT.RayCountDirectional
; Should return: 8

r.Shadow.Virtual.SMRT.SamplesPerRayDirectional
; Should return: 4
```

---

## Performance Optimization

### Baseline Check

**In PIE:**
```
Stat FPS
Stat Unit
Stat Lumen
Stat RHI
```

**Target metrics (RTX 3060):**
- FPS: 60+
- GPU time: <16.6ms
- Frame time: <16.6ms

### Optimization Tiers

**Tier 1: Minor impact (-5% quality, +10% FPS)**
```ini
; DefaultEngine.ini
r.AmbientOcclusionMaxQuality=25         ; was 50
r.Shadow.Virtual.SMRT.SamplesPerRayDirectional=2  ; was 4
```

**Tier 2: Moderate impact (-10% quality, +20% FPS)**
```ini
r.Shadow.Virtual.SMRT.RayCountDirectional=4  ; was 8
r.Lumen.ScreenProbeGather.Quality=1          ; was 2
r.SSR.Quality=1                               ; was 2
```

**Tier 3: High impact (-20% quality, +30% FPS)**
```ini
r.Lumen.TraceMeshSDFs=0                ; Lose bounce light!
r.ReflectionMethod=0                   ; Disable SSR (use only Lumen Reflections)
```

**Apply tiers incrementally:** Test Tier 1 first, if FPS still low -> Tier 2, etc.

---

## Build Testing

### Create Test Build

1. Project -> Package Project -> Windows (64-bit)
2. Configuration: Development (not Shipping)
3. Wait for packaging
4. Launch .exe

### Comparison Checklist

| Aspect | PIE | Build | Match? |
|--------|-----|-------|--------|
| Dark corners brightness | | | [ ] |
| Shadow softness | | | [ ] |
| Exposure stability | | | [ ] |
| Overall brightness | | | [ ] |
| FPS | | | [ ] |

**If mismatch:**
- Build darker -> Adjust EV in World Settings
- Build brighter -> Adjust Shadows Gain
- FPS lower in build -> Reduce quality tier

---

## Console Command Reference

### Lighting

```
; Auto Exposure
r.DefaultFeature.AutoExposure 0
r.EyeAdaptationQuality 0

; Exposure
r.TonemapperGamma 1.08

; AO
r.AmbientOcclusionIntensity 0.5
r.AmbientOcclusionLevels 2
r.VisualizeAO 1

; Lumen
r.Lumen.DiffuseIndirect.SceneMultiplier 1.5
r.Lumen.TraceMeshSDFs 1
r.Lumen.Visualize.Mode 1

; VSM
r.Shadow.Virtual.SMRT.RayCountDirectional 8
r.Shadow.Virtual.SMRT.SamplesPerRayDirectional 4
r.Shadow.Virtual.Visualize 1

; Skylight
r.SkyLight.RealTimeReflectionCaptureTimeSlice 0  ; Force recapture

; Lighting visualization
r.LightingOverride 0  ; Diffuse only
r.LightingOverride 2  ; Indirect diffuse (Lumen GI)
```

### Performance

```
; Stats
Stat FPS
Stat Unit
Stat Lumen
Stat RHI
Stat GPU

; Quality reduction (temporary)
r.Lumen.ScreenProbeGather.Quality 1
r.Shadow.Virtual.SMRT.RayCountDirectional 4
r.AmbientOcclusionLevels 1
```

---

## Known Limitations

### World Partition Streaming

**Issue:** Lumen Surface Cache may lag when streaming cells load/unload

**Symptoms:**
- Brief lighting flicker on cell boundary
- GI updates delayed when entering new cell

**Mitigation:**
1. Increase loading radius (in World Partition settings)
2. Use smaller cell size for smoother streaming
3. Not fixable in current UE 5.5 architecture

---

### Large Cell Size (12800m)

**Current config:** City17_Persistent.ini -> CellSize=12800

**Impact:**
- Fewer streaming events (good for performance)
- Lumen cache slower to update (bad for dynamic GI)
- Large memory footprint per cell

**Alternative:** Use 256m cells (ALIS standard)
- More streaming events
- Faster Lumen updates
- Lower per-cell memory

**To change:**
1. Edit `City17_Persistent.ini`
2. Change `CellSize=256`
3. Re-import World Partition
4. **WARNING:** Major change, test thoroughly!

---

## Emergency Reset

**If everything broken:**

1. **Revert DefaultEngine.ini:**
   ```bash
   git checkout Config/DefaultEngine.ini
   ```

2. **Delete World Settings overrides:**
   - World Settings -> Default Post Processing
   - Disable all Override checkboxes

3. **Reset BP_SunSky_Child:**
   - Content Browser -> BP_SunSky_Child
   - Right-click -> Replace References
   - Create new instance

4. **Restart editor**

5. **Re-apply settings from scratch**

---

## Getting Help

**If issue persists after all troubleshooting:**

1. Capture evidence:
   - Screenshot of dark corner
   - Screenshot of World Settings
   - Screenshot of BP_SunSky_Child Details
   - `Stat Unit` screenshot

2. Check configuration:
   ```
   ; In PIE console
   r.DefaultFeature.AutoExposure
   r.Lumen.TraceMeshSDFs
   r.AmbientOcclusionIntensity
   ```

3. Review documentation:
   - [Lighting Setup Guide](./lighting-setup-editor.md)
   - [BP_SunSky_Child Guide](./lighting-bp-sunsky.md)
   - [Post Process Guide](./lighting-postprocess.md)

4. Check plan:
   - `<user-home>\.claude\plans\silly-brewing-panda.md`

---

## Back to Index

[<- Return to Lighting Setup Guide](./lighting-setup-editor.md)
