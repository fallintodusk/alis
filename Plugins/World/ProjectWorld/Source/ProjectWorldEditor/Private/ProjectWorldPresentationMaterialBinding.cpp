// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldPresentationMaterialBinding.h"

#include "ProjectWorldPresentationProfile.h"

#include "Dom/JsonObject.h"
#include "Interfaces/IPluginManager.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Utilities/ProjectSha256.h"

namespace ProjectWorldPresentationMaterialBinding
{
	namespace
	{
		constexpr TCHAR TerrainObjectPath[] =
			TEXT("/ProjectMaterial/Generated/Terrain/MI_ProjectTerrain_Default.MI_ProjectTerrain_Default");
		constexpr TCHAR MaterialManifestRelativePath[] =
			TEXT("Data/Manifests/Materials/accepted.material-manifest.json");

		bool IsSha256(const FString& Value)
		{
			if (Value.Len() != 64)
			{
				return false;
			}
			for (const TCHAR Character : Value)
			{
				if (!FChar::IsHexDigit(Character))
				{
					return false;
				}
			}
			return true;
		}

		bool ReadRequiredString(
			const TSharedPtr<FJsonObject>& Object,
			const TCHAR* Name,
			FString& OutValue)
		{
			return Object.IsValid() && Object->TryGetStringField(Name, OutValue) && !OutValue.IsEmpty();
		}

		FString PackageFilename(const FString& PluginContentDir, const FString& ObjectPath)
		{
			const FString PackageName = FPackageName::ObjectPathToPackageName(ObjectPath);
			const FString Prefix = TEXT("/ProjectMaterial/");
			if (!PackageName.StartsWith(Prefix, ESearchCase::CaseSensitive))
			{
				return FString();
			}
			return FPaths::Combine(
				PluginContentDir,
				PackageName.RightChop(Prefix.Len()) + FPackageName::GetAssetPackageExtension());
		}
	}

	bool ResolveTerrain(
		FProjectWorldPresentationResources& InOutResources,
		FString& OutError)
	{
		InOutResources.TerrainMaterial = nullptr;
		InOutResources.TerrainMaterialObjectPath.Reset();
		InOutResources.TerrainMaterialPackageSha256.Reset();
		InOutResources.TerrainMaterialSemanticIdentity.Reset();
		InOutResources.TerrainMaterialManifestSha256.Reset();

		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ProjectMaterial"));
		if (!Plugin.IsValid())
		{
			OutError = TEXT("ProjectMaterial is unavailable for terrain.default.");
			return false;
		}
		const FString ManifestPath = FPaths::Combine(Plugin->GetBaseDir(), MaterialManifestRelativePath);
		FString ManifestText;
		if (!FFileHelper::LoadFileToString(ManifestText, *ManifestPath) ||
			!FProjectSha256::HashFile(ManifestPath, InOutResources.TerrainMaterialManifestSha256))
		{
			OutError = TEXT("The accepted ProjectMaterial manifest is missing or unreadable.");
			return false;
		}
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ManifestText);
		const TArray<TSharedPtr<FJsonValue>>* Records = nullptr;
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid() ||
			!Root->TryGetArrayField(TEXT("records"), Records) || Records == nullptr)
		{
			OutError = TEXT("The accepted ProjectMaterial manifest has an invalid envelope.");
			return false;
		}

		TSharedPtr<FJsonObject> TerrainRecord;
		TSharedPtr<FJsonObject> ParentRecord;
		FString ParentObjectPath;
		FString ParentPackageSha256;
		for (const TSharedPtr<FJsonValue>& Value : *Records)
		{
			const TSharedPtr<FJsonObject>* Record = nullptr;
			FString OutputObjectPath;
			if (!Value->TryGetObject(Record) || Record == nullptr ||
				!ReadRequiredString(*Record, TEXT("output_object_path"), OutputObjectPath))
			{
				OutError = TEXT("The accepted ProjectMaterial manifest contains an invalid record.");
				return false;
			}
			if (OutputObjectPath == TerrainObjectPath)
			{
				if (TerrainRecord.IsValid())
				{
					OutError = TEXT("terrain.default has duplicate accepted material records.");
					return false;
				}
				TerrainRecord = *Record;
				if (!ReadRequiredString(TerrainRecord, TEXT("dependency_object_path"), ParentObjectPath) ||
					!ReadRequiredString(TerrainRecord, TEXT("dependency_package_sha256"), ParentPackageSha256))
				{
					OutError = TEXT("terrain.default has no authenticated parent dependency.");
					return false;
				}
			}
		}
		if (!TerrainRecord.IsValid())
		{
			OutError = TEXT("terrain.default is absent from the accepted ProjectMaterial manifest.");
			return false;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Records)
		{
			const TSharedPtr<FJsonObject>* Record = nullptr;
			FString OutputObjectPath;
			if (Value->TryGetObject(Record) && Record != nullptr &&
				ReadRequiredString(*Record, TEXT("output_object_path"), OutputObjectPath) &&
				OutputObjectPath == ParentObjectPath)
			{
				ParentRecord = *Record;
				break;
			}
		}

		FString AcceptedPackageSha256;
		FString SemanticIdentity;
		FString AcceptedParentSha256;
		if (!ReadRequiredString(TerrainRecord, TEXT("package_sha256"), AcceptedPackageSha256) ||
			!ReadRequiredString(TerrainRecord, TEXT("semantic_identity"), SemanticIdentity) ||
			!ReadRequiredString(ParentRecord, TEXT("package_sha256"), AcceptedParentSha256) ||
			!IsSha256(AcceptedPackageSha256) || !IsSha256(SemanticIdentity) ||
			!IsSha256(ParentPackageSha256) || AcceptedParentSha256 != ParentPackageSha256)
		{
			OutError = TEXT("terrain.default manifest identity or dependency digest is invalid.");
			return false;
		}

		const FString TerrainFilename = PackageFilename(Plugin->GetContentDir(), TerrainObjectPath);
		const FString ParentFilename = PackageFilename(Plugin->GetContentDir(), ParentObjectPath);
		FString ActualTerrainSha256;
		FString ActualParentSha256;
		if (TerrainFilename.IsEmpty() || ParentFilename.IsEmpty() ||
			!FProjectSha256::HashFile(TerrainFilename, ActualTerrainSha256) ||
			!FProjectSha256::HashFile(ParentFilename, ActualParentSha256) ||
			ActualTerrainSha256 != AcceptedPackageSha256 || ActualParentSha256 != ParentPackageSha256)
		{
			OutError = TEXT("terrain.default bytes do not match the accepted ProjectMaterial manifest.");
			return false;
		}

		UMaterialInstanceConstant* Terrain =
			LoadObject<UMaterialInstanceConstant>(nullptr, TerrainObjectPath);
		if (Terrain == nullptr || Terrain->Parent == nullptr ||
			Terrain->Parent->GetPathName() != ParentObjectPath)
		{
			OutError = TEXT("terrain.default cannot be loaded with its authenticated parent.");
			return false;
		}
		InOutResources.TerrainMaterial = Terrain;
		InOutResources.TerrainMaterialObjectPath = TerrainObjectPath;
		InOutResources.TerrainMaterialPackageSha256 = AcceptedPackageSha256;
		InOutResources.TerrainMaterialSemanticIdentity = SemanticIdentity;
		return true;
	}
}
