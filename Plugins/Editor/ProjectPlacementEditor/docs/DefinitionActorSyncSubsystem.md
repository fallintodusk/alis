# DefinitionActorSyncSubsystem

Editor subsystem that syncs placed actors when definitions are regenerated.

## SOLID Architecture (Dependency Inversion)

This subsystem is DECOUPLED from ProjectDefinitionGenerator via ProjectEditorCore:

```
ProjectEditorCore (shared abstractions)
+-- FDefinitionEvents::OnDefinitionRegenerated() delegate
         ^                    ^
         |                    |
ProjectDefinitionGenerator    ProjectPlacementEditor
(broadcasts)                  (subscribes via this subsystem)
```

**Why this matters:**
- Neither plugin depends on the other - both depend on the abstraction
- Independent development and testing of each plugin
- Generator doesn't know about actors; sync subsystem doesn't know about JSON parsing

## Separation of Concerns (SOLID)

| Component | Responsibility |
|-----------|---------------|
| ProjectDefinitionGenerator | JSON -> UObjectDefinition asset generation |
| DefinitionActorSyncSubsystem | ALL definition update logic (CanReapply + Reapply + Replace) |
| ProjectObjectActorFactory | Initial actor spawning from definitions |
| AInteractableActor | Pure runtime container (interaction dispatch, metadata only) |

**Actor is SOLID-compliant:** Has NO knowledge of definitions. Subsystem owns all definition logic.

## Update Flow

```
1. User edits JSON file
          |
          v
2. DefinitionGeneratorEditorSubsystem detects change, shows regenerate notification
          |
          v
3. User clicks "Regenerate" -> DefinitionGeneratorSubsystem regenerates asset
          |
          v
4. Generator broadcasts FDefinitionEvents::OnDefinitionRegenerated()
          |
          v
5. This subsystem receives event, finds matching actors, shows countdown
          |
          v
6. After countdown (or "Apply Now"), updates actors via Reapply or Replace
```

## Two-Tier Update Approach

### 1. Reapply (default) - `ReapplyDefinition()`

Subsystem's in-place property update. Actor is just a container being manipulated.

**Preserves:**
- Actor references (other actors referencing this one)
- Sequencer bindings
- Attachments to other actors
- Folder assignments in World Outliner

**Used for:**
- Property changes (capability settings, etc.)
- Mesh transforms (position, rotation, scale within actor)
- Mesh asset swaps (different StaticMesh)

### 2. Replace (fallback) - `ReplaceActorFromDefinition()`

Destroy old actor, spawn new via factory when structure changes.

**Preserves:**
- Transform (world position/rotation/scale)
- Actor label
- Folder assignment

**Used for:**
- Structural changes: meshes added or removed
- Structural changes: capabilities added or removed

### Structure Detection - `CanReapplyDefinition()`

On-the-fly comparison of:
- Mesh IDs (via DefMeshId component tags)
- Capability types (via CDO PrimaryAssetId)

If structure matches, Reapply is safe. Otherwise, Replace.

## Key Properties

| Property | Location | Purpose |
|----------|----------|---------|
| `ObjectDefinitionId` | Actor | Links actor to source definition (metadata for subsystem lookup) |
| `DefinitionStructureHash` | Actor | Set by factory, reserved for future optimization |
| `DefMeshId` component tag | Mesh components | Links mesh to definition entry for reliable matching |

## Subsystem Methods

| Method | Purpose |
|--------|---------|
| `CanReapplyDefinition()` | Check if structure matches (mesh IDs + capability types) |
| `ReapplyDefinition()` | In-place update: mesh assets, transforms, capability properties |
| `ReplaceActorFromDefinition()` | Full replace: destroy old, spawn new via factory |

## Related Files

- [ProjectEditorCore/DefinitionEvents.h](../../ProjectEditorCore/Source/ProjectEditorCore/Public/DefinitionEvents.h) - Shared delegate
- [ProjectObjectActorFactory.h](../Source/ProjectPlacementEditor/Public/ProjectObjectActorFactory.h) - Sets tracking properties
- [AInteractableActor](../../../Resources/ProjectObject/Source/ProjectObject/Public/InteractableActor.h) - Pure runtime container (metadata only)

## See Also

- [ProjectObject README](../../../Plugins/Resources/ProjectObject/README.md#definition-update-system-auto-update-when-json-changes)
