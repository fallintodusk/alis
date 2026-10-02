// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectMaterialSurfaceBuilder.h"

#include "ProjectMaterialPatternAuthority.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "MaterialEditingLibrary.h"
#include "Materials/MaterialExpressionAbs.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstantBiasScale.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionPower.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSubstrate.h"
#include "Materials/MaterialExpressionTransformPosition.h"
#include "Materials/MaterialExpressionTransform.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Engine/TextureRenderTarget2D.h"
#include "MeshPartitionMaterialExpressionUtils.h"
#include "Misc/PackageName.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "ProjectTextureRuntimeCatalog.h"

namespace ProjectMaterialSurfaceBuilderPrivate
{
constexpr TCHAR OwnerKey[] = TEXT("ProjectGenerationOwner");
constexpr TCHAR ArchetypeKey[] = TEXT("ProjectSurfaceArchetype");

template <typename TExpression>
TExpression* CreateExpression(UMaterial* Material, int32 X, int32 Y)
{
	return Cast<TExpression>(UMaterialEditingLibrary::CreateMaterialExpression(
		Material, TExpression::StaticClass(), X, Y));
}

UObject* FindOrCreateAsset(
	const FString& PackageName,
	const FString& AssetName,
	UClass* AssetClass,
	FString& OutError)
{
	if (FPackageName::DoesPackageExist(PackageName))
	{
		const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *PackageName, *AssetName);
		UObject* Existing = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath);
		if (Existing == nullptr || !Existing->IsA(AssetClass))
		{
			OutError = FString::Printf(TEXT("Existing surface output has the wrong class: %s"), *ObjectPath);
			return nullptr;
		}
		return Existing;
	}
	UPackage* Package = CreatePackage(*PackageName);
	if (Package == nullptr)
	{
		OutError = FString::Printf(TEXT("Could not create surface package: %s"), *PackageName);
		return nullptr;
	}
	UObject* Asset = NewObject<UObject>(Package, AssetClass, *AssetName, RF_Public | RF_Standalone);
	if (Asset == nullptr)
	{
		OutError = FString::Printf(TEXT("Could not create surface asset: %s"), *PackageName);
		return nullptr;
	}
	FAssetRegistryModule::AssetCreated(Asset);
	return Asset;
}

UMaterialExpressionScalarParameter* Scalar(
	UMaterial* Material,
	const FProjectMaterialSurfaceRecipe& Recipe,
	const TCHAR* Name,
	int32 X,
	int32 Y)
{
	auto* Expression = CreateExpression<UMaterialExpressionScalarParameter>(Material, X, Y);
	if (Expression != nullptr)
	{
		Expression->ParameterName = Name;
		Expression->DefaultValue = Recipe.Scalars[FName(Name)];
	}
	return Expression;
}

UMaterialExpressionVectorParameter* Vector(
	UMaterial* Material,
	const FProjectMaterialSurfaceRecipe& Recipe,
	const TCHAR* Name,
	int32 X,
	int32 Y)
{
	auto* Expression = CreateExpression<UMaterialExpressionVectorParameter>(Material, X, Y);
	if (Expression != nullptr)
	{
		Expression->ParameterName = Name;
		Expression->DefaultValue = Recipe.Vectors[FName(Name)];
	}
	return Expression;
}

UMaterialExpressionComponentMask* Mask(
	UMaterial* Material,
	UMaterialExpression* Input,
	int32 Channel,
	int32 X,
	int32 Y)
{
	auto* Expression = CreateExpression<UMaterialExpressionComponentMask>(Material, X, Y);
	if (Expression != nullptr)
	{
		Expression->R = Channel == 0;
		Expression->G = Channel == 1;
		Expression->B = Channel == 2;
		Expression->A = Channel == 3;
		Expression->Input.Connect(0, Input);
	}
	return Expression;
}

UMaterialExpression* BuildCoordinates(
	UMaterial* Material,
	EProjectMaterialProjectionFrame Projection,
	FString& OutError)
{
	auto* World = CreateExpression<UMaterialExpressionWorldPosition>(Material, -1300, -520);
	auto* ToMeters = CreateExpression<UMaterialExpressionDivide>(Material, -900, -520);
	if (World == nullptr || ToMeters == nullptr)
	{
		OutError = TEXT("Could not build metric position expressions.");
		return nullptr;
	}
	UMaterialExpression* Position = World;
	if (Projection == EProjectMaterialProjectionFrame::ObjectMetricTriplanar2D)
	{
		auto* ToLocal = CreateExpression<UMaterialExpressionTransformPosition>(Material, -1100, -520);
		if (ToLocal == nullptr)
		{
			OutError = TEXT("Could not build object-local projection.");
			return nullptr;
		}
		ToLocal->TransformSourceType = TRANSFORMPOSSOURCE_World;
		ToLocal->TransformType = TRANSFORMPOSSOURCE_Local;
		ToLocal->Input.Connect(0, World);
		Position = ToLocal;
	}
	ToMeters->A.Connect(0, Position);
	ToMeters->ConstB = 100.0f;
	return ToMeters;
}

UMaterialExpression* BuildTriplanarPattern(
	UMaterial* Material,
	const FProjectMaterialSurfaceRecipe& Recipe,
	UTextureRenderTarget2D* Texture,
	UMaterialExpression* PositionMeters,
	FString& OutError)
{
	if (Texture == nullptr || Texture->SRGB || Texture->IsSRGB())
	{
		OutError = TEXT("Structure output must be a linear texture-data resource.");
		return nullptr;
	}
	auto* PatternScale = Scalar(Material, Recipe, TEXT("PatternScaleMeters"), -1080, -700);
	auto* Coordinates = CreateExpression<UMaterialExpressionDivide>(Material, -860, -600);
	auto* PositionX = Mask(Material, Coordinates, 0, -640, -720);
	auto* PositionY = Mask(Material, Coordinates, 1, -640, -620);
	auto* PositionZ = Mask(Material, Coordinates, 2, -640, -520);
	auto* Xy = CreateExpression<UMaterialExpressionAppendVector>(Material, -420, -720);
	auto* Xz = CreateExpression<UMaterialExpressionAppendVector>(Material, -420, -600);
	auto* Yz = CreateExpression<UMaterialExpressionAppendVector>(Material, -420, -480);
	auto* SampleXy = CreateExpression<UMaterialExpressionTextureSample>(Material, -180, -720);
	auto* SampleXz = CreateExpression<UMaterialExpressionTextureSample>(Material, -180, -600);
	auto* SampleYz = CreateExpression<UMaterialExpressionTextureSample>(Material, -180, -480);
	auto* WorldNormal = CreateExpression<UMaterialExpressionVertexNormalWS>(Material, -860, -340);
	UMaterialExpression* ProjectionNormal = WorldNormal;
	UMaterialExpressionTransform* LocalNormal = nullptr;
	if (Recipe.ProjectionFrame == EProjectMaterialProjectionFrame::ObjectMetricTriplanar2D)
	{
		LocalNormal = CreateExpression<UMaterialExpressionTransform>(Material, -660, -340);
		if (LocalNormal != nullptr)
		{
			LocalNormal->TransformSourceType = TRANSFORMSOURCE_World;
			LocalNormal->TransformType = TRANSFORM_Local;
			LocalNormal->Input.Connect(0, WorldNormal);
			ProjectionNormal = LocalNormal;
		}
	}
	auto* AbsoluteNormal = CreateExpression<UMaterialExpressionAbs>(Material, -460, -340);
	auto* WeightX = Mask(Material, AbsoluteNormal, 0, -240, -400);
	auto* WeightY = Mask(Material, AbsoluteNormal, 1, -240, -320);
	auto* WeightZ = Mask(Material, AbsoluteNormal, 2, -240, -240);
	auto* WeightXy = CreateExpression<UMaterialExpressionAdd>(Material, -20, -360);
	auto* WeightSum = CreateExpression<UMaterialExpressionAdd>(Material, 160, -300);
	auto* NormalizedX = CreateExpression<UMaterialExpressionDivide>(Material, 340, -400);
	auto* NormalizedY = CreateExpression<UMaterialExpressionDivide>(Material, 340, -320);
	auto* NormalizedZ = CreateExpression<UMaterialExpressionDivide>(Material, 340, -240);
	auto* WeightedXy = CreateExpression<UMaterialExpressionMultiply>(Material, 120, -720);
	auto* WeightedXz = CreateExpression<UMaterialExpressionMultiply>(Material, 120, -600);
	auto* WeightedYz = CreateExpression<UMaterialExpressionMultiply>(Material, 120, -480);
	auto* AddFirst = CreateExpression<UMaterialExpressionAdd>(Material, 420, -620);
	auto* Packed = CreateExpression<UMaterialExpressionAdd>(Material, 620, -540);
	if (PatternScale == nullptr || Coordinates == nullptr || PositionX == nullptr ||
		PositionY == nullptr || PositionZ == nullptr || Xy == nullptr || Xz == nullptr ||
		Yz == nullptr || SampleXy == nullptr || SampleXz == nullptr || SampleYz == nullptr ||
		WorldNormal == nullptr ||
		(Recipe.ProjectionFrame == EProjectMaterialProjectionFrame::ObjectMetricTriplanar2D &&
		 LocalNormal == nullptr) || AbsoluteNormal == nullptr || WeightX == nullptr ||
		WeightY == nullptr || WeightZ == nullptr || WeightXy == nullptr || WeightSum == nullptr ||
		NormalizedX == nullptr || NormalizedY == nullptr || NormalizedZ == nullptr ||
		WeightedXy == nullptr || WeightedXz == nullptr || WeightedYz == nullptr ||
		AddFirst == nullptr || Packed == nullptr)
	{
		OutError = TEXT("Could not create the metric triplanar Structure projection.");
		return nullptr;
	}
	Coordinates->A.Connect(0, PositionMeters);
	Coordinates->B.Connect(0, PatternScale);
	Xy->A.Connect(0, PositionX);
	Xy->B.Connect(0, PositionY);
	Xz->A.Connect(0, PositionX);
	Xz->B.Connect(0, PositionZ);
	Yz->A.Connect(0, PositionY);
	Yz->B.Connect(0, PositionZ);
	for (UMaterialExpressionTextureSample* Sample : {SampleXy, SampleXz, SampleYz})
	{
		Sample->Texture = Texture;
		Sample->SamplerType = SAMPLERTYPE_LinearColor;
	}
	SampleXy->Coordinates.Connect(0, Xy);
	SampleXz->Coordinates.Connect(0, Xz);
	SampleYz->Coordinates.Connect(0, Yz);
	AbsoluteNormal->Input.Connect(0, ProjectionNormal);
	WeightXy->A.Connect(0, WeightX);
	WeightXy->B.Connect(0, WeightY);
	WeightSum->A.Connect(0, WeightXy);
	WeightSum->B.Connect(0, WeightZ);
	NormalizedX->A.Connect(0, WeightX);
	NormalizedX->B.Connect(0, WeightSum);
	NormalizedY->A.Connect(0, WeightY);
	NormalizedY->B.Connect(0, WeightSum);
	NormalizedZ->A.Connect(0, WeightZ);
	NormalizedZ->B.Connect(0, WeightSum);
	WeightedXy->A.Connect(5, SampleXy);
	WeightedXy->B.Connect(0, NormalizedZ);
	WeightedXz->A.Connect(5, SampleXz);
	WeightedXz->B.Connect(0, NormalizedY);
	WeightedYz->A.Connect(5, SampleYz);
	WeightedYz->B.Connect(0, NormalizedX);
	AddFirst->A.Connect(0, WeightedXy);
	AddFirst->B.Connect(0, WeightedXz);
	Packed->A.Connect(0, AddFirst);
	Packed->B.Connect(0, WeightedYz);
	return Packed;
}

UMaterialExpression* BuildTerrainColor(
	UMaterial* Material,
	const FProjectMaterialSurfaceRecipe& Recipe,
	UMaterialExpression* CoverPatch,
	const TMap<FName, int32>& SemanticChannels,
	FString& OutError)
{
	const int32* GroundIndex = SemanticChannels.Find(TEXT("ground"));
	const int32* HydroIndex = SemanticChannels.Find(TEXT("hydro_transition"));
	if (GroundIndex == nullptr || HydroIndex == nullptr)
	{
		OutError = TEXT("Terrain surface generation requires ground and hydro_transition channels.");
		return nullptr;
	}
	auto* Resource = CreateExpression<UE::MeshPartition::UMaterialExpressionMeshPartitionResource>(
		Material, -1300, 260);
	auto* Ground = CreateExpression<UE::MeshPartition::UMaterialExpressionMeshPartitionChannelSampleIndex>(
		Material, -1080, 180);
	auto* Hydro = CreateExpression<UE::MeshPartition::UMaterialExpressionMeshPartitionChannelSampleIndex>(
		Material, -1080, 340);
	auto* Normal = CreateExpression<UMaterialExpressionVertexNormalWS>(Material, -1300, -40);
	auto* NormalZ = Mask(Material, Normal, 2, -1080, -40);
	auto* OneMinusSlope = CreateExpression<UMaterialExpressionOneMinus>(Material, -860, -40);
	auto* SlopeContrast = Scalar(Material, Recipe, TEXT("SlopeContrast"), -860, 80);
	auto* Slope = CreateExpression<UMaterialExpressionPower>(Material, -640, 0);
	auto* GroundColor = Vector(Material, Recipe, TEXT("GroundColor"), -640, -220);
	auto* SteepColor = Vector(Material, Recipe, TEXT("SteepGroundColor"), -640, -140);
	auto* FlatAmount = CreateExpression<UMaterialExpressionOneMinus>(Material, -420, -60);
	auto* FlatCover = CreateExpression<UMaterialExpressionMultiply>(Material, -220, -80);
	auto* ExposedAmount = CreateExpression<UMaterialExpressionOneMinus>(Material, -20, -80);
	auto* SlopeColor = CreateExpression<UMaterialExpressionLinearInterpolate>(Material, -420, -140);
	auto* HydroTint = Vector(Material, Recipe, TEXT("HydroTint"), -420, 220);
	auto* GroundBlend = CreateExpression<UMaterialExpressionLinearInterpolate>(Material, -180, 40);
	auto* HydroDarkening = Scalar(Material, Recipe, TEXT("HydroDarkening"), -420, 420);
	auto* HydroAmount = CreateExpression<UMaterialExpressionMultiply>(Material, -180, 340);
	auto* Final = CreateExpression<UMaterialExpressionLinearInterpolate>(Material, 40, 100);
	if (Resource == nullptr || Ground == nullptr || Hydro == nullptr || Normal == nullptr ||
		NormalZ == nullptr || OneMinusSlope == nullptr || SlopeContrast == nullptr || Slope == nullptr ||
		GroundColor == nullptr || SteepColor == nullptr || FlatAmount == nullptr ||
		FlatCover == nullptr || ExposedAmount == nullptr || SlopeColor == nullptr || HydroTint == nullptr ||
		GroundBlend == nullptr || HydroDarkening == nullptr || HydroAmount == nullptr || Final == nullptr)
	{
		OutError = TEXT("Could not build the closed terrain semantic color graph.");
		return nullptr;
	}
	Ground->ChannelTextureInput.Connect(0, Resource);
	Ground->ConstChannelIndex = *GroundIndex;
	Hydro->ChannelTextureInput.Connect(0, Resource);
	Hydro->ConstChannelIndex = *HydroIndex;
	OneMinusSlope->Input.Connect(0, NormalZ);
	Slope->Base.Connect(0, OneMinusSlope);
	Slope->Exponent.Connect(0, SlopeContrast);
	FlatAmount->Input.Connect(0, Slope);
	FlatCover->A.Connect(0, FlatAmount);
	FlatCover->B.Connect(0, CoverPatch);
	ExposedAmount->Input.Connect(0, FlatCover);
	SlopeColor->A.Connect(0, GroundColor);
	SlopeColor->B.Connect(0, SteepColor);
	SlopeColor->Alpha.Connect(0, ExposedAmount);
	GroundBlend->A.Connect(0, HydroTint);
	GroundBlend->B.Connect(0, SlopeColor);
	GroundBlend->Alpha.Connect(0, Ground);
	HydroAmount->A.Connect(0, Hydro);
	HydroAmount->B.Connect(0, HydroDarkening);
	Final->A.Connect(0, GroundBlend);
	Final->B.Connect(0, HydroTint);
	Final->Alpha.Connect(0, HydroAmount);
	return Final;
}
}

bool ProjectMaterialSurfaceBuilder::BuildParent(
	const FProjectMaterialSurfaceRecipe& Recipe,
	const FProjectMaterialPatternDependency& PatternDependency,
	const FString& PackageName,
	const TMap<FName, int32>& SemanticChannels,
	UObject*& OutAsset,
	TArray<FString>& OutCompileErrors,
	FString& OutError)
{
	using namespace ProjectMaterialSurfaceBuilderPrivate;
	OutAsset = nullptr;
	UMaterial* Material = Cast<UMaterial>(FindOrCreateAsset(
		PackageName, Recipe.MaterialId, UMaterial::StaticClass(), OutError));
	if (Material == nullptr)
	{
		return false;
	}
	UTextureRenderTarget2D* Pattern = LoadObject<UTextureRenderTarget2D>(
		nullptr, *PatternDependency.OutputObjectPath);
	UProjectTextureOutputSlot* OutputSlot = Pattern != nullptr
		? Cast<UProjectTextureOutputSlot>(Pattern->GetAssetUserDataOfClass(
			UProjectTextureOutputSlot::StaticClass()))
		: nullptr;
	if (Pattern == nullptr || OutputSlot == nullptr || OutputSlot->Catalog == nullptr ||
		OutputSlot->SlotName != TEXT("Structure"))
	{
		OutError = FString::Printf(TEXT("Accepted runtime Structure output cannot be loaded: %s"),
			*PatternDependency.OutputObjectPath);
		return false;
	}
	UMaterialEditingLibrary::DeleteAllMaterialExpressions(Material);
	auto* Coordinates = BuildCoordinates(Material, Recipe.ProjectionFrame, OutError);
	auto* PatternSample = Coordinates == nullptr ? nullptr
		: BuildTriplanarPattern(Material, Recipe, Pattern, Coordinates, OutError);
	auto* Macro = Mask(Material, PatternSample, 0, -400, -520);
	auto* Cover = Mask(Material, PatternSample, 1, -400, -440);
	auto* GroundDetail = Mask(Material, PatternSample, 2, -400, -280);
	if (Coordinates == nullptr || PatternSample == nullptr || Macro == nullptr || Cover == nullptr ||
		GroundDetail == nullptr ||
		PatternDependency.OutputContract != TEXT("surface_structure_rgb_v1"))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Could not bind the accepted runtime Structure output.");
		}
		return false;
	}
	auto* MacroStrength = Scalar(Material, Recipe, TEXT("MacroStrength"), -400, -280);
	auto* DetailStrength = Scalar(Material, Recipe, TEXT("DetailStrength"), -400, -120);
	auto* MacroAmount = CreateExpression<UMaterialExpressionMultiply>(Material, -160, -500);
	auto* MacroBrightness = CreateExpression<UMaterialExpressionOneMinus>(Material, 40, -500);
	UMaterialExpression* BaseColor = Recipe.Archetype == TEXT("terrain_metric")
		? BuildTerrainColor(Material, Recipe, Cover, SemanticChannels, OutError)
		: Vector(Material, Recipe, TEXT("BaseColor"), -160, -80);
	auto* FinalColor = CreateExpression<UMaterialExpressionMultiply>(Material, 280, -180);
	auto* BaseRoughness = Scalar(Material, Recipe, TEXT("BaseRoughness"), -160, 540);
	auto* RoughnessAdd = CreateExpression<UMaterialExpressionAdd>(Material, 40, 540);
	auto* Roughness = CreateExpression<UMaterialExpressionClamp>(Material, 260, 540);
	auto* DetailCentered = CreateExpression<UMaterialExpressionConstantBiasScale>(Material, -160, -280);
	auto* DetailAmount = CreateExpression<UMaterialExpressionMultiply>(Material, 40, -300);
	auto* Slab = CreateExpression<UMaterialExpressionSubstrateSlabBSDF>(Material, 700, -100);
	if (MacroStrength == nullptr || DetailStrength == nullptr ||
		MacroAmount == nullptr || MacroBrightness == nullptr || BaseColor == nullptr || FinalColor == nullptr ||
		BaseRoughness == nullptr || RoughnessAdd == nullptr || Roughness == nullptr ||
		DetailCentered == nullptr || DetailAmount == nullptr || Slab == nullptr)
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Could not build the closed surface_substrate graph.");
		}
		return false;
	}
	MacroAmount->A.Connect(0, Macro);
	MacroAmount->B.Connect(0, MacroStrength);
	MacroBrightness->Input.Connect(0, MacroAmount);
	FinalColor->A.Connect(0, BaseColor);
	FinalColor->B.Connect(0, MacroBrightness);
	RoughnessAdd->A.Connect(0, BaseRoughness);
	RoughnessAdd->B.Connect(0, DetailAmount);
	Roughness->Input.Connect(0, RoughnessAdd);
	Roughness->MinDefault = 0.0f;
	Roughness->MaxDefault = 1.0f;
	DetailCentered->Input.Connect(0, GroundDetail);
	DetailCentered->Bias = -0.5f;
	DetailCentered->Scale = 1.0f;
	DetailAmount->A.Connect(0, DetailCentered);
	DetailAmount->B.Connect(0, DetailStrength);
	Slab->DiffuseAlbedo.Connect(0, FinalColor);
	Slab->Roughness.Connect(0, Roughness);
	UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData();
	if (EditorData == nullptr)
	{
		OutError = TEXT("Generated surface material has no Editor graph data.");
		return false;
	}
	EditorData->FrontMaterial.Connect(0, Slab);
	Material->BlendMode = BLEND_Opaque;
	Material->MaterialDomain = MD_Surface;
	Material->SetShadingModel(MSM_DefaultLit);
	Material->GetOutermost()->GetMetaData().SetValue(Material, OwnerKey, TEXT("ProjectMaterial"));
	Material->GetOutermost()->GetMetaData().SetValue(Material, ArchetypeKey, *Recipe.Archetype);
	Material->PostEditChange();
	OutCompileErrors = UMaterialEditingLibrary::RecompileMaterial(Material);
	if (!OutCompileErrors.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Surface material compile failed: %s"),
			*FString::Join(OutCompileErrors, TEXT(" | ")));
		return false;
	}
	Material->MarkPackageDirty();
	OutAsset = Material;
	return true;
}

bool ProjectMaterialSurfaceBuilder::BuildInstance(
	const FProjectMaterialSurfaceRecipe& Recipe,
	const FString& PackageName,
	UObject*& OutAsset,
	FString& OutError)
{
	using namespace ProjectMaterialSurfaceBuilderPrivate;
	UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, *Recipe.ParentObjectPath);
	if (Parent == nullptr)
	{
		OutError = FString::Printf(TEXT("Surface instance parent cannot be loaded: %s"),
			*Recipe.ParentObjectPath);
		return false;
	}
	UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(FindOrCreateAsset(
		PackageName, Recipe.MaterialId, UMaterialInstanceConstant::StaticClass(), OutError));
	if (Instance == nullptr)
	{
		return false;
	}
	Instance->ClearParameterValuesEditorOnly();
	UMaterialEditingLibrary::SetMaterialInstanceParent(Instance, Parent);
	for (const TPair<FName, double>& ScalarValue : Recipe.Scalars)
	{
		float ParentValue = 0.0f;
		if (!Parent->GetScalarParameterValue(FHashedMaterialParameterInfo(ScalarValue.Key), ParentValue))
		{
			OutError = FString::Printf(TEXT("Unknown surface scalar parameter: %s"),
				*ScalarValue.Key.ToString());
			return false;
		}
		Instance->SetScalarParameterValueEditorOnly(
			FMaterialParameterInfo(ScalarValue.Key), ScalarValue.Value);
	}
	for (const TPair<FName, FLinearColor>& VectorValue : Recipe.Vectors)
	{
		FLinearColor ParentValue;
		if (!Parent->GetVectorParameterValue(FHashedMaterialParameterInfo(VectorValue.Key), ParentValue))
		{
			OutError = FString::Printf(TEXT("Unknown surface vector parameter: %s"),
				*VectorValue.Key.ToString());
			return false;
		}
		Instance->SetVectorParameterValueEditorOnly(
			FMaterialParameterInfo(VectorValue.Key), VectorValue.Value);
	}
	UMaterialEditingLibrary::UpdateMaterialInstance(Instance);
	Instance->GetOutermost()->GetMetaData().SetValue(Instance, OwnerKey, TEXT("ProjectMaterial"));
	Instance->GetOutermost()->GetMetaData().SetValue(Instance, ArchetypeKey, *Recipe.Archetype);
	Instance->PostEditChange();
	Parent->GetOutermost()->SetDirtyFlag(false);
	Instance->MarkPackageDirty();
	OutAsset = Instance;
	return true;
}

bool ProjectMaterialSurfaceBuilder::Verify(
	const FProjectMaterialSurfaceRecipe& Recipe,
	const FProjectMaterialPatternDependency& PatternDependency,
	const TMap<FName, int32>& SemanticChannels,
	UObject* Asset,
	FString& OutError)
{
	using namespace ProjectMaterialSurfaceBuilderPrivate;
	FMetaData& MetaData = Asset->GetOutermost()->GetMetaData();
	if (MetaData.GetValue(Asset, OwnerKey) != TEXT("ProjectMaterial") ||
		MetaData.GetValue(Asset, ArchetypeKey) != Recipe.Archetype)
	{
		OutError = TEXT("Reloaded surface ownership metadata is invalid.");
		return false;
	}
	if (Recipe.ArtifactKind == EProjectMaterialSurfaceArtifactKind::Instance)
	{
		const UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(Asset);
		if (Instance == nullptr || Instance->Parent == nullptr ||
			Instance->Parent->GetPathName() != Recipe.ParentObjectPath)
		{
			OutError = TEXT("Reloaded surface instance parent does not match the recipe.");
			return false;
		}
		return true;
	}
	const UMaterial* Material = Cast<UMaterial>(Asset);
	if (Material == nullptr || !Material->HasSubstrateFrontMaterialConnected())
	{
		OutError = TEXT("Reloaded surface parent is not an explicit Substrate material.");
		return false;
	}
	const TArray<UMaterialExpression*> Expressions = UMaterialEditingLibrary::GetMaterialExpressions(Material);
	TSet<FName> Parameters;
	int32 Slabs = 0;
	int32 PatternSamples = 0;
	TArray<int32> ChannelIndices;
	for (const UMaterialExpression* Expression : Expressions)
	{
		if (const auto* Parameter = Cast<UMaterialExpressionParameter>(Expression))
		{
			Parameters.Add(Parameter->ParameterName);
		}
		Slabs += Expression->IsA<UMaterialExpressionSubstrateSlabBSDF>() ? 1 : 0;
		if (const auto* Sample = Cast<UMaterialExpressionTextureSample>(Expression))
		{
			PatternSamples += Sample->Texture != nullptr &&
				Sample->Texture->GetPathName() == PatternDependency.OutputObjectPath ? 1 : 100;
		}
		if (const auto* Sample = Cast<UE::MeshPartition::UMaterialExpressionMeshPartitionChannelSampleIndex>(Expression))
		{
			ChannelIndices.Add(Sample->ConstChannelIndex);
		}
	}
	if (Slabs != 1 || PatternSamples != 3 ||
		Parameters.Num() != Recipe.Scalars.Num() + Recipe.Vectors.Num())
	{
		OutError = TEXT("Reloaded surface parent does not match the closed archetype shape.");
		return false;
	}
	if (Recipe.Archetype == TEXT("terrain_metric"))
	{
		ChannelIndices.Sort();
		TArray<int32> Expected = {
			SemanticChannels.FindRef(TEXT("ground")),
			SemanticChannels.FindRef(TEXT("hydro_transition"))};
		Expected.Sort();
		if (ChannelIndices != Expected)
		{
			OutError = TEXT("Reloaded terrain parent does not match the semantic channel layout.");
			return false;
		}
	}
	else if (!ChannelIndices.IsEmpty())
	{
		OutError = TEXT("Object surface unexpectedly depends on Mesh Terrain channels.");
		return false;
	}
	return true;
}
