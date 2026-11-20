#include "ProjectLightSettings.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLightSunTest, "Project.Time.Integration.LightSolarDirection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProjectLightSunTest::RunTest(const FString&)
{
	const FVector East(1, 0, 0), North(0, -1, 0), Up(0, 0, 1);
	FVector Sun;
	double Elevation;
	TestTrue(TEXT("Native solar noon"), ProjectLight::SunDirection(FDateTime(2026, 3, 21, 12), 0, 0, East, North, Up, Sun, Elevation));
	TestTrue(TEXT("Sun above equator at noon; native 180 offset removed"), Elevation > 80 && Sun.Z > 0.98);
	TestTrue(TEXT("Native solar midnight"), ProjectLight::SunDirection(FDateTime(2026, 3, 21, 0), 0, 0, East, North, Up, Sun, Elevation));
	TestTrue(TEXT("Sun below horizon at midnight"), Elevation < -80 && Sun.Z < -0.98);
	ProjectLight::SunDirection(FDateTime(2026, 3, 21, 6), 0, 0, East, North, Up, Sun, Elevation);
	TestTrue(TEXT("Morning Sun is east under negative-Y north convention"), Sun.X > 0.98);
	TestFalse(TEXT("Duplicate axes refused"), ProjectLight::SunDirection(FDateTime(2026, 3, 21, 6), 0, 0, East, East, Up, Sun, Elevation));
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectLightSunTest, "Project.Time.Integration.LightSolarDirection", "[Fast][Integration][Light]")
