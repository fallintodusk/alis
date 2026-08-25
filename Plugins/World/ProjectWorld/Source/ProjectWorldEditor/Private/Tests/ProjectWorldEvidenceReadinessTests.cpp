// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldEvidenceReadiness.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldEvidenceReadinessContractTest,
	"Project.World.Evidence.ReadinessContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectWorldEvidenceReadinessContractTest::RunTest(const FString& Parameters)
{
	FProjectWorldEvidenceReadiness Readiness;
	TestFalse(TEXT("The request frame cannot capture"), Readiness.Advance(10, 0));
	TestFalse(TEXT("A repeated engine frame cannot advance readiness"), Readiness.Advance(10, 0));
	TestFalse(TEXT("A pending compilation resets readiness"), Readiness.Advance(11, 1));
	TestFalse(TEXT("The first settled frame cannot capture"), Readiness.Advance(12, 0));
	TestFalse(TEXT("The second settled frame cannot capture"), Readiness.Advance(13, 0));
	TestTrue(TEXT("The third settled frame can capture"), Readiness.Advance(14, 0));
	return true;
}

#endif
