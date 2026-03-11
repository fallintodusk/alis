// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DefinitionTypeInfo.generated.h"

// -------------------------------------------------------------------------
// PARSING ARCHITECTURE VISION
// -------------------------------------------------------------------------
//
// GOAL: Maximum automation with FJsonObjectConverter::JsonObjectToUStruct
//
// PRINCIPLE: JSON is Source of Truth, USTRUCT mirrors JSON structure
// - Field names in USTRUCT match JSON keys (case-insensitive)
// - Nested structs match nested JSON objects
// - Auto-parse handles everything, callback handles edge cases
//
// EDGE CASES (handled by CustomJsonImportCallback):
// - Array-format vectors: JSON [X, Y, Z] -> FVector
// - Array-format rotators: JSON [P, Y, R] -> FRotator
// - Soft object paths: Normalizes "/Game/Foo" -> "/Game/Foo.Foo"
// - Soft class paths: Normalizes "/Game/Foo" -> "/Game/Foo.Foo_C"
//
// TRANSITION: FieldMappings is LEGACY infrastructure
// - If FieldMappings is EMPTY -> use pure auto-parse (new approach)
// - If FieldMappings exists -> use explicit mapping (backward compat)
// - Goal: migrate all definition types to empty FieldMappings
//
// EXAMPLE JSON -> USTRUCT:
//   JSON: { "id": "Door", "meshes": [...], "capabilities": [...] }
//   USTRUCT: FName Id; TArray<FObjectMeshEntry> Meshes; TArray<FObjectCapabilityEntry> Capabilities;
//
// -------------------------------------------------------------------------

/**
 * Field type enum for JSON-to-Property parsing.
 *
 * LEGACY: This enum exists for backward compatibility with explicit FieldMappings.
 * NEW APPROACH: Use auto-parse with USTRUCT field names matching JSON keys.
 *
 * The universal generator handles ALL parsing; plugins just provide data mappings.
 */
UENUM(BlueprintType)
enum class EDefinitionFieldType : uint8
{
	/** Auto-detect from UProperty reflection (string, int, float, bool) */
	Auto,

	/** FString from JSON string */
	String,

	/** FName from JSON string */
	Name,

	/** FText from JSON string (localized text) */
	Text,

	/** int32 from JSON number */
	Int,

	/** float from JSON number */
	Float,

	/** bool from JSON bool */
	Bool,

	/** FVector from JSON object {x, y, z} */
	Vector,

	/** FRotator from JSON object {pitch, yaw, roll} */
	Rotator,

	/** FIntPoint from JSON object {x, y} */
	IntPoint,

	/** TSoftObjectPtr from JSON string (asset path, normalized) */
	SoftObject,

	/** TSoftClassPtr from JSON string (class path, adds _C suffix) */
	SoftClass,

	/** FGameplayTag from JSON string */
	GameplayTag,

	/** FGameplayTagContainer from JSON array of strings */
	GameplayTagContainer,

	/** TArray<TSoftObjectPtr> from JSON array of strings */
	SoftObjectArray,

	/** TArray<TSoftClassPtr> from JSON array of strings */
	SoftClassArray,

	/** TMap<FGameplayTag, float> from JSON object { "Tag.Name": value } */
	GameplayTagFloatMap,

	/** TArray<FObjectMeshEntry> from JSON array [{id, asset, transform?}] */
	MeshEntryArray,

	/** TArray<FObjectCapabilityEntry> from JSON array [{type, scope, properties?}] */
	CapabilityEntryArray
};

/**
 * Field mapping from JSON key to UProperty.
 * Supports nested paths like "world.staticMesh" -> "WorldStaticMesh"
 */
USTRUCT(BlueprintType)
struct FDefinitionFieldMapping
{
	GENERATED_BODY()

	/** JSON key path (e.g., "displayName", "world.staticMesh") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString JsonPath;

	/** UProperty name on the definition class (supports nested: "Data.DisplayName") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString PropertyName;

	/** Field type for parsing (Auto uses reflection) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EDefinitionFieldType FieldType = EDefinitionFieldType::Auto;

	/** If true, this field is required */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bRequired = false;

	FDefinitionFieldMapping() = default;

	FDefinitionFieldMapping(const FString& InJsonPath, const FString& InPropertyName, EDefinitionFieldType InFieldType = EDefinitionFieldType::Auto, bool bInRequired = false)
		: JsonPath(InJsonPath), PropertyName(InPropertyName), FieldType(InFieldType), bRequired(bInRequired)
	{}
};

/**
 * Registration info for a definition type.
 *
 * Data-driven: plugins provide a manifest JSON file (Content/Data/Generator/<Type>.type.json)
 * which is auto-discovered and parsed into this struct.
 *
 * Generator handles all logic; plugins only provide data mappings.
 */
USTRUCT(BlueprintType)
struct PROJECTDEFINITIONGENERATOR_API FDefinitionTypeInfo
{
	GENERATED_BODY()

	/** The UClass of the definition asset (e.g., UObjectDefinition::StaticClass()) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UClass> DefinitionClass = nullptr;

	/** Plugin name for FProjectPaths::GetPluginDataDir() */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString PluginName;

	/** Subdirectory within plugin's Data folder (empty = root of Data folder) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SourceSubDir;

	/** File glob pattern for source files (empty = "*.json", e.g. "dlg_*.json") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SourceFileGlob;

	/** Content path for generated assets (e.g., "/ProjectObject/Objects") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString GeneratedContentPath;

	/** If true, all generated assets go flat into GeneratedContentPath (no source directory hierarchy).
	 *  Useful when source files are co-located in another type's directory tree. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bFlattenGeneratedPath = false;

	/** Property name for the ID field (default: first FName property or "Id") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString IdPropertyName = TEXT("Id");

	/** Property name for SourceJsonHash (for incremental generation) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString HashPropertyName = TEXT("SourceJsonHash");

	/** Property name for GeneratorVersion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString VersionPropertyName = TEXT("GeneratorVersion");

	/** Property name for bGenerated flag */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString GeneratedFlagPropertyName = TEXT("bGenerated");

	/** Property name for SourceJsonPath */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SourcePathPropertyName = TEXT("SourceJsonPath");

	/** Generator version - increment when schema changes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 GeneratorVersion = 1;

	/**
	 * Field mappings from JSON to UProperty.
	 *
	 * LEGACY: If empty, uses pure auto-parse (USTRUCT field names match JSON keys).
	 * NEW APPROACH: Leave empty, ensure USTRUCT mirrors JSON structure.
	 *
	 * Migration: Remove FieldMappings from schema, rename USTRUCT fields to match JSON.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FDefinitionFieldMapping> FieldMappings;

	/**
	 * Custom parser delegate for complex types.
	 * Called after automatic field mapping.
	 * Signature: bool(const TSharedPtr<FJsonObject>& Json, UObject* Asset, FString& OutError)
	 */
	TFunction<bool(const TSharedPtr<FJsonObject>&, UObject*, FString&)> CustomParser;

	bool IsValid() const
	{
		return DefinitionClass != nullptr && !PluginName.IsEmpty() && !GeneratedContentPath.IsEmpty();
	}
};
