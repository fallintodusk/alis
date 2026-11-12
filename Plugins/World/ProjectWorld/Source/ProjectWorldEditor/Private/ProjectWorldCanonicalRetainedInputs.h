// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "CoreMinimal.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldIdentifiedBasinParsing.h"
#include "ProjectWorldAuthoredTerrainParsing.h"
struct FProjectWorldCanonicalValidation;
namespace ProjectWorldCanonicalRefinement
{
	struct FRetainedContext { FString NormalizedPath,NativePath; };
	struct FRetainedFeatureInputs
	{
		FString PolicyHash;
		TMap<FString,FRetainedContext> Contexts;
		FProjectWorldCanonicalConstructedPolicy Policy;
	};
	using FRetainedFeatureContexts=TMap<FString,FRetainedFeatureInputs>;
	bool ValidateRetainedInputs(const FString& Root,bool Refinement,bool HasRequest,
		const TSet<FString>& EvidencePaths,
		TFunctionRef<bool(const FString&,const FString&,int64,bool)> IsVerifiedOutput,
		TMap<FString,FString>& OutRoles,FRetainedFeatureContexts& OutContexts,
		ProjectWorldIdentifiedBasinParsing::FRetainedInputs& OutBasins,
		ProjectWorldAuthoredTerrainParsing::FRetainedInputs& OutAuthored,FProjectWorldCanonicalValidation& Validation);
}
