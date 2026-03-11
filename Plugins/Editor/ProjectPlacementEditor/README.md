# ProjectPlacementEditor

Dockable panel for spawning template actors and DataAssets.

Editor-only plugin that connects generated definitions to placed actors and keeps scene instances synchronized when those definitions change.

## Data Pipeline Role

**This plugin handles PROPAGATION** - cascading dependency updates (Definition -> Actor updates).

**NOT in scope:**
- **SYNC** (mechanical updates like asset path fixes) -> See [ProjectAssetSync](../ProjectAssetSync/README.md)
- **GENERATION** (JSON parsing and DataAsset creation) -> See [ProjectDefinitionGenerator](../ProjectDefinitionGenerator/README.md)

**How it fits in the pipeline:**
1. **SYNC**: Asset renamed -> JSON paths updated -> Redirectors fixed
2. **GENERATION**: JSON changed -> DataAsset regenerated -> Broadcasts `OnDefinitionRegenerated`
3. **PROPAGATION** (this plugin): Subscribes to `OnDefinitionRegenerated` -> Updates scene actors (Reapply/Replace strategies)

**See:** [Data Pipeline Architecture](../../../docs/data/README.md#data-pipeline-architecture) for complete flow documentation.

## Usage

**Window > Project Placement** opens the panel.

### Templates Section

C++ actor classes - click button to spawn:

| Template | Description |
|----------|-------------|
| Hinged Openable | Door, window, hatch (rotates on hinge) |
| Sliding Openable | Sliding door, drawer, panel (linear motion) |
| Pickup Item | Generic pickup (set ObjectDefinitionId after placement) |
| Loot Container | Container with loot (set LootContents after placement) |

### Items Section

[UObjectDefinition](../../Resources/ProjectObject/Source/ProjectObject/Public/Data/ObjectDefinition.h) DataAssets via `CreateAssetPicker` (pickup-capable objects):
- Large thumbnails, folder hierarchy, search
- Double-click: opens asset editor
- Drag-drop to viewport: spawns `AProjectPickupItemActor`

### Objects Section

[UObjectDefinition](../../Resources/ProjectObject/Source/ProjectObject/Public/Data/ObjectDefinition.h) DataAssets via `CreateAssetPicker`:
- Large thumbnails, folder hierarchy, search
- Double-click: opens asset editor
- Drag-drop to viewport: spawns definition-selected actor class (`spawnClass` if set, else `AInteractableActor`) with meshes and capabilities
- Capabilities resolved via [FCapabilityRegistry](../../Gameplay/ProjectObjectCapabilities/Source/ProjectObjectCapabilities/Public/CapabilityRegistry.h) (CDO scan)
- Asset resolution: stable ID via `GetPrimaryAssetId()`, folder path is for browsing only

**Architecture:** See [Flexible Path Pattern](../../../docs/architecture/flexible_path.md) for ID-based asset resolution.

**Thumbnail mechanics:** See [Thumbnail Mechanics](docs/thumbnail_mechanics.md) for engine interaction details, cache strategy, and troubleshooting.

#### Capability Filtering

[SProjectObjectBrowser](Source/ProjectPlacementEditor/Private/Widgets/SProjectObjectBrowser.cpp) provides two-level dynamic filtering:

**Level 1 - Capability Filter:**
- Buttons auto-generated from discovered capabilities in AssetRegistry
- Filters via `ALIS.Cap.<Name>` tags (no asset loads)
- Only shows capabilities that exist in C++ (`FCapabilityRegistry::HasCapability` validation)
- Error notification if JSON references unimplemented capabilities

**Level 2 - Item.Type Filter (visible when Pickup selected):**
- Buttons auto-generated from `ALIS.ItemTag.Item.Type.*` tags in content
- Shows only types that exist (Consumable, Equipment, Currency, etc.)

**Auto-refresh:** Filters refresh on AssetRegistry scan complete + manual Refresh button.

## Architecture

| File | Purpose |
|------|---------|
| `SProjectAssetPicker.cpp` | Combined UI: templates + items + objects |
| `SProjectItemBrowser.cpp` | Items asset browser widget |
| `SProjectObjectBrowser.cpp` | Objects asset browser widget |
| `ProjectPlacementEditor.cpp` | Tab registration, asset listeners |
| `ProjectPickupItemActorFactory.cpp` | ObjectDefinition (pickup) -> Actor spawn logic |
| `ProjectObjectActorFactory.cpp` | ObjectDefinition -> Actor spawn logic, **definition tracking** |

### ProjectObjectActorFactory - Definition Tracking

When spawning actors, the factory enables auto-update when JSON changes:

1. **Sets `ObjectDefinitionId`** - links actor to source definition
2. **Sets `DefinitionStructureHash`** - enables structural change detection
3. **Tags mesh components** with `DefMeshId=<mesh_id>` - enables reliable matching during reapply

This supports the two-tier update system:
- **Reapply**: Property/transform changes update in-place (preserves references)
- **Replace**: Structural changes (meshes/capabilities added/removed) respawn actor

See [ProjectObject README](../../Resources/ProjectObject/README.md#definition-update-system-auto-update-when-json-changes) for details.

### Placement Troubleshooting (Definition Actors)

If drag-drop placement fails, check Output Log for fail-fast guardrail messages:

- `SpawnActor failed: Failed to persist definition host state ...`
- `SpawnActor failed: Capability property apply failed ...`

Interpretation:

- Host state failure means definition metadata did not persist on spawned actor. Actor is destroyed intentionally to avoid broken sync state.
- Capability apply failure means definition JSON contains a property that could not be applied to spawned capability component.

Fix path:

1. Regenerate the definition asset from source JSON.
2. Fix reported JSON property/tag issue.
3. Place again from Project Placement.

## Subsystems

### DefinitionActorSyncSubsystem - Actor Auto-Update System

Automatically synchronizes placed world actors when their definitions are modified. Implements three update modes:

**Mode 0: Manual Editor Updates (Interactive)**
- Triggered when definition saved in editor
- Shows 5-second countdown with Apply Now/Cancel buttons
- Uses Apply (in-place) or Replace (respawn) strategies
- Undo support via FScopedTransaction

**Mode 1: Passive World Partition Updates (Silent)**
- Triggered when actors load from external packages (World Partition streaming)
- Silent updates (no countdown or UI)
- Content hash check for idempotency
- Warning notification only on structural mismatch

**Mode 2: Batch Commandlet Updates (Silent)**
- Triggered during actor saves (WorldPartitionResaveActorsBuilder, manual saves)
- PreSave hook in AProjectWorldActor
- Idempotent via content hash check
- Scales to thousands of actors

**Documentation:**
- [Architecture](docs/ActorSyncArchitecture.md) - Full system design, flow diagrams, implementation details
- [Quick Reference](docs/ActorSyncQuickReference.md) - Common tasks, troubleshooting, API reference

## Adding Templates

1. Add module dependency to `Build.cs`
2. Add include and entry to `Templates` array in `SProjectAssetPicker::Construct()`

## Dependencies

- `ProjectObject` - UObjectDefinition, openable actors, AInteractableActor
- `ProjectObjectCapabilities` - FCapabilityRegistry
- `ProjectEditorCore` - FDefinitionEvents delegate for decoupled pub/sub
- `GameplayTags` - FGameplayTag property special-case in SetPropertyByName

## Legacy Paths

Code marker format:
- `// LEGACY_OBJECT_PARENT_GENERALIZATION(L###): <reason>. Remove when <condition>.`

| Legacy ID | Location | Why It Exists | Remove Trigger |
|-----------|----------|---------------|----------------|
| _(none active)_ | n/a | Sync discovery now uses host abstraction instead of `AProjectWorldActor` iteration | n/a |

## References

- [ProjectObject](../../Resources/ProjectObject/README.md)
- [ProjectEditorCore](../ProjectEditorCore/README.md)
- [Thumbnail Mechanics](docs/thumbnail_mechanics.md)
