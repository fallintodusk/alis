// Copyright ALIS. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Widgets/InventoryPanelGridBuilder.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
// Keep this mirror small and self-contained: the test's job is to lock the
// formula (CellPitch = CellSize + 2 * GridLine, Host = Pitch*N + 2*Outer)
// so any drift in GridBuilder constants is caught here instead of via a
// visual regression on the panel.
constexpr float TestGridSlotLineWidth = 1.f;
constexpr float TestGridHostOuterPadding = 4.f;

FIntPoint ExpectedHostSize(int32 W, int32 H, float CellSize)
{
	const float CellPitch = CellSize + 2.f * TestGridSlotLineWidth;
	const float OuterSlack = 2.f * TestGridHostOuterPadding;
	return FIntPoint(
		FMath::RoundToInt(CellPitch * W + OuterSlack),
		FMath::RoundToInt(CellPitch * H + OuterSlack));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryPanelGridSizing_HostSizeMatchesFormula,
	"ProjectIntegrationTests.UI.Framework.Inventory.GridSizing.HostSizeMatchesFormula",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInventoryPanelGridSizing_HostSizeMatchesFormula::RunTest(const FString& Parameters)
{
	(void)Parameters;

	constexpr float CellSize = 64.f;

	// Representative grid shapes across the panel.
	struct FCase { int32 W; int32 H; const TCHAR* Name; };
	const FCase Cases[] = {
		{2, 2, TEXT("Hand / 2x2")},
		{5, 4, TEXT("Nearby / 5x4")},
		{6, 6, TEXT("Primary / 6x6")},
		{6, 8, TEXT("Backpack / 6x8")},
		{1, 1, TEXT("Degenerate / 1x1")},
	};

	for (const FCase& C : Cases)
	{
		const FIntPoint Actual = FInventoryPanelGridBuilder::ComputeGridHostPixelSize(C.W, C.H, CellSize);
		const FIntPoint Expected = ExpectedHostSize(C.W, C.H, CellSize);
		TestEqual(FString::Printf(TEXT("%s width"), C.Name), Actual.X, Expected.X);
		TestEqual(FString::Printf(TEXT("%s height"), C.Name), Actual.Y, Expected.Y);
	}

	// Hand grid sanity: formula must reproduce the historical 140x140 hand
	// SizeBox that was hand-tuned in JSON before the change. If this fails,
	// either the formula drifted or hand size constants were changed.
	const FIntPoint Hand = FInventoryPanelGridBuilder::ComputeGridHostPixelSize(2, 2, CellSize);
	TestEqual(TEXT("Hand 2x2@64 must match historical 140x140"), Hand, FIntPoint(140, 140));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryPanelGridSizing_FrameOverheadMatchesFontMath,
	"ProjectIntegrationTests.UI.Framework.Inventory.GridSizing.FrameOverheadMatchesFontMath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInventoryPanelGridSizing_FrameOverheadMatchesFontMath::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// Font content area in UpdateGridVisuals is `CellSize - GetCellFrameOverhead()`.
	// Historical code used `CellSize - 10.f`; the named overhead must equal 10
	// at default constants (1 * 2 + 4 * 2).
	const float Overhead = FInventoryPanelGridBuilder::GetCellFrameOverhead();
	TestEqual(TEXT("Frame overhead preserves historical -10 magic"), Overhead, 10.f);

	// Scales symmetrically with CellSize: content area should always be positive
	// for any cell size > overhead.
	const float Content = 64.f - Overhead;
	TestTrue(TEXT("Cell content area positive at default cell size"), Content > 0.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryPanelGridSizing_HostSizeClampsNonPositive,
	"ProjectIntegrationTests.UI.Framework.Inventory.GridSizing.HostSizeClampsNonPositive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInventoryPanelGridSizing_HostSizeClampsNonPositive::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// Degenerate input should not produce negative extents.
	const FIntPoint ZeroZero = FInventoryPanelGridBuilder::ComputeGridHostPixelSize(0, 0, 64.f);
	TestEqual(TEXT("Zero-grid width equals outer slack only"), ZeroZero.X, 8);
	TestEqual(TEXT("Zero-grid height equals outer slack only"), ZeroZero.Y, 8);

	const FIntPoint Negative = FInventoryPanelGridBuilder::ComputeGridHostPixelSize(-5, -3, 64.f);
	TestEqual(TEXT("Negative W clamps to zero grid"), Negative.X, 8);
	TestEqual(TEXT("Negative H clamps to zero grid"), Negative.Y, 8);

	return true;
}

// Automation tag registration (UE 5.7 static-init pattern via
// REGISTER_SIMPLE_AUTOMATION_TEST_TAGS in Core/Misc/AutomationTest.h).
// Reference: docs/agents/canonical.md "Tag taxonomy" + "Single-token CLI
// filter"; dispatch via `iterate.ps1 -Mode Gate -Tags "Fast"` (BasicString
// substring, single token).
// Taxonomy: Speed=Fast, Kind=Unit (pure math, no world/widgets),
// Area=Inventory.
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FInventoryPanelGridSizing_HostSizeMatchesFormula,
	"ProjectIntegrationTests.UI.Framework.Inventory.GridSizing.HostSizeMatchesFormula",
	"[Fast][Unit][Inventory]")
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FInventoryPanelGridSizing_FrameOverheadMatchesFontMath,
	"ProjectIntegrationTests.UI.Framework.Inventory.GridSizing.FrameOverheadMatchesFontMath",
	"[Fast][Unit][Inventory]")
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FInventoryPanelGridSizing_HostSizeClampsNonPositive,
	"ProjectIntegrationTests.UI.Framework.Inventory.GridSizing.HostSizeClampsNonPositive",
	"[Fast][Unit][Inventory]")

#endif // WITH_DEV_AUTOMATION_TESTS
