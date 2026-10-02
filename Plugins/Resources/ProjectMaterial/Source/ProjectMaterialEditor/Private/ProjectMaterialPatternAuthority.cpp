// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectMaterialPatternAuthority.h"

#include "ProjectMaterialCompilerIdentity.h"
#include "Dom/JsonObject.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace ProjectMaterialPatternAuthorityPrivate
{
	bool ReadString(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Name,
		FString& OutValue,
		FString& OutError)
	{
		if (!Object.IsValid() || !Object->TryGetStringField(Name, OutValue) || OutValue.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Pattern manifest record is missing %s."), Name);
			return false;
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

	bool HashFile(const FString& Path, FString& OutHash)
	{
		TArray<uint8> Bytes;
		return FFileHelper::LoadFileToArray(Bytes, *Path) &&
			(OutHash = FProjectMaterialCompilerIdentity::ComputeSha256(Bytes)).Len() == 64;
	}
}

bool FProjectMaterialPatternAuthority::Resolve(
	const FString& PatternId,
	const FString& ManifestPath,
	const FString& PatternPackageRoot,
	const FString& PatternContentRoot,
	FProjectMaterialPatternDependency& OutDependency,
	FString& OutError)
{
	using namespace ProjectMaterialPatternAuthorityPrivate;
	OutDependency = {};
	OutError.Reset();
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *ManifestPath))
	{
		OutError = FString::Printf(TEXT("Accepted ProjectTexture pattern manifest is unreadable: %s"), *ManifestPath);
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	const TArray<TSharedPtr<FJsonValue>>* Records = nullptr;
	FString Schema;
	double SchemaVersion = 0.0;
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid() ||
		!Root->TryGetStringField(TEXT("$schema"), Schema) ||
		Schema != TEXT("../../Schemas/pattern-manifest.schema.json") ||
		!Root->TryGetNumberField(TEXT("schema_version"), SchemaVersion) ||
		SchemaVersion != 3.0 ||
		!Root->TryGetArrayField(TEXT("records"), Records) || Records == nullptr)
	{
		OutError = TEXT("Accepted ProjectTexture pattern manifest envelope is invalid.");
		return false;
	}

	TSharedPtr<FJsonObject> Match;
	for (const TSharedPtr<FJsonValue>& Value : *Records)
	{
		const TSharedPtr<FJsonObject>* Record = nullptr;
		FString RecordPatternId;
		FString OutputObjectPath;
		if (!Value->TryGetObject(Record) || Record == nullptr ||
			!ReadString(*Record, TEXT("pattern_id"), RecordPatternId, OutError) ||
			!ReadString(*Record, TEXT("output_object_path"), OutputObjectPath, OutError))
		{
			if (OutError.IsEmpty())
			{
				OutError = TEXT("Accepted ProjectTexture pattern manifest record is malformed.");
			}
			return false;
		}
		if (RecordPatternId == PatternId)
		{
			if (Match.IsValid())
			{
				OutError = TEXT("ProjectTexture pattern identity is duplicated in the accepted manifest.");
				return false;
			}
			Match = *Record;
		}
	}
	if (!Match.IsValid())
	{
		OutError = TEXT("Required ProjectTexture pattern is absent from the accepted manifest.");
		return false;
	}

	FString OutputContract;
	FString OutputObjectPath;
	FString SemanticIdentity;
	FString PackageSha256;
	FString EngineCompatibility;
	if (!ReadString(Match, TEXT("output_contract"), OutputContract, OutError) ||
		OutputContract != TEXT("surface_structure_rgb_v1") ||
		!ReadString(Match, TEXT("output_object_path"), OutputObjectPath, OutError) ||
		!ReadString(Match, TEXT("semantic_identity"), SemanticIdentity, OutError) ||
		!ReadString(Match, TEXT("package_sha256"), PackageSha256, OutError) ||
		!ReadString(Match, TEXT("engine_compatibility"), EngineCompatibility, OutError) ||
		!IsSha256(SemanticIdentity) || !IsSha256(PackageSha256))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Accepted ProjectTexture pattern authority does not satisfy the material output contract.");
		}
		return false;
	}
	const FEngineVersion Engine = FEngineVersion::Current();
	const FString ExpectedEngine = FString::Printf(TEXT("%d.%d"), Engine.GetMajor(), Engine.GetMinor());
	if (EngineCompatibility != ExpectedEngine)
	{
		OutError = FString::Printf(
			TEXT("ProjectTexture pattern targets UE %s, current compatibility is %s."),
			*EngineCompatibility, *ExpectedEngine);
		return false;
	}

	const FString Prefix = PatternPackageRoot + TEXT("/");
	const FString PackageName = FPackageName::ObjectPathToPackageName(OutputObjectPath);
	if (!PackageName.StartsWith(Prefix))
	{
		OutError = TEXT("ProjectTexture pattern object path escapes its owner mount.");
		return false;
	}
	const FString PackageFile = FPaths::Combine(
		PatternContentRoot,
		PackageName.RightChop(Prefix.Len()) + FPackageName::GetAssetPackageExtension());
	FString ActualPackageSha256;
	if (!HashFile(PackageFile, ActualPackageSha256) || ActualPackageSha256 != PackageSha256)
	{
		OutError = TEXT("ProjectTexture pattern bytes do not match the accepted manifest.");
		return false;
	}
	OutDependency.PatternId = PatternId;
	OutDependency.OutputContract = OutputContract;
	OutDependency.OutputObjectPath = OutputObjectPath;
	OutDependency.SemanticIdentity = SemanticIdentity;
	OutDependency.PackageSha256 = PackageSha256;
	return true;
}
