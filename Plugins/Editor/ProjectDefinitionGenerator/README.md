# ProjectDefinitionGenerator

Universal JSON-to-DataAsset generator for definition types (Items, Objects, etc.).

Editor-only plugin for turning text-based definitions into generated Unreal assets.

## Overview

This editor plugin provides a **data-driven** approach to generating UDataAsset definitions from JSON source files. Resource plugins only provide:
- Runtime definition class (e.g., `UObjectDefinition`)
- Source JSON data files
- JSON Schema with `x-alis-generator` extension block

The generator handles all logic: parsing, field mapping, incremental generation, orphan cleanup.

## Data Pipeline Role

**This plugin handles GENERATION** - one-way transformation with validation (JSON -> DataAsset).

**NOT in scope:**
- **SYNC** (mechanical updates like asset path fixes) -> See [ProjectAssetSync](../ProjectAssetSync/README.md)
- **PROPAGATION** (cascading actor updates) -> See [ProjectPlacementEditor](../ProjectPlacementEditor/README.md)

**How it fits in the pipeline:**
1. **SYNC**: Asset renamed -> JSON paths updated -> Redirectors fixed
2. **GENERATION** (this plugin): JSON changed -> DataAsset regenerated -> Broadcasts `OnDefinitionRegenerated`
3. **PROPAGATION**: Definition changed -> Scene actors updated

**See:** [Data Pipeline Architecture](../../../docs/data/README.md#data-pipeline-architecture) for complete flow documentation.

## Architecture

```
+---------------------------+
|  Resource Plugin          |
|  (ProjectObject, etc.)    |
|                           |
|  Data/                    |
|    Schemas/               |
|      object.schema.json   |  <- x-alis-generator block defines type
|    Objects/               |
|      Template/            |
|        door.json          |  <- SOT (Source of Truth)
|                           |
|  Content/Objects/         |
|    Template/              |
|      door.uasset          |  <- Generated DataAsset
+---------------------------+
         |
         v
+---------------------------+
| ProjectDefinitionGenerator|
|                           |
| DiscoverAndRegisterManifests()
|   -> Scans *.schema.json  |
|   -> Parses x-alis-generator
|   -> Registers type       |
|                           |
| GenerateAllForType()      |
|   -> Finds JSON files     |
|   -> Hash comparison      |
|   -> Creates/updates assets
|   -> Preserves folder hierarchy
+---------------------------+
```

## Usage

### Schema-Based Registration (Recommended)

Add `x-alis-generator` block to your JSON schema:

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "title": "Object Definition",

  "x-alis-generator": {
    "TypeName": "Object",
    "DefinitionClass": "/Script/ProjectObject.ObjectDefinition",
    "SourceSubDir": "",
    "GeneratedContentPath": "/ProjectObject/Objects",
    "IdPropertyName": "ObjectId",
    "GeneratorVersion": 1,

    "FieldMappings": [
      { "JsonPath": "displayName", "PropertyName": "Data.DisplayName", "FieldType": "Text", "bRequired": true },
      { "JsonPath": "world.staticMesh", "PropertyName": "WorldStaticMesh", "FieldType": "SoftObject" }
    ]
  },

  "type": "object",
  "properties": { ... }
}
```

The generator auto-discovers schemas from `Plugins/**/Data/Schemas/*.schema.json`.

### Manual Registration (C++)

```cpp
FDefinitionTypeInfo TypeInfo;
TypeInfo.DefinitionClass = UObjectDefinition::StaticClass();
TypeInfo.PluginName = TEXT("ProjectObject");
TypeInfo.SourceSubDir = TEXT("");
TypeInfo.GeneratedContentPath = TEXT("/ProjectObject/Objects");
TypeInfo.IdPropertyName = TEXT("ObjectId");
TypeInfo.GeneratorVersion = 1;

TypeInfo.FieldMappings = {
    { TEXT("displayName"), TEXT("Data.DisplayName"), EDefinitionFieldType::Text, true },
    { TEXT("world.staticMesh"), TEXT("WorldStaticMesh"), EDefinitionFieldType::SoftObject }
};

UDefinitionGeneratorSubsystem::Get()->RegisterDefinitionType(TEXT("Object"), TypeInfo);
```

### Triggering Generation

**Editor Menu:** Tools -> Project Definition Generator -> Generate All

**Commandlet:**
```bash
UnrealEditor-Cmd.exe Alis.uproject -run=GenerateDefinitions -nopause
```

**C++ API:**
```cpp
auto* Generator = UDefinitionGeneratorSubsystem::Get();
Generator->DiscoverAndRegisterManifests();
Generator->GenerateAll(bForceRegenerate);
```

## Field Types

| FieldType | JSON Value | UProperty Type |
|-----------|------------|----------------|
| `Auto` | any | Auto-detected from reflection |
| `String` | string | FString |
| `Name` | string | FName |
| `Text` | string | FText (localized) |
| `Int` | number | int32 |
| `Float` | number | float/double |
| `Bool` | boolean | bool |
| `Vector` | `{x,y,z}` | FVector |
| `Rotator` | `{pitch,yaw,roll}` | FRotator |
| `IntPoint` | `{x,y}` | FIntPoint |
| `SoftObject` | string (path) | TSoftObjectPtr |
| `SoftClass` | string (path) | TSoftClassPtr (adds _C suffix) |
| `GameplayTag` | string | FGameplayTag |
| `GameplayTagContainer` | array of strings | FGameplayTagContainer |
| `SoftObjectArray` | array of strings | TArray<TSoftObjectPtr> |
| `SoftClassArray` | array of strings | TArray<TSoftClassPtr> |
| `GameplayTagFloatMap` | `{"Tag.Name": value}` | TMap<FGameplayTag, float> |

## Features

### Incremental Generation

Assets are only regenerated when:
- JSON file hash changes
- Generator version increases
- Force regenerate flag is set

Each generated asset stores:
- `SourceJsonHash` - MD5 of normalized JSON
- `GeneratorVersion` - Version from schema
- `SourceJsonPath` - Relative path for orphan tracking
- `bGenerated` - Flag indicating auto-generation

### Folder Hierarchy Preservation

Source JSON folder structure is mirrored to generated Content:

```
Data/Objects/Template/door.json
    -> /ProjectObject/Objects/Template/door.uasset
```

### Orphan Cleanup

When JSON files are moved/renamed/deleted:
```cpp
Generator->CleanupOrphanedAssets(TEXT("Item"));
```

Compares `SourceJsonPath` metadata against existing JSON files.

### Nested Property Paths

Both JSON paths and property names support dot notation:

```json
{ "JsonPath": "world.staticMesh", "PropertyName": "Data.WorldVisual.StaticMesh" }
```

## x-alis-generator Block Reference

| Field | Required | Default | Description |
|-------|----------|---------|-------------|
| `TypeName` | Yes | - | Unique type name (e.g., "Item") |
| `DefinitionClass` | Yes | - | Full class path |
| `GeneratedContentPath` | Yes | - | Content path for generated assets |
| `PluginName` | No | Inferred | Plugin name for FProjectPaths |
| `SourceSubDir` | No | "" | Subdirectory within Data/ |
| `IdPropertyName` | No | "Id" | Property name for asset ID |
| `GeneratorVersion` | No | 1 | Increment on schema changes |
| `HashPropertyName` | No | "SourceJsonHash" | Property for hash storage |
| `VersionPropertyName` | No | "GeneratorVersion" | Property for version storage |
| `GeneratedFlagPropertyName` | No | "bGenerated" | Property for generated flag |
| `SourcePathPropertyName` | No | "SourceJsonPath" | Property for source path |
| `FieldMappings` | No | [] | Array of field mapping objects |

### FieldMapping Object

| Field | Required | Default | Description |
|-------|----------|---------|-------------|
| `JsonPath` | Yes | - | JSON key path (e.g., "world.staticMesh") |
| `PropertyName` | Yes | - | UProperty name (e.g., "Data.WorldMesh") |
| `FieldType` | No | "Auto" | Type hint for parsing |
| `bRequired` | No | false | Fail if field missing |

## Definition Class Requirements

Your definition class should have these metadata properties:

```cpp
UCLASS(BlueprintType)
class UObjectDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ID field (name configurable via IdPropertyName)
    UPROPERTY(EditAnywhere, AssetRegistrySearchable)
    FName ObjectId;

    // Generator metadata (names configurable)
    UPROPERTY(VisibleAnywhere, Category = "Generator")
    bool bGenerated = false;

    UPROPERTY(VisibleAnywhere, Category = "Generator")
    int32 GeneratorVersion = 0;

    UPROPERTY(VisibleAnywhere, Category = "Generator")
    FString SourceJsonPath;

    UPROPERTY(VisibleAnywhere, Category = "Generator")
    FString SourceJsonHash;

    // Your data fields...
};
```

## Adding a New Definition Type

1. Create definition class in your resource plugin
2. Add generator metadata properties (bGenerated, SourceJsonHash, etc.)
3. Create JSON schema in `Data/Schemas/<type>.schema.json`
4. Add `x-alis-generator` block with field mappings
5. Create source JSON files in `Data/<Type>/`
6. Run generator (menu or commandlet)

## Extending Field Types

If your type needs complex parsing not covered by `EDefinitionFieldType`:

1. Add new enum value to `EDefinitionFieldType` in [DefinitionTypeInfo.h](Source/ProjectDefinitionGenerator/Public/DefinitionTypeInfo.h)
2. Add parsing logic to `SetPropertyFromJson()` in [DefinitionGeneratorSubsystem.cpp](Source/ProjectDefinitionGenerator/Private/DefinitionGeneratorSubsystem.cpp)
3. Update `StringToFieldType()` for schema parsing

Or use `CustomParser` delegate for one-off complex types.

## File Structure

```
ProjectDefinitionGenerator/
  Source/ProjectDefinitionGenerator/
    Public/
      DefinitionTypeInfo.h           <- FDefinitionTypeInfo, EDefinitionFieldType
      DefinitionGeneratorSubsystem.h <- Main generator subsystem
      GenerateDefinitionsCommandlet.h
    Private/
      DefinitionGeneratorSubsystem.cpp
      DefinitionGeneratorMenuExtension.cpp  <- Editor menu integration
      GenerateDefinitionsCommandlet.cpp
  ProjectDefinitionGenerator.uplugin
  README.md
```

## Dependencies

- ProjectCore (for FProjectPaths)
- ProjectObject (for UObjectDefinition)

## See Also

- [../../../docs/data/README.md](../../../docs/data/README.md) - Public data architecture overview
- [../ProjectPlacementEditor/README.md](../ProjectPlacementEditor/README.md) - Propagation stage after generation
- [../../Resources/ProjectObject/README.md](../../Resources/ProjectObject/README.md) - Example consumer of generated object definitions

Note:
- concrete schema JSON files live under plugin `Data/` folders in the full project tree
- those generated or source data payloads are not part of the public mirror snapshot
