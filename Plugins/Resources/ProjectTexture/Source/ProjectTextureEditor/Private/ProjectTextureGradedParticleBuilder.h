// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
struct FProjectTexturePatternRecipe;
class UObject;
namespace ProjectTextureGradedParticleBuilder
{
bool Build(const FProjectTexturePatternRecipe& Recipe,const FString& Package,UObject*& Asset,FString& Error);
bool Verify(const FProjectTexturePatternRecipe& Recipe,UObject* Asset,FString& Error);
}
