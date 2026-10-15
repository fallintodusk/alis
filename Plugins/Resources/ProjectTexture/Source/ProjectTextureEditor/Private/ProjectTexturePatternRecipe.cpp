// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectTexturePatternRecipe.h"
#include "ProjectTextureGradedParticleControls.h"
#include "ProjectTextureParticleControls.h"

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
	bool bSoil,
	bool bAggregate,
	bool bGraded,
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
		const bool bSoilNode = bSoil && (
			(Node.Algorithm == TEXT("soil_clump_structure_native") && Node.OutputLayout == TEXT("soil_height_clump_grain_rgb_v1") && Node.Parents.IsEmpty()) ||
			(Node.Algorithm == TEXT("soil_normal_native") && Node.OutputLayout == TEXT("soil_normal_rgb_v1") && Node.Parents.Num() == 1) ||
			(Node.Algorithm == TEXT("soil_masks_native") && Node.OutputLayout == TEXT("soil_cavity_roughness_pigment_rgb_v1") && Node.Parents.Num() == 1));
		const bool bAggregateNode=bAggregate && ((Node.Algorithm==TEXT("particle_signals_native") && Node.OutputLayout==TEXT("aggregate_height_pigment_roughness_rgb_v1") && Node.Parents.IsEmpty()) ||
			(Node.Algorithm==TEXT("aggregate_normal_native") && Node.OutputLayout==TEXT("aggregate_normal_rgb_v1") && Node.Parents.Num()==1));
		const bool bGradedNode=bGraded && (
			(Node.Algorithm==TEXT("graded_particles_native") && Node.OutputLayout==TEXT("soil_raw_height_occupancy_grain_rgb_v1") && Node.Parents.IsEmpty()) ||
			(Node.Algorithm==TEXT("graded_filter_native") && Node.OutputLayout==TEXT("soil_height_clump_grain_rgb_v1") && Node.Parents.Num()==1) ||
			(Node.Algorithm==TEXT("soil_normal_native") && Node.OutputLayout==TEXT("soil_normal_rgb_v1") && Node.Parents.Num()==1) ||
			(Node.Algorithm==TEXT("soil_masks_native") && Node.OutputLayout==TEXT("soil_cavity_roughness_pigment_rgb_v1") && Node.Parents.Num()==1));
		if (bGraded ? !bGradedNode : bAggregate ? !bAggregateNode : bSoil ? !bSoilNode : (!bBasis && !bGround))
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
	bool bSoil,
	bool bAggregate,
	bool bGraded,
	TArray<FProjectTexturePatternExport>& OutExports,
	FString& OutError)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	const TArray<FString> Slots = bAggregate ? TArray<FString>{TEXT("Signals"),TEXT("Normal")} : (bSoil || bGraded) ? TArray<FString>{TEXT("Structure"), TEXT("Normal"), TEXT("Masks")}
		: TArray<FString>{TEXT("Structure")};
	const TArray<FString> Layouts = bAggregate ? TArray<FString>{TEXT("aggregate_height_pigment_roughness_rgb_v1"),TEXT("aggregate_normal_rgb_v1")} : (bSoil || bGraded) ? TArray<FString>{TEXT("soil_height_clump_grain_rgb_v1"),
		TEXT("soil_normal_rgb_v1"), TEXT("soil_cavity_roughness_pigment_rgb_v1")}
		: TArray<FString>{TEXT("surface_structure_rgb_v1")};
	if (!Root->TryGetArrayField(TEXT("exports"), Values) || !Values || Values->Num() != Slots.Num())
	{
		OutError = TEXT("Runtime export bundle has the wrong cardinality.");
		return false;
	}
	TSet<FString> Descriptors;
	for (int32 Index = 0; Index < Values->Num(); ++Index)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		FProjectTexturePatternExport Export;
		if (!(*Values)[Index]->TryGetObject(Object) || !Object ||
			!ValidateFields(*Object, {TEXT("slot"), TEXT("node_id"), TEXT("descriptor_id")}, TEXT("runtime export"), OutError) ||
			!ReadRequiredString(*Object, TEXT("slot"), Export.Slot, OutError) ||
			!ReadRequiredString(*Object, TEXT("node_id"), Export.NodeId, OutError) ||
			!ReadRequiredString(*Object, TEXT("descriptor_id"), Export.DescriptorId, OutError)) return false;
		const FProjectTexturePatternNode* Node = Nodes.FindByPredicate(
			[&Export](const auto& Candidate) { return Candidate.NodeId == Export.NodeId; });
		if (Export.Slot != Slots[Index] || !Node || Node->OutputLayout != Layouts[Index] ||
			!IsSafeIdentifier(Export.DescriptorId) || !Export.DescriptorId.StartsWith(TEXT("RT_")) ||
			Descriptors.Contains(Export.DescriptorId))
		{
			OutError = TEXT("Export slot, node layout, or descriptor is invalid.");
			return false;
		}
		Descriptors.Add(Export.DescriptorId);
		OutExports.Add(MoveTemp(Export));
	}
	if (bSoil && (Nodes.Num() != 3 || Nodes[0].OutputLayout != Layouts[0] ||
		Nodes[1].OutputLayout != Layouts[1] || Nodes[2].OutputLayout != Layouts[2] ||
		OutExports[0].NodeId != Nodes[0].NodeId || OutExports[1].NodeId != Nodes[1].NodeId || OutExports[2].NodeId != Nodes[2].NodeId ||
		Nodes[1].Parents != TArray<FString>{Nodes[0].NodeId} || Nodes[2].Parents != TArray<FString>{Nodes[0].NodeId}))
	{
		OutError = TEXT("Soil requires one coherent structure parent and its two derived outputs.");
		return false;
	}
	if (bAggregate && (Nodes.Num()!=2 || Nodes[0].OutputLayout!=Layouts[0] || Nodes[1].OutputLayout!=Layouts[1] ||
		Nodes[0].NodeId!=TEXT("aggregate_signals") || Nodes[1].NodeId!=TEXT("aggregate_normal") ||
		OutExports[0].NodeId!=Nodes[0].NodeId || OutExports[1].NodeId!=Nodes[1].NodeId || Nodes[1].Parents!=TArray<FString>{Nodes[0].NodeId}))
	{
		OutError=TEXT("Aggregate requires a signals parent and its coherent normal output."); return false;
	}
	if (bGraded && (Nodes.Num()!=4 || Nodes[0].Algorithm!=TEXT("graded_particles_native") ||
		Nodes[1].Algorithm!=TEXT("graded_filter_native") || Nodes[2].Algorithm!=TEXT("soil_normal_native") ||
		Nodes[3].Algorithm!=TEXT("soil_masks_native") || Nodes[1].Parents!=TArray<FString>{Nodes[0].NodeId} ||
		Nodes[2].Parents!=TArray<FString>{Nodes[1].NodeId} || Nodes[3].Parents!=TArray<FString>{Nodes[1].NodeId} ||
		OutExports[0].NodeId!=Nodes[1].NodeId || OutExports[1].NodeId!=Nodes[2].NodeId || OutExports[2].NodeId!=Nodes[3].NodeId))
	{ OutError=TEXT("Graded particles require a private raw stage and three coherent filtered exports."); return false; }
	return true;
}

FString Normalize(const FProjectTexturePatternRecipe& Recipe)
{
	FString Result = FString::Printf(
		TEXT("schema=%s|pattern=%s|catalog=%s|family=%s|algorithm=%s|compiler=%s|")
		TEXT("output_contract=%s|seed=%d|resolution=%s|folder=%s"),
		*Recipe.SchemaVersion, *Recipe.PatternId, *Recipe.CatalogId, *Recipe.Family, *Recipe.Algorithm,
		*Recipe.CompilerVersion, *Recipe.OutputContract, Recipe.Seed,
		*Recipe.ResolutionClass, *Recipe.RelativeFolder);
	Result += ProjectTextureParticleControls::Normalize(Recipe.StructureControls);
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
		 TEXT("resolution_class"), TEXT("nodes"), TEXT("exports"), TEXT("structure")},
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
	const bool bSoil = Schema == TEXT("4") && OutRecipe.Family == TEXT("soil_structure") &&
		OutRecipe.CompilerVersion == TEXT("6") && OutRecipe.OutputContract == TEXT("soil_structure_normal_masks_v1");
	const bool bTerrain = Schema == SchemaVersion && OutRecipe.Family == Family &&
		OutRecipe.CompilerVersion == CompilerVersion && OutRecipe.OutputContract == OutputContract;
	const bool bAggregate=Schema==TEXT("6") && OutRecipe.Family==TEXT("aggregate_surface") && OutRecipe.CompilerVersion==TEXT("8") &&
		OutRecipe.OutputContract==TEXT("aggregate_signals_normal_v1") && Seed<=255 && OutRecipe.ResolutionClass==TEXT("medium");
	const bool bGraded=Schema==TEXT("7") && OutRecipe.Family==TEXT("graded_particles") && OutRecipe.CompilerVersion==TEXT("9") &&
		OutRecipe.OutputContract==TEXT("soil_structure_normal_masks_v1") && Seed<=255 && OutRecipe.ResolutionClass==TEXT("medium");
	if (SchemaPath != TEXT("../../Schemas/pattern-recipe.schema.json") || (!bSoil && !bTerrain && !bAggregate && !bGraded) ||
		!IsSafeIdentifier(OutRecipe.PatternId) || !IsSafeIdentifier(OutRecipe.CatalogId) ||
		!OutRecipe.CatalogId.StartsWith(TEXT("DA_")) ||
		(OutRecipe.ResolutionClass != TEXT("low") && OutRecipe.ResolutionClass != TEXT("medium") &&
		 OutRecipe.ResolutionClass != TEXT("high")))
	{
		OutError = TEXT("Pattern recipe uses an unsupported schema, identity, family, output, or resolution.");
		return false;
	}
	if (bGraded ? !ProjectTextureGradedParticleControls::Parse(Root,OutRecipe.StructureControls,OutError) :
		bAggregate ? !ProjectTextureParticleControls::Parse(Root,OutRecipe.StructureControls,OutError) : Root->HasField(TEXT("structure")))
	{
		if (OutError.IsEmpty()) OutError=TEXT("Structural controls are not admitted by this pattern family.");
		return false;
	}
	OutRecipe.SchemaVersion = Schema;
	OutRecipe.Algorithm = bGraded ? TEXT("graded_particles_native") : Algorithm;
	OutRecipe.Seed = static_cast<int32>(Seed);
	if (!ReadNodes(Root, OutRecipe.Seed, OutRecipe.ResolutionClass, bSoil, bAggregate, bGraded, OutRecipe.Nodes, OutError) ||
		!ReadExports(Root, OutRecipe.Nodes, bSoil, bAggregate, bGraded, OutRecipe.Exports, OutError))
	{
		return false;
	}

	if (bAggregate || bGraded)
	{
		FString FilterIdentity;
		if (bGraded && !ProjectTextureGradedParticleControls::NativeFilterIdentity(FilterIdentity,OutError)) return false;
		FString Parent;
		TMap<FString,FString> ConsumedParents;
		for (int32 I=0;I<OutRecipe.Nodes.Num();++I)
		{
			auto& N=OutRecipe.Nodes[I];
			if (bGraded)
			{
				Parent.Reset();
				for (const auto& P : N.Parents) Parent+=ConsumedParents.FindChecked(P);
			}
			N.StructuralIdentity=ComputeStringSha256(N.StructuralIdentity+ProjectTextureParticleControls::Normalize(OutRecipe.StructureControls)+Parent+(I==1 ? FilterIdentity : FString()));
			ConsumedParents.Add(N.NodeId,N.StructuralIdentity);
			Parent=N.StructuralIdentity;
		}
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
	if (bGraded) OutRecipe.NormalizedSemantics+=TEXT("|native_filter_stage=")+OutRecipe.Nodes[1].StructuralIdentity;
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
		Recipe.Exports.IsEmpty())
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
