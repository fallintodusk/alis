#include "ProjectTimeClock.h"
#include "ProjectTimeZone.h"
#include "Misc/AutomationTest.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectTimeClockTest, "Project.Time.Clock.ModesAndContinuity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectTimeClockTest::RunTest(const FString&)
{
	const FDateTime Start(2026, 12, 31, 23, 59, 50);
	FProjectTimeClock Clock;
	FProjectTimeSnapshot Value;
	TestTrue(TEXT("Live source is sampled"), Clock.Read(Start, 10.0, Value));
	TestEqual(TEXT("Live UTC"), Value.Utc, Start);
	TestFalse(TEXT("Direct seek defaults to denied"), Clock.Seek(Start, 10.0));
	Clock.SetControlsAllowed(true);
	TestTrue(TEXT("Admitted seek enters paused Demo"), Clock.Seek(Start, 10.0));
	Clock.Read(Start + FTimespan::FromDays(1), 20.0, Value);
	TestEqual(TEXT("Paused Demo ignores PC clock changes"), Value.Utc, Start);
	TestTrue(TEXT("Demo mode is explicit"), Value.bDemo);
	TestTrue(TEXT("Set accelerated rate"), Clock.SetRate(60.0, Start, 20.0));
	Clock.Read(Start, 21.0, Value);
	TestEqual(TEXT("Monotonic minute crosses year"), Value.Utc, Start + FTimespan::FromMinutes(1));
	TestTrue(TEXT("Re-anchor rate change"), Clock.SetRate(2.0, Start, 21.0));
	Clock.Read(Start, 22.0, Value);
	TestEqual(TEXT("No epoch scaling or time jump"), Value.Utc, Start + FTimespan::FromSeconds(62));
	TestFalse(TEXT("NaN rate refused"), Clock.SetRate(std::numeric_limits<double>::quiet_NaN(), Start, 22.0));
	Clock.SetControlsAllowed(false);
	Clock.Read(Start, 23.0, Value);
	TestFalse(TEXT("Revocation clears Demo"), Value.bDemo);
	TestEqual(TEXT("Revocation returns to fresh Live"), Value.Utc, Start);
	TestFalse(TEXT("Reset cannot grant permission"), Clock.Reset());
	Clock.SetControlsAllowed(true);
	TestTrue(TEXT("Seek near limit"), Clock.Seek(FDateTime::MaxValue(), 0.0));
	TestTrue(TEXT("Start bounded rate"), Clock.SetRate(60.0, Start, 0.0));
	TestFalse(TEXT("Overflow refuses rather than wrapping"), Clock.Read(Start, 1.0, Value));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectTimeZoneTest, "Project.Time.CivilZone.OfflineRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectTimeZoneTest::RunTest(const FString&)
{
	TestTrue(TEXT("Moscow native rules"), ProjectTimeZone::IsValid(TEXT("Europe/Moscow")));
	TestTrue(TEXT("New York native rules"), ProjectTimeZone::IsValid(TEXT("America/New_York")));
	TestFalse(TEXT("Missing ID refuses host fallback"), ProjectTimeZone::IsValid(TEXT("")));
	TestFalse(TEXT("Unknown ID refuses GMT fallback"), ProjectTimeZone::IsValid(TEXT("Unknown/Invalid")));
	TestTrue(TEXT("Moscow UTC+3"), ProjectTimeZone::Format(FDateTime(2026, 1, 15, 12), TEXT("Europe/Moscow")).ToString().Contains(TEXT("15:00:00")));
	TestTrue(TEXT("New York winter UTC-5"), ProjectTimeZone::Format(FDateTime(2026, 1, 15, 12), TEXT("America/New_York")).ToString().Contains(TEXT("07:00:00")));
	TestTrue(TEXT("New York summer UTC-4"), ProjectTimeZone::Format(FDateTime(2026, 7, 15, 12), TEXT("America/New_York")).ToString().Contains(TEXT("08:00:00")));
	TestTrue(TEXT("PC source is fresh UTC"), FMath::Abs((ProjectTimeZone::ReadPcUtc() - FDateTime::UtcNow()).GetTotalSeconds()) < 2.0);
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectTimeClockTest, "Project.Time.Clock.ModesAndContinuity", "[Fast][Unit][Time]")
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectTimeZoneTest, "Project.Time.CivilZone.OfflineRules", "[Fast][Integration][Time]")
