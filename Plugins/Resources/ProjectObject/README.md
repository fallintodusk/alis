# ProjectObject

Composed world objects for ALIS - characters, creatures, buildings, furniture, and interactive elements.

This README describes the code-side composition model. Some folder examples refer to the full project checkout; the public mirror excludes Unreal content payloads and other non-code assets.

## Purpose

- **ACTORS ONLY** - this plugin contains AActor classes that compose components
- Placeable C++ actors for world objects
- Composes capability components from ProjectObjectCapabilities
- Composes motion components from ProjectMotionSystem
- Thin wrappers - behavior lives in reusable components
- Pattern: Actor = Mesh + Capability Components

## Actors vs Components (IMPORTANT)

```
ProjectObject (this)                 ProjectObjectCapabilities (Gameplay/)
--------------------                 ------------------------------------
ACTORS that compose                  COMPONENTS ONLY (reusable capabilities)

AInteractableActor (base)            ULockableComponent
AOpenableActor (Mesh + Lock)         UHealthComponent (future)
AHingedOpenable (+ Rotator)          UPowerableComponent (future)
ASlidingOpenable (+ Slider)
```

**Rule:** If you're creating a placeable world object, put it HERE.
If you're creating a reusable capability (lock, health, power), put it in ProjectObjectCapabilities.

## Architecture

```
Plugins/Resources/ProjectObject/
|-- Content/
|   |-- Human/                       <- Humanoid characters (player, NPCs)
|   |-- Animal/                      <- Animal creatures
|   |-- Template/                    <- Man-made object templates
|   |   `-- Fenestration/            <- Doors, windows (Content)
|   `-- Nature/                      <- Natural objects (rocks, plants)
`-- Source/ProjectObject/
    |-- Public/
    |   `-- Template/                <- Base actor templates (C++)
    |       |-- Interactable/
    |       |   `-- InteractableActor.h    <- Base: interaction dispatch
    |       `-- Openable/
    |           |-- OpenableActor.h        <- Base: Mesh + LockComponent
    |           |-- HingedOpenable.h       <- Adds SpringRotatorComponent
    |           `-- SlidingOpenable.h      <- Adds SpringSliderComponent
    `-- Private/
        `-- Template/
            |-- Interactable/
            |   `-- InteractableActor.cpp
            `-- Openable/
                `-- *.cpp
```

## Content Categories

| Folder | Purpose | Examples |
|--------|---------|----------|
| `Human/` | Humanoid characters with skeletal meshes | Player, NPCs, zombies |
| `Animal/` | Animal creatures with skeletal meshes | Dogs, birds, mutants |
| `Template/` | Man-made static/interactive objects | Buildings, furniture, doors |
| `Nature/` | Natural world objects | Rocks, trees (non-animated) |

**NOTE:** Characters in `Human/` and `Animal/` REFERENCE assets from **universal asset libraries**:
- ProjectAnimation (skeletal templates, animations, blendspaces)
- ProjectMesh (static mesh parts)
- ProjectMaterial (materials)
- ProjectTexture (textures)

They **compose** these universal assets into complete characters/objects.

## Dependency Flow

```
ProjectInventory (Features)
    |
    v depends on
ProjectObject (Resources) <- this plugin
    |
    v depends on
ProjectAnimation (skeletal templates, animations - for Human/Animal)
ProjectMotionSystem (USpringRotatorComponent, procedural motion)
ProjectObjectCapabilities (ULockableComponent)
ProjectWorld (AProjectWorldActor base)
ProjectCore
```

## Door Actor

`AProjectDoorActor` - Interactive door composing motion and lock capabilities.

### Components

| Component | Source | Purpose |
|-----------|--------|---------|
| `UStaticMeshComponent` | Engine | Door visual mesh |
| `USpringRotatorComponent` | ProjectMotionSystem | Spring-damper rotation |
| `ULockableComponent` | ProjectObjectCapabilities | Key/lock access control |

### Placement

Place via **Place Actors -> PROJECT_Template -> Project Door Actor**

(Registered by [ProjectPlacementEditor](../../Editor/ProjectPlacementEditor/README.md))

### Configuration

Configure via components in Details panel:

**RotatorComponent (Motion):**
| Property | Default | Description |
|----------|---------|-------------|
| `OpenAngle` | 90 | Rotation angle when open (degrees) |
| `Stiffness` | 100 | Spring stiffness (higher = faster) |
| `Damping` | 10 | Damping ratio (higher = less oscillation) |

**LockComponent (Access):**
| Property | Default | Description |
|----------|---------|-------------|
| `LockTag` | Empty | Required key tag (empty = unlocked) |
| `bConsumeKeyOnUnlock` | false | Whether key is consumed on use |

**Actor:**
| Property | Description |
|----------|-------------|
| `DoorMeshAsset` | Door visual mesh (soft reference) |

### Lock/Key System

Doors can be locked via `LockComponent.LockTag` (gameplay tag). The lock component blocks interaction
and provides a UI prompt via `GetInteractionPrompt`. Key checks and inventory consumption are not
wired here yet. That integration belongs to gameplay/interaction logic, not ProjectObject.

### Interaction Flow (Single Selection)

When multiple capabilities are eligible for the same interaction, `AInteractableActor` selects exactly one capability.
Selection follows the same rule as focus/HUD label resolution.

```
Player presses E on "door" mesh
    |
    v
AInteractableActor::OnInteract
    |
    Select best capability:
      1) highest-priority mesh-scoped capability matching hit hierarchy
      2) else highest-priority actor-scoped capability
    |
    [100] LockableComponent::OnComponentInteract()
            -> executes once (no multi-trigger fan-out)
```

**Selection rules:**
- **Mesh-scoped**: highest-priority matching capability wins.
- **Actor-scope fallback**: if no mesh match, highest-priority actor-scoped capability wins.
- **No hit component**: highest-priority cached capability executes.

### API

```cpp
// Convenience (delegates to RotatorComponent)
void ToggleDoor();

// Access components directly for queries
RotatorComponent->bIsOpen;
RotatorComponent->bIsAnimating;
LockComponent->IsLocked();
LockComponent->GetLockTag();
```

## Data-Driven Objects

[UObjectDefinition](Source/ProjectObject/Public/Data/ObjectDefinition.h) enables fully data-driven object creation from JSON.

**Pattern:** JSON describes complete object (meshes + capabilities + optional item data), factory builds dynamically on `spawnClass` (or falls back to [AInteractableActor](Source/ProjectObject/Public/Template/Interactable/InteractableActor.h)).

**Note:** Do not add a "$schema" field to object JSON files under `Plugins/Resources/ProjectObject/Content/`. The generator does not use it, and the schema does not allow it.

**Architecture:** See [Layer Contract](docs/layer_contract.md) for the 3-layer separation (Capabilities / Item Data / GAS Profiles).

### Structure

| Property | Type | Description |
|----------|------|-------------|
| `ObjectId` | FName | Stable ID (decoupled from asset path) |
| `SpawnClass` | TSoftClassPtr | Optional parent actor class for spawn (`spawnClass` JSON field). Accepted input forms: `/Game/.../BP_Name` or `/Script/Module.ClassName` |
| `AttachToComponentTag` | FName | Optional default attach root tag for generated meshes |
| `ActorTags` | TArray<FName> | Optional actor tags applied to spawned actor instance (`actorTags` JSON field) |
| `Meshes` | TArray | Mesh ID + soft object ref + optional materials |
| `Capabilities` | TArray | Type + scope + properties |

**ID Resolution:** `ObjectId` is stable for gameplay/runtime references (survives folder moves). Folder path is for editor browsing only. Cross-plugin lookup via `AssetManager->GetPrimaryAssetObject()`.

### Mesh Entry Fields

Each mesh entry in `Meshes` array supports:

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `id` | string | [v] | Mesh identifier for capability scoping |
| `asset` | string | [v] | Path to StaticMesh (auto-normalized) |
| `parent` | string | | Parent mesh ID for hierarchical attachment |
| `transform` | object | | Relative location/rotation/scale |
| `physics` | object | | Mass, damping, collision settings |
| `materials` | array | | Material overrides (index = slot) |

**Material Overrides:**
- Array index maps to material slot index (0 = slot 0, etc.)
- Supports short paths: `/ProjectObject/Materials/M_Wood`
- Auto-normalized to full soft paths: `/ProjectObject/Materials/M_Wood.M_Wood`
- Null entries use mesh default for that slot
- Empty array or omitted = use all mesh defaults

**Example:**
```json
{
  "id": "frame",
  "asset": "/ProjectObject/.../SM_DoorFrame",
  "materials": [
    "/ProjectObject/Materials/M_Oak",
    null,
    "/Game/Materials/M_Handle"
  ]
}
```

### Capabilities

Capabilities are C++ components with stable IDs via `GetPrimaryAssetId()`:

| ID | Component | Scope |
|----|-----------|-------|
| `Lockable` | [ULockableComponent](../../Gameplay/ProjectObjectCapabilities/Source/ProjectObjectCapabilities/Public/Access/LockableComponent.h) | actor |
| `Hinged` | [USpringRotatorComponent](../../Systems/ProjectMotionSystem/Source/ProjectMotionSystem/Public/Components/SpringRotatorComponent.h) | mesh |
| `Sliding` | [USpringSliderComponent](../../Systems/ProjectMotionSystem/Source/ProjectMotionSystem/Public/Components/SpringSliderComponent.h) | mesh |

### AssetRegistry Tags

`UObjectDefinition` exports capability tags to AssetRegistry for UI filtering without asset loads. See [GetAssetRegistryTags](Source/ProjectObject/Private/Data/ObjectDefinition.cpp).

**Tags exported:**

| Tag | Example Value | Purpose |
|-----|---------------|---------|
| `ALIS.Cap.<Name>` | `"true"` | Boolean per capability (Hinged, Lockable, Pickup, etc.) |
| `ALIS.Section.<Name>` | `"true"` | Boolean per section (Item, Loot, etc.) |
| `ALIS.ItemTag.<Tag>` | `"true"` | Boolean per item tag (e.g., `ALIS.ItemTag.Item.Type.Consumable`) |
| `DisplayName` | `"Water Bottle"` | Tooltip display (from Item section) |
| `Weight` | `"0.5"` | Tooltip display (from Item section) |

**Usage:** UI reads tags via `FAssetData::GetTagValue()` - no asset load required. Enables filtering thousands of objects instantly.

**Scope behavior:**
- `["actor"]` - single component on actor root
- `["panel"]` - component bound to that mesh via interaction target interface
- `["left", "right"]` - spawns component per mesh (same config)

### Placement

**Window > Project Placement > Objects** section:
- Double-click: opens asset editor
- Drag-drop to viewport: spawns actor

### Placement Guardrails (Fail Fast)

Spawn now fails fast instead of silently placing partially-invalid actors.

Error signatures to watch in Output Log:

- `ObjectSpawn: Failed to persist definition host state ...`
- `ObjectSpawn: Capability property apply failed ...`

Meaning:

- Host state error: actor was spawned but definition metadata did not round-trip correctly (`ObjectDefinitionId` and hashes).
- Capability apply error: one or more capability properties from JSON could not be applied (for example wrong property name, bad gameplay tag, unsupported value format).

Expected response:

1. Regenerate the definition from source JSON.
2. Fix the reported failing property in JSON or tag registration.
3. Re-place actor from Project Placement (do not keep partially-configured old instance).

**Architecture:** See [Flexible Path Pattern](../../../docs/architecture/flexible_path.md)

### Definition Update System (Auto-Update When JSON Changes)

When JSON definitions change, placed actors are automatically updated without manual intervention.

**See:** [Data Pipeline Architecture](../../../docs/data/README.md#data-pipeline-architecture) for complete sync/generation/propagation flow.

**Update behavior:**

**Two-tier approach:**

| Change Type | Action | What's Preserved |
|-------------|--------|------------------|
| Property value (OpenPosition, Stiffness...) | **Reapply** | Everything - references, sequencer, attachments |
| Mesh transform | **Reapply** | Everything |
| Mesh asset path | **Reapply** | Everything |
| Mesh material overrides | **Reapply** | Everything |
| Mesh added/removed | **Replace** | Transform, label, folder |
| Capability added/removed | **Replace** | Transform, label, folder |

**Implementation details:**
- `ObjectDefinitionId` on actor links it to source definition
- `DefinitionStructureHash` detects structural changes
- Mesh components tagged with `DefMeshId=<id>` for reliable matching
- Actor update flow is handled by ProjectPlacementEditor and the definition propagation pipeline described in [../../../docs/data/README.md](../../../docs/data/README.md)

**Logging:** Full cycle logging under `LogInteractableActor` and `LogDefinitionGeneratorEditor` categories

---

## Adding New Object Types (C++ Templates)

Follow the pattern:

1. Create header in `Public/Template/<Category>/`
2. Create implementation in `Private/Template/<Category>/`
3. Inherit from `AProjectWorldActor` (provides DataId GUID)
4. Compose capability components as needed
5. Register in [ProjectPlacementEditor](../../Editor/ProjectPlacementEditor/README.md)
6. Add interaction handling in appropriate Features plugin

## Legacy Paths

Code marker format:
- `// LEGACY_OBJECT_PARENT_GENERALIZATION(L###): <reason>. Remove when <condition>.`

| Legacy ID | Location | Why It Exists | Remove Trigger |
|-----------|----------|---------------|----------------|
| `L001` | `Source/ProjectObject/Private/Spawning/ObjectSpawnUtility.cpp` (`SpawnObjectFromDefinitionInternal`) | Keep fallback spawn behavior for definitions without `spawnClass` | Remove when all targeted definitions use explicit parent/host path and fallback-free smoke tests pass |

## References

- [ProjectPlacementEditor](../../Editor/ProjectPlacementEditor/README.md) - Place Actors registration
- [ProjectMotionSystem](../../Systems/ProjectMotionSystem/README.md) - USpringRotatorComponent
- [ProjectObjectCapabilities](../../Gameplay/ProjectObjectCapabilities/README.md) - ULockableComponent
- [ProjectInventory](../../Features/ProjectInventory/README.md) - Interaction handling
- [ProjectWorld](../../World/ProjectWorld/README.md) - Base actor class
