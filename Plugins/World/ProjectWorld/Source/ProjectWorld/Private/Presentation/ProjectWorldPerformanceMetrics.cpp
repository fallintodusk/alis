// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Presentation/ProjectWorldPerformanceMetrics.h"

double ProjectWorldPerformanceMetrics::Percentile(TArray<double> Samples, double Quantile)
{
	if (Samples.IsEmpty())
	{
		return 0.0;
	}
	Samples.Sort();
	const int32 Index = FMath::Clamp(
		FMath::CeilToInt(Quantile * static_cast<double>(Samples.Num())) - 1,
		0,
		Samples.Num() - 1);
	return Samples[Index];
}

FProjectWorldPerformanceStatistics ProjectWorldPerformanceMetrics::Calculate(
	const TArray<FProjectWorldPerformanceFrame>& Frames)
{
	TArray<double> FrameSamples;
	TArray<double> GameSamples;
	TArray<double> RenderSamples;
	TArray<double> GPUSamples;
	FrameSamples.Reserve(Frames.Num());
	GameSamples.Reserve(Frames.Num());
	RenderSamples.Reserve(Frames.Num());
	GPUSamples.Reserve(Frames.Num());
	for (const FProjectWorldPerformanceFrame& Frame : Frames)
	{
		FrameSamples.Add(Frame.FrameMilliseconds);
		GameSamples.Add(Frame.GameMilliseconds);
		RenderSamples.Add(Frame.RenderMilliseconds);
		GPUSamples.Add(Frame.GPUMilliseconds);
	}

	FProjectWorldPerformanceStatistics Result;
	Result.SampleCount = Frames.Num();
	Result.FrameP95Milliseconds = Percentile(FrameSamples, 0.95);
	Result.FrameP99Milliseconds = Percentile(FrameSamples, 0.99);
	Result.FrameMaxMilliseconds = Percentile(FrameSamples, 1.0);
	Result.GameP95Milliseconds = Percentile(GameSamples, 0.95);
	Result.RenderP95Milliseconds = Percentile(RenderSamples, 0.95);
	Result.GPUP95Milliseconds = Percentile(GPUSamples, 0.95);
	return Result;
}

bool ProjectWorldPerformanceMetrics::IsAccepted(
	const FProjectWorldPerformanceStatistics& Statistics,
	int32 StreamingFailures,
	double FrameP95BudgetMilliseconds,
	FString& OutReason)
{
	if (Statistics.SampleCount < 300)
	{
		OutReason = FString::Printf(
			TEXT("Only %d performance frames were captured; at least 300 are required."),
			Statistics.SampleCount);
		return false;
	}
	if (Statistics.GPUP95Milliseconds <= 0.0)
	{
		OutReason = TEXT("Native GPU frame timing was unavailable.");
		return false;
	}
	if (StreamingFailures != 0)
	{
		OutReason = FString::Printf(TEXT("%d streaming readiness failures were observed."), StreamingFailures);
		return false;
	}
	if (Statistics.FrameP95Milliseconds > FrameP95BudgetMilliseconds)
	{
		OutReason = FString::Printf(
			TEXT("Frame p95 %.3f ms exceeded the %.3f ms budget."),
			Statistics.FrameP95Milliseconds,
			FrameP95BudgetMilliseconds);
		return false;
	}
	OutReason.Reset();
	return true;
}
