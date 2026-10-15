#pragma once
#include "CoreMinimal.h"
struct FProjectTexturePatternRecipe;
namespace ProjectTextureAggregateBuilder
{
	bool Build(const FProjectTexturePatternRecipe& Recipe, const FString& PackageName, UObject*& OutAsset, FString& OutError);
	bool Verify(const FProjectTexturePatternRecipe& Recipe, UObject* Asset, FString& OutError);
}
