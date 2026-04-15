# Proper Motion Physics - PhysicsConstraint Implementation

## Status: DECISION - Prefer kinematic collision-aware motion (slider swept translation + hinge collision gating), Chaos constraints experimental

NOTE: Canonical reference is now
Plugins/Systems/ProjectMotionSystem/docs/motion_modes.md
Keep this file as a working log.

Implementation complete (Phases 1-4 done), but PIE exit crashes in Chaos.

Decision:
- Restore kinematic collision-aware motion as the default for openables.
- Keep Chaos constraints only for prototype experiments (explicit opt-in via MotionMode = "Chaos", default = "Kinematic").
- Chaos config values in JSON do not enable Chaos by themselves.
- If MotionMode is omitted, kinematic is used and only KinematicConfig values apply.
- JSON overrides can use legacy keys (e.g., Stiffness, HingePivotLocal);
  they map to KinematicConfig/ChaosConfig fields.

Goals:
- Simple, SOLID code path for doors and sliders.
- No Chaos crash on PIE exit.
- Easy switch between kinematic and Chaos without duplicating components.

Config note:
- Editor config is grouped into KinematicConfig and ChaosConfig on each component.
- JSON property keys remain unchanged.

---

## Current Blocker

### Symptom

PIE exit crashes in `FPBDIslandManager` during physics scene destruction:

```
UnrealEditor-Chaos.dll!Chaos::Private::FPBDIsland::Trash() Line 522
  -> IslandManager.cpp:522 (Array index out of bounds / RangeCheck)
UnrealEditor-Chaos.dll!FPBDIslandManager::Reset() Line 705
UnrealEditor-Chaos.dll!Chaos::FPBDRigidsEvolutionBase::ResetConstraints() Line 723
UnrealEditor-Chaos.dll!Chaos::FPhysicsSolverBase::DestroySolver() Line 341
...
UnrealEditor-Engine.dll!UWorld::FinishDestroy() Line 1559
```

### Root Cause Analysis

1. **Constraint bound to body with no physics state**: If anchor doesn't have a valid physics body when constraint is created, Chaos binds to invalid handle. On teardown, island graph has stale/corrupted edges.

2. **Key insight (Epic forums)**: `QueryOnly` collision may NOT create physics body. Need `PhysicsOnly` or `QueryAndPhysics`.

### What We Tried

| # | Attempt | Result |
|---|---------|--------|
| 1 | `TermComponentConstraint()` in EndPlay | Crash |
| 2 | `ConstraintInstance.TermConstraint()` directly | Crash |
| 3 | Disable physics on mesh BEFORE term | Crash |
| 4 | `BreakConstraint()` before term | Crash |
| 5 | `SetConstrainedComponents(nullptr)` before term | Crash |
| 6 | Add `IsPhysicsStateCreated()` guards | Crash |
| 7 | Use `AddInstanceComponent()` + `RF_Transient` | Crash |
| 8 | Change anchor `QueryOnly` -> `PhysicsOnly` | UNTESTED (needs PIE exit test) |

### Current Fix (Applied, UNTESTED)

```cpp
// MotionConstraintHelpers.cpp - anchor creation
OutCreatedAnchor->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
OutCreatedAnchor->SetMobility(EComponentMobility::Movable);
OutCreatedAnchor->SetGenerateOverlapEvents(false);
Owner->AddInstanceComponent(OutCreatedAnchor);
OutCreatedAnchor->OnComponentCreated();
```

Also added:
- Deferred constraint setup retries (timer, up to 10 attempts) in Chaos drivers when physics state is not ready.
- Logging for Chaos setup OK/FAIL and retry attempts. Base logs BeginPlay and EndPlay.

Test steps (required):
1) Open map with hinged and sliding objects.
2) PIE, open and close each object 20 times.
3) Exit PIE and confirm no crash.
4) Check `Saved/Logs/Alis.log` for constraint setup errors.

---

## Plan (Detailed)

### Phase 1 - Refactor for switchable motion

Purpose: Separate "what motion to do" from "how motion is applied".

1) Define a small motion driver strategy (kinematic and Chaos implementations).
- Use `enum class EMotionMode { Kinematic, Chaos };` for selection.
- Implement drivers as small non-UObject structs/classes (no UInterface).
- Store drivers as members or TUniquePtr (no UObject ownership).
- Keep SpringRotatorComponent and SpringSliderComponent as facades. They select a driver and pass inputs.
- Put drivers in a clear folder, for example:
  - `Private/MotionDrivers/Kinematic/`
  - `Private/MotionDrivers/Chaos/`
- Avoid duplicating components. The component decides the driver and owns lifecycle.
- Keep the driver API tiny: Init(...), Tick(...), Shutdown().

2) Add a simple selection rule.
- Default to Kinematic.
- Chaos only when an explicit flag is present (JSON or component flag).
- Key for JSON: MotionMode = "Chaos" (default = "Kinematic").
- Keep the decision helper in one place (SpringMotionComponentBase::ShouldUseChaosMode).

3) Keep logging, but make it mode-aware.
- BeginPlay logs MotionMode and selected driver mode (Chaos or Kinematic).
- Chaos drivers log setup OK/FAIL and retry attempts.
- EndPlay logs start/done for the component.

### Phase 2 - Restore kinematic collision-aware motion as default

Purpose: Re-enable last stable behavior and use it as baseline.

1) Compare history and locate last known good kinematic version.
- Use `git log` on:
  - `Plugins/Systems/ProjectMotionSystem/Source/ProjectMotionSystem/Private/Components/`
  - `Plugins/Systems/ProjectMotionSystem/Source/ProjectMotionSystem/Public/Components/`
- Identify the commit before Chaos was introduced.

2) Extract kinematic behavior.
- Copy the kinematic logic into the new Kinematic driver.
- Keep rotation/translation math from the old code.
- Ensure slider uses swept translation and hinge uses collision gating (no constraints).

3) Integrate with new driver selection.
- Components call the driver update per tick.
- Chaos driver is only constructed when explicit opt-in is set.

4) Mitigate "pawn kick" in kinematic mode.
- Gate motion when overlapping the pawn capsule (pause or clamp).
- Optionally reduce impulses by adjusting collision response while moving.
- Keep collisions for static world if required.

### Phase 3 - Stabilize Chaos path (optional)

Purpose: Keep Chaos as experimental, with strict guardrails.

- Keep all Chaos code in the Chaos driver.
- Only create constraints if physics state is ready.
- Keep AddInstanceComponent + PhysicsOnly anchor setup.
- Provide explicit logging so crashes are attributable to Chaos driver only.

### Phase 4 - Validation

Acceptance criteria:
- Default path uses kinematic driver and does not create constraints.
- No PIE exit crash when opening and closing doors.
- Logs show motion driver selection and lifecycle steps.

Notes:
- Chaos path stays opt-in until Epic resolves the FPBDIslandManager crash.

---

## Key Files

| File | Purpose |
|------|---------|
| `MotionConstraintHelpers.cpp:37-51` | Anchor creation - **PhysicsOnly fix here** |
| `SpringMotionComponentBase.cpp:14-90` | BeginPlay/EndPlay lifecycle logging |
| `MotionDrivers/Chaos/RotatorChaosDriver.cpp` | Chaos constraint setup, retry, logging |
| `MotionDrivers/Chaos/SliderChaosDriver.cpp` | Chaos constraint setup, retry, logging |

---

## References

- [Epic Forum: Dynamic constraint crash](https://forums.unrealengine.com/t/physics-component-dynamic-removal-add-cause-chaos-physics-to-crash/2662057)
- [Epic Forum: Collision vs Physics body](https://forums.unrealengine.com/t/blueprint-set-to-simulate-physics-but-collision-enabled-is-incompatible/335391)
- UE 5.7 Source: `Chaos/Private/Chaos/Island/IslandManager.cpp`

---

## Schema Reference (for JSON authoring)

Note: To opt in to Chaos constraints, add "MotionMode": "Chaos" to properties.

### Hinged Capability

```json
{
  "type": "Hinged",
  "scope": ["door"],
  "properties": {
    "OpenAngle": "-95",
    "HingePivotLocal": "(0,-50,0)",
    "HingeMotorStrength": "8000",
    "HingeMotorDamping": "150",
    "HingeMotorMaxForce": "200000"
  }
}
```

### Sliding Capability

```json
{
  "type": "Sliding",
  "scope": ["drawer"],
  "properties": {
    "ClosedPosition": "(X=0 Y=0 Z=0)",
    "OpenPosition": "(X=0 Y=30 Z=0)",
    "SliderMotorStrength": "5000",
    "SliderMotorDamping": "100",
    "SliderMotorMaxForce": "100000"
  }
}
```

---

## Gotchas

1. **Anchor needs physics body**: `PhysicsOnly` collision, not `NoCollision`/`QueryOnly`
2. **Constraint frames**: Use `p.Chaos.DebugDraw.ConstraintAxis 1` to visualize
3. **Motor sleep**: Disable motor when at target to allow physics sleep
4. **Self-collision**: `SetDisableCollision(true)` prevents jitter at pivot
5. **Deprecated API**: Use `SetOrientationDriveTwistAndSwing(twist, swing)` not old API
