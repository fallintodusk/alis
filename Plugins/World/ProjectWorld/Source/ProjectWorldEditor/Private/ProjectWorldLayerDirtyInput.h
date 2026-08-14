// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldLayerBaseIdentity
{
	FString NormalizedLayerContractHash;
	TMap<FString, FString> CanonicalInputs;
};

struct FProjectWorldLayerDirtyInput
{
	FString RealizationProfileId;
	FString InputHash;
	TMap<FString, FProjectWorldLayerBaseIdentity> BaseLayers;
	TMap<FString, TSet<FString>> OperatorAdditions;
};

namespace ProjectWorldLayerDirtyInput
{
	bool Load(
		const FString& Path,
		FProjectWorldLayerDirtyInput& OutInput,
		FString& OutErrorCode,
		FString& OutError);
}
