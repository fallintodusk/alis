// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Misc/AutomationTest.h"
#include "Presentation/ProjectWorldPerformanceConsumerRegistration.h"

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

#endif
