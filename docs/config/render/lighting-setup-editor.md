# Lighting Setup Guide - Index

## Overview

This guide fixes dark corners in interiors and shaded outdoor areas in City17_Persistent_WP by configuring:
1. BP_SunSky_Child (Directional Light + Skylight)
2. Post Process settings (World Settings)
3. Testing and troubleshooting

---

## Quick Start

### Prerequisites
- Config/DefaultEngine.ini updated (see [lighting-fix-summary.md](./lighting-fix-summary.md))
- Unreal Editor restarted after .ini changes
- City17_Persistent_WP.umap open

### 3-Step Process

**STEP 1: .ini Configuration** [OK] (COMPLETED)
- See [render.md](./render.md) for applied settings

**STEP 2: BP_SunSky_Child Setup** [ACTION] (ACTION REQUIRED)
- [-> Go to BP_SunSky_Child guide](./lighting-bp-sunsky.md)
- Configure Directional Light: Intensity 9.0 lux, Shadow Amount 0.85
- Configure Skylight: Intensity 3.0 (KEY FIX!)

**STEP 3: Post Process Setup** [ACTION] (ACTION REQUIRED)
- [-> Go to Post Process guide](./lighting-postprocess.md)
- Configure Exposure: Manual mode, EV 8.5 fixed
- Configure Color Grading: Shadows Gain 1.15, Global Gamma 1.08
- Configure AO: Intensity 0.5

---

## [!] IMPORTANT: World Partition

All settings must be in **Persistent Level**, NOT streaming cells!

**Verify before starting:**
1. Open City17_Persistent_WP.umap
2. World Outliner -> check **Persistent Level** is selected
3. BP_SunSky_Child must be in Persistent Level (not in cell)

**Why:** Settings in streaming cells reset when cells unload, causing lighting jumps.

---

## Expected Result

| Zone | Before | After | Improvement |
|------|--------|-------|-------------|
| Room corners | Nearly black | Readable | +40-60% brightness |
| Shaded outdoor | Too dark | Natural | +25-35% brightness |
| Overall scene | Harsh contrast | Balanced | Even lighting |
| Indoor<->outdoor | Harsh adaptation | Smooth | Fixed exposure |

**Performance on RTX 3060:** 60+ FPS stable

---

## Key Settings Summary

**3 critical changes:**

1. **Skylight Intensity = 3.0** (was ~1.0)
   -> Fills corners with indirect light

2. **Shadows Gain = 1.15** (was 1.0)
   -> Lightens dark areas by 15%

3. **AO Intensity = 0.5** (was 1.0)
   -> Reduces ambient occlusion darkening

**Additional:**
- Auto Exposure OFF (in DefaultEngine.ini)
- VSM RayCount = 8 (soft shadows)
- Lumen TraceMeshSDFs = 1 (better bounce light)

---

## Documentation Links

### Implementation Guides
- [BP_SunSky_Child Setup](./lighting-bp-sunsky.md) - Directional Light + Skylight
- [Post Process Setup](./lighting-postprocess.md) - Exposure, Color Grading, AO
- [Troubleshooting](./lighting-troubleshooting.md) - Common issues and solutions
- [Quick Reference](./lighting-fix-summary.md) - Condensed guide

### Technical Docs
- [render.md](./render.md) - Render config (synced with DefaultEngine.ini)
- `Config/DefaultEngine.ini` - Engine settings
- `<user-home>\.claude\plans\silly-brewing-panda.md` - Full plan with UE5 research

### Official UE5 Documentation
- [Lumen Global Illumination](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-global-illumination-and-reflections-in-unreal-engine)
- [Virtual Shadow Maps](https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine)
- [Post Process Effects](https://dev.epicgames.com/documentation/en-us/unreal-engine/post-process-effects-in-unreal-engine)
- [Color Grading](https://dev.epicgames.com/documentation/en-us/unreal-engine/color-grading-and-the-filmic-tonemapper-in-unreal-engine)

---

## Checklist

- [ ] Config/DefaultEngine.ini updated, editor restarted
- [ ] BP_SunSky_Child configured ([guide](./lighting-bp-sunsky.md))
- [ ] Post Process configured ([guide](./lighting-postprocess.md))
- [ ] Tested in PIE (corners brighter)
- [ ] FPS >50 on RTX 3060
- [ ] Changes saved (Ctrl+S on BP and level)

---

## Next Steps

1. Follow [BP_SunSky_Child guide](./lighting-bp-sunsky.md)
2. Follow [Post Process guide](./lighting-postprocess.md)
3. Test in PIE
4. If issues -> see [Troubleshooting](./lighting-troubleshooting.md)
