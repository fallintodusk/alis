// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectTextureRuntimeAssetBuilder.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNoise.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/Material.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/PackageName.h"
#include "ProjectTextureRuntimeCatalog.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

namespace ProjectTextureRuntimeAssetBuilderPrivate
{
constexpr TCHAR OwnerKey[] = TEXT("ProjectGenerationOwner");
constexpr TCHAR PatternKey[] = TEXT("ProjectPatternId");

template <typename TExpression>
TExpression* CreateExpression(UMaterial* Material, int32 X, int32 Y)
{
	return Cast<TExpression>(UMaterialEditingLibrary::CreateMaterialExpression(
		Material, TExpression::StaticClass(), X, Y));
}

template <typename TAsset>
TAsset* CreateAsset(UPackage* Package, const TCHAR* Name)
{
	TAsset* Asset = NewObject<TAsset>(Package, Name, RF_Public | RF_Standalone);
	if (Asset != nullptr)
	{
		FAssetRegistryModule::AssetCreated(Asset);
	}
	return Asset;
}

UMaterialExpressionComponentMask* Mask(
	UMaterial* Material,
	UMaterialExpression* Input,
	int32 Channel,
	int32 X,
	int32 Y,
	int32 OutputIndex = 0)
{
	auto* Result = CreateExpression<UMaterialExpressionComponentMask>(Material, X, Y);
	if (Result != nullptr)
	{
		Result->R = Channel == 0;
		Result->G = Channel == 1;
		Result->B = Channel == 2;
		Result->A = Channel == 3;
		Result->Input.Connect(OutputIndex, Input);
	}
	return Result;
}

bool FinalizeGenerator(UMaterial* Material, UMaterialExpression* Packed, FString& OutError)
{
	auto* Opacity = CreateExpression<UMaterialExpressionConstant>(Material, 720, 180);
	UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData();
	if (Opacity == nullptr || EditorData == nullptr)
	{
		OutError = TEXT("Could not complete the runtime generator output graph.");
		return false;
	}
	Opacity->R = 1.0f;
	EditorData->EmissiveColor.Connect(0, Packed);
	EditorData->Opacity.Connect(0, Opacity);
	Material->MaterialDomain = MD_UI;
	Material->BlendMode = BLEND_AlphaComposite;
	Material->SetShadingModel(MSM_Unlit);
	Material->PostEditChange();
	const TArray<FString> Errors = UMaterialEditingLibrary::RecompileMaterial(Material);
	if (!Errors.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Runtime generator compile failed: %s"),
			*FString::Join(Errors, TEXT(" | ")));
		return false;
	}
	Material->MarkPackageDirty();
	return true;
}

UMaterialExpressionNoise* Noise(
	UMaterial* Material,
	UMaterialExpression* Position,
	ENoiseFunction Function,
	uint32 RepeatSize,
	int32 Y)
{
	auto* Result = CreateExpression<UMaterialExpressionNoise>(Material, -260, Y);
	if (Result != nullptr)
	{
		Result->Position.Connect(0, Position);
		Result->Scale = static_cast<float>(RepeatSize);
		Result->Quality = 1;
		Result->NoiseFunction = Function;
		Result->bTurbulence = false;
		Result->Levels = 1;
		Result->OutputMin = 0.0f;
		Result->OutputMax = 1.0f;
		Result->LevelScale = 2.0f;
		Result->bTiling = true;
		Result->RepeatSize = RepeatSize;
	}
	return Result;
}

bool BuildBasisMaterial(UMaterial* Material, int32 Seed, FString& OutError)
{
	auto* Coordinates = CreateExpression<UMaterialExpressionTextureCoordinate>(Material, -980, 0);
	auto* Z = CreateExpression<UMaterialExpressionConstant>(Material, -980, 100);
	auto* Coordinates3D = CreateExpression<UMaterialExpressionAppendVector>(Material, -820, 0);
	auto* SeedVector = CreateExpression<UMaterialExpressionConstant3Vector>(Material, -980, 160);
	auto* Position = CreateExpression<UMaterialExpressionAdd>(Material, -620, 0);
	if (Coordinates == nullptr || Z == nullptr || Coordinates3D == nullptr ||
		SeedVector == nullptr || Position == nullptr)
	{
		OutError = TEXT("Could not create the tile coordinate graph.");
		return false;
	}
	SeedVector->Constant = FLinearColor(
		static_cast<float>(Seed % 17), static_cast<float>(Seed % 29), 0.0f, 1.0f);
	Z->R = 0.0f;
	Coordinates3D->A.Connect(0, Coordinates);
	Coordinates3D->B.Connect(0, Z);
	Position->A.Connect(0, Coordinates3D);
	Position->B.Connect(0, SeedVector);
	auto* Macro = Noise(Material, Position, NOISEFUNCTION_GradientALU, 4, -300);
	auto* Medium = Noise(Material, Position, NOISEFUNCTION_GradientALU, 12, -100);
	auto* Aggregate = Noise(Material, Position, NOISEFUNCTION_VoronoiALU, 24, 100);
	auto* Detail = Noise(Material, Position, NOISEFUNCTION_ValueALU, 64, 300);
	auto* AggregateWeight = CreateExpression<UMaterialExpressionMultiply>(Material, 60, 80);
	auto* DetailWeight = CreateExpression<UMaterialExpressionMultiply>(Material, 60, 240);
	auto* DetailAdd = CreateExpression<UMaterialExpressionAdd>(Material, 260, 160);
	auto* DetailClamp = CreateExpression<UMaterialExpressionClamp>(Material, 440, 160);
	auto* Rg = CreateExpression<UMaterialExpressionAppendVector>(Material, 60, -160);
	auto* Packed = CreateExpression<UMaterialExpressionAppendVector>(Material, 640, 20);
	if (Macro == nullptr || Medium == nullptr || Aggregate == nullptr || Detail == nullptr ||
		AggregateWeight == nullptr || DetailWeight == nullptr || DetailAdd == nullptr ||
		DetailClamp == nullptr || Rg == nullptr || Packed == nullptr)
	{
		OutError = TEXT("Could not create the packed natural noise basis.");
		return false;
	}
	Rg->A.Connect(0, Macro);
	Rg->B.Connect(0, Medium);
	AggregateWeight->A.Connect(0, Aggregate);
	AggregateWeight->ConstB = 0.65f;
	DetailWeight->A.Connect(0, Detail);
	DetailWeight->ConstB = 0.35f;
	DetailAdd->A.Connect(0, AggregateWeight);
	DetailAdd->B.Connect(0, DetailWeight);
	DetailClamp->Input.Connect(0, DetailAdd);
	DetailClamp->MinDefault = 0.0f;
	DetailClamp->MaxDefault = 1.0f;
	Packed->A.Connect(0, Rg);
	Packed->B.Connect(0, DetailClamp);
	return FinalizeGenerator(Material, Packed, OutError);
}

bool BuildGroundMaterial(
	UMaterial* Material,
	UTextureRenderTarget2D* LinearDefault,
	FString& OutError)
{
	auto* Coordinates = CreateExpression<UMaterialExpressionTextureCoordinate>(Material, -1120, 0);
	auto* Parent = CreateExpression<UMaterialExpressionTextureSampleParameter2D>(Material, -920, 0);
	if (Coordinates == nullptr || Parent == nullptr || LinearDefault == nullptr)
	{
		OutError = TEXT("Could not create the cached-parent sampling graph.");
		return false;
	}
	Parent->ParameterName = TEXT("ParentStructure");
	Parent->Texture = LinearDefault;
	Parent->SamplerType = SAMPLERTYPE_LinearColor;
	Parent->Coordinates.Connect(0, Coordinates);
	auto* Macro = Mask(Material, Parent, 0, -660, -260, 5);
	auto* Medium = Mask(Material, Parent, 1, -660, -100, 5);
	auto* BasisDetail = Mask(Material, Parent, 2, -660, 100, 5);
	auto* CoverMedium = CreateExpression<UMaterialExpressionMultiply>(Material, -420, -140);
	auto* CoverMacro = CreateExpression<UMaterialExpressionMultiply>(Material, -420, -20);
	auto* CoverAdd = CreateExpression<UMaterialExpressionAdd>(Material, -180, -80);
	auto* Cover = CreateExpression<UMaterialExpressionClamp>(Material, 220, -40);
	auto* GroundAggregate = CreateExpression<UMaterialExpressionMultiply>(Material, -180, 160);
	auto* GroundMedium = CreateExpression<UMaterialExpressionMultiply>(Material, -180, 280);
	auto* GroundAdd = CreateExpression<UMaterialExpressionAdd>(Material, 20, 220);
	auto* Ground = CreateExpression<UMaterialExpressionClamp>(Material, 220, 220);
	auto* Rg = CreateExpression<UMaterialExpressionAppendVector>(Material, 440, -60);
	auto* Packed = CreateExpression<UMaterialExpressionAppendVector>(Material, 660, 80);
	if (Macro == nullptr || Medium == nullptr || BasisDetail == nullptr ||
		CoverMedium == nullptr || CoverMacro == nullptr || CoverAdd == nullptr || Cover == nullptr ||
		GroundAggregate == nullptr || GroundMedium == nullptr || GroundAdd == nullptr ||
		Ground == nullptr || Rg == nullptr || Packed == nullptr)
	{
		OutError = TEXT("Could not create the natural ground derivation graph.");
		return false;
	}
	CoverMedium->A.Connect(0, Medium);
	CoverMedium->ConstB = 0.75f;
	CoverMacro->A.Connect(0, Macro);
	CoverMacro->ConstB = 0.45f;
	CoverAdd->A.Connect(0, CoverMedium);
	CoverAdd->B.Connect(0, CoverMacro);
	Cover->Input.Connect(0, CoverAdd);
	Cover->MinDefault = 0.0f;
	Cover->MaxDefault = 1.0f;
	GroundAggregate->A.Connect(0, BasisDetail);
	GroundAggregate->ConstB = 0.65f;
	GroundMedium->A.Connect(0, Medium);
	GroundMedium->ConstB = 0.45f;
	GroundAdd->A.Connect(0, GroundAggregate);
	GroundAdd->B.Connect(0, GroundMedium);
	Ground->Input.Connect(0, GroundAdd);
	Ground->MinDefault = 0.0f;
	Ground->MaxDefault = 1.0f;
	Rg->A.Connect(0, Macro);
	Rg->B.Connect(0, Cover);
	Packed->A.Connect(0, Rg);
	Packed->B.Connect(0, Ground);
	return FinalizeGenerator(Material, Packed, OutError);
}

int32 Resolution(const FString& ResolutionClass)
{
	return ResolutionClass == TEXT("low") ? 256 : ResolutionClass == TEXT("high") ? 1024 : 512;
}
}

bool ProjectTextureRuntimeAssetBuilder::Build(
	const FProjectTexturePatternRecipe& Recipe,
	const FString& PackageName,
	UObject*& OutAsset,
	FString& OutError)
{
	using namespace ProjectTextureRuntimeAssetBuilderPrivate;
	OutAsset = nullptr;
	UPackage* Package = CreatePackage(*PackageName);
	if (Package == nullptr || Recipe.Nodes.Num() != 2 || Recipe.Exports.Num() != 1)
	{
		OutError = TEXT("The selected runtime pattern requires one package, two nodes, and one export.");
		return false;
	}
	UMaterial* Basis = CreateAsset<UMaterial>(Package, TEXT("M_ProjectNaturalNoiseBasis"));
	UMaterial* Ground = CreateAsset<UMaterial>(Package, TEXT("M_ProjectNaturalGroundStructure"));
	auto* Catalog = CreateAsset<UProjectTextureRuntimeCatalog>(Package, *Recipe.CatalogId);
	auto* Target = CreateAsset<UTextureRenderTarget2D>(Package, *Recipe.Exports[0].DescriptorId);
	if (Target != nullptr)
	{
		Target->RenderTargetFormat = RTF_RGBA8;
		Target->bForceLinearGamma = true;
		Target->SRGB = false;
	}
	if (Basis == nullptr || Ground == nullptr || Catalog == nullptr || Target == nullptr ||
		!BuildBasisMaterial(Basis, Recipe.Seed, OutError) ||
		!BuildGroundMaterial(Ground, Target, OutError))
	{
		return false;
	}
	const FString OutputPath = FString::Printf(
		TEXT("%s.%s"), *PackageName, *Recipe.Exports[0].DescriptorId);
	Catalog->PatternId = FName(Recipe.PatternId);
	Catalog->SemanticIdentity = FProjectTexturePatternRecipeContract::ComputeArtifactSemanticIdentity(
		Recipe, OutputPath);
	Catalog->OutputContract = FName(Recipe.OutputContract);
	Catalog->BudgetBytes = 8 * 1024 * 1024;
	const int32 Size = Resolution(Recipe.ResolutionClass);
	for (int32 Index = 0; Index < Recipe.Nodes.Num(); ++Index)
	{
		const FProjectTexturePatternNode& Source = Recipe.Nodes[Index];
		FProjectTextureRuntimeNodeDescriptor& Node = Catalog->Nodes.AddDefaulted_GetRef();
		Node.NodeId = FName(Source.NodeId);
		Node.StructuralIdentity = Source.StructuralIdentity;
		for (const FString& Parent : Source.Parents)
		{
			Node.ParentNodeIds.Add(FName(Parent));
		}
		Node.GeneratorMaterial = Index == 0 ? Basis : Ground;
		Node.ParentTextureParameter = Index == 0 ? NAME_None : FName(TEXT("ParentStructure"));
		Node.Width = Size;
		Node.Height = Size;
		Node.Format = RTF_RGBA8;
		Node.bGenerateMips = true;
	}
	Target->RenderTargetFormat = RTF_RGBA8;
	Target->ClearColor = FLinearColor::Transparent;
	Target->bAutoGenerateMips = true;
	Target->AddressX = TA_Wrap;
	Target->AddressY = TA_Wrap;
	Target->MipsAddressU = TA_Wrap;
	Target->MipsAddressV = TA_Wrap;
	Target->MipsSamplerFilter = TF_Trilinear;
	Target->Filter = TF_Trilinear;
	Target->bForceLinearGamma = true;
	Target->InitAutoFormat(Size, Size);
	auto* Slot = NewObject<UProjectTextureOutputSlot>(Target);
	if (Slot == nullptr)
	{
		OutError = TEXT("Could not attach the runtime Structure output contract.");
		return false;
	}
	Slot->Catalog = Catalog;
	Slot->SlotName = FName(Recipe.Exports[0].Slot);
	Slot->NodeId = FName(Recipe.Exports[0].NodeId);
	Target->AddAssetUserData(Slot);
	const TArray<UObject*> Assets = {Basis, Ground, Catalog, Target};
	for (UObject* Asset : Assets)
	{
		Package->GetMetaData().SetValue(Asset, OwnerKey, TEXT("ProjectTexture"));
		Package->GetMetaData().SetValue(Asset, PatternKey, *Recipe.PatternId);
		Asset->MarkPackageDirty();
	}
	OutAsset = Target;
	return true;
}

bool ProjectTextureRuntimeAssetBuilder::Verify(
	const FProjectTexturePatternRecipe& Recipe,
	UObject* Asset,
	FString& OutError)
{
	using namespace ProjectTextureRuntimeAssetBuilderPrivate;
	UTextureRenderTarget2D* Target = Cast<UTextureRenderTarget2D>(Asset);
	const UProjectTextureOutputSlot* Slot = Target != nullptr
		? Cast<UProjectTextureOutputSlot>(Target->GetAssetUserDataOfClass(UProjectTextureOutputSlot::StaticClass()))
		: nullptr;
	const UProjectTextureRuntimeCatalog* Catalog = Slot != nullptr ? Slot->Catalog : nullptr;
	if (Target == nullptr || Slot == nullptr || Catalog == nullptr || Catalog->Nodes.Num() != 2 ||
		Catalog->OutputContract != FName(Recipe.OutputContract) ||
		Slot->SlotName != TEXT("Structure") || Slot->GetRenderTarget() != Target ||
		Target->RenderTargetFormat != RTF_RGBA8 ||
		Target->SRGB || Target->IsSRGB() ||
		Target->AddressX != TA_Wrap || Target->AddressY != TA_Wrap ||
		Target->MipsAddressU != TA_Wrap || Target->MipsAddressV != TA_Wrap ||
		Target->Filter != TF_Trilinear || Target->MipsSamplerFilter != TF_Trilinear ||
		!Target->bAutoGenerateMips || Catalog->SemanticIdentity.Len() != 64)
	{
		OutError = TEXT("Reloaded runtime catalog, output slot, or wrap/filter/mip contract is invalid.");
		return false;
	}
	int32 TilingSignals = 0;
	for (const FProjectTextureRuntimeNodeDescriptor& Node : Catalog->Nodes)
	{
		if (Node.StructuralIdentity.Len() != 64 || Node.GeneratorMaterial == nullptr ||
			Node.Width != Resolution(Recipe.ResolutionClass) || Node.Height != Node.Width ||
			Node.Format != RTF_RGBA8 || !Node.bGenerateMips)
		{
			OutError = TEXT("Reloaded runtime node contract is invalid.");
			return false;
		}
		const UMaterial* Generator = Cast<UMaterial>(Node.GeneratorMaterial);
		if (Generator == nullptr)
		{
			OutError = TEXT("Reloaded runtime node generator is not a material.");
			return false;
		}
		const UMaterialEditorOnlyData* GeneratorGraph = Generator->GetEditorOnlyData();
		const UMaterialExpressionConstant* GeneratorOpacity = GeneratorGraph != nullptr
			? Cast<UMaterialExpressionConstant>(GeneratorGraph->Opacity.Expression) : nullptr;
		if (Generator->MaterialDomain != MD_UI || Generator->BlendMode != BLEND_AlphaComposite ||
			GeneratorOpacity == nullptr || !FMath::IsNearlyEqual(GeneratorOpacity->R, 1.0f))
		{
			OutError = TEXT("Reloaded runtime generator must write RGB with constant opacity.");
			return false;
		}
		if (!Node.ParentTextureParameter.IsNone())
		{
			int32 ParentSamples = 0;
			for (UMaterialExpression* Expression : UMaterialEditingLibrary::GetMaterialExpressions(
				const_cast<UMaterial*>(Generator)))
			{
				const UMaterialExpressionTextureSampleParameter2D* ParentSample =
					Cast<UMaterialExpressionTextureSampleParameter2D>(Expression);
				if (ParentSample != nullptr && ParentSample->ParameterName == Node.ParentTextureParameter)
				{
					++ParentSamples;
					if (ParentSample->SamplerType != SAMPLERTYPE_LinearColor ||
						ParentSample->Texture != Target)
					{
						OutError = TEXT("Reloaded DAG parent must use the linear Structure output as data.");
						return false;
					}
				}
			}
			if (ParentSamples != 1)
			{
				OutError = TEXT("Reloaded DAG child must contain one matching linear parent sample.");
				return false;
			}
		}
		for (const UMaterialExpression* Expression :
			UMaterialEditingLibrary::GetMaterialExpressions(Generator))
		{
			if (const auto* NoiseExpression = Cast<UMaterialExpressionNoise>(Expression))
			{
				TilingSignals += NoiseExpression->bTiling && NoiseExpression->RepeatSize >= 4 ? 1 : 100;
			}
		}
	}
	FMetaData& MetaData = Target->GetOutermost()->GetMetaData();
	if (TilingSignals != 4 || MetaData.GetValue(Target, OwnerKey) != TEXT("ProjectTexture") ||
		MetaData.GetValue(Target, PatternKey) != Recipe.PatternId)
	{
		OutError = TEXT("Reloaded runtime generator topology or ownership metadata is invalid.");
		return false;
	}
	return true;
}
