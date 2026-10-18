// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
class FJsonObject;
struct FProjectWorldCanonicalFeature;
struct FProjectWorldCanonicalValidation;
namespace ProjectWorldRoadContractParsing
{
	bool Read(const TSharedPtr<FJsonObject>& Attributes, FProjectWorldCanonicalFeature& Feature,
		FProjectWorldCanonicalValidation& Validation);
}
