// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once

#include "CoreMinimal.h"

struct FProjectTexturePatternRecipe;
class UObject;
class UMaterial;
class UTextureRenderTarget2D;

namespace ProjectTextureSoilBuilder
{
bool Build(const FProjectTexturePatternRecipe& Recipe, const FString& PackageName,
	UObject*& OutAsset, FString& OutError);
bool Verify(const FProjectTexturePatternRecipe& Recipe, UObject* Asset, FString& OutError);
bool BuildDerivedMaterial(UMaterial* Material, UTextureRenderTarget2D* Parent,
	int32 Size, bool bNormal, FString& OutError);
}
