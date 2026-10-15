// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectMaterialSurfaceRecipe.h"

#include "ProjectMaterialCompilerIdentity.h"
#include "ProjectMaterialPatternAuthority.h"
#include "ProjectMaterialTerrainLayoutContract.h"

#include "Dom/JsonObject.h"
#include "Misc/EngineVersion.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Utilities/ProjectSha256.h"

namespace ProjectMaterialSurfaceRecipePrivate
{
	constexpr TCHAR Family[] = TEXT("surface_substrate");
	constexpr TCHAR TerrainArchetype[] = TEXT("terrain_metric");
	constexpr TCHAR TerrainDetailArchetype[] = TEXT("terrain_detail_metric");
	constexpr TCHAR ObjectArchetype[] = TEXT("object_metric");

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

	bool IsBindingId(const FString& Value)
	{
		TArray<FString> Segments;
		Value.ParseIntoArray(Segments, TEXT("."), false);
		if (Segments.Num() < 2)
		{
			return false;
		}
		for (const FString& Segment : Segments)
		{
			if (Segment.IsEmpty() || Segment[0] < TEXT('a') || Segment[0] > TEXT('z'))
			{
				return false;
			}
			for (const TCHAR Character : Segment)
			{
				if ((Character < TEXT('a') || Character > TEXT('z')) &&
					(Character < TEXT('0') || Character > TEXT('9')) && Character != TEXT('_'))
				{
					return false;
				}
			}
		}
		return true;
	}

	TMap<FString, FVector2D> ScalarContract(const FString& Archetype)
	{
		if (Archetype == TEXT("terrain_height_masks_metric"))
		{
			auto Contract=ScalarContract(TEXT("height_masks_metric"));
			Contract.Add(TEXT("HydroDarkening"), {0,1});
			return Contract;
		}
		if (Archetype == TEXT("height_masks_metric"))
		{
			auto Contract=ScalarContract(TEXT("height_masks_uv"));
			Contract.Append(TMap<FString,FVector2D>{{TEXT("MacroScaleMeters"),{.05,1000}},
				{TEXT("MacroBlendStartMeters"),{0,1000}},{TEXT("MacroBlendEndMeters"),{.01,10000}},
				{TEXT("NearMacroStrength"),{0,1}},{TEXT("FarMacroStrength"),{0,1}},
				{TEXT("MacroRoughnessStrength"),{0,1}},{TEXT("AggregatePigmentStrength"),{0,1}},
				{TEXT("SecondaryScaleRatio"),{.05,16}},{TEXT("CompositionContrast"),{0,1}},
				{TEXT("CompositionStrength"),{0,1}}});
			return Contract;
		}
		if (Archetype == TEXT("aggregate_metric")) return {
			{TEXT("TileMeters"), {0.05, 10.0}}, {TEXT("TextureMipBias"), {-2.0, 2.0}},
			{TEXT("ReliefStrength"), {0.0, 2.0}}, {TEXT("SurfaceRoughness"), {0.05, 1.0}},
			{TEXT("RoughnessVariation"), {0.0, 1.0}}, {TEXT("MacroMeters"), {1.0, 1000.0}},
			{TEXT("MacroContrast"), {0.0, 1.0}}, {TEXT("MacroRoughness"), {0.0, 1.0}}};
		if (Archetype == TEXT("single_layer_water")) return {
			{TEXT("SurfaceRoughness"), {0.02, 1.0}}, {TEXT("WaterSpecular"), {0.0, 1.0}},
			{TEXT("WaterPhaseG"), {-0.99, 0.99}}, {TEXT("RippleTileMeters"), {0.05, 100.0}},
			{TEXT("RippleSpeed"), {0.0, 1.0}}, {TEXT("RippleStrength"), {0.0, 2.0}},
			{TEXT("LiveTimeEnabled"), {1.0, 1.0}}, {TEXT("PreviewSeconds"), {0.0, 0.0}}};
		if (Archetype == TEXT("height_masks_uv")) return {
			{TEXT("BaseRoughness"), {0.05, 1.0}}, {TEXT("PatternScaleMeters"), {0.05, 10.0}},
			{TEXT("BumpHeightCentimeters"), {0.0, 2.0}}, {TEXT("RoughnessStrength"), {0.0, 1.0}},
			{TEXT("AOInfluence"), {0.0, 1.0}}, {TEXT("GrainColorStrength"), {0.0, 1.0}}};
		TMap<FString, FVector2D> Contract = {
			{TEXT("BaseRoughness"), {0.0, 1.0}},
			{TEXT("PatternScaleMeters"), {0.05, 100.0}},
			{TEXT("MacroStrength"), {0.0, 1.0}},
			{TEXT("DetailStrength"), {0.0, 1.0}}};
		if (FProjectMaterialSurfaceRecipeContract::IsTerrainArchetype(Archetype))
		{
			Contract.Add(TEXT("HydroDarkening"), {0.0, 1.0});
			Contract.Add(TEXT("SlopeContrast"), {0.01, 16.0});
		}
		if (Archetype == TerrainDetailArchetype)
		{
			Contract.Add(TEXT("FineScaleMeters"), {0.05, 10.0});
			Contract.Add(TEXT("BumpHeightCentimeters"), {0.0, 1.0});
			Contract.Add(TEXT("FineColorStrength"), {0.0, 1.0});
		}
		return Contract;
	}

	TArray<FString> VectorContract(const FString& Archetype)
	{
		if (Archetype == TEXT("terrain_height_masks_metric")) return {TEXT("SoilDarkColor"), TEXT("SoilLightColor"), TEXT("GrainColor"), TEXT("HydroTint")};
		if (Archetype == TEXT("aggregate_metric")) return {TEXT("BinderColor"),TEXT("AggregateColor")};
		if (Archetype == TEXT("single_layer_water")) return {TEXT("WaterAlbedo"), TEXT("WaterExtinction"), TEXT("WaterSurfaceColor"), TEXT("BehindWaterScale")};
		if (Archetype == TEXT("height_masks_uv") || Archetype == TEXT("height_masks_metric")) return {TEXT("SoilDarkColor"), TEXT("SoilLightColor"), TEXT("GrainColor")};
		return FProjectMaterialSurfaceRecipeContract::IsTerrainArchetype(Archetype)
			? TArray<FString>({TEXT("GroundColor"), TEXT("SteepGroundColor"), TEXT("HydroTint")})
			: TArray<FString>({TEXT("BaseColor")});
	}

	bool ReadScalars(
		const TSharedPtr<FJsonObject>& Root,
		const FString& Archetype,
		TMap<FName, double>& OutValues,
		FString& OutError)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (!Root->TryGetObjectField(TEXT("scalars"), Object) || Object == nullptr)
		{
			OutError = TEXT("Missing object field: scalars");
			return false;
		}
		const TMap<FString, FVector2D> Contract = ScalarContract(Archetype);
		TSet<FString> Allowed;
		for (const TPair<FString, FVector2D>& Entry : Contract)
		{
			Allowed.Add(Entry.Key);
		}
		if (!ValidateFields(*Object, Allowed, TEXT("surface scalar"), OutError) ||
			(*Object)->Values.Num() != Contract.Num())
		{
			if (OutError.IsEmpty())
			{
				OutError = TEXT("Every archetype scalar is required exactly once.");
			}
			return false;
		}
		for (const TPair<FString, FVector2D>& Entry : Contract)
		{
			double Value = 0.0;
			if (!(*Object)->TryGetNumberField(Entry.Key, Value) || !FMath::IsFinite(Value) ||
				Value < Entry.Value.X || Value > Entry.Value.Y)
			{
				OutError = FString::Printf(TEXT("Surface scalar is missing or out of range: %s"), *Entry.Key);
				return false;
			}
			OutValues.Add(FName(Entry.Key), Value);
		}
		return true;
	}

	bool ReadVectors(
		const TSharedPtr<FJsonObject>& Root,
		const FString& Archetype,
		TMap<FName, FLinearColor>& OutValues,
		FString& OutError)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (!Root->TryGetObjectField(TEXT("vectors"), Object) || Object == nullptr)
		{
			OutError = TEXT("Missing object field: vectors");
			return false;
		}
		const TArray<FString> Names = VectorContract(Archetype);
		TSet<FString> Allowed;
		for (const FString& Name : Names)
		{
			Allowed.Add(Name);
		}
		if (!ValidateFields(*Object, Allowed, TEXT("surface vector"), OutError) ||
			(*Object)->Values.Num() != Names.Num())
		{
			if (OutError.IsEmpty())
			{
				OutError = TEXT("Every archetype vector is required exactly once.");
			}
			return false;
		}
		for (const FString& Name : Names)
		{
			const TArray<TSharedPtr<FJsonValue>>* Components = nullptr;
			if (!(*Object)->TryGetArrayField(Name, Components) || Components == nullptr || Components->Num() != 4)
			{
				OutError = FString::Printf(TEXT("Surface vector must contain four numbers: %s"), *Name);
				return false;
			}
			double Values[4] = {};
			for (int32 Index = 0; Index < 4; ++Index)
			{
				if (!(*Components)[Index]->TryGetNumber(Values[Index]) || !FMath::IsFinite(Values[Index]) ||
					Values[Index] < 0.0 || Values[Index] > 1.0)
				{
					OutError = FString::Printf(TEXT("Surface vector component is out of range: %s"), *Name);
					return false;
				}
			}
			OutValues.Add(FName(Name), FLinearColor(Values[0], Values[1], Values[2], Values[3]));
		}
		return true;
	}

	FString Number(const double Value)
	{
		return FString::Printf(TEXT("%.17g"), Value);
	}

	FString Normalize(const FProjectMaterialSurfaceRecipe& Recipe)
	{
		TArray<FName> ScalarNames;
		Recipe.Scalars.GetKeys(ScalarNames);
		ScalarNames.Sort(FNameLexicalLess());
		TArray<FName> VectorNames;
		Recipe.Vectors.GetKeys(VectorNames);
		VectorNames.Sort(FNameLexicalLess());
		const TCHAR* Kind = Recipe.ArtifactKind == EProjectMaterialSurfaceArtifactKind::Parent
			? TEXT("parent") : TEXT("instance");
		const TCHAR* Projection = Recipe.ProjectionFrame == EProjectMaterialProjectionFrame::WorldMetricTriplanar2D
			? TEXT("world_metric_triplanar_2d") : Recipe.ProjectionFrame == EProjectMaterialProjectionFrame::WorldMetricXY2D
			? TEXT("world_metric_xy_2d") : Recipe.ProjectionFrame == EProjectMaterialProjectionFrame::UVUnitMeter ? TEXT("uv_unit_meter") : TEXT("object_metric_triplanar_2d");
		FString Semantics;
		for (const FName Name : Recipe.SemanticBindings)
		{
			if (!Semantics.IsEmpty())
			{
				Semantics += TEXT(",");
			}
			Semantics += Name.ToString();
		}
		FString Result = FString::Printf(
			TEXT("schema=%s|id=%s|kind=%s|binding=%s|family=%s|archetype=%s|compiler=%s|")
			TEXT("parent=%s|folder=%s|projection=%s|pattern=%s|semantics=%s"),
			Recipe.Archetype == TEXT("terrain_height_masks_metric") ? TEXT("9") : Recipe.Archetype == TEXT("height_masks_metric") ? TEXT("8") : (Recipe.Archetype == TEXT("single_layer_water") || Recipe.Archetype == TEXT("aggregate_metric") || Recipe.Archetype == TEXT("height_masks_uv")) ? TEXT("7") : TEXT("4"),
			*Recipe.MaterialId,
			Kind,
			*Recipe.BindingId,
			*Recipe.Family,
			*Recipe.Archetype,
			*Recipe.CompilerVersion,
			*Recipe.ParentObjectPath,
			*Recipe.RelativeFolder,
			Projection,
			*Recipe.PatternId,
			*Semantics);
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

bool FProjectMaterialSurfaceRecipeContract::Parse(
	const FString& Json,
	const FString& SourcePath,
	const FString& RecipeRoot,
	FProjectMaterialSurfaceRecipe& OutRecipe,
	FString& OutError)
{
	using namespace ProjectMaterialSurfaceRecipePrivate;
	OutRecipe = {};
	OutError.Reset();
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("Surface recipe is not valid JSON.");
		return false;
	}
	const TSet<FString> Allowed = {
		TEXT("$schema"), TEXT("schema_version"), TEXT("material_id"), TEXT("artifact_kind"),
		TEXT("binding_id"), TEXT("family"), TEXT("archetype"), TEXT("compiler_version"),
		TEXT("parent"), TEXT("projection_frame"), TEXT("pattern"), TEXT("semantic_bindings"),
		TEXT("scalars"), TEXT("vectors")};
	if (!ValidateFields(Root, Allowed, TEXT("surface recipe"), OutError))
	{
		return false;
	}
	FString SchemaPath;
	FString SchemaVersion;
	FString Kind;
	FString Projection;
	if (!ReadRequiredString(Root, TEXT("$schema"), SchemaPath, OutError) ||
		!ReadRequiredString(Root, TEXT("schema_version"), SchemaVersion, OutError) ||
		!ReadRequiredString(Root, TEXT("material_id"), OutRecipe.MaterialId, OutError) ||
		!ReadRequiredString(Root, TEXT("artifact_kind"), Kind, OutError) ||
		!ReadRequiredString(Root, TEXT("family"), OutRecipe.Family, OutError) ||
		!ReadRequiredString(Root, TEXT("archetype"), OutRecipe.Archetype, OutError) ||
		!ReadRequiredString(Root, TEXT("compiler_version"), OutRecipe.CompilerVersion, OutError) ||
		!ReadRequiredString(Root, TEXT("projection_frame"), Projection, OutError))
	{
		return false;
	}
	const bool bWater = OutRecipe.Archetype == TEXT("single_layer_water");
	const bool bAggregate = OutRecipe.Archetype == TEXT("aggregate_metric");
	const bool bSemanticHeight = OutRecipe.Archetype == TEXT("terrain_height_masks_metric");
	const bool bMetricHeight = IsMetricHeightArchetype(OutRecipe.Archetype);
	if (SchemaPath != TEXT("../../Schemas/surface-recipe.schema.json") || (bSemanticHeight ? (SchemaVersion != TEXT("9") || OutRecipe.CompilerVersion != TEXT("10")) : bMetricHeight ? (SchemaVersion != TEXT("8") || OutRecipe.CompilerVersion != TEXT("9")) : (bWater || bAggregate || OutRecipe.Archetype == TEXT("height_masks_uv")) ? (SchemaVersion != TEXT("7") || OutRecipe.CompilerVersion != TEXT("8"))
			: (SchemaVersion != TEXT("4") || OutRecipe.CompilerVersion != TEXT("5"))) ||
		OutRecipe.Family != Family ||
		(!IsTerrainArchetype(OutRecipe.Archetype) && OutRecipe.Archetype != ObjectArchetype && OutRecipe.Archetype != TEXT("height_masks_uv") && !bMetricHeight && !bWater && !bAggregate))
	{
		OutError = TEXT("Surface recipe uses an unsupported schema, family, archetype, or compiler version.");
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
		OutRecipe.ArtifactKind = EProjectMaterialSurfaceArtifactKind::Parent;
		if (!OutRecipe.MaterialId.StartsWith(TEXT("M_")) || OutRecipe.MaterialId.StartsWith(TEXT("MI_")) ||
			Root->HasField(TEXT("parent")) || Root->HasField(TEXT("binding_id")))
		{
			OutError = TEXT("Surface parents require an M_ identity and no parent or binding_id.");
			return false;
		}
	}
	else if (Kind == TEXT("instance"))
	{
		OutRecipe.ArtifactKind = EProjectMaterialSurfaceArtifactKind::Instance;
		if (!OutRecipe.MaterialId.StartsWith(TEXT("MI_")) ||
			!ReadRequiredString(Root, TEXT("parent"), OutRecipe.ParentObjectPath, OutError) ||
			!ReadRequiredString(Root, TEXT("binding_id"), OutRecipe.BindingId, OutError) ||
			!IsBindingId(OutRecipe.BindingId))
		{
			OutError = TEXT("Surface instances require an MI_ identity, parent, and dotted binding_id.");
			return false;
		}
	}
	else
	{
		OutError = TEXT("artifact_kind must be parent or instance.");
		return false;
	}
	const bool bTerrain = IsTerrainArchetype(OutRecipe.Archetype);
	if ((bTerrain && !bMetricHeight && Projection != TEXT("world_metric_triplanar_2d")) ||
		((bWater || bMetricHeight) && Projection != TEXT("world_metric_xy_2d")) ||
		(bAggregate && Projection != TEXT("world_metric_xy_2d") && Projection != TEXT("world_metric_triplanar_2d")) ||
		(!bTerrain && !bWater && !bAggregate && !bMetricHeight && Projection != (OutRecipe.Archetype == TEXT("height_masks_uv") ? TEXT("uv_unit_meter") : TEXT("object_metric_triplanar_2d"))))
	{
		OutError = TEXT("Projection frame does not match the selected archetype.");
		return false;
	}
	OutRecipe.ProjectionFrame = bMetricHeight ? EProjectMaterialProjectionFrame::WorldMetricXY2D : bTerrain || (bAggregate && Projection==TEXT("world_metric_triplanar_2d"))
		? EProjectMaterialProjectionFrame::WorldMetricTriplanar2D
		: bWater || bAggregate || bMetricHeight ? EProjectMaterialProjectionFrame::WorldMetricXY2D
		: OutRecipe.Archetype == TEXT("height_masks_uv") ? EProjectMaterialProjectionFrame::UVUnitMeter : EProjectMaterialProjectionFrame::ObjectMetricTriplanar2D;

	const TSharedPtr<FJsonObject>* Pattern = nullptr;
	if (bWater && Root->HasField(TEXT("pattern")))
	{
		OutError = TEXT("Native river resources are not a ProjectTexture pattern.");
		return false;
	}
	if (!bWater && (!Root->TryGetObjectField(TEXT("pattern"), Pattern) || Pattern == nullptr ||
		(*Pattern)->Values.Num() != 1 ||
		!ReadRequiredString(*Pattern, TEXT("pattern_id"), OutRecipe.PatternId, OutError) ||
		!IsSafeIdentifier(OutRecipe.PatternId)))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Surface recipe pattern identity is invalid or unsupported.");
		}
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Semantics = nullptr;
	if (!Root->TryGetArrayField(TEXT("semantic_bindings"), Semantics) || Semantics == nullptr ||
		Semantics->Num() != (bTerrain ? 2 : 0))
	{
		OutError = TEXT("Surface recipe semantic bindings do not match the archetype.");
		return false;
	}
	const TArray<FName> ExpectedSemantics = bTerrain
		? TArray<FName>({TEXT("ground"), TEXT("hydro_transition")}) : TArray<FName>();
	for (int32 Index = 0; Index < ExpectedSemantics.Num(); ++Index)
	{
		FString Name;
		if (!(*Semantics)[Index]->TryGetString(Name) || Name != ExpectedSemantics[Index].ToString())
		{
			OutError = TEXT("Terrain semantic bindings must be exactly ground, hydro_transition.");
			return false;
		}
		OutRecipe.SemanticBindings.Add(ExpectedSemantics[Index]);
	}
	if (!ReadScalars(Root, OutRecipe.Archetype, OutRecipe.Scalars, OutError) ||
		!ReadVectors(Root, OutRecipe.Archetype, OutRecipe.Vectors, OutError))
	{
		return false;
	}
	if (bMetricHeight && OutRecipe.Scalars[TEXT("MacroBlendEndMeters")]<=OutRecipe.Scalars[TEXT("MacroBlendStartMeters")])
	{
		OutError=TEXT("Macro distance interval must have a positive length."); return false;
	}

	FString AbsoluteRoot = FPaths::ConvertRelativePathToFull(RecipeRoot);
	FPaths::NormalizeDirectoryName(AbsoluteRoot);
	AbsoluteRoot += TEXT("/");
	FString RelativePath = FPaths::ConvertRelativePathToFull(SourcePath);
	if (!FPaths::MakePathRelativeTo(RelativePath, *AbsoluteRoot) || RelativePath.StartsWith(TEXT("..")) ||
		!RelativePath.EndsWith(TEXT(".surface.json")))
	{
		OutError = TEXT("Surface recipe source escapes the configured recipe root.");
		return false;
	}
	OutRecipe.RelativeFolder = FPaths::GetPath(RelativePath).Replace(TEXT("\\"), TEXT("/"));
	if (OutRecipe.RelativeFolder.IsEmpty() || OutRecipe.RelativeFolder.Contains(TEXT("..")))
	{
		OutError = TEXT("Surface recipe must live in a named concern folder.");
		return false;
	}
	OutRecipe.SourcePath = RelativePath.Replace(TEXT("\\"), TEXT("/"));
	OutRecipe.NormalizedSemantics = Normalize(OutRecipe);
	OutRecipe.RecipeSha256 = FProjectMaterialCompilerIdentity::ComputeStringSha256(OutRecipe.NormalizedSemantics);
	return OutRecipe.RecipeSha256.Len() == 64 &&
		FProjectSha256::HashNormalizedText(Json, OutRecipe.RecipeSourceSha256);
}

bool FProjectMaterialSurfaceRecipeContract::ResolveOutputIdentity(
	const FProjectMaterialSurfaceRecipe& Recipe,
	const FString& OutputPackageRoot,
	FString& OutPackageName,
	FString& OutObjectPath,
	FString& OutError)
{
	OutError.Reset();
	if (!OutputPackageRoot.StartsWith(TEXT("/")) || OutputPackageRoot.Contains(TEXT("..")) ||
		OutputPackageRoot.EndsWith(TEXT("/")) || Recipe.RelativeFolder.Contains(TEXT("..")))
	{
		OutError = TEXT("Surface output root or recipe concern folder is invalid.");
		return false;
	}
	OutPackageName = FString::Printf(
		TEXT("%s/%s/%s"), *OutputPackageRoot, *Recipe.RelativeFolder, *Recipe.MaterialId);
	OutObjectPath = FString::Printf(TEXT("%s.%s"), *OutPackageName, *Recipe.MaterialId);
	if (!OutPackageName.StartsWith(OutputPackageRoot + TEXT("/")))
	{
		OutError = TEXT("Derived surface output escapes the configured package root.");
		return false;
	}
	return true;
}

FString FProjectMaterialSurfaceRecipeContract::ComputeArtifactSemanticIdentity(
	const FProjectMaterialSurfaceRecipe& Recipe,
	const FString& OutputObjectPath,
	const FProjectMaterialSurfaceDependency& Pattern,
	const FString& ParentPackageSha256,
	const FProjectMaterialTerrainLayoutContract* TerrainLayout)
{
	// A surface binds only what it consumes from its producers: the pattern's object path and output
	// contract, and the terrain channel layout. Their semantic identities, exact package bytes, and
	// receipt identity are provenance: authenticated before generation and recorded, never rebuild keys.
	const FString Layout = TerrainLayout != nullptr
		? FString::Printf(TEXT("%s:%d:%s"), *TerrainLayout->LayoutId, TerrainLayout->LayoutVersion,
			*TerrainLayout->LayoutSha256)
		: FString();
	FString Identity = FString::Printf(
		TEXT("recipe=%s|family=%s|archetype=%s|compiler=%s|pattern=%s:%s:%s|")
		TEXT("parent=%s:%s|layout=%s|output=%s|engine=%s|fingerprint=%s"),
		*Recipe.RecipeSha256,
		*Recipe.Family,
		*Recipe.Archetype,
		*Recipe.CompilerVersion,
		*Recipe.PatternId,
		*Pattern.OutputObjectPath,
		*Pattern.OutputContract,
		*Recipe.ParentObjectPath,
		*ParentPackageSha256,
		*Layout,
		*OutputObjectPath,
		*GetEngineCompatibilityIdentity(),
		*FProjectMaterialCompilerIdentity::GetCompilerFingerprint());
	if (!Pattern.NativeResourceHashes.IsEmpty())
	{
		TArray<FString> Paths;
		Pattern.NativeResourceHashes.GetKeys(Paths); Paths.Sort();
		for (const FString& Path : Paths) Identity += TEXT("|native_resource=") + Path + TEXT(":") + Pattern.NativeResourceHashes.FindChecked(Path);
	}
	return FProjectMaterialCompilerIdentity::ComputeStringSha256(Identity);
}

FString FProjectMaterialSurfaceRecipeContract::GetEngineCompatibilityIdentity()
{
	const FEngineVersion Version = FEngineVersion::Current();
	return FString::Printf(TEXT("%d.%d"), Version.GetMajor(), Version.GetMinor());
}

bool FProjectMaterialSurfaceRecipeContract::IsTerrainArchetype(const FString& Archetype)
{
	return Archetype == ProjectMaterialSurfaceRecipePrivate::TerrainArchetype ||
		Archetype == ProjectMaterialSurfaceRecipePrivate::TerrainDetailArchetype ||
		Archetype == TEXT("terrain_height_masks_metric");
}

bool FProjectMaterialSurfaceRecipeContract::IsMetricHeightArchetype(const FString& Archetype)
{
	return Archetype == TEXT("height_masks_metric") || Archetype == TEXT("terrain_height_masks_metric");
}

bool FProjectMaterialSurfaceRecipeContract::RequiresDirectNaniteUsage(const FString& Archetype)
{
	return Archetype == TEXT("aggregate_metric") || IsMetricHeightArchetype(Archetype) ||
		Archetype == TEXT("height_masks_uv");
}
