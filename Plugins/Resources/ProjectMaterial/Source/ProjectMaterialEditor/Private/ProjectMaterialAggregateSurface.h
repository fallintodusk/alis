#pragma once
#include "CoreMinimal.h"
class UMaterial;
struct FProjectMaterialSurfaceRecipe;
struct FProjectMaterialSurfaceDependency;
namespace ProjectMaterialAggregateSurface
{
	bool Build(UMaterial*,const FProjectMaterialSurfaceRecipe&,const FProjectMaterialSurfaceDependency&,TArray<FString>&,FString&);
	bool Verify(const FProjectMaterialSurfaceRecipe&,const FProjectMaterialSurfaceDependency&,const UObject*,FString&);
}
