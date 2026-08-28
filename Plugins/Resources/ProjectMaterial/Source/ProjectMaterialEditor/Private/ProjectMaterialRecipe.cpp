// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectMaterialRecipe.h"

#include "Dom/JsonObject.h"
#include "Misc/EngineVersion.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace ProjectMaterialRecipePrivate
{
constexpr TCHAR SchemaVersion[] = TEXT("1");
constexpr TCHAR Family[] = TEXT("surface_opaque");
constexpr TCHAR Archetype[] = TEXT("landscape_basic_v1");
constexpr TCHAR CompilerVersion[] = TEXT("1");

uint32 RotateRight(uint32 Value, uint32 Count)
{
	return (Value >> Count) | (Value << (32U - Count));
}

FString Sha256(const TArrayView<const uint8> Bytes)
{
	static constexpr uint32 RoundConstants[64] = {
		0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
		0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
		0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
		0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
		0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
		0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
		0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
		0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};
	uint32 Hash[8] = {
		0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
		0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
	const uint64 BitLength = static_cast<uint64>(Bytes.Num()) * 8ULL;
	const int32 PaddedSize = Align(Bytes.Num() + 1 + 8, 64);
	TArray<uint8> Padded;
	Padded.SetNumZeroed(PaddedSize);
	if (!Bytes.IsEmpty())
	{
		FMemory::Memcpy(Padded.GetData(), Bytes.GetData(), Bytes.Num());
	}
	Padded[Bytes.Num()] = 0x80U;
	for (int32 Index = 0; Index < 8; ++Index)
	{
		Padded[PaddedSize - 1 - Index] = static_cast<uint8>(BitLength >> (Index * 8));
	}
	for (int32 Offset = 0; Offset < PaddedSize; Offset += 64)
	{
		uint32 Words[64] = {};
		for (int32 Index = 0; Index < 16; ++Index)
		{
			const int32 Base = Offset + Index * 4;
			Words[Index] =
				(static_cast<uint32>(Padded[Base]) << 24) |
				(static_cast<uint32>(Padded[Base + 1]) << 16) |
				(static_cast<uint32>(Padded[Base + 2]) << 8) |
				static_cast<uint32>(Padded[Base + 3]);
		}
		for (int32 Index = 16; Index < 64; ++Index)
		{
			const uint32 S0 = RotateRight(Words[Index - 15], 7) ^
				RotateRight(Words[Index - 15], 18) ^ (Words[Index - 15] >> 3);
			const uint32 S1 = RotateRight(Words[Index - 2], 17) ^
				RotateRight(Words[Index - 2], 19) ^ (Words[Index - 2] >> 10);
			Words[Index] = Words[Index - 16] + S0 + Words[Index - 7] + S1;
		}
		uint32 A = Hash[0];
		uint32 B = Hash[1];
		uint32 C = Hash[2];
		uint32 D = Hash[3];
		uint32 E = Hash[4];
		uint32 F = Hash[5];
		uint32 G = Hash[6];
		uint32 H = Hash[7];
		for (int32 Index = 0; Index < 64; ++Index)
		{
			const uint32 Sigma1 = RotateRight(E, 6) ^ RotateRight(E, 11) ^ RotateRight(E, 25);
			const uint32 Choice = (E & F) ^ ((~E) & G);
			const uint32 Temporary1 = H + Sigma1 + Choice + RoundConstants[Index] + Words[Index];
			const uint32 Sigma0 = RotateRight(A, 2) ^ RotateRight(A, 13) ^ RotateRight(A, 22);
			const uint32 Majority = (A & B) ^ (A & C) ^ (B & C);
			const uint32 Temporary2 = Sigma0 + Majority;
			H = G;
			G = F;
			F = E;
			E = D + Temporary1;
			D = C;
			C = B;
			B = A;
			A = Temporary1 + Temporary2;
		}
		Hash[0] += A;
		Hash[1] += B;
		Hash[2] += C;
		Hash[3] += D;
		Hash[4] += E;
		Hash[5] += F;
		Hash[6] += G;
		Hash[7] += H;
	}
	FString Result;
	Result.Reserve(64);
	for (const uint32 Word : Hash)
	{
		Result += FString::Printf(TEXT("%08x"), Word);
	}
	return Result;
}

bool IsSafeIdentifier(const FString& Value)
{
	if (Value.IsEmpty() || !FChar::IsAlpha(Value[0]))
	{
		return false;
	}
	for (const TCHAR Character : Value)
	{
		if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
		{
			return false;
		}
	}
	return true;
}

bool ValidateFields(
	const TSharedPtr<FJsonObject>& Object,
	const TSet<FString>& Allowed,
	const FString& Context,
	FString& OutError)
{
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Object->Values)
	{
		if (!Allowed.Contains(Field.Key))
		{
			OutError = FString::Printf(TEXT("Unknown %s field: %s"), *Context, *Field.Key);
			return false;
		}
	}
	return true;
}

bool ReadRequiredString(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* Name,
	FString& OutValue,
	FString& OutError)
{
	if (!Object->TryGetStringField(Name, OutValue) || OutValue.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Missing or invalid string field: %s"), Name);
		return false;
	}
	return true;
}

FString Number(double Value)
{
	return FString::Printf(TEXT("%.17g"), Value);
}

bool ReadScalars(
	const TSharedPtr<FJsonObject>& Root,
	TMap<FName, double>& OutScalars,
	FString& OutError)
{
	const TSharedPtr<FJsonObject>* Scalars = nullptr;
	if (!Root->TryGetObjectField(TEXT("scalars"), Scalars) || Scalars == nullptr)
	{
		OutError = TEXT("Missing object field: scalars");
		return false;
	}
	const TSet<FString> Allowed = {TEXT("Roughness"), TEXT("SlopeContrast")};
	if (!ValidateFields(*Scalars, Allowed, TEXT("scalar parameter"), OutError))
	{
		return false;
	}
	for (const FString& Name : Allowed)
	{
		double Value = 0.0;
		if (!(*Scalars)->TryGetNumberField(Name, Value) || !FMath::IsFinite(Value))
		{
			OutError = FString::Printf(TEXT("Missing or invalid scalar parameter: %s"), *Name);
			return false;
		}
		if ((Name == TEXT("Roughness") && (Value < 0.0 || Value > 1.0)) ||
			(Name == TEXT("SlopeContrast") && (Value < 0.01 || Value > 16.0)))
		{
			OutError = FString::Printf(TEXT("Scalar parameter out of range: %s"), *Name);
			return false;
		}
		OutScalars.Add(FName(Name), Value);
	}
	return true;
}

bool ReadVectors(
	const TSharedPtr<FJsonObject>& Root,
	TMap<FName, FLinearColor>& OutVectors,
	FString& OutError)
{
	const TSharedPtr<FJsonObject>* Vectors = nullptr;
	if (!Root->TryGetObjectField(TEXT("vectors"), Vectors) || Vectors == nullptr)
	{
		OutError = TEXT("Missing object field: vectors");
		return false;
	}
	const TSet<FString> Allowed = {TEXT("LowSlopeColor"), TEXT("SteepSlopeColor")};
	if (!ValidateFields(*Vectors, Allowed, TEXT("vector parameter"), OutError))
	{
		return false;
	}
	for (const FString& Name : Allowed)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!(*Vectors)->TryGetArrayField(Name, Values) || Values == nullptr || Values->Num() != 4)
		{
			OutError = FString::Printf(TEXT("Vector parameter must contain four numbers: %s"), *Name);
			return false;
		}
		double Components[4] = {};
		for (int32 Index = 0; Index < 4; ++Index)
		{
			if (!(*Values)[Index]->TryGetNumber(Components[Index]) ||
				!FMath::IsFinite(Components[Index]) || Components[Index] < 0.0 || Components[Index] > 1.0)
			{
				OutError = FString::Printf(TEXT("Vector component out of range: %s"), *Name);
				return false;
			}
		}
		OutVectors.Add(FName(Name), FLinearColor(
			Components[0], Components[1], Components[2], Components[3]));
	}
	return true;
}

FString Normalize(const FProjectMaterialRecipe& Recipe)
{
	TArray<FName> ScalarNames;
	Recipe.Scalars.GetKeys(ScalarNames);
	ScalarNames.Sort(FNameLexicalLess());
	TArray<FName> VectorNames;
	Recipe.Vectors.GetKeys(VectorNames);
	VectorNames.Sort(FNameLexicalLess());
	FString Result = FString::Printf(
		TEXT("schema=1|id=%s|kind=%s|family=%s|archetype=%s|compiler=%s|parent=%s|folder=%s"),
		*Recipe.MaterialId,
		Recipe.ArtifactKind == EProjectMaterialArtifactKind::Parent ? TEXT("parent") : TEXT("instance"),
		*Recipe.Family,
		*Recipe.Archetype,
		*Recipe.CompilerVersion,
		*Recipe.ParentObjectPath,
		*Recipe.RelativeFolder);
	for (const FName Name : ScalarNames)
	{
		Result += FString::Printf(TEXT("|s:%s=%s"), *Name.ToString(), *Number(Recipe.Scalars[Name]));
	}
	for (const FName Name : VectorNames)
	{
		const FLinearColor Value = Recipe.Vectors[Name];
		Result += FString::Printf(TEXT("|v:%s=%s,%s,%s,%s"),
			*Name.ToString(), *Number(Value.R), *Number(Value.G), *Number(Value.B), *Number(Value.A));
	}
	return Result;
}
}

bool FProjectMaterialRecipeContract::Parse(
	const FString& Json,
	const FString& SourcePath,
	const FString& RecipeRoot,
	FProjectMaterialRecipe& OutRecipe,
	FString& OutError)
{
	using namespace ProjectMaterialRecipePrivate;
	OutRecipe = {};
	OutError.Reset();
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("Recipe is not valid JSON.");
		return false;
	}
	const TSet<FString> Allowed = {
		TEXT("$schema"), TEXT("schema_version"), TEXT("material_id"), TEXT("artifact_kind"),
		TEXT("family"), TEXT("archetype"), TEXT("compiler_version"), TEXT("parent"),
		TEXT("scalars"), TEXT("vectors")};
	if (!ValidateFields(Root, Allowed, TEXT("recipe"), OutError))
	{
		return false;
	}
	FString SchemaPath;
	FString Schema;
	FString Kind;
	if (!ReadRequiredString(Root, TEXT("$schema"), SchemaPath, OutError) ||
		!ReadRequiredString(Root, TEXT("schema_version"), Schema, OutError) ||
		!ReadRequiredString(Root, TEXT("material_id"), OutRecipe.MaterialId, OutError) ||
		!ReadRequiredString(Root, TEXT("artifact_kind"), Kind, OutError) ||
		!ReadRequiredString(Root, TEXT("family"), OutRecipe.Family, OutError) ||
		!ReadRequiredString(Root, TEXT("archetype"), OutRecipe.Archetype, OutError) ||
		!ReadRequiredString(Root, TEXT("compiler_version"), OutRecipe.CompilerVersion, OutError))
	{
		return false;
	}
	if (SchemaPath != TEXT("../../Schemas/material-recipe.schema.json") ||
		Schema != SchemaVersion || OutRecipe.Family != Family ||
		OutRecipe.Archetype != Archetype || OutRecipe.CompilerVersion != CompilerVersion)
	{
		OutError = TEXT("Recipe uses an unsupported schema, family, archetype, or compiler version.");
		return false;
	}
	if (!IsSafeIdentifier(OutRecipe.MaterialId) ||
		(!OutRecipe.MaterialId.StartsWith(TEXT("M_")) && !OutRecipe.MaterialId.StartsWith(TEXT("MI_"))))
	{
		OutError = TEXT("material_id must be a safe M_ or MI_ identifier.");
		return false;
	}
	if (Kind == TEXT("parent"))
	{
		OutRecipe.ArtifactKind = EProjectMaterialArtifactKind::Parent;
		if (!OutRecipe.MaterialId.StartsWith(TEXT("M_")) || OutRecipe.MaterialId.StartsWith(TEXT("MI_")) ||
			Root->HasField(TEXT("parent")))
		{
			OutError = TEXT("Parent recipes require an M_ identity and no parent field.");
			return false;
		}
	}
	else if (Kind == TEXT("instance"))
	{
		OutRecipe.ArtifactKind = EProjectMaterialArtifactKind::Instance;
		if (!OutRecipe.MaterialId.StartsWith(TEXT("MI_")) ||
			!ReadRequiredString(Root, TEXT("parent"), OutRecipe.ParentObjectPath, OutError))
		{
			OutError = TEXT("Instance recipes require an MI_ identity and parent path.");
			return false;
		}
	}
	else
	{
		OutError = TEXT("artifact_kind must be parent or instance.");
		return false;
	}
	FString AbsoluteRoot = FPaths::ConvertRelativePathToFull(RecipeRoot);
	FPaths::NormalizeDirectoryName(AbsoluteRoot);
	AbsoluteRoot += TEXT("/");
	const FString AbsoluteSource = FPaths::ConvertRelativePathToFull(SourcePath);
	FString RelativePath = AbsoluteSource;
	if (!FPaths::MakePathRelativeTo(RelativePath, *AbsoluteRoot) ||
		RelativePath.StartsWith(TEXT("..")) || !RelativePath.EndsWith(TEXT(".material.json")))
	{
		OutError = TEXT("Recipe source escapes the configured recipe root.");
		return false;
	}
	OutRecipe.RelativeFolder = FPaths::GetPath(RelativePath).Replace(TEXT("\\"), TEXT("/"));
	if (OutRecipe.RelativeFolder.IsEmpty() || OutRecipe.RelativeFolder.Contains(TEXT("..")))
	{
		OutError = TEXT("Recipe must live in a named family folder.");
		return false;
	}
	OutRecipe.SourcePath = RelativePath.Replace(TEXT("\\"), TEXT("/"));
	if (!ReadScalars(Root, OutRecipe.Scalars, OutError) ||
		!ReadVectors(Root, OutRecipe.Vectors, OutError))
	{
		return false;
	}
	OutRecipe.NormalizedSemantics = Normalize(OutRecipe);
	OutRecipe.RecipeSha256 = ComputeStringSha256(OutRecipe.NormalizedSemantics);
	return !OutRecipe.RecipeSha256.IsEmpty();
}

FString FProjectMaterialRecipeContract::ComputeArtifactSemanticIdentity(
	const FProjectMaterialRecipe& Recipe,
	const FString& OutputObjectPath,
	const FString& DependencyPackageSha256)
{
	return ComputeStringSha256(FString::Printf(
		TEXT("recipe=%s|family=%s|archetype=%s|compiler=%s|dependency=%s|dependency_sha=%s|output=%s|engine=%s|fingerprint=%s"),
		*Recipe.RecipeSha256,
		*Recipe.Family,
		*Recipe.Archetype,
		*Recipe.CompilerVersion,
		*Recipe.ParentObjectPath,
		*DependencyPackageSha256,
		*OutputObjectPath,
		*GetEngineIdentity(),
		*GetCompilerFingerprint()));
}

bool FProjectMaterialRecipeContract::ResolveOutputIdentity(
	const FProjectMaterialRecipe& Recipe,
	const FString& OutputPackageRoot,
	FString& OutPackageName,
	FString& OutObjectPath,
	FString& OutError)
{
	OutError.Reset();
	if (!OutputPackageRoot.StartsWith(TEXT("/")) || OutputPackageRoot.Contains(TEXT("..")) ||
		OutputPackageRoot.EndsWith(TEXT("/")) || Recipe.RelativeFolder.Contains(TEXT("..")))
	{
		OutError = TEXT("Output package root or recipe folder is invalid.");
		return false;
	}
	OutPackageName = FString::Printf(
		TEXT("%s/%s/%s"), *OutputPackageRoot, *Recipe.RelativeFolder, *Recipe.MaterialId);
	OutObjectPath = FString::Printf(TEXT("%s.%s"), *OutPackageName, *Recipe.MaterialId);
	if (!OutPackageName.StartsWith(OutputPackageRoot + TEXT("/")))
	{
		OutError = TEXT("Derived output escapes the configured package root.");
		return false;
	}
	return true;
}

FString FProjectMaterialRecipeContract::ComputeSha256(const TArrayView<const uint8> Bytes)
{
	return ProjectMaterialRecipePrivate::Sha256(Bytes);
}

FString FProjectMaterialRecipeContract::ComputeStringSha256(const FString& Value)
{
	FTCHARToUTF8 Utf8(*Value);
	return ComputeSha256(MakeArrayView(
		reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length()));
}

FString FProjectMaterialRecipeContract::GetEngineIdentity()
{
	const FEngineVersion Version = FEngineVersion::Current();
	return FString::Printf(TEXT("%s|changelist=%u"), *Version.ToString(), Version.GetChangelist());
}

FString FProjectMaterialRecipeContract::GetCompilerFingerprint()
{
#ifndef PROJECT_MATERIAL_COMPILER_SOURCE_SHA256
#error ProjectMaterialEditor must bind its compiler fingerprint to the admitted source set.
#endif
	return FString(UTF8_TO_TCHAR(PROJECT_MATERIAL_COMPILER_SOURCE_SHA256));
}
