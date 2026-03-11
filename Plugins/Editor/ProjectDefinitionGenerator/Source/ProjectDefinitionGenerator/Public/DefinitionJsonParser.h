// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DefinitionTypeInfo.h"

class FJsonObject;
class FJsonValue;

/**
 * JSON parser for definition assets.
 * Handles JSON -> UObject property mapping via reflection.
 *
 * ARCHITECTURE VISION:
 * - JSON is Source of Truth, USTRUCT mirrors JSON structure
 * - Maximum automation via FJsonObjectConverter::JsonObjectToUStruct
 * - Custom callback handles edge cases (array vectors, soft pointer normalization)
 * - FieldMappings is LEGACY - new types should use pure auto-parse
 *
 * PARSING MODES:
 * 1. AUTO-PARSE (new): FieldMappings empty -> JsonObjectToUStruct on entire asset
 * 2. EXPLICIT (legacy): FieldMappings defined -> field-by-field mapping
 *
 * CALLBACK HANDLES:
 * - Array-format vectors: JSON [X, Y, Z] -> FVector
 * - Array-format rotators: JSON [P, Y, R] -> FRotator
 * - TSoftObjectPtr: Normalizes path "/Game/Foo" -> "/Game/Foo.Foo"
 * - TSoftClassPtr: Normalizes path "/Game/Foo" -> "/Game/Foo.Foo_C"
 *
 * SOLID: Single responsibility - only JSON parsing logic.
 */
class PROJECTDEFINITIONGENERATOR_API FDefinitionJsonParser
{
public:
	/**
	 * Parse JSON and populate asset using reflection + field mappings.
	 * @param TypeInfo Type configuration with field mappings
	 * @param JsonObject Parsed JSON data
	 * @param Asset Target asset to populate
	 * @param OutError Error message if failed
	 * @return True if parsing succeeded
	 */
	static bool ParseJsonToAsset(
		const FDefinitionTypeInfo& TypeInfo,
		const TSharedPtr<FJsonObject>& JsonObject,
		UObject* Asset,
		FString& OutError);

	/**
	 * Set a property value from JSON using field type.
	 * @param Asset Target asset
	 * @param PropertyName Property name (supports nested paths like "Data.DisplayName")
	 * @param JsonValue JSON value to set
	 * @param FieldType Expected field type
	 * @param OutError Error message if failed
	 * @return True if property was set
	 */
	static bool SetPropertyFromJson(
		UObject* Asset,
		const FString& PropertyName,
		const TSharedPtr<FJsonValue>& JsonValue,
		EDefinitionFieldType FieldType,
		FString& OutError);

	/**
	 * Get JSON value at dot-separated path (e.g., "world.staticMesh").
	 */
	static TSharedPtr<FJsonValue> GetJsonValueAtPath(
		const TSharedPtr<FJsonObject>& JsonObject,
		const FString& Path);

	/**
	 * Normalize asset path for TSoftObjectPtr (adds .AssetName if missing).
	 */
	static FString NormalizeAssetPath(const FString& Path);

	/**
	 * Normalize class path for TSoftClassPtr (adds .ClassName_C if missing).
	 */
	static FString NormalizeClassPath(const FString& Path);

	/**
	 * Convert field type string to enum.
	 */
	static EDefinitionFieldType StringToFieldType(const FString& TypeString);
};
