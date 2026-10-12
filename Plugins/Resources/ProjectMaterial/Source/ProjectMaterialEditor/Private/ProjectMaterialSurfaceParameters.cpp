// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectMaterialSurfaceParameters.h"
#include "ProjectMaterialSurfaceRecipe.h"
#include "Materials/MaterialInterface.h"

bool ProjectMaterialSurfaceParameters::Validate(
	const FProjectMaterialSurfaceRecipe& Recipe, UMaterialInterface* Parent, FString& OutError)
{
	if (!Parent)
	{
		OutError = TEXT("Surface instance parent is unavailable.");
		return false;
	}
	for (const auto& Value : Recipe.Scalars)
	{
		float Existing = 0.0f;
		if (!FMath::IsFinite(Value.Value) || !FMath::IsFinite(static_cast<float>(Value.Value)) ||
			!Parent->GetScalarParameterValue(FHashedMaterialParameterInfo(Value.Key), Existing))
		{
			OutError = FString::Printf(TEXT("Invalid or unknown surface scalar parameter: %s"), *Value.Key.ToString());
			return false;
		}
	}
	for (const auto& Value : Recipe.Vectors)
	{
		FLinearColor Existing;
		if (!FMath::IsFinite(Value.Value.R) || !FMath::IsFinite(Value.Value.G) ||
			!FMath::IsFinite(Value.Value.B) || !FMath::IsFinite(Value.Value.A) ||
			!Parent->GetVectorParameterValue(FHashedMaterialParameterInfo(Value.Key), Existing))
		{
			OutError = FString::Printf(TEXT("Invalid or unknown surface vector parameter: %s"), *Value.Key.ToString());
			return false;
		}
	}
	return true;
}
