// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldPresentationMaterialRealization.h"

#include "ProjectWorldPresentationProfile.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialParameters.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace ProjectWorldPresentationMaterialRealization
{
	namespace
	{
		bool HasExpectedParameters(
			const UMaterialInstanceConstant& Instance,
			const FProjectWorldPresentationProfile& Profile)
		{
			FLinearColor PrimaryColor;
			FLinearColor SecondaryColor;
			FLinearColor LineColor;
			float TileScale = 0.0f;
			return Instance.GetVectorParameterValue(
				FHashedMaterialParameterInfo(TEXT("Checker Colour 1")),
				PrimaryColor,
				true) &&
				Instance.GetVectorParameterValue(
					FHashedMaterialParameterInfo(TEXT("Checker Colour 2")),
					SecondaryColor,
					true) &&
				Instance.GetVectorParameterValue(
					FHashedMaterialParameterInfo(TEXT("Line Colour")),
					LineColor,
					true) &&
				Instance.GetScalarParameterValue(
					FHashedMaterialParameterInfo(TEXT("Tile Scale")),
					TileScale,
					true) &&
				PrimaryColor.Equals(Profile.TerrainPrimaryColor) &&
				SecondaryColor.Equals(Profile.TerrainSecondaryColor) &&
				LineColor.Equals(Profile.TerrainLineColor) &&
				FMath::IsNearlyEqual(TileScale, static_cast<float>(Profile.TerrainTileScale));
		}
	}

	bool Prepare(
		const FProjectWorldPresentationProfile& Profile,
		FProjectWorldPresentationResources& InOutResources,
		const FString& GeneratedPackageRoot,
		FString& OutError)
	{
		if (InOutResources.TerrainMaterial == nullptr)
		{
			OutError = TEXT("The terrain parent material is unavailable.");
			return false;
		}

		const FString AssetName = TEXT("MI_ProjectWorldTerrain_") + Profile.ProfileId;
		const FString PackageName = GeneratedPackageRoot + TEXT("Presentation/") + AssetName;
		const FString ObjectPath = PackageName + TEXT(".") + AssetName;
		UMaterialInstanceConstant* Instance =
			LoadObject<UMaterialInstanceConstant>(nullptr, *ObjectPath);
		if (Instance != nullptr &&
			Instance->Parent == InOutResources.TerrainMaterial &&
			HasExpectedParameters(*Instance, Profile))
		{
			InOutResources.TerrainMaterial = Instance;
			return true;
		}
		if (Instance == nullptr)
		{
			UPackage* Package = CreatePackage(*PackageName);
			if (Package == nullptr)
			{
				OutError = TEXT("Cannot create the generated terrain material package.");
				return false;
			}
			Instance = NewObject<UMaterialInstanceConstant>(
				Package,
				*AssetName,
				RF_Public | RF_Standalone);
			if (Instance == nullptr)
			{
				OutError = TEXT("Cannot create the generated terrain material instance.");
				return false;
			}
			FAssetRegistryModule::AssetCreated(Instance);
		}

		Instance->SetParentEditorOnly(InOutResources.TerrainMaterial);
		Instance->SetVectorParameterValueEditorOnly(
			FMaterialParameterInfo(TEXT("Checker Colour 1")),
			Profile.TerrainPrimaryColor);
		Instance->SetVectorParameterValueEditorOnly(
			FMaterialParameterInfo(TEXT("Checker Colour 2")),
			Profile.TerrainSecondaryColor);
		Instance->SetVectorParameterValueEditorOnly(
			FMaterialParameterInfo(TEXT("Line Colour")),
			Profile.TerrainLineColor);
		Instance->SetScalarParameterValueEditorOnly(
			FMaterialParameterInfo(TEXT("Tile Scale")),
			Profile.TerrainTileScale);
		Instance->PostEditChange();
		Instance->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			PackageName,
			FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArguments;
		SaveArguments.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArguments.SaveFlags = SAVE_NoError;
		if (!UPackage::SavePackage(
			Instance->GetOutermost(),
			Instance,
			*Filename,
			SaveArguments))
		{
			OutError = TEXT("Cannot save the generated terrain material instance.");
			return false;
		}
		InOutResources.TerrainMaterial = Instance;
		return true;
	}
}
