// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;
struct FProjectWorldCanonicalFeature;
struct FProjectWorldCanonicalValidation;

namespace ProjectWorldWaterContractParsing
{
	bool Read(
		const TSharedPtr<FJsonObject>& Attributes,
		FProjectWorldCanonicalFeature& OutFeature,
		FProjectWorldCanonicalValidation& OutValidation);
}
