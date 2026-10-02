// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectTexturePatternManifest.h"

#include "ProjectTexturePatternRecipe.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace ProjectTexturePatternManifestPrivate
{
	bool ReadString(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Name,
		FString& OutValue,
		FString& OutError)
	{
		if (!Object->TryGetStringField(Name, OutValue) || OutValue.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Pattern manifest record is missing %s."), Name);
			return false;
		}
		return true;
	}
}

bool ProjectTexturePatternManifest::Load(
	const FString& ManifestPath,
	TMap<FString, FProjectTexturePatternManifestRecord>& OutRecords,
	FString& OutError)
{
	OutRecords.Reset();
	OutError.Reset();
	if (!FPaths::FileExists(ManifestPath))
	{
		return true;
	}
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *ManifestPath))
	{
		OutError = FString::Printf(TEXT("Could not read pattern manifest: %s"), *ManifestPath);
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	double SchemaVersion = 0.0;
	FString Schema;
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid() || Root->Values.Num() != 3 ||
		!Root->TryGetStringField(TEXT("$schema"), Schema) ||
		Schema != TEXT("../../Schemas/pattern-manifest.schema.json") ||
		!Root->TryGetNumberField(TEXT("schema_version"), SchemaVersion) || SchemaVersion != 3.0 ||
		!Root->TryGetArrayField(TEXT("records"), Values) || Values == nullptr)
	{
		OutError = TEXT("Pattern manifest envelope is invalid.");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		FProjectTexturePatternManifestRecord Record;
		if (!Value->TryGetObject(Object) || Object == nullptr || (*Object)->Values.Num() != 13 ||
			!ProjectTexturePatternManifestPrivate::ReadString(*Object, TEXT("recipe_path"), Record.RecipePath, OutError) ||
			!ProjectTexturePatternManifestPrivate::ReadString(*Object, TEXT("recipe_sha256"), Record.RecipeSha256, OutError) ||
			!ProjectTexturePatternManifestPrivate::ReadString(*Object, TEXT("recipe_source_sha256"), Record.RecipeSourceSha256, OutError) ||
			!ProjectTexturePatternManifestPrivate::ReadString(*Object, TEXT("pattern_id"), Record.PatternId, OutError) ||
			!ProjectTexturePatternManifestPrivate::ReadString(*Object, TEXT("family"), Record.Family, OutError) ||
			!ProjectTexturePatternManifestPrivate::ReadString(*Object, TEXT("algorithm"), Record.Algorithm, OutError) ||
			!ProjectTexturePatternManifestPrivate::ReadString(*Object, TEXT("compiler_version"), Record.CompilerVersion, OutError) ||
			!ProjectTexturePatternManifestPrivate::ReadString(*Object, TEXT("output_contract"), Record.OutputContract, OutError) ||
			!ProjectTexturePatternManifestPrivate::ReadString(*Object, TEXT("output_object_path"), Record.OutputObjectPath, OutError) ||
			!ProjectTexturePatternManifestPrivate::ReadString(*Object, TEXT("semantic_identity"), Record.SemanticIdentity, OutError) ||
			!ProjectTexturePatternManifestPrivate::ReadString(*Object, TEXT("package_sha256"), Record.PackageSha256, OutError) ||
			!ProjectTexturePatternManifestPrivate::ReadString(*Object, TEXT("compiler_fingerprint"), Record.CompilerFingerprint, OutError) ||
			!ProjectTexturePatternManifestPrivate::ReadString(*Object, TEXT("engine_compatibility"), Record.EngineCompatibility, OutError))
		{
			if (OutError.IsEmpty())
			{
				OutError = TEXT("Pattern manifest record has an invalid shape.");
			}
			return false;
		}
		if (OutRecords.Contains(Record.OutputObjectPath))
		{
			OutError = FString::Printf(
				TEXT("Pattern manifest contains duplicate output identity: %s"), *Record.OutputObjectPath);
			return false;
		}
		OutRecords.Add(Record.OutputObjectPath, MoveTemp(Record));
	}
	return true;
}

FString ProjectTexturePatternManifest::Serialize(
	const TArray<FProjectTexturePatternManifestRecord>& Records)
{
	TArray<FProjectTexturePatternManifestRecord> Sorted = Records;
	Sorted.Sort([](const auto& Left, const auto& Right)
	{
		return Left.OutputObjectPath < Right.OutputObjectPath;
	});
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("$schema"), TEXT("../../Schemas/pattern-manifest.schema.json"));
	Root->SetNumberField(TEXT("schema_version"), 3);
	TArray<TSharedPtr<FJsonValue>> Values;
	for (const FProjectTexturePatternManifestRecord& Record : Sorted)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("recipe_path"), Record.RecipePath);
		Object->SetStringField(TEXT("recipe_sha256"), Record.RecipeSha256);
		Object->SetStringField(TEXT("recipe_source_sha256"), Record.RecipeSourceSha256);
		Object->SetStringField(TEXT("pattern_id"), Record.PatternId);
		Object->SetStringField(TEXT("family"), Record.Family);
		Object->SetStringField(TEXT("algorithm"), Record.Algorithm);
		Object->SetStringField(TEXT("compiler_version"), Record.CompilerVersion);
		Object->SetStringField(TEXT("output_contract"), Record.OutputContract);
		Object->SetStringField(TEXT("output_object_path"), Record.OutputObjectPath);
		Object->SetStringField(TEXT("semantic_identity"), Record.SemanticIdentity);
		Object->SetStringField(TEXT("package_sha256"), Record.PackageSha256);
		Object->SetStringField(TEXT("compiler_fingerprint"), Record.CompilerFingerprint);
		Object->SetStringField(TEXT("engine_compatibility"), Record.EngineCompatibility);
		Values.Add(MakeShared<FJsonValueObject>(Object));
	}
	Root->SetArrayField(TEXT("records"), Values);
	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Root, Writer);
	Json.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
	return Json + TEXT("\n");
}

bool ProjectTexturePatternManifest::Save(
	const FString& ManifestPath,
	const TArray<FProjectTexturePatternManifestRecord>& Records,
	FString& OutManifestSha256,
	FString& OutError)
{
	const FString Json = Serialize(Records);
	OutManifestSha256 = FProjectTexturePatternRecipeContract::ComputeStringSha256(Json);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ManifestPath), true);
	const FString StagingPath = ManifestPath + TEXT(".staging");
	if (!FFileHelper::SaveStringToFile(
		Json, *StagingPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) ||
		!IFileManager::Get().Move(*ManifestPath, *StagingPath, true, true, false, false))
	{
		IFileManager::Get().Delete(*StagingPath, false, true);
		OutError = FString::Printf(TEXT("Could not atomically save pattern manifest: %s"), *ManifestPath);
		return false;
	}
	return true;
}
