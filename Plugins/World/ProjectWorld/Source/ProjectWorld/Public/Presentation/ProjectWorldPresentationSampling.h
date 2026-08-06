// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class UWorld;

// Pure decision policy for the packaged Presentation Gate, extracted so the
// contract has focused automation coverage independent of the expensive
// packaged rendered run. Consumed by FProjectWorldPresentationGate on every
// warmup and sampling tick.
namespace ProjectWorldPresentation
{
	// One inspection of the currently loaded world state.
	struct FRuntimeRoleScan
	{
		bool bValid = false;
		FString Error;
		TSet<FString> LoadedRoles;
	};

	// Inspects every loaded actor carrying a ProjectWorld.RuntimeRole tag.
	// Stale profile/hash ownership or a role duplicated in one loaded state
	// invalidates the scan; a valid scan returns the loaded role set for
	// cross-viewpoint accumulation.
	PROJECTWORLD_API FRuntimeRoleScan ScanRuntimeRoles(
		UWorld& World,
		const FString& RuntimeProfileId,
		const FString& RuntimeProfileHash);

	// A sampled frame duration is valid only when finite and positive. Stalls
	// of any magnitude remain valid samples and must never be filtered; an
	// invalid value is not a rendered frame and must reject the run instead
	// of silently extending the sample window.
	PROJECTWORLD_API bool IsValidSampleFrameMs(double FrameTimeMs);

	// Required route roles not yet present in the accumulated observation set.
	PROJECTWORLD_API TArray<FString> MissingRequiredRoles(const TSet<FString>& ObservedRoles);
}
