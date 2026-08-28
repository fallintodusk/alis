// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectMaterialManifest.h"

#include "ProjectMaterialRecipe.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace ProjectMaterialManifestPrivate
{
bool ReadString(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* Name,
	FString& OutValue,
	FString& OutError)
{
	if (!Object->TryGetStringField(Name, OutValue) || OutValue.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Manifest record is missing %s."), Name);
		return false;
	}
	return true;
}
}

bool ProjectMaterialManifest::Load(
	const FString& ManifestPath,
	TMap<FString, FProjectMaterialManifestRecord>& OutRecords,
	FString& OutError)
{
	OutRecords.Reset();
	if (!FPaths::FileExists(ManifestPath))
	{
		return true;
	}
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *ManifestPath))
	{
		OutError = FString::Printf(TEXT("Could not read material manifest: %s"), *ManifestPath);
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FString SchemaVersion;
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid() ||
		!Root->TryGetStringField(TEXT("schema_version"), SchemaVersion) || SchemaVersion != TEXT("1"))
	{
		OutError = TEXT("Material manifest is malformed or has an unsupported schema.");
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Root->TryGetArrayField(TEXT("records"), Values) || Values == nullptr)
	{
		OutError = TEXT("Material manifest is missing records.");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		const TSharedPtr<FJsonObject> Object = Value->AsObject();
		FProjectMaterialManifestRecord Record;
		if (!Object.IsValid() ||
			!ProjectMaterialManifestPrivate::ReadString(Object, TEXT("recipe_path"), Record.RecipePath, OutError) ||
			!ProjectMaterialManifestPrivate::ReadString(Object, TEXT("recipe_sha256"), Record.RecipeSha256, OutError) ||
			!ProjectMaterialManifestPrivate::ReadString(Object, TEXT("family"), Record.Family, OutError) ||
			!ProjectMaterialManifestPrivate::ReadString(Object, TEXT("archetype"), Record.Archetype, OutError) ||
			!ProjectMaterialManifestPrivate::ReadString(Object, TEXT("compiler_version"), Record.CompilerVersion, OutError) ||
			!ProjectMaterialManifestPrivate::ReadString(Object, TEXT("output_object_path"), Record.OutputObjectPath, OutError) ||
			!ProjectMaterialManifestPrivate::ReadString(Object, TEXT("semantic_identity"), Record.SemanticIdentity, OutError) ||
			!ProjectMaterialManifestPrivate::ReadString(Object, TEXT("package_sha256"), Record.PackageSha256, OutError) ||
			!ProjectMaterialManifestPrivate::ReadString(Object, TEXT("compiler_fingerprint"), Record.CompilerFingerprint, OutError) ||
			!ProjectMaterialManifestPrivate::ReadString(Object, TEXT("engine_identity"), Record.EngineIdentity, OutError))
		{
			return false;
		}
		Object->TryGetStringField(TEXT("dependency_object_path"), Record.DependencyObjectPath);
		Object->TryGetStringField(TEXT("dependency_package_sha256"), Record.DependencyPackageSha256);
		if (OutRecords.Contains(Record.OutputObjectPath))
		{
			OutError = FString::Printf(TEXT("Duplicate output in material manifest: %s"), *Record.OutputObjectPath);
			return false;
		}
		OutRecords.Add(Record.OutputObjectPath, MoveTemp(Record));
	}
	return true;
}

FString ProjectMaterialManifest::Serialize(const TArray<FProjectMaterialManifestRecord>& Records)
{
	TArray<FProjectMaterialManifestRecord> Sorted = Records;
	Sorted.Sort([](const auto& Left, const auto& Right)
	{
		return Left.OutputObjectPath < Right.OutputObjectPath;
	});
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("$schema"), TEXT("../../Schemas/material-manifest.schema.json"));
	Root->SetStringField(TEXT("schema_version"), TEXT("1"));
	TArray<TSharedPtr<FJsonValue>> Values;
	for (const FProjectMaterialManifestRecord& Record : Sorted)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("recipe_path"), Record.RecipePath);
		Object->SetStringField(TEXT("recipe_sha256"), Record.RecipeSha256);
		Object->SetStringField(TEXT("family"), Record.Family);
		Object->SetStringField(TEXT("archetype"), Record.Archetype);
		Object->SetStringField(TEXT("compiler_version"), Record.CompilerVersion);
		if (!Record.DependencyObjectPath.IsEmpty())
		{
			Object->SetStringField(TEXT("dependency_object_path"), Record.DependencyObjectPath);
			Object->SetStringField(TEXT("dependency_package_sha256"), Record.DependencyPackageSha256);
		}
		Object->SetStringField(TEXT("output_object_path"), Record.OutputObjectPath);
		Object->SetStringField(TEXT("semantic_identity"), Record.SemanticIdentity);
		Object->SetStringField(TEXT("package_sha256"), Record.PackageSha256);
		Object->SetStringField(TEXT("compiler_fingerprint"), Record.CompilerFingerprint);
		Object->SetStringField(TEXT("engine_identity"), Record.EngineIdentity);
		Values.Add(MakeShared<FJsonValueObject>(Object));
	}
	Root->SetArrayField(TEXT("records"), Values);
	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Root, Writer);
	return Json + LINE_TERMINATOR;
}

bool ProjectMaterialManifest::Save(
	const FString& ManifestPath,
	const TArray<FProjectMaterialManifestRecord>& Records,
	FString& OutSha256,
	FString& OutError)
{
	const FString Json = Serialize(Records);
	OutSha256 = FProjectMaterialRecipeContract::ComputeStringSha256(Json);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ManifestPath), true);
	const FString StagingPath = ManifestPath + TEXT(".staging");
	if (!FFileHelper::SaveStringToFile(Json, *StagingPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("Could not stage material manifest: %s"), *StagingPath);
		return false;
	}
	if (!IFileManager::Get().Move(*ManifestPath, *StagingPath, true, true, false, true))
	{
		IFileManager::Get().Delete(*StagingPath);
		OutError = FString::Printf(TEXT("Could not promote material manifest: %s"), *ManifestPath);
		return false;
	}
	return true;
}
