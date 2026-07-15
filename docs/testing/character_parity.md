# Character Parity Testing

SOT for automated character regression capture and agent-driven animation debugging
for the definition-driven `DefinitionCharacter` runtime. The automation namespace
retains `Character.Parity` for stable test filters, but the deleted `BP_Hero` path
is not part of the runtime or pass/fail contract.

Use this doc when an agent needs to investigate:
- Hero spawn and runtime wiring
- definition-driven `DefinitionCharacter` spawn and behavior
- SkeletalAssembly lifecycle
- Motion Matching and Mutable integration
- first-person body behavior and clipping
- parity captures and saved runtime dumps
- Blueprint CDO defaults and saved inspection artifacts

---

## Quick Start

```powershell
# All character parity-namespace tests (idle + locomotion + anim sanity)
.\scripts\ue\test\character\capture_parity.ps1 -TimeoutSeconds 300

# Camera-yaw parity timeline only
.\scripts\ue\test\character\capture_parity.ps1 -TestFilter "ProjectIntegrationTests.Character.Parity.CameraYawTimeline" -TimeoutSeconds 600

# Clean-path isolation first: same definition-driven hero through Mode A-D
.\scripts\ue\test\character\capture_parity.ps1 -TestFilter "ProjectIntegrationTests.Character.Parity.CleanPathIsolationMatrix" -TimeoutSeconds 900

# Skip ObjectDefinition regeneration only if you explicitly want to test against
# the currently generated assets on disk
.\scripts\ue\test\character\capture_parity.ps1 -SkipDefinitionGeneration

# Locomotion timeline only
.\scripts\ue\test\character\capture_parity.ps1 -TestFilter "ProjectIntegrationTests.Character.Parity.LocomotionTimeline" -TimeoutSeconds 240

# Idle snapshot only
.\scripts\ue\test\character\capture_parity.ps1 -TestFilter "ProjectIntegrationTests.Character.Parity.IdleSnapshot"
```

Output: `Saved/Validation/CharacterDebug/` (JSON, JSONL, PNG sidecars with unique RunId)

Preflight:
- `capture_parity.ps1` runs `GenerateDefinitions -type=Object` before the test
- this keeps `Hero.json` and the generated `Hero.uasset` in sync for definition-driven spawn

## Five Focused Tests

| Test | Filter | Duration | What |
|------|--------|----------|------|
| IdleSnapshot | `...Parity.IdleSnapshot` | ~15s | Capture the definition-driven hero at rest |
| CleanPathIsolationMatrix | `...Parity.CleanPathIsolationMatrix` | ~180s | Definition-driven 4-mode fault-line isolation: driver -> retarget -> local -> full Mutable |
| CameraYawTimeline | `...Parity.CameraYawTimeline` | ~80s | Definition-driven scripted camera yaw sweep, bridge fields, visible mesh propagation |
| LocomotionTimeline | `...Parity.LocomotionTimeline` | ~68s | 15-phase movement matrix, JSONL + summary |
| SimpleAnimSanity | `...Parity.SimpleAnimSanity` | ~12s | Bypass ABP, play AnimSequence on DriverBody |

### CleanPathIsolationMatrix Baseline

Use the same spawned definition-driven hero in this order:

1. Mode A - `DriverBody` only
2. Mode B - `DriverBody -> WorldBody`
3. Mode C - `DriverBody -> WorldBody -> LocalBody`
4. Mode D - full Mutable chain

The first mode that breaks is the real fault line.

Each mode runs the same fixed matrix:
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

Each sample records:
- phase, time, engine frame
- actor/control yaw
- speed, acceleration, movement mode
- gait, stance, movement direction
- rotation mode, state machine state, target rotation delta
- `Enable_AO`, `ShouldTurnInPlace`, `NoValidAnim`, `CurrentSelectedAnim`
- transition history tail
- input-state aim/strafe/crouch flags
- driver/world/local bone transforms
- visible mesh identity

Failure layers:
- Layer 1 - MM contract
- Layer 2 - raw driver pose
- Layer 3 - retarget propagation
- Layer 4 - local/customization propagation

### CameraYawTimeline Matrix (13 phases)

| # | Phase | Duration | Input |
|---|-------|----------|-------|
| 0 | IdleSettle | 1.0s | none |
| 1 | IdleYaw30 | 1.0s | idle, yaw 0 -> 30 |
| 2 | IdleYaw60 | 1.0s | idle, yaw 30 -> 60 |
| 3 | IdleYaw100 | 1.0s | idle, yaw 60 -> 100 |
| 4 | IdleHold100 | 1.0s | idle, hold yaw 100 |
| 5 | IdleBackTo0 | 1.0s | idle, yaw 100 -> 0 |
| 6 | MoveForward | 2.0s | forward |
| 7 | MoveForwardYaw90 | 2.0s | forward, yaw 0 -> 90 |
| 8 | StrafeLeft | 1.5s | left |
| 9 | StrafeRight | 1.5s | right |
| 10 | CrouchIdleYaw90 | 1.5s | crouch idle, yaw 0 -> 90 |
| 11 | CrouchForwardYaw | 1.5s | crouch forward, yaw 90 -> 180 |
| 12 | StopFinal | 1.0s | none |

CameraYawTimeline samples every 0.25s and records:
- actor yaw, control yaw, actor-control delta
- top-level ABP values such as `RotationMode`, `Enable_AO`, `ShouldTurnInPlace`, `AOYaw`
- bridge-fed `CharacterProperties` such as `OrientationIntent` and `AimingRotation`
- driver mesh, world-visible mesh, and owner-visible mesh bone transforms and anim bindings
- turn-state evidence such as `TransitionHistory`, `CurrentSelectedAnim`, and `TargetRotationDelta`
- root-turn evidence from root bone facing, not only component yaw

Use this test to answer a specific question:
- are bridge values wrong, or is the visible definition-driven mesh chain bypassing the retargeted world visual layer?

Current gate intent:
- `BridgeTracksControlYaw` must be true in yaw-sweep phases
- definition-driven `WorldBody` must be alive and retargeted (`ABP_WorldBodyRetarget_C`)
- owner-visible local body must explicitly copy pose from `WorldBody`, not raw `DriverBody`
- exact upper-chain pose equality is not required because camera-safety correction intentionally modifies the local spine/neck/head after CopyPose
- torso/head must respond during yaw-sweep phases
- large idle-yaw phases must either enter a turn clip/state or remain in an already-active turn clip while body/root catch-up continues
- large turn phases must also show a root/body catch-up sign:
  actor yaw movement, root bone facing movement, or non-zero target rotation delta

Important:
- `IdleHold100` is a hold phase, not a fresh yaw-response phase
- the earlier 2026-04-07 "pass" overstated correctness because the old gate could false-pass on idle sway
- the revised gate is stricter: it no longer counts a stale phase-boundary sample or raw head motion as proof of turning
- this test now aims to prove both propagation and sustained turn-state behavior for large idle-yaw phases
- `CameraYawTimeline` validates only the current definition-driven runtime

### LocomotionTimeline Movement Matrix (15 phases)

| # | Phase | Duration | Input |
|---|-------|----------|-------|
| 0 | IdleSettle | 2.0s | none |
| 1 | ForwardAccel | 1.0s | forward |
| 2 | ForwardSteady | 2.0s | forward |
| 3 | ReleaseStop | 1.5s | none |
| 4 | Backward | 2.0s | back |
| 5 | StrafeLeft | 2.0s | left |
| 6 | StrafeRight | 2.0s | right |
| 7 | DiagForwardLeft | 2.0s | fwd+left |
| 8 | DiagForwardRight | 2.0s | fwd+right |
| 9 | CrouchIdle | 1.0s | crouch |
| 10 | CrouchForward | 2.0s | crouch+fwd |
| 11 | Uncrouch | 1.0s | uncrouch |
| 12 | TurnLeft | 1.5s | yaw -90/s |
| 13 | TurnRight | 1.5s | yaw +90/s |
| 14 | JumpFallLand | 2.0s | jump |

Three-layer sampling every 0.25s: Movement ground truth, ABP semantic state, component-space bone poses.

### LocomotionTimeline Output Files

- `definition_locomotion_timeline_{RunId}.jsonl` -- one JSON object per sample
- `definition_locomotion_summary_{RunId}.json` -- per-phase peak speed, foot vertical range, crouch/air flags

### CameraYawTimeline Output Files

- `definition_camera_yaw_timeline_{RunId}.jsonl`
- `definition_camera_yaw_summary_{RunId}.json`

### CleanPathIsolationMatrix Output Files

- `definition_clean_path_timeline_{RunId}.jsonl`
- `definition_clean_path_summary_{RunId}.json`

### IdleSnapshot Flow

1. boots the game with `-ProjectSkipFrontEnd`
2. verifies the possessed pawn is `DefinitionCharacter`
3. waits for Mutable rebuild
4. captures the definition-driven character
5. verifies the JSON sidecar exists

---

## Source Of Truth

Read these in order before making conclusions from runtime data.

| Path | Why it matters |
|------|----------------|
| `Plugins/Resources/ProjectObject/Content/Human/Hero/Hero.json` | Hero runtime definition SOT |
| `Plugins/Resources/ProjectObject/docs/layer_contract.md` | JSON `meshes`, `capabilities`, `sections`, Kind/Role/Visibility |
| `Plugins/Systems/ProjectSkeletalAssembly/docs/architecture.md` | Assembly lifecycle, registry, debug capture ownership |
| `Plugins/Gameplay/ProjectSkeletalCapabilities/docs/architecture.md` | Mutable, Motion Matching, LocalFirstPerson boundaries |
| `Plugins/Gameplay/ProjectCharacter/docs/design.md` | Definition-driven character responsibilities |
| `Plugins/Gameplay/ProjectSinglePlay/README.md` | Definition selection and spawn policy |
| `docs/animation/README.md` | High-level animation layering only |

---

## Data-Driven Model

Treat the runtime path like this:

```text
Hero.json
  -> meshes[]
  -> capabilities[]
  -> sections.animation / customization / view
  -> ObjectSpawnUtility
  -> DefinitionCharacter + capability components
  -> runtime captures in Saved/Validation/CharacterDebug
```

Key current hero facts:
- `Hero.json` spawns `/Script/ProjectCharacter.DefinitionCharacter`
- hero capabilities currently include `SkeletalAssembly`, `MotionMatching`, `MutableCustomization`, `LocalFirstPerson`, `DebugCapture`
- hero sections currently include:
  - `animation` -> `locomotionProfile`, `traversalProfile`
  - `customization` -> `mutableSource`
  - `view` -> `defaultMode`, `cameraParent`, `attachmentPolicy`, `relativeOffset`

---

## How It Works

1. `capture_parity.ps1` preloads the editor-built `ProjectIntegrationTests`
   module, then calls `run_cpp_tests_safe.ps1` with `-ProjectSkipFrontEnd`
   and one shared `CharacterCaptureRunId` for every selected test
2. Boot bypass in `UProjectLoadingSubsystem::StartInitialExperience()` skips menu travel
3. `FCharacterParityIdleTest` runs as a latent command:
   - Stage 0: Wait for and verify a possessed `DefinitionCharacter`
   - Stage 1: Wait for Mutable rebuild and capture definition-driven state
   - Stage 2: Verify the JSON sidecar exists
4. `FCharacterParityLocomotionTest` runs the 15-phase movement matrix on the
   same definition-driven pawn and writes a definition timeline and summary.
5. `FCharacterParityCameraYawTest` validates the definition-driven camera/body path:
   - Stage 0: Wait for possessed pawn in any game world
   - Stage 1: Ensure `DefinitionCharacter` is active
   - Stage 2: Wait for Mutable rebuild and retargeted visual chain
   - Stage 3: Settle streaming and animation state
   - Stage 4: Run the 13-phase yaw timeline
   - Stage 5: Write the definition summary and validate turn-state/root-catch-up evidence
6. `FCharacterCleanPathIsolationTest` isolates the definition-driven mesh chain:
   - Stage 0: Wait for possessed pawn in any game world
   - Stage 1: Ensure `DefinitionCharacter` is active
   - Stage 2: Capture original mesh wiring
   - Stage 3: Apply Mode A-D one at a time on the same hero
   - Stage 4: Run the shared scripted matrix in each mode
   - Stage 5: Write the definition clean-path summary and classify the first failing layer

The wrapper rejects the run unless the test module ready marker is present and
the discovered, started, and completed counts agree. Duplicate start or
completion markers, missing expected names, unexpected names, and any failed
test also reject the run. The default broad filter locks the exact five test
names. Required artifacts must exist and be non-empty for the same shared RunId;
C++ write failures are automation errors, and the wrapper independently checks
the resulting files. Captures, timelines, and summaries are reported separately;
unrelated JSON schemas are never suggested as a diff.

Important harness note:
- older camera-yaw runs captured one stale sample at each phase boundary before the new phase input had actually taken effect
- this could contaminate a phase with the previous phase's yaw, movement, or crouch state
- the revised test starts phase measurement only after the new phase input is active

Primary implementation entry points:
- capture wrapper: `scripts/ue/test/character/capture_parity.ps1`
- idle automation test:
  `Plugins/Test/ProjectIntegrationTests/Source/ProjectIntegrationTests/Private/Integration/CharacterParityIdleTest.cpp`
- locomotion automation test:
  `Plugins/Test/ProjectIntegrationTests/Source/ProjectIntegrationTests/Private/Integration/CharacterParityLocomotionTest.cpp`
- dedicated camera-yaw automation test:
  `Plugins/Test/ProjectIntegrationTests/Source/ProjectIntegrationTests/Private/Integration/CharacterParityCameraYawTest.cpp`
- runtime capture component:
  `Plugins/Systems/ProjectSkeletalAssembly/Source/ProjectSkeletalAssembly/Public/CharacterDebugCaptureComponent.h`

---

## Debug Commands

| Command | Effect |
|---------|--------|
| `project.character.debug 0` | Hide overlay |
| `project.character.debug 1` | Show overlay with runtime model, pawn, capabilities, camera, meshes, bones |
| `project.character.capture [label]` | Screenshot + JSON sidecar to `Saved/Validation/CharacterDebug/` |

---

## Overlay Fields

- Runtime model identifier (`definition_driven` for ObjectDefinition-hosted captures)
- Pawn class, definition ID
- Capability IDs and active state
- Camera parent, socket, transform
- Mesh components: asset, parent, visibility flags, animClass
- Selected bone transforms: root, pelvis, spine_03, spine_05, neck_01, head

---

## Historical Blueprint Baseline

`BP_Hero.uasset` is no longer tracked and must not be treated as an executable
runtime path. The saved inspection below is historical evidence for explaining
older tuning values only:

```text
Saved/Inspection/BP_Hero_CDO_2026-04-03.json
```

Current behavior is authoritative in `Hero.json`, `DefinitionCharacter`, capability
configuration, and fresh runtime captures.

---

## Runtime Fields To Compare

Good first checks in the capture JSONs:
- `runtimeModel`
- `definitionId`
- `pawnClass`
- `assemblyState`
- `capabilities`
- `camera`
- `movement`
- `capsule`
- `meshes`
- `boneTransforms`
- filenames grouped by the same RunId

Important definition-driven expectations:
- `assemblyState = Ready`
- active capabilities include `MotionMatching` and `LocalFirstPerson`
- mesh roles appear for:
  - `DriverBody`
  - `WorldBody`
  - `LocalBody`
  - `Head`
  - `BodyCustomization`
  - `HeadCustomization`
  - `LocalBodyCustomization`

---

## Log Markers

Search logs for:
- `LogSkeletalAssembly`
- `CharacterDebugCapture`
- `MutableCustomization`
- `MotionMatching`
- `DefinitionCharacter`
- `Timed out`
- `FAILED`

Ignore repetitive `LogVitalsViewModel: Warning: RefreshFromASC: Cached ASC is stale/null` unless the task is explicitly about vitals binding.

---

## Boot Bypass

`-ProjectSkipFrontEnd` flag in `ProjectLoadingSubsystem.cpp` skips the Orchestrator's initial experience load (which travels to MainMenu). The gameplay map stays loaded so automation tests have a possessed pawn.

Only affects standalone `-game` mode with the flag. Editor (PIE) and normal game boot are unaffected.

---

## Known Non-Bugs

Do not file these as regressions without stronger evidence:
- some movement defaults were seeded from the historical Blueprint baseline and may intentionally differ from older dynamic gait/strafe behavior
- bone transform differences are expected when capture timing or movement state differs
- parent role meshes may be empty while Mutable output lives on child customization meshes
- visibility-driven defaults only apply when a mesh entry explicitly sets `visibility`

---

## Cross-References

- [Assembly architecture](../../Plugins/Systems/ProjectSkeletalAssembly/docs/architecture.md) - framework design
- [Capability rationale](../../Plugins/Gameplay/ProjectSkeletalCapabilities/docs/architecture.md) - adapter boundaries
- [Capture script](../../scripts/ue/test/character/capture_parity.ps1) - automation wrapper
- [Idle test](../../Plugins/Test/ProjectIntegrationTests/Source/ProjectIntegrationTests/Private/Integration/CharacterParityIdleTest.cpp) - capture validation
- [UE inspection guide](agent_ue_inspection.md) - broader dump and inspection patterns
