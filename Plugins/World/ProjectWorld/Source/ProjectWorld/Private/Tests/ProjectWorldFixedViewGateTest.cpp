// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Presentation/ProjectWorldFixedViewGate.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldFixedViewContractTest,
	"Project.World.Presentation.FixedViewContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectWorldFixedViewContractTest::RunTest(const FString& Parameters)
{
	FVector Parsed;
	TestTrue(TEXT("A complete coordinate triple parses."),
		ProjectWorldFixedViewContract::ParseVector(TEXT("-2243,11681,685"), Parsed));
	TestEqual(TEXT("Parsed X is preserved."), Parsed.X, -2243.0);
	TestFalse(TEXT("An incomplete coordinate triple is rejected."),
		ProjectWorldFixedViewContract::ParseVector(TEXT("1,2"), Parsed));

	FProjectWorldFixedViewConfig Config;
	Config.OperationId = TEXT("grandpa_shipping");
	Config.ResultPath = TEXT("C:/evidence/result.json");
	Config.ScreenshotPath = TEXT("C:/evidence/view.png");
	Config.MapPackage = TEXT("/City17/Maps/City17_Persistent_WP");
	Config.SubjectClassPath = TEXT("/Script/ProjectCharacter.DefinitionCharacter");
	Config.PlayerLocation = FVector(-2243.0, 11681.0, 685.0);
	Config.LookAtLocation = FVector(-2469.0, 11788.0, 735.0);
	Config.SubjectLocation = FVector(-2469.0, 11788.0, 685.0);
	FString Error;
	TestTrue(TEXT("The fixed-view package contract accepts the known City17 vantage."),
		ProjectWorldFixedViewContract::ValidateConfig(Config, Error));

	Config.ResultPath = TEXT("relative/result.json");
	TestFalse(TEXT("A relative evidence path is rejected."),
		ProjectWorldFixedViewContract::ValidateConfig(Config, Error));
	return true;
}

#endif
