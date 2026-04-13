# Fix First-Person Body Clipping

Research and solution plan for the body mesh clipping issue in first-person view.

**SOT for:** first-person body clipping problem, architecture, failed attempts, next steps.
Referenced from `create_skeletal_assembly_framework.md`.

---

## Current Status (2026-04-13)

### Milestone: Clipping solved, filter needs idle polish

**Clipping during sprint-stop is solved.** FilterV1 eliminates all ray/proxy/capsule body
intrusions during transitions. Zero camera-volume violations across all 5 sprint-stop phases.
A slight upper-body stretch remains during extreme decel but is visually acceptable.

**Critical bug fixed:** `GetControlRotation().Pitch` returns 0-360 range in PIE (e.g. 270
for looking down) but tests used normalized -90. Added `FRotator::NormalizeAxis()` so the
filter now activates correctly in both PIE and automation.

**Next focus:** FilterV1 applies spine pitch correction during idle look-down which makes
the left hand position look unnatural. The filter should only apply the upper-chain
correction during movement transitions (sprint-stop, jump-land), not during idle standing.

---

## Previous Status (2026-04-10)

Phase 1 core architecture is **implemented but not validated**. The mesh split, copy-pose,
spine yaw, and neck anti-clip exist, but camera intrusion is still common during transitions.

### Automated Reproduction (2026-04-10)

Bug is now fully reproduced in `FirstPersonClipMatrixTest` with visual evidence.
Test runs 15 phases at -80 pitch (max look-down) covering idle, run, sprint-stop, jump,
crouch, uncrouch, strafe. Screenshots captured at first bad frame per phase.

Output: `Saved/Validation/ClipMatrix/` (JSONL + summary + edge screenshots)
Map: `Content/Project/Maps/Test/ClipMatrix_CleanMap` (flat floor, default character spawn)

Runner commands (PowerShell from project root):
```powershell
# Baseline etalon (no upper-chain filter, neck attach only)
./scripts/ue/test/character/capture_parity.ps1 -TestFilter "ProjectIntegrationTests.Character.FirstPerson.ClipMatrix.Baseline" -TimeoutSeconds 900

# FilterV1 (current guard-driven upper-chain correction)
./scripts/ue/test/character/capture_parity.ps1 -TestFilter "ProjectIntegrationTests.Character.FirstPerson.ClipMatrix.FilterV1" -TimeoutSeconds 900

# Both A/B in one shot (sprint-stop scenario)
./scripts/ue/test/character/capture_parity.ps1 -TestFilter "ProjectIntegrationTests.Character.FirstPerson.ClipMatrix.Baseline" -TimeoutSeconds 900
./scripts/ue/test/character/capture_parity.ps1 -TestFilter "ProjectIntegrationTests.Character.FirstPerson.ClipMatrix.FilterV1" -TimeoutSeconds 900

# Full matrix (all 15 phases, uses runtime default UpperChainMode)
./scripts/ue/test/character/capture_parity.ps1 -TestFilter "ProjectIntegrationTests.Character.FirstPerson.ClipMatrix.Default" -TimeoutSeconds 900
```

### Layer Isolation Results (2026-04-10)

Ran same test in 4 modes to find which layer intrusion starts in:

| Mode | Description | Failed Phases |
|---|---|---|
| A: DriverBody | Raw MM output, no retarget | 13/15 |
| B: WorldBody | Retargeted full body | 11/15 |
| C: LocalRaw | Owner-visible, SpineLock OFF | 11/15 |
| D: LocalCorrected | Owner-visible, SpineLock ON | 10/15 |

**Conclusion: the problem is the ANIMATION SOURCE, not the mesh layer.**
- All 4 modes fail on the same core phases (Run, Jump, Uncrouch, Strafe while looking down)
- Intrusion starts at DriverBody (MM pose) -- retarget and copy-pose don't change it
- SpineLock correction adds almost nothing (10 vs 11, only Idle_MaxDown differs)
- The neck hole is visible because HideBoneByName creates an open mesh edge, and the MM pose
  brings that edge into the camera's view during ANY dynamic movement at steep look angles

### Failed Fix Attempts (2026-04-09 through 2026-04-10)

| What was tried | Why it failed |
|---|---|
| Extended spine chain 3->5 bones + yaw redistribution | Compounding through hierarchy, head drifted OUT of camera |
| Spine pitch distribution (camera vertical -> body lean) | Bent body TOWARD camera, made clipping worse |
| Distance-based backward pitch push | Measured world distance, but neck is "behind" camera in local space -- push always near zero |
| Camera-local-X constraint | Camera forward points down when looking down, so "behind camera X" doesn't mean "not visible" |
| Ray-cone constraint (neck sphere) | Constraint reads source mesh, correction on local mesh -- can't converge because measurement doesn't see correction |
| UE 5.5+ FirstPersonScale on camera | GPU-only transform, doesn't change bone positions, confused test metrics, neck hole still visible in PIE |
| Crouch camera smoothing (FInterpTo) | Different problem, didn't help clipping |
| "Did bones rotate" test | Useless metric, proves nothing about visual clipping |

### What the data proves

The neck hole geometry problem exists at ALL layers because:
1. MM animation brings body close to camera during movement transitions
2. Head/neck_01 are hidden via HideBoneByName creating an open mesh edge
3. At -80 pitch, the camera looks directly at the neck stump area
4. No amount of spine rotation can close a geometry hole

### Remaining residual issues

1. **Neck hole visible when looking down during movement** (primary, proven in test)
2. **Fast camera Z rotation exposes back/shoulders** (secondary)
3. **Crouch stand-up visual pop** (separate issue)

### Why the current fix keeps failing

The current `LocalBodyAnimInstance` reacts to **camera yaw/pitch** (heuristic), not to the **actual distance between camera and neck/head/chest** (geometric). That works in idle, then fails on jump, sprint stop, crouch, or any transition where the animation drives the torso into the camera.

The current layer:
- Distributes yaw over 3 spine bones
- Adds small neck pitch anti-clip from control pitch only
- Does NOT answer: "Is the head/neck/upper torso currently entering the camera volume?"
- Only answers: "The player is looking down, so tilt neck a bit."

That is why it breaks on jump, braking, sprint stop, and transitional poses -- those are driven by locomotion animation, not by camera pitch alone.

### Failed Attempts (2026-04-09 through 2026-04-10) -- all reverted

| What was tried | Why it failed |
|---|---|
| Extended spine chain from 3 bones (01-03) to 5 (01-05) with redistributed yaw | Compounding through hierarchy made visible effect at spine_05 too strong, head drifted OUT of camera |
| Added spine pitch distribution (camera vertical -> body lean) | Bending body forward pushed neck/shoulders INTO camera view -- exact opposite of intended |
| Crouch camera smoothing (OnStartCrouch/OnEndCrouch + FInterpTo) | Did not help the reported issue |
| Integration test checking "did bones rotate" | Useless metric -- proves nothing about whether player sees inside body |
| Distance-based backward pitch push on neck_01 | Measured world distance which doesn't correlate with camera view; push always near zero |
| Camera-local-X constraint on neck_01 | Camera forward points down when looking down, so "behind camera X" doesn't mean "not visible" |
| Ray-cone constraint on neck_01 | **neck_01 is HIDDEN (HideBoneByName scales to zero) -- rotating it has ZERO visual effect** |
| UE 5.5+ FirstPersonScale on camera | GPU-only transform, doesn't fix geometry hole, confused test metrics |
| NeckLockNode targeting spine_05 with pitch only (-35 deg max) | Rotation alone can't close a geometry hole -- only tilts the edge, hole still visible |
| spine_05 scale collapse to 0.01 when looking down | Destroyed body shape -- no shoulders, body looks like a cone/tube |
| spine_05 scale to 0.6 + pitch when looking down | Test screenshots looked clean BUT PIE showed hole still visible during movement stops. Scale deforms shoulders and hand clips into body |
| Hiding spine_04+spine_05+spine_03 bones | Removed entire upper body -- player sees legs with waist stump |

### Key Discovery (2026-04-10)

**The NeckLockNode was targeting neck_01 which is HIDDEN via HideBoneByName.**
`HideBoneByName` scales the bone to zero for rendering. `GetBoneTransform` still returns
the skeletal position, but rotating a zero-scaled bone has zero visual effect.
The anim instance was working correctly the entire time -- bone diffs proved up to 115
degrees difference -- but the rotation was applied to an invisible bone.

spine_05 (bone index 6) is the last VISIBLE bone before the hidden neck_01 (index 7).
Full skeleton chain: pelvis -> spine_01 -> spine_02 -> spine_03 -> spine_04 -> spine_05 -> neck_01 -> neck_02 -> head

**The NeckLockNode MUST target a visible bone (spine_05) to have any visual effect.**
But spine_05 rotation/scale alone cannot close the geometry hole during movement transitions.

### Hard lessons

- Do NOT distribute pitch across spine bones (makes clipping worse)
- Do NOT add spine_04/05 to yaw chain without measuring compounding effect first
- `ControlRot.Pitch` can wrap (330 vs -30) -- normalize with `FRotator::NormalizeAxis()` first
- A real clipping test must check if body geometry enters camera volume, not bone rotation values
- Keep crouch fix as a separate commit from look-clipping fix
- Do not change bridge, mutable, local-body, and camera in one commit

---

## Policy Ownership (do not mix responsibilities)

| System | Responsibility | NOT responsible for |
|---|---|---|
| `MotionMatchingBridgeAnimInstance` | Feed sample ABP contract: movement mode, stance, rotation mode, gait, aim, input state | Camera anti-clip |
| `MutableCustomizationCapability` | Mutable/mesh rebuild/mapping policy | Visual policy for camera bugs |
| `LocalFirstPersonCapability` | Owner-only visibility, local body install, source binding | Pose correction |
| `LocalBodyAnimInstance` | Final camera-safety correction only | Bridge data, mutable state |
| `DefinitionCharacter` | Camera and movement only | Body clipping (keep crouch smoothing isolated) |

**Known impurity (2026-04-09):** `MutableCustomizationCapability` currently still owns
visual policy that belongs elsewhere: WorldBody promotion, skeletal mesh copy from
BodyCustomization to WorldBody, anim reinit, tick prerequisites, BodyCustomization hiding,
head/local-body leader-pose rewiring. This is a debugging hazard -- source can change
under you. Treat as known impurity: do not add more visual policy there, do not refactor
during clipping work, but **every test artifact must dump current promoted visual source
AND current copy-pose source on every phase**, not just once.

---

## Proven Dead Ends (do not retry)

- **Bone rotation on hidden bones** -- zero visual effect (HideBoneByName scales to zero)
- **Bone rotation on visible bones (spine_05)** -- tilts the edge but can't close the hole
- **Bone scale on visible bones** -- deforms body shape (cone/tube), hand clips into body
- **Hiding more bones** -- removes too much body (waist stump)
- **UE 5.5+ FirstPersonScale** -- GPU-only, doesn't affect skeleton, confused tests
- **Any heuristic based on camera pitch alone** -- fails during movement transitions when MM pose drives body into camera regardless of pitch
- **NeckLockNode targeting neck_01** -- neck_01 is hidden (HideBoneByName scales to zero), rotating it has zero visual effect
- **Camera-space geometric intrusion detection** -- measures source mesh, correction on local mesh, can't converge; measurement doesn't see its own correction
- **Alpha-driven spine pitch distribution (Phase 7, 2026-04-10)** -- rotating spine_03/04/05 backward moves ALL children (clavicles, arms, hands) with it. Eliminated bone intrusions (0/15) but caused arm-into-torso clipping. Counter-rotating clavicles made ray-hit failures worse (5/15 vs 4/15). The fundamental problem: additive bone rotation fights the skeleton hierarchy. You cannot rotate the spine without moving the arms. No amount of manual weight tuning or clavicle compensation can fix this -- it's the wrong abstraction.
- **Spine pitch + TwoBoneIK hand pin (Phase 8, 2026-04-10)** -- spine pushes body backward, TwoBoneIK pins hands to pre-correction positions. Automated test shows 0 bone intrusions and 10/15 pass. BUT PIE shows body pushed absurdly far back (looks like third-person), hands stretched ugly from IK fighting spine, core clipping problem still present during transitions. The automated test criteria are invalid -- passing the test means nothing visually. **"Push body away from camera" is the WRONG approach entirely.** The body should be ALIGNED to the camera, not pushed away from it.
- **All "push body backward" approaches** -- fundamentally wrong framing. The problem is not "body too close to camera." The problem is "camera and body are not aligned." Pushing body away creates third-person look, not first-person.

### Key Discovery: HiddenBones was unnecessary (2026-04-10)

**HiddenBones "head,neck_01" removed -- stump problem eliminated.**

The Head is a SEPARATE skeletal mesh component in Hero.json (visibility=SkipOwner).
The Body Mutable component (CO_Character Body) does NOT include head geometry.
The head bone exists on the shared skeleton but has NO vertices on the Body mesh.

Hiding neck_01 via HideBoneByName was scaling neck vertices to zero on the Body mesh,
creating an open mesh edge (the "neck stump"). The neck geometry is part of the Body
mesh and should STAY VISIBLE.

Result after removing HiddenBones:
- No floating head (Head mesh is SkipOwner -- owner never sees it)
- Neck visible with natural neck top (no open stump hole)
- Much better than before -- the gaping mesh edge is gone

**Remaining problem:** Player can see their nape (back of neck / upper back) during
transitions when MM animation moves body forward of camera. The body and camera are
not aligned during movement inertia.

## What Actually Works (from investigation)

1. **CopyPoseFromMesh works correctly** -- local body copies full MM pose from WorldBody
2. **The test infrastructure works** -- FirstPersonClipMatrixTest reproduces the bug with screenshots (15 phases, -80 pitch)
3. **Spine yaw tracking (01/02/03 at 40/30/30%)** -- camera yaw following works fine, keep it
4. **Alpha-driven pitch eliminated bone intrusions** -- 0/15 bone intrusions (was 10/15). But causes arm-into-body clipping because spine rotation moves everything above it.
5. **Removing HiddenBones eliminated the neck stump** -- Head is a separate mesh (SkipOwner). Body mesh has no head geometry. Hiding neck_01 was creating the open mesh edge.
6. **The CORRECT approach is to align upper body to camera** -- not push it away. Lock spine_05/neck to camera position so the body follows the view, not fight it.

## Next Fix Strategy: Spine Correction + TwoBoneIK Hand Pin (2026-04-10)

### Why additive bone rotation alone failed (Phase 7)

Rotating spine_03/04/05 backward eliminated bone intrusions (0/15 phases) BUT moved
clavicles and arms with the spine (skeleton hierarchy). Left hand clips into torso.
Counter-rotating clavicles made ray-hits worse. The problem: you cannot rotate parent
bones without moving children. But the SPINE CORRECTION ITSELF WORKED -- we just need
to put the hands back afterward.

### Why IKRig/FBIK asset path was rejected

Epic's IK Rig pipeline is asset-based: `UIKRigDefinition` asset required, editor-only
setup for goals/chains/stiffness. Investigated and abandoned -- too heavy for a
2-bone-per-arm correction. The asset exists but will not be used.

### Correct approach: two-step correction

**Step 1: Correct spine** (proven -- 0 bone intrusions in Phase 7)
**Step 2: Pin hands back** with TwoBoneIK (undo arm side-effect)

This is simpler, faster, and fully C++ with zero assets.

### How TwoBoneIK hand pinning works

```
BEFORE spine correction:
  hand_l is at position A (from MM via CopyPose)
  hand_r is at position B

AFTER spine correction:
  spine tilts backward
  hand_l moved to A' (wrong -- dragged by spine)
  hand_r moved to B' (wrong -- dragged by spine)

TwoBoneIK on left arm:
  target = A (saved before correction)
  solves: clavicle_l -> upperarm_l -> lowerarm_l -> hand_l
  result: hand_l back at A, elbow bends naturally

TwoBoneIK on right arm:
  target = B (saved before correction)
  result: hand_r back at B
```

The spine tilts backward (keeps upper body out of camera).
The arms solve naturally through the elbows to reach the original hand positions.
Hands never clip into the torso because they stay where MM put them.

### Pipeline (all C++, no assets)

```
CopyPoseFromMesh(WorldBody)
-> save hand_l + hand_r component-space transforms
-> ConvertLocalToCS
-> Spine01(yaw 40%) -> Spine02(yaw 30%) -> Spine03(yaw 30% + pitch)
-> Spine04(pitch) -> Spine05(pitch)
-> TwoBoneIK_ArmL (target = saved hand_l)
-> TwoBoneIK_ArmR (target = saved hand_r)
-> ConvertCSToLocal
-> Output
```

### What we reuse from Phase 7

- 4-alpha computation (DownLook, Brake, Air, Crouch) from CMC state -- KEEP
- Composite pitch clamped to [-40, 0] -- KEEP
- Spine pitch distribution across spine_03/04/05 -- KEEP (weights 20/35/45)
- All UPROPERTY tuning params -- KEEP
- Spine yaw tracking on spine_01/02/03 -- KEEP (unchanged)

### What is new

- `FAnimNode_TwoBoneIK` x2 in the proxy (left arm, right arm)
- Save hand transforms before spine correction (game thread, in NativeUpdateAnimation)
- Feed saved transforms as IK effector targets
- IKBone = hand_l / hand_r
- No new module deps (`FAnimNode_TwoBoneIK` is in AnimGraphRuntime, already used)

### FAnimNode_TwoBoneIK key settings

From `AnimGraphRuntime/Public/BoneControllers/AnimNode_TwoBoneIK.h`:
- `IKBone`: hand_l or hand_r (the end effector bone)
- `EffectorLocation`: saved hand world/component-space position
- `EffectorLocationSpace`: BCS_ComponentSpace
- `bAllowStretching`: false
- `bTakeRotationFromEffectorSpace`: true (preserve hand rotation)
- `bMaintainEffectorRelRot`: true

### Escalation path (if TwoBoneIK is not enough)

If elbows flip, hands drift, or the result looks wrong:
1. Try adjusting TwoBoneIK settings (stretch, rotation mode)
2. If still bad: use direct PBIK solver in C++ (no asset needed)
   - `FPBIKSolver` from `Plugins/Experimental/FullBodyIK/Source/PBIK/Public/Core/PBIKSolver.h`
   - Multi-goal, per-bone stiffness, constraint-based
   - Requires custom anim node wrapper but no editor asset
3. Last resort: IKRig asset with editor setup (already investigated, setup documented)

### References

- FAnimNode_TwoBoneIK: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/AnimGraphRuntime/FAnimNode_TwoBoneIK
- IKRig (rejected for now): https://dev.epicgames.com/documentation/en-us/unreal-engine/ik-rig-in-unreal-engine
- PBIK solver (escalation): https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/IKRig/FIKRigFullBodyIKSolver
- Copy Pose: https://dev.epicgames.com/documentation/en-us/unreal-engine/copy-a-pose-in-unreal-engine

### Test infrastructure (keep)

- `FirstPersonClipMatrixTest.cpp` -- 15 phases, -80 pitch, mid-phase screenshots
- 4 layer isolation modes (DriverBody/WorldBody/LocalRaw/LocalCorrected)
- Output: `Saved/Validation/ClipMatrix/`
- Runner: `capture_parity.ps1 -TestFilter "ProjectIntegrationTests.Character.FirstPerson.ClipMatrix"`

### Phase 2 - Prove what mesh the player is actually seeing

**Non-goal:** Phase 2 does not try to improve aesthetics. It only proves exactly which
tracked point enters the camera volume, in which layer, in which phase.

Add one JSON dump per phase with:
- mesh name, role tag, skeletal mesh asset
- anim class on each visible/follow mesh
- leader pose source, copy pose source
- hidden / ownerNoSee / onlyOwnerSee / castHiddenShadow
- current source chosen by `FindCopyPoseSource()`
- **current visible owner-view component name** (which mesh is actually rendered for owner)
- **current hidden owner-view component name** (which mesh is culled for owner)
- **whether LocalBodyCustomization or WorldBody is the actually rendered owner-view mesh**

### Phase 3 - Build one deterministic clip matrix test

Scripted phases, fixed timing, no manual input:

1. idle neutral
2. look down 30 / 60 / 80
3. run start / steady
4. sprint start / sprint stop + look down
5. jump rise / apex + look down / land
6. crouch idle + look down / crouch move + look down

Per frame capture: camera transform, actor transform, control rotation,
movement mode/speed/acceleration, **current promoted visual source, current copy-pose source**,
component-space and **camera-local** transforms for:
head, neck_01, spine_05, spine_03, clavicle_l, clavicle_r, upperarm_l, upperarm_r.

### Phase 4 - Measure the real failure (the missing piece)

Transform tracked bone points into **camera local space** each frame.
Define a **forbidden volume** around camera origin.

**Fail the frame** if any tracked point enters the forbidden volume.

Tracked points: head, neck_01, spine_05, clavicle_l, clavicle_r, upperarm_l, upperarm_r.

**Forbidden volume contract (initial threshold for first capture run, not yet acceptance threshold):**
- Shape: sphere
- Radius: 15 cm initial -- tune after first data shows real intrusion distances
- Origin: camera world location
- Sample rate: every frame
- Fail threshold: 3+ consecutive bad frames fail a phase (filter single-frame noise)
- These numbers are provisional until the first clip-matrix run produces real data

This is the real regression test: not "spine moved" but "body entered camera volume or did not."

### Phase 5 - Isolate the bad layer

Run the same test in four modes (existing clean-path isolation pattern):

1. DriverBody only visible
2. WorldBody visible
3. LocalBodyCustomization visible, no local correction
4. LocalBodyCustomization visible, with local correction

That tells exactly where the intrusion starts.

### Phase 6 - Implement camera-space safety constraint

Inside `LocalBodyAnimInstance`, after copy-pose:

1. Read camera transform
2. Compute all tracked points in camera local space
3. If any point enters forbidden volume, apply corrections in this priority:
   - First: upper-spine / clavicle / neck correction (rotate away from camera)
   - Then: tiny torso push (small enough to not desync feet/body feel)
   - Root shift only as last-resort micro correction (desyncs local body from world body)
   - Emergency fallback: owner-only hide on offending bone region

Root-first is wrong because it desyncs local feet/body from world body and creates new
visual lies. Upper-bone correction first is safer because it only affects the parts the
owner sees and does not change the body's grounding.

Per-frame hide must NOT become the primary system -- emergency fallback only.

---

## Problem Statement

**Symptom:** Player sees inside the body mesh -- internal geometry, backfaces, neck interior.

**Primary demonstrated failure mode:** MM root drift during inertia/stop. Camera is attached to capsule at fixed offset `(X=23 Y=0 Z=62)`, but MM root continues forward on stop animations. Mesh overshoots capsule for several frames.

**Other confirmed manifestations:**
- Fast camera Z rotation (vertical look) exposes back/shoulders
- Crouch stand-up transition causes visual pop/delay

**Current setup** (data-driven via `Hero.json` + `DefinitionCharacter`):

```
Camera -> attached to RootComponent (capsule)
         -> offset from Hero.json view section: (X=23 Y=0 Z=62)
         -> bUsePawnControlRotation = true

Mesh   -> animated by motion matching on DriverBody (independent root motion)
         -> root bone drifts relative to capsule on stop/start
```

---

## Existing Mesh Architecture

All mesh layers are defined in `Hero.json` and spawned by the skeletal assembly framework.
No hardcoded mesh subobjects on `ADefinitionCharacter` -- everything comes from data.

```
Hero.json meshes[]:

DriverBody (Layer 1 - hidden)
  - Runs Motion Matching (SandboxCharacter_CMC_ABP)
  - visibility: "Hidden"
  - Role tag: AssemblyRole=DriverBody

WorldBody (Layer 2a - other players see)
  - Retargeted from DriverBody via ABP_WorldBodyRetarget
  - visibility: "SkipOwner" (bOwnerNoSee=true)
  - Parent: DriverBody
  - Mutable output: BodyCustomization child (SkipOwner)

LocalBody (Layer 2b - owner sees, headless)
  - Owner-only variant (bOnlyOwnerSee=true)
  - Copies pose from WorldBody via ULocalBodyAnimInstance (pure C++)
  - Spine yaw tracking + neck anti-clip (ModifyBone nodes)
  - HiddenBones: head,neck_01 (via LocalFirstPerson capability)
  - Mutable output: LocalBodyCustomization child (OwnerOnly)

Head (separate mesh - SkipOwner)
  - bOwnerNoSee=true, bCastHiddenShadow=true
  - Leader-follows WorldBody (or DriverBody during early init)
  - Mutable output: HeadCustomization child (SkipOwner)
```

**Capabilities** (from Hero.json, installed by assembly framework):
- `SkeletalAssembly` -- mesh lifecycle, state machine
- `MotionMatching` -- installs bridge PostProcess ABP on DriverBody
- `MutableCustomization` -- COI clone, CSK creation, mesh promotion
- `LocalFirstPerson` -- visibility policy, bone hiding, LocalBodyAnimInstance install
- `DebugCapture` -- diagnostic JSON + screenshots

Motion matching runs exactly **once** on DriverBody. WorldBody is a retarget follower. LocalBodyCustomization has its own C++ anim graph (CopyPose + 3 ModifyBone + NeckLock).

---

## Hard Constraints

These constraints are validated and non-negotiable. Any solution must respect all of them.

1. **Rigid camera.** No spring arm. Camera CANNOT be attached to animated bones -- causes motion sickness and removes player agency.
2. **Full animation fidelity.** Motion matching drives realistic inertia. Damage bends, stumbles, and stop overshoot must play fully. No Control Rig blocking or procedural deformation of upper body.
3. **Visible body.** Player expects to look down and see their physical character intact. No dithering, fading, or making the body invisible.

---

## Architecture Rationale (decision made, implemented)

Headless local body was chosen because it satisfies all three hard constraints (rigid camera, full animation fidelity, visible body) at the cost of +1 mesh component + small anim graph.

Rejected alternatives: camera on head bone (motion sickness), Control Rig spine lock (blocks damage anims), mesh fade/dither (breaks immersion), separate FP arms only (no visible body), HideBoneByName on single mesh (torso still clips).

Performance: MM runs once on DriverBody. WorldBody retargets cheaply. LocalBodyCustomization has a tiny C++ anim graph (CopyPose + 4 ModifyBone). Per viewer only one body variant renders (bOwnerNoSee/bOnlyOwnerSee culling). Profile with `stat SceneRendering` and `stat anim` to verify budget.

---

## Mutable Integration (as built)

`MutableCustomizationCapability` handles all Mutable wiring via Hero.json properties.

```
COI_Hero (cloned preset instance)
  |
  +-- CSK -> BodyCustomization (child of WorldBody, SkipOwner)
  +-- CSK -> HeadCustomization (child of Head, SkipOwner)
  +-- CSK -> LocalBodyCustomization (child of LocalBody, OwnerOnly)
```

ComponentNameMapping in Hero.json: `BodyCustomization=Body,HeadCustomization=Head,LocalBodyCustomization=Body`

**How visibility survives Mutable rebuild:**
- Mutable replaces mesh assets inside CSK children, resets some component flags
- `LocalFirstPersonCapability` binds to `COI.UpdatedNativeDelegate` and re-applies:
  - HideBoneByName(head, neck_01) on LocalBody + LocalBodyCustomization
  - bOwnerNoSee/bOnlyOwnerSee flags
  - Reinstalls ULocalBodyAnimInstance if Mutable reset the anim class
  - Retries groom attachment after 0.5s (async groom binding)

**Head hiding approach:** `HideBoneByName` on head,neck_01 (not bone scale on ancestors).
Scales bone to zero but preserves skeleton hierarchy for animation. GPU still skins hidden verts -- acceptable cost for single player character.

---

---

## Chosen Solution: Headless Local Body with Copy-Pose Correction

Best practical architecture for rigid-camera full-body first person.
Fully implemented via Hero.json + capability framework. No hardcoded mesh subobjects.

### Full Architecture (as built)

```
DriverBody (Hero.json mesh, visibility=Hidden)
  - Runs Motion Matching once (SandboxCharacter_CMC_ABP)
  - MotionMatchingCapability installs bridge PostProcess ABP
       |
  ABP_WorldBodyRetarget (IK Retarget)
       |
WorldBody (Hero.json mesh, visibility=SkipOwner)
  - Full body, final visible pose for other players
  - bOwnerNoSee=true
  - Head mesh: bCastHiddenShadow=true (shadow with head)
  - Mutable: BodyCustomization child
       |
  ULocalBodyAnimInstance (CopyPoseFromMesh, pure C++)
       |
LocalBody (Hero.json mesh, visibility=OwnerOnly)
  - Headless: head,neck_01 hidden via LocalFirstPerson capability
  - bOnlyOwnerSee=true, CastShadow=false
  - Spine yaw tracking: spine_01/02/03 (40/30/30%)
  - Neck anti-clip: neck_01 backward pitch when looking down
  - Mutable: LocalBodyCustomization child
```

**What the player experiences:**
- Looks down: sees legs, arms, lower body -- all animated by motion matching
- Stops moving: body overshoots forward, but no head/upper geometry to clip into camera; spine yaw keeps upper body aligned with camera
- Shadow on ground: full body silhouette with head (from WorldBody/Head)
- Other players: see full body with head (WorldBody + BodyCustomization)

---

## Implementation Checklist

### Phase 1: Headless Local Body + LocalBody AnimInstance -- IMPLEMENTED, NOT VALIDATED

Core split exists. Hero.json defines world/local visibility mesh variants.
LocalBodyCustomization gets `ULocalBodyAnimInstance` (pure C++) that copies pose from
WorldBody and applies spine yaw tracking + neck anti-clip.

Camera intrusion is still common during transitions -- Phase 1 validation blocked on
Phase 2-6 (clip matrix test + geometric constraint).

**Key decisions (as implemented):**
- Pure C++ anim graph via `FLocalBodyAnimInstanceProxy` -- no Blueprint ABP needed
- Copy-pose source = WorldBody (preferred), fallback to DriverBody during early init only
- `LocalFirstPersonCapability` installs the anim instance and manages visibility lifecycle
- No Linked Anim Layers -- graph is small enough for direct node chain

#### 1a. Character setup

- [x] Create headless mesh variant via HideBoneByName on head,neck_01 (Hero.json HiddenBones property)
- [x] Add LocalBody mesh to character (Layer 2b) -- via SkeletalAssembly framework, Hero.json meshes[]
- [x] Wire retarget from MM driver to WorldBody (ABP_WorldBodyRetarget)
- [x] LocalBody copies from WorldBody via CopyPoseFromMesh (LocalBodyAnimInstance)
- [x] Mutable integration: MutableCustomizationCapability creates CSKs for all mesh roles
- [x] In Mutable UpdatedDelegate: re-apply head geometry removal + visibility (LocalFirstPersonCapability)
- [x] Configure: world mesh bOwnerNoSee=true, local mesh bOnlyOwnerSee=true
- [x] World mesh: Head mesh bCastHiddenShadow=true for proper shadows
- [x] Local mesh: CastShadow=false (shadow comes from world mesh)

#### 1b. LocalBody AnimInstance (C++) -- implemented

`ULocalBodyAnimInstance` + `FLocalBodyAnimInstanceProxy` in ProjectSkeletalCapabilities.
Pure C++ -- no Blueprint ABP. Graph is small enough for direct node chain.

Pipeline: `CopyPose -> CS -> Spine01 -> Spine02 -> Spine03 -> NeckLock -> Local -> Output`

- [x] CopyPoseFromMesh from WorldBody (falls back to DriverBody during early init)
- [x] Spine yaw tracking: spine_01/02/03 additive rotation from control-actor yaw delta (40/30/30%)
- [x] Neck anti-clip: neck_01 backward pitch when looking steeply down ([-15,-45] -> [0,-8] deg)
- [x] Source mode enum for deterministic testing (Auto/WorldBodyOnly/DriverBodyOnly)
- [x] Source change logging (every change, not just once)
- [x] Pitch wrapping fix: FRotator::NormalizeAxis(ControlRot.Pitch)
- [x] Camera-space geometric intrusion detection (2026-04-09):
  - Tracks neck_01, spine_05, head distances to camera
  - Warning radius 35cm, ramps correction alpha from 0 to 1
  - PushPitchDeg computed from camera forward dot with bone direction
  - Distributed across spine (20/30/50%) + neck (30%), upper-bone first
  - Camera pointer cached (no per-frame GetComponents allocation)
  - One-frame bone transform lag documented (acceptable, ramp-in margin absorbs)

#### 1d. Character wiring -- implemented

- [x] LocalBodyCustomization gets ULocalBodyAnimInstance installed by LocalFirstPersonCapability
- [x] LeaderPose cleared before AnimInstance install (prevents stale leader-pose state)
- [x] AlwaysTickPoseAndRefreshBones on LocalBody mesh

#### 1e. Validation -- PARTIAL

- [x] bOwnerNoSee/bOnlyOwnerSee survive Mutable regeneration (LocalFirstPersonCapability re-applies)
- [ ] Profile: stat SceneRendering and stat anim
- [ ] Test: stop from run/walk -- STILL CLIPS (residual issue #1)
- [x] Test: look down -- legs/lower body visible and animated
- [ ] Test: multiplayer -- other players see full body with head
- [x] Test: shadows cast correctly (Head mesh bCastHiddenShadow)
- [ ] Test: damage/stumble animations play on both meshes
- [x] Test: Mutable cosmetic change updates both variants correctly
- [ ] Tune: residual clipping needs better approach (see failed attempt above)

### Implementation Phase 2: Completed diagnostic + test infrastructure

- [x] Phase 0: freeze bridge/mutable/camera/retarget -- only touch diagnostics + local-body
- [x] Phase 1: make copy-pose source deterministic (ELocalBodySourceMode enum + change logging)
- [x] Phase 2: JSON dump per phase proving visible mesh identity (in FirstPersonClipMatrixTest)
- [x] Phase 3: build FirstPersonClipMatrix test with camera-local bone transforms (18 phases)
- [x] Phase 4: add forbidden-volume assertions (15cm sphere, 3 consecutive frame threshold)
- [x] Phase 6: implement camera-space constraint in LocalBodyAnimInstance (replaced by Phase 7)

### Implementation Phase 7: Alpha-Driven Upper-Chain Override -- WRONG APPROACH (2026-04-10)

Implemented and tested. Eliminated bone intrusions (0/15) but caused arm-into-torso
clipping because spine rotation moves ALL children. Counter-rotating clavicles made
ray-hits worse. The approach is fundamentally wrong -- manual additive bone rotation
fights the skeleton hierarchy. Code reverted to spine-only (no clavicles) as interim.

- [x] Remove NeckLockNode (targets hidden neck_01, zero visual effect)
- [x] Add Spine04Node, Spine05Node (ModifyBone, additive CS pitch)
- [x] Add 4-alpha computation (DownLook, Brake, Air, Crouch) from CMC state
- [x] Add UPROPERTY tuning params
- [x] Build and test: 0 bone intrusions, 4 ray-hit failures, arm-into-body clipping
- [x] Tried clavicle counter-rotation: made it worse (5/15 vs 4/15)
- [x] Conclusion: wrong abstraction. Need constrained solver, not manual rotation.

### Implementation Phase 8: Spine Correction + TwoBoneIK Hand Pin -- PARTIALLY SUCCESSFUL (2026-04-10)

Two-step C++ approach: spine pitch pushes upper body back, TwoBoneIK on each arm
pins hands to pre-correction positions. Implemented and tested in 3 iterations.

#### Results

| Attempt | Changes | Failed Phases | Notes |
|---|---|---|---|
| 8a | Spine pitch + TwoBoneIK alpha=1.0 | 5/15 | RunStop regressed (was 4/15 without IK). Full IK pin pulls arms forward toward camera |
| 8b | Soft IK blend (alpha 0-0.5 ramped by pitch) | 5/15 | RunStop rayHits 53->17 but still fails. Same 5 phases |
| 8c | Stronger correction (-45/-25/-20/-20/-18, clamp -55) | 5/15 | CrouchRun 20.5cm, Uncrouch 20.1cm -- barely changed. Spine correction at physical limit |

**Key finding:** ALL 5 failures are ray-hit-only (boneIntrusion=0 in ALL 15 phases).
The camera forward ray passes within 25cm sphere of spine_05 during forward-leaning
transitions. The ray hits the **neck stump mesh surface** (visible in Uncrouch screenshot).
No amount of spine rotation can close this open mesh edge.

**What the automated test says:**
- 0/15 bone intrusions, 10/15 phases pass automated test

**What PIE actually shows (2026-04-10, manual verification):**
- Body pushed absurdly far back -- looks like third-person, not first-person
- Hands stretched/ugly from TwoBoneIK fighting spine correction
- Core problem (seeing inside body during transitions) still 100% present
- The automated test passes phases that LOOK WRONG in actual gameplay
- **Conclusion: the test criteria are invalid. Passing the test means nothing if the visual result is unacceptable.**
- **Conclusion: "push body away from camera" is the WRONG framing of the problem.**

#### Implementation steps (completed)

- [x] Brought back Phase 7 spine pitch code (Spine04/05 + 4-alpha system)
- [x] Added FAnimNode_TwoBoneIK x2 for hand pinning
- [x] Saved hand transforms from source mesh (pre-correction)
- [x] Soft IK blend: alpha ramps 0->0.5 based on pitch magnitude
- [x] Tested 3 correction strength levels
- [x] Screenshots confirm: idle/run/strafe clean, neck stump visible in crouch/uncrouch/fall

### Implementation Phase 9: Attach Upper Body to Camera (next)

HiddenBones removed -- neck stump gone. Remaining problem: player sees their nape
(back of neck / upper back) during transitions because MM moves body forward of camera.

**Root cause reframed:** The camera is at a fixed capsule offset. The body drifts
independently via MM root motion. When the body drifts forward (sprint stop, jump,
crouch), the camera stays behind and the player sees the back of their own neck/upper body.

**Fix: lock upper body to camera.** Instead of pushing body away, PULL it to where
the camera is. The neck/spine_05 should track the camera position so the body is
always correctly aligned with the player's viewpoint.

**How it works:**
1. Get the eye/head position from the WorldBody mesh (which has the full head)
2. Use that as reference for where the camera "should be" relative to the body
3. In LocalBodyAnimInstance, after CopyPose, use ModifyBone to TRANSLATE spine_05
   so it stays aligned with the camera position
4. The spine chain between pelvis and spine_05 stretches/compresses naturally
5. Lower body (legs, pelvis) follows MM as before -- no change to locomotion feel

**Key difference from all previous attempts:**
- Previous: rotate spine to push body away -> third-person look, ugly
- New: translate spine_05 to camera position -> body stays where eyes are

#### Measured head bone heights (2026-04-10, from WorldBody head bone logs)

| State | ActorZ | HeadZ | Head-Actor | Cam-Actor (current) | Gap |
|---|---|---|---|---|---|
| GROUND idle | 278 | 332 | **54** | 62 | camera 8cm above head |
| JUMP apex | 368 | 381 | **12** | 62 | camera 50cm above head |
| CROUCH settled | 250 | 278 | **28** | 62 | camera 34cm above head |

Camera offset is fixed Z=62 (Hero.json `relativeOffset`). Head bone moves dynamically
with MM animations: tucks during jump, lowers during crouch, leans in idle.
A single fixed offset cannot match all states.

#### Implementation steps

Phase 9a: Dynamic camera Z tracking (in DefinitionCharacter)
- [ ] Each tick: read head bone Z from WorldBody (source mesh with full head)
- [ ] Compute dynamic offset: `headBoneZ - actorZ` (smoothed with FInterpTo)
- [ ] Set `FirstPersonCamera->SetRelativeLocation(FVector(23, 0, dynamicZ))`
- [ ] Where: `DefinitionCharacter.cpp` line 349 area (where ViewConfig.RelativeOffset is applied)
- [ ] Smooth with FInterpTo to avoid jitter (speed ~10-15 for responsive but not jarring)
- [ ] Fallback: if no head bone found, use Hero.json static offset
- [ ] File: `Plugins/Gameplay/ProjectCharacter/Source/ProjectCharacter/Private/DefinitionCharacter.cpp`

Phase 9b: Neck lock to camera (in LocalBodyAnimInstance)
- [x] BMM_Replace in BCS_ComponentSpace -- correct method, locks bone firmly
- [x] BMM_Additive attempted -- DOES NOT WORK during dynamic MM transitions (stop/start/jump), too weak
- [x] Debug arrows: Blue=camera, Red=head, Yellow=src neck, Green=corrected neck, Cyan=neck fwd
- [x] Logs: per-state position data every tick during jump/crouch, every 1s on ground
- **Dead end: BMM_Additive** -- cannot keep up with MM root drift during transitions
- **Dead end: Replace neck_01 AT camera position** -- stretches neck forward because camera is at eyes, neck is behind/below eyes
- **Validated approach (2026-04-12): Replace neck_01 at camera + calibrated neck offset**
  - Source of truth is Hero.json `sections.view.neckOffset`
  - Runtime target is `camera_world + actor_rotation * neckOffset`
  - Implemented in `ULocalBodyAnimInstance::NativeInitializeAnimation()` + `NativeUpdateAnimation()`
  - Neck offset is now loaded once on anim init, not re-read from disk every tick
- [x] Implement neck target using calibrated camera-space neck offset

Phase 9d: Automated stop repro + upper-spine inertia correction (2026-04-12)
- [x] Updated `FirstPersonClipMatrixTest` to reproduce `SprintStop_MaxDown` explicitly
- [x] Added sprint control path to the test (`StartSprint` / `StopSprint` reflection with speed fallback)
- [x] Disabled screenshot capture in the matrix run to keep timing/logs clean
- [x] Added `rayBone` / `nearestRayHitBone` logging to summary and warnings
- [x] Confirmed the failure bone is `spine_05`, not `neck_01`
- [x] Added bounded upper-spine retreat on visible chain (`spine_03` + `spine_05`)
  - retreat is driven in camera-local space by distance from the camera forward ray
  - starts only when looking down and `spine_05` enters the clearance cylinder
  - uses additive component-space translation only; no root move, no IK asset, no full-body solver
- [x] Tuned retreat safety margin and weight split to target transition inertia without reopening idle/strafe

Validated artifacts:
- Baseline repro: `Saved/Validation/ClipMatrix/20260412_153855_D_LocalCorrected_clip_matrix_summary.json`
  - `failedPhaseCount = 6`
  - `RunStop_MaxDown` nearest ray hit = `9.7 cm`
  - all logged failures were torso ray hits, now attributed by test instrumentation to `spine_05`
- Final validated run: `Saved/Validation/ClipMatrix/20260412_160659_D_LocalCorrected_clip_matrix_summary.json`
  - `failedPhaseCount = 2`
  - `SprintStop_MaxDown`: `32` ray-hit frames, nearest ray hit = `19.0 cm`, bone = `spine_05`
  - `RunJumpLand_MaxDown`: `33` ray-hit frames, nearest ray hit = `19.2 cm`, bone = `spine_05`
  - `Idle_MaxDown`, `Sprint_MaxDown`, `JumpFall_MaxDown`, `CrouchRun_MaxDown`, `Uncrouch_MaxDown` are now clean

What the 2026-04-12 data proves:
- Hard neck lock is the correct foundation. The neck itself is no longer the failing surface.
- The remaining issue is upper-body inertia after stop/landing: the camera ray still intersects `spine_05` for ~0.27-0.28s while movement speed is already `0`.
- This is a visible torso-chain problem, not a reason to escalate to full IK yet.
- Next escalation, if needed, should stay local to the visible upper chain:
  - add a narrowly targeted stop/landing alpha for upper-spine retreat or pitch clamp
  - do NOT reintroduce generic spine pitch distribution across the whole chain
  - do NOT move the capsule/root to solve this

Iteration: strict max-down repro hardening (2026-04-12, second pass)
- [x] Raised scripted look-down from soft `-80` to strict requested `-89`
- [x] Logged actual camera pitch and `cameraForward dot -Up` per sample
- [x] Added targeted first-person failure screenshots for:
  - `SprintStop_MaxDown`
  - `RunJumpLand_MaxDown`
  - `CrouchRun_MaxDown`
- [x] Added a weighted upper-torso proxy point:
  - `proxy = spine_05 * 0.55 + clavicle_l * 0.225 + clavicle_r * 0.225`
  - recorded `proxyCamLocal`, `proxyPerpDist`, and proxy ray-hit metrics in timeline/summary
- [x] Added phase-onset edge logs with exact pitch, speed, crouch/fall state, `spine_05` local position, and proxy local position
- [x] Exposed `LocalBodyAnimInstance.GetCurrentSourceName()` so the matrix can record the live copy-pose source instead of `unknown`

Strict baseline artifacts:
- `Saved/Validation/ClipMatrix/20260412_163436_D_LocalCorrected_clip_matrix_summary.json`
- `Saved/Validation/ClipMatrix/20260412_163436_D_LocalCorrected_SprintStop_MaxDown_EDGE_t0.38_p-89.0_ray29.6_proxy-1.0.png`
- `Saved/Validation/ClipMatrix/20260412_163436_D_LocalCorrected_RunJumpLand_MaxDown_EDGE_t0.41_p-89.0_ray32.3_proxy-1.0.png`
- `Saved/Validation/ClipMatrix/20260412_163436_D_LocalCorrected_CrouchRun_MaxDown_EDGE_t0.39_p-89.0_ray20.9_proxy-1.0.png`

Strict baseline result:
- `failedPhaseCount = 4`
- `SprintStop_MaxDown` no longer fails consecutively at `-89`, but still logs a one-frame torso edge contact at `29.6 cm`
- `JumpFall_MaxDown` is a real full-down failure now:
  - `rayHitFrames = 43`
  - `proxyHitFrames = 36`
  - nearest proxy hit `22.8 cm`
- `RunJumpLand_MaxDown` remains failing:
  - `rayHitFrames = 13`
  - `proxyHitFrames = 8`
  - nearest ray hit `18.5 cm`
  - nearest proxy hit `17.4 cm`
- `CrouchRun_MaxDown` now reproduces exactly under strict max-down:
  - `rayHitFrames = 68`
  - `proxyHitFrames = 62`
  - nearest ray hit `15.2 cm`
  - nearest proxy hit `14.5 cm`
- `Uncrouch_MaxDown` also fails under strict max-down:
  - `rayHitFrames = 26`
  - `proxyHitFrames = 22`

What the stricter repro changed:
- The previous matrix under-reported the real issue because `-80` was too forgiving.
- The visible failure family is broader than stop/landing alone once the camera is truly maxed downward:
  - crouch-forward
  - uncrouch
  - jump-fall / air top
  - run-jump landing
- The failure still belongs to the visible upper torso chain, not the hidden neck bone:
  - all new failures are still attributed to the visible `spine_05` / upper torso region
  - actual logged camera pitch is `-89.0`, `downDot = 1.000`

Next fix direction after strict repro:
- Keep the existing `neck_01` hard attach unchanged
- Move the visible upper chain based on the weighted upper-torso proxy, not only raw `spine_05`
- Add chain distribution and a small backward upper-spine pitch clamp so the torso folds away instead of only translating

Iteration: chain guard rollout (2026-04-12, final)
- [x] Added `spine_04` to the LocalBody anim chain so the visible upper torso can be corrected across `spine_03 -> spine_04 -> spine_05`
- [x] Kept the existing hard `neck_01` attach untouched
- [x] Switched the upper-body guard from raw `spine_05` center to a weighted upper-torso proxy
- [x] Added additive upper-spine pitch guard on the visible chain
- [x] Changed retreat direction from camera-back bias to mostly perpendicular camera-local push
- [x] Added state-aware guard strength:
  - crouch boost
  - landing hold
  - low-speed settle boost only when the torso proxy is already intruding
- [x] Added settle-time focus so more of the correction lands on `spine_05`
- [x] Added a final `spine_05` settle keep-out on top of the broader torso proxy guard

Intermediate validation history:
- `20260412_163948_D_LocalCorrected`
  - first chain pass
  - improved strict repro from `4/15` to `4/15` with smaller windows
  - `JumpFall_MaxDown` improved strongly, but `CrouchRun_MaxDown` and `RunJumpLand_MaxDown` still failed
- `20260412_164345_D_LocalCorrected`
  - perpendicular push + stronger state multipliers
  - reduced to `1/15` under screenshot-enabled repro
  - `CrouchRun_MaxDown`, `JumpFall_MaxDown`, `Uncrouch_MaxDown` clean
  - only `RunJumpLand_MaxDown` remained
- `20260412_164943_D_LocalCorrected`
  - logs-only authoritative gate exposed that screenshot capture was hiding a remaining stop/landing residue
  - still `2/15` failed: `SprintStop_MaxDown`, `RunJumpLand_MaxDown`
- `20260412_165254_D_LocalCorrected`
  - stronger settle multipliers alone did not solve the remaining stop/landing residue
  - still `2/15` failed
- `20260412_165721_D_LocalCorrected`
  - settle-time focus pushed more of the correction toward `spine_05`, but was still not sufficient
  - still `2/15` failed

Final validated result:
- `Saved/Validation/ClipMatrix/20260412_170032_D_LocalCorrected_clip_matrix_summary.json`
  - `failedPhaseCount = 0`
  - strict requested look-down stayed at `-89`
  - all target edge cases now clean:
    - `SprintStop_MaxDown`
    - `JumpFall_MaxDown`
    - `RunJumpLand_MaxDown`
    - `CrouchRun_MaxDown`
    - `Uncrouch_MaxDown`

Final visual artifact references:
- `Saved/Validation/ClipMatrix/20260412_163436_D_LocalCorrected_SprintStop_MaxDown_EDGE_t0.38_p-89.0_ray29.6_proxy-1.0.png`
- `Saved/Validation/ClipMatrix/20260412_163436_D_LocalCorrected_RunJumpLand_MaxDown_EDGE_t0.41_p-89.0_ray32.3_proxy-1.0.png`
- `Saved/Validation/ClipMatrix/20260412_163436_D_LocalCorrected_CrouchRun_MaxDown_EDGE_t0.39_p-89.0_ray20.9_proxy-1.0.png`
- `Saved/Validation/ClipMatrix/20260412_164345_D_LocalCorrected_CrouchRun_MaxDown_CHECK_t0.90_p-89.0_ray-1.0_proxy-1.0.png`

Important harness note:
- Default matrix capture is back to logs-only (`bCapturePhaseScreenshots = false`).
- First-person screenshots are still useful for visual review, but they can perturb tight landing timing enough to distort the strict automated matrix.
- Use the saved strict artifact runs above for the visual history, and use `20260412_170032_D_LocalCorrected` as the authoritative automated gate.

Reopened: proxy-based "green" was a false pass (2026-04-12, live runtime audit)
- [x] Checked the newest live runtime log instead of trusting the proxy/ray-only matrix
- [x] Confirmed the previous chain-guard conclusion was wrong
  - `Saved/Logs/Alis.log:8413` shows `AIR` `HeadDelta=(-5.6,2.7,-50.4)|50.8| CorrDelta=26.4`
  - `Saved/Logs/Alis.log:8202` shows `GROUND` `HeadDelta=(-15.6,-0.5,-25.5)|29.9| CorrDelta=26.4`
  - `Saved/Logs/Alis.log:8207` shows `CROUCH` `HeadDelta=(6.3,4.1,28.1)|29.1| CorrDelta=26.4`
- [x] Root cause of the false pass:
  - the matrix was grading only camera-ray / torso-proxy intrusion
  - the live bug is also a **neckline stretch** case where the copied source neck/head drifts far from the fixed neck target even when the torso proxy is not inside the camera ray
- [x] Mark previous approach as failed for final validation
  - the `spine_03/04/05` proxy-chain guard reduced some ray hits
  - it did **not** solve the real visible stretch defect in stop / air / crouch edge cases
- [x] Replace the matrix pass condition with direct source-head / source-neck vs fixed-neck-target stretch metrics
- [x] Re-run strict repro and capture the real failing phases under the new metric
- [x] Implement a direct upper-chain neck-gap constraint in C++ and revalidate

Follow-up failed attempts after the false green (2026-04-12)
- [x] Harness fix: make matrix failures fail the test for real
  - `FirstPersonClipMatrixTest.cpp` now uses `AddError`, not `AddWarning`, when any phase fails
  - `capture_parity.ps1` now exits non-zero on a bad matrix instead of reporting a misleading pass
- [x] Attempt A: remove camera-back retreat and try rotation-only upper-chain counter-bend
  - build + strict run artifact: `Saved/Validation/ClipMatrix/20260412_203520_D_LocalCorrected_clip_matrix_summary.json`
  - result: still `13/15` failed
  - what improved:
    - some direct bone intrusion metrics dropped
    - several ray/proxy counts reduced
  - why it failed:
    - stop / jump / crouch still had large source-neck drift windows
    - rotation-only guard did not keep the visible chain out of the camera or preserve the local neck-to-spine relationship
- [x] Attempt B: add corrected local neck-chain stretch metric and neck-gap-vector upper-chain follow
  - build + strict run artifact: `Saved/Validation/ClipMatrix/20260412_204336_D_LocalCorrected_clip_matrix_summary.json`
  - result: still `12/15` failed
  - what improved:
    - `StrafeR_MaxDown` became clean
    - some phases had lower ray/proxy counts than Attempt A
  - why it failed:
    - the follow math was still wrong for the real visible geometry
    - worst samples show the corrected visible `spine_05` still ends up ahead of and even above the fixed neck target in camera local space
    - this matches the live complaint: upper body folds the wrong way and drives hands into the torso/legs instead of constraining the chain away from the camera
  - worst-sample evidence from `20260412_204336_D_LocalCorrected_clip_matrix_timeline.jsonl`:
    - `SprintStop_MaxDown`: `spine_05_camLocal=V(X=24.78, Y=-3.41, Z=-12.27)` while `neck_01_camLocal=V(X=10.58, Y=-1.00, Z=-24.19)`
    - `JumpFall_MaxDown`: `spine_05_camLocal=V(X=41.92, Y=-4.52, Z=-20.81)` while `neck_01_camLocal=V(X=10.58, Y=-1.00, Z=-24.19)`
    - `Uncrouch_MaxDown`: `spine_05_camLocal=V(X=34.48, Y=-1.05, Z=-20.54)` while `neck_01_camLocal=V(X=10.58, Y=-1.00, Z=-24.19)`
  - conclusion:
    - generic neck-gap follow is not enough
    - next fix must be a direct camera-local upper-chain constraint using the visible chain geometry, not a proxy "follow the neck gap" translation

Current reopened direction (2026-04-12)
- [x] Add direct camera-local logging for source/corrected upper-chain geometry in the failing states
- [x] Replace generic neck-gap follow with a chain constraint driven by `spine_05` vs fixed neck target in camera local space
- [ ] Keep existing neck attach unchanged
- [ ] Re-run strict matrix and only accept the result if the log and screenshot evidence match the pass

More failed iterations after reopening (2026-04-12, night pass)
- [x] Attempt C: camera-local retreat + guard-pitch on the visible upper chain
  - build + strict run artifact: `Saved/Validation/ClipMatrix/20260412_205606_D_LocalCorrected_clip_matrix_summary.json`
  - result: `13/15` failed
  - why it failed:
    - the retreat solved some direct ray overlap but created larger visible chain distortion
    - the body was still folding the wrong way relative to the fixed neck target
- [x] Attempt D: remove retreat translation and keep only the manual upper-chain pitch clamp
  - build + strict run artifact: `Saved/Validation/ClipMatrix/20260412_210016_D_LocalCorrected_clip_matrix_summary.json`
  - result: `13/15` failed
  - why it failed:
    - pure manual pitch was not enough to control the stop / fall / crouch edge cases
    - the visible upper chain still entered the camera ray repeatedly
- [x] Attempt E: switch to FABRIK upper-chain solve to preserve chain length
  - build + strict run artifact: `Saved/Validation/ClipMatrix/20260412_210638_D_LocalCorrected_clip_matrix_summary.json`
  - result: `10/15` failed
  - what improved:
    - corrected chain stretch dropped to `0`
    - several phases that were previously stretching became structurally cleaner
  - why it still failed:
    - the unconstrained solve reached the neck target by folding the visible upper chain in the wrong direction
    - the torso still entered the view during stop / landing / crouch-run
- [x] Attempt F: move FABRIK root lower to `pelvis`
  - build + strict run artifact: `Saved/Validation/ClipMatrix/20260412_210927_D_LocalCorrected_clip_matrix_summary.json`
  - result: still `10/15` failed
  - why it failed:
    - chain length stayed correct, but the pose was still anatomically wrong
    - the corrected `spine_05` could still end up ahead of and above the corrected neck target in camera local space
    - this matches the live screenshots where the torso folds up into the view and the hands drive into the legs

Current direction after the failed FABRIK pass (2026-04-12)
- [x] Keep the existing neck attach target exact
- [x] Stop trusting chain-length-only metrics
- [x] Add a direct corrected upper-chain fold metric:
  - fail when corrected `spine_05` sits too far ahead of corrected `neck_01`
  - fail when corrected `spine_05` rises above corrected `neck_01`
- [ ] Replace the unconstrained upper solve with a constrained visible-chain pass
- [ ] Re-run strict matrix and verify that logs, screenshots, and summary all agree

### Repro Harness Correction (2026-04-13)

- [x] Mark the previous "0 failed phases" state as a false pass
  - why it was wrong:
    - the matrix was measuring `headStretch` and `neckGap` but not failing on them
    - screenshot selection was also wrong because it ranked only ray / proxy / corrected-chain metrics
    - result: the test reported green while the real stop / land / crouch issue was still visible
- [x] Convert the matrix to a two-pass KISS flow in `FirstPersonClipMatrixTest.cpp`
  - pass 1 = logs + JSON only
  - pass 2 = replay exact edge timestamps and request first-person screenshots after the bad frame is known
  - this avoids screenshot-induced timing drift in the measurement pass
- [x] Add stronger debug probes for upper-body lean
  - keep the existing camera-ray vs bone spheres
  - keep the upper-torso proxy sphere
  - add an upper-torso capsule probe:
    - camera inside torso capsule?
    - camera forward ray intersects torso capsule?
  - add world / actor / camera diagnostics:
    - `camActorZ`
    - `sourceHeadActorZ`
    - `sourceNeckActorZ`
    - source vs corrected upper-chain vectors
- [x] Add targeted artifact replay with debug overlay
  - screenshot files now land in `Saved/Validation/ClipMatrix/`
  - sidecar JSON is written next to each PNG
  - latest targeted artifacts:
    - `Saved/Validation/ClipMatrix/20260413_074615_D_LocalCorrected_SprintStop_MaxDown_EDGE_t0.47_p-89.0_ray-1.0_proxy-1.0_capsule-1.0.png`
    - `Saved/Validation/ClipMatrix/20260413_074615_D_LocalCorrected_RunJumpLand_MaxDown_EDGE_t0.47_p-89.0_ray-1.0_proxy-1.0_capsule-1.0.png`
    - `Saved/Validation/ClipMatrix/20260413_074615_D_LocalCorrected_CrouchRun_MaxDown_EDGE_t0.91_p-89.0_ray-1.0_proxy-1.0_capsule-1.0.png`
- [x] Fix edge ranking so replay chooses the real windows instead of `t ~= 0.01`
  - use `sourceNeckTargetGapDist` and `sourceHeadCameraDist` as edge-ranking signals
  - latest replay windows now match the visual issue:
    - `SprintStop_MaxDown -> t ~= 0.47`
    - `RunJumpLand_MaxDown -> t ~= 0.47`
    - `CrouchRun_MaxDown -> t ~= 0.91`
- [x] Make the matrix fail on sustained stretch windows
  - add `maxConsecutiveHeadStretch >= threshold`
  - add `maxConsecutiveNeckGap >= threshold`
  - this is the missing gate that was allowing false green runs
- [x] Latest honest validation
  - build:
    - `.\scripts\ue\build\build.bat AlisEditor Win64 Development`
  - test:
    - `.\scripts\ue\test\character\capture_parity.ps1 -TestFilter "ProjectIntegrationTests.Character.FirstPerson.ClipMatrix.Default" -TimeoutSeconds 900`
  - artifact:
    - `Saved/Validation/ClipMatrix/20260413_074615_D_LocalCorrected_clip_matrix_summary.json`
  - result:
    - `13/15` failed
  - failed phases now correctly include the user-reported dynamic cases:
    - `SprintStop_MaxDown`
    - `Jump_MaxDown`
    - `JumpFall_MaxDown`
    - `JumpLand_MaxDown`
    - `RunJumpLand_MaxDown`
    - `CrouchRun_MaxDown`
  - key point:
    - the harness is now trustworthy enough to judge the runtime fix
    - the runtime chain fix is still NOT solved

### Repro Harness Tightening (2026-04-13, second pass)

- [x] Replace screenshot polling with the engine processed callback
  - use `FScreenshotRequest::OnScreenshotRequestProcessed()` instead of assuming
    that `!IsScreenshotRequested()` means the PNG is already done
  - sidecar JSON is now written after the screenshot is processed
  - sidecar now records:
    - `requestedFrame`
    - `processedFrame`
    - `requestedPhaseTime`
    - `requestedTimeErrorSec`
    - `screenshotFileExists`
- [x] Remove early replay slack so edge screenshots request on-or-after the measured bad frame
  - latest replay timing from `Saved/Logs/Alis.log`:
    - `SprintStop_MaxDown`: `dt=0.0010`
    - `RunJumpLand_MaxDown`: `dt=0.0079`
    - `CrouchRun_MaxDown`: `dt=0.0001`
  - latest sidecars proving this:
    - `Saved/Validation/ClipMatrix/20260413_080420_D_LocalCorrected_SprintStop_MaxDown_EDGE_t0.48_p-89.0_ray-1.0_proxy-1.0_capsule-1.0.json`
    - `Saved/Validation/ClipMatrix/20260413_080420_D_LocalCorrected_RunJumpLand_MaxDown_EDGE_t0.47_p-89.0_ray-1.0_proxy-1.0_capsule-1.0.json`
    - `Saved/Validation/ClipMatrix/20260413_080420_D_LocalCorrected_CrouchRun_MaxDown_EDGE_t0.92_p-89.0_ray-1.0_proxy-1.0_capsule-1.0.json`
- [x] Add explicit proof metrics for the debated causes
  - camera / view contract:
    - `expectedCameraActorZ`
    - `cameraActorZError`
    - `cameraHeadZDelta`
    - `cameraNeckZDelta`
  - lean direction:
    - `sourceUpperLeanDeg`
    - `correctedUpperLeanDeg`
    - `sourceUpperForwardCm`
    - `correctedUpperForwardCm`
    - `upperForwardDeltaCm`
  - fixed neck target visibility:
    - `desiredNeckWorld`
    - `desiredNeckCamLocal`
    - screenshot overlays now draw the desired neck target plus source/corrected neck->target lines
- [x] What the tightened run proves
  - latest summary:
    - `Saved/Validation/ClipMatrix/20260413_080420_D_LocalCorrected_clip_matrix_summary.json`
  - result:
    - still `13/15` failed
  - strong conclusions:
    - camera height is NOT the active failure:
      - `cameraActorZExpected = 55`
      - failing phases show `camActorZ=55.0..55.0` and `err=0.00`
    - screenshot timing is now trustworthy enough for manual review
    - the remaining problem is still the body / source-pose relationship, not replay lag
- [x] What the tightened run says about the current chain fix
  - `SprintStop_MaxDown`:
    - still `headStretch=13`
    - still `neckGap=108`
    - camera height stable
    - screenshot landed within `1.0 ms` of the measured edge
  - `RunJumpLand_MaxDown`:
    - still `headStretch=24`
    - still `neckGap=111`
    - camera height stable
    - screenshot landed within `7.9 ms` of the measured edge
  - `CrouchRun_MaxDown`:
    - no head stretch at the edge frame, but `neckGap=160`
    - camera height stable
    - screenshot landed within `0.05 ms` of the measured edge
  - interpretation:
    - the harness now disproves the earlier excuses:
      - not a screenshot-lag artifact
      - not a camera-Z drift artifact
    - current runtime correction still does not solve the real misalignment
    - next runtime fix must be judged against this harness, not against older false-green runs

### Focused Sprint-Stop Repro (2026-04-13, third pass)

- [x] Add a short focused automation scenario for the exact stop case the user reports
  - new test:
    - `ProjectIntegrationTests.Character.FirstPerson.ClipMatrix.SprintStopOnly`
  - path:
    - `2.0s` sprint forward, `2.0s` stop, repeat once, then `1.0s` settle
  - pitch:
    - fixed at `-89.0`
  - intent:
    - isolate the simple "sprint then stop while looking down" failure without jump or crouch noise
- [x] Focus artifact capture on the stop windows in that short scenario
  - latest summary:
    - `Saved/Validation/ClipMatrix/20260413_082548_D_LocalCorrected_SprintStopOnly_clip_matrix_summary.json`
  - latest edge screenshots:
    - `Saved/Validation/ClipMatrix/20260413_082548_D_LocalCorrected_SprintStopOnly_SprintStopA_MaxDown_EDGE_t0.48_p-89.0_ray-1.0_proxy-1.0_capsule-1.0.png`
    - `Saved/Validation/ClipMatrix/20260413_082548_D_LocalCorrected_SprintStopOnly_SprintStopB_MaxDown_EDGE_t0.48_p-89.0_ray-1.0_proxy-1.0_capsule-1.0.png`
  - latest edge sidecars:
    - `Saved/Validation/ClipMatrix/20260413_082548_D_LocalCorrected_SprintStopOnly_SprintStopA_MaxDown_EDGE_t0.48_p-89.0_ray-1.0_proxy-1.0_capsule-1.0.json`
    - `Saved/Validation/ClipMatrix/20260413_082548_D_LocalCorrected_SprintStopOnly_SprintStopB_MaxDown_EDGE_t0.48_p-89.0_ray-1.0_proxy-1.0_capsule-1.0.json`
- [x] Result from the focused run
  - `4/5` phases failed:
    - `SprintForwardA_MaxDown`
    - `SprintStopA_MaxDown`
    - `SprintForwardB_MaxDown`
    - `SprintStopB_MaxDown`
  - `SprintStopFinalSettle` passed
  - most important proof:
    - both stop phases fail while `speed=0.0`
    - camera height is still exact:
      - `camActorZ=55.0..55.0`
      - `err=0.00 cm`
    - stop-phase neck stretch is still severe:
      - `SprintStopA_MaxDown`: `headStretch=15`, `neckGap=119`, `neckGapDist=33.7 cm`
      - `SprintStopB_MaxDown`: `headStretch=15`, `neckGap=120`, `neckGapDist=33.7 cm`
- [x] Screenshot timing proof for the focused run
  - `SprintStopA_MaxDown`:
    - target `t=0.483`
    - queued `t=0.488`
    - `dt=0.0054`
    - processed with `file=1`
  - `SprintStopB_MaxDown`:
    - target `t=0.482`
    - queued `t=0.489`
    - `dt=0.0069`
    - processed with `file=1`
- [x] Conclusion from the focused run
  - the narrowed test now catches the editor-visible sprint-stop bug
  - this was the missing proof step
  - the current runtime chain correction is still NOT a valid fix

### Runtime Filter Attempts (2026-04-13, fourth pass)

- [x] Safe cleanup kept
  - default PIE/runtime debug clutter is now disabled behind cvars:
    - `alis.LocalBody.DebugDraw=0`
    - `alis.LocalBody.DebugLog=0`
  - this is safe to keep while the runtime fix remains unresolved
- [x] Failed attempt A: pure filter / no upper follow
  - intent:
    - remove all default upper-spine follow and only apply a pitch-only filter when the fixed neck gap grows
  - result:
    - severe regression
    - latest bad artifact from that branch:
      - `Saved/Validation/ClipMatrix/20260413_084227_D_LocalCorrected_SprintStopOnly_clip_matrix_summary.json`
  - proof:
    - `SprintStopA_MaxDown` exploded to owner-visible intrusion:
      - `boneIntrusion=45`
      - `rayHits=213`
      - `proxyHits=139`
      - `capsuleInside=52`
      - `capsuleRay=122`
    - screenshot clearly showed torso folding into the camera
  - decision:
    - rejected
- [x] Failed attempt B: backward-only retreat driven directly from neck gap
  - intent:
    - keep no default bend, but retreat only on camera-backward X and remove the old lateral/down solve
  - result:
    - still a strong regression
    - latest bad artifact from that branch:
      - `Saved/Validation/ClipMatrix/20260413_085049_D_LocalCorrected_SprintStopOnly_clip_matrix_summary.json`
  - proof:
    - `SprintStopA_MaxDown`:
      - `boneIntrusion=102`
      - `rayHits=210`
      - `proxyHits=137`
      - `capsuleInside=108`
      - `capsuleRay=121`
    - even `SprintStopFinalSettle` became failed
  - decision:
    - rejected
- [x] Restored runtime baseline after both regressions
  - current runtime state is back to the last non-regressed solve
  - verification artifact after restore:
    - `Saved/Validation/ClipMatrix/20260413_085513_D_LocalCorrected_SprintStopOnly_clip_matrix_summary.json`
  - restored status:
    - `4/5` failed again, matching the earlier focused baseline instead of the worse regressions
    - stop phases are back to:
      - no owner-visible ray/proxy/capsule hits
      - remaining failure is the original neck-stretch / neck-gap problem
  - decision:
    - keep this restored baseline while designing the next fix

Phase 9c: Camera offset fixed (2026-04-10)
- [x] Hero.json relativeOffset Z changed: 62 -> 55 (matches head bone at ~54cm + 1cm eyes offset)
- [x] DefinitionCharacter.cpp constructor default: 62 -> 55
- [x] LocalBodyAnimInstance.cpp: replaced ALL hardcoded camera offset with actual UCameraComponent::GetComponentLocation()
- [x] Verified in test: Cam-Actor=55.0, Head-Actor=55.1 in idle (perfect match!)
- [x] UE capsule handles crouch/jump automatically (no per-state interpolation needed)

Test results after camera fix:
- 15/15 fail -- EXPECTED because neck_01 is now intentionally AT camera (0-2cm distance)
- Old test assumes "bones near camera = bad" but now "neck near camera = correct"
- [x] Update ClipMatrix test focus from anonymous ray hit to explicit `spine_05` attribution
- [x] Replace run-stop repro with sprint-stop repro
- [x] Disable screenshot capture during matrix repro (logs/json only)
- [ ] PIE: verify idle, crouch, jump eye height visually
- [ ] PIE: verify no nape/back visible when looking down
- [ ] Remove debug arrows and per-tick logging when validated

#### Engine research (2026-04-10, for reference)

| Solution | C++ anim node | Multi-goal | Asset needed | Status |
|---|---|---|---|---|
| **FAnimNode_TwoBoneIK** | YES | NO (per-arm) | NO | Production, chosen |
| FAnimNode_IKRig + FBIK | YES | YES | YES (UIKRigDefinition) | Production |
| FAnimNode_ControlRig | Partial | YES (via RigVM) | YES (ControlRig asset) | Production |
| FPBIKSolver (direct) | Custom needed | YES | NO | Experimental |

Key engine files (for escalation):
- `AnimGraphRuntime/Public/BoneControllers/AnimNode_TwoBoneIK.h` -- chosen solution
- `IKRig/Public/AnimNodes/AnimNode_IKRig.h` -- escalation option B
- `IKRig/Public/Rig/Solvers/IKRigFullBodyIK.h` -- FBIK settings reference
- `Plugins/Experimental/FullBodyIK/Source/PBIK/Public/Core/PBIKSolver.h` -- escalation option A

### Future Spike: UE 5.7 Native FP Rendering

Only investigate AFTER Phase 7 is validated.

- [ ] Research FirstPersonPrimitiveType + FirstPersonScale (GPU-only, may complement bone override)
- [ ] Test custom FP FOV, anti-clipping scale, world-space representation
- [ ] Evaluate if it replaces manual bOwnerNoSee/bOnlyOwnerSee setup
- [ ] Check constraints: deferred rendering, static lighting, groom support

---

## Decision: First Person vs Third Person

Alis uses **first-person view** with a rigid camera. This means:

- **Primary architecture:** headless local body split via Hero.json mesh definitions
- **Current anti-clip:** spine yaw tracking (works) + alpha pitch (interim, has arm clipping side effect)
- **Needed next:** Control Rig FBIK solver with pinned hands + camera-relative chest target (Phase 8)
- **Optional engine spike:** UE 5.7 FirstPersonPrimitiveType -- GPU-level complement, not replacement

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
