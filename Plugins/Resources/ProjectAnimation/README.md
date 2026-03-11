# ProjectAnimation

**Universal animation asset library** for the Alis project - skeletal meshes, animations, blendspaces, and skeleton-bound glue code.

## Purpose

**Universal animation asset library** - same pattern as other primitive resource plugins:

| Plugin | Asset Type |
|--------|------------|
| ProjectTexture | Textures |
| ProjectMaterial | Materials |
| ProjectMesh | Static meshes |
| **ProjectAnimation** | **Skeletal meshes, animations, blendspaces** |
| ProjectAudio | Sound assets |

Contains:
- **Skeletal mesh templates** (base rigs, skeletons)
- **Animation sequences** (locomotion cycles, actions)
- **Blendspaces** (locomotion blends)
- **Animation Blueprints** (animation state logic)
- **Physics assets** (ragdoll setup)
- **Thin runtime module** for skeleton-bound glue

This plugin knows about specific skeletons, bone names, and curve names.

## Key Principle

**This is an asset library, NOT a character repository.**

- **Composed characters** (player, NPCs, creatures) go to **ProjectObject** (`Human/`, `Animal/`)
- **Reusable animation assets** stay here
- Depends on **ProjectMotionSystem**, NOT gameplay modules

If you find yourself importing ProjectCharacter or ProjectGAS, STOP and move that logic to ProjectMotionSystem or ProjectCharacter.

## Architecture Mental Model

```
Resources Layer - Universal Asset Libraries:
  ProjectTexture    -> texture assets
  ProjectMaterial   -> material assets
  ProjectMesh       -> static mesh assets
  ProjectAnimation  -> skeletal meshes, animations, blendspaces  <-- THIS (universal library)
  ProjectAudio      -> sound assets

Resources Layer - Compositions:
  ProjectObject     -> composed objects (Human/, Animal/, HumanMade/, Nature/)
                       Uses assets from universal libraries above

Systems Layer - Functions:
  ProjectMotionSystem -> universal motion algorithms, procedural, IK
```

## Folder Structure

```
Source/ProjectAnimation/
|- Public/
|  |- AnimInstance/      # UProjectAnimInstanceBase
|  |- Drivers/           # ISkeletonLocomotionDriver
|  `- DataAssets/        # ULocomotionAnimSet
`- Private/
   `- ...

Content/
|- Humanoid/             # Humanoid skeleton assets
|  |- Skeleton/          # Base humanoid skeleton
|  |- Animations/        # Locomotion, actions, etc.
|  `- Blendspaces/       # Locomotion blendspaces
|- Quadruped/            # Four-legged creature assets
|  |- Skeleton/
|  |- Animations/
|  `- Blendspaces/
`- Shared/               # Cross-skeleton assets
   `- ...
```

**NOTE:** Composed characters (BP_PlayerCharacter, BP_ZombieNPC, etc.) live in **ProjectObject/Content/Human/** or **ProjectObject/Content/Animal/**. They REFERENCE assets from here.

## Deliverables

### Runtime Module

| Class | Description |
|-------|-------------|
| `UProjectAnimInstanceBase` | Light base AnimInstance with motion pose variables |
| `ISkeletonLocomotionDriver` | Interface for skeleton-specific bone/curve mapping |
| `ULocomotionAnimSet` | Data asset mapping gait/stance -> sequences/blendspaces |

### Content (TODO)

| Asset | Description |
|-------|-------------|
| Skeletal meshes | Character/NPC meshes |
| Skeletons | Skeleton assets |
| Animation sequences | Locomotion cycles, actions |
| Blendspaces | Locomotion blendspaces |
| Animation Blueprints | Per-skeleton-family ABPs |
| Physics assets | Ragdoll setup |

## Hard Rules

1. **May depend on Foundation + Systems only** - no Gameplay imports
2. **If gameplay logic creeps in, move it** - to ProjectMotionSystem or ProjectCharacter
3. **Skeleton-specific data stays here** - bone names, curve names, animation assets

## Dependencies

- `ProjectCore` - foundation interfaces
- `ProjectMotionSystem` - motion pose types, interfaces

## Usage

### In Character (Wiring Point)
 
 This is the standard pattern for driving the animation system from Gameplay code (e.g., `AProjectCharacter`).
 
 ```cpp
 // ProjectCharacter::UpdateMotionPose (Tick)
 void AProjectCharacter::UpdateMotionPose(float DeltaTime)
 {
     // 1. Compute pure motion (ProjectMotionSystem responsibility)
     //    - Uses physics velocity, acceleration, falling state
     //    - No gameplay opinion
     FProjectMotionPose Pose;
     Pose.Speed = GetVelocity().Size();
     Pose.Direction = GetVelocity().GetSafeNormal();
     Pose.bInAir = GetCharacterMovement()->IsFalling();
 
     // 2. Gameplay logic overrides (ProjectCharacter responsibility)
     //    - Reads GAS attributes (Stamina, Health)
     //    - Decides Gait (Sprint vs Run)
     if (AbilitySystemComponent)
     {
         float Stamina = ASC->GetNumericAttribute(StaminaAttr);
         // Downgrade sprint if out of stamina
         if (Stamina < SprintThreshold && DesiredGait == EProjectGait::Sprint)
         {
             Pose.Gait = EProjectGait::Run;
         }
         else
         {
             Pose.Gait = DesiredGait;
         }
     }
     Pose.Stance = CurrentStance;
 
     // 3. Push to AnimInstance (ProjectAnimation responsibility)
     //    - The bridge between motion data and the AnimGraph
     if (auto* AnimInstance = Cast<UProjectAnimInstanceBase>(GetMesh()->GetAnimInstance()))
     {
         AnimInstance->SetMotionPose(Pose);
     }
 }
 ```
 
 ### In Animation Blueprint (AnimGraph)
 
 1.  Reparent your AnimBP to `UProjectAnimInstanceBase`.
 2.  Use the `MotionPose` variables (Speed, Gait, Stance) to drive your State Machine.
 3.  **Do not cast to Character** inside the AnimBP (strict decoupling).

## Orchestrator Registration

This plugin is registered via `dev_manifest.json`, NOT `Alis.uproject`.

- `ProjectMotionSystem` - **AlwaysOn** (small code, many uses)
- `ProjectAnimation` - **OnDemand** (heavy content)

## Related Documentation

- [Animation System Overview](../../../docs/systems/animation.md)
- [ProjectMotionSystem](../../Systems/ProjectMotionSystem/README.md)
