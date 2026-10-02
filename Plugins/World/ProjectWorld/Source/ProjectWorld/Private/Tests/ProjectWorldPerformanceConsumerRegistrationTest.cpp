// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Misc/AutomationTest.h"
#include "Presentation/ProjectWorldPerformanceConsumerRegistration.h"
#include "Presentation/ProjectWorldPerformanceCollector.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldPerformanceConsumerRegistrationTest,
	"ProjectWorld.PlayableTour.Performance.ConsumerRegistration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldPerformanceConsumerRegistrationTest::RunTest(const FString& Parameters)
{
	FProjectWorldPerformanceConsumerRegistration Registration;
	TestFalse(TEXT("A new consumer is not registered."), Registration.IsRegistered());

	Registration.MarkRegistered();
	TestTrue(TEXT("Registration is explicit."), Registration.IsRegistered());
	TestTrue(TEXT("The first teardown owns the registered consumer."), Registration.Consume());
	TestFalse(TEXT("The consumer is no longer registered after teardown."), Registration.IsRegistered());
	TestFalse(TEXT("Shutdown cannot tear down the consumer a second time."), Registration.Consume());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldPerformanceCollectorCapacityTest,
	"Project.World.Performance.CollectorCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectWorldPerformanceCollectorCapacityTest::RunTest(const FString& Parameters)
{
	FProjectWorldPerformanceCollector Collector;
	const FString Route = TEXT("steady_center");
	Collector.ReserveRoute(Route, 600);
	TestEqual(TEXT("Steady sample capacity exists before capture"), Collector.FramesFor(Route).Max(), 600);
	Collector.BeginRoute(Route, 600);
	IPerformanceDataConsumer::FFrameData Frame;
	Frame.TrueDeltaSeconds = 0.01;
	Frame.GPUTimeSeconds = 0.007;
	for (int32 Index = 0; Index < 601; ++Index)
	{
		Collector.ProcessFrame(Frame);
	}
	TestEqual(TEXT("Capture remains capped at 600 frames"), Collector.FramesFor(Route).Num(), 600);
	TestEqual(TEXT("Capture does not grow its sample allocation"), Collector.FramesFor(Route).Max(), 600);
	return true;
}

#endif
