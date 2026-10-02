// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectTexturePatternRecipe.h"

#include "Dom/JsonObject.h"
#include "Misc/EngineVersion.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Utilities/ProjectSha256.h"

namespace ProjectTexturePatternRecipePrivate
{
constexpr TCHAR SchemaVersion[] = TEXT("3");
constexpr TCHAR Family[] = TEXT("surface_structure");
constexpr TCHAR Algorithm[] = TEXT("runtime_dag_native");
constexpr TCHAR CompilerVersion[] = TEXT("5");
constexpr TCHAR OutputContract[] = TEXT("surface_structure_rgb_v1");

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

bool ReadNodes(
	const TSharedPtr<FJsonObject>& Root,
	int32 Seed,
	const FString& ResolutionClass,
	TArray<FProjectTexturePatternNode>& OutNodes,
	FString& OutError)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Root->TryGetArrayField(TEXT("nodes"), Values) || Values == nullptr || Values->IsEmpty())
	{
		OutError = TEXT("nodes must contain at least one runtime node.");
		return false;
	}
	TMap<FString, FString> Identities;
	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (!Value->TryGetObject(Object) || Object == nullptr ||
			!ValidateFields(*Object,
				{TEXT("node_id"), TEXT("algorithm"), TEXT("parents"), TEXT("output_layout")},
				TEXT("runtime node"), OutError))
		{
			if (OutError.IsEmpty())
			{
				OutError = TEXT("Runtime node must be an object.");
			}
			return false;
		}
		FProjectTexturePatternNode Node;
		const TArray<TSharedPtr<FJsonValue>>* Parents = nullptr;
		if (!ReadRequiredString(*Object, TEXT("node_id"), Node.NodeId, OutError) ||
			!ReadRequiredString(*Object, TEXT("algorithm"), Node.Algorithm, OutError) ||
			!ReadRequiredString(*Object, TEXT("output_layout"), Node.OutputLayout, OutError) ||
			!(*Object)->TryGetArrayField(TEXT("parents"), Parents) || Parents == nullptr ||
			!IsSafeIdentifier(Node.NodeId) || Identities.Contains(Node.NodeId))
		{
			if (OutError.IsEmpty())
			{
				OutError = TEXT("Runtime node identity, layout, or parents are invalid.");
			}
			return false;
		}
		for (const TSharedPtr<FJsonValue>& ParentValue : *Parents)
		{
			FString Parent;
			if (!ParentValue->TryGetString(Parent) || !Identities.Contains(Parent) ||
				Node.Parents.Contains(Parent))
			{
				OutError = TEXT("Runtime nodes must be unique and listed after every parent.");
				return false;
			}
			Node.Parents.Add(Parent);
		}
		const bool bBasis = Node.Algorithm == TEXT("natural_noise_basis_native") &&
			Node.OutputLayout == TEXT("natural_noise_basis_rgb_v1") && Node.Parents.IsEmpty();
		const bool bGround = Node.Algorithm == TEXT("natural_ground_structure_native") &&
			Node.OutputLayout == TEXT("surface_structure_rgb_v1") && Node.Parents.Num() == 1;
		if (!bBasis && !bGround)
		{
			OutError = TEXT("Runtime node uses an unsupported algorithm, layout, or parent cardinality.");
			return false;
		}
		FString ParentIdentities;
		for (const FString& Parent : Node.Parents)
		{
			ParentIdentities += TEXT(":") + Identities[Parent];
		}
		Node.StructuralIdentity = FProjectTexturePatternRecipeContract::ComputeStringSha256(
			FString::Printf(TEXT("node=%s|algorithm=%s|layout=%s|seed=%d|resolution=%s|parents=%s"),
				*Node.NodeId, *Node.Algorithm, *Node.OutputLayout, Seed, *ResolutionClass,
				*ParentIdentities));
		Identities.Add(Node.NodeId, Node.StructuralIdentity);
		OutNodes.Add(MoveTemp(Node));
	}
	return true;
}

bool ReadExports(
	const TSharedPtr<FJsonObject>& Root,
	const TArray<FProjectTexturePatternNode>& Nodes,
	TArray<FProjectTexturePatternExport>& OutExports,
	FString& OutError)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Root->TryGetArrayField(TEXT("exports"), Values) || Values == nullptr || Values->Num() != 1)
	{
		OutError = TEXT("The first runtime output bundle requires exactly one Structure export.");
		return false;
	}
	const TSharedPtr<FJsonObject>* Object = nullptr;
	FProjectTexturePatternExport Export;
	if (!(*Values)[0]->TryGetObject(Object) || Object == nullptr ||
		!ValidateFields(*Object, {TEXT("slot"), TEXT("node_id"), TEXT("descriptor_id")},
			TEXT("runtime export"), OutError) ||
		!ReadRequiredString(*Object, TEXT("slot"), Export.Slot, OutError) ||
		!ReadRequiredString(*Object, TEXT("node_id"), Export.NodeId, OutError) ||
		!ReadRequiredString(*Object, TEXT("descriptor_id"), Export.DescriptorId, OutError))
	{
		return false;
	}
	const FProjectTexturePatternNode* Node = Nodes.FindByPredicate(
		[&Export](const FProjectTexturePatternNode& Candidate)
		{
			return Candidate.NodeId == Export.NodeId;
		});
	if (Export.Slot != TEXT("Structure") || Node == nullptr ||
		Node->OutputLayout != TEXT("surface_structure_rgb_v1") ||
		!IsSafeIdentifier(Export.DescriptorId) || !Export.DescriptorId.StartsWith(TEXT("RT_")))
	{
		OutError = TEXT("Structure export must target a surface_structure_rgb_v1 node and RT_ descriptor.");
		return false;
	}
	OutExports.Add(MoveTemp(Export));
	return true;
}

FString Normalize(const FProjectTexturePatternRecipe& Recipe)
{
	FString Result = FString::Printf(
		TEXT("schema=3|pattern=%s|catalog=%s|family=%s|algorithm=%s|compiler=%s|")
		TEXT("output_contract=%s|seed=%d|resolution=%s|folder=%s"),
		*Recipe.PatternId, *Recipe.CatalogId, *Recipe.Family, *Recipe.Algorithm,
		*Recipe.CompilerVersion, *Recipe.OutputContract, Recipe.Seed,
		*Recipe.ResolutionClass, *Recipe.RelativeFolder);
	for (const FProjectTexturePatternNode& Node : Recipe.Nodes)
	{
		Result += FString::Printf(TEXT("|node:%s:%s:%s:%s"), *Node.NodeId, *Node.Algorithm,
			*FString::Join(Node.Parents, TEXT(",")), *Node.OutputLayout);
	}
	for (const FProjectTexturePatternExport& Export : Recipe.Exports)
	{
		Result += FString::Printf(TEXT("|export:%s:%s:%s"), *Export.Slot,
			*Export.NodeId, *Export.DescriptorId);
	}
	return Result;
}
}

bool FProjectTexturePatternRecipeContract::Parse(
	const FString& Json,
	const FString& SourcePath,
	const FString& RecipeRoot,
	FProjectTexturePatternRecipe& OutRecipe,
	FString& OutError)
{
	using namespace ProjectTexturePatternRecipePrivate;
	OutRecipe = {};
	OutError.Reset();
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("Pattern recipe is not valid JSON.");
		return false;
	}
	if (!ValidateFields(Root,
		{TEXT("$schema"), TEXT("schema_version"), TEXT("pattern_id"), TEXT("catalog_id"),
		 TEXT("family"), TEXT("compiler_version"), TEXT("output_contract"), TEXT("seed"),
		 TEXT("resolution_class"), TEXT("nodes"), TEXT("exports")},
		TEXT("pattern recipe"), OutError))
	{
		return false;
	}
	FString SchemaPath;
	FString Schema;
	double Seed = -1.0;
	if (!ReadRequiredString(Root, TEXT("$schema"), SchemaPath, OutError) ||
		!ReadRequiredString(Root, TEXT("schema_version"), Schema, OutError) ||
		!ReadRequiredString(Root, TEXT("pattern_id"), OutRecipe.PatternId, OutError) ||
		!ReadRequiredString(Root, TEXT("catalog_id"), OutRecipe.CatalogId, OutError) ||
		!ReadRequiredString(Root, TEXT("family"), OutRecipe.Family, OutError) ||
		!ReadRequiredString(Root, TEXT("compiler_version"), OutRecipe.CompilerVersion, OutError) ||
		!ReadRequiredString(Root, TEXT("output_contract"), OutRecipe.OutputContract, OutError) ||
		!ReadRequiredString(Root, TEXT("resolution_class"), OutRecipe.ResolutionClass, OutError) ||
		!Root->TryGetNumberField(TEXT("seed"), Seed) || !FMath::IsFinite(Seed) ||
		Seed < 0.0 || Seed > 2147483647.0 || Seed != FMath::FloorToDouble(Seed))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("seed must be an integer in the signed positive range.");
		}
		return false;
	}
	if (SchemaPath != TEXT("../../Schemas/pattern-recipe.schema.json") ||
		Schema != SchemaVersion || OutRecipe.Family != Family ||
		OutRecipe.CompilerVersion != CompilerVersion || OutRecipe.OutputContract != OutputContract ||
		!IsSafeIdentifier(OutRecipe.PatternId) || !IsSafeIdentifier(OutRecipe.CatalogId) ||
		!OutRecipe.CatalogId.StartsWith(TEXT("DA_")) ||
		(OutRecipe.ResolutionClass != TEXT("low") && OutRecipe.ResolutionClass != TEXT("medium") &&
		 OutRecipe.ResolutionClass != TEXT("high")))
	{
		OutError = TEXT("Pattern recipe uses an unsupported schema, identity, family, output, or resolution.");
		return false;
	}
	OutRecipe.Algorithm = Algorithm;
	OutRecipe.Seed = static_cast<int32>(Seed);
	if (!ReadNodes(Root, OutRecipe.Seed, OutRecipe.ResolutionClass, OutRecipe.Nodes, OutError) ||
		!ReadExports(Root, OutRecipe.Nodes, OutRecipe.Exports, OutError))
	{
		return false;
	}

	FString AbsoluteRoot = FPaths::ConvertRelativePathToFull(RecipeRoot);
	FPaths::NormalizeDirectoryName(AbsoluteRoot);
	AbsoluteRoot += TEXT("/");
	FString RelativePath = FPaths::ConvertRelativePathToFull(SourcePath);
	if (!FPaths::MakePathRelativeTo(RelativePath, *AbsoluteRoot) ||
		RelativePath.StartsWith(TEXT("..")) || !RelativePath.EndsWith(TEXT(".pattern.json")))
	{
		OutError = TEXT("Pattern recipe source escapes the configured recipe root.");
		return false;
	}
	OutRecipe.RelativeFolder = FPaths::GetPath(RelativePath).Replace(TEXT("\\"), TEXT("/"));
	if (OutRecipe.RelativeFolder.IsEmpty() || OutRecipe.RelativeFolder.Contains(TEXT("..")))
	{
		OutError = TEXT("Pattern recipe must live in a named concern folder.");
		return false;
	}
	OutRecipe.SourcePath = RelativePath.Replace(TEXT("\\"), TEXT("/"));
	OutRecipe.NormalizedSemantics = Normalize(OutRecipe);
	OutRecipe.RecipeSha256 = ComputeStringSha256(OutRecipe.NormalizedSemantics);
	return OutRecipe.RecipeSha256.Len() == 64 &&
		FProjectSha256::HashNormalizedText(Json, OutRecipe.RecipeSourceSha256);
}

bool FProjectTexturePatternRecipeContract::ResolveOutputIdentity(
	const FProjectTexturePatternRecipe& Recipe,
	const FString& OutputPackageRoot,
	FString& OutPackageName,
	FString& OutObjectPath,
	FString& OutError)
{
	OutError.Reset();
	if (!OutputPackageRoot.StartsWith(TEXT("/")) || OutputPackageRoot.Contains(TEXT("..")) ||
		OutputPackageRoot.EndsWith(TEXT("/")) || Recipe.RelativeFolder.Contains(TEXT("..")) ||
		Recipe.Exports.Num() != 1)
	{
		OutError = TEXT("Output package root, recipe folder, or export bundle is invalid.");
		return false;
	}
	OutPackageName = FString::Printf(
		TEXT("%s/%s/%s"), *OutputPackageRoot, *Recipe.RelativeFolder, *Recipe.CatalogId);
	OutObjectPath = FString::Printf(
		TEXT("%s.%s"), *OutPackageName, *Recipe.Exports[0].DescriptorId);
	if (!OutPackageName.StartsWith(OutputPackageRoot + TEXT("/")))
	{
		OutError = TEXT("Derived pattern output escapes the configured package root.");
		return false;
	}
	return true;
}

FString FProjectTexturePatternRecipeContract::ComputeArtifactSemanticIdentity(
	const FProjectTexturePatternRecipe& Recipe,
	const FString& OutputObjectPath)
{
	return ComputeStringSha256(FString::Printf(
		TEXT("recipe=%s|family=%s|algorithm=%s|compiler=%s|output_contract=%s|")
		TEXT("output=%s|engine=%s|fingerprint=%s"),
		*Recipe.RecipeSha256, *Recipe.Family, *Recipe.Algorithm, *Recipe.CompilerVersion,
		*Recipe.OutputContract, *OutputObjectPath, *GetEngineCompatibilityIdentity(),
		*GetCompilerFingerprint()));
}

FString FProjectTexturePatternRecipeContract::ComputeStringSha256(const FString& Value)
{
	FTCHARToUTF8 Utf8(*Value);
	TArray<uint8> Bytes;
	Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	FString Hash;
	return FProjectSha256::HashBuffer(Bytes, Hash) ? Hash : FString();
}

FString FProjectTexturePatternRecipeContract::GetEngineCompatibilityIdentity()
{
	const FEngineVersion Version = FEngineVersion::Current();
	return FString::Printf(TEXT("%d.%d"), Version.GetMajor(), Version.GetMinor());
}

FString FProjectTexturePatternRecipeContract::GetCompilerFingerprint()
{
#ifndef PROJECT_TEXTURE_COMPILER_SOURCE_SHA256
#error ProjectTextureEditor must bind its compiler fingerprint to the admitted source set.
#endif
	return FString(UTF8_TO_TCHAR(PROJECT_TEXTURE_COMPILER_SOURCE_SHA256));
}
