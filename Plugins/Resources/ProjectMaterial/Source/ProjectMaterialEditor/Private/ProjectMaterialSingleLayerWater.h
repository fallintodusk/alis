#pragma once
#include "CoreMinimal.h"
class UMaterial;
struct FProjectMaterialSurfaceRecipe;
struct FProjectMaterialSurfaceDependency;
namespace ProjectMaterialSingleLayerWater
{
	bool ResolveResources(FProjectMaterialSurfaceDependency& OutResources, FString& OutError);
	bool Build(UMaterial* Material, const FProjectMaterialSurfaceRecipe& Recipe,
		TArray<FString>& OutCompileErrors, FString& OutError);
	bool Verify(const FProjectMaterialSurfaceRecipe& Recipe, const UObject* Asset, FString& OutError);
}
