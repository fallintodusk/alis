// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldCanonicalBundle;
struct FProjectWorldRealizationProfile;
struct FProjectWorldRealizationResult;
class AActor;
class UWorld;

namespace ProjectWorldWaterRealization
{
	bool Apply(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRealizationProfile& Profile,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError);

	bool ReadActorIdentity(
		const AActor* Actor,
		FString& OutCellId,
		FString& OutSemanticHash);
}
