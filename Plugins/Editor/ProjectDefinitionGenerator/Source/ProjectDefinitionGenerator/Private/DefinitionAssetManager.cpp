// Copyright ALIS. All Rights Reserved.

#include "DefinitionAssetManager.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogDefinitionAssetManager, Log, All);

TMap<FString, FAssetData> FDefinitionAssetManager::BuildAssetLookupMap(const FDefinitionTypeInfo& TypeInfo)
{
	TMap<FString, FAssetData> AssetMap;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(TypeInfo.DefinitionClass->GetClassPathName());
	Filter.PackagePaths.Add(FName(*TypeInfo.GeneratedContentPath));
	Filter.bRecursivePaths = true;

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	for (const FAssetData& AssetData : AssetDataList)
	{
		AssetMap.Add(AssetData.AssetName.ToString(), AssetData);
	}

	return AssetMap;
}

UObject* FDefinitionAssetManager::FindExistingAsset(
	const FDefinitionTypeInfo& TypeInfo,
	const FString& AssetId,
	const TMap<FString, FAssetData>* CachedMap)
{
	// Use cached map if provided (batch operation)
	if (CachedMap)
	{
		if (const FAssetData* Found = CachedMap->Find(AssetId))
		{
			return Found->GetAsset();
		}
		return nullptr;
	}

	// Fallback to direct registry scan for single lookups
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(TypeInfo.DefinitionClass->GetClassPathName());
	Filter.PackagePaths.Add(FName(*TypeInfo.GeneratedContentPath));
	Filter.bRecursivePaths = true;

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	for (const FAssetData& AssetData : AssetDataList)
	{
		if (AssetData.AssetName.ToString() == AssetId)
		{
			return AssetData.GetAsset();
		}
	}

	return nullptr;
}

UObject* FDefinitionAssetManager::CreateNewAsset(
	const FDefinitionTypeInfo& TypeInfo,
	const FString& AssetId,
	const FString& Category)
{
	FString PackagePath = TypeInfo.GeneratedContentPath;
	if (!Category.IsEmpty())
	{
		PackagePath = PackagePath / Category;
	}
	PackagePath = PackagePath / AssetId;

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		UE_LOG(LogDefinitionAssetManager, Error, TEXT("Failed to create package: %s"), *PackagePath);
		return nullptr;
	}

	// Guard: check for stale object at the same path (hot-reload REINST_ class mismatch)
	UObject* ExistingObj = StaticFindObject(UObject::StaticClass(), Package, *AssetId);
	if (ExistingObj)
	{
		if (ExistingObj->GetClass() == TypeInfo.DefinitionClass)
		{
			UE_LOG(LogDefinitionAssetManager, Log,
				TEXT("[CreateNewAsset] Reusing existing object '%s' (class %s)"),
				*PackagePath, *TypeInfo.DefinitionClass->GetName());
			return ExistingObj;
		}

		// Different class at same path - stale object from hot-reload / live coding
		UE_LOG(LogDefinitionAssetManager, Warning,
			TEXT("[CreateNewAsset] Stale object at '%s': existing class '%s' != expected '%s'. "
				"This happens after Live Coding adds new definition classes. "
				"Renaming stale object; restart the editor for a clean state."),
			*PackagePath,
			*ExistingObj->GetClass()->GetPathName(),
			*TypeInfo.DefinitionClass->GetPathName());

		ExistingObj->Rename(nullptr, GetTransientPackage(),
			REN_DontCreateRedirectors | REN_NonTransactional);
	}

	UObject* NewAsset = NewObject<UObject>(
		Package,
		TypeInfo.DefinitionClass,
		*AssetId,
		RF_Public | RF_Standalone | RF_Transactional);

	if (NewAsset)
	{
		FAssetRegistryModule::AssetCreated(NewAsset);
		Package->MarkPackageDirty();
	}

	return NewAsset;
}

bool FDefinitionAssetManager::SaveAsset(UObject* Asset)
{
	if (!Asset)
	{
		return false;
	}

	UPackage* Package = Asset->GetOutermost();
	if (!Package)
	{
		return false;
	}

	const FString PackageFileName = FPackageName::LongPackageNameToFilename(
		Package->GetName(), FPackageName::GetAssetPackageExtension());

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.Error = GError;

	FSavePackageResultStruct Result = UPackage::Save(Package, Asset, *PackageFileName, SaveArgs);

	return Result.Result == ESavePackageResult::Success;
}
