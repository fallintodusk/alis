// Copyright ALIS. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "ProjectGradientHelpers.h"
#include "Theme/ProjectUIThemeData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectGradientHelpers_SampleGradient_TwoStops, "Project.UI.GradientHelpers.SampleGradient.TwoStops", EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext)
bool FProjectGradientHelpers_SampleGradient_TwoStops::RunTest(const FString& Parameters)
{
	// GIVEN a gradient with two color stops (white to black)
	FProjectUIGradient Gradient;
	Gradient.ColorStops = {
		FLinearColor::White,
		FLinearColor::Black
	};

	// WHEN sampling at start (0.0)
	FLinearColor StartColor = UProjectGradientHelpers::SampleGradient(Gradient, 0.0f);

	// THEN should return white
	TestEqual("Start color should be white", StartColor, FLinearColor::White);

	// WHEN sampling at end (1.0)
	FLinearColor EndColor = UProjectGradientHelpers::SampleGradient(Gradient, 1.0f);

	// THEN should return black
	TestEqual("End color should be black", EndColor, FLinearColor::Black);

	// WHEN sampling at middle (0.5)
	FLinearColor MiddleColor = UProjectGradientHelpers::SampleGradient(Gradient, 0.5f);

	// THEN should return gray (interpolated)
	FLinearColor ExpectedGray(0.5f, 0.5f, 0.5f, 1.0f);
	TestEqual("Middle color should be gray", MiddleColor, ExpectedGray);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectGradientHelpers_SampleGradient_ThreeStops, "Project.UI.GradientHelpers.SampleGradient.ThreeStops", EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext)
bool FProjectGradientHelpers_SampleGradient_ThreeStops::RunTest(const FString& Parameters)
{
	// GIVEN a gradient with three color stops (red, green, blue)
	FProjectUIGradient Gradient;
	Gradient.ColorStops = {
		FLinearColor::Red,
		FLinearColor::Green,
		FLinearColor::Blue
	};

	// WHEN sampling at 0.0
	FLinearColor Color0 = UProjectGradientHelpers::SampleGradient(Gradient, 0.0f);
	TestEqual("Position 0.0 should be red", Color0, FLinearColor::Red);

	// WHEN sampling at 0.5
	FLinearColor Color05 = UProjectGradientHelpers::SampleGradient(Gradient, 0.5f);
	TestEqual("Position 0.5 should be green", Color05, FLinearColor::Green);

	// WHEN sampling at 1.0
	FLinearColor Color1 = UProjectGradientHelpers::SampleGradient(Gradient, 1.0f);
	TestEqual("Position 1.0 should be blue", Color1, FLinearColor::Blue);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectGradientHelpers_SampleGradient_ClampPosition, "Project.UI.GradientHelpers.SampleGradient.ClampPosition", EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext)
bool FProjectGradientHelpers_SampleGradient_ClampPosition::RunTest(const FString& Parameters)
{
	// GIVEN a gradient with two color stops
	FProjectUIGradient Gradient;
	Gradient.ColorStops = {
		FLinearColor::White,
		FLinearColor::Black
	};

	// WHEN sampling beyond start (-0.5)
	FLinearColor ColorNegative = UProjectGradientHelpers::SampleGradient(Gradient, -0.5f);

	// THEN should clamp to start color (white)
	TestEqual("Negative position should clamp to white", ColorNegative, FLinearColor::White);

	// WHEN sampling beyond end (1.5)
	FLinearColor ColorBeyond = UProjectGradientHelpers::SampleGradient(Gradient, 1.5f);

	// THEN should clamp to end color (black)
	TestEqual("Beyond position should clamp to black", ColorBeyond, FLinearColor::Black);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectGradientHelpers_SampleGradient_EmptyStops, "Project.UI.GradientHelpers.SampleGradient.EmptyStops", EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext)
bool FProjectGradientHelpers_SampleGradient_EmptyStops::RunTest(const FString& Parameters)
{
	// GIVEN a gradient with no color stops
	FProjectUIGradient Gradient;
	Gradient.ColorStops.Empty();

	// WHEN sampling
	FLinearColor Color = UProjectGradientHelpers::SampleGradient(Gradient, 0.5f);

	// THEN should return fallback white
	TestEqual("Empty gradient should return white", Color, FLinearColor::White);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectGradientHelpers_SampleGradient_SingleStop, "Project.UI.GradientHelpers.SampleGradient.SingleStop", EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext)
bool FProjectGradientHelpers_SampleGradient_SingleStop::RunTest(const FString& Parameters)
{
	// GIVEN a gradient with one color stop (red)
	FProjectUIGradient Gradient;
	Gradient.ColorStops = { FLinearColor::Red };

	// WHEN sampling at any position
	FLinearColor Color0 = UProjectGradientHelpers::SampleGradient(Gradient, 0.0f);
	FLinearColor Color05 = UProjectGradientHelpers::SampleGradient(Gradient, 0.5f);
	FLinearColor Color1 = UProjectGradientHelpers::SampleGradient(Gradient, 1.0f);

	// THEN all should return the single color
	TestEqual("Single stop at 0.0 should return red", Color0, FLinearColor::Red);
	TestEqual("Single stop at 0.5 should return red", Color05, FLinearColor::Red);
	TestEqual("Single stop at 1.0 should return red", Color1, FLinearColor::Red);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectGradientHelpers_CreateGradientTexture_ValidInput, "Project.UI.GradientHelpers.CreateGradientTexture.ValidInput", EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext)
bool FProjectGradientHelpers_CreateGradientTexture_ValidInput::RunTest(const FString& Parameters)
{
	// GIVEN a valid gradient with two color stops
	FProjectUIGradient Gradient;
	Gradient.ColorStops = {
		FLinearColor::Red,
		FLinearColor::Blue
	};
	Gradient.AngleDegrees = 90.0f;

	// WHEN creating a gradient texture
	UTexture2D* Texture = UProjectGradientHelpers::CreateGradientTexture(Gradient, 64, 64);

	// THEN texture should be created
	TestNotNull("Texture should be created", Texture);

	if (Texture)
	{
		// AND texture should have correct dimensions
		TestEqual("Texture width should be 64", Texture->GetSizeX(), 64);
		TestEqual("Texture height should be 64", Texture->GetSizeY(), 64);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectGradientHelpers_CreateGradientTexture_InvalidInput, "Project.UI.GradientHelpers.CreateGradientTexture.InvalidInput", EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext)
bool FProjectGradientHelpers_CreateGradientTexture_InvalidInput::RunTest(const FString& Parameters)
{
	// GIVEN a gradient with only one color stop
	FProjectUIGradient Gradient;
	Gradient.ColorStops = { FLinearColor::Red };

	// WHEN creating a gradient texture
	UTexture2D* Texture = UProjectGradientHelpers::CreateGradientTexture(Gradient, 64, 64);

	// THEN texture should be null (requires at least 2 stops)
	TestNull("Texture should be null for single stop", Texture);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectUIThemeData_GetGradientPreset_Exists, "Project.UI.ThemeData.GetGradientPreset.Exists", EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext)
bool FProjectUIThemeData_GetGradientPreset_Exists::RunTest(const FString& Parameters)
{
	// GIVEN a theme data with default gradient presets
	UProjectUIThemeData* Theme = NewObject<UProjectUIThemeData>();

	// WHEN getting a preset that exists (Primary)
	FProjectUIGradient Gradient;
	bool bFound = Theme->GetGradientPreset("Primary", Gradient);

	// THEN should find it
	TestTrue("Primary gradient preset should exist", bFound);

	// AND gradient should have color stops
	TestTrue("Primary gradient should have color stops", Gradient.ColorStops.Num() >= 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectUIThemeData_GetGradientPreset_NotExists, "Project.UI.ThemeData.GetGradientPreset.NotExists", EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext)
bool FProjectUIThemeData_GetGradientPreset_NotExists::RunTest(const FString& Parameters)
{
	// GIVEN a theme data
	UProjectUIThemeData* Theme = NewObject<UProjectUIThemeData>();

	// WHEN getting a preset that doesn't exist
	FProjectUIGradient Gradient;
	bool bFound = Theme->GetGradientPreset("NonExistent", Gradient);

	// THEN should not find it
	TestFalse("Non-existent gradient preset should not be found", bFound);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectUIThemeData_DefaultGradientPresets, "Project.UI.ThemeData.DefaultGradientPresets", EAutomationTestFlags::ProductFilter | EAutomationTestFlags::EditorContext)
bool FProjectUIThemeData_DefaultGradientPresets::RunTest(const FString& Parameters)
{
	// GIVEN a newly created theme data
	UProjectUIThemeData* Theme = NewObject<UProjectUIThemeData>();

	// WHEN checking default presets
	// THEN should have Primary, Background, Success, Error presets
	FProjectUIGradient Gradient;

	TestTrue("Should have Primary preset", Theme->GetGradientPreset("Primary", Gradient));
	TestTrue("Should have Background preset", Theme->GetGradientPreset("Background", Gradient));
	TestTrue("Should have Success preset", Theme->GetGradientPreset("Success", Gradient));
	TestTrue("Should have Error preset", Theme->GetGradientPreset("Error", Gradient));

	return true;
}
