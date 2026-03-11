// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "DefinitionTypeInfo.h"

/**
 * Asset manager for definition assets.
 * Handles find, create, save operations.
 *
 * SOLID: Single responsibility - only asset CRUD operations.
 * Extracted from UDefinitionGeneratorSubsystem for maintainability.
 */
class PROJECTDEFINITIONGENERATOR_API FDefinitionAssetManager
{
public:
	/**
	 * Build asset lookup map for batch operations.
	 * @param TypeInfo Type configuration
	 * @return Map of AssetName -> FAssetData
	 */
	static TMap<FString, FAssetData> BuildAssetLookupMap(const FDefinitionTypeInfo& TypeInfo);

	/**
	 * Find existing asset by ID.
	 * @param TypeInfo Type configuration
	 * @param AssetId Asset ID to find
	 * @param CachedMap Optional cached lookup map for batch operations
	 * @return Existing asset or nullptr
	 */
	static UObject* FindExistingAsset(
		const FDefinitionTypeInfo& TypeInfo,
		const FString& AssetId,
		const TMap<FString, FAssetData>* CachedMap = nullptr);

	/**
	 * Create new asset in category subfolder.
	 * @param TypeInfo Type configuration
	 * @param AssetId Asset ID (becomes asset name)
	 * @param Category Subfolder category (mirrors JSON hierarchy)
	 * @return New asset or nullptr on failure
	 */
	static UObject* CreateNewAsset(
		const FDefinitionTypeInfo& TypeInfo,
		const FString& AssetId,
		const FString& Category);

	/**
	 * Save asset to disk.
	 * @param Asset Asset to save
	 * @return True if saved successfully
	 */
	static bool SaveAsset(UObject* Asset);
};
