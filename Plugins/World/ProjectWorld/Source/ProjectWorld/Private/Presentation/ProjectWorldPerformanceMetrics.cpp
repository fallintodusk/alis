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
	TArray<double> RHISamples;
	FrameSamples.Reserve(Frames.Num());
	GameSamples.Reserve(Frames.Num());
	RenderSamples.Reserve(Frames.Num());
	GPUSamples.Reserve(Frames.Num());
	RHISamples.Reserve(Frames.Num());
	for (const FProjectWorldPerformanceFrame& Frame : Frames)
	{
		FrameSamples.Add(Frame.FrameMilliseconds);
		GameSamples.Add(Frame.GameMilliseconds);
		RenderSamples.Add(Frame.RenderMilliseconds);
		GPUSamples.Add(Frame.GPUMilliseconds);
		RHISamples.Add(Frame.RHIMilliseconds);
	}

	FProjectWorldPerformanceStatistics Result;
	Result.SampleCount = Frames.Num();
	Result.FrameP50Milliseconds = Percentile(FrameSamples, 0.50);
	Result.FrameP95Milliseconds = Percentile(FrameSamples, 0.95);
	Result.FrameP99Milliseconds = Percentile(FrameSamples, 0.99);
	Result.FrameMaxMilliseconds = Percentile(FrameSamples, 1.0);
	Result.GameP50Milliseconds = Percentile(GameSamples, 0.50);
	Result.GameP95Milliseconds = Percentile(GameSamples, 0.95);
	Result.GameP99Milliseconds = Percentile(GameSamples, 0.99);
	Result.RenderP50Milliseconds = Percentile(RenderSamples, 0.50);
	Result.RenderP95Milliseconds = Percentile(RenderSamples, 0.95);
	Result.RenderP99Milliseconds = Percentile(RenderSamples, 0.99);
	Result.GPUP50Milliseconds = Percentile(GPUSamples, 0.50);
	Result.GPUP95Milliseconds = Percentile(GPUSamples, 0.95);
	Result.GPUP99Milliseconds = Percentile(GPUSamples, 0.99);
	Result.RHIP50Milliseconds = Percentile(RHISamples, 0.50);
	Result.RHIP95Milliseconds = Percentile(RHISamples, 0.95);
	Result.RHIP99Milliseconds = Percentile(RHISamples, 0.99);
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

bool ProjectWorldPerformanceMetrics::HasValidSteadySamples(
	const TArray<FProjectWorldPerformanceFrame>& Frames,
	FString& OutReason)
{
	if (Frames.Num() != 600)
	{
		OutReason = FString::Printf(TEXT("Steady capture requires exactly 600 frames; observed %d."), Frames.Num());
		return false;
	}
	double MinGpu = TNumericLimits<double>::Max();
	double MaxGpu = 0.0;
	for (const FProjectWorldPerformanceFrame& Frame : Frames)
	{
		if (!FMath::IsFinite(Frame.FrameMilliseconds) || Frame.FrameMilliseconds <= 0.0 ||
			!FMath::IsFinite(Frame.GameMilliseconds) || Frame.GameMilliseconds < 0.0 ||
			!FMath::IsFinite(Frame.RenderMilliseconds) || Frame.RenderMilliseconds < 0.0 ||
			!FMath::IsFinite(Frame.GPUMilliseconds) || Frame.GPUMilliseconds <= 0.0)
		{
			OutReason = TEXT("Steady capture contains an absent or invalid native frame timing.");
			return false;
		}
		MinGpu = FMath::Min(MinGpu, Frame.GPUMilliseconds);
		MaxGpu = FMath::Max(MaxGpu, Frame.GPUMilliseconds);
	}
	if (MaxGpu - MinGpu <= 0.001)
	{
		OutReason = TEXT("Native GPU timing did not discriminate across steady frames.");
		return false;
	}
	OutReason.Reset();
	return true;
}
