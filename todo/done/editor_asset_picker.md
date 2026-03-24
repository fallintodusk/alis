# Editor Asset Picker - TODO

Roadmap for extending the placement system.

**Implementation docs:** [ProjectPlacementEditor README](../../Plugins/Editor/ProjectPlacementEditor/README.md)

**Completed:** Phase 1 (Items Picker), Phase 1.5 (Universal Definition Generator), Phase 2 (Objects in Picker)

**Architecture:** See [Flexible Path Pattern](../../docs/architecture/flexible_path.md) for ID-based asset resolution.

---

## Phase 2: Objects in Picker (COMPLETED)

Fully data-driven objects. JSON is complete blueprint - meshes with capabilities and settings.

### Implementation Summary

| Component | Location | Purpose |
|-----------|----------|---------|
| [UObjectDefinition](../../Plugins/Resources/ProjectObject/Source/ProjectObject/Public/Data/ObjectDefinition.h) | ProjectObject | DataAsset with meshes + capabilities |
| [FCapabilityRegistry](../../Plugins/Editor/ProjectPlacementEditor/Source/ProjectPlacementEditor/Public/CapabilityRegistry.h) | ProjectPlacementEditor | CDO scan for capability IDs |
| [UProjectObjectActorFactory](../../Plugins/Editor/ProjectPlacementEditor/Source/ProjectPlacementEditor/Public/ProjectObjectActorFactory.h) | ProjectPlacementEditor | Spawns actor from definition |
| [SProjectObjectBrowser](../../Plugins/Editor/ProjectPlacementEditor/Source/ProjectPlacementEditor/Private/Widgets/SProjectObjectBrowser.h) | ProjectPlacementEditor | UI widget for Objects section |

### Capability IDs

Components register via `GetPrimaryAssetId()` returning `FPrimaryAssetId("CapabilityComponent", "<ID>")`:

| ID | Component |
|----|-----------|
| `Lockable` | [ULockableComponent](../../Plugins/Gameplay/ProjectObjectCapabilities/Source/ProjectObjectCapabilities/Public/Access/LockableComponent.h) |
| `Hinged` | [USpringRotatorComponent](../../Plugins/Systems/ProjectMotionSystem/Source/ProjectMotionSystem/Public/Components/SpringRotatorComponent.h) |
| `Sliding` | [USpringSliderComponent](../../Plugins/Systems/ProjectMotionSystem/Source/ProjectMotionSystem/Public/Components/SpringSliderComponent.h) |

### Object JSON Schema

```json
{
  "$schema": "../../Schemas/object.schema.json",
  "id": "WoodenDoor",

  "meshes": [
    { "id": "frame", "asset": "/ProjectObject/Doors/SM_Door_Frame" },
    { "id": "panel", "asset": "/ProjectObject/Doors/SM_Door_Panel" }
  ],

  "capabilities": [
    {
      "type": "Lockable",
      "scope": ["actor"],
      "properties": {
        "LockTag": "Key.RoomA",
        "bConsumeKeyOnUnlock": "false"
      }
    },
    {
      "type": "Hinged",
      "scope": ["panel"],
      "properties": {
        "OpenAngle": "90",
        "Stiffness": "100",
        "Damping": "20"
      }
    }
  ]
}
```

**Contracts:**
- Scope: always array (`["actor"]`, `["panel"]`, `["left", "right"]`)
- Properties: nested under `properties` key, string values parsed via `FProperty::ImportText()`
- Asset paths: generator auto-appends `.<AssetName>` if missing

---

## Phase 3: Enhancements (Optional)

### 3.1 Thumbnails / Icons (COMPLETED)

- [x] Auto-generate icons from first mesh for editor placement
- [x] Cache in editor-only location (static cache, not shipped)

**Implementation:** `UDefinitionThumbnailRenderer` base class delegates to engine mesh thumbnail.
- `UItemDefinitionThumbnailRenderer` - uses `WorldSkeletalMesh` or `WorldStaticMesh`
- `UObjectDefinitionThumbnailRenderer` - uses `Meshes[0].Asset` (JSON author controls order)

### 3.2 Validation

- [ ] Validate mesh IDs referenced in capabilities exist in meshes array
- [ ] Validate capability types exist in CapabilityRegistry
- [ ] Editor warnings for missing references

### 3.3 JSON Asset Ref Auto-Fix

**Problem:** Asset rename/move breaks JSON refs (no UUID system).

**Solution:** Editor module tracks JSON refs and rewrites on asset changes.

- [ ] Build index: asset path -> list of (JSON file, JSON key) references
- [ ] Hook `FAssetRegistryModule::AssetRenamed` delegate
- [ ] On rename/move: rewrite all JSON files that reference old path
- [ ] Commandlet for CI validation: `ValidateJsonAssetRefs`

### 3.4 Generator Integration

- [ ] Support `"GeneratedContentPath": "$SOURCE_DIR"` to output DA_*.uasset next to source JSON
- [ ] Orphan cleanup via `SourceJsonPath` - if JSON deleted, remove orphaned asset
- [ ] Add `x-alis-generator` block to JSON schema

---

## References

- [Flexible Path Pattern](../../docs/architecture/flexible_path.md) - ID-based asset resolution
- [ProjectPlacementEditor README](../../Plugins/Editor/ProjectPlacementEditor/README.md) - Picker implementation
- [ProjectObject README](../../Plugins/Resources/ProjectObject/README.md) - Actor composition pattern
- [ProjectDefinitionGenerator](../../Plugins/Editor/ProjectDefinitionGenerator/) - Universal JSON-to-DataAsset generator
