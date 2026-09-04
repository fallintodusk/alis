// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Misc/AutomationTest.h"
#include "Presentation/ProjectWorldPerformanceSampleWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldPerformanceSampleWriterTest,
	"ProjectWorld.PlayableTour.PerformanceSamples.ExactProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldPerformanceSampleWriterTest::RunTest(const FString& Parameters)
{
	FProjectWorldPerformanceFrame First;
	First.FrameMilliseconds = 10.0;
	First.GameMilliseconds = 5.0;
	First.RenderMilliseconds = 7.0;
	First.GPUMilliseconds = 8.0;
	FProjectWorldPerformanceFrame Second = First;
	Second.FrameMilliseconds = 20.0;
	const FString Csv = ProjectWorldPerformanceSampleWriter::Serialize({First, Second});
	TArray<FString> Lines;
	Csv.ParseIntoArrayLines(Lines, false);
	TestEqual(TEXT("One header plus every collector frame is serialized."), Lines.Num(), 3);
	TestEqual(TEXT("The projection header is stable."), Lines[0],
		FString(TEXT("FrameTime,GameThreadTime,RenderThreadTime,GPUTime")));
	TestTrue(TEXT("The first collector frame is preserved."), Lines[1].StartsWith(TEXT("10,")));
	TestTrue(TEXT("The second collector frame is preserved."), Lines[2].StartsWith(TEXT("20,")));
	return true;
}

#endif
