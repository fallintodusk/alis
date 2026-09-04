// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Presentation/ProjectWorldPerformanceMetrics.h"

namespace ProjectWorldPerformanceSampleWriter
{
	FString Serialize(const TArray<FProjectWorldPerformanceFrame>& Frames);
	bool Write(
		const FString& Path,
		const TArray<FProjectWorldPerformanceFrame>& Frames,
		FString& OutError);
}
