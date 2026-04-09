# Migrate Placement Editor from Template Classes to JSON Definitions

Parent doc: [create_skeletal_assembly_framework.md](create_skeletal_assembly_framework.md) (Phase 3.5 follow-up)

Status: planned

---

## Problem

The editor placement toolbar (`SProjectTemplateBar`) spawns actors from hardcoded C++ template classes:

```cpp
{ AHingedOpenable::StaticClass(), "Hinged Openable", "Door, window, hatch" },
{ ASlidingOpenable::StaticClass(), "Sliding Openable", "Sliding door, drawer" },
```

These template classes (`AOpenableActor`, `AHingedOpenable`, `ASlidingOpenable`) are legacy pre-capability code. They duplicate what the data-driven path already does: `ObjectSpawnUtility::SpawnFromDefinition()` creates an `AInteractableActor` with the right capabilities from JSON.

This forced `ProjectObject` to depend on `ProjectMotionSystem` (for `USpringRotatorComponent`). Phase 3.5 moved them to `ProjectOpenableTemplates` as a temporary fix, but the real fix is removing the template classes entirely.

## Solution

Replace template class references in the placement toolbar with JSON definition references. The toolbar picks a definition by ID and calls `SpawnFromDefinition()`.

### Before (template classes)

```
Designer drags "Hinged Door" from toolbar
  -> Editor spawns AHingedOpenable (hardcoded C++ class)
  -> Class has USpringRotatorComponent baked in constructor
  -> Requires ProjectMotionSystem compile-time dependency
```

### After (definitions)

```
Designer drags "Hinged Door" from toolbar
  -> Editor picks ObjectDefinition:Door_Template_Hinged
  -> Calls SpawnFromDefinition()
  -> AInteractableActor + Hinged capability from registry
  -> No compile-time dependency on ProjectMotionSystem
```

### JSON templates for placement

Create simple template definitions in ProjectObject for the toolbar:

```
Plugins/Resources/ProjectObject/Content/Template/
  Door_Template_Hinged.json      <- basic door with Hinged + Lockable
  Door_Template_Sliding.json     <- basic drawer with Sliding
  Interactable_Template.json     <- bare interactable actor
```

These are NOT game content -- they're editor convenience templates for placement. Designers customize them after placing (change mesh, adjust properties).

Example `Door_Template_Hinged.json`:
```json
{
  "$schema": "../../Data/Schemas/object.schema.json",
  "id": "Door_Template_Hinged",
  "meshes": [
    { "id": "body", "asset": "/Engine/BasicShapes/Cube" }
  ],
  "capabilities": [
    { "type": "Hinged", "scope": ["body"], "properties": { "OpenAngle": "-85" } },
    { "type": "Lockable", "scope": ["body"] }
  ]
}
```

### Placement toolbar update

`SProjectTemplateBar` entries change from class references to definition IDs:

```cpp
// Before
{ AHingedOpenable::StaticClass(), "Hinged Openable", "Door, window, hatch" },

// After
{ FPrimaryAssetId("ObjectDefinition", "Door_Template_Hinged"), "Hinged Door", "Door, window, hatch" },
```

## Steps

- [ ] Create template JSON definitions in `ProjectObject/Content/Template/`
- [ ] Update `SProjectTemplateBar` to spawn from definitions instead of class references
- [ ] Remove `ProjectOpenableTemplates` plugin entirely (delete all files)
- [ ] Remove `ProjectOpenableTemplates` from `Alis.uproject`
- [ ] Remove `ProjectOpenableTemplates` dependency from `ProjectPlacementEditor.Build.cs`
- [ ] Verify toolbar drag-and-drop still works in editor
- [ ] Verify spawned actors have correct capabilities (Hinged swings, Sliding slides)

## Result

- `ProjectOpenableTemplates` plugin deleted (zero legacy template code)
- `ProjectObject` stays clean (no motion system dependency)
- Placement toolbar is fully data-driven (same path as JSON definitions)
- Template definitions live alongside other content in ProjectObject
- One spawn path for everything: manual placement AND level loading
