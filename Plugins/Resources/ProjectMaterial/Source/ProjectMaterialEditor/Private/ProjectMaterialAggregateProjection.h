// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
class UMaterial;
class UMaterialExpression;
class UTexture;
struct FProjectMaterialSurfaceRecipe;
namespace ProjectMaterialAggregateProjection
{
    struct FOutputs
    {
        UMaterialExpression* Signals;
        UMaterialExpression* Normal;
        UMaterialExpression* Macro;
    };
    FOutputs Build(UMaterial*,const FProjectMaterialSurfaceRecipe&,UTexture*,UTexture*);
    bool Signals(const UMaterialExpression*,UTexture*,const TCHAR* Scale);
    bool Normal(const UMaterialExpression*,UTexture*);
}
