# Fix First-Person Body Clipping

Research and solution plan for the body mesh clipping issue in first-person view.

---

## Problem Statement

**Symptom:** When the player stops moving, the body (driven by motion matching) slides forward relative to the camera. The player can see inside the body mesh -- internal geometry, backfaces, eye sockets, etc.

**Root cause:** Camera is attached to the capsule component at a fixed offset (0, 0, 64), but motion matching animates the skeletal mesh root with inertia and overshoot. When stopping, the mesh continues forward while the capsule (and camera) stays put. This creates a gap between camera position and mesh head position.

**Current setup** (`ProjectCharacter.cpp:109-112`):

```
Camera -> attached to CapsuleComponent (RootComponent)
         -> SetRelativeLocation(0, 0, 64)
         -> bUsePawnControlRotation = true

Mesh   -> animated by motion matching (independent root motion)
         -> root bone drifts relative to capsule on stop/start
```

**Why this happens with motion matching specifically:**

- Motion matching selects animation clips that best match current velocity/trajectory
- Stop animations have deceleration curves where the mesh root continues forward
- The capsule stops instantly (or near-instantly via CharacterMovementComponent)
- Result: mesh overshoots capsule position for several frames

---

## Existing Mesh Architecture

The MotionMatching plugin (`Plugins/ThirdParty/MotionMatching/README.md`) defines a two-layer pattern:

```
Layer 1: MM Driver Mesh (hidden)
  - Runs Motion Matching animation (CPU-intensive pose search)
  - HiddenInGame = true (invisible)
  - Owns the AnimBP with MM state machine

Layer 2: Visual Character Mesh (visible)
  - Gets retargeted pose from Layer 1 via IK Retargeter
  - Does NOT run motion matching itself
  - This is what the player and other players see
  - Mutable (CustomizableObject) generates this mesh at runtime
```

**The headless local body is a third `USkeletalMeshComponent` on the actor.** It is NOT a passive follower -- it has its own tiny AnimBP that copies the final visual pose from WorldBodyMesh and applies small local-only upper-body corrections. This matters for animation cost (small extra graph eval), render cost, attachment checks, visibility debugging, and profiling.

```
Layer 1: MMDriverMesh (hidden)
  - Runs Motion Matching only once
  - HiddenInGame = true

Layer 2a: WorldBodyMesh (full body)
  - Final visible world pose
  - Follows driver via existing retarget / leader-follow path
  - bOwnerNoSee=true

Layer 2b: LocalBodyMesh (headless / camera-safe)
  - Owner-only variant (bOnlyOwnerSee=true)
  - Copies final pose from WorldBodyMesh via CopyPoseFromMesh
  - Applies tiny local-only upper-body anti-clip correction (ModifyBone)
  - Has its own AnimBP (ABP_AlisLocalBody)
```

Motion matching runs exactly **once** on the driver mesh. WorldBodyMesh is a cheap follower. LocalBodyMesh adds a small extra animation graph cost because `CopyPoseFromMesh` and `ModifyBone` nodes evaluate on that mesh each tick.

---

## Hard Constraints

These constraints are validated and non-negotiable. Any solution must respect all of them.

1. **Rigid camera.** No spring arm. Camera CANNOT be attached to animated bones -- causes motion sickness and removes player agency.
2. **Full animation fidelity.** Motion matching drives realistic inertia. Damage bends, stumbles, and stop overshoot must play fully. No Control Rig blocking or procedural deformation of upper body.
3. **Visible body.** Player expects to look down and see their physical character intact. No dithering, fading, or making the body invisible.

---

## Why Headless Local Body (and not the alternatives)

### Comparison Table

| Approach | Fixes clipping? | Rigid camera? | Full anim fidelity? | Body visible? | Performance | Verdict |
|----------|:-:|:-:|:-:|:-:|---|---|
| **Headless Local Body (Layer 2 split)** | YES | YES | YES | YES (legs+arms) | +1 component, +1 draw call | **CHOSEN** |
| Camera on head bone | YES | NO - motion sickness | YES | YES | Zero cost | Rejected: ruins UX |
| Control Rig spine lock | Partial | YES | NO - blocks damage anims | YES | Low | Rejected: ruins animations |
| Mesh fade/dither near camera | YES | YES | YES | NO - floating legs | Low | Rejected: breaks immersion |
| HideBoneByName (single mesh) | Head only | YES | YES | Partial | GPU still skins hidden verts | Rejected: torso still clips |
| Separate FP arms only | YES | YES | N/A (different mesh) | NO - arms only, no body | +1 draw call, new art | Rejected: no visible body |
| UE 5.7 Native FP Rendering | Partial (FOV/depth/anti-clip) | YES | YES | YES | Engine-managed | Complementary, not full fix |
| Additive counter-lean | Partial | YES | NO - procedural deformation | YES | Low | Contradicts constraint #2 |

### Why Headless Local Body wins

1. **Only approach that satisfies ALL three constraints** -- rigid camera, full animations, visible body
2. **Every other approach trades away at least one constraint** -- this one trades a small extra animation cost instead
3. **Performance tradeoff is acceptable** -- one extra component with a tiny AnimBP per character, profiling required to confirm in production

---

## Performance Tradeoff: Acceptable, Profile Required

### Motion Matching cost: runs once

Motion matching (the expensive part) runs once on the driver mesh (Layer 1). This is unchanged by the local body split.

### WorldBodyMesh: cheap follower

WorldBodyMesh follows the driver via LeaderPose/IK Retarget. Epic documents that LeaderPose reduces **game-thread** animation cost -- the follower does not tick its AnimBP.

### LocalBodyMesh: small extra anim cost

LocalBodyMesh has its own AnimBP with `CopyPoseFromMesh` + `ModifyBone` nodes. Unlike LeaderPose, `CopyPoseFromMesh` runs animation graph evaluation on the child mesh each tick. This is **more expensive** than a passive follower -- Epic is explicit about this distinction in the modular character docs. However, the graph is tiny (copy + 3 bone modifications), so the per-frame cost is small.

### Render-thread cost: additional, not zero

Each extra skeletal mesh component renders separately with its own draw calls, skinning pass, and visibility checks. Per viewer, only one Layer 2 variant is visible (the other is culled by `bOwnerNoSee`/`bOnlyOwnerSee`). So the render cost is +1 component overhead, not +2 visible meshes. The culling check itself is lightweight (scene proxy level, before draw commands are built).

### Headless mesh vs HideBoneByName

`HideBoneByName` scales bones to zero but the GPU still skins every vertex attached to hidden bones. A true headless mesh variant has fewer vertices to skin. For the specific goal of removing head geometry, a separate mesh is more efficient than hiding bones on a full mesh.

### Bottom line

The tradeoff is acceptable for a single player character. **Profile with `stat SceneRendering` and `stat anim` after implementation** to verify the cost is within budget on target hardware.

---

## Operational Cost: Minimal

### No duplicated gameplay logic

| System | Duplicated? | Why |
|--------|:-:|---|
| Motion matching | No | Runs on driver (Layer 1) only |
| Animation graph | No | Evaluated on driver only |
| Input / movement | No | Single character, single capsule |
| GAS / abilities | No | Single ASC on character |
| Weapon attachment | No | Attach to driver; both layers follow bones |
| AnimNotifies (footsteps, VFX) | No | Fire on driver's AnimBP only |
| Mutable appearance update | Shared | Same CO instance, both variants update together |
| Visibility flags | One-time setup | Set in constructor |

### What IS duplicated

1. **One extra `USkeletalMeshComponent`** in the character (Layer 2b)
2. **One extra `CustomizableObjectInstanceUsage`** linking Mutable to Layer 2b
3. **Visibility flag setup** in constructor
4. **Head geometry removal** re-applied after each Mutable update (in `UpdatedDelegate`)

The headless variant is a rendering concern, not a gameplay concern. All gameplay systems interact with the driver mesh or the character -- they don't know or care about Layer 2a vs 2b.

---

## Mutable Integration

Mutable (`CustomizableObject`) generates skeletal mesh assets at runtime for character customization.

**How it works with Layer 2 split:**

```
CustomizableObjectInstance (shared)
  |
  +-- CustomizableObjectInstanceUsage -> Layer 2a (World Body, full)
  +-- CustomizableObjectInstanceUsage -> Layer 2b (Local Body, headless)
```

**Key expectations (verify with regression tests):**
- Mutable updates the mesh asset inside a component, not component properties
- `bOwnerNoSee` / `bOnlyOwnerSee` are expected to survive Mutable regeneration (component-level properties, not mesh-level) -- **verify with a test after implementation**
- `UpdatedDelegate` fires per-instance -- use it to re-apply head geometry removal on Layer 2b after cosmetic changes
- Pattern already exists in `Infinite.cpp:195-229` which creates multiple components per character

**Headless variant options:**
- **Option A:** Same CO, remove head geometry via Mutable section toggle or CO parameter
- **Option B:** Same CO, remove head geometry in `UpdatedDelegate` callback on Layer 2b
- **Do NOT use bone scale = 0 on ancestor bones** (upper-spine, neck) -- scaling an ancestor affects the entire child chain and can break arm/shoulder/clavicle bones depending on skeleton hierarchy. Use true geometry removal (Mutable section, mesh LOD section, or vertex group exclusion) that preserves the full skeleton.

---

## UE 5.7 Native First-Person Rendering

The native FP rendering system (UE 5.5+) is **complementary, not a full fix**.

**What it provides:**
- Custom first-person FOV (arms/weapon at different FOV than world)
- Anti-clipping scale for near-camera geometry
- World-space representation for shadows/reflections
- Owner/world visibility split pattern

**What it does NOT solve:**
- Mesh-vs-capsule divergence during motion matching stop overshoot
- Automatic headless body management
- Geometry removal

**Known constraints:**
- Some features depend on deferred/GBuffer rendering
- Static lighting disabled for FP components
- Groom/hair not supported on FP components

**Verdict:** Worth a spike in Phase 3 to see if it simplifies visibility management or replaces manual `bOwnerNoSee`/`bOnlyOwnerSee` setup. Not sufficient alone.

---

## Chosen Solution: Headless Local Body with Copy-Pose Correction

Best practical architecture for rigid-camera full-body first person.

### Full Architecture

```
                    MMDriverMesh (Layer 1)
                    - Runs motion matching (once)
                    - HiddenInGame = true
                         |
                    IK Retarget / LeaderPose
                         |
                    WorldBodyMesh (Layer 2a)
                    - Full mesh, final visual pose
                    - bOwnerNoSee=true
                    - bCastHiddenShadow=true
                    - Other players see
                    - Casts player shadow
                    - Mutable cosmetics
                         |
                    CopyPoseFromMesh
                         |
                    LocalBodyMesh (Layer 2b)
                    - Headless (head geometry removed)
                    - bOnlyOwnerSee=true
                    - CastShadow=false
                    - Own tiny AnimBP (ABP_AlisLocalBody)
                    - ModifyBone corrections (spine_03, clavicles)
                    - Mutable cosmetics (same CO)
```

**What the player experiences:**
- Looks down: sees legs, arms, lower body -- all animated by motion matching
- Stops moving: body overshoots forward, but no head/upper geometry to clip into camera; small upper-body bias pushes torso away from camera
- Shadow on ground: full body silhouette with head (from Layer 2a)
- Other players: see full body with head

---

## Implementation Checklist

### Phase 1: Headless Local Body + LocalBody AnimBP

Core fix. Splits Layer 2 into world/local visibility variants. LocalBody gets its own
tiny AnimBP that copies pose from WorldBodyMesh and applies small additive bone corrections
to push upper body away from camera (anti-clip bias).

**Key decisions:**
- Do NOT copy or inherit Driver AnimBP -- Driver stays the only Motion Matching graph
- LocalBody AnimBP source = `WorldBodyMesh` (already retargeted final visual pose)
- Only use `MMDriverMesh` directly if local shares skeleton with driver and does NOT share
  final skeleton with world (`CopyPoseFromMesh` only copies matching bones)
- C++ base for variables/update logic, tiny BP graph for anim nodes
- No Linked Anim Layers until this grows later

#### 1a. Character setup

- [ ] Create headless mesh variant via Mutable section toggle or geometry removal (NOT bone scale on ancestors -- preserves full skeleton, removes only head-related geometry)
- [ ] Add `USkeletalMeshComponent* LocalBodyMesh` to character (Layer 2b)
- [ ] Wire retarget/LeaderPose from MM driver to WorldBodyMesh (Layer 2a only -- LocalBody copies from WorldBody, not driver)
- [ ] Add second `CustomizableObjectInstanceUsage` for Layer 2b
- [ ] In `UpdatedDelegate`: re-apply head geometry removal on Layer 2b after Mutable update
- [ ] Configure: world mesh `bOwnerNoSee=true`, local mesh `bOnlyOwnerSee=true`
- [ ] World mesh: `bCastHiddenShadow=true` for proper shadows
- [ ] Local mesh: `CastShadow=false` (shadow comes from world mesh)
- [ ] Add character API accessors:
  ```cpp
  USkeletalMeshComponent* GetWorldBodyMesh() const;
  USkeletalMeshComponent* GetLocalBodyMesh() const;
  UCameraComponent* GetViewCamera() const;
  ```

#### 1b. LocalBody AnimInstance (C++)

- [ ] Create `UAlisLocalBodyAnimInstance` (C++ base class)
  - `NativeInitializeAnimation()` -- cache character, set `SourceMeshComponent = WorldBodyMesh`
  - `NativeUpdateAnimation(DeltaSeconds)` -- compute correction values per tick
- [ ] Exposed properties (BlueprintReadOnly):
  - `SourceMeshComponent` -- for CopyPoseFromMesh node in BP
  - `ClipFixAlpha` -- master blend (0=off, 1=full correction)
  - `Spine03PitchDeg` -- spine_03 pitch offset
  - `ClavicleLYawDeg`, `ClavicleRYawDeg` -- clavicle yaw offsets
  - `ClavicleLRollDeg`, `ClavicleRRollDeg` -- clavicle roll offsets
- [ ] Alpha triggers (both contribute, take max):
  - Proximity: camera-to-chest distance (18..32 range, closer = more correction)
  - Look-down: control rotation pitch (-10..-45 range, steeper = more correction)
- [ ] Initial correction values (start small, tune later):
  - `Spine03PitchDeg = -8 * Alpha` (pitch back)
  - `ClavicleLYawDeg = +5 * Alpha`, `ClavicleRYawDeg = -5 * Alpha`
  - `ClavicleLRollDeg = -4 * Alpha`, `ClavicleRRollDeg = +4 * Alpha`

#### 1c. LocalBody AnimBP (Blueprint)

- [ ] Create `ABP_AlisLocalBody` with parent class = `UAlisLocalBodyAnimInstance`
- [ ] Graph (keep tiny):
  ```
  CopyPoseFromMesh(SourceMeshComponent)
   -> ModifyBone(spine_03, AddToExisting, ComponentSpace, Pitch=Spine03PitchDeg, Alpha=ClipFixAlpha)
   -> ModifyBone(clavicle_l, AddToExisting, ComponentSpace, Yaw=ClavicleLYawDeg, Roll=ClavicleLRollDeg, Alpha=ClipFixAlpha)
   -> ModifyBone(clavicle_r, AddToExisting, ComponentSpace, Yaw=ClavicleRYawDeg, Roll=ClavicleRRollDeg, Alpha=ClipFixAlpha)
   -> OutputPose
  ```
- [ ] Do NOT touch: root, pelvis, legs, lower spine, driver mesh, world mesh

#### 1d. Character wiring

- [ ] Set LocalBodyMesh anim instance class to `ABP_AlisLocalBody`
- [ ] Set tick prerequisite: `LocalBodyMesh->AddTickPrerequisiteComponent(WorldBodyMesh)`
  (Epic requires source mesh ticks before CopyPoseFromMesh consumer)

#### 1e. Validation

- [ ] Verify: `bOwnerNoSee`/`bOnlyOwnerSee` survive Mutable regeneration (regression test)
- [ ] Profile: `stat SceneRendering` and `stat anim` to confirm acceptable cost
- [ ] Test: stop from run/walk -- no clipping visible
- [ ] Test: look down -- legs/lower body visible and animated
- [ ] Test: multiplayer -- other players see full body with head
- [ ] Test: shadows cast correctly from world mesh
- [ ] Test: damage/stumble animations play on both meshes
- [ ] Test: Mutable cosmetic change updates both variants correctly
- [ ] Test: additive corrections look natural, no visible snapping
- [ ] Tune: if spine_03 + clavicles not enough, add `neck_01` ModifyBone

### Phase 2: Camera Pushback Safety Net

Belt-and-suspenders for edge cases (extreme bends, crouch transitions).

- [ ] Add custom collision channel for self-body trace
- [ ] Implement sphere trace in camera tick (forward from camera)
- [ ] Smooth pushback along camera local -X only (no rotation)
- [ ] Tune: trace radius, pushback speed, max offset
- [ ] Test: crouch/uncrouch transitions
- [ ] Test: damage animations that bend torso backward toward camera
- [ ] Test: verify zero pushback during normal gameplay (no phantom sliding)

### Phase 3: UE 5.7 Native FP Rendering (Spike)

Evaluate if engine-level first-person rendering simplifies or replaces parts of Phase 1.

- [ ] Research `bFirstPersonRendering` in UE 5.7 project settings
- [ ] Test custom FP FOV, anti-clipping scale, world-space representation
- [ ] Evaluate if it replaces manual `bOwnerNoSee`/`bOnlyOwnerSee` setup
- [ ] Check constraints: deferred rendering dependency, static lighting, groom support
- [ ] Adopt if it simplifies the pipeline, keep manual setup if not

---

## Decision: First Person vs Third Person

Alis uses **first-person view** with a rigid camera. This means:

- **Primary architecture:** headless local body split (removes head geometry from owner view)
- **Primary anti-clip behavior:** local copy-pose AnimBP with tiny upper-body bias (spine_03 + clavicles)
- **Secondary safety net:** camera pushback for edge cases (extreme bends, crouch transitions)
- **Optional engine spike:** UE 5.7 native first-person rendering -- may simplify visibility management

If Alis ever adds a tight third-person mode (ADS, over-shoulder), pushback becomes primary and the local body split may be simplified.

---

## References

| Resource | URL |
|----------|-----|
| UE5.7 First Person Rendering | https://dev.epicgames.com/documentation/en-us/unreal-engine/first-person-rendering |
| UE5.7 First Person Template | https://dev.epicgames.com/documentation/en-us/unreal-engine/first-person-template-in-unreal-engine |
| UE5.7 Modular Characters | https://dev.epicgames.com/documentation/en-us/unreal-engine/working-with-modular-characters-in-unreal-engine |
| Motion Matching Docs | https://dev.epicgames.com/documentation/en-us/unreal-engine/motion-matching-in-unreal-engine |
| Animation Optimization | https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-optimization-in-unreal-engine |
| HideBoneByName API | https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/USkinnedMeshComponent/HideBoneByName |
| CustomizableObjectInstanceUsage API | https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/CustomizableObject/UCustomizableObjectInstanceUsage |
| APlayerCameraManager API | https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/APlayerCameraManager |
| FAnimNode_ModifyBone API | https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/AnimGraphRuntime/FAnimNode_ModifyBone |
| Linked Anim Layers | https://dev.epicgames.com/documentation/en-us/unreal-engine/using-animation-blueprint-linking-in-unreal-engine |
| Community: MM in First Person | https://forums.unrealengine.com/t/community-tutorial-motion-matching-in-first-person-perspective/1987475 |

---

## Key UE5 API Quick Reference

| Property / Method | Purpose |
|-------------------|---------|
| `bOwnerNoSee` | Hide mesh from owning player only |
| `bOnlyOwnerSee` | Show mesh to owning player only |
| `bCastHiddenShadow` | Cast shadows even when mesh is hidden |
| `HideBoneByName(Name, PBO_None)` | Hide specific bone and children (scales to zero, still skins -- avoid on ancestors) |
| `SetLeaderPoseComponent()` | Share bone transforms, skip game-thread anim eval on follower |
| `CustomizableObjectInstanceUsage` | Bridge between Mutable CO instance and skeletal mesh component |
| `UpdatedDelegate` | Callback after Mutable finishes mesh regeneration |
| `bFirstPersonRendering` | UE5.5+ native FP render pass (FOV/depth/anti-clip) |
| `CopyPoseFromMesh` | Anim node: copies bone transforms from source skeletal mesh component |
| `FAnimNode_ModifyBone` | Anim node: additive/override rotation per bone (cheap local correction) |
| `AddTickPrerequisiteComponent()` | Ensures source mesh ticks before consumer (required for CopyPose) |
| `SweepSingleByChannel()` | Sphere/capsule trace for pushback |
| `FMath::FInterpTo()` | Smooth float interpolation |
