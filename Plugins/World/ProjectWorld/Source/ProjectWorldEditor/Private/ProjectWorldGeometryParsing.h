// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;
struct FProjectWorldCanonicalValidation;

namespace ProjectWorldGeometryParsing
{
	bool ReadGeometry(
		const TSharedPtr<FJsonObject>& Geometry,
		FString& OutType,
		TArray<FVector2D>& OutOuterPoints,
		FProjectWorldCanonicalValidation& OutValidation,
		TArray<TArray<FVector2D>>* OutParts = nullptr);
}
