// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"

struct FProjectMaterialSurfaceRecipe;
struct FProjectMaterialPatternDependency;
class UMaterial;
class UObject;

namespace ProjectMaterialSoilBuilder
{
bool Build(UMaterial* Material, const FProjectMaterialSurfaceRecipe& Recipe,
	const FProjectMaterialPatternDependency& Pattern, TArray<FString>& OutErrors, FString& OutError);
bool Verify(const FProjectMaterialSurfaceRecipe& Recipe,
	const FProjectMaterialPatternDependency& Pattern, const UObject* Asset, FString& OutError);
}
