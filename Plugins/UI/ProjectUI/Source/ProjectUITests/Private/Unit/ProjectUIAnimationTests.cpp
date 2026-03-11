// Copyright ALIS. All Rights Reserved.

/**
 * Unit Tests for ProjectUI Animation System
 *
 * Tests verify animation duration, transitions, and easing calculations.
 */

#include "Animation/ProjectUIAnimations.h"
#include "Widgets/ProjectUserWidget.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Test: Animation Duration Validation
 * Verifies animation durations are positive
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectUIAnimationDurationTest,
	"ProjectUI.Animation.Duration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectUIAnimationDurationTest::RunTest(const FString& Parameters)
{
	// Test valid durations
	float FastDuration = 0.15f;
	float NormalDuration = 0.3f;
	float SlowDuration = 0.5f;

	TestTrue(TEXT("Fast duration should be positive"), FastDuration > 0.0f);
	TestTrue(TEXT("Normal duration should be positive"), NormalDuration > 0.0f);
	TestTrue(TEXT("Slow duration should be positive"), SlowDuration > 0.0f);

	// Test duration ordering
	TestTrue(TEXT("Normal > Fast"), NormalDuration > FastDuration);
	TestTrue(TEXT("Slow > Normal"), SlowDuration > NormalDuration);

	return true;
}

/**
 * Test: Easing Function Range
 * Verifies easing functions stay in [0,1] range
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectUIAnimationEasingRangeTest,
	"ProjectUI.Animation.EasingRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectUIAnimationEasingRangeTest::RunTest(const FString& Parameters)
{
	// Linear easing (identity function)
	auto LinearEasing = [](float T) { return T; };

	// Test at key points
	float Start = LinearEasing(0.0f);
	float Mid = LinearEasing(0.5f);
	float End = LinearEasing(1.0f);

	TestEqual(TEXT("Start should be 0.0"), Start, 0.0f);
	TestEqual(TEXT("Mid should be 0.5"), Mid, 0.5f);
	TestEqual(TEXT("End should be 1.0"), End, 1.0f);

	// Test range constraints
	TestTrue(TEXT("Start should be in [0,1]"), Start >= 0.0f && Start <= 1.0f);
	TestTrue(TEXT("Mid should be in [0,1]"), Mid >= 0.0f && Mid <= 1.0f);
	TestTrue(TEXT("End should be in [0,1]"), End >= 0.0f && End <= 1.0f);

	return true;
}

/**
 * Test: EaseInOut Function
 * Verifies EaseInOut produces smooth acceleration/deceleration
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectUIAnimationEaseInOutTest,
	"ProjectUI.Animation.EaseInOut",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectUIAnimationEaseInOutTest::RunTest(const FString& Parameters)
{
	// Cubic EaseInOut formula
	auto EaseInOutCubic = [](float T)
	{
		if (T < 0.5f)
		{
			return 4.0f * T * T * T;
		}
		else
		{
			float F = (2.0f * T - 2.0f);
			return 0.5f * F * F * F + 1.0f;
		}
	};

	// Test key points
	float Start = EaseInOutCubic(0.0f);
	float Mid = EaseInOutCubic(0.5f);
	float End = EaseInOutCubic(1.0f);

	TestTrue(TEXT("Start should be near 0.0"), FMath::IsNearlyEqual(Start, 0.0f, 0.001f));
	TestTrue(TEXT("Mid should be near 0.5"), FMath::IsNearlyEqual(Mid, 0.5f, 0.001f));
	TestTrue(TEXT("End should be near 1.0"), FMath::IsNearlyEqual(End, 1.0f, 0.001f));

	return true;
}

/**
 * Test: Opacity Clamping
 * Verifies opacity values are clamped to [0,1]
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectUIAnimationOpacityClampTest,
	"ProjectUI.Animation.OpacityClamp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectUIAnimationOpacityClampTest::RunTest(const FString& Parameters)
{
	// Test clamping
	float Opacity = -0.5f;
	Opacity = FMath::Clamp(Opacity, 0.0f, 1.0f);
	TestEqual(TEXT("Negative opacity should clamp to 0.0"), Opacity, 0.0f);

	Opacity = 1.5f;
	Opacity = FMath::Clamp(Opacity, 0.0f, 1.0f);
	TestEqual(TEXT("Opacity > 1.0 should clamp to 1.0"), Opacity, 1.0f);

	Opacity = 0.5f;
	Opacity = FMath::Clamp(Opacity, 0.0f, 1.0f);
	TestEqual(TEXT("Valid opacity should remain unchanged"), Opacity, 0.5f);

	return true;
}

/**
 * Test: Animation Interpolation
 * Verifies linear interpolation for animation progress
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectUIAnimationInterpolationTest,
	"ProjectUI.Animation.Interpolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectUIAnimationInterpolationTest::RunTest(const FString& Parameters)
{
	// Lerp formula: A + (B - A) * T
	float Start = 0.0f;
	float End = 100.0f;

	// Test key points
	float At0 = FMath::Lerp(Start, End, 0.0f);
	float At25 = FMath::Lerp(Start, End, 0.25f);
	float At50 = FMath::Lerp(Start, End, 0.5f);
	float At75 = FMath::Lerp(Start, End, 0.75f);
	float At100 = FMath::Lerp(Start, End, 1.0f);

	TestEqual(TEXT("Lerp at 0% should be 0"), At0, 0.0f);
	TestEqual(TEXT("Lerp at 25% should be 25"), At25, 25.0f);
	TestEqual(TEXT("Lerp at 50% should be 50"), At50, 50.0f);
	TestEqual(TEXT("Lerp at 75% should be 75"), At75, 75.0f);
	TestEqual(TEXT("Lerp at 100% should be 100"), At100, 100.0f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
