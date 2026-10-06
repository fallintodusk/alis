// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldEvidenceReadiness.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ProjectWorldEvidenceReadinessTests
{
	FProjectWorldEvidenceSubjectReport MakeReport(bool bPresent, int32 Expected, int32 Drawn, int32 Pending)
	{
		FProjectWorldEvidenceSubjectReport Report;
		Report.SubjectName = TEXT("TestSubject");
		Report.bPresent = bPresent;
		Report.ExpectedUnits = Expected;
		Report.DrawnUnits = Drawn;
		Report.PendingBuilds = Pending;
		return Report;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldEvidenceReadinessContractTest,
	"Project.World.Evidence.ReadinessContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectWorldEvidenceReadinessContractTest::RunTest(const FString& Parameters)
{
	// A world without subjects captures exactly as the compile wait alone allows.
	const bool bNoSubjects = ProjectWorldEvidenceReadiness::AreSubjectsDrawn({});
	FProjectWorldEvidenceReadiness Readiness;
	TestFalse(TEXT("The request frame cannot capture"), Readiness.Advance(10, 0, bNoSubjects));
	TestFalse(TEXT("A repeated engine frame cannot advance readiness"), Readiness.Advance(10, 0, bNoSubjects));
	TestFalse(TEXT("A pending compilation resets readiness"), Readiness.Advance(11, 1, bNoSubjects));
	TestFalse(TEXT("The first settled frame cannot capture"), Readiness.Advance(12, 0, bNoSubjects));
	TestFalse(TEXT("The second settled frame cannot capture"), Readiness.Advance(13, 0, bNoSubjects));
	TestTrue(TEXT("The third settled frame can capture"), Readiness.Advance(14, 0, bNoSubjects));

	FProjectWorldEvidenceReadiness SubjectReadiness;
	TestFalse(TEXT("A drawn frame starts the count"), SubjectReadiness.Advance(20, 0, true));
	TestFalse(TEXT("A second drawn frame cannot capture"), SubjectReadiness.Advance(21, 0, true));
	TestFalse(TEXT("An undrawn subject resets readiness"), SubjectReadiness.Advance(22, 0, false));
	TestFalse(TEXT("The first drawn frame after the reset cannot capture"), SubjectReadiness.Advance(23, 0, true));
	TestFalse(TEXT("The second drawn frame after the reset cannot capture"), SubjectReadiness.Advance(24, 0, true));
	TestTrue(TEXT("The third drawn frame after the reset can capture"), SubjectReadiness.Advance(25, 0, true));

	FProjectWorldEvidenceReadiness UndrawnReadiness;
	for (uint64 Frame = 30; Frame < 40; ++Frame)
	{
		TestFalse(TEXT("A subject that is never drawn never captures"), UndrawnReadiness.Advance(Frame, 0, false));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldEvidenceSubjectDecisionTest,
	"Project.World.Evidence.SubjectDecision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectWorldEvidenceSubjectDecisionTest::RunTest(const FString& Parameters)
{
	using ProjectWorldEvidenceReadiness::AreSubjectsDrawn;
	using ProjectWorldEvidenceReadinessTests::MakeReport;

	TestTrue(TEXT("No subject reports count as drawn"), AreSubjectsDrawn({}));
	TestFalse(TEXT("A present subject expecting no units is not drawn"), AreSubjectsDrawn({MakeReport(true, 0, 0, 0)}));
	TestFalse(TEXT("An undrawn expected unit is not drawn"), AreSubjectsDrawn({MakeReport(true, 4, 3, 0)}));
	TestFalse(TEXT("A pending build is not drawn"), AreSubjectsDrawn({MakeReport(true, 4, 4, 1)}));
	TestTrue(TEXT("All expected units drawn with no pending build is drawn"), AreSubjectsDrawn({MakeReport(true, 4, 4, 0)}));
	TestTrue(TEXT("A subject that is not present is ignored"), AreSubjectsDrawn({MakeReport(false, 0, 0, 3)}));
	TestTrue(
		TEXT("A subject that is not present does not hide a drawn one"),
		AreSubjectsDrawn({MakeReport(false, 0, 0, 0), MakeReport(true, 2, 2, 0)}));
	TestFalse(
		TEXT("One undrawn subject among present subjects is not drawn"),
		AreSubjectsDrawn({MakeReport(true, 4, 4, 0), MakeReport(true, 2, 1, 0)}));
	return true;
}

#endif
