# Skeletal Capabilities Rationale

SOT for why `ProjectSkeletalCapabilities` exists as a separate plugin from `ProjectObjectCapabilities`.

---

## Dependency Domain Isolation

The split is by dependency domain, not by gameplay concept.

`ProjectObjectCapabilities` is generic: Lockable, Pickup, Hinged, Audio. It has no third-party engine plugin dependencies.

Skeletal adapters drag in engine plugin dependencies:
- `MutableCustomizationCapability` depends on CustomizableObject (Mutable)
- `MotionMatchingCapability` will depend on PoseSearch (when wired beyond v1 stub)
- `ULocalBodyAnimInstance` depends on AnimGraphRuntime (CopyPose, ModifyBone nodes)

If these lived in ProjectObjectCapabilities, every consumer would inherit Mutable + PoseSearch + AnimGraphRuntime -- even worlds with only doors and pickups.

**Rule:** plugin name matches its dependency domain. Same registry, same capability mental model, different dependency domains.

---

## Adapters

| Capability | Dependencies | Status |
|-----------|-------------|--------|
| MutableCustomization | CustomizableObject | Complete -- explicit ComponentName mapping, one COI shared across CSKs, JSON-driven DefaultParameters |
| LocalFirstPerson | AnimGraphRuntime, AnimationCore | Complete -- bone hiding, visibility re-apply, LocalBodyAnimInstance |
| MotionMatching | Engine (reflection) | Complete -- PostProcess bridge feeds CMC data to MM AnimBP via reflection |

---

## Mutable Adapter Flow

1. Actor-scoped capability (one instance per actor)
2. Self-manages timing via assembly state delegate (waits for Ready)
3. Discovers target meshes by `AssemblyRole=*Customization` tags
4. Loads CO from `MutableSource` property
5. Creates COI via `CO->CreateInstance()`
6. Creates CSK per target with explicit ComponentName from `ComponentNameMapping` property
7. Triggers `UpdateSkeletalMeshAsync` on COI
8. Binds to `UpdatedNativeDelegate` for rebuild callbacks

**ComponentName mapping:** `"BodyCustomization=Body,HeadCustomization=Head,LocalBodyCustomization=Body"` -- explicit, not ordinal.

**COI strategy:** runtime `CreateInstance()`, not pre-authored COI asset. Legacy `COI_Hero` at `/Game/Project/Test/Mutable/` was a test artifact. Default clothing is applied via `DefaultParameters` property (JSON-driven).

---

## MotionMatching Adapter Flow

1. Capability waits for assembly Ready (same lifecycle as MutableCustomization)
2. Finds DriverBody mesh via `AssemblyRole=DriverBody` tag
3. Installs `UMotionMatchingBridgeAnimInstance` as PostProcess AnimInstance on DriverBody
4. PostProcess updates AFTER primary AnimBP (which zeros CharacterProperties via failed interface call)
5. Bridge reads CMC values (velocity, acceleration, movement mode, etc.) and writes CharacterProperties via cached FProperty reflection
6. Forces `UseThreadSafeUpdateAnimation=true` so Update_Logic defers to BlueprintThreadSafeUpdateAnimation (runs after our injection)

**Timing contract:** UE5 `TickAnimInstances()` order is Primary -> PostProcess -> ParallelUpdate. PostProcess injection window is guaranteed by engine.

**V1 defaults:** Gait hardcoded to Run, JustLanded/LandVelocity not tracked. Full gait system is future work.

**Current camera bridge baseline:** `RotationMode=1` (Strafe), `OrientationIntent=ActorRotation`, `AimingRotation=full control/base aim`.
Bridge also seeds the nested InputState flags needed by the sample AnimBP:
- `WantsToStrafe=true`
- `WantsToAim=true`
- `WantsToCrouch=CharacterMovement->IsCrouching()`

This is the current first-person baseline that restored camera/body correlation.
Long-term, RotationMode and InputState policy should come from view/input policy rather than staying bridge-owned.

---

## LocalBodyAnimInstance

`ULocalBodyAnimInstance` was moved from ProjectCharacter to this plugin because it is skeletal assembly infrastructure, not character-class-specific behavior. ProjectCharacter now depends on ProjectSkeletalCapabilities (not the reverse).

Copy-pose source discovery (no AProjectCharacter cast):
1. Role tag: `AssemblyRole=WorldBody` (production definition-driven path)
2. Role tag: `AssemblyRole=DriverBody` while the world visual layer is empty during early init
3. Component name fallback: `WorldBodyMesh` for historical `AProjectCharacter` inspection only

After Mutable rebuild, `WorldBody` is promoted to the generated body mesh and
re-initialized only when its mesh or anim instance actually changed. The
retarget AnimBP for production comes from object data (`Hero.json`), so the
owner-visible local body copies from the same retargeted world visual layer
without Mutable code overriding asset paths. The component-name fallback does
not participate in runtime pawn selection.

---

## LocalBody First-Person Correction

### Architecture

Strategy pattern via `ILocalBodyCorrection` interface. Each mode is a self-contained class owning its own anim nodes, settings, and evaluation logic. The anim instance is a thin orchestrator.

Files: `Source/ProjectSkeletalCapabilities/Public/LocalBody/` and `Private/LocalBody/`

Modes:
- `Disabled` -- pass-through, no correction (baseline etalon)
- `TransitionGuard` -- stateful correction driven by source-mesh drift and movement state
- `AngleClamp` -- reactive angle-based clamp (experimental)
- `ChainIK` -- CCDIK neck chain + TwoBoneIK arm restore (current default)

Pipeline (all modes share the base chain):
```
CopyPose -> CS -> Spine01(yaw 40%) -> Spine02(yaw 30%) -> [Correction] -> Local -> Output
```

ChainIK correction chain:
```
NeckLock(neck_01 BMM_Replace) -> CCDIK(spine_03..spine_05) -> TwoBoneIK(hand_l) -> TwoBoneIK(hand_r)
```

### Skeleton Chain (Body mesh)

```
pelvis -> spine_01 -> spine_02 -> spine_03 -> spine_04 -> spine_05 -> neck_01 -> head
                                                           -> clavicle_l -> upperarm_l -> lowerarm_l -> hand_l
                                                           -> clavicle_r -> upperarm_r -> lowerarm_r -> hand_r
```

spine_05 is the last visible bone before neck_01. Head is a separate mesh (SkipOwner).

### Policy Ownership

| System | Responsibility | NOT responsible for |
|---|---|---|
| MotionMatchingBridgeAnimInstance | Feed sample ABP contract | Camera anti-clip |
| MutableCustomizationCapability | Mutable/mesh rebuild/mapping | Visual policy for camera bugs |
| LocalFirstPersonCapability | Owner-only visibility, local body install | Pose correction |
| LocalBodyAnimInstance | Final camera-safety correction only | Bridge data, mutable state |
| DefinitionCharacter | Camera and movement only | Body clipping |

### Proven Dead Ends (do not retry)

- Bone rotation on hidden bones -- zero visual effect (HideBoneByName scales to zero)
- Bone rotation on visible bones (spine_05) -- tilts edge, can't close geometry hole
- Bone scale on visible bones -- deforms body shape, hand clips into body
- Hiding more bones -- removes too much body (waist stump)
- UE 5.5+ FirstPersonScale -- GPU-only, doesn't affect skeleton
- Heuristic based on camera pitch alone -- fails during movement transitions
- Camera-space geometric intrusion detection -- measures source, correction on local mesh, can't converge
- Additive spine pitch distribution -- rotating spine moves ALL children (clavicles, arms). Fundamental hierarchy problem.
- "Push body backward" approaches -- wrong framing. Problem is alignment, not distance.

### Key Discoveries

- HiddenBones "head,neck_01" was unnecessary. Head is separate mesh (SkipOwner). Hiding neck_01 created open mesh edge ("neck stump"). Removed.
- NeckLockNode must target visible bone (spine_05 is last visible before hidden neck_01)
- `GetControlRotation().Pitch` returns 0-360 in PIE (e.g. 270 for looking down). Must use `FRotator::NormalizeAxis()`.
- CCDIK `RotationLimitPerJoints` array must be pre-sized via `ResizeRotationLimitPerJoints()` even when `bEnableRotationLimit=false` (engine indexes it unconditionally)
- Strategy is latched at proxy init. Runtime mode change via reflection requires `InitAnim(true)` to rewire nodes.

### Hard Lessons

- Do NOT distribute pitch across spine bones (makes clipping worse)
- `ControlRot.Pitch` can wrap (330 vs -30) -- normalize first
- Automated test must check camera-volume intrusion, not bone rotation values
- Keep crouch fix separate from look-clipping fix
- Do not change bridge, mutable, local-body, and camera in one commit

### Test Infrastructure

Map: `Content/Project/Maps/Test/ClipMatrix_CleanMap`
Runner: `scripts/ue/test/character/capture_parity.ps1`

```powershell
# Baseline (no correction)
./scripts/ue/test/character/capture_parity.ps1 -TestFilter "ProjectIntegrationTests.Character.FirstPerson.ClipMatrix.Baseline" -TimeoutSeconds 900

# TransitionGuard (mode=1 via reflection)
./scripts/ue/test/character/capture_parity.ps1 -TestFilter "ProjectIntegrationTests.Character.FirstPerson.ClipMatrix.FilterV1" -TimeoutSeconds 900

# Full matrix (all 15 phases, runtime default)
./scripts/ue/test/character/capture_parity.ps1 -TestFilter "ProjectIntegrationTests.Character.FirstPerson.ClipMatrix.Default" -TimeoutSeconds 900
```

Output: `Saved/Validation/ClipMatrix/` (JSONL timeline + summary JSON + edge screenshots)

---

## Cross-References

- [Assembly architecture](../../Systems/ProjectSkeletalAssembly/docs/architecture.md) - one-registry model, lifecycle
- [Layer contract](../../Resources/ProjectObject/docs/layer_contract.md) - Kind/Role/Visibility on meshes
- [Character design](../ProjectCharacter/docs/design.md) - definition-driven character
