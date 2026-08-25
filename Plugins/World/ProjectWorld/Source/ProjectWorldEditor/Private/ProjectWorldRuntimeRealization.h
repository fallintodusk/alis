// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldCanonicalBundle;
struct FProjectWorldRealizationResult;
struct FProjectWorldRuntimeProfile;
class AActor;
class UWorld;

namespace ProjectWorldRuntimeRealization
{
	bool IsRuntimeRoleActor(const AActor& Actor);

	bool IsCurrentRuntimeActorForApply(
		const AActor& Actor,
		const FProjectWorldCanonicalBundle& Bundle,
		bool bRuntimeRequested);

	bool Validate(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRuntimeProfile& Profile,
		FString& OutError);

	bool Apply(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRuntimeProfile& Profile,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError);

	bool CaptureAndCheckStructuralBudgets(
		UWorld* World,
		const FProjectWorldRuntimeProfile& Profile,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError);
}
