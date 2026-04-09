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

`ULocalBodyAnimInstance` was moved from ProjectCharacter to this plugin because it is skeletal assembly infrastructure, not legacy-character-specific behavior. ProjectCharacter now depends on ProjectSkeletalCapabilities (not the reverse).

Pipeline: `CopyPoseFromMesh -> LocalToCS -> ModifyBone(root) -> CSToLocal -> Output`

Copy-pose source discovery (no AProjectCharacter cast):
1. Role tag: `AssemblyRole=WorldBody` (preferred modular path)
2. Component name fallback: `WorldBodyMesh` (legacy path)
3. Role tag: `AssemblyRole=DriverBody` only while the world visual layer is still empty during early init

After Mutable rebuild, `WorldBody` is promoted to the generated body mesh and
re-initialized only when its mesh or anim instance actually changed. The
retarget AnimBP now comes from object data (`Hero.json` or legacy Blueprint
defaults), so both legacy and modular owner-visible local body copy from the
same retargeted world visual layer without Mutable code overriding asset paths.

---

## Cross-References

- [Assembly architecture](../../Systems/ProjectSkeletalAssembly/docs/architecture.md) - one-registry model, lifecycle
- [Layer contract](../../Resources/ProjectObject/docs/layer_contract.md) - Kind/Role/Visibility on meshes
- [Character design](../ProjectCharacter/docs/design.md) - legacy vs modular character
