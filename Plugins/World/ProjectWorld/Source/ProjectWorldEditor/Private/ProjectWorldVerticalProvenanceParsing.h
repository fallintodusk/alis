// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"

class FJsonObject;
struct FProjectWorldCanonicalTerrain;
struct FProjectWorldCanonicalValidation;

namespace ProjectWorldVerticalProvenanceParsing
{
	bool Read(const TSharedPtr<FJsonObject>& Object, FProjectWorldCanonicalTerrain& OutTerrain,
		FProjectWorldCanonicalValidation& Validation);
}
