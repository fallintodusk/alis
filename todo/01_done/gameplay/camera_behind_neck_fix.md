# Camera Behind Neck Fix (First-Person)

**Status:** In Progress  
**Date:** 2026-04-16  
**Related:** [Test Report](16.04.26_test_report.md) | [Packaging Crash Fix](packaging_crash_hero_fix.md)

---

## Problem

In the packaged build, the first-person camera is positioned behind/inside the character's neck. The player sees the back of the LocalBody torso mesh instead of the game world.

> Screenshot reference: see [test report](16.04.26_test_report.md) for packaged build captures.

---

## Runtime Logs

```
LogDefinitionCharacter: ApplyViewConfig: Camera offset set to X=23.000 Y=0.000 Z=73.000
LogProjectSkeletalCapabilities: [LocalBody] IDLE NECK OFFSET: local=(X=-23.52, Y=-1.46, Z=-23.31) -- paste local into Hero.json neckOffset
LogProjectSkeletalCapabilities: [LocalBody] Spine tracking active: NeckOffset=X=-8.000 Y=0.000 Z=-10.000
LogInteraction: Warning: [InteractionComponent] SetupPostProcess: No CameraComponent found on owner 'DefinitionCharacter'
```

---

## Analysis

### Character Dimensions

| Metric | Value |
|---|---|
| Capsule radius | 23 |
| Capsule half-height | 88 |
| Capsule total height | 176 cm |
| Camera Z (from capsule center) | +73 |
| Camera height above ground | 88 + 73 = **161 cm** (eye level) |

### Camera Offset Math

The camera sits at `relativeOffset = (X=23, Y=0, Z=73)` from the capsule center. This places it 23 units forward and 161 cm above ground — correct for eye level on a 176 cm character.

### Root Cause: neckOffset Z Too Shallow

The LocalBody mesh is positioned relative to the camera via `neckOffset`. With `neckOffset.Z = -11`, the mesh sits only 11 units below the camera — **not enough** to clear the torso from the camera's view.

The runtime log recommends a deeper offset:

```
IDLE NECK OFFSET: local=(X=-23.52, Y=-1.46, Z=-23.31)
```

The mesh needs to be pushed down ~12 more units (from Z=-11 to Z=-23) so the neck/torso geometry falls below the camera frustum.

### Secondary Issue: Camera Creation Timing

`InteractionComponent` warns `No CameraComponent found` at `BeginPlay` because the camera is created dynamically in `ApplyViewConfig()` when the skeletal assembly reaches Ready state — after `BeginPlay`. This is a timing issue, not the visual bug, but should be addressed separately.

### Camera System Summary

- `UCameraComponent` attached to `RootComponent` (capsule)
- `bUsePawnControlRotation = true`
- `bUseControllerRotationYaw = false` (Motion Matching ABP handles body rotation)
- Camera created dynamically in `ApplyViewConfig()` when assembly reaches Ready state

---

## Fix Plan

### Step 1 — Update neckOffset in Hero.json

Change `neckOffset` from `(X=-24 Y=-1 Z=-11)` to `(X=-23 Y=-1 Z=-23)` to match the runtime-recommended idle neck offset.

```json
"view": {
    "attachmentPolicy": "CapsuleFixed",
    "cameraParent": "Root",
    "defaultMode": "FirstPerson",
    "neckOffset": "(X=-23 Y=-1 Z=-23)",
    "relativeOffset": "(X=23 Y=0 Z=73)"
}
```

### Step 2 — Verify in Packaged Build

Repackage and confirm the LocalBody torso mesh no longer blocks the camera view.

### Step 3 — Address InteractionComponent Warning (Follow-Up)

Investigate deferred camera lookup in `InteractionComponent` so the `No CameraComponent` warning is eliminated. Lower priority — does not affect visuals.

---

## File References

| File | Lines | Purpose |
|---|---|---|
| [Hero.json view section](../../Plugins/Resources/ProjectObject/Content/Human/Hero/Hero.json) | 122–128 | `relativeOffset` and `neckOffset` values |
| [DefinitionCharacter.cpp — ApplyViewConfig](../../Plugins/Gameplay/ProjectCharacter/Source/ProjectCharacter/Private/DefinitionCharacter.cpp) | 328–371 | Dynamic camera creation |
| [DefinitionCharacter.cpp — Capsule](../../Plugins/Gameplay/ProjectCharacter/Source/ProjectCharacter/Private/DefinitionCharacter.cpp) | 55 | Capsule size (23, 88) |
| [LocalBodyAnimInstance.cpp](../../Plugins/Gameplay/ProjectSkeletalCapabilities/Source/ProjectSkeletalCapabilities/Private/LocalBody/LocalBodyAnimInstance.cpp) | 19–46 | neckOffset loading from Hero.json |
| [InteractionComponent.cpp](../../Plugins/Gameplay/ProjectInteraction/Source/ProjectInteraction/Private/InteractionComponent.cpp) | 830 | Camera-not-found warning |

---

## Research — Industry Approaches

### Camera Attachment: Capsule Root vs Head Bone

| | Capsule Root | Head Bone Socket |
|---|---|---|
| Stability | Rock solid — no animation jitter | Inherits ALL head bone movement |
| Motion sickness | Low risk | High risk with unexpected head movement |
| Crouch | Must manually adjust Z | Automatic via skeleton |
| Industry use | **Dominant** (Lyra, Fortnite, most UE5 FPS) | Niche (VR, immersive sims) |

**Verdict:** Stay on capsule root. Motion Matching animations would cause judder on head bone.

### LocalBody-Camera Relationship (AAA Pattern)

```
Camera (on capsule, bUsePawnControlRotation=true)
  ↓ neckOffset pushes LocalBody below camera
LocalBody (OnlyOwnerSee, head hidden, spine IK toward camera)
WorldBody (OwnerNoSee, full body, casts shadow)
```

**Standard solutions for mesh-camera clipping:**
1. **NeckOffset / spine pull** — translate LocalBody so neck sits behind camera (our approach)
2. **Hide head + neck bones** — `HideBoneByName("head")` (already done)
3. **Near clip reduction** — `r.SetNearClipPlane 1.0` (causes Z-fighting, avoid)

### Eye Height Reference

| Target | Height from ground | Camera Z offset (capsule center at 88) |
|---|---|---|
| Anatomically correct (94% of 176cm) | 165cm | Z=77 |
| Game standard (slightly lower) | 161cm | Z=73 |
| Our current | 161cm | Z=73 ← correct |

### Sources

- [UE5 True First Person Camera Tutorial](https://dev.epicgames.com/community/learning/tutorials/Zme7/)
- [UE5.4 First Person + Motion Matching](https://dev.epicgames.com/community/learning/tutorials/6dRa/)
- [FPS Camera in Lyra Starter Game](https://dev.epicgames.com/community/learning/tutorials/RBpm/)
- [Camera Stable on Head Bone — Epic Forums](https://forums.unrealengine.com/t/keep-camera-stable-whilst-attached-to-head-bone/122413)
- [GASP Rotation at 0 Velocity — Epic Forums](https://forums.unrealengine.com/t/gasp-ue-5-5-cant-get-player-character-to-face-along-the-camera-forward-at-0-velocity/2484990)

---

## Related Docs

- [16.04.26 Test Report](16.04.26_test_report.md) — Packaged build test results
- [Packaging Crash Hero Fix](packaging_crash_hero_fix.md) — Related Hero.json packaging fix
