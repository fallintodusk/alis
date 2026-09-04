// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Misc/AutomationTest.h"
#include "Misc/Parse.h"
#include "Presentation/ProjectWorldPerformanceEnvelope.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldPerformanceEnvelopeTest,
	"ProjectWorld.PlayableTour.PerformanceEnvelope.Uncapped",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldPerformanceEnvelopeTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("VSync=0 is not UE boolean-flag syntax."),
		FParse::Param(TEXT("-VSync=0"), TEXT("vsync")));
	TestTrue(TEXT("novsync is UE boolean-flag syntax."),
		FParse::Param(TEXT("-novsync"), TEXT("novsync")));

	FProjectWorldPerformanceEnvelope Envelope;
	Envelope.VSync = 0;
	Envelope.MaxFps = 0.0f;
	Envelope.bSmoothFrameRate = false;
	Envelope.bUseFixedFrameRate = false;
	Envelope.FixedFrameRate = 30.0f;
	Envelope.bUseFixedTimeStep = false;
	Envelope.FixedDeltaTimeSeconds = 1.0 / 30.0;
	Envelope.bBenchmarking = false;
	Envelope.DynamicResolutionOperationMode = 0;
	Envelope.DynamicResolutionStatus = TEXT("disabled");
	Envelope.bDynamicResolutionEnabled = false;
	FString Error;
	TestTrue(TEXT("The uncapped native-resolution state is accepted."), Envelope.Validate(Error));

	Envelope.VSync = 1;
	TestFalse(TEXT("VSync fails closed."), Envelope.Validate(Error));
	Envelope.VSync = 0;
	Envelope.MaxFps = 60.0f;
	TestFalse(TEXT("A frame cap fails closed."), Envelope.Validate(Error));
	Envelope.MaxFps = 0.0f;
	Envelope.bSmoothFrameRate = true;
	TestFalse(TEXT("Frame smoothing fails closed."), Envelope.Validate(Error));
	Envelope.bSmoothFrameRate = false;
	Envelope.bUseFixedFrameRate = true;
	TestFalse(TEXT("A fixed engine frame rate fails closed."), Envelope.Validate(Error));
	Envelope.bUseFixedFrameRate = false;
	Envelope.bUseFixedTimeStep = true;
	TestFalse(TEXT("A fixed time step fails closed."), Envelope.Validate(Error));
	Envelope.bUseFixedTimeStep = false;
	Envelope.DynamicResolutionOperationMode = 2;
	Envelope.DynamicResolutionStatus = TEXT("enabled");
	Envelope.bDynamicResolutionEnabled = true;
	TestFalse(TEXT("Dynamic resolution fails closed."), Envelope.Validate(Error));
	return true;
}

#endif
