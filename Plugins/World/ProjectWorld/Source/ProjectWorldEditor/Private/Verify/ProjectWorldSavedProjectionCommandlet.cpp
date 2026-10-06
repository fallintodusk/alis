// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldSavedProjectionCommandlet.h"

#include "ProjectWorldRealizationProfile.h"
#include "ProjectWorldVerify.h"

#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHandle.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectWorldSavedProjection, Log, All);

UProjectWorldSavedProjectionCommandlet::UProjectWorldSavedProjectionCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UProjectWorldSavedProjectionCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> Parameters;
	ParseCommandLine(*Params, Tokens, Switches, Parameters);
	const FString* Map = Parameters.Find(TEXT("Map"));
	const FString* ProfilePath = Parameters.Find(TEXT("RealizationProfile"));
	const FString* ResultPath = Parameters.Find(TEXT("Result"));
	if (Map == nullptr || ProfilePath == nullptr || ResultPath == nullptr ||
		!FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(*ResultPath),
			FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Validation/WorldRealization"))))
	{
		UE_LOG(LogProjectWorldSavedProjection, Error, TEXT("Saved projection requires a map, realization profile, and safe result path."));
		return 2;
	}
	FProjectWorldRealizationProfile Profile;
	FString ErrorCode;
	FString Error;
	if (!ProjectWorldRealizationProfile::Load(*ProfilePath, Profile, ErrorCode, Error) ||
		Profile.MapPackagePath != *Map)
	{
		UE_LOG(LogProjectWorldSavedProjection, Error, TEXT("Realization profile does not match map: %s"), *Error);
		return 3;
	}
	const FProjectWorldRealizationLayer* Buildings = Profile.Layers.FindByPredicate([](const auto& Layer)
	{
		return Layer.LayerId == TEXT("buildings") && Layer.GeneratorId == TEXT("project_building_massing");
	});
	if (Buildings == nullptr)
	{
		UE_LOG(LogProjectWorldSavedProjection, Error, TEXT("Realization profile has no building producer."));
		return 3;
	}
	const FString MapFilename = FPackageName::LongPackageNameToFilename(
		*Map, FPackageName::GetMapPackageExtension());
	TArray<FString> ExternalActors;
	IFileManager::Get().FindFilesRecursive(ExternalActors,
		*FPackageName::LongPackageNameToFilename(ULevel::GetExternalActorsPath(*Map)),
		TEXT("*.uasset"), true, false);
	FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"))
		.Get().ScanFilesSynchronous(ExternalActors, true);
	if (!FEditorFileUtils::LoadMap(MapFilename, false, false))
	{
		UE_LOG(LogProjectWorldSavedProjection, Error, TEXT("Saved map cannot be loaded: %s"), **Map);
		return 4;
	}
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (World == nullptr || World->GetWorldPartition() == nullptr)
	{
		return 4;
	}
	TArray<FWorldPartitionReference> References;
	World->GetWorldPartition()->LoadAllActors(References);
	FAssetCompilingManager::Get().FinishAllCompilation();
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("schema"), TEXT("project-world-saved-projection:v1"));
	Root->SetStringField(TEXT("map_package"), *Map);
	struct FProducer
	{
		FString GeneratorId;
		int32 GeneratorVersion;
		FString ArtifactRoot;
		FString ResultName;
	};
	for (const FProducer& Producer : {
		FProducer{TEXT("map"), 1, *Map, TEXT("map")},
		FProducer{TEXT("project_building_massing"), Buildings->GeneratorVersion,
			Buildings->ArtifactRoot, TEXT("buildings")}})
	{
		int32 ComparisonVersion = 0;
		FProjectWorldProjectFn Project;
		if (!ProjectWorldVerify::FindProjection(Producer.GeneratorId, Producer.GeneratorVersion, ComparisonVersion, Project))
		{
			UE_LOG(LogProjectWorldSavedProjection, Error, TEXT("No saved projection registered: %s"), *Producer.GeneratorId);
			return 5;
		}
		FProjectWorldProjection Projection;
		if (!Project({World, Producer.ArtifactRoot, nullptr}, Projection, Error) ||
			!Projection.IsValid(Error) || Projection.IsEmpty())
		{
			UE_LOG(LogProjectWorldSavedProjection, Error, TEXT("Saved projection rejected: %s"), *Error);
			return 5;
		}
		TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("generator_id"), Producer.GeneratorId);
		Item->SetNumberField(TEXT("generator_version"), Producer.GeneratorVersion);
		Item->SetNumberField(TEXT("comparison_version"), ComparisonVersion);
		Item->SetNumberField(TEXT("record_count"), Projection.Sorted().Num());
		Item->SetStringField(TEXT("sha256"), Projection.Sha256());
		Item->SetStringField(TEXT("projection_json"), Projection.ToCanonicalJson());
		Root->SetObjectField(Producer.ResultName, Item);
	}
	Root->SetStringField(TEXT("status"), TEXT("accepted"));
	FString Json;
	if (!FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json)))
	{
		return 5;
	}
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(*ResultPath), true);
	return FFileHelper::SaveStringToFile(Json, **ResultPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) ? 0 : 5;
}
