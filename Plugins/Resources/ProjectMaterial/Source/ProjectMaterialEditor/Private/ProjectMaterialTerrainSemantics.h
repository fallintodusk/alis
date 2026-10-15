// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
class UMaterial;
class UMaterialExpression;
struct FProjectMaterialSurfaceRecipe;

namespace ProjectMaterialTerrainSemantics
{
UMaterialExpression* ComposeColor(UMaterial*, const FProjectMaterialSurfaceRecipe&,
    UMaterialExpression* GroundColor, const TMap<FName,int32>& Channels, FString& Error);
}
