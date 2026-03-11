# ProjectMotionSystem

Skeleton-agnostic motion primitives for the Alis project.

## Purpose

Provides reusable procedural motion outputs - NOT "play feel" or skeleton-specific animation. Used by characters, trees, props, doors, and any moving object.

## Key Principle

**No skeletal mesh dependencies.** This plugin knows nothing about:
- Bone names
- Curve names
- Animation assets
- Specific skeletons

## Openable Motion (Doors, Drawers)

ProjectObject openables use:
- `USpringRotatorComponent` for hinged motion
- `USpringSliderComponent` for sliding motion

### Motion Modes

1) Kinematic (no Chaos constraints)
- Recommended default for stability.
- Slider uses swept translation to stop on blocking hits.
- Hinge uses collision gating (overlap or blocking checks) to pause motion and avoid pushing the pawn.

2) Chaos constraints (experimental)
- Known issue in UE 5.7: Chaos island teardown can crash on PIE exit
  (FPBDIslandManager UnbindEdgeFromNodes / Reset).
- Use only for prototype testing. Do not enable for production content.
- If you need physics blocking, keep it opt-in and isolated.

Project policy: Prefer kinematic collision-aware motion (slider swept
translation + hinge collision gating) for openables. Chaos constraints are not
considered stable for shipping in the current engine version.

Opt-in key: set capability property `MotionMode = "Chaos"`
(default = "Kinematic").
Chaos config values in JSON do not enable Chaos by themselves.
If MotionMode is omitted, kinematic is used and only KinematicConfig values apply.
Defaults come from component CDO (KinematicConfig Stiffness 100, Damping 20).
See docs/motion_modes.md for kinematic preset values.

Editor config grouping:
- Kinematic settings live in `KinematicConfig`
- Chaos settings live in `ChaosConfig`
- JSON property keys remain unchanged

## Action Receiver (IProjectActionReceiver)

`USpringMotionComponentBase` implements `IProjectActionReceiver`. Dialogue or other
systems can drive motion via action strings. **Authority-only**: no-ops on client.

| Action | Method |
|--------|--------|
| `motion.open` | Open() -- deterministic, always opens |
| `motion.close` | Close() -- deterministic, always closes |
| `motion.toggle` | Toggle() -- reverses current state |

Prefer `motion.open` in scripted sequences (deterministic). Use `motion.toggle` for
player interaction (existing OnComponentInteract behavior).

## Folder Structure

```
Source/ProjectMotionSystem/
|- Public/
|  |- Core/           # FProjectMotionPose, EGait, EStance, types
|  |- Components/     # UMotionSourceComponent
|  |- Solvers/        # UMotionSolverLibrary, IK math
|  |- Procedural/     # Sway, bob, rotate (non-skeletal)
|  `- Interfaces/     # IMotionDataProvider
`- Private/
   `- ...
```

## Deliverables

### Core Types (`Core/`)

| Type | Description |
|------|-------------|
| `FProjectMotionPose` | Locomotion state struct: speed, direction, gait, stance, in_air, lean, stride, stamina |
| `EProjectGait` | Enum: Idle, Walk, Run, Sprint |
| `EProjectStance` | Enum: Upright, Crouch, Prone |

### Components (`Components/`)

| Component | Description |
|-----------|-------------|
| `UMotionSourceComponent` | Computes pose from owner velocity/acceleration without knowing skeleton |

### Solvers (`Solvers/`)

| Class | Description |
|-------|-------------|
| `UMotionSolverLibrary` | Static functions: spring-damper, smoothing, simple IK math |

### Procedural (`Procedural/`)

| Component | Description |
|-----------|-------------|
| `UProceduralSwayComponent` | Wind sway for trees, flags, foliage |
| `UProceduralBobComponent` | Vertical bob for floating objects, buoys (TODO) |
| `UProceduralRotateComponent` | Continuous rotation for fans, spinning objects (TODO) |

### Interfaces (`Interfaces/`)

| Interface | Description |
|-----------|-------------|
| `IMotionDataProvider` | Interface for characters/pawns to provide motion data |

## Hard Rules

1. **No `USkeletalMeshComponent` dependency** - unless absolutely required
2. **No bone names, curve names, or anim assets** - skeleton-specific goes in ProjectAnimation
3. **GAS-agnostic** - receives stamina as float, doesn't know about AttributeSets

## Usage

### For Characters

```cpp
// Character implements IMotionDataProvider
class AProjectCharacter : public ACharacter, public IMotionDataProvider
{
    virtual float GetMovementSpeed() const override { return GetVelocity().Size(); }
    // ...
};

// Full integration example:
// See Plugins/Resources/ProjectAnimation/README.md (#in-character-wiring-point)
```

### For Trees/Props

```cpp
// Just add the component
UPROPERTY()
UProceduralSwayComponent* SwayComponent;

// Configure in editor or code
SwayComponent->SwayAmplitude = 10.0f;
SwayComponent->SwayFrequency = 0.3f;
SwayComponent->WindDirection = FVector(1, 0, 0);
```

## Dependencies

- `ProjectCore` - foundation interfaces, logging

## Consumers

- `ProjectAnimation` - skeleton-specific animation (depends on this)
- `ProjectCharacter` - implements `IMotionDataProvider`
- World objects - use procedural components directly

## Related Documentation

- [Animation System Overview](../../../docs/systems/animation.md)
- [ProjectAnimation](../../Resources/ProjectAnimation/README.md)
- [Motion Modes and Chaos Notes](docs/motion_modes.md)
