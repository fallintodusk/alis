// Copyright ALIS. All Rights Reserved.

/**
 * Unit Tests for UProjectEasingFunctions
 *
 * Verifies mathematical correctness of easing functions after refactoring
 * to use UE's built-in constants.
 */

#include "Animation/ProjectEasingFunctions.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// ============================================================================
// Boundary Conditions - All easing functions must satisfy these
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectUI_Easing_LinearBoundaries,
	"ProjectUI.Easing.LinearBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectUI_Easing_LinearBoundaries::RunTest(const FString& Parameters)
{
	// Linear: f(0) = 0, f(1) = 1, f(0.5) = 0.5
	TestEqual(TEXT("Linear(0) = 0"), UProjectEasingFunctions::Ease(0.0f, EProjectEasingType::Linear), 0.0f);
	TestEqual(TEXT("Linear(1) = 1"), UProjectEasingFunctions::Ease(1.0f, EProjectEasingType::Linear), 1.0f);
	TestEqual(TEXT("Linear(0.5) = 0.5"), UProjectEasingFunctions::Ease(0.5f, EProjectEasingType::Linear), 0.5f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectUI_Easing_SineBoundaries,
	"ProjectUI.Easing.SineBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectUI_Easing_SineBoundaries::RunTest(const FString& Parameters)
{
	const float Tolerance = 0.001f;

	// SineIn: f(0) = 0, f(1) = 1
	TestEqual(TEXT("SineIn(0) = 0"), UProjectEasingFunctions::Ease(0.0f, EProjectEasingType::SineIn), 0.0f, Tolerance);
	TestEqual(TEXT("SineIn(1) = 1"), UProjectEasingFunctions::Ease(1.0f, EProjectEasingType::SineIn), 1.0f, Tolerance);

	// SineOut: f(0) = 0, f(1) = 1
	TestEqual(TEXT("SineOut(0) = 0"), UProjectEasingFunctions::Ease(0.0f, EProjectEasingType::SineOut), 0.0f, Tolerance);
	TestEqual(TEXT("SineOut(1) = 1"), UProjectEasingFunctions::Ease(1.0f, EProjectEasingType::SineOut), 1.0f, Tolerance);

	// SineInOut: f(0) = 0, f(1) = 1
	TestEqual(TEXT("SineInOut(0) = 0"), UProjectEasingFunctions::Ease(0.0f, EProjectEasingType::SineInOut), 0.0f, Tolerance);
	TestEqual(TEXT("SineInOut(1) = 1"), UProjectEasingFunctions::Ease(1.0f, EProjectEasingType::SineInOut), 1.0f, Tolerance);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectUI_Easing_ExponentialBoundaries,
	"ProjectUI.Easing.ExponentialBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectUI_Easing_ExponentialBoundaries::RunTest(const FString& Parameters)
{
	const float Tolerance = 0.001f;

	// ExpoIn: f(0) = 0, f(1) = 1
	TestEqual(TEXT("ExpoIn(0) = 0"), UProjectEasingFunctions::Ease(0.0f, EProjectEasingType::ExpoIn), 0.0f, Tolerance);
	TestEqual(TEXT("ExpoIn(1) = 1"), UProjectEasingFunctions::Ease(1.0f, EProjectEasingType::ExpoIn), 1.0f, Tolerance);

	// ExpoOut: f(0) = 0, f(1) = 1
	TestEqual(TEXT("ExpoOut(0) = 0"), UProjectEasingFunctions::Ease(0.0f, EProjectEasingType::ExpoOut), 0.0f, Tolerance);
	TestEqual(TEXT("ExpoOut(1) = 1"), UProjectEasingFunctions::Ease(1.0f, EProjectEasingType::ExpoOut), 1.0f, Tolerance);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectUI_Easing_ElasticBoundaries,
	"ProjectUI.Easing.ElasticBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectUI_Easing_ElasticBoundaries::RunTest(const FString& Parameters)
{
	const float Tolerance = 0.001f;

	// ElasticIn: f(0) = 0, f(1) = 1
	TestEqual(TEXT("ElasticIn(0) = 0"), UProjectEasingFunctions::Ease(0.0f, EProjectEasingType::ElasticIn), 0.0f, Tolerance);
	TestEqual(TEXT("ElasticIn(1) = 1"), UProjectEasingFunctions::Ease(1.0f, EProjectEasingType::ElasticIn), 1.0f, Tolerance);

	// ElasticOut: f(0) = 0, f(1) = 1
	TestEqual(TEXT("ElasticOut(0) = 0"), UProjectEasingFunctions::Ease(0.0f, EProjectEasingType::ElasticOut), 0.0f, Tolerance);
	TestEqual(TEXT("ElasticOut(1) = 1"), UProjectEasingFunctions::Ease(1.0f, EProjectEasingType::ElasticOut), 1.0f, Tolerance);

	return true;
}

// ============================================================================
// Range Validation - Output must be in valid range
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectUI_Easing_LinearRange,
	"ProjectUI.Easing.LinearRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectUI_Easing_LinearRange::RunTest(const FString& Parameters)
{
	// Test several points to ensure output stays in [0, 1]
	for (float t = 0.0f; t <= 1.0f; t += 0.1f)
	{
		float Result = UProjectEasingFunctions::Ease(t, EProjectEasingType::Linear);
		TestTrue(FString::Printf(TEXT("Linear(%f) >= 0"), t), Result >= 0.0f);
		TestTrue(FString::Printf(TEXT("Linear(%f) <= 1"), t), Result <= 1.0f);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectUI_Easing_CubicRange,
	"ProjectUI.Easing.CubicRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectUI_Easing_CubicRange::RunTest(const FString& Parameters)
{
	// CubicIn/Out should stay in [0, 1] range
	for (float t = 0.0f; t <= 1.0f; t += 0.1f)
	{
		float CubicIn = UProjectEasingFunctions::Ease(t, EProjectEasingType::CubicIn);
		float CubicOut = UProjectEasingFunctions::Ease(t, EProjectEasingType::CubicOut);

		TestTrue(FString::Printf(TEXT("CubicIn(%f) in range"), t), CubicIn >= 0.0f && CubicIn <= 1.0f);
		TestTrue(FString::Printf(TEXT("CubicOut(%f) in range"), t), CubicOut >= 0.0f && CubicOut <= 1.0f);
	}

	return true;
}

// ============================================================================
// Symmetry Tests - InOut functions should be symmetric
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectUI_Easing_SineInOutSymmetry,
	"ProjectUI.Easing.SineInOutSymmetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectUI_Easing_SineInOutSymmetry::RunTest(const FString& Parameters)
{
	const float Tolerance = 0.001f;

	// SineInOut should be symmetric around t=0.5
	float At25 = UProjectEasingFunctions::Ease(0.25f, EProjectEasingType::SineInOut);
	float At75 = UProjectEasingFunctions::Ease(0.75f, EProjectEasingType::SineInOut);

	// Due to symmetry: f(0.25) + f(0.75) should equal 1.0
	TestEqual(TEXT("SineInOut symmetry"), At25 + At75, 1.0f, Tolerance);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectUI_Easing_CubicInOutSymmetry,
	"ProjectUI.Easing.CubicInOutSymmetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectUI_Easing_CubicInOutSymmetry::RunTest(const FString& Parameters)
{
	const float Tolerance = 0.001f;

	// CubicInOut should be symmetric around t=0.5
	float At25 = UProjectEasingFunctions::Ease(0.25f, EProjectEasingType::CubicInOut);
	float At75 = UProjectEasingFunctions::Ease(0.75f, EProjectEasingType::CubicInOut);

	TestEqual(TEXT("CubicInOut symmetry"), At25 + At75, 1.0f, Tolerance);

	return true;
}

// ============================================================================
// Monotonicity Tests - Most easing functions should be monotonic increasing
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectUI_Easing_LinearMonotonic,
	"ProjectUI.Easing.LinearMonotonic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectUI_Easing_LinearMonotonic::RunTest(const FString& Parameters)
{
	// Linear should be strictly monotonic increasing
	float Prev = 0.0f;
	for (float t = 0.1f; t <= 1.0f; t += 0.1f)
	{
		float Current = UProjectEasingFunctions::Ease(t, EProjectEasingType::Linear);
		TestTrue(FString::Printf(TEXT("Linear(%f) > Linear(%f)"), t, t - 0.1f), Current > Prev);
		Prev = Current;
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectUI_Easing_SineMonotonic,
	"ProjectUI.Easing.SineMonotonic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectUI_Easing_SineMonotonic::RunTest(const FString& Parameters)
{
	// SineIn/Out should be monotonic increasing
	float PrevIn = 0.0f;
	float PrevOut = 0.0f;

	for (float t = 0.1f; t <= 1.0f; t += 0.1f)
	{
		float CurrentIn = UProjectEasingFunctions::Ease(t, EProjectEasingType::SineIn);
		float CurrentOut = UProjectEasingFunctions::Ease(t, EProjectEasingType::SineOut);

		TestTrue(FString::Printf(TEXT("SineIn(%f) monotonic"), t), CurrentIn > PrevIn);
		TestTrue(FString::Printf(TEXT("SineOut(%f) monotonic"), t), CurrentOut > PrevOut);

		PrevIn = CurrentIn;
		PrevOut = CurrentOut;
	}

	return true;
}

// ============================================================================
// Input Clamping Tests - Inputs outside [0, 1] should be clamped
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectUI_Easing_InputClamping,
	"ProjectUI.Easing.InputClamping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectUI_Easing_InputClamping::RunTest(const FString& Parameters)
{
	const float Tolerance = 0.001f;

	// Inputs below 0 should clamp to 0
	TestEqual(TEXT("Clamp negative input"),
		UProjectEasingFunctions::Ease(-0.5f, EProjectEasingType::Linear), 0.0f, Tolerance);

	// Inputs above 1 should clamp to 1
	TestEqual(TEXT("Clamp large input"),
		UProjectEasingFunctions::Ease(1.5f, EProjectEasingType::Linear), 1.0f, Tolerance);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
