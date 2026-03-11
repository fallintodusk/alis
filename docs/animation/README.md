# Animation System

Overview of the animation architecture for the Alis project.

## Architecture

The animation system follows the Resources/Systems separation pattern:

```
Systems/ProjectMotionSystem/     <- Skeleton-agnostic motion algorithms (FUNCTIONS)
        |
        v
Resources/ProjectAnimation/      <- Universal animation asset library (like ProjectMesh, ProjectTexture)
        |
        v
Resources/ProjectObject/         <- Composed characters: Human/, Animal/ (COMPOSITIONS)
        ^
        |
Gameplay/ProjectCharacter/       <- Wiring point (owns GAS integration)
```

## Mental Model

| Layer | Plugin | Role |
|-------|--------|------|
| **Systems** | ProjectMotionSystem | Universal motion functions (procedural, IK, state) |
| **Resources** | ProjectAnimation | **Universal animation asset library** (same pattern as ProjectMesh, ProjectTexture, ProjectMaterial) |
| **Resources** | ProjectObject | Composed objects (characters USE assets from above) |

## Plugin Responsibilities

### 1. ProjectMotionSystem (The Functions)
**Location:** [`Plugins/Systems/ProjectMotionSystem/`](../../Plugins/Systems/ProjectMotionSystem/README.md)
- **Role:** Skeleton-agnostic motion algorithms (Speed, Direction, Sway, IK).
- **No** skeletal meshes, bone names, or animation assets.
- **Consumers:** Characters, Prop Sway, Doors, Vehicles.

### 2. ProjectAnimation (Universal Asset Library)
**Location:** [`Plugins/Resources/ProjectAnimation/`](../../Plugins/Resources/ProjectAnimation/README.md)
- **Role:** **Universal animation asset library** - same pattern as ProjectMesh/ProjectTexture/ProjectMaterial.
- **Contains:** Skeletal mesh templates, Animations, Blendspaces, AnimBPs, Physics Assets.
- **Depends on:** ProjectMotionSystem.
- **NOT for:** Composed characters (those go to ProjectObject).

### 3. ProjectObject (The Compositions)
**Location:** [`Plugins/Resources/ProjectObject/`](../../Plugins/Resources/ProjectObject/README.md)
- **Role:** Composed world objects including characters.
- **Contains:** `Human/` (player, NPCs), `Animal/` (creatures), `HumanMade/`, `Nature/`.
- **Characters here** reference assets from ProjectAnimation.
 
 ## Data Flow
 
 ```
 [Character Tick] -> [ProjectMotionSystem] -> [FProjectMotionPose] -> [Adjust for Gameplay (GAS)] -> [ProjectAnimation (AnimInstance)]
 ```
 
 **For the full integration code example, see:**
 -> **[ProjectAnimation Integration Guide](../../Plugins/Resources/ProjectAnimation/README.md#in-character-wiring-point)**
 
 ## GAS Integration
 
 GAS integration lives in **ProjectCharacter**, NOT in the animation system. The character reads attributes (Stamina) and downgrades the Gait in the `FProjectMotionPose` before sending it to the AnimInstance.
 
 ## Related Documentation
 
 - [ProjectMotionSystem README](../../Plugins/Systems/ProjectMotionSystem/README.md)
 - [ProjectAnimation README](../../Plugins/Resources/ProjectAnimation/README.md)
 - [Plugin Architecture](../architecture/plugin_rules.md)
