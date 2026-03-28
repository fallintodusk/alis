// Copyright ALIS. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Presentation/ProjectUIWidgetBinder.h"
#include "Presentation/ProjectUIGridVisualState.h"
#include "Interaction/ProjectUIGridDragDropController.h"
#include "Interaction/ProjectUIGridHitDetector.h"
#include "Overlay/ProjectUIPopupPresenter.h"
#include "Overlay/ProjectUIHoverTooltipPresenter.h"
#include "MVVM/InventoryViewModel.h"
#include "Widgets/W_MainMenu.h"
#include "Support/ProjectInventoryReadOnlyMock.h"
#include "Widgets/InventoryPanelTextUpdater.h"
#include "Widgets/InventoryPanelState.h"
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

	const bool bOutOfBounds = FProjectUIGridDragDropController::ValidateFootprint(
		Payload, 3, 3, 4, 4, false, EnabledChecker, OccupantChecker);
	TestFalse(TEXT("Footprint should fail when placement exceeds grid bounds"), bOutOfBounds);

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
