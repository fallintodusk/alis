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

	TArray<FProjectWorldFixedViewScalarOverride> ScalarOverrides;
	FString Error;
	TestTrue(TEXT("Multiple finite scalar overrides parse."),
		ProjectWorldFixedViewContract::ParseScalarOverrides(
			TEXT("PatternScaleMeters:0.5;MacroStrength:0.8"), ScalarOverrides, Error));
	TestEqual(TEXT("Both scalar overrides are retained."), ScalarOverrides.Num(), 2);
	if (ScalarOverrides.Num() == 2)
	{
		TestEqual(TEXT("The first scalar name is retained."), ScalarOverrides[0].ParameterName, FString(TEXT("PatternScaleMeters")));
		TestEqual(TEXT("The second scalar value is retained."), ScalarOverrides[1].Value, 0.8f);
	}
	TestFalse(TEXT("Duplicate scalar parameter names are rejected."),
		ProjectWorldFixedViewContract::ParseScalarOverrides(
			TEXT("PatternScaleMeters:0.5;PatternScaleMeters:1.0"), ScalarOverrides, Error));
	TestFalse(TEXT("A malformed scalar value is rejected."),
		ProjectWorldFixedViewContract::ParseScalarOverrides(
			TEXT("MacroStrength:not-a-number"), ScalarOverrides, Error));

	FProjectWorldFixedViewConfig Config;
	Config.OperationId = TEXT("grandpa_shipping");
	Config.ResultPath = TEXT("C:/evidence/result.json");
	Config.ScreenshotPath = TEXT("C:/evidence/view.png");
	Config.MapPackage = TEXT("/City17/Maps/City17_Persistent_WP");
	Config.SubjectClassPath = TEXT("/Script/ProjectCharacter.DefinitionCharacter");
	Config.PlayerLocation = FVector(-2243.0, 11681.0, 685.0);
	Config.LookAtLocation = FVector(-2469.0, 11788.0, 735.0);
	Config.SubjectLocation = FVector(-2469.0, 11788.0, 685.0);
	TestTrue(TEXT("The fixed-view package contract accepts the known City17 vantage."),
		ProjectWorldFixedViewContract::ValidateConfig(Config, Error));

	Config.SubjectClassPath = TEXT("/Script/Engine.StaticMeshActor");
	Config.FixtureMeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");
	TestFalse(TEXT("A partial fixture contract is rejected."),
		ProjectWorldFixedViewContract::ValidateConfig(Config, Error));
	Config.FixtureMaterialPath = TEXT("/ProjectMaterial/Surfaces/Object/MI_ProjectMetricCube.MI_ProjectMetricCube");
	Config.FixtureStartLocation = FVector(-2243.0, 11681.0, 685.0);
	Config.FixtureStartRotation = FRotator::ZeroRotator;
	Config.FixtureFinalRotation = FRotator(20.0, 35.0, 10.0);
	TestTrue(TEXT("A complete generic movable fixture contract is accepted."),
		ProjectWorldFixedViewContract::ValidateConfig(Config, Error));
	FProjectWorldFixedViewScalarOverride ScaleOverride;
	ScaleOverride.ParameterName = TEXT("PatternScaleMeters");
	ScaleOverride.Value = 0.5f;
	Config.ScalarOverrides.Add(ScaleOverride);
	FProjectWorldFixedViewScalarOverride StrengthOverride;
	StrengthOverride.ParameterName = TEXT("MacroStrength");
	StrengthOverride.Value = 0.8f;
	Config.ScalarOverrides.Add(StrengthOverride);
	TestTrue(TEXT("A finite transient fixture scalar override is accepted."),
		ProjectWorldFixedViewContract::ValidateConfig(Config, Error));
	Config.FixtureScale = FVector(0.0, 1.0, 1.0);
	TestFalse(TEXT("A non-positive fixture scale is rejected."),
		ProjectWorldFixedViewContract::ValidateConfig(Config, Error));
	Config.FixtureScale = FVector::OneVector;
	Config.ScalarTargetMaterialPath = TEXT("/ProjectMaterial/Surfaces/Terrain/MI_ProjectTerrain_Default.MI_ProjectTerrain_Default");
	TestTrue(TEXT("A world material scalar target is accepted."),
		ProjectWorldFixedViewContract::ValidateConfig(Config, Error));
	Config.ScalarTargetRadiusCentimeters = 0.0f;
	TestFalse(TEXT("A world material scalar target without a positive search radius is rejected."),
		ProjectWorldFixedViewContract::ValidateConfig(Config, Error));

	Config.ResultPath = TEXT("relative/result.json");
	TestFalse(TEXT("A relative evidence path is rejected."),
		ProjectWorldFixedViewContract::ValidateConfig(Config, Error));
	return true;
}

#endif
