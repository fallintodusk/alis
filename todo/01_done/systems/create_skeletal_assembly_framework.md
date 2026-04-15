# Skeletal Assembly Framework -- Active Work

Remaining work items for the modular skeletal assembly framework.

Architecture decisions and completed phases have been extracted to permanent SOT docs:
- [Assembly architecture](../../Plugins/Systems/ProjectSkeletalAssembly/docs/architecture.md)
- [Capabilities rationale](../../Plugins/Gameplay/ProjectSkeletalCapabilities/docs/architecture.md)
- [Character design (legacy vs modular)](../../Plugins/Gameplay/ProjectCharacter/docs/design.md)
- [Layer contract (Kind/Role/Visibility)](../../Plugins/Resources/ProjectObject/docs/layer_contract.md)
- [Parity testing](../../docs/testing/character_parity.md)

Full CDO reference: `Saved/Inspection/BP_Hero_CDO_2026-04-03.json`

---

## Phase Status

| Phase | Status |
|-------|--------|
| 0 - Freeze legacy | DONE |
| 1 - Scan kernel + registry + assembly plugin | DONE |
| 1.5 - JSON body decision | DONE |
| 2 - Definition extension + generator | DONE |
| 3 - Modular hero (ADefinitionCharacter) | DONE |
| 3.5 - Decouple ProjectObject from ProjectMotionSystem | DONE |
| 4 - Wire switching | DONE |
| 5 - Debug capture | DONE |
| 6a - Mutable adapter | DONE |
| 6b - Camera/body orchestration | DONE |
| 6c - Local first-person body | DONE |
| 6d - MotionMatching locomotion | DONE (pass-through PostProcess ABP) |
| 6d.1 - Camera-driven body rotation | DONE (propagation + spine yaw tracking) |
| 6d.2 - Project-owned retarget wrapper | DONE |
| 6d.3 - Remove per-frame rotation tick | DONE |
| 6d.4 - Fast camera vs body desync (spine rotation layer) | DONE (spine_01/02/03 additive yaw from control-actor delta) |
| 6d.5 - LocalBody floating above WorldBody (root offset bug) | DONE (removed RootFreezeNode camera-drift math entirely) |
| 6d.6 - Camera height validation | DONE (matches legacy: Z=62, capsule halfHeight=86) |
| 6d.7 - Neck clipping after sprint stop | DONE (neck_01 additive pitch anti-clip when looking down) |
| 6d.8 - Mutable COI preset clone | DONE (MutableInstance property, deferred param apply) |
| 6d.9 - No silent legacy fallback | DONE (modular spawn failure = error, no fallback) |
| 6e - Movement/traversal | Future |
| 7 - SOT extraction | DONE |
| 7b - Repo-wide doc sweep | TODO (see checklist below) |
| 8 - Bridge asset to data-driven reference | DONE |
| 9 - Dead code cleanup | DONE |
| 10 - Jump fix on modular | BASELINE (air entry works, foot range differs from legacy) |

---

## Remaining Architectural Debt

### Parent-mesh vs CSK-output routing

In modular path, Mutable output lives on CSK child components (BodyCustomization, HeadCustomization, LocalBodyCustomization) while parent meshes (WorldBody, LocalBody, Head) stay empty. In legacy, Mutable replaces the parent mesh directly. Both paths work visually but code reading parent-role meshes may get null.

Impact: low for v1 (visual parity achieved). May need explicit parent-mesh sync for systems that query mesh by role tag and expect a non-null asset.

### Visibility-driven defaults scope

ObjectSpawnUtility applies `AlwaysTickPoseAndRefreshBones`, `FirstPersonPrimitiveType`, `NoCollision` only when explicit `Visibility` is set. This correctly scopes to hero-like characters. Future role-driven skeletal layers without visibility policy won't get these defaults. If needed, add explicit mesh properties in the definition schema.

---

## Completed: MotionMatching Locomotion (6d)

**Root cause:** PostProcess AnimInstance with no AnimGraph evaluates to ref pose, overwrites primary ABP's valid locomotion pose.

**Fix (2 parts):**
1. C++ `MotionMatchingBridgeAnimInstance::NativeUpdateAnimation` injects CharacterProperties via reflection at correct timing (after BPI zeros, before ThreadSafe Update_Logic)
2. BP `ABP_MotionMatchingBridge` (pass-through AnimGraph: LinkedAnimGraphInput -> OutputPose) preserves primary ABP's pose

**Key facts discovered during investigation:**
- GASP ABP MovementMode enum: 0=OnGround, 1=InAir (NOT UE's EMovementMode 4/5)
- `NoValidAnim=T` is normal (legacy also has it TRUE while animating)
- Tick-based writes (before or after ABP) never survive to animation evaluation
- `bDisablePostProcessBlueprint=true` prevents both update AND evaluation
- Direct output variable writes from tick look correct in readback but don't affect state machine

**Why PostProcess timing is required (DO NOT use component tick for data injection):**
1. ABP `BlueprintUpdateAnimation`: calls BPI (fails on DefinitionCharacter, zeros CharacterProperties), runs Update_Logic with zeros -> idle state
2. PostProcess `NativeUpdateAnimation`: writes correct CharacterProperties (AFTER BPI zeros, BEFORE ThreadSafe)
3. ABP `BlueprintThreadSafeUpdateAnimation`: Update_Logic reads our injected data -> correct locomotion state
4. Evaluation: uses correct state -> locomotion pose

Component tick writes (before or after ABP) never survive to evaluation because:
- Before ABP: BPI call overwrites our values
- After ABP: evaluation already happened, writes only visible in next-frame readback

**Parity verified:** All 15 locomotion phases match legacy within ~1-2 units (CleanMap, 132 samples/character, zero warnings).

**Asset location:** `Plugins/Gameplay/ProjectSkeletalCapabilities/Content/MotionMatching/ABP_MotionMatchingBridge`

---

## Baseline Restored: Camera-Driven Body Rotation (6d.1)

**Status:** Propagation chain works. Baseline validated with self-contained camera-yaw test.
Fast horizontal camera spin while idle/moving still exposes body back (see 6d.4).
Turn-in-place steering (`ShouldTurnInPlace`, `TargetRotationDelta`) not yet proven.

Modular first-person body now follows camera yaw through the intended layered path:

`DriverBody -> WorldBody (ABP_WorldBodyRetarget) -> LocalBody (CopyPose)`

Historical note:
- `Saved/Validation/CharacterDebug/*_camera_yaw_*_20260407_164410.*` proved the visual propagation fix
- it did NOT prove correct idle turn-state entry
- that run used the older gate that could false-pass on idle sway and a stale phase-boundary sample
- the corrected proof run is `Saved/Validation/CharacterDebug/*_camera_yaw_*_20260408_160232.*`

Important: legacy is not fully correct either. The current camera-yaw work uses
legacy as a relative baseline to expose structural differences, but final signoff
must be against intended behavior, not "matches BP_Hero".

**Current proof setup (2026-04-08):**
- `DefinitionCharacter` keeps `bUseControllerRotationYaw = false`
- `DefinitionCharacter` drives rotation like the sample/legacy pre-CMC path:
  - `bUseControllerDesiredRotation = true`
  - `bOrientRotationToMovement = false`
  - instant ground rotation rate, finite falling rotation rate
- bridge baseline writes:
  - `RotationMode = 1` (Strafe)
  - `OrientationIntent = ActorRotation`
  - `AimingRotation = full control/base aim`
  - `InputState.WantsToStrafe = true`
  - `InputState.WantsToAim = true`
  - `InputState.WantsToCrouch = CharacterMovement->IsCrouching()`
- Mutable rebuild promotes generated body mesh onto `WorldBody`
- `WorldBody` keeps its data-owned `ABP_WorldBodyRetarget_C` anim class
- `ULocalBodyAnimInstance` copies from `WorldBody` and falls back to `DriverBody` only during early init
- dedicated test exists:
  `ProjectIntegrationTests.Character.Parity.CameraYawTimeline`
- `scripts/ue/test/character/capture_parity.ps1` now runs
  `GenerateDefinitions -type=Object` before launch, so `Hero.json` changes
  regenerate `Hero.uasset` automatically for the parity path
- latest proof run:
  `Saved/Validation/CharacterDebug/*_camera_yaw_*_20260408_160232.*`

**What is now proven:**

1. `bUseControllerRotationYaw = false` is active and camera yaw now catches the actor/body.
   - The old frozen-actor state is gone.
   - In `modular_camera_yaw_summary_20260408_160232.json`:
     - `IdleYaw60`: `MaxAbsActorYawFromStart = 22.6`
     - `IdleYaw100`: `30.3`
     - `IdleBackTo0`: `75.3`
     - `CrouchIdleYaw90`: `74.9`

2. The earlier `RotationMode = 1 means Aim` reading was wrong.
   - `SandboxCharacter_Mover.Get_RotationMode` proves:
     - `NewEnumerator0 = OrientToMovement`
     - `NewEnumerator1 = Strafe`
     - `NewEnumerator2 = Aim`
   - The sample graph comment explicitly describes those three modes.

3. The active bridge baseline is `Strafe + actor intent + full aim`, not `Aim + yaw-only intent`.
   - `RotationMode = 1` enables the sample AO path.
   - `OrientationIntent = ActorRotation` matches the sample/legacy target-rotation feed.
   - `AimingRotation` stays full control/base aim.
   - `InputState` no longer zeros out turn-relevant flags.
   - `modular_camera_yaw_summary_20260408_160232.json` shows `BridgeTracksControlYaw = true`
     for every yaw-sweep phase.

4. The modular world visual layer is now real, active, and owner-local parity is restored.
   - `modular_camera_yaw_summary_20260408_160232.json` shows:
     - `HasWorld = true`
     - `VisibleTracksWorld = true`
     - `SawRetargetWorldVisual = true`
   - Runtime log proves the expected promotion:
     - `Promoted WorldBody visual source ... AnimClass=ABP_WorldBodyRetarget_C ...`
   - Owner-visible body now copies the same world visual source instead of the raw driver pose.

5. Visible torso and head now respond to camera yaw in modular.
   - `modular_camera_yaw_summary_20260408_160232.json`:
     - `IdleYaw60`: `VisiblePelvis=13.3`, `VisibleSpine=101.0`, `VisibleHead=51.1`
     - `IdleYaw100`: `39.2`, `33.2`, `34.4`
     - `IdleBackTo0`: `64.3`, `20.2`, `49.5`
     - `CrouchIdleYaw90`: `68.6`, `48.5`, `20.1`
   - That is the actual fix for "body does not move with camera".

6. The modular test is no longer gated by legacy behavior.
   - Legacy is still captured for comparison artifacts.
   - Modular pass/fail is now self-contained and does not require legacy phase parity.
   - This avoids treating `BP_Hero` as a correctness oracle.

7. Large idle yaw now proves a real rotating-body state, not just idle sway.
   - `IdleYaw60` enters `M_Neutral_Stand_Turn_045_R`.
   - `IdleYaw100` stays on `M_Neutral_Stand_Turn_045_R` for the whole phase while:
     - actor yaw changes by `30.3`
     - visible pelvis changes by `39.2`
     - visible spine changes by `33.2`
     - visible head changes by `34.4`
   - `IdleBackTo0` and `CrouchIdleYaw90` also keep `TurnStateObserved = true`.

8. Legacy is also broken in absolute camera/body terms.
   - The same camera-yaw test shows legacy never becomes a clean gold standard:
     - `ShouldTurnInPlace` stays `false` through the idle yaw phases
     - visible response exists, but it is still not a complete turn-in-place solution
   - So "modular vs legacy" and "correct camera/body behavior" are separate questions.

9. Legacy visible body is NOT the same pose source as the driver.
   - `Saved/Inspection/BP_Hero_CDO_2026-04-03.json` proves `WorldBodyMesh` uses
     a dedicated retarget wrapper AnimBP.
   - Runtime parity samples prove the visible legacy meshes diverge from the driver.
   - The world-body retarget wrapper AnimGraph is just `Retarget Pose From Mesh`, so legacy
     is showing a dedicated visual retarget layer, not the raw motion-matching driver.

10. The camera-yaw test itself needed correction.
   - Old camera-yaw summaries could count a stale first sample from the previous phase.
   - Old large-turn success criteria were too permissive and could treat idle upper-body sway as proof of turning.
   - Old "root turn" reporting was using mesh component yaw instead of root bone facing.
   - The revised gate now accepts two valid large-idle-yaw proofs:
     - explicit turn-state evidence (`ShouldTurnInPlace`, transition history, rotation break, target delta)
     - or an already-active turn clip plus actor/body catch-up
   - Large turn phases also require a root/body catch-up sign:
     - actor yaw movement
     - root bone facing movement
     - or non-zero `TargetRotationDelta`

**Implemented fix:**

- `Hero.json`
  - `WorldBody` now carries the retarget AnimBP asset path
- `MutableCustomizationCapability.cpp`
  - generated body mesh is copied onto `WorldBody`
  - `WorldBody` keeps the anim class that data or legacy Blueprint defaults assigned
  - world-body anim re-init now happens only when mesh/anim state actually changed
- `LocalBodyAnimInstance.cpp`
  - copy-pose source now prefers `WorldBody`
  - `DriverBody` is only an early-init fallback until `WorldBody` is live
- `MotionMatchingCapability.cpp` / `LocalFirstPersonCapability.cpp`
  - head leader-pose now prefers `WorldBody`
- `CharacterParityCameraYawTest.cpp`
  - gate now checks the correct architecture:
    - bridge tracks control yaw
    - retargeted `WorldBody` exists
    - owner-visible mesh tracks `WorldBody`
    - torso/head respond during yaw phases

---

## Remaining Work

### Completed: project-owned retarget wrapper (6d.2)

We no longer rely on third-party `ABP_GenericRetarget` as the runtime source of
truth for modular `WorldBody`.

Implemented:
- duplicated the vendor wrapper into project-owned
  `/ProjectSkeletalCapabilities/MotionMatching/ABP_WorldBodyRetarget`
- narrowed its `IKRetargeter_Map` to the runtime keys we actually use:
  `DefMeshId=WorldBody`, `AssemblyRole=WorldBody`, `RTG_AutoGenerated`
- switched `Hero.json` to the project-owned wrapper
- removed `MutableCustomizationCapability` hardcoded retarget-ABP override
- removed runtime `RTG_AutoGenerated` tag promotion from Mutable rebuild flow

Result:
- world-body retarget asset path is data-owned again
- Mutable rebuild no longer overrides that asset path every update
- stale `RTG_Mannequin -> IK_Mannequin` dependency is no longer required by the modular path
- owner-visible local body still follows the same retargeted world visual layer

### Completed: Remove per-frame rotation tick (6d.3)

Removed `Tick` override and `PrimaryActorTick.bCanEverTick = true` from
`ADefinitionCharacter`. The only dynamic rotation case (falling vs grounded
`RotationRate`) now uses `OnMovementModeChanged` override.

- `PrimaryActorTick.bCanEverTick = false`
- `UpdateRotationPolicy()` called from `BeginPlay`, `PossessedBy`, and `OnMovementModeChanged`
- Magic `-1.0f` replaced with named `InstantRotationRate` and `FallingRotationRate` constants
- Test namespace collision fixed: anonymous namespaces renamed to `CameraYawHelpers`, `CleanPathHelpers`, `LocomotionHelpers`

Note: trigger coverage is better but still partial. If crouch/uncrouch ever
needs to affect rotation policy, a new hook will be needed. Not a blocker now.

### Completed: LocalBody floating above WorldBody (6d.5)

**Root cause was:** `RootFreezeNode` computed camera-to-neck drift in mesh space,
pushing root bone UP ~150 units toward camera. Fundamentally wrong for grounded FP body.

**Fix applied (2026-04-08):**
- Removed `RootFreezeNode` from the anim chain entirely
- Root bone now copies directly from source mesh via `CopyPoseFromMesh`
- No camera-drift compensation -- feet stay grounded at mesh origin
- New chain: `CopyPose -> CS -> Spine01 -> Spine02 -> Spine03 -> NeckLock -> Local -> Output`

**Files:** `LocalBodyAnimInstance.cpp`, `LocalBodyAnimInstance.h`

### Completed: Fast camera rotation exposes body back (6d.4)

**Root cause was:** MM pose lags behind control rotation by at least one frame.
No direct camera-driven upper body rotation existed.

**Fix applied (2026-04-08):**
- Added 3 `FAnimNode_ModifyBone` for `spine_01/02/03` in `LocalBodyAnimInstance`
- Each applies additive yaw rotation in component space
- `YawDeltaDeg = FMath::FindDeltaAngleDegrees(ActorYaw, ControlYaw)`, clamped to [-90, 90]
- Distribution: spine_01 = 40%, spine_02 = 30%, spine_03 = 30%
- Gives instant upper body camera tracking regardless of MM pose lag

**Files:** `LocalBodyAnimInstance.cpp`, `LocalBodyAnimInstance.h`

Layer 2 (bridge orientation improvement) deferred -- not needed if spine tracking
covers the visual gap. Revisit only if turn-in-place issues surface.

### Completed: Neck clipping after sprint stop (6d.7)

**Root cause was:** Camera-drift root offset (6d.5) pushed mesh toward camera,
and hidden neck/head bone transforms clipped through camera near plane.

**Fix applied (2026-04-08):**
- Removed root camera drift (6d.5 fix) eliminates the primary cause
- Added `NeckLockNode` (`FAnimNode_ModifyBone` on `neck_01`) as safety net
- When control pitch < -15 deg (looking down), applies additive backward pitch
- Maps [-15, -45] pitch -> [0, -8] deg neck tilt to keep neck away from camera
- Alpha = 1.0 when `bEnableSpineLock` is true, 0.0 otherwise

**Files:** `LocalBodyAnimInstance.cpp`, `LocalBodyAnimInstance.h`

### Completed: Camera height validation (6d.6)

Verified against BP_Hero CDO dump:
- Legacy camera: `(X=23, Y=0, Z=62)` -- matches DefinitionCharacter exactly
- Capsule: radius=30, halfHeight=86 -- matches exactly
- Eye height from ground = 148cm for 172cm character (86% of height)
- Real human eyes at ~93%, but lower camera is common FPS design choice
- No change needed -- camera matches legacy BP_Hero

### Turn-in-place and target rotation steering (separate from 6d.1)

Camera/body propagation is fixed, but the full Game Animation Sample turning path is
not yet proven:

- `ShouldTurnInPlace` stays `false`
- `TargetRotationDelta` stays `0`
- `RotationMode` is still a first-person baseline, not final view-mode-driven policy

This is follow-up tuning / steering work, not the same defect as the broken modular visual chain.

### Clean-path testing baseline (use this before full-chain guesses)

Do not use `BP_Hero` as the first source of truth for this bug class.

Use the same spawned modular hero in this order:
1. vendor/sample MM contract on raw `DriverBody` only
2. `DriverBody -> WorldBody` retarget wrapper
3. `DriverBody -> WorldBody -> LocalBody` copy-pose chain
4. full Mutable chain

The first stage that breaks is the real fault line.

Implemented automation:
- `ProjectIntegrationTests.Character.Parity.CleanPathIsolationMatrix`
- runner:
  `scripts/ue/test/character/capture_parity.ps1 -TestFilter "ProjectIntegrationTests.Character.Parity.CleanPathIsolationMatrix" -TimeoutSeconds 900`

Matrix used in every mode:
- idle
- forward accel / steady / stop
- backward
- strafe left / right
- diagonal left / right
- crouch idle / forward
- idle yaw 30 / 60 / 100
- yaw back
- move + yaw
- jump / fall / land

Failure classification:
- Layer 1 - MM contract
- Layer 2 - raw driver pose
- Layer 3 - retarget propagation
- Layer 4 - local/customization propagation

Latest validated run:
- `Saved/Validation/CharacterDebug/modular_clean_path_summary_20260408_164843.json`

Current result:
- Mode A passed
- Mode B passed
- Mode C passed
- Mode D passed
- no earlier break surfaced before the full chain on the current build

Interpretation:
- stop using legacy `BP_Hero` as a correctness oracle
- stop assuming retarget/local/Mutable are the first break without isolating modes A-D
- future camera/body failures should first reproduce under this clean-path matrix, then investigate the first failing mode

### Visibility fix applied (2026-04-07)

Hero.json customization meshes now have explicit visibility:
- BodyCustomization: `SkipOwner` (matches WorldBody parent)
- HeadCustomization: `SkipOwner` (matches Head parent)
- LocalBodyCustomization: `OwnerOnly` (matches LocalBody parent)

Previously missing -- player could see both world and local bodies.

### Leg sliding on local body (known, pre-existing)

Local body shows slight leg sliding in idle. Also present on legacy BP_Hero. Not a modular regression. Separate investigation needed for CopyPose/bone restriction interaction.

### Baseline Restored: Jump on modular (phase 10)

Air entry works: `EnteredAir=True` on both modular and legacy (test run 20260408_173012).
Foot vertical range differs significantly (29.2 modular vs 79.8 legacy) -- likely
different JumpZVelocity or jump animation selection. The originally reported
`EnteredAir=False` was from an older build before motion matching and rotation fixes.

Remaining: foot range gap needs investigation -- may be JumpZVelocity tuning
or MM jump animation selection issue. Not blocking but not fully at parity.

### Completed: Bridge asset path to data (phase 8)

Moved hardcoded ABP path to data-driven:
- `Hero.json` MotionMatching capability now has `BridgeAnimBPPath` property
- `UMotionMatchingCapability` has `UPROPERTY FString BridgeAnimBPPath`
- `TryInstallPostProcessBridge` uses property with fallback to default path
- Spawn system passes JSON property via `SetPropertyByName`

Note: property contract is stringly-typed (raw path string, not TSoftClassPtr).
Acceptable for current object-definition system, but should migrate to a typed
soft class reference when the schema supports it.

### Completed: Dead code cleanup (phase 9)

- Removed `FindSubProperty` alias in `MotionMatchingBridgeAnimInstance.cpp`
- All usages replaced with `FindPropWithFallback` directly
- `CanContainContent=true` in ProjectSkeletalCapabilities.uplugin kept (required for content)

### Completed: Mutable COI preset clone (6d.8)

**Root cause:** `CO->CreateInstance()` returns an instance but `GetParameterCount()`
returns 0 because the CO is not yet compiled at init time. Mutable auto-compiles
asynchronously, but `SetEnumParameterSelectedOption` silently ignores params on
an uncompiled instance. Character spawned naked.

**Fix (2026-04-09):**
- Added `MutableInstance` property: soft ref to a pre-saved COI asset
- When set, `Clone()` the saved instance (carries baked param selections)
- When not set, `CreateInstance()` from `MutableSource` (fresh runtime path)
- `DefaultParameters` applied as overrides on top of either path
- If CO not compiled at init time, deferred param apply in `OnMutableInstanceUpdated`
  after first async update (CO is compiled by then), triggers second update
- Hero.json uses `COI_Hero` preset, no DefaultParameters needed

**Files:** `MutableCustomizationCapability.cpp/h`, `Hero.json`

### Completed: No silent legacy fallback (6d.9)

Modular spawn failure now logs `Error` and returns nullptr instead of silently
falling back to legacy `BP_Hero`. This was masking real bugs (e.g. JSON comment
field being parsed as a property, causing spawn failure -> silent legacy fallback
-> no spine tracking -> "camera doesn't move body").

**File:** `SinglePlayerGameMode.cpp`

### Failed: FP body clipping fix attempt (2026-04-09)

Architecture SOT: [architecture.md](../../Plugins/Gameplay/ProjectSkeletalCapabilities/docs/architecture.md) (dead ends, discoveries, test infra).
Active work: [fix_fp_body_clipping.md](fix_fp_body_clipping.md).

### Other session fixes (2026-04-09)

- Orchestrator plugin registry "already mounted" log downgraded from Warning to Log
  (expected behavior, not a problem) -- `OrchestratorPluginRegistry.cpp`
- LocalFirstPerson capability scope changed from `["LocalBody"]` to `["actor"]`
  in Hero.json (capability discovers meshes via role tags, doesn't need spawn
  system mesh target; false "interaction target interface" warning eliminated)
- Test namespace unity build collisions fixed: latent command classes wrapped
  inside their helper namespaces, `using namespace` directives removed --
  `CharacterParityCameraYawTest.cpp`, `CharacterCleanPathIsolationTest.cpp`,
  `CharacterParityLocomotionTest.cpp`

### Movement tuning parity (phase 6e, deferred)

Legacy BP_Hero `UpdateMovement_PreCMC` dynamically overrides MaxWalkSpeed from gait/strafe vectors. Modular uses static `RunSpeed=500`. Full parity needs a movement capability.

---

## Test Infrastructure

- `Character.Parity.IdleSnapshot` -- idle capture (legacy + modular JSON)
- `Character.Parity.CleanPathIsolationMatrix` -- modular-only A-D fault-line isolation on one hero
- `Character.Parity.LocomotionTimeline` -- 15-phase movement matrix (JSONL + summary)
- `Character.Parity.CameraYawTimeline` -- modular-only yaw/turn-state proof on the active chain
- `Character.Parity.SimpleAnimSanity` -- bypass ABP, prove mesh evaluation
- Runner: `scripts/ue/test/character/capture_parity.ps1 -TimeoutSeconds 300`
- Map: `Content/Developers/<user>/CleanMap` (flat plane with project GameMode)
- Output: `Saved/Validation/CharacterDebug/` (JSON, JSONL, PNG)

---

## Post-Phase Doc Scan Checklist

- [ ] `docs/architecture/plugin_architecture.md` -- add ProjectSkeletalAssembly to plugin map
- [ ] `docs/architecture/core_principles.md` -- verify capability model matches one-registry
- [ ] `docs/architecture/data_driven.md` -- update with Kind/Role/Visibility fields
- [ ] `docs/data/README.md` -- update generation pipeline with new section types
- [ ] `Plugins/Foundation/ProjectCore/README.md` -- add FRegisteredClassScan, IAssemblyCapability
- [ ] `Plugins/Gameplay/ProjectObjectCapabilities/README.md` -- update registry description
- [ ] `Plugins/Resources/ProjectObject/README.md` -- update ObjectDefinition fields
- [ ] `Plugins/Systems/ProjectSkeletalAssembly/README.md` -- create (new plugin)
- [ ] `Plugins/Gameplay/ProjectSkeletalCapabilities/README.md` -- create (new plugin)
- [ ] `Plugins/Gameplay/ProjectCharacter/README.md` -- update legacy vs modular
- [ ] `Plugins/Gameplay/ProjectSinglePlay/README.md` -- document ESinglePlayCharacterSystem
