// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldStaticPartitionAuditCommandlet.h"

#include "ProjectWorldRuntimeProfile.h"
#include "ProjectWorldStaticPartitionAudit.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/Level.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHandle.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectWorldStaticPartitionAudit, Log, All);

namespace
{
	bool IsSafeResultPath(const FString& ResultPath)
	{
		const FString EvidenceRoot = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Validation/WorldRealization")));
		return FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(ResultPath), EvidenceRoot);
	}

	UWorld* LoadWorld(const FString& MapPackagePath, FString& OutError)
	{
		if (!FPackageName::IsValidLongPackageName(MapPackagePath) ||
			!MapPackagePath.StartsWith(TEXT("/ProjectWorldData/Generated/")))
		{
			OutError = TEXT("Static partition audit map must stay under /ProjectWorldData/Generated/.");
			return nullptr;
		}
		const FString MapFilename = FPackageName::LongPackageNameToFilename(
			MapPackagePath,
			FPackageName::GetMapPackageExtension());
		if (!IFileManager::Get().FileExists(*MapFilename))
		{
			OutError = FString::Printf(TEXT("Static partition audit map does not exist: %s"), *MapFilename);
			return nullptr;
		}

		IAssetRegistry& AssetRegistry =
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		const FString ExternalActorsPath = ULevel::GetExternalActorsPath(MapPackagePath);
		TArray<FString> ExternalActorFiles;
		IFileManager::Get().FindFilesRecursive(
			ExternalActorFiles,
			*FPackageName::LongPackageNameToFilename(ExternalActorsPath),
			TEXT("*.uasset"),
			true,
			false);
		AssetRegistry.ScanFilesSynchronous(ExternalActorFiles, true);
		if (!FEditorFileUtils::LoadMap(MapFilename, false, false))
		{
			OutError = TEXT("Cannot load the generated World Partition map.");
			return nullptr;
		}
		return GEditor->GetEditorWorldContext().World();
	}

	bool WriteReceipt(const FString& ResultPath, const TSharedPtr<FJsonObject>& Receipt)
	{
		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		if (!FJsonSerializer::Serialize(Receipt.ToSharedRef(), Writer))
		{
			return false;
		}
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(ResultPath), true);
		return FFileHelper::SaveStringToFile(Json, *ResultPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
}

UProjectWorldStaticPartitionAuditCommandlet::UProjectWorldStaticPartitionAuditCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UProjectWorldStaticPartitionAuditCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> Parameters;
	ParseCommandLine(*Params, Tokens, Switches, Parameters);
	const FString* MapPackagePath = Parameters.Find(TEXT("Map"));
	const FString* RuntimeProfilesValue = Parameters.Find(TEXT("RuntimeProfiles"));
	const FString* SelectedProfileId = Parameters.Find(TEXT("SelectedProfile"));
	const FString* ResultValue = Parameters.Find(TEXT("Result"));
	if (MapPackagePath == nullptr || RuntimeProfilesValue == nullptr ||
		SelectedProfileId == nullptr || ResultValue == nullptr)
	{
		UE_LOG(
			LogProjectWorldStaticPartitionAudit,
			Error,
			TEXT("[ProjectWorldStaticPartitionAuditCommandlet::Main] Usage - require -Map=<package> -RuntimeProfiles=<path|path> -SelectedProfile=<id> -Result=<path>."));
		return 2;
	}

	const FString ResultPath = FPaths::ConvertRelativePathToFull(*ResultValue);
	if (!IsSafeResultPath(ResultPath))
	{
		UE_LOG(
			LogProjectWorldStaticPartitionAudit,
			Error,
			TEXT("[ProjectWorldStaticPartitionAuditCommandlet::Main] Unsafe result path - %s"),
			*ResultPath);
		return 2;
	}
	IFileManager::Get().Delete(*ResultPath, false, true);

	TArray<FString> ProfilePaths;
	RuntimeProfilesValue->ParseIntoArray(ProfilePaths, TEXT("|"), true);
	TArray<FProjectWorldRuntimeProfile> Profiles;
	for (const FString& ProfilePath : ProfilePaths)
	{
		FProjectWorldRuntimeProfile& Profile = Profiles.AddDefaulted_GetRef();
		FString ErrorCode;
		FString Error;
		if (!ProjectWorldRuntimeProfile::Load(
			FPaths::ConvertRelativePathToFull(ProfilePath), Profile, ErrorCode, Error))
		{
			UE_LOG(
				LogProjectWorldStaticPartitionAudit,
				Error,
				TEXT("[ProjectWorldStaticPartitionAuditCommandlet::Main] Runtime profile rejected - code=%s error=%s path=%s"),
				*ErrorCode,
				*Error,
				*ProfilePath);
			return 3;
		}
	}

	FString Error;
	UWorld* World = LoadWorld(*MapPackagePath, Error);
	if (World == nullptr || !World->IsPartitionedWorld())
	{
		UE_LOG(
			LogProjectWorldStaticPartitionAudit,
			Error,
			TEXT("[ProjectWorldStaticPartitionAuditCommandlet::Main] Map load rejected - %s"),
			*Error);
		return 4;
	}
	TArray<FWorldPartitionReference> LoadedActorReferences;
	World->GetWorldPartition()->LoadAllActors(LoadedActorReferences);

	TSharedPtr<FJsonObject> Receipt;
	const bool bAccepted = ProjectWorldStaticPartitionAudit::Capture(
		World, Profiles, *SelectedProfileId, Receipt, Error);
	if (!Receipt.IsValid())
	{
		UE_LOG(
			LogProjectWorldStaticPartitionAudit,
			Error,
			TEXT("[ProjectWorldStaticPartitionAuditCommandlet::Main] Audit failed - %s"),
			*Error);
		return 5;
	}
	if (!WriteReceipt(ResultPath, Receipt))
	{
		UE_LOG(
			LogProjectWorldStaticPartitionAudit,
			Error,
			TEXT("[ProjectWorldStaticPartitionAuditCommandlet::Main] Receipt write failed - %s"),
			*ResultPath);
		return 6;
	}

	if (bAccepted)
	{
		UE_LOG(
			LogProjectWorldStaticPartitionAudit,
			Display,
			TEXT("[ProjectWorldStaticPartitionAuditCommandlet::Main] Complete - status=accepted profiles=%d result=%s"),
			Profiles.Num(),
			*ResultPath);
	}
	else
	{
		UE_LOG(
			LogProjectWorldStaticPartitionAudit,
			Error,
			TEXT("[ProjectWorldStaticPartitionAuditCommandlet::Main] Complete - status=rejected profiles=%d result=%s"),
			Profiles.Num(),
			*ResultPath);
	}
	return bAccepted ? 0 : 1;
}
