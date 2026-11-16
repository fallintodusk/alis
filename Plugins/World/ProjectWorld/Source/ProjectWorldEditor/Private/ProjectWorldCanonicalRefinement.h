#pragma once

#include "CoreMinimal.h"
#include "ProjectWorldAuthoredTerrainParsing.h"
#include "ProjectWorldShoreTransitionParsing.h"

class FJsonObject;
struct FProjectWorldCanonicalTerrain;
struct FProjectWorldCanonicalBundle;
struct FProjectWorldCanonicalValidation;

namespace ProjectWorldCanonicalRefinement
{
	bool ResolveBase(const FString& Root, TSharedPtr<FJsonObject>& TerrainObject,
		TSharedPtr<FJsonObject>& RefinementObject,
		TFunctionRef<bool(const FString&, const FString&, int64)> IsVerifiedOutput,
		FProjectWorldCanonicalValidation& Validation);
	bool Parse(const TSharedPtr<FJsonObject>& Object, const FString& Grid, const FString& Cell,
		FProjectWorldCanonicalTerrain& Terrain, FProjectWorldCanonicalValidation& Validation,
		const ProjectWorldAuthoredTerrainParsing::FRetainedInputs& AuthoredInputs,TSet<FString>& UsedAuthoredInputs,
		ProjectWorldShoreTransitionParsing::FInputs& ShoreInputs);
	bool ValidateSeams(const FProjectWorldCanonicalBundle& Bundle, FProjectWorldCanonicalValidation& Validation);
}
