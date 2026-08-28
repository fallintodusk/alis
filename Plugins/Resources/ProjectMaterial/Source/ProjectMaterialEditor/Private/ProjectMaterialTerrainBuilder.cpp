// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectMaterialTerrainBuilder.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionPower.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"

namespace ProjectMaterialTerrainBuilderPrivate
{
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
	bool& bOutCreated,
	FString& OutError)
{
	bOutCreated = false;
	if (FPackageName::DoesPackageExist(PackageName))
	{
		const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *PackageName, *AssetName);
		UObject* Existing = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath);
		if (Existing == nullptr || !Existing->IsA(AssetClass))
		{
			OutError = FString::Printf(TEXT("Existing output cannot be loaded with the expected class: %s"), *ObjectPath);
			return nullptr;
		}
		return Existing;
	}
	UPackage* Package = CreatePackage(*PackageName);
	if (Package == nullptr)
	{
		OutError = FString::Printf(TEXT("Could not create package: %s"), *PackageName);
		return nullptr;
	}
	if (UObject* Existing = StaticFindObject(UObject::StaticClass(), Package, *AssetName))
	{
		if (!Existing->IsA(AssetClass))
		{
			OutError = FString::Printf(TEXT("Existing output has the wrong class: %s"), *PackageName);
			return nullptr;
		}
		return Existing;
	}
	UObject* Asset = NewObject<UObject>(Package, AssetClass, *AssetName, RF_Public | RF_Standalone);
	if (Asset == nullptr)
	{
		OutError = FString::Printf(TEXT("Could not create output asset: %s"), *PackageName);
		return nullptr;
	}
	bOutCreated = true;
	FAssetRegistryModule::AssetCreated(Asset);
	return Asset;
}
}

bool ProjectMaterialTerrainBuilder::BuildParent(
	const FProjectMaterialRecipe& Recipe,
	const FString& PackageName,
	UObject*& OutAsset,
	TArray<FString>& OutCompileErrors,
	FString& OutError)
{
	bool bCreated = false;
	UMaterial* Material = Cast<UMaterial>(ProjectMaterialTerrainBuilderPrivate::FindOrCreateAsset(
		PackageName, Recipe.MaterialId, UMaterial::StaticClass(), bCreated, OutError));
	if (Material == nullptr)
	{
		return false;
	}
	UMaterialEditingLibrary::DeleteAllMaterialExpressions(Material);
	auto* Normal = ProjectMaterialTerrainBuilderPrivate::CreateExpression<UMaterialExpressionVertexNormalWS>(Material, -800, -200);
	auto* Mask = ProjectMaterialTerrainBuilderPrivate::CreateExpression<UMaterialExpressionComponentMask>(Material, -620, -200);
	auto* OneMinus = ProjectMaterialTerrainBuilderPrivate::CreateExpression<UMaterialExpressionOneMinus>(Material, -440, -200);
	auto* SlopeContrast = ProjectMaterialTerrainBuilderPrivate::CreateExpression<UMaterialExpressionScalarParameter>(Material, -440, 20);
	auto* Power = ProjectMaterialTerrainBuilderPrivate::CreateExpression<UMaterialExpressionPower>(Material, -240, -180);
	auto* LowColor = ProjectMaterialTerrainBuilderPrivate::CreateExpression<UMaterialExpressionVectorParameter>(Material, -220, -420);
	auto* SteepColor = ProjectMaterialTerrainBuilderPrivate::CreateExpression<UMaterialExpressionVectorParameter>(Material, -220, -320);
	auto* Lerp = ProjectMaterialTerrainBuilderPrivate::CreateExpression<UMaterialExpressionLinearInterpolate>(Material, 20, -240);
	auto* Roughness = ProjectMaterialTerrainBuilderPrivate::CreateExpression<UMaterialExpressionScalarParameter>(Material, 20, 80);
	if (Normal == nullptr || Mask == nullptr || OneMinus == nullptr || SlopeContrast == nullptr ||
		Power == nullptr || LowColor == nullptr || SteepColor == nullptr || Lerp == nullptr || Roughness == nullptr)
	{
		OutError = TEXT("The landscape_basic_v1 expression graph could not be created.");
		return false;
	}
	Mask->R = false;
	Mask->G = false;
	Mask->B = true;
	Mask->A = false;
	SlopeContrast->ParameterName = TEXT("SlopeContrast");
	SlopeContrast->DefaultValue = Recipe.Scalars[TEXT("SlopeContrast")];
	SlopeContrast->SliderMin = 0.01f;
	SlopeContrast->SliderMax = 16.0f;
	LowColor->ParameterName = TEXT("LowSlopeColor");
	LowColor->DefaultValue = Recipe.Vectors[TEXT("LowSlopeColor")];
	SteepColor->ParameterName = TEXT("SteepSlopeColor");
	SteepColor->DefaultValue = Recipe.Vectors[TEXT("SteepSlopeColor")];
	Roughness->ParameterName = TEXT("Roughness");
	Roughness->DefaultValue = Recipe.Scalars[TEXT("Roughness")];
	Roughness->SliderMin = 0.0f;
	Roughness->SliderMax = 1.0f;
	Mask->Input.Connect(0, Normal);
	OneMinus->Input.Connect(0, Mask);
	Power->Base.Connect(0, OneMinus);
	Power->Exponent.Connect(0, SlopeContrast);
	Lerp->A.Connect(0, LowColor);
	Lerp->B.Connect(0, SteepColor);
	Lerp->Alpha.Connect(0, Power);
	UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData();
	if (EditorData == nullptr)
	{
		OutError = TEXT("The landscape_basic_v1 material has no Editor graph data.");
		return false;
	}
	EditorData->BaseColor.Connect(0, Lerp);
	EditorData->Roughness.Connect(0, Roughness);
	Material->BlendMode = BLEND_Opaque;
	Material->MaterialDomain = MD_Surface;
	Material->SetShadingModel(MSM_DefaultLit);
	Material->PostEditChange();
	OutCompileErrors = UMaterialEditingLibrary::RecompileMaterial(Material);
	if (!OutCompileErrors.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Material compile failed: %s"), *FString::Join(OutCompileErrors, TEXT(" | ")));
		return false;
	}
	Material->MarkPackageDirty();
	OutAsset = Material;
	return true;
}

bool ProjectMaterialTerrainBuilder::BuildInstance(
	const FProjectMaterialRecipe& Recipe,
	const FString& PackageName,
	UObject*& OutAsset,
	FString& OutError)
{
	UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, *Recipe.ParentObjectPath);
	if (Parent == nullptr)
	{
		OutError = FString::Printf(TEXT("Instance parent cannot be loaded: %s"), *Recipe.ParentObjectPath);
		return false;
	}
	bool bCreated = false;
	UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(
		ProjectMaterialTerrainBuilderPrivate::FindOrCreateAsset(
			PackageName, Recipe.MaterialId, UMaterialInstanceConstant::StaticClass(), bCreated, OutError));
	if (Instance == nullptr)
	{
		return false;
	}
	Instance->ClearParameterValuesEditorOnly();
	UMaterialEditingLibrary::SetMaterialInstanceParent(Instance, Parent);
	for (const TPair<FName, double>& Scalar : Recipe.Scalars)
	{
		float ParentValue = 0.0f;
		if (!Parent->GetScalarParameterValue(FHashedMaterialParameterInfo(Scalar.Key), ParentValue))
		{
			OutError = FString::Printf(TEXT("Unknown scalar parameter on parent: %s"), *Scalar.Key.ToString());
			return false;
		}
		Instance->SetScalarParameterValueEditorOnly(
			FMaterialParameterInfo(Scalar.Key), Scalar.Value);
	}
	for (const TPair<FName, FLinearColor>& Vector : Recipe.Vectors)
	{
		FLinearColor ParentValue;
		if (!Parent->GetVectorParameterValue(FHashedMaterialParameterInfo(Vector.Key), ParentValue))
		{
			OutError = FString::Printf(TEXT("Unknown vector parameter on parent: %s"), *Vector.Key.ToString());
			return false;
		}
		Instance->SetVectorParameterValueEditorOnly(
			FMaterialParameterInfo(Vector.Key), Vector.Value);
	}
	UMaterialEditingLibrary::UpdateMaterialInstance(Instance);
	Instance->PostEditChange();
	Parent->GetOutermost()->SetDirtyFlag(false);
	Instance->MarkPackageDirty();
	OutAsset = Instance;
	return true;
}

bool ProjectMaterialTerrainBuilder::Verify(
	const FProjectMaterialRecipe& Recipe,
	UObject* Asset,
	FString& OutError)
{
	if (Recipe.ArtifactKind == EProjectMaterialArtifactKind::Parent)
	{
		const UMaterial* Material = Cast<UMaterial>(Asset);
		if (Material == nullptr)
		{
			OutError = TEXT("Reloaded parent is not a material.");
			return false;
		}
		const TArray<UMaterialExpression*> Expressions = UMaterialEditingLibrary::GetMaterialExpressions(Material);
		TSet<FName> Parameters;
		for (const UMaterialExpression* Expression : Expressions)
		{
			if (const UMaterialExpressionParameter* Parameter = Cast<UMaterialExpressionParameter>(Expression))
			{
				Parameters.Add(Parameter->ParameterName);
			}
		}
		if (Expressions.Num() != 9 || Parameters.Num() != 4 ||
			!Parameters.Contains(TEXT("Roughness")) || !Parameters.Contains(TEXT("SlopeContrast")) ||
			!Parameters.Contains(TEXT("LowSlopeColor")) || !Parameters.Contains(TEXT("SteepSlopeColor")))
		{
			OutError = TEXT("Reloaded landscape_basic_v1 graph shape does not match the closed archetype.");
			return false;
		}
		return true;
	}
	const UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(Asset);
	if (Instance == nullptr || Instance->Parent == nullptr ||
		Instance->Parent->GetPathName() != Recipe.ParentObjectPath)
	{
		OutError = TEXT("Reloaded instance parent does not match the recipe.");
		return false;
	}
	for (const TPair<FName, double>& Scalar : Recipe.Scalars)
	{
		float Actual = 0.0f;
		if (!Instance->GetScalarParameterValue(FHashedMaterialParameterInfo(Scalar.Key), Actual, true) ||
			!FMath::IsNearlyEqual(Actual, static_cast<float>(Scalar.Value)))
		{
			OutError = FString::Printf(TEXT("Reloaded scalar parameter does not match: %s"), *Scalar.Key.ToString());
			return false;
		}
	}
	for (const TPair<FName, FLinearColor>& Vector : Recipe.Vectors)
	{
		FLinearColor Actual;
		if (!Instance->GetVectorParameterValue(FHashedMaterialParameterInfo(Vector.Key), Actual, true) ||
			!Actual.Equals(Vector.Value))
		{
			OutError = FString::Printf(TEXT("Reloaded vector parameter does not match: %s"), *Vector.Key.ToString());
			return false;
		}
	}
	return true;
}
