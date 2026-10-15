// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
class UMaterialInterface;
class UMaterialInstanceConstant;
class UTextureRenderTarget2D;
struct FProjectMaterialSurfaceDependency;
struct FProjectMaterialSurfaceRecipe;

namespace ProjectMaterialPatternBindings
{
FString Contract(const FProjectMaterialSurfaceDependency&);
struct FBinding
{
	FName Parameter;
	UTextureRenderTarget2D* Target = nullptr;
};
bool Resolve(const FProjectMaterialSurfaceDependency&, TArray<FBinding>&, FString&);
bool ValidateSource(const FProjectMaterialSurfaceRecipe&, const FProjectMaterialSurfaceDependency&, FString&);
bool ValidateParent(const FProjectMaterialSurfaceRecipe&, const FProjectMaterialSurfaceDependency&,
	UMaterialInterface*, TArray<FBinding>&, FString&);
bool VerifyInstance(const FProjectMaterialSurfaceRecipe&, const FProjectMaterialSurfaceDependency&,
	const UMaterialInstanceConstant*, FString&);
bool VerifyBindings(const FProjectMaterialSurfaceRecipe&, const FProjectMaterialSurfaceDependency&,
	const UMaterialInstanceConstant*, FString&);
}
