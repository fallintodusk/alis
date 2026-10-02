// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "ProjectTexturePatternRecipe.h"

class UObject;

namespace ProjectTextureRuntimeAssetBuilder
{
bool Build(
	const FProjectTexturePatternRecipe& Recipe,
	const FString& PackageName,
	UObject*& OutAsset,
	FString& OutError);

bool Verify(
	const FProjectTexturePatternRecipe& Recipe,
	UObject* Asset,
	FString& OutError);
}
