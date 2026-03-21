// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "AssetRegistry/AssetData.h"
#include "UObject/SoftObjectPath.h"
#include "DefinitionTypeInfo.h"
#include "DefinitionGeneratorSubsystem.generated.h"

class UObjectDefinition;
class UProjectAbilitySet;

// NOTE: FOnDefinitionRegenerated delegate is in ProjectEditorCore/DefinitionEvents.h
// This module broadcasts to FDefinitionEvents::OnDefinitionRegenerated()
// Subscribers (e.g., ProjectPlacementEditor) subscribe there - no direct dependency

/**
 * Result of generating a single definition asset.
 */
USTRUCT(BlueprintType)
struct FDefinitionGenerationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString TypeName;

	UPROPERTY(BlueprintReadOnly)
	FString AssetId;

	UPROPERTY(BlueprintReadOnly)
	FString SourcePath;

	UPROPERTY(BlueprintReadOnly)
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly)
	FString ErrorMessage;

	UPROPERTY(BlueprintReadOnly)
	bool bSkipped = false;
};

/**
 * Universal definition generator subsystem.
 *
 * Data-driven architecture: plugins provide only:
 * - Runtime definition class (e.g., UObjectDefinition)
 * - Source JSON data files
 * - JSON Schema with x-alis-generator extension: Data/Schemas/<type>.schema.json
 *
 * This subsystem auto-discovers schemas and handles all generation:
 * - JSON parsing via reflection + field mappings from x-alis-generator block
 * - Hash-based incremental generation
 * - Path hierarchy preservation (Content mirrors Data structure)
 * - Orphan cleanup
 * - Asset save/load with redirectors
 *
 * Schemas are discovered automatically from enabled plugins.
 * No C++ registration code needed in resource plugins.
 */
UCLASS()
class PROJECTDEFINITIONGENERATOR_API UDefinitionGeneratorSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	/** Get the singleton instance */
	static UDefinitionGeneratorSubsystem* Get();

	//~ Begin USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	/**
	 * Register a definition type for generation.
	 * @param TypeName Unique name for this type (e.g., "Item", "Object")
	 * @param Info Configuration for this definition type
	 */
	UFUNCTION(BlueprintCallable, Category = "DefinitionGenerator")
	void RegisterDefinitionType(const FString& TypeName, const FDefinitionTypeInfo& Info);

	/**
	 * Unregister a definition type.
	 */
	UFUNCTION(BlueprintCallable, Category = "DefinitionGenerator")
	void UnregisterDefinitionType(const FString& TypeName);

	/**
	 * Get registered type info.
	 */
	UFUNCTION(BlueprintPure, Category = "DefinitionGenerator")
	bool GetDefinitionTypeInfo(const FString& TypeName, FDefinitionTypeInfo& OutInfo) const;

	/**
	 * Get all registered type names.
	 */
	UFUNCTION(BlueprintPure, Category = "DefinitionGenerator")
	TArray<FString> GetRegisteredTypeNames() const;

	/**
	 * Generate all assets for a specific type.
	 * @param TypeName The registered type name
	 * @param bForceRegenerate If true, regenerate all even if hash matches
	 * @return Array of generation results
	 */
	UFUNCTION(BlueprintCallable, Category = "DefinitionGenerator")
	TArray<FDefinitionGenerationResult> GenerateAllForType(const FString& TypeName, bool bForceRegenerate = false);

	/**
	 * Generate all assets for all registered types.
	 */
	UFUNCTION(BlueprintCallable, Category = "DefinitionGenerator")
	TArray<FDefinitionGenerationResult> GenerateAll(bool bForceRegenerate = false);

	/**
	 * Generate a single asset from JSON file.
	 */
	UFUNCTION(BlueprintCallable, Category = "DefinitionGenerator")
	FDefinitionGenerationResult GenerateFromJson(const FString& TypeName, const FString& JsonFilePath, bool bForceRegenerate = false);

	/**
	 * Delete orphaned generated assets for a type.
	 */
	UFUNCTION(BlueprintCallable, Category = "DefinitionGenerator")
	int32 CleanupOrphanedAssets(const FString& TypeName);

	/**
	 * Find existing asset by ID for a type.
	 * @param TypeName The registered type name
	 * @param AssetId The asset ID to find
	 * @return The existing asset or nullptr if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "DefinitionGenerator")
	UObject* FindExistingAssetForType(const FString& TypeName, const FString& AssetId);

	/**
	 * Compute MD5 hash of file contents (normalized line endings).
	 */
	UFUNCTION(BlueprintPure, Category = "DefinitionGenerator")
	static FString ComputeFileHash(const FString& FilePath);

	/**
	 * Resolve the generated asset id from JSON using the type's configured id field
	 * and compatibility fallbacks.
	 */
	static bool ResolveAssetIdFromJson(
		const FDefinitionTypeInfo& TypeInfo,
		const TSharedPtr<class FJsonObject>& JsonObject,
		FString& OutAssetId,
		FString& OutError);

	/**
	 * Discover and register all definition types from plugin schemas.
	 * Scans enabled plugins for Data/Schemas/*.schema.json files with x-alis-generator block.
	 * Called automatically on Initialize, but can be called manually to refresh.
	 */
	UFUNCTION(BlueprintCallable, Category = "DefinitionGenerator")
	void DiscoverAndRegisterManifests();

	/**
	 * Get the source directory path for a registered type.
	 * @param TypeName The registered type name
	 * @return Full disk path to the source JSON directory
	 */
	UFUNCTION(BlueprintPure, Category = "DefinitionGenerator")
	FString GetSourceDirectoryForType(const FString& TypeName) const;

	/**
	 * Get file glob patterns claimed by OTHER types sharing the same source directory.
	 * Used to exclude files belonging to sibling types (e.g., Object skips dlg_*.json).
	 */
	TArray<FString> GetExclusionGlobsForType(const FString& TypeName) const;

	/** Check if filename matches any of the exclusion globs */
	static bool MatchesAnyGlob(const FString& FileName, const TArray<FString>& Globs);

private:
	/** Registered definition types */
	TMap<FString, FDefinitionTypeInfo> RegisteredTypes;

	/** Cached asset lookup map for batch operations (used with FDefinitionAssetManager) */
	TMap<FString, FAssetData> CachedAssetMap;
	bool bUsingCachedAssetMap = false;

	/** Get source JSON directory for type */
	FString GetSourceDirectory(const FDefinitionTypeInfo& TypeInfo) const;

	/** Recursively remove empty subdirectories under a content path */
	static void CleanupEmptyDirectories(const FString& ContentPath);

	/** Compute content hash from normalized JSON text (UTF-8 safe) */
	FString ComputeContentHash(const FString& NormalizedJsonText) const;

	/** Compute structure hash from object definition (mesh IDs + capability types) */
	FString ComputeStructureHash(const UObjectDefinition* Def) const;

	/**
	 * Generate AbilitySet from item section GrantedAbilities/GrantedEffects.
	 * Uses deterministic hash for deduplication (same abilities = same asset).
	 * Creates in /ProjectGAS/Content/AbilitySets/AS_<hash>.uasset
	 * Sets EquipAbilitySet path on item section.
	 */
	void GenerateAbilitySetIfNeeded(UObjectDefinition* ObjDef);

	/** Compute hash from abilities/effects for AbilitySet deduplication */
	FString ComputeAbilitySetHash(const TArray<FSoftClassPath>& Abilities, const TArray<FSoftObjectPath>& Effects) const;
};
