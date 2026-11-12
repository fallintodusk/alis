// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
class FJsonObject;
struct FProjectWorldCanonicalFeature;
struct FProjectWorldCanonicalValidation;
struct FProjectWorldCanonicalConstructedPolicy;
namespace ProjectWorldConstructedContractParsing
{
	bool Read(const TSharedPtr<FJsonObject>& Attributes, FProjectWorldCanonicalFeature& Feature,
		FProjectWorldCanonicalValidation& Validation);
	bool ReadPolicy(const TSharedPtr<FJsonObject>& Document,FProjectWorldCanonicalConstructedPolicy& Policy);
}
