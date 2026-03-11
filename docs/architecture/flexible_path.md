# Flexible Path Pattern

Prevent path dependency issues by decoupling asset/class identity from filesystem paths.

**Related docs:**
- [Plugin Architecture](plugin_architecture.md)
- [Data-Driven Design](data_driven.md)

**Implementations:**
- [UObjectDefinition](../../Plugins/Resources/ProjectObject/Source/ProjectObject/Public/Data/ObjectDefinition.h) - Objects and items use this pattern
- [FCapabilityRegistry](../../Plugins/Editor/ProjectPlacementEditor/Source/ProjectPlacementEditor/Public/CapabilityRegistry.h) - CDO scan registry for capabilities

---

## The Problem

Unreal's default asset referencing uses filesystem paths:
```
/Game/Items/Consumables/Apple.Apple
```

**Path dependencies cause issues:**
- Rename folder -> all references break
- Move plugin -> all references break
- Refactor structure -> hours of redirect fixes
- JSON with hardcoded paths -> manual updates

---

## The Solution: Stable IDs via GetPrimaryAssetId()

UE provides `UObject::GetPrimaryAssetId()` - a virtual method that returns a stable identifier.

```cpp
// Engine/Source/Runtime/CoreUObject/Public/UObject/Object.h
virtual FPrimaryAssetId GetPrimaryAssetId() const;
```

`FPrimaryAssetId` has two parts:
- **Type**: Category (e.g., "ObjectDefinition", "CapabilityComponent")
- **Name**: Unique ID within type (e.g., "Apple", "Lockable")

**Important caveat:** IDs are stable as identifiers, but **AssetManager discovery is still path-based**. You must configure `PrimaryAssetTypesToScan` directories in DefaultGame.ini for the AssetManager to find your assets.

---

## Pattern A: DataAssets (Items, Objects)

For **data** that needs JSON as source of truth (replication, inventory, etc.).

**Flow:**
```
object.json -> Generator -> UObjectDefinition DataAsset -> AssetManager -> Runtime lookup by ID
```

**Implementation:**
```cpp
// UObjectDefinition.h
UCLASS()
class UObjectDefinition : public UPrimaryDataAsset
{
    UPROPERTY()
    FName ObjectId;  // Stable ID from JSON, e.g., "Apple"

    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId(FPrimaryAssetType("ObjectDefinition"), ObjectId);
    }
};
```

**JSON source:**
```json
{
  "id": "Apple",
  "displayName": "Apple",
  "icon": "/Game/Icons/T_Apple"
}
```

**Note:** Generator normalizes soft paths by appending `.<AssetName>` when missing (e.g., `/Game/Icons/T_Apple` -> `/Game/Icons/T_Apple.T_Apple`).

**Runtime lookup:**
```cpp
FPrimaryAssetId AssetId("ObjectDefinition", "Apple");

// Option 1: If already loaded (editor, cached)
UObjectDefinition* Def = AssetManager->GetPrimaryAssetObject<UObjectDefinition>(AssetId);

// Option 2: Async load (runtime, recommended)
FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
FSoftObjectPath AssetPath = AssetManager->GetPrimaryAssetPath(AssetId);
StreamableManager.RequestAsyncLoad(AssetPath, FStreamableDelegate::CreateLambda([](){ /* loaded */ }));
```

**Path safety:**
- Asset can be at `/Game/Objects/Apple` or `/Game/Food/Fruits/Apple`
- ID stays "Apple", references don't break
- No CoreRedirects needed for path changes; ID changes require aliases/migration
- JSON only changes if ID changes (rare)

---

## Pattern B: C++ Classes (Capabilities)

For **code** that already exists as C++ classes (components, actors).

**Flow:**
```
C++ Component with GetPrimaryAssetId() -> CDO scan at startup -> Registry -> Runtime lookup by ID
```

**Implementation:**
```cpp
// ULockableComponent.h
UCLASS()
class ULockableComponent : public UActorComponent
{
    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId(FPrimaryAssetType("CapabilityComponent"), FName("Lockable"));
    }
};
```

**Registry building (editor/runtime startup):**
```cpp
TMap<FName, UClass*> CapabilityRegistry;

void BuildCapabilityRegistry()
{
    for (TObjectIterator<UClass> It; It; ++It)
    {
        if (It->IsChildOf(UActorComponent::StaticClass()))
        {
            UActorComponent* CDO = It->GetDefaultObject<UActorComponent>();
            FPrimaryAssetId AssetId = CDO->GetPrimaryAssetId();

            if (AssetId.IsValid() && AssetId.PrimaryAssetType == "CapabilityComponent")
            {
                CapabilityRegistry.Add(AssetId.PrimaryAssetName, *It);
            }
        }
    }
}
```

**JSON reference:**
```json
{
  "capabilities": [
    { "type": "Lockable", "scope": ["actor"], "lockTag": "Key.RoomA" }
  ]
}
```

**Path safety:**
- Component can move from `ProjectObjectCapabilities` to `ProjectAccessControl`
- ID stays "Lockable", JSON doesn't change
- No CoreRedirects needed for path changes; ID changes require aliases/migration
- Only requires rebuild (not content fix)

---

## Comparison

| Aspect | Pattern A (DataAssets) | Pattern B (C++ Classes) |
|--------|------------------------|------------------------|
| Use case | Data (ObjectDefinitions) | Code (Components) |
| ID source | JSON field | C++ GetPrimaryAssetId() |
| Registration | AssetManager scans assets | CDO scan at startup |
| Add new | Create JSON file | Create C++ class |
| Path rename | ID stable, no JSON edit | ID stable, no JSON edit |
| Requires rebuild | No (hot reload) | Yes (code change) |

---

## Engine Reference

**Key UE classes (relative to Engine/Source/Runtime/):**
- `UObject::GetPrimaryAssetId()` - Base virtual method
  - `CoreUObject/Public/UObject/Object.h:1030`
- `UPrimaryDataAsset` - DataAsset with built-in GetPrimaryAssetId
  - `Engine/Classes/Engine/DataAsset.h`
- `UAssetManager` - Singleton for primary asset management
  - `Engine/Classes/Engine/AssetManager.h`
- `FPrimaryAssetId` - Type + Name pair for stable identification
  - `CoreUObject/Public/UObject/PrimaryAssetId.h`

**UPrimaryDataAsset::GetPrimaryAssetId() implementation:**
```cpp
// Engine/Private/DataAsset.cpp:54
FPrimaryAssetId UPrimaryDataAsset::GetPrimaryAssetId() const
{
    // Data assets use Class and ShortName by default
    return FPrimaryAssetId(GetClass()->GetFName(), GetFName());
}
```

Override to use custom ID instead of asset name.

---

## Best Practices

1. **Always override GetPrimaryAssetId()** for assets/classes that need stable references
2. **Use FName IDs** that are meaningful and unlikely to change (e.g., "Apple", not "Item_001")
3. **Store ID in JSON** for data-driven assets (generator reads it)
4. **Store ID in C++** for code-based classes (static or method)
5. **Build registries at startup** for fast runtime lookup
6. **Reference definitions by ID** - ObjectDefinitions and Capabilities use stable IDs, not paths
7. **Art assets may use soft paths** - meshes, textures, sounds still use FSoftObjectPath
   - Protected by editor auto-fix on rename/move (see Phase 3.4 in TODO)
   - Validated by CI commandlet

---

## Alis-Specific Implementations

### Objects (Pattern A)
- **Class:** [UObjectDefinition](../../Plugins/Resources/ProjectObject/Source/ProjectObject/Public/Data/ObjectDefinition.h)
- **JSON:** Co-located with art assets
- **ID field:** `ObjectId`
- **Type:** `"ObjectDefinition"`
- **Note:** Item data lives in `ObjectDefinition.Item` (inventory, GAS)
- **Factory:** [UProjectObjectActorFactory](../../Plugins/Editor/ProjectPlacementEditor/Source/ProjectPlacementEditor/Public/ProjectObjectActorFactory.h)

### Capabilities (Pattern B)
- **Classes:** [ULockableComponent](../../Plugins/Gameplay/ProjectObjectCapabilities/Source/ProjectObjectCapabilities/Public/Access/LockableComponent.h), [USpringRotatorComponent](../../Plugins/Systems/ProjectMotionSystem/Source/ProjectMotionSystem/Public/Components/SpringRotatorComponent.h), [USpringSliderComponent](../../Plugins/Systems/ProjectMotionSystem/Source/ProjectMotionSystem/Public/Components/SpringSliderComponent.h)
- **No JSON:** ID in C++ code via `GetPrimaryAssetId()`
- **Type:** `"CapabilityComponent"`
- **Registry:** [FCapabilityRegistry](../../Plugins/Editor/ProjectPlacementEditor/Source/ProjectPlacementEditor/Public/CapabilityRegistry.h) - built lazily via CDO scan

---

## Migration Guide

### Converting path-based references to ID-based:

1. Add `GetPrimaryAssetId()` override to class
2. Choose stable ID (won't change)
3. Update consumers to lookup by ID instead of path
4. For JSON: replace path strings with ID strings
5. Build registry/cache for runtime lookup

### Renaming an ID (rare):

**For DataAssets:**
1. Update JSON `id` field
2. Regenerate asset (generator updates `ObjectId`)
3. Update consuming code/JSON
4. Optional: Add old ID to `Aliases` array for graceful migration

**For C++ classes:**
1. Update `GetPrimaryAssetId()` return value
2. Rebuild
3. Update consuming JSON
4. Optional: Check for old ID in registry lookup for migration period
