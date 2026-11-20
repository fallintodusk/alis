#pragma once
#include "CoreMinimal.h"

class UWorld;
struct FProjectWorldCanonicalBundle;
struct FProjectWorldRuntimeProfile;
struct FProjectWorldPresentationProfile;
struct FProjectWorldPresentationResources;
struct FProjectWorldRealizationResult;

namespace ProjectWorldDaylightRealization
{
	bool Apply(UWorld& World, const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRuntimeProfile* Runtime, const FProjectWorldPresentationProfile& Presentation,
		const FProjectWorldPresentationResources& Resources, FProjectWorldRealizationResult& Result, FString& Error);
}
