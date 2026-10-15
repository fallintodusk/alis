// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
class UMaterialInstanceConstant;
struct FProjectMaterialSurfaceRecipe;
namespace ProjectMaterialPreviewAuthority
{
bool Load(const FString& Path, const FString& Root, const FString& LayoutReceipt,
	FProjectMaterialSurfaceRecipe& OutRecipe, UMaterialInstanceConstant*& OutBaseline, FString& OutError,
	const FString& FixtureRoot = FString(), const FString& PatternTestRoot = FString());
}
