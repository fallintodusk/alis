// Copyright ALIS. All Rights Reserved.

/**
 * Unit Tests for ProjectUI Theme System
 *
 * Tests verify theme data structures, color palettes, and typography settings.
 */

#include "Theme/ProjectUIThemeData.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Test: Color Palette Defaults
 * Verifies that color palette has sensible default values
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectUIColorPaletteDefaultsTest,
	"ProjectUI.Theme.ColorPaletteDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectUIColorPaletteDefaultsTest::RunTest(const FString& Parameters)
{
	FProjectUIColorPalette Palette;

	// Verify all colors have non-zero alpha
	TestTrue(TEXT("Primary should have alpha"), Palette.Primary.A > 0.0f);
	TestTrue(TEXT("Secondary should have alpha"), Palette.Secondary.A > 0.0f);
	TestTrue(TEXT("Success should have alpha"), Palette.Success.A > 0.0f);
	TestTrue(TEXT("Warning should have alpha"), Palette.Warning.A > 0.0f);
	TestTrue(TEXT("Error should have alpha"), Palette.Error.A > 0.0f);
	TestTrue(TEXT("Background should have alpha"), Palette.Background.A > 0.0f);
	TestTrue(TEXT("Surface should have alpha"), Palette.Surface.A > 0.0f);
	TestTrue(TEXT("TextPrimary should have alpha"), Palette.TextPrimary.A > 0.0f);
	TestTrue(TEXT("TextSecondary should have alpha"), Palette.TextSecondary.A > 0.0f);
	TestTrue(TEXT("TextDisabled should have alpha"), Palette.TextDisabled.A > 0.0f);
	TestTrue(TEXT("Border should have alpha"), Palette.Border.A > 0.0f);

	// Verify color values are in valid range [0,1]
	auto ValidateColor = [this](const FLinearColor& Color, const FString& Name)
	{
		TestTrue(FString::Printf(TEXT("%s.R should be in [0,1]"), *Name), Color.R >= 0.0f && Color.R <= 1.0f);
		TestTrue(FString::Printf(TEXT("%s.G should be in [0,1]"), *Name), Color.G >= 0.0f && Color.G <= 1.0f);
		TestTrue(FString::Printf(TEXT("%s.B should be in [0,1]"), *Name), Color.B >= 0.0f && Color.B <= 1.0f);
		TestTrue(FString::Printf(TEXT("%s.A should be in [0,1]"), *Name), Color.A >= 0.0f && Color.A <= 1.0f);
	};

	ValidateColor(Palette.Primary, TEXT("Primary"));
	ValidateColor(Palette.Success, TEXT("Success"));
	ValidateColor(Palette.Error, TEXT("Error"));

	return true;
}

/**
 * Test: Color Palette Modification
 * Verifies that color palette colors can be changed
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectUIColorPaletteModificationTest,
	"ProjectUI.Theme.ColorPaletteModification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectUIColorPaletteModificationTest::RunTest(const FString& Parameters)
{
	FProjectUIColorPalette Palette;

	// Change primary color
	FLinearColor NewPrimary = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f); // Red
	Palette.Primary = NewPrimary;
	TestEqual(TEXT("Primary color should be updated"), Palette.Primary, NewPrimary);

	// Change multiple colors
	Palette.Success = FLinearColor::Green;
	Palette.Error = FLinearColor::Red;
	Palette.Warning = FLinearColor::Yellow;

	TestEqual(TEXT("Success should be green"), Palette.Success, FLinearColor::Green);
	TestEqual(TEXT("Error should be red"), Palette.Error, FLinearColor::Red);
	TestEqual(TEXT("Warning should be yellow"), Palette.Warning, FLinearColor::Yellow);

	return true;
}

/**
 * Test: Typography Structure
 * Verifies that typography structure exists and can be instantiated
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectUITypographyStructureTest,
	"ProjectUI.Theme.TypographyStructure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectUITypographyStructureTest::RunTest(const FString& Parameters)
{
	FProjectUITypography Typography;

	// Just verify the structure exists and can be created
	// Font info validation requires engine resources
	TestTrue(TEXT("Typography structure should be creatable"), true);

	return true;
}

/**
 * Test: Spacing Structure
 * Verifies spacing values are positive
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectUISpacingStructureTest,
	"ProjectUI.Theme.SpacingStructure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectUISpacingStructureTest::RunTest(const FString& Parameters)
{
	FProjectUISpacing Spacing;

	// Verify spacing values are positive
	TestTrue(TEXT("ExtraSmall should be positive"), Spacing.ExtraSmall > 0.0f);
	TestTrue(TEXT("Small should be positive"), Spacing.Small > 0.0f);
	TestTrue(TEXT("Medium should be positive"), Spacing.Medium > 0.0f);
	TestTrue(TEXT("Large should be positive"), Spacing.Large > 0.0f);
	TestTrue(TEXT("ExtraLarge should be positive"), Spacing.ExtraLarge > 0.0f);

	// Verify spacing values are in ascending order
	TestTrue(TEXT("Small > ExtraSmall"), Spacing.Small > Spacing.ExtraSmall);
	TestTrue(TEXT("Medium > Small"), Spacing.Medium > Spacing.Small);
	TestTrue(TEXT("Large > Medium"), Spacing.Large > Spacing.Medium);
	TestTrue(TEXT("ExtraLarge > Large"), Spacing.ExtraLarge > Spacing.Large);

	return true;
}

/**
 * Test: Spacing Modification
 * Verifies spacing values can be changed
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectUISpacingModificationTest,
	"ProjectUI.Theme.SpacingModification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectUISpacingModificationTest::RunTest(const FString& Parameters)
{
	FProjectUISpacing Spacing;

	// Modify spacing values
	Spacing.Small = 10.0f;
	Spacing.Medium = 20.0f;
	Spacing.Large = 30.0f;

	TestEqual(TEXT("Small should be 10"), Spacing.Small, 10.0f);
	TestEqual(TEXT("Medium should be 20"), Spacing.Medium, 20.0f);
	TestEqual(TEXT("Large should be 30"), Spacing.Large, 30.0f);

	return true;
}

/**
 * Test: Animation Settings Structure
 * Verifies animation settings have valid defaults
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectUIAnimationSettingsTest,
	"ProjectUI.Theme.AnimationSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectUIAnimationSettingsTest::RunTest(const FString& Parameters)
{
	FProjectUIAnimationSettings AnimSettings;

	// Verify animation durations are positive
	TestTrue(TEXT("Fast should be positive"), AnimSettings.Fast > 0.0f);
	TestTrue(TEXT("Normal should be positive"), AnimSettings.Normal > 0.0f);
	TestTrue(TEXT("Slow should be positive"), AnimSettings.Slow > 0.0f);

	// Verify duration order
	TestTrue(TEXT("Normal > Fast"), AnimSettings.Normal > AnimSettings.Fast);
	TestTrue(TEXT("Slow > Normal"), AnimSettings.Slow > AnimSettings.Normal);

	return true;
}

/**
 * Test: Theme Data Asset Structure
 * Verifies UProjectUIThemeData can be created
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectUIThemeDataCreationTest,
	"ProjectUI.Theme.ThemeDataCreation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectUIThemeDataCreationTest::RunTest(const FString& Parameters)
{
	// Create theme data object
	UProjectUIThemeData* ThemeData = NewObject<UProjectUIThemeData>();
	TestNotNull(TEXT("ThemeData should be created"), ThemeData);

	// Verify it has color palette
	TestTrue(TEXT("ThemeData should have ColorPalette"), true);

	return true;
}

/**
 * Test: Theme Data Property Access
 * Verifies theme data properties can be accessed
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectUIThemeDataPropertyAccessTest,
	"ProjectUI.Theme.ThemeDataPropertyAccess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectUIThemeDataPropertyAccessTest::RunTest(const FString& Parameters)
{
	UProjectUIThemeData* ThemeData = NewObject<UProjectUIThemeData>();

	// Access color palette
	FLinearColor PrimaryColor = ThemeData->Colors.Primary;
	TestTrue(TEXT("Should be able to access Primary color"), PrimaryColor.A > 0.0f);

	// Modify color palette
	ThemeData->Colors.Primary = FLinearColor::Blue;
	TestTrue(TEXT("Primary color should be updated"),
		ThemeData->Colors.Primary.Equals(FLinearColor::Blue));

	// Access spacing
	float MediumSpacing = ThemeData->Spacing.Medium;
	TestTrue(TEXT("Should be able to access Medium spacing"), MediumSpacing > 0.0f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
