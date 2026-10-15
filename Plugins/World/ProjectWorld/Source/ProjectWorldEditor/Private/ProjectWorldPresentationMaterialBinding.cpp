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
#include "Misc/PackagePath.h"
#include "UObject/Package.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Utilities/ProjectSha256.h"

namespace ProjectWorldPresentationMaterialBinding
{
	namespace
	{
		constexpr TCHAR MaterialManifestRelativePath[] =
			TEXT("Data/Manifests/Surfaces/accepted.surface-manifest.json");

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
			if (!FPackageName::IsValidObjectPath(ObjectPath) || !FPackageName::IsValidLongPackageName(PackageName) || !PackageName.StartsWith(Prefix, ESearchCase::CaseSensitive) || ObjectPath.Contains(TEXT("..")))
			{
				return FString();
			}
			return FPaths::Combine(
				PluginContentDir,
				PackageName.RightChop(Prefix.Len()) + FPackageName::GetAssetPackageExtension());
		}
	}

	bool Resolve(const FString& RequestedBindingId,FProjectWorldMaterialReference& OutReference,FString& OutError)
	{
		OutReference={}; OutError.Reset();
		FProjectWorldMaterialReference Candidate;

		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ProjectMaterial"));
		if (!Plugin.IsValid())
		{
			OutError = FString::Printf(TEXT("ProjectMaterial is unavailable for binding %s."), *RequestedBindingId);
			return false;
		}
		const FString ManifestPath = FPaths::Combine(Plugin->GetBaseDir(), MaterialManifestRelativePath);
		FString ManifestText;
		if (!FFileHelper::LoadFileToString(ManifestText, *ManifestPath) ||
			!FProjectSha256::HashFile(ManifestPath, Candidate.ManifestSha256))
		{
			OutError = TEXT("The accepted ProjectMaterial manifest is missing or unreadable.");
			return false;
		}
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ManifestText);
		const TArray<TSharedPtr<FJsonValue>>* Records = nullptr;
		FString Schema;
		double SchemaVersion = 0.0;
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid() ||
			!Root->TryGetStringField(TEXT("$schema"), Schema) ||
			Schema != TEXT("../../Schemas/surface-manifest.schema.json") ||
			!Root->TryGetNumberField(TEXT("schema_version"), SchemaVersion) || SchemaVersion != 3.0 ||
			!Root->TryGetArrayField(TEXT("records"), Records) || Records == nullptr)
		{
			OutError = TEXT("The accepted ProjectMaterial manifest has an invalid envelope.");
			return false;
		}

		TSharedPtr<FJsonObject> InstanceRecord;
		TSharedPtr<FJsonObject> ParentRecord;
		FString InstanceObjectPath;
		FString ParentObjectPath;
		FString ParentPackageSha256;
		TSet<FString> Objects,Bindings;
		for (const TSharedPtr<FJsonValue>& Value : *Records)
		{
			const TSharedPtr<FJsonObject>* Record = nullptr;
			FString BindingId;
			FString OutputObjectPath;
			if (!Value->TryGetObject(Record) || Record == nullptr ||
				!ReadRequiredString(*Record, TEXT("output_object_path"), OutputObjectPath))
			{
				OutError = TEXT("The accepted ProjectMaterial manifest contains an invalid record.");
				return false;
			}
			(*Record)->TryGetStringField(TEXT("binding_id"), BindingId);
			if (Objects.Contains(OutputObjectPath) || (!BindingId.IsEmpty() && Bindings.Contains(BindingId)))
			{OutError=TEXT("Accepted material manifest has duplicate object or binding identities."); return false;}
			Objects.Add(OutputObjectPath); if (!BindingId.IsEmpty()) Bindings.Add(BindingId);
			if (BindingId == RequestedBindingId)
			{
				if (InstanceRecord.IsValid())
				{
					OutError = TEXT("Requested surface binding has duplicate accepted material records.");
					return false;
				}
				InstanceRecord = *Record;
				InstanceObjectPath = OutputObjectPath;
				if (!ReadRequiredString(InstanceRecord, TEXT("dependency_object_path"), ParentObjectPath) ||
					!ReadRequiredString(InstanceRecord, TEXT("dependency_package_sha256"), ParentPackageSha256))
				{
					OutError = TEXT("Requested surface binding has no authenticated parent dependency.");
					return false;
				}
			}
		}
		if (!InstanceRecord.IsValid())
		{
			OutError = TEXT("Requested surface binding is absent from the accepted ProjectMaterial manifest.");
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
		if (!ReadRequiredString(InstanceRecord, TEXT("package_sha256"), AcceptedPackageSha256) ||
			!ReadRequiredString(InstanceRecord, TEXT("semantic_identity"), SemanticIdentity) ||
			!ReadRequiredString(ParentRecord, TEXT("package_sha256"), AcceptedParentSha256) ||
			!IsSha256(AcceptedPackageSha256) || !IsSha256(SemanticIdentity) ||
			!IsSha256(ParentPackageSha256) || AcceptedParentSha256 != ParentPackageSha256)
		{
			OutError = TEXT("Requested surface binding manifest identity or dependency digest is invalid.");
			return false;
		}

		const FString InstanceFilename = PackageFilename(Plugin->GetContentDir(), InstanceObjectPath);
		const FString ParentFilename = PackageFilename(Plugin->GetContentDir(), ParentObjectPath);
		FString ActualInstanceSha256;
		FString ActualParentSha256;
		if (InstanceFilename.IsEmpty() || ParentFilename.IsEmpty() ||
			!FProjectSha256::HashFile(InstanceFilename, ActualInstanceSha256) ||
			!FProjectSha256::HashFile(ParentFilename, ActualParentSha256) ||
			ActualInstanceSha256 != AcceptedPackageSha256 || ActualParentSha256 != ParentPackageSha256)
		{
			OutError = TEXT("Requested surface binding bytes do not match the accepted ProjectMaterial manifest.");
			return false;
		}

		UMaterialInstanceConstant* Terrain =
			LoadObject<UMaterialInstanceConstant>(nullptr, *InstanceObjectPath);
		if (Terrain == nullptr || Terrain->Parent == nullptr ||
			Terrain->Parent->GetPathName() != ParentObjectPath ||
			Terrain->GetOutermost()->IsDirty() || Terrain->Parent->GetOutermost()->IsDirty() ||
			!FPaths::IsSamePath(Terrain->GetOutermost()->GetLoadedPath().GetLocalFullPath(),InstanceFilename) ||
			!FPaths::IsSamePath(Terrain->Parent->GetOutermost()->GetLoadedPath().GetLocalFullPath(),ParentFilename))
		{
			OutError = TEXT("Requested surface binding cannot be loaded with its authenticated parent.");
			return false;
		}
		Candidate.Material=Terrain; Candidate.ObjectPath=InstanceObjectPath;
		Candidate.PackageSha256=AcceptedPackageSha256; Candidate.SemanticIdentity=SemanticIdentity;
		OutReference=MoveTemp(Candidate); return true;
	}
	bool ResolveTerrain(FProjectWorldPresentationResources& Resources,FString& Error)
	{
		Resources.TerrainMaterial=nullptr; Resources.TerrainMaterialObjectPath.Reset();
		Resources.TerrainMaterialPackageSha256.Reset(); Resources.TerrainMaterialSemanticIdentity.Reset();
		Resources.TerrainMaterialManifestSha256.Reset(); Resources.MaterialBindings.Remove(TEXT("terrain"));
		FProjectWorldMaterialReference R;
		if (!Resolve(TEXT("terrain.default"),R,Error)) return false;
		Resources.TerrainMaterial=R.Material; Resources.TerrainMaterialObjectPath=R.ObjectPath;
		Resources.TerrainMaterialPackageSha256=R.PackageSha256; Resources.TerrainMaterialSemanticIdentity=R.SemanticIdentity;
		Resources.TerrainMaterialManifestSha256=R.ManifestSha256;
		Resources.MaterialBindings.Add(TEXT("terrain"),MoveTemp(R)); return true;
	}
}
