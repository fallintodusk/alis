// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
class UMaterial;
class UMaterialExpression;
class UMaterialExpressionTextureSampleParameter2D;
class UTexture;
namespace ProjectMaterialAggregateVariation
{
bool Bind(UMaterial*,UMaterialExpression* UV,UMaterialExpression* Bias,UTexture* Signals,
    UMaterialExpressionTextureSampleParameter2D*,UMaterialExpressionTextureSampleParameter2D*,FString&);
bool Sample(const UMaterialExpressionTextureSampleParameter2D*,const UTexture*);
bool Verify(UMaterial*,const UTexture* Signals);
}
