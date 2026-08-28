// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "ProjectMaterialRecipe.h"

class UObject;

namespace ProjectMaterialTerrainBuilder
{
bool BuildParent(
	const FProjectMaterialRecipe& Recipe,
	const FString& PackageName,
	UObject*& OutAsset,
	TArray<FString>& OutCompileErrors,
	FString& OutError);

bool BuildInstance(
	const FProjectMaterialRecipe& Recipe,
	const FString& PackageName,
	UObject*& OutAsset,
	FString& OutError);

bool Verify(const FProjectMaterialRecipe& Recipe, UObject* Asset, FString& OutError);
}
