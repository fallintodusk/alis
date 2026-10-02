// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "ProjectMaterialSurfaceRecipe.h"

class UObject;
struct FProjectMaterialPatternDependency;

namespace ProjectMaterialSurfaceBuilder
{
bool BuildParent(
	const FProjectMaterialSurfaceRecipe& Recipe,
	const FProjectMaterialPatternDependency& Pattern,
	const FString& PackageName,
	const TMap<FName, int32>& SemanticChannels,
	UObject*& OutAsset,
	TArray<FString>& OutCompileErrors,
	FString& OutError);

bool BuildInstance(
	const FProjectMaterialSurfaceRecipe& Recipe,
	const FString& PackageName,
	UObject*& OutAsset,
	FString& OutError);

bool Verify(
	const FProjectMaterialSurfaceRecipe& Recipe,
	const FProjectMaterialPatternDependency& Pattern,
	const TMap<FName, int32>& SemanticChannels,
	UObject* Asset,
	FString& OutError);
}
