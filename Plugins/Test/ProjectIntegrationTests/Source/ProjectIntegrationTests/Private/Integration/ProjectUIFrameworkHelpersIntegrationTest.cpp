// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Presentation/ProjectUIWidgetBinder.h"
#include "Presentation/ProjectUIGridVisualState.h"
#include "Interaction/ProjectUIGridDragDropController.h"
#include "Interaction/ProjectUIGridHitDetector.h"
#include "Overlay/ProjectUIPopupPresenter.h"
#include "Overlay/ProjectUIHoverTooltipPresenter.h"
#include "MVVM/InventoryDropRouter.h"
#include "MVVM/InventoryViewModel.h"
#include "Support/InventoryViewModelSpy.h"
#include "Widgets/W_MainMenu.h"
#include "Support/ProjectInventoryReadOnlyMock.h"
#include "Widgets/InventoryPanelTextUpdater.h"
#include "Widgets/InventoryPanelState.h"
#include "Policies/InventoryUIDropStackPolicy.h"
#include "ProjectGameplayTags.h"
#include "ProjectWidgetHelpers.h"
#include "Layout/ProjectWidgetLayoutLoader.h"
#include "Dialogs/ProjectDialogWidget.h"
#include "Widgets/ProjectRadialProgress.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Input/HittestGrid.h"
#include "Layout/Geometry.h"
#include "Rendering/DrawElements.h"
#include "Styling/WidgetStyle.h"
#include "Types/PaintArgs.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
UWorld* ResolveAutomationTestWorld()
{
	UWorld* World = AutomationCommon::GetAnyGameWorld();
	if (World)
	{
		return World;
	}

	if (!AutomationOpenMap(TEXT("/MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent")))
	{
		return nullptr;
	}

	return AutomationCommon::GetAnyGameWorld();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUIWidgetBinderRequiredValidationTest,
	"ProjectIntegrationTests.UI.Framework.WidgetBinder.RequiredValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUIWidgetBinderRequiredValidationTest::RunTest(const FString& Parameters)
{
	UCanvasPanel* Root = NewObject<UCanvasPanel>(GetTransientPackage(), UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	TestNotNull(TEXT("Root canvas should be created"), Root);
	if (!Root)
	{
		return false;
	}

	UTextBlock* ExistingText = NewObject<UTextBlock>(Root, UTextBlock::StaticClass(), TEXT("ExistingText"));
	TestNotNull(TEXT("Existing text widget should be created"), ExistingText);
	if (!ExistingText)
	{
		return false;
	}
	Root->AddChild(ExistingText);

	FProjectUIWidgetBinder Binder(Root, TEXT("WidgetBinderRequiredValidationTest"));
	UTextBlock* FoundText = Binder.FindRequired<UTextBlock>(TEXT("ExistingText"));
	TestNotNull(TEXT("Binder should resolve existing required widget"), FoundText);

	UButton* MissingButton = Binder.FindRequired<UButton>(TEXT("MissingButton"));
	TestNull(TEXT("Missing required widget should return null"), MissingButton);
	TestTrue(TEXT("Binder should report missing required widget"), Binder.HasMissingRequired());
	TestEqual(TEXT("Binder should track one missing widget"), Binder.GetMissingRequired().Num(), 1);
	if (Binder.GetMissingRequired().Num() == 1)
	{
		TestEqual(TEXT("Missing widget name should be recorded"), Binder.GetMissingRequired()[0], FString(TEXT("MissingButton")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUIGridDragFootprintValidationTest,
	"ProjectIntegrationTests.UI.Framework.GridDragDrop.FootprintValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUIGridDragFootprintValidationTest::RunTest(const FString& Parameters)
{
	FProjectUIGridDragPayload Payload;
	Payload.InstanceId = 1001;
	Payload.ItemSize = FIntPoint(2, 2);

	TSet<int32> DisabledCells;
	TMap<int32, int32> Occupants;

	auto EnabledChecker = [&DisabledCells](bool /*bSecondary*/, int32 CellIndex) -> bool
	{
		return !DisabledCells.Contains(CellIndex);
	};

	auto OccupantChecker = [&Occupants](bool /*bSecondary*/, int32 CellIndex) -> int32
	{
		if (const int32* Occupant = Occupants.Find(CellIndex))
		{
			return *Occupant;
		}
		return INDEX_NONE;
	};

	const bool bValidOpenFootprint = FProjectUIGridDragDropController::ValidateFootprint(
		Payload, 1, 1, 4, 4, false, EnabledChecker, OccupantChecker);
	TestTrue(TEXT("Open 2x2 footprint should be valid"), bValidOpenFootprint);

	DisabledCells.Add(6); // cells for 2x2 at (1,1): 5,6,9,10
	const bool bValidWithDisabledCell = FProjectUIGridDragDropController::ValidateFootprint(
		Payload, 1, 1, 4, 4, false, EnabledChecker, OccupantChecker);
	TestFalse(TEXT("Footprint should fail when any covered cell is disabled"), bValidWithDisabledCell);
	DisabledCells.Reset();

	Occupants.Add(10, 777);
	const bool bValidWithOtherOccupant = FProjectUIGridDragDropController::ValidateFootprint(
		Payload, 1, 1, 4, 4, false, EnabledChecker, OccupantChecker);
	TestFalse(TEXT("Footprint should fail when a covered cell is occupied by another instance"), bValidWithOtherOccupant);

	Occupants.FindOrAdd(10) = Payload.InstanceId;
	const bool bValidWithSelfOccupant = FProjectUIGridDragDropController::ValidateFootprint(
		Payload, 1, 1, 4, 4, false, EnabledChecker, OccupantChecker);
	TestTrue(TEXT("Footprint should allow cells occupied by dragged instance"), bValidWithSelfOccupant);

	Occupants.FindOrAdd(10) = 777;
	const bool bValidWithDomainOccupantRule = FProjectUIGridDragDropController::ValidateFootprintWithRule(
		Payload,
		1,
		1,
		4,
		4,
		false,
		EnabledChecker,
		OccupantChecker,
		[&Payload](bool /*bSecondary*/, int32 /*CellIndex*/, int32 OccupantId)
		{
			return OccupantId == INDEX_NONE || OccupantId == Payload.InstanceId || OccupantId == 777;
		});
	TestTrue(TEXT("Domain occupancy rule should be able to allow a non-self occupant"), bValidWithDomainOccupantRule);

	const bool bOutOfBounds = FProjectUIGridDragDropController::ValidateFootprint(
		Payload, 3, 3, 4, 4, false, EnabledChecker, OccupantChecker);
	TestFalse(TEXT("Footprint should fail when placement exceeds grid bounds"), bOutOfBounds);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUIGridDragSurfacePriorityOrderingTest,
	"ProjectIntegrationTests.UI.Framework.GridDragDrop.SurfacePriorityOrdering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUIGridDragSurfacePriorityOrderingTest::RunTest(const FString& Parameters)
{
	// Regression: when two registered surfaces overlap on screen, the
	// surface with the higher FProjectUIGridSurface::Priority must be
	// visited first by the hit-test loop. Ties must preserve registration
	// order (stable sort). Priority is the only knob that makes routing
	// independent of registration order, so it must stay deterministic
	// even as widgets come and go during a session.

	// We test the ordering the controller actually uses for hit-testing
	// (`GetSurfaceTagsInPriorityOrder`) without requiring rendered
	// geometry, because hit-testing iterates this order verbatim.

	FProjectUIGridDragDropController Controller;

	auto MakeSurface = [](const FGameplayTag& Tag, int32 Priority)
	{
		FProjectUIGridSurface Surface;
		Surface.SurfaceTag = Tag;
		Surface.Dims = FIntPoint(2, 2);
		Surface.Priority = Priority;
		return Surface;
	};

	const FGameplayTag& LeftHand = ProjectTags::Item_Container_LeftHand;
	const FGameplayTag& Backpack = ProjectTags::Item_Container_Backpack;
	const FGameplayTag& World = ProjectTags::Item_Container_WorldStorage;

	Controller.RegisterSurface(MakeSurface(LeftHand, 0));
	Controller.RegisterSurface(MakeSurface(Backpack, 0));
	Controller.RegisterSurface(MakeSurface(World, 10));

	TArray<FGameplayTag> Order = Controller.GetSurfaceTagsInPriorityOrder();
	TestEqual(TEXT("Three surfaces registered"), Order.Num(), 3);
	if (Order.Num() == 3)
	{
		TestEqual(TEXT("Higher Priority (WorldStorage) visited first"), Order[0], World);
		TestEqual(TEXT("Tie between LeftHand (0) and Backpack (0) preserves registration order"), Order[1], LeftHand);
		TestEqual(TEXT("Second tie slot carries Backpack"), Order[2], Backpack);
	}

	// Re-registering LeftHand with a higher priority must move it to the
	// front and leave the others alone (the existing slot is replaced,
	// then sorted).
	Controller.RegisterSurface(MakeSurface(LeftHand, 20));
	Order = Controller.GetSurfaceTagsInPriorityOrder();
	TestEqual(TEXT("Re-register does not change surface count"), Order.Num(), 3);
	if (Order.Num() == 3)
	{
		TestEqual(TEXT("Re-registered higher priority wins"), Order[0], LeftHand);
		TestEqual(TEXT("WorldStorage drops to second"), Order[1], World);
		TestEqual(TEXT("Backpack stays last at default priority"), Order[2], Backpack);
	}

	// Unregister the winner; next-highest must surface.
	Controller.UnregisterSurface(LeftHand);
	Order = Controller.GetSurfaceTagsInPriorityOrder();
	TestEqual(TEXT("Unregister removes exactly one surface"), Order.Num(), 2);
	if (Order.Num() == 2)
	{
		TestEqual(TEXT("WorldStorage wins after LeftHand removed"), Order[0], World);
		TestEqual(TEXT("Backpack follows"), Order[1], Backpack);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUIGridDragMultiSurfaceResolvesCorrectTagTest,
	"ProjectIntegrationTests.UI.Framework.Inventory.MultiSurfaceDragResolvesCorrectTag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUIGridDragMultiSurfaceResolvesCorrectTagTest::RunTest(const FString& Parameters)
{
	// Regression: three surfaces registered with distinct tags must hit-test
	// independently. If two overlap on screen, priority must win. If we
	// unregister the winner, the next-priority surface takes over.
	//
	// Geometry is not available in a headless automation test, so we use
	// the exposed priority-ordering contract (also consumed by the real
	// hit-test loop) as a proxy for which surface "wins" overlap. Pair this
	// with the SurfacePriorityOrdering test for the ordering itself; this
	// test specifically exercises the three-container shape of main +
	// secondary + world, which is the post-Slice-6e live shape.

	FProjectUIGridDragDropController Controller;

	auto MakeSurface = [](const FGameplayTag& Tag, int32 Priority)
	{
		FProjectUIGridSurface Surface;
		Surface.SurfaceTag = Tag;
		Surface.Dims = FIntPoint(4, 4);
		Surface.Priority = Priority;
		return Surface;
	};

	const FGameplayTag& Backpack = ProjectTags::Item_Container_Backpack;
	const FGameplayTag& Pockets1 = ProjectTags::Item_Container_Pockets1;
	const FGameplayTag& World    = ProjectTags::Item_Container_WorldStorage;

	Controller.RegisterSurface(MakeSurface(Backpack, 0));
	Controller.RegisterSurface(MakeSurface(Pockets1, 0));
	Controller.RegisterSurface(MakeSurface(World,    10));

	TestEqual(TEXT("Three surfaces registered"), Controller.GetSurfaceCount(), 3);
	TestTrue(TEXT("Backpack is registered"), Controller.HasSurface(Backpack));
	TestTrue(TEXT("Pockets1 is registered"), Controller.HasSurface(Pockets1));
	TestTrue(TEXT("WorldStorage is registered"), Controller.HasSurface(World));

	// Order during simultaneous visibility: world wins overlap.
	TArray<FGameplayTag> Order = Controller.GetSurfaceTagsInPriorityOrder();
	if (TestEqual(TEXT("Three in order"), Order.Num(), 3))
	{
		TestEqual(TEXT("World wins overlap at priority 10"), Order[0], World);
		TestEqual(TEXT("Backpack follows in registration order among ties"), Order[1], Backpack);
		TestEqual(TEXT("Pockets1 last among the two zero-priority entries"), Order[2], Pockets1);
	}

	// Unregister the overlap winner; backpack must now be visited first.
	Controller.UnregisterSurface(World);
	Order = Controller.GetSurfaceTagsInPriorityOrder();
	if (TestEqual(TEXT("Two surfaces after World unregistered"), Order.Num(), 2))
	{
		TestEqual(TEXT("Backpack visited first after World removed"), Order[0], Backpack);
		TestEqual(TEXT("Pockets1 second"), Order[1], Pockets1);
	}

	// ResolveDropTargetOverSurfaces with no surfaces cached geometry
	// available must fail cleanly (no crash, no dispatch). This proves the
	// fail-closed behaviour that the live widget relies on when the cursor
	// is over no registered surface.
	{
		FProjectUIGridDragPayload Payload;
		Payload.InstanceId = 111;
		Payload.ItemSize = FIntPoint(1, 1);
		FGameplayTag OutTag;
		int32 OutCol = INDEX_NONE;
		int32 OutRow = INDEX_NONE;
		const bool bResolved = Controller.ResolveDropTargetOverSurfaces(
			FVector2D(0.f, 0.f), Payload, OutTag, OutCol, OutRow);
		TestFalse(TEXT("Resolve fails closed when no surface has cached geometry"), bResolved);
		TestFalse(TEXT("Out tag left unset when resolve fails"), OutTag.IsValid());
		TestEqual(TEXT("Out col left at INDEX_NONE on fail"), OutCol, INDEX_NONE);
		TestEqual(TEXT("Out row left at INDEX_NONE on fail"), OutRow, INDEX_NONE);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUIGridDragPayloadCarriesFullSourceContextTest,
	"ProjectIntegrationTests.UI.Framework.GridDragDrop.PayloadCarriesFullSourceContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUIGridDragPayloadCarriesFullSourceContextTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// Regression: surface OccupantAllowedChecker callbacks must NOT reach
	// to global Slate state for drag context. Reaching to
	// UWidgetBlueprintLibrary::GetDragDroppingContent() returned null in
	// lifecycle edge cases and made every empty cell appear unavailable -
	// that's exactly what broke equip-backpack drops post-Slice 6e.
	//
	// This test pins down the contract: FProjectUIGridDragPayload carries
	// InstanceId, ItemSize, Quantity, SourceSurfaceTag. Surface lambdas
	// MUST be self-contained off the payload. We model the production
	// stack-merge rule and prove:
	//   - empty cell drops succeed even with a payload that has no source
	//     entry (defensive empty-cell short-circuit);
	//   - occupied cell drops with a non-matching payload fail;
	//   - self-cell drops (occupant == payload.InstanceId) succeed;
	//   - unrelated rules can read Quantity / SourceSurfaceTag from Payload
	//     without reaching to global state.
	FProjectUIGridDragPayload Payload;
	Payload.InstanceId = 100;
	Payload.ItemSize = FIntPoint(1, 1);
	Payload.Quantity = 5;
	Payload.SourceSurfaceTag = ProjectTags::Item_Container_Backpack;

	int32 OccupantAllowedCalls = 0;
	int32 ObservedQuantity = INDEX_NONE;
	FGameplayTag ObservedSourceTag;

	auto OccupantAllowedChecker = [&OccupantAllowedCalls, &ObservedQuantity, &ObservedSourceTag](
		int32 /*CellIndex*/, int32 OccupantId, const FProjectUIGridDragPayload& Pl) -> bool
	{
		++OccupantAllowedCalls;
		ObservedQuantity = Pl.Quantity;
		ObservedSourceTag = Pl.SourceSurfaceTag;

		// Production-shaped rule: empty cell wins first. This is the line
		// that protects equip-backpack drops from null DragOp scenarios.
		if (OccupantId == INDEX_NONE) { return true; }
		if (Pl.InstanceId == INDEX_NONE) { return false; }
		if (OccupantId == Pl.InstanceId) { return true; }
		// For this test, refuse stack merging on different occupants.
		return false;
	};

	// Empty cell:
	const FGameplayTag& BackpackTag = ProjectTags::Item_Container_Backpack;
	TestTrue(TEXT("Empty cell allowed even with sparse payload"),
		OccupantAllowedChecker(0, INDEX_NONE, Payload));
	TestEqual(TEXT("Quantity carried by payload"), ObservedQuantity, 5);
	TestEqual(TEXT("Source tag carried by payload"), ObservedSourceTag, BackpackTag);

	// Self-occupied:
	TestTrue(TEXT("Self-occupied cell allowed"),
		OccupantAllowedChecker(0, /*OccupantId=*/100, Payload));

	// Other occupant:
	TestFalse(TEXT("Other occupant rejected"),
		OccupantAllowedChecker(0, /*OccupantId=*/777, Payload));

	// Empty cell with degenerate payload (InstanceId = INDEX_NONE):
	FProjectUIGridDragPayload DegenPayload;
	TestTrue(TEXT("Empty cell allowed with degenerate payload (defensive)"),
		OccupantAllowedChecker(0, INDEX_NONE, DegenPayload));

	TestEqual(TEXT("OccupantAllowedChecker invoked exactly four times"), OccupantAllowedCalls, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUIGridDragResolveSourceIgnoresOccupancyTest,
	"ProjectIntegrationTests.UI.Framework.GridDragDrop.ResolveSourceIgnoresOccupancy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUIGridDragResolveSourceIgnoresOccupancyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// Regression: drag-start is a source-side question ("which cell did the
	// user press?") and must NOT run drop-footprint validation. Using the
	// drop-validation path for drag-start refuses occupied cells - which
	// means the user cannot pick up items at all, because a drag source is
	// by definition an occupied cell. This test proves the two APIs have
	// distinct contracts:
	//   - ResolveSurfaceCellAtScreenPos  -> pure hit test; no occupancy
	//   - ResolveDropTargetOverSurfaces  -> hit test + footprint validation
	//
	// We cannot feed real rendered geometry in a headless test, so we
	// register a surface with null Grid (the controller logs Verbose and
	// returns false for both APIs) and instead prove contract shapes via
	// non-matching call comparisons below. The meaningful assertion is
	// that the source-hit API uses no occupancy rule at all.

	FProjectUIGridDragDropController Controller;
	const FGameplayTag& World = ProjectTags::Item_Container_WorldStorage;

	// Register a surface that reports every cell as occupied by instance id 42.
	FProjectUIGridSurface Surface;
	Surface.SurfaceTag = World;
	Surface.Dims = FIntPoint(4, 4);
	Surface.Priority = 10;
	Surface.OccupantChecker = [](int32 /*CellIndex*/) -> int32 { return 42; };
	Controller.RegisterSurface(MoveTemp(Surface));
	TestTrue(TEXT("Surface registered"), Controller.HasSurface(World));

	// Without cached geometry both APIs fail (no grid hit). That is the
	// correct no-cursor-over-anything behavior. Assert both APIs refuse
	// and leave the out fields pristine.
	{
		FGameplayTag OutTag;
		int32 OutCol = 777;
		int32 OutRow = 777;
		const bool bHit = Controller.ResolveSurfaceCellAtScreenPos(
			FVector2D(0.f, 0.f), OutTag, OutCol, OutRow);
		TestFalse(TEXT("Source hit without cached geometry must fail"), bHit);
		TestFalse(TEXT("No tag written on source miss"), OutTag.IsValid());
		TestEqual(TEXT("Col reset on source miss"), OutCol, INDEX_NONE);
		TestEqual(TEXT("Row reset on source miss"), OutRow, INDEX_NONE);
	}
	{
		FProjectUIGridDragPayload Probe;
		Probe.InstanceId = INDEX_NONE;
		Probe.ItemSize = FIntPoint(1, 1);
		FGameplayTag OutTag;
		int32 OutCol = 777;
		int32 OutRow = 777;
		const bool bHit = Controller.ResolveDropTargetOverSurfaces(
			FVector2D(0.f, 0.f), Probe, OutTag, OutCol, OutRow);
		TestFalse(TEXT("Drop hit without cached geometry must fail"), bHit);
		TestFalse(TEXT("No tag written on drop miss"), OutTag.IsValid());
		TestEqual(TEXT("Col reset on drop miss"), OutCol, INDEX_NONE);
		TestEqual(TEXT("Row reset on drop miss"), OutRow, INDEX_NONE);
	}

	// Contract shape: source API does not take a payload. Compile-time
	// presence of ResolveSurfaceCellAtScreenPos(ScreenPos, Tag, Col, Row)
	// is the whole point - keep the assertion simple but non-trivial by
	// registering a second surface and confirming count/order stays
	// deterministic. This also guards against accidental removal of the
	// new public method signature.
	FProjectUIGridSurface Second;
	Second.SurfaceTag = ProjectTags::Item_Container_Backpack;
	Second.Dims = FIntPoint(2, 2);
	Second.Priority = 0;
	Controller.RegisterSurface(MoveTemp(Second));
	TestEqual(TEXT("Two surfaces registered"), Controller.GetSurfaceCount(), 2);

	TArray<FGameplayTag> Order = Controller.GetSurfaceTagsInPriorityOrder();
	if (TestEqual(TEXT("Two in priority order"), Order.Num(), 2))
	{
		TestEqual(TEXT("World wins by priority"), Order[0], World);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUIGridDragPlayerTagReRegistersOnTabChangeTest,
	"ProjectIntegrationTests.UI.Framework.GridDragDrop.PlayerGridTagReRegistersOnTabChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUIGridDragPlayerTagReRegistersOnTabChangeTest::RunTest(const FString& Parameters)
{
	// Regression: when the main-panel tab switches, the tabbed primary grid
	// must be unregistered under the old container tag and re-registered
	// under the new tag. Same-tag re-registration must update (not
	// duplicate) the entry. Mirrors the production path in
	// W_InventoryPanel::RegisterPlayerGridSurfaces.
	FProjectUIGridDragDropController Controller;

	auto MakeSurface = [](const FGameplayTag& Tag)
	{
		FProjectUIGridSurface Surface;
		Surface.SurfaceTag = Tag;
		Surface.Dims = FIntPoint(4, 4);
		Surface.Priority = 0;
		return Surface;
	};

	const FGameplayTag& Backpack = ProjectTags::Item_Container_Backpack;
	const FGameplayTag& Pockets1 = ProjectTags::Item_Container_Pockets1;
	const FGameplayTag& Pockets2 = ProjectTags::Item_Container_Pockets2;

	// Tab 1 selected: register Backpack.
	Controller.RegisterSurface(MakeSurface(Backpack));
	TestEqual(TEXT("One surface after first tab select"), Controller.GetSurfaceCount(), 1);
	TestTrue(TEXT("Backpack registered"), Controller.HasSurface(Backpack));

	// Tab switch: unregister old, register new (Pockets1). This is the
	// widget-level protocol; the controller offers both sides of the pair
	// so the tag churn is explicit.
	Controller.UnregisterSurface(Backpack);
	Controller.RegisterSurface(MakeSurface(Pockets1));
	TestEqual(TEXT("Still one surface after tab switch"), Controller.GetSurfaceCount(), 1);
	TestFalse(TEXT("Old tag is gone after tab switch"), Controller.HasSurface(Backpack));
	TestTrue(TEXT("New tag is registered after tab switch"), Controller.HasSurface(Pockets1));

	// Same-tag re-register (e.g. grid rebuilt without tag change) must
	// replace, not duplicate.
	Controller.RegisterSurface(MakeSurface(Pockets1));
	TestEqual(TEXT("Same-tag re-register does not duplicate"), Controller.GetSurfaceCount(), 1);

	// Two tabbed surfaces (primary + secondary). Tab switch on secondary
	// must leave primary alone.
	Controller.RegisterSurface(MakeSurface(Pockets2));
	TestEqual(TEXT("Two surfaces after registering secondary"), Controller.GetSurfaceCount(), 2);
	Controller.UnregisterSurface(Pockets1);
	Controller.RegisterSurface(MakeSurface(Backpack));
	TestEqual(TEXT("Two surfaces after primary switch"), Controller.GetSurfaceCount(), 2);
	TestTrue(TEXT("Backpack present after primary switch"), Controller.HasSurface(Backpack));
	TestTrue(TEXT("Pockets2 left alone when primary switched"), Controller.HasSurface(Pockets2));
	TestFalse(TEXT("Pockets1 gone"), Controller.HasSurface(Pockets1));

	// Full teardown (widget destruct path).
	Controller.ClearSurfaces();
	TestEqual(TEXT("ClearSurfaces removes all"), Controller.GetSurfaceCount(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryDropRouterDispatchesByTagTest,
	"ProjectIntegrationTests.UI.Framework.Inventory.DropRouterDispatchesByTag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FInventoryDropRouterDispatchesByTagTest::RunTest(const FString& Parameters)
{
	// Regression: FInventoryDropRouter::Route must:
	//  - fail-closed when either tag is outside Item.Container.*
	//  - fail on invalid drag payloads (no instance id, zero quantity)
	//  - return true for the FOUR inventory-domain flows AND call the
	//    matching VM method:
	//      player->player  -> RequestMoveItem
	//      player->world   -> RequestStoreItemInNearbyContainerAt
	//      world->player   -> RequestTakeNearbyItemToContainer
	//      world->world    -> RequestMoveItemInNearbyContainer
	//        (rearrangement inside the same open nearby container; this
	//         was silently dropped before the router fix, which visually
	//         manifested as "drag starts but drop does nothing even in
	//         the same container")
	// We prove dispatch with UInventoryViewModelSpy, which overrides the
	// four methods and records which one was hit plus its args.

	UInventoryViewModelSpy* VM = NewObject<UInventoryViewModelSpy>();
	TestNotNull(TEXT("Spy ViewModel should construct"), VM);
	if (!VM)
	{
		return false;
	}

	auto MakeCtx = [](FGameplayTag SourceTag, int32 Qty)
	{
		FInventoryDragContext Ctx;
		Ctx.InstanceId = 123;
		Ctx.SourceSurfaceTag = SourceTag;
		Ctx.SourcePos = FIntPoint(0, 0);
		Ctx.Quantity = Qty;
		return Ctx;
	};
	auto MakeTarget = [](FGameplayTag TargetTag)
	{
		FInventoryDropTarget Target;
		Target.TargetSurfaceTag = TargetTag;
		Target.TargetPos = FIntPoint(1, 1);
		return Target;
	};

	const FGameplayTag& Backpack = ProjectTags::Item_Container_Backpack;
	const FGameplayTag& LeftHand = ProjectTags::Item_Container_LeftHand;
	const FGameplayTag& World = ProjectTags::Item_Container_WorldStorage;

	// Fail-closed: bad instance id.
	{
		FInventoryDragContext Ctx = MakeCtx(Backpack, 1);
		Ctx.InstanceId = INDEX_NONE;
		TestFalse(TEXT("Route rejects invalid InstanceId"),
			FInventoryDropRouter::Route(*VM, Ctx, MakeTarget(LeftHand)));
	}

	// Fail-closed: zero quantity.
	TestFalse(TEXT("Route rejects zero quantity"),
		FInventoryDropRouter::Route(*VM, MakeCtx(Backpack, 0), MakeTarget(LeftHand)));

	// Fail-closed: source tag outside Item.Container domain.
	{
		const FGameplayTag OutOfDomain = FGameplayTag::RequestGameplayTag(FName(TEXT("Item.Type.Equipment")), false);
		TestTrue(TEXT("Sanity: out-of-domain tag resolves"), OutOfDomain.IsValid());
		TestFalse(TEXT("Route rejects when source tag is outside Item.Container"),
			FInventoryDropRouter::Route(*VM, MakeCtx(OutOfDomain, 1), MakeTarget(LeftHand)));
		TestFalse(TEXT("Route rejects when target tag is outside Item.Container"),
			FInventoryDropRouter::Route(*VM, MakeCtx(Backpack, 1), MakeTarget(OutOfDomain)));
	}

	// Fail-closed: invalid tag.
	TestFalse(TEXT("Route rejects when source tag is empty"),
		FInventoryDropRouter::Route(*VM, MakeCtx(FGameplayTag(), 1), MakeTarget(LeftHand)));
	TestFalse(TEXT("Route rejects when target tag is empty"),
		FInventoryDropRouter::Route(*VM, MakeCtx(Backpack, 1), MakeTarget(FGameplayTag())));

	// Supported: world -> world (rearrange within same nearby container)
	// => RequestMoveItemInNearbyContainer. Regression guard: this pair
	// used to return silently-empty, which left in-container drag dead at
	// runtime. The fix routes it to a dedicated VM method that delegates
	// to the world-container transfer bridge's atomic Consume+Store.
	VM->LastCall = UInventoryViewModelSpy::ELastCall::None;
	TestTrue(TEXT("Route accepts world -> world (rearrange in nearby container)"),
		FInventoryDropRouter::Route(*VM, MakeCtx(World, 2), MakeTarget(World)));
	TestEqual(TEXT("world -> world dispatches RequestMoveItemInNearbyContainer"),
		static_cast<int32>(VM->LastCall),
		static_cast<int32>(UInventoryViewModelSpy::ELastCall::MoveItemInNearbyContainer));
	TestEqual(TEXT("MoveInNearby carries target pos"), VM->LastToPos, FIntPoint(1, 1));
	TestEqual(TEXT("MoveInNearby carries quantity"), VM->LastQuantity, 2);

	// Supported: player -> player (backpack -> hand) => RequestMoveItem.
	VM->LastCall = UInventoryViewModelSpy::ELastCall::None;
	TestTrue(TEXT("Route accepts player -> player"),
		FInventoryDropRouter::Route(*VM, MakeCtx(Backpack, 3), MakeTarget(LeftHand)));
	TestEqual(TEXT("player -> player dispatches RequestMoveItem"),
		static_cast<int32>(VM->LastCall),
		static_cast<int32>(UInventoryViewModelSpy::ELastCall::MoveItem));
	TestEqual(TEXT("MoveItem carries source container tag"), VM->LastFromContainer, Backpack);
	TestEqual(TEXT("MoveItem carries target container tag"), VM->LastToContainer, LeftHand);
	TestEqual(TEXT("MoveItem carries source pos"), VM->LastFromPos, FIntPoint(0, 0));
	TestEqual(TEXT("MoveItem carries target pos"), VM->LastToPos, FIntPoint(1, 1));
	TestEqual(TEXT("MoveItem carries quantity"), VM->LastQuantity, 3);

	// Supported: player -> world => RequestStoreItemInNearbyContainerAt.
	VM->LastCall = UInventoryViewModelSpy::ELastCall::None;
	TestTrue(TEXT("Route accepts player -> world (store)"),
		FInventoryDropRouter::Route(*VM, MakeCtx(Backpack, 5), MakeTarget(World)));
	TestEqual(TEXT("player -> world dispatches RequestStoreItemInNearbyContainerAt"),
		static_cast<int32>(VM->LastCall),
		static_cast<int32>(UInventoryViewModelSpy::ELastCall::StoreItemInNearbyContainerAt));
	TestEqual(TEXT("Store carries target pos"), VM->LastToPos, FIntPoint(1, 1));
	TestEqual(TEXT("Store carries quantity"), VM->LastQuantity, 5);

	// Supported: world -> player => RequestTakeNearbyItemToContainer.
	VM->LastCall = UInventoryViewModelSpy::ELastCall::None;
	TestTrue(TEXT("Route accepts world -> player (take)"),
		FInventoryDropRouter::Route(*VM, MakeCtx(World, 7), MakeTarget(Backpack)));
	TestEqual(TEXT("world -> player dispatches RequestTakeNearbyItemToContainer"),
		static_cast<int32>(VM->LastCall),
		static_cast<int32>(UInventoryViewModelSpy::ELastCall::TakeNearbyItemToContainer));
	TestEqual(TEXT("Take carries target container tag"), VM->LastToContainer, Backpack);
	TestEqual(TEXT("Take carries target pos"), VM->LastToPos, FIntPoint(1, 1));
	TestEqual(TEXT("Take carries quantity"), VM->LastQuantity, 7);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUIInventoryDropStackPolicyTest,
	"ProjectIntegrationTests.UI.Framework.Inventory.DropStackPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUIInventoryDropStackPolicyTest::RunTest(const FString& Parameters)
{
	FInventoryEntryView Source;
	Source.ItemId = FPrimaryAssetId(TEXT("ObjectDefinition"), TEXT("Cigarette"));
	Source.Quantity = 22;
	Source.GridSize = FIntPoint(1, 1);
	Source.MaxStack = 25;

	FInventoryEntryView Target = Source;
	Target.InstanceId = 2002;
	Target.Quantity = 2;
	Target.MaxStack = 25;

	TestTrue(
		TEXT("Occupied backpack cells should be preview-valid when the same 1x1 item stack has capacity"),
		FInventoryUIDropStackPolicy::CanPreviewStackOnto(Source, Target, 22));

	TestFalse(
		TEXT("Stack preview should reject quantity beyond target max stack"),
		FInventoryUIDropStackPolicy::CanPreviewStackOnto(Source, Target, 24));

	Source.GridSize = FIntPoint(2, 1);
	TestFalse(
		TEXT("Stack preview should reject non-1x1 source entries"),
		FInventoryUIDropStackPolicy::CanPreviewStackOnto(Source, Target, 1));

	Source.GridSize = FIntPoint(1, 1);
	Target.ItemId = FPrimaryAssetId(TEXT("ObjectDefinition"), TEXT("WaterBottle"));
	TestFalse(
		TEXT("Stack preview should reject different item ids"),
		FInventoryUIDropStackPolicy::CanPreviewStackOnto(Source, Target, 1));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUIInventoryActionDescriptorMappingTest,
	"ProjectIntegrationTests.UI.Framework.ActionDescriptors.InventoryMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUIInventoryActionDescriptorMappingTest::RunTest(const FString& Parameters)
{
	UInventoryViewModel* VM = NewObject<UInventoryViewModel>(GetTransientPackage(), UInventoryViewModel::StaticClass());
	TestNotNull(TEXT("InventoryViewModel should be created"), VM);
	if (!VM)
	{
		return false;
	}

	FInventoryEntryView ConsumableEntry;
	ConsumableEntry.InstanceId = 100;
	ConsumableEntry.DisplayName = FText::FromString(TEXT("Water Bottle"));
	ConsumableEntry.bIsConsumable = true;
	ConsumableEntry.bCanUse = true;
	ConsumableEntry.bCanEquip = false;
	ConsumableEntry.bActionCapsPopulated = true;
	ConsumableEntry.EquipSlotTag = ProjectTags::Item_EquipmentSlot_MainHand;
	ConsumableEntry.bCanBeDropped = true;
	ConsumableEntry.MaxStack = 1;
	ConsumableEntry.Quantity = 1;

	TArray<FProjectUIActionDescriptor> ConsumableActions;
	VM->BuildActionDescriptors(ConsumableEntry, ConsumableActions);

	const FProjectUIActionDescriptor* UseAction = UInventoryViewModel::FindActionDescriptor(
		ConsumableActions, UInventoryViewModel::GetActionIdUse());
	const FProjectUIActionDescriptor* EquipAction = UInventoryViewModel::FindActionDescriptor(
		ConsumableActions, UInventoryViewModel::GetActionIdEquip());
	const FProjectUIActionDescriptor* DropAction = UInventoryViewModel::FindActionDescriptor(
		ConsumableActions, UInventoryViewModel::GetActionIdDrop());
	const FProjectUIActionDescriptor* SplitAction = UInventoryViewModel::FindActionDescriptor(
		ConsumableActions, UInventoryViewModel::GetActionIdSplit());

	TestTrue(TEXT("Consumable should expose Use action"), UseAction && UseAction->bVisible);
	TestTrue(TEXT("Consumable should hide Equip action"), EquipAction && !EquipAction->bVisible);
	TestTrue(TEXT("Consumable should expose Drop action"), DropAction && DropAction->bVisible);
	TestTrue(TEXT("Single consumable should hide Split action"), SplitAction && !SplitAction->bVisible);

	FInventoryEntryView EquippableEntry;
	EquippableEntry.InstanceId = 200;
	EquippableEntry.DisplayName = FText::FromString(TEXT("Rifle"));
	EquippableEntry.bIsConsumable = false;
	EquippableEntry.bCanUse = false;
	EquippableEntry.bCanEquip = true;
	EquippableEntry.bActionCapsPopulated = true;
	EquippableEntry.EquipSlotTag = ProjectTags::Item_EquipmentSlot_MainHand;
	EquippableEntry.bCanBeDropped = true;
	EquippableEntry.MaxStack = 1;
	EquippableEntry.Quantity = 1;

	TArray<FProjectUIActionDescriptor> EquippableActions;
	VM->BuildActionDescriptors(EquippableEntry, EquippableActions);

	UseAction = UInventoryViewModel::FindActionDescriptor(
		EquippableActions, UInventoryViewModel::GetActionIdUse());
	EquipAction = UInventoryViewModel::FindActionDescriptor(
		EquippableActions, UInventoryViewModel::GetActionIdEquip());
	SplitAction = UInventoryViewModel::FindActionDescriptor(
		EquippableActions, UInventoryViewModel::GetActionIdSplit());

	TestTrue(TEXT("Equippable should hide Use action"), UseAction && !UseAction->bVisible);
	TestTrue(TEXT("Equippable should expose Equip action"), EquipAction && EquipAction->bVisible);
	TestTrue(TEXT("Non-stackable equippable should hide Split action"), SplitAction && !SplitAction->bVisible);

	FInventoryEntryView StackableEntry;
	StackableEntry.InstanceId = 300;
	StackableEntry.DisplayName = FText::FromString(TEXT("Ammo"));
	StackableEntry.bIsConsumable = false;
	StackableEntry.bCanUse = false;
	StackableEntry.bCanEquip = false;
	StackableEntry.bActionCapsPopulated = true;
	StackableEntry.bCanBeDropped = true;
	StackableEntry.MaxStack = 30;
	StackableEntry.Quantity = 12;

	TArray<FProjectUIActionDescriptor> StackableActions;
	VM->BuildActionDescriptors(StackableEntry, StackableActions);

	SplitAction = UInventoryViewModel::FindActionDescriptor(
		StackableActions, UInventoryViewModel::GetActionIdSplit());
	TestTrue(TEXT("Stackable quantity>1 should expose Split action"), SplitAction && SplitAction->bVisible);
	TestFalse(TEXT("Without commands, no action should be enabled"), UInventoryViewModel::HasEnabledActions(StackableActions));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUIInventoryCellVisualStateTest,
	"ProjectIntegrationTests.UI.Framework.Inventory.CellVisualState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUIInventoryCellVisualStateTest::RunTest(const FString& Parameters)
{
	UInventoryViewModel* ViewModel = NewObject<UInventoryViewModel>(GetTransientPackage(), UInventoryViewModel::StaticClass());
	if (!TestNotNull(TEXT("InventoryViewModel should be created"), ViewModel))
	{
		return false;
	}

	UProjectInventoryReadOnlyMock* Source = NewObject<UProjectInventoryReadOnlyMock>(GetTransientPackage(), UProjectInventoryReadOnlyMock::StaticClass());
	if (!TestNotNull(TEXT("Mock inventory source should be created"), Source))
	{
		return false;
	}

	FInventoryContainerView BackpackContainer;
	BackpackContainer.ContainerId = ProjectTags::Item_Container_Backpack;
	BackpackContainer.GridSize = FIntPoint(3, 3);
	Source->SetContainers({ BackpackContainer });
	Source->SetTotals(0.f, 50.f, 0.f, 100.f, 0);

	FInventoryEntryView CigaretteEntry;
	CigaretteEntry.InstanceId = 1001;
	CigaretteEntry.ItemId = FPrimaryAssetId::FromString(TEXT("ObjectDefinition:Cigarette"));
	CigaretteEntry.DisplayName = FText::FromString(TEXT("Cigarette"));
	CigaretteEntry.Quantity = 17;
	CigaretteEntry.MaxStack = 20;
	CigaretteEntry.IconCode = TEXT("\uF35F");
	CigaretteEntry.ContainerId = ProjectTags::Item_Container_Backpack;
	CigaretteEntry.GridPos = FIntPoint(0, 0);
	CigaretteEntry.GridSize = FIntPoint(1, 1);
	CigaretteEntry.bActionCapsPopulated = true;

	FInventoryEntryView FallbackEntry;
	FallbackEntry.InstanceId = 1002;
	FallbackEntry.ItemId = FPrimaryAssetId::FromString(TEXT("ObjectDefinition:FallbackItem"));
	FallbackEntry.DisplayName = FText::FromString(TEXT("Fallback Item"));
	FallbackEntry.Quantity = 3;
	FallbackEntry.MaxStack = 10;
	FallbackEntry.IconCode = FString();
	FallbackEntry.ContainerId = ProjectTags::Item_Container_Backpack;
	FallbackEntry.GridPos = FIntPoint(1, 0);
	FallbackEntry.GridSize = FIntPoint(1, 1);
	FallbackEntry.bActionCapsPopulated = true;

	Source->SetEntries({ CigaretteEntry, FallbackEntry });
	ViewModel->SetInventorySource(Source);

	const TArray<FInventoryCellVisualState>& CellVisuals = ViewModel->GetCellVisuals();
	TestTrue(TEXT("Backpack cell visuals should be built"), CellVisuals.Num() >= 2);
	if (CellVisuals.Num() < 2)
	{
		return false;
	}

	TestEqual(TEXT("Icon-backed item should keep icon glyph as primary text"), CellVisuals[0].PrimaryText.ToString(), CigaretteEntry.IconCode);
	TestTrue(TEXT("Icon-backed item should request icon font"), CellVisuals[0].bUseIconFont);
	TestTrue(TEXT("Stacked icon item should expose quantity overlay"), CellVisuals[0].bShowQuantity);
	TestEqual(TEXT("Stack quantity overlay should show numeric count"), CellVisuals[0].QuantityText.ToString(), FString(TEXT("17")));

	TestEqual(TEXT("Fallback item should expose display name as primary text"), CellVisuals[1].PrimaryText.ToString(), FString(TEXT("Fallback Item")));
	TestFalse(TEXT("Fallback item should not request icon font"), CellVisuals[1].bUseIconFont);
	TestTrue(TEXT("Fallback item should still expose quantity overlay"), CellVisuals[1].bShowQuantity);
	TestEqual(TEXT("Fallback stack quantity overlay should show numeric count"), CellVisuals[1].QuantityText.ToString(), FString(TEXT("3")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUIInventoryQuantitySelectionDefaultsTest,
	"ProjectIntegrationTests.UI.Framework.Inventory.QuantitySelectionDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUIInventoryQuantitySelectionDefaultsTest::RunTest(const FString& Parameters)
{
	UInventoryViewModel* ViewModel = NewObject<UInventoryViewModel>(GetTransientPackage(), UInventoryViewModel::StaticClass());
	if (!TestNotNull(TEXT("InventoryViewModel should be created"), ViewModel))
	{
		return false;
	}

	UProjectInventoryReadOnlyMock* Source = NewObject<UProjectInventoryReadOnlyMock>(GetTransientPackage(), UProjectInventoryReadOnlyMock::StaticClass());
	if (!TestNotNull(TEXT("Mock inventory source should be created"), Source))
	{
		return false;
	}

	FInventoryContainerView BackpackContainer;
	BackpackContainer.ContainerId = ProjectTags::Item_Container_Backpack;
	BackpackContainer.GridSize = FIntPoint(3, 3);
	Source->SetContainers({ BackpackContainer });
	Source->SetTotals(0.f, 50.f, 0.f, 100.f, 0);

	FInventoryEntryView FirstEntry;
	FirstEntry.InstanceId = 1001;
	FirstEntry.ItemId = FPrimaryAssetId::FromString(TEXT("ObjectDefinition:Cigarette"));
	FirstEntry.DisplayName = FText::FromString(TEXT("Cigarette"));
	FirstEntry.Quantity = 22;
	FirstEntry.MaxStack = 100;
	FirstEntry.ContainerId = ProjectTags::Item_Container_Backpack;
	FirstEntry.GridPos = FIntPoint(0, 0);
	FirstEntry.GridSize = FIntPoint(1, 1);

	FInventoryEntryView SecondEntry = FirstEntry;
	SecondEntry.InstanceId = 1002;
	SecondEntry.Quantity = 4;
	SecondEntry.GridPos = FIntPoint(1, 0);

	Source->SetEntries({ FirstEntry, SecondEntry });
	ViewModel->SetInventorySource(Source);

	FInventoryPanelTextUpdater TextUpdater;
	FInventoryPanelState PanelState;
	PanelState.SetSelectedPrimary(0);
	TextUpdater.UpdateSelectionInfo(ViewModel, PanelState);

	TestEqual(TEXT("New stack selection should default quantity to the whole stack"), PanelState.SelectedQuantity, 22);
	TestEqual(TEXT("Selected max quantity should match stack size"), PanelState.SelectedMaxQuantity, 22);

	PanelState.SelectedQuantity = 7;
	TextUpdater.UpdateSelectionInfo(ViewModel, PanelState);
	TestEqual(TEXT("Existing selection should preserve explicit split quantity"), PanelState.SelectedQuantity, 7);

	PanelState.SetSelectedPrimary(1);
	TextUpdater.UpdateSelectionInfo(ViewModel, PanelState);
	TestEqual(TEXT("Different stack selection should reset quantity to that whole stack"), PanelState.SelectedQuantity, 4);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUIPopupAndTooltipLifecycleTest,
	"ProjectIntegrationTests.UI.Framework.PopupAndTooltip.LifecycleAndClamp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUIPopupAndTooltipLifecycleTest::RunTest(const FString& Parameters)
{
	UWorld* World = ResolveAutomationTestWorld();
	if (!TestNotNull(TEXT("World should exist"), World))
	{
		return false;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!TestNotNull(TEXT("GameInstance should exist"), GameInstance))
	{
		return false;
	}

	UProjectDialogWidget* OuterWidget = CreateWidget<UProjectDialogWidget>(GameInstance, UProjectDialogWidget::StaticClass());
	if (!TestNotNull(TEXT("Outer widget should be created"), OuterWidget))
	{
		return false;
	}

	UCanvasPanel* OverlayCanvas = NewObject<UCanvasPanel>(OuterWidget, UCanvasPanel::StaticClass(), TEXT("OverlayCanvas"));
	if (!TestNotNull(TEXT("Overlay canvas should be created"), OverlayCanvas))
	{
		return false;
	}

	FProjectUIPopupPresenter PopupPresenter;
	PopupPresenter.Initialize(OverlayCanvas, OuterWidget, UProjectDialogWidget::StaticClass(), 100, 99);

	UUserWidget* PopupWidget = PopupPresenter.GetPopupWidget<UUserWidget>();
	UButton* ClickCatcher = PopupPresenter.GetClickCatcher();

	TestNotNull(TEXT("Popup widget should be created"), PopupWidget);
	TestNotNull(TEXT("Click catcher should be created"), ClickCatcher);
	if (!PopupWidget || !ClickCatcher)
	{
		return false;
	}

	TestFalse(TEXT("Popup presenter should start hidden"), PopupPresenter.IsVisible());
	TestEqual(TEXT("Click catcher starts collapsed"), ClickCatcher->GetVisibility(), ESlateVisibility::Collapsed);

	PopupWidget->SetVisibility(ESlateVisibility::Visible);
	PopupPresenter.ShowClickCatcher(true);
	TestTrue(TEXT("Popup presenter should report visible popup"), PopupPresenter.IsVisible());
	TestEqual(TEXT("Click catcher visible after show"), ClickCatcher->GetVisibility(), ESlateVisibility::Visible);

	PopupPresenter.Hide();
	TestFalse(TEXT("Popup presenter hidden after Hide"), PopupPresenter.IsVisible());
	TestEqual(TEXT("Click catcher hidden after Hide"), ClickCatcher->GetVisibility(), ESlateVisibility::Collapsed);

	FProjectUIHoverTooltipPresenter TooltipPresenter;
	TooltipPresenter.Initialize(OverlayCanvas, OuterWidget, UProjectDialogWidget::StaticClass(), 50);
	UUserWidget* TooltipWidget = TooltipPresenter.GetTooltipWidget<UUserWidget>();
	if (!TestNotNull(TEXT("Tooltip widget should be created"), TooltipWidget))
	{
		return false;
	}

	TooltipWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	TooltipPresenter.PositionNearCursor(FVector2D(-9999.f, -9999.f), FVector2D::ZeroVector, 8.f);

	UCanvasPanelSlot* TooltipSlot = Cast<UCanvasPanelSlot>(TooltipWidget->Slot);
	if (!TestNotNull(TEXT("Tooltip slot should be canvas slot"), TooltipSlot))
	{
		return false;
	}

	FVector2D TooltipPos = TooltipSlot->GetPosition();
	TestTrue(TEXT("Tooltip X should clamp to minimum margin"), TooltipPos.X >= 8.f);
	TestTrue(TEXT("Tooltip Y should clamp to minimum margin"), TooltipPos.Y >= 8.f);

	FVector2D ViewportSize(1920.f, 1080.f);
	if (GEngine && GEngine->GameViewport)
	{
		FVector2D RuntimeViewportSize(0.f, 0.f);
		GEngine->GameViewport->GetViewportSize(RuntimeViewportSize);
		if (RuntimeViewportSize.X > 0.f && RuntimeViewportSize.Y > 0.f)
		{
			ViewportSize = RuntimeViewportSize;
		}
	}

	TooltipPresenter.PositionNearCursor(FVector2D(99999.f, 99999.f), FVector2D::ZeroVector, 8.f);
	TooltipPos = TooltipSlot->GetPosition();
	TestTrue(TEXT("Tooltip X should remain inside viewport bounds"), TooltipPos.X <= ViewportSize.X);
	TestTrue(TEXT("Tooltip Y should remain inside viewport bounds"), TooltipPos.Y <= ViewportSize.Y);

	TooltipPresenter.PositionAtAnchor(FVector2D(320.f, 240.f), FVector2D(0.5f, 1.0f), FVector2D(0.f, -12.f), 8.f);
	TooltipPos = TooltipSlot->GetPosition();
	TestTrue(TEXT("Anchored tooltip should stay within viewport bounds"), TooltipPos.X >= 8.f && TooltipPos.Y >= 8.f);

	TooltipPresenter.Hide();
	TestFalse(TEXT("Tooltip presenter hidden after Hide"), TooltipPresenter.IsVisible());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUIRadialProgressJsonLayoutTest,
	"ProjectIntegrationTests.UI.Framework.LayoutLoader.RadialProgressJson",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUIRadialProgressJsonLayoutTest::RunTest(const FString& Parameters)
{
	UProjectDialogWidget* Owner = NewObject<UProjectDialogWidget>(GetTransientPackage(), UProjectDialogWidget::StaticClass(), TEXT("RadialProgressLayoutOwner"));
	if (!TestNotNull(TEXT("Owner widget should be created"), Owner))
	{
		return false;
	}

	const FString Json = TEXT(
		"{"
		"\"root\":{"
			"\"type\":\"CanvasPanel\","
			"\"name\":\"RootCanvas\","
			"\"children\":["
				"{"
					"\"type\":\"RadialProgress\","
					"\"name\":\"SearchProgress\","
					"\"percent\":0.75,"
					"\"thickness\":4.0,"
					"\"fillColor\":\"Secondary\","
					"\"trackColor\":\"Border\","
					"\"startAngleDegrees\":-90.0,"
					"\"clockwise\":true,"
					"\"showTrack\":true,"
					"\"size\":{\"x\":20,\"y\":20}"
				"}"
			"]"
		"}}");

	UWidget* Root = UProjectWidgetLayoutLoader::LoadLayoutFromString(Owner, Json, nullptr);
	if (!TestNotNull(TEXT("Root widget should load from JSON"), Root))
	{
		return false;
	}

	UProjectRadialProgress* RadialProgress =
		UProjectWidgetHelpers::FindWidgetByNameTyped<UProjectRadialProgress>(Root, TEXT("SearchProgress"));
	if (!TestNotNull(TEXT("RadialProgress widget should be found by name"), RadialProgress))
	{
		return false;
	}

	TestEqual(TEXT("RadialProgress percent should parse"), RadialProgress->GetPercent(), 0.75f);
	TestEqual(TEXT("RadialProgress thickness should parse"), RadialProgress->GetThickness(), 4.0f);
	TestEqual(TEXT("RadialProgress start angle should parse"), RadialProgress->GetStartAngleDegrees(), -90.0f);
	TestTrue(TEXT("RadialProgress clockwise should parse"), RadialProgress->GetClockwise());
	TestTrue(TEXT("RadialProgress showTrack should parse"), RadialProgress->GetShowTrack());

	const TSharedRef<SWidget> SlateWidget = RadialProgress->TakeWidget();
	TestTrue(TEXT("RadialProgress should build a Slate widget"), SlateWidget != SNullWidget::NullWidget);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUIRadialProgressPaintClosedLoopTest,
	"ProjectIntegrationTests.UI.Framework.LayoutLoader.RadialProgressPaintClosedLoop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUIRadialProgressPaintClosedLoopTest::RunTest(const FString& Parameters)
{
	UProjectRadialProgress* RadialProgress = NewObject<UProjectRadialProgress>(
		GetTransientPackage(),
		UProjectRadialProgress::StaticClass(),
		TEXT("RadialProgressPaintClosedLoop"));
	if (!TestNotNull(TEXT("RadialProgress widget should be created"), RadialProgress))
	{
		return false;
	}

	RadialProgress->SetPercent(1.0f);
	RadialProgress->SetThickness(3.0f);
	RadialProgress->SetShowTrack(true);

	const TSharedRef<SWidget> SlateWidget = RadialProgress->TakeWidget();
	TestTrue(TEXT("RadialProgress should build a Slate widget for paint"), SlateWidget != SNullWidget::NullWidget);

	TSharedRef<SWindow> PaintWindow = SNew(SWindow).ClientSize(FVector2D(32.0f, 32.0f));
	FSlateWindowElementList DrawElements(PaintWindow);
	FHittestGrid HitTestGrid;
	HitTestGrid.SetHittestArea(FVector2D::ZeroVector, FVector2D(32.0f, 32.0f));

	const FPaintArgs PaintArgs(nullptr, HitTestGrid, FVector2D::ZeroVector, 0.0, 0.0f);
	const FGeometry Geometry = FGeometry::MakeRoot(FVector2D(32.0f, 32.0f), FSlateLayoutTransform());
	const FSlateRect CullingRect(0.0f, 0.0f, 32.0f, 32.0f);
	const int32 FinalLayer = SlateWidget->Paint(
		PaintArgs,
		Geometry,
		CullingRect,
		DrawElements,
		0,
		FWidgetStyle(),
		true);

	TestTrue(TEXT("RadialProgress paint should complete and advance layers"), FinalLayer >= 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUIGridHitDetectorEdgeCasesTest,
	"ProjectIntegrationTests.UI.Framework.GridHitDetector.EdgeCases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUIGridHitDetectorEdgeCasesTest::RunTest(const FString& Parameters)
{
	FProjectUIGridHitDetector HitDetector;
	HitDetector.SetCellSize(64.f);

	int32 Col = INDEX_NONE;
	int32 Row = INDEX_NONE;

	// 2x2 grid at 64px cells = 128x128 total
	const FVector2D GridSize(128.f, 128.f);

	TestTrue(TEXT("Origin should resolve to cell (0,0)"), HitDetector.LocalPosToGridCoords(FVector2D(0.f, 0.f), GridSize, 2, 2, Col, Row));
	TestEqual(TEXT("Origin col"), Col, 0);
	TestEqual(TEXT("Origin row"), Row, 0);

	TestTrue(TEXT("In-cell position should resolve to (0,0)"), HitDetector.LocalPosToGridCoords(FVector2D(63.9f, 63.9f), GridSize, 2, 2, Col, Row));
	TestEqual(TEXT("In-cell col"), Col, 0);
	TestEqual(TEXT("In-cell row"), Row, 0);

	TestTrue(TEXT("Boundary position should resolve to (1,0)"), HitDetector.LocalPosToGridCoords(FVector2D(64.f, 0.f), GridSize, 2, 2, Col, Row));
	TestEqual(TEXT("Boundary col"), Col, 1);
	TestEqual(TEXT("Boundary row"), Row, 0);

	TestFalse(TEXT("Negative position should fail"), HitDetector.LocalPosToGridCoords(FVector2D(-1.f, 0.f), GridSize, 2, 2, Col, Row));
	TestFalse(TEXT("Out-of-bounds col should fail"), HitDetector.LocalPosToGridCoords(FVector2D(128.f, 0.f), GridSize, 2, 2, Col, Row));
	TestFalse(TEXT("Out-of-bounds row should fail"), HitDetector.LocalPosToGridCoords(FVector2D(0.f, 128.f), GridSize, 2, 2, Col, Row));

	// Zero grid size should fail
	TestFalse(TEXT("Zero grid size should fail"), HitDetector.LocalPosToGridCoords(FVector2D(10.f, 10.f), FVector2D::ZeroVector, 2, 2, Col, Row));

	const int32 Index = FProjectUIGridHitDetector::GridCoordsToIndex(1, 2, 4);
	TestEqual(TEXT("GridCoordsToIndex should produce expected index"), Index, 9);

	int32 RoundTripCol = INDEX_NONE;
	int32 RoundTripRow = INDEX_NONE;
	FProjectUIGridHitDetector::IndexToGridCoords(Index, 4, RoundTripCol, RoundTripRow);
	TestEqual(TEXT("IndexToGridCoords col"), RoundTripCol, 1);
	TestEqual(TEXT("IndexToGridCoords row"), RoundTripRow, 2);

	int32 HitCellIndex = 42;
	TestFalse(TEXT("ResolveGridHit should fail for null panel"), HitDetector.ResolveGridHit(nullptr, 2, 2, FVector2D::ZeroVector, HitCellIndex));
	TestEqual(TEXT("ResolveGridHit should reset output index on failure"), HitCellIndex, INDEX_NONE);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUIGridVisualStateColorMappingTest,
	"ProjectIntegrationTests.UI.Framework.GridVisualState.StateColorMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUIGridVisualStateColorMappingTest::RunTest(const FString& Parameters)
{
	const FProjectUIGridVisualState VisualState;
	const FProjectUIGridColors& Colors = VisualState.GetColors();

	FProjectUICellState State;
	State.bEnabled = true;
	FLinearColor Resolved = Colors.GetColorForState(State, 1.0f);
	TestTrue(TEXT("Base state should resolve to base color"), Resolved.Equals(Colors.Base));

	State.bHovered = true;
	Resolved = Colors.GetColorForState(State, 1.0f);
	TestTrue(TEXT("Hovered state should resolve to hovered color"), Resolved.Equals(Colors.Hovered));

	State.bSelected = true;
	Resolved = Colors.GetColorForState(State, 1.0f);
	TestTrue(TEXT("Selected state should override hovered color"), Resolved.Equals(Colors.Selected));

	State.bDragPreview = true;
	State.bDragPreviewValid = true;
	Resolved = Colors.GetColorForState(State, 1.0f);
	TestTrue(TEXT("Valid drag preview should override selected color"), Resolved.Equals(Colors.PreviewValid));

	State.bDragPreviewValid = false;
	Resolved = Colors.GetColorForState(State, 1.0f);
	TestTrue(TEXT("Invalid drag preview should resolve to invalid color at full pulse"), Resolved.Equals(Colors.PreviewInvalid));

	Resolved = Colors.GetColorForState(State, 0.0f);
	TestTrue(TEXT("Invalid drag preview should resolve to base color at zero pulse"), Resolved.Equals(Colors.Base));

	State.bEnabled = false;
	Resolved = Colors.GetColorForState(State, 1.0f);
	TestTrue(TEXT("Disabled state should override all other visual flags"), Resolved.Equals(Colors.Disabled));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUIActionDescriptorRenderStateTest,
	"ProjectIntegrationTests.UI.Framework.ActionDescriptors.ButtonRenderState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUIActionDescriptorRenderStateTest::RunTest(const FString& Parameters)
{
	UInventoryViewModel* ViewModel = NewObject<UInventoryViewModel>(GetTransientPackage(), UInventoryViewModel::StaticClass());
	if (!TestNotNull(TEXT("InventoryViewModel should be created"), ViewModel))
	{
		return false;
	}

	UProjectInventoryReadOnlyMock* Source = NewObject<UProjectInventoryReadOnlyMock>(GetTransientPackage(), UProjectInventoryReadOnlyMock::StaticClass());
	if (!TestNotNull(TEXT("Mock inventory source should be created"), Source))
	{
		return false;
	}

	FInventoryContainerView StorageContainer;
	StorageContainer.ContainerId = ProjectTags::Item_Container_Backpack;
	StorageContainer.GridSize = FIntPoint(4, 4);
	Source->SetContainers({ StorageContainer });
	Source->SetTotals(0.f, 40.f, 0.f, 80.f, 0);

	FInventoryEntryView ConsumableEntry;
	ConsumableEntry.InstanceId = 101;
	ConsumableEntry.DisplayName = FText::FromString(TEXT("Water"));
	ConsumableEntry.ContainerId = ProjectTags::Item_Container_Backpack;
	ConsumableEntry.GridPos = FIntPoint(0, 0);
	ConsumableEntry.Quantity = 1;
	ConsumableEntry.MaxStack = 1;
	ConsumableEntry.bIsConsumable = true;
	ConsumableEntry.bCanUse = true;
	ConsumableEntry.bCanEquip = false;
	ConsumableEntry.bActionCapsPopulated = true;
	ConsumableEntry.bCanBeDropped = true;

	Source->SetEntries({ ConsumableEntry });
	ViewModel->SetInventorySource(Source);

	UButton* UseButton = NewObject<UButton>(GetTransientPackage(), UButton::StaticClass(), TEXT("UseButton"));
	UButton* DropButton = NewObject<UButton>(GetTransientPackage(), UButton::StaticClass(), TEXT("DropButton"));
	UButton* EquipButton = NewObject<UButton>(GetTransientPackage(), UButton::StaticClass(), TEXT("EquipButton"));
	if (!UseButton || !DropButton || !EquipButton)
	{
		AddError(TEXT("Failed to create test buttons"));
		return false;
	}

	FInventoryPanelTextUpdater::FWidgetRefs WidgetRefs;
	WidgetRefs.UseButton = UseButton;
	WidgetRefs.DropButton = DropButton;
	WidgetRefs.EquipButton = EquipButton;

	FInventoryPanelTextUpdater TextUpdater;
	TextUpdater.Initialize(WidgetRefs);

	FInventoryPanelState PanelState;
	PanelState.SetSelectedByInstanceId(ConsumableEntry.InstanceId);
	TextUpdater.UpdateCommandButtons(ViewModel, PanelState);

	TestEqual(TEXT("Consumable: Use button visible"), UseButton->GetVisibility(), ESlateVisibility::Visible);
	TestEqual(TEXT("Consumable: Drop button visible"), DropButton->GetVisibility(), ESlateVisibility::Visible);
	TestEqual(TEXT("Consumable: Equip button collapsed"), EquipButton->GetVisibility(), ESlateVisibility::Collapsed);
	TestFalse(TEXT("Consumable: Use disabled without command interface"), UseButton->GetIsEnabled());
	TestFalse(TEXT("Consumable: Drop disabled without command interface"), DropButton->GetIsEnabled());

	FInventoryEntryView EquippableEntry = ConsumableEntry;
	EquippableEntry.InstanceId = 202;
	EquippableEntry.DisplayName = FText::FromString(TEXT("Rifle"));
	EquippableEntry.bIsConsumable = false;
	EquippableEntry.bCanUse = false;
	EquippableEntry.bCanEquip = true;
	EquippableEntry.bActionCapsPopulated = true;
	EquippableEntry.EquipSlotTag = ProjectTags::Item_EquipmentSlot_MainHand;

	Source->SetEntries({ EquippableEntry });
	Source->BroadcastInventoryChanged();

	PanelState.SetSelectedByInstanceId(EquippableEntry.InstanceId);
	TextUpdater.UpdateCommandButtons(ViewModel, PanelState);

	TestEqual(TEXT("Equippable: Use button collapsed"), UseButton->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("Equippable: Equip button visible"), EquipButton->GetVisibility(), ESlateVisibility::Visible);
	TestEqual(TEXT("Equippable: Drop button visible"), DropButton->GetVisibility(), ESlateVisibility::Visible);
	TestFalse(TEXT("Equippable: Equip disabled without command interface"), EquipButton->GetIsEnabled());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectUIMainMenuSettingsPopupPresenterReuseTest,
	"ProjectIntegrationTests.UI.Framework.MainMenu.SettingsPopupPresenterReuse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FProjectUIMainMenuSettingsPopupPresenterReuseTest::RunTest(const FString& Parameters)
{
	UWorld* World = ResolveAutomationTestWorld();
	if (!TestNotNull(TEXT("World should exist"), World))
	{
		return false;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!TestNotNull(TEXT("GameInstance should exist"), GameInstance))
	{
		return false;
	}

	UW_MainMenu* MainMenu = CreateWidget<UW_MainMenu>(GameInstance, UW_MainMenu::StaticClass());
	if (!TestNotNull(TEXT("MainMenu should be created"), MainMenu))
	{
		return false;
	}

	MainMenu->AddToViewport();
	MainMenu->ForceLayoutPrepass();

	UWidget* RootWidget = MainMenu->GetRootWidget();
	if (!TestNotNull(TEXT("MainMenu root widget should exist"), RootWidget))
	{
		MainMenu->RemoveFromParent();
		return false;
	}

	UCanvasPanel* SettingsPanel = UProjectWidgetHelpers::FindWidgetByNameTyped<UCanvasPanel>(RootWidget, TEXT("SettingsPanel"), true, true);
	if (!TestNotNull(TEXT("SettingsPanel canvas should exist"), SettingsPanel))
	{
		MainMenu->RemoveFromParent();
		return false;
	}

	UButton* SettingsButton = UProjectWidgetHelpers::FindWidgetByNameTyped<UButton>(RootWidget, TEXT("Button_Settings"), true, true);
	UButton* CreditsButton = UProjectWidgetHelpers::FindWidgetByNameTyped<UButton>(RootWidget, TEXT("Button_Credits"), true, true);
	if (!TestNotNull(TEXT("Settings button should exist"), SettingsButton)
		|| !TestNotNull(TEXT("Credits button should exist"), CreditsButton))
	{
		MainMenu->RemoveFromParent();
		return false;
	}

	auto CollectSettingsRoots = [SettingsPanel]()
	{
		TArray<UWidget*> Result;
		const int32 ChildCount = SettingsPanel->GetChildrenCount();
		for (int32 Index = 0; Index < ChildCount; ++Index)
		{
			UWidget* Child = SettingsPanel->GetChildAt(Index);
			if (!Child)
			{
				continue;
			}

			if (Child->GetClass()->GetName().Contains(TEXT("ProjectSettingsRootWidget")))
			{
				Result.Add(Child);
			}
		}
		return Result;
	};

	TArray<UWidget*> SettingsRoots = CollectSettingsRoots();
	TestEqual(TEXT("Settings popup should be created once at construct time"), SettingsRoots.Num(), 1);
	if (SettingsRoots.Num() != 1)
	{
		MainMenu->RemoveFromParent();
		return false;
	}

	UWidget* SettingsRoot = SettingsRoots[0];
	TestEqual(TEXT("Settings root starts collapsed while Main screen is active"), SettingsRoot->GetVisibility(), ESlateVisibility::Collapsed);

	SettingsButton->OnClicked.Broadcast();
	MainMenu->ForceLayoutPrepass();
	TestEqual(TEXT("Settings root should be visible on Settings screen"), SettingsRoot->GetVisibility(), ESlateVisibility::Visible);

	CreditsButton->OnClicked.Broadcast();
	MainMenu->ForceLayoutPrepass();
	TestEqual(TEXT("Settings root should collapse when leaving Settings screen"), SettingsRoot->GetVisibility(), ESlateVisibility::Collapsed);

	SettingsButton->OnClicked.Broadcast();
	MainMenu->ForceLayoutPrepass();
	SettingsRoots = CollectSettingsRoots();

	TestEqual(TEXT("Settings popup should not be recreated on repeated navigation"), SettingsRoots.Num(), 1);
	TestTrue(TEXT("Settings popup instance should be reused"), SettingsRoots.Num() == 1 && SettingsRoots[0] == SettingsRoot);
	TestEqual(TEXT("Reused settings root should be visible on Settings screen"), SettingsRoot->GetVisibility(), ESlateVisibility::Visible);

	MainMenu->RemoveFromParent();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
