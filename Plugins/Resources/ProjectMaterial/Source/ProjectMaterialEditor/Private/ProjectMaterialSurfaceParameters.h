// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
class UMaterialInterface;
struct FProjectMaterialSurfaceRecipe;
namespace ProjectMaterialSurfaceParameters
{
bool Validate(const FProjectMaterialSurfaceRecipe& Recipe, UMaterialInterface* Parent, FString& OutError);
}
