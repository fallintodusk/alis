// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectMaterialSurfaceManifest.h"

#include "ProjectMaterialCompilerIdentity.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace ProjectMaterialSurfaceManifestPrivate
{
	bool ReadString(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Name,
		FString& OutValue,
		FString& OutError)
	{
		if (!Object->TryGetStringField(Name, OutValue) || OutValue.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Surface manifest record is missing %s."), Name);
			return false;
		}
		return true;
	}

	bool ValidateFields(const TSharedPtr<FJsonObject>& Object, FString& OutError)
	{
		const TSet<FString> Allowed = {
			TEXT("recipe_path"), TEXT("recipe_sha256"), TEXT("recipe_source_sha256"),
			TEXT("binding_id"), TEXT("family"),
			TEXT("archetype"), TEXT("compiler_version"), TEXT("pattern_id"),
			TEXT("pattern_object_path"), TEXT("pattern_output_contract"),
			TEXT("pattern_semantic_identity"), TEXT("pattern_package_sha256"),
			TEXT("dependency_object_path"), TEXT("dependency_package_sha256"),
			TEXT("terrain_layout_receipt_sha256"), TEXT("output_object_path"),
			TEXT("semantic_identity"), TEXT("package_sha256"), TEXT("compiler_fingerprint"),
			TEXT("engine_compatibility")};
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Object->Values)
		{
			if (!Allowed.Contains(Field.Key))
			{
				OutError = FString::Printf(TEXT("Unknown surface manifest field: %s"), *Field.Key);
				return false;
			}
		}
		return true;
	}

	bool IsSha256(const FString& Value)
	{
		if (Value.Len() != 64)
		{
			return false;
		}
		for (const TCHAR Character : Value)
		{
			if (!FChar::IsHexDigit(Character) || FChar::IsUpper(Character))
			{
				return false;
			}
		}
		return true;
	}
}

bool ProjectMaterialSurfaceManifest::Load(
	const FString& ManifestPath,
	TMap<FString, FProjectMaterialSurfaceManifestRecord>& OutRecords,
	FString& OutError)
{
	using namespace ProjectMaterialSurfaceManifestPrivate;
	OutRecords.Reset();
	OutError.Reset();
	if (!FPaths::FileExists(ManifestPath))
	{
		return true;
	}
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *ManifestPath))
	{
		OutError = FString::Printf(TEXT("Could not read surface manifest: %s"), *ManifestPath);
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	double SchemaVersion = 0.0;
	FString Schema;
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid() || Root->Values.Num() != 3 ||
		!Root->TryGetStringField(TEXT("$schema"), Schema) ||
		Schema != TEXT("../../Schemas/surface-manifest.schema.json") ||
		!Root->TryGetNumberField(TEXT("schema_version"), SchemaVersion) || SchemaVersion != 3.0 ||
		!Root->TryGetArrayField(TEXT("records"), Values) || Values == nullptr)
	{
		OutError = TEXT("Surface manifest envelope is invalid.");
		return false;
	}
	TSet<FString> BindingIds;
	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		FProjectMaterialSurfaceManifestRecord Record;
		if (!Value->TryGetObject(Object) || Object == nullptr || !ValidateFields(*Object, OutError) ||
			!ReadString(*Object, TEXT("recipe_path"), Record.RecipePath, OutError) ||
			!ReadString(*Object, TEXT("recipe_sha256"), Record.RecipeSha256, OutError) ||
			!ReadString(*Object, TEXT("recipe_source_sha256"), Record.RecipeSourceSha256, OutError) ||
			!ReadString(*Object, TEXT("family"), Record.Family, OutError) ||
			!ReadString(*Object, TEXT("archetype"), Record.Archetype, OutError) ||
			!ReadString(*Object, TEXT("compiler_version"), Record.CompilerVersion, OutError) ||
			!ReadString(*Object, TEXT("pattern_id"), Record.PatternId, OutError) ||
			!ReadString(*Object, TEXT("pattern_object_path"), Record.PatternObjectPath, OutError) ||
			!ReadString(*Object, TEXT("pattern_output_contract"), Record.PatternOutputContract, OutError) ||
			!ReadString(*Object, TEXT("pattern_semantic_identity"), Record.PatternSemanticIdentity, OutError) ||
			!ReadString(*Object, TEXT("pattern_package_sha256"), Record.PatternPackageSha256, OutError) ||
			!ReadString(*Object, TEXT("output_object_path"), Record.OutputObjectPath, OutError) ||
			!ReadString(*Object, TEXT("semantic_identity"), Record.SemanticIdentity, OutError) ||
			!ReadString(*Object, TEXT("package_sha256"), Record.PackageSha256, OutError) ||
			!ReadString(*Object, TEXT("compiler_fingerprint"), Record.CompilerFingerprint, OutError) ||
			!ReadString(*Object, TEXT("engine_compatibility"), Record.EngineCompatibility, OutError))
		{
			return false;
		}
		(*Object)->TryGetStringField(TEXT("binding_id"), Record.BindingId);
		(*Object)->TryGetStringField(TEXT("dependency_object_path"), Record.DependencyObjectPath);
		(*Object)->TryGetStringField(TEXT("dependency_package_sha256"), Record.DependencyPackageSha256);
		(*Object)->TryGetStringField(
			TEXT("terrain_layout_receipt_sha256"), Record.TerrainLayoutReceiptSha256);
		if (!IsSha256(Record.RecipeSha256) || !IsSha256(Record.RecipeSourceSha256) ||
			!IsSha256(Record.PatternSemanticIdentity) ||
			!IsSha256(Record.PatternPackageSha256) || !IsSha256(Record.SemanticIdentity) ||
			!IsSha256(Record.PackageSha256) || !IsSha256(Record.CompilerFingerprint) ||
			(!Record.DependencyObjectPath.IsEmpty() != !Record.DependencyPackageSha256.IsEmpty()) ||
			(!Record.DependencyPackageSha256.IsEmpty() && !IsSha256(Record.DependencyPackageSha256)) ||
			(!Record.TerrainLayoutReceiptSha256.IsEmpty() && !IsSha256(Record.TerrainLayoutReceiptSha256)))
		{
			OutError = TEXT("Surface manifest record contains an invalid digest or dependency pair.");
			return false;
		}
		if (OutRecords.Contains(Record.OutputObjectPath) ||
			(!Record.BindingId.IsEmpty() && BindingIds.Contains(Record.BindingId)))
		{
			OutError = TEXT("Surface manifest contains a duplicate output or binding identity.");
			return false;
		}
		if (!Record.BindingId.IsEmpty())
		{
			BindingIds.Add(Record.BindingId);
		}
		OutRecords.Add(Record.OutputObjectPath, MoveTemp(Record));
	}
	return true;
}

FString ProjectMaterialSurfaceManifest::Serialize(
	const TArray<FProjectMaterialSurfaceManifestRecord>& Records)
{
	TArray<FProjectMaterialSurfaceManifestRecord> Sorted = Records;
	Sorted.Sort([](const auto& Left, const auto& Right)
	{
		return Left.OutputObjectPath < Right.OutputObjectPath;
	});
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("$schema"), TEXT("../../Schemas/surface-manifest.schema.json"));
	Root->SetNumberField(TEXT("schema_version"), 3);
	TArray<TSharedPtr<FJsonValue>> Values;
	for (const FProjectMaterialSurfaceManifestRecord& Record : Sorted)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("recipe_path"), Record.RecipePath);
		Object->SetStringField(TEXT("recipe_sha256"), Record.RecipeSha256);
		Object->SetStringField(TEXT("recipe_source_sha256"), Record.RecipeSourceSha256);
		if (!Record.BindingId.IsEmpty())
		{
			Object->SetStringField(TEXT("binding_id"), Record.BindingId);
		}
		Object->SetStringField(TEXT("family"), Record.Family);
		Object->SetStringField(TEXT("archetype"), Record.Archetype);
		Object->SetStringField(TEXT("compiler_version"), Record.CompilerVersion);
		Object->SetStringField(TEXT("pattern_id"), Record.PatternId);
		Object->SetStringField(TEXT("pattern_object_path"), Record.PatternObjectPath);
		Object->SetStringField(TEXT("pattern_output_contract"), Record.PatternOutputContract);
		Object->SetStringField(TEXT("pattern_semantic_identity"), Record.PatternSemanticIdentity);
		Object->SetStringField(TEXT("pattern_package_sha256"), Record.PatternPackageSha256);
		if (!Record.DependencyObjectPath.IsEmpty())
		{
			Object->SetStringField(TEXT("dependency_object_path"), Record.DependencyObjectPath);
			Object->SetStringField(TEXT("dependency_package_sha256"), Record.DependencyPackageSha256);
		}
		if (!Record.TerrainLayoutReceiptSha256.IsEmpty())
		{
			Object->SetStringField(
				TEXT("terrain_layout_receipt_sha256"), Record.TerrainLayoutReceiptSha256);
		}
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

bool ProjectMaterialSurfaceManifest::Save(
	const FString& ManifestPath,
	const TArray<FProjectMaterialSurfaceManifestRecord>& Records,
	FString& OutManifestSha256,
	FString& OutError)
{
	const FString Json = Serialize(Records);
	OutManifestSha256 = FProjectMaterialCompilerIdentity::ComputeStringSha256(Json);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ManifestPath), true);
	const FString StagingPath = ManifestPath + TEXT(".staging");
	if (!FFileHelper::SaveStringToFile(
		Json, *StagingPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) ||
		!IFileManager::Get().Move(*ManifestPath, *StagingPath, true, true, false, false))
	{
		IFileManager::Get().Delete(*StagingPath, false, true);
		OutError = FString::Printf(TEXT("Could not atomically save surface manifest: %s"), *ManifestPath);
		return false;
	}
	return true;
}
