// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

struct FProjectWorldPerformanceFrame
{
	double FrameMilliseconds = 0.0;
	double GameMilliseconds = 0.0;
	double RenderMilliseconds = 0.0;
	double GPUMilliseconds = 0.0;
	double RHIMilliseconds = 0.0;
};

struct FProjectWorldPerformanceStatistics
{
	int32 SampleCount = 0;
	double FrameP50Milliseconds = 0.0;
	double FrameP95Milliseconds = 0.0;
	double FrameP99Milliseconds = 0.0;
	double FrameMaxMilliseconds = 0.0;
	double GameP50Milliseconds = 0.0;
	double GameP95Milliseconds = 0.0;
	double GameP99Milliseconds = 0.0;
	double RenderP50Milliseconds = 0.0;
	double RenderP95Milliseconds = 0.0;
	double RenderP99Milliseconds = 0.0;
	double GPUP50Milliseconds = 0.0;
	double GPUP95Milliseconds = 0.0;
	double GPUP99Milliseconds = 0.0;
	double RHIP50Milliseconds = 0.0;
	double RHIP95Milliseconds = 0.0;
	double RHIP99Milliseconds = 0.0;
};

namespace ProjectWorldPerformanceMetrics
{
	PROJECTWORLD_API double Percentile(TArray<double> Samples, double Quantile);

	PROJECTWORLD_API FProjectWorldPerformanceStatistics Calculate(
		const TArray<FProjectWorldPerformanceFrame>& Frames);

	PROJECTWORLD_API bool IsAccepted(
		const FProjectWorldPerformanceStatistics& Statistics,
		int32 StreamingFailures,
		double FrameP95BudgetMilliseconds,
		FString& OutReason);

	PROJECTWORLD_API bool HasValidSteadySamples(
		const TArray<FProjectWorldPerformanceFrame>& Frames,
		FString& OutReason);
}
