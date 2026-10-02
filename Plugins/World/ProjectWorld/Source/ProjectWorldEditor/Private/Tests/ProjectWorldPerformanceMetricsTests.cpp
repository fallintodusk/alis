// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Misc/AutomationTest.h"
#include "Presentation/ProjectWorldPerformanceMetrics.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldPerformanceMetricsTest,
	"Project.World.Performance.Metrics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectWorldPerformanceMetricsTest::RunTest(const FString& Parameters)
{
	TArray<FProjectWorldPerformanceFrame> Frames;
	for (int32 Index = 1; Index <= 500; ++Index)
	{
		FProjectWorldPerformanceFrame& Frame = Frames.AddDefaulted_GetRef();
		Frame.FrameMilliseconds = Index / 100.0;
		Frame.GameMilliseconds = Index / 200.0;
		Frame.RenderMilliseconds = Index / 250.0;
		Frame.GPUMilliseconds = Index / 125.0;
	}

	const FProjectWorldPerformanceStatistics Statistics =
		ProjectWorldPerformanceMetrics::Calculate(Frames);
	TestEqual(TEXT("Sample count"), Statistics.SampleCount, 500);
	TestEqual(TEXT("Frame p95 uses nearest rank"), Statistics.FrameP95Milliseconds, 4.75);
	TestEqual(TEXT("Frame p99 uses nearest rank"), Statistics.FrameP99Milliseconds, 4.95);
	TestEqual(TEXT("Frame max"), Statistics.FrameMaxMilliseconds, 5.0);

	FString Reason;
	TestTrue(TEXT("A complete under-budget capture passes"),
		ProjectWorldPerformanceMetrics::IsAccepted(Statistics, 0, 16.67, Reason));
	TestFalse(TEXT("Streaming failure rejects"),
		ProjectWorldPerformanceMetrics::IsAccepted(Statistics, 1, 16.67, Reason));

	FProjectWorldPerformanceStatistics Slow = Statistics;
	Slow.FrameP95Milliseconds = 16.68;
	TestFalse(TEXT("Frame budget rejects"),
		ProjectWorldPerformanceMetrics::IsAccepted(Slow, 0, 16.67, Reason));

	TArray<FProjectWorldPerformanceFrame> SteadyFrames;
	for (int32 Index = 0; Index < 600; ++Index)
	{
		FProjectWorldPerformanceFrame& Frame = SteadyFrames.AddDefaulted_GetRef();
		Frame.FrameMilliseconds = 12.0 + Index / 1000.0;
		Frame.GameMilliseconds = 5.0;
		Frame.RenderMilliseconds = 6.0;
		Frame.GPUMilliseconds = 7.0 + Index / 1000.0;
	}
	TestTrue(TEXT("Advancing steady GPU samples pass"),
		ProjectWorldPerformanceMetrics::HasValidSteadySamples(SteadyFrames, Reason));
	SteadyFrames[300].GPUMilliseconds = 0.0;
	TestFalse(TEXT("A missing steady GPU sample rejects"),
		ProjectWorldPerformanceMetrics::HasValidSteadySamples(SteadyFrames, Reason));
	SteadyFrames[300].GPUMilliseconds = SteadyFrames[0].GPUMilliseconds;
	for (FProjectWorldPerformanceFrame& Frame : SteadyFrames)
	{
		Frame.GPUMilliseconds = 7.0;
	}
	TestFalse(TEXT("Non-discriminating steady GPU samples reject"),
		ProjectWorldPerformanceMetrics::HasValidSteadySamples(SteadyFrames, Reason));
	SteadyFrames.RemoveAt(0);
	TestFalse(TEXT("A short steady window rejects"),
		ProjectWorldPerformanceMetrics::HasValidSteadySamples(SteadyFrames, Reason));
	return true;
}

#endif
