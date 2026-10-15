// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
class UMaterial;
class UObject;
struct FProjectMaterialSurfaceRecipe;
struct FProjectMaterialSurfaceDependency;

namespace ProjectMaterialMetricHeightSurface
{
bool Build(UMaterial*,const FProjectMaterialSurfaceRecipe&,const FProjectMaterialSurfaceDependency&,TArray<FString>&,FString&,
    const TMap<FName,int32>& Channels = {});
bool Verify(const FProjectMaterialSurfaceRecipe&,const FProjectMaterialSurfaceDependency&,const UObject*,FString&,
    const TMap<FName,int32>& Channels = {});
}
