# BP_SunSky_Child Setup Guide

## Overview

Configure BP_SunSky_Child to fix dark corners by adjusting Directional Light and Skylight parameters.

---

## Location

- **Blueprint:** `Content/Project/Placeables/Environment/BP_SunSky_Child.uasset`
- **Level:** `Plugins/World/City17/Content/Maps/City17_Persistent_WP.umap`
- **Placement:** **Persistent Level** (mandatory!)

---

## How to Open

### Option 1: Edit on Level (Recommended)

1. Open `City17_Persistent_WP.umap`
2. World Outliner -> Persistent Level -> find `BP_SunSky_Child`
3. Select -> Details panel
4. Expand components: `DirectionalLight` and `SkyLight`

**Use for:** Quick iteration, immediate preview in viewport.

### Option 2: Edit Blueprint

1. Content Browser -> `Content/Project/Placeables/Environment/`
2. Double-click `BP_SunSky_Child.uasset`
3. Blueprint Editor -> Components panel -> select component
4. Details panel -> configure Default Values

**Use for:** Permanent changes, version control.

---

## Directional Light Configuration

**Purpose:** Sun. Primary direct lighting source.

### Basic Parameters

| Parameter | Category | Value | Rationale |
|-----------|----------|-------|-----------|
| **Intensity** | Light | `9.0` lux | Realistic daylight (8-10 lux range) |
| **Light Color** | Light | RGB(255, 245, 235) | Neutral daylight, slightly warm |
| **Indirect Lighting Intensity** | Light | `1.2` | +20% Lumen bounce light |

### Shadow Parameters (CRITICAL!)

| Parameter | Category | Value | Rationale |
|-----------|----------|-------|-----------|
| **Shadow Amount** | Shadow | `0.85` | **KEY!** Lightens shadows 15%, fixes dark outdoor areas |
| **Source Angle** | Shadow | `0.5357` | Realistic sun disk size |
| **Shadow Bias** | Shadow | `0.3` | Reduces shadow acne |
| **Slope Bias** | Shadow | `0.4` | Prevents self-shadowing on slopes |
| **Cast Shadows** | Light | `True` | Uses Virtual Shadow Maps |

**Why Shadow Amount = 0.85:**
- Default `1.0` = fully dark shadows
- `0.85` = 15% lighter shadows
- Shaded outdoor areas become readable
- Details in shadows visible

---

## Skylight Configuration (CRITICAL!)

**Purpose:** Sky. Fill lighting for corners and shadow areas.

### Basic Parameters

| Parameter | Category | Value | Rationale |
|-----------|----------|-------|-----------|
| **Intensity** | Light | **`3.0`** | **KEY!** +200% fill light (was ~1.0) |
| **Source Type** | Light | `SLS Captured Scene` | Realtime environment capture |
| **Cubemap Resolution** | Skylight | `128` | Quality/memory balance |
| **Lower Hemisphere Color** | Light | RGB(30, 35, 40) | Weak ambient from below |
| **Cast Shadows** | Light | `True` | Natural sky shadowing |
| **Real Time Capture** | Skylight | `True` | Updates on lighting changes |

**Why Intensity = 3.0:**
- With Auto Exposure OFF, need compensation
- Skylight fills dark corners with indirect light
- Range 2.5-3.5 optimal for survival/realism
- Start with 3.0, adjust if needed

### Post-Configuration Action

**IMPORTANT:** After changing Directional Light settings:
1. Select Skylight component
2. Find "Recapture Scene" button
3. Press to update cubemap immediately

---

## Testing

### In Viewport

1. After changes -> viewport updates in realtime
2. Fly camera to dark corner
3. Check if details visible

### In PIE

1. Press Play In Editor
2. Navigate to problem areas:
   - Room corners
   - Under canopies
   - Between buildings
3. Verify corners 40-60% brighter

---

## Iteration Guide

**If corners still too dark:**

| Action | Parameter | Increase to | Effect |
|--------|-----------|-------------|--------|
| Boost sky | Skylight Intensity | 3.5 | +17% fill |
| Lighten shadows | Shadow Amount | 0.75 | +10% lighter |

**If too bright (washed out):**

| Action | Parameter | Decrease to | Effect |
|--------|-----------|-------------|--------|
| Reduce sky | Skylight Intensity | 2.5 | -17% fill |
| Darken shadows | Shadow Amount | 0.90 | -5% darker |

**If shadows too harsh:**

| Action | Parameter | Increase to | Effect |
|--------|-----------|-------------|--------|
| Softer shadows | Source Angle | 1.0 | Wider penumbra |

---

## Console Commands for Testing

Open console (` key) in PIE:

```
; Temporarily boost Skylight (test only)
r.Lumen.DiffuseIndirect.SceneMultiplier 1.5

; Visualize lighting
r.LightingOverride 0  ; Diffuse only
r.VisualizeAO 1       ; AO only

; Force Skylight recapture
r.SkyLight.RealTimeReflectionCaptureTimeSlice 0
```

---

## Saving Changes

### Option 1: Level Instance
- File -> Save Current Level (Ctrl+S)
- Changes saved in .umap

### Option 2: Blueprint
- File -> Save Selected (Ctrl+S)
- Changes saved in .uasset

**Recommendation:** Use Option 1 for faster iteration.

---

## Verification

1. Close editor
2. Re-open City17_Persistent_WP.umap
3. Select BP_SunSky_Child
4. Verify in Details:
   - Directional Light -> Shadow Amount = 0.85
   - Skylight -> Intensity = 3.0
5. Launch PIE -> check corners

---

## Common Issues

### Skylight Intensity keeps resetting to 1.0

**Cause:** Editing wrong instance (cell instead of Persistent Level)

**Fix:**
1. World Outliner -> switch to Persistent Level filter
2. Find BP_SunSky_Child in Persistent Level
3. Delete instances in streaming cells if any
4. Edit only Persistent Level instance

### Changes not visible in viewport

**Cause:** Skylight cubemap not updated

**Fix:**
1. Select Skylight component
2. Press "Recapture Scene"
3. Wait for cubemap update (~2-5 sec)

### Shadows still too dark after Shadow Amount = 0.85

**Cause:** Post Process AO overriding

**Fix:**
- See [Post Process guide](./lighting-postprocess.md)
- Set AO Intensity = 0.5
- Set Shadows Gain = 1.15

---

## Next Steps

After configuring BP_SunSky_Child:

1. [-> Configure Post Process](./lighting-postprocess.md)
2. Test in PIE
3. If issues -> [Troubleshooting](./lighting-troubleshooting.md)

---

## Back to Index

[<- Return to Lighting Setup Guide](./lighting-setup-editor.md)
