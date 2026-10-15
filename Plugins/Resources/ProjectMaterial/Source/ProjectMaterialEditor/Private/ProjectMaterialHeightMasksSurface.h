// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"

struct FProjectMaterialSurfaceRecipe;
struct FProjectMaterialSurfaceDependency;
class UMaterial;
class UObject;

namespace ProjectMaterialHeightMasksSurface
{
bool Build(UMaterial* Material, const FProjectMaterialSurfaceRecipe& Recipe,
	const FProjectMaterialSurfaceDependency& Pattern, TArray<FString>& OutErrors, FString& OutError);
bool Verify(const FProjectMaterialSurfaceRecipe& Recipe,
	const FProjectMaterialSurfaceDependency& Pattern, const UObject* Asset, FString& OutError);
}
