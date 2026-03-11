# Motion Modes

## Purpose

This document captures the durable decisions and critical nuance for motion
drivers in ProjectMotionSystem. It explains the default policy, why Chaos is
experimental, and the signals to watch in logs.

## Policy

- Default: kinematic collision-aware motion for openables.
- Chaos constraints: opt-in only for prototypes.
- JSON key: MotionMode = "Chaos" (default = "Kinematic").
- If MotionMode is omitted, kinematic is used and only KinematicConfig values apply.
- Chaos config values in JSON do not enable Chaos by themselves.
- Defaults come from component CDO (KinematicConfig Stiffness 100, Damping 20).
- Legacy: Force Chaos Mode (bForceChaosMode) is deprecated, avoid in new content.
- JSON overrides can still use legacy keys (e.g., Stiffness, HingePivotLocal);
  they map to KinematicConfig/ChaosConfig fields.

## Architecture Summary

- Components are routers: select driver and pass targets.
- Drivers apply motion and own Chaos lifecycle.
- Kinematic hinge uses collision gating. Kinematic slider uses swept translation.
- Chaos driver uses PhysicsConstraintComponent.
- Config is grouped on components:
  - KinematicConfig for kinematic values
  - ChaosConfig for Chaos values
  - JSON property keys remain unchanged

## Kinematic Presets (Starter Values)

These are general-purpose values for small openables. Tune per asset.

- Small hinged door: Stiffness 120, Damping 22
- Small drawer or light slider: Stiffness 110, Damping 20
- If motion feels too slow, raise Stiffness by 10-20 and Damping by 2-4

## Known Issue (UE 5.7)

Chaos island teardown can crash on PIE exit:
- FPBDIslandManager UnbindEdgeFromNodes / Reset
- Happens during solver destruction when constraint graph is corrupted

Root causes seen:
- Anchor has no physics body at bind time.
- Over-touching constraint teardown during EndPlay.

## Chaos Requirements

- Anchor must have a physics body: PhysicsOnly collision, not QueryOnly.
- Use AddInstanceComponent + OnComponentCreated + RegisterComponent.
- Use SetDisableCollision(true) on the constraint.
- Defer setup until physics state exists (driver retry loop).

## Logging Signals (Alis.log)

- BeginPlay logs MotionMode, SelectedMode, ForceChaos, mesh state.
- Chaos drivers log setup OK/FAIL and retry attempts.
- EndPlay logs start and done for the component.

## Test Steps (Chaos)

1) PIE open map with hinged and sliding objects.
2) Open and close each object 20 times.
3) Exit PIE and confirm no crash.
4) Check Saved/Logs/Alis.log for setup failures.

## References

- https://forums.unrealengine.com/t/physics-component-dynamic-removal-add-cause-chaos-physics-to-crash/2662057
- https://forums.unrealengine.com/t/blueprint-set-to-simulate-physics-but-collision-enabled-is-incompatible/335391
