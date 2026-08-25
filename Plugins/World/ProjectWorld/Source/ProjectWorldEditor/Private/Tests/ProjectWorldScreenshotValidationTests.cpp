// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Misc/AutomationTest.h"
#include "Presentation/ProjectWorldScreenshotValidation.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldScreenshotValidationTest,
	"Project.World.Presentation.ScreenshotValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectWorldScreenshotValidationTest::RunTest(const FString& Parameters)
{
	constexpr int32 Width = 64;
	constexpr int32 Height = 64;
	FString Error;
	TArray<FColor> White;
	White.Init(FColor::White, Width * Height);
	TestFalse(TEXT("A white frame is rejected."),
		ProjectWorldScreenshotValidation::ValidatePixels(Width, Height, White, Error));

	TArray<FColor> Varied;
	Varied.SetNumUninitialized(Width * Height);
	for (int32 Index = 0; Index < Varied.Num(); ++Index)
	{
		Varied[Index] = Index % 2 == 0 ? FColor(30, 90, 20) : FColor(160, 180, 120);
	}
	Error.Reset();
	TestTrue(TEXT("A varied rendered frame is accepted."),
		ProjectWorldScreenshotValidation::ValidatePixels(Width, Height, Varied, Error));
	return true;
}

#endif
