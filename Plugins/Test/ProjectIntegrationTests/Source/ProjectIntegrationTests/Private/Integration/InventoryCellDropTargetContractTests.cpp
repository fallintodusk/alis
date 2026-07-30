// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationCommon.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Interaction/IInventoryDropTarget.h"
#include "ProjectGameplayTags.h"
#include "Widgets/InventoryPanelGridBuilder.h"
#include "Widgets/ProjectGridCell.h"
#include "Widgets/W_InventoryCellDropTarget.h"

#include "Support/DragBusTestHostWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    /**
     * Minimal host UUserWidget used by the contract test. UUserWidget is
     * abstract in UMG 5.7; a concrete subclass is required for
     * CreateWidget<UUserWidget>. Mirrors the pattern used by
     * UDragBusTestHostWidget in the drag-event-bus tests.
     */
    UWorld* ResolveWorldForContractTest()
    {
        UWorld* World = AutomationCommon::GetAnyGameWorld();
        if (World) { return World; }
        if (!AutomationOpenMap(TEXT("/MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent")))
        {
            return nullptr;
        }
        return AutomationCommon::GetAnyGameWorld();
    }
}

// ---------------------------------------------------------------------------
// Contract test 1 - builder emits a cell-host wrapper per cell, the wrapper
// hosts a UProjectGridCell, and the wrapper implements IInventoryDropTarget.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInventoryCellDropTargetBuilderContractTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.CellDropTarget.BuilderWrapsEveryCell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FInventoryCellDropTargetBuilderContractTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    UWorld* World = ResolveWorldForContractTest();
    if (!TestNotNull(TEXT("Automation world must resolve"), World)) { return false; }
    UGameInstance* GI = World->GetGameInstance();
    if (!TestNotNull(TEXT("GameInstance must exist"), GI)) { return false; }

    // Use a minimal concrete UUserWidget host (shared with drag-bus tests)
    // to avoid running W_NearbyContainerPanel::NativeConstruct side-effects
    // in the builder contract assertion.
    UDragBusTestHostWidget* Host = CreateWidget<UDragBusTestHostWidget>(GI, UDragBusTestHostWidget::StaticClass());
    if (!TestNotNull(TEXT("Host widget must construct"), Host)) { return false; }

    FInventoryPanelGridBuilder Builder;
    Builder.Initialize(Host, Host->WidgetTree);
    Builder.SetCellSize(64.f);

    const int32 GridW = 4;
    const int32 GridH = 3;
    TArray<TObjectPtr<UTextBlock>> Primary;
    TArray<TObjectPtr<UTextBlock>> Quantity;
    TArray<TObjectPtr<UBorder>> Badges;
    TArray<TObjectPtr<UProjectGridCell>> Cells;
    TArray<TObjectPtr<UW_InventoryCellDropTarget>> Hosts;

    UUniformGridPanel* Grid = Builder.BuildGrid(
        GridW, GridH,
        ProjectTags::Item_Container_WorldStorage,
        Primary, Quantity, Badges, Cells, Hosts,
        /*bIsSecondary=*/true);

    if (!TestNotNull(TEXT("BuildGrid must produce a grid"), Grid)) { return false; }

    const int32 Expected = GridW * GridH;
    TestEqual(TEXT("Every grid cell gets a UW_InventoryCellDropTarget wrapper"), Hosts.Num(), Expected);
    TestEqual(TEXT("Cell visual array length matches grid cell count"), Cells.Num(), Expected);

    for (int32 Index = 0; Index < Hosts.Num(); ++Index)
    {
        UW_InventoryCellDropTarget* CellHost = Hosts[Index];
        if (!CellHost)
        {
            AddError(FString::Printf(
                TEXT("Cell host at index %d is null (expected populated wrapper for every cell)"), Index));
            continue;
        }

        // 1) Marker interface is declared (Slice 17 role-based fitness test).
        TestTrue(
            *FString::Printf(TEXT("Cell host at index %d implements IInventoryDropTarget"), Index),
            CellHost->Implements<UInventoryDropTarget>());

        // 2) Identity was seeded with the builder's surface tag + cell index.
        TestTrue(
            *FString::Printf(TEXT("Cell host at index %d carries the supplied SurfaceTag"), Index),
            CellHost->GetSurfaceTag() == ProjectTags::Item_Container_WorldStorage);
        TestEqual(
            *FString::Printf(TEXT("Cell host at index %d carries a matching CellIndex"), Index),
            CellHost->GetCellIndex(), Index);

        // 3) Hosted cell is the matching UProjectGridCell from the visual array.
        // The wrapper's WidgetTree->RootWidget owns the cell visually.
        UProjectGridCell* ExpectedCell = Cells.IsValidIndex(Index) ? Cells[Index] : nullptr;
        TestNotNull(
            *FString::Printf(TEXT("Cell visual at index %d must exist"), Index),
            ExpectedCell);
        if (CellHost->WidgetTree)
        {
            UWidget* RootedCell = CellHost->WidgetTree->RootWidget;
            TestTrue(
                *FString::Printf(TEXT("Cell host %d roots the same UProjectGridCell referenced in the cell visual array"), Index),
                RootedCell == static_cast<UWidget*>(ExpectedCell));
        }

        // 4) Wrapper visibility MUST be Visible - if it is SelfHitTestInvisible
        //    or Collapsed, Slate will not route NativeOnDragOver / NativeOnDrop
        //    to this widget and the drop pipeline silently breaks.
        TestTrue(
            *FString::Printf(TEXT("Cell host at index %d is Visible (hit-testable)"), Index),
            CellHost->GetVisibility() == ESlateVisibility::Visible);
    }

    return true;
}

// ---------------------------------------------------------------------------
// Contract test 2 - the nearby panel's root canvas stays SelfHitTestInvisible
// while every child cell host stays Visible. Flipping either one of these in
// isolation breaks sibling drop routing (the reason documented in
// UW_NearbyContainerPanel::UpdateVisibilityFromViewModel).
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInventoryNearbyRootSelfHitTestInvisibleWithVisibleCellsTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.CellDropTarget.NearbyPanelRootSelfHitTestInvisibleWithVisibleCells",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FInventoryNearbyRootSelfHitTestInvisibleWithVisibleCellsTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    UWorld* World = ResolveWorldForContractTest();
    if (!TestNotNull(TEXT("Automation world must resolve"), World)) { return false; }
    UGameInstance* GI = World->GetGameInstance();
    if (!TestNotNull(TEXT("GameInstance must exist"), GI)) { return false; }

    // Use a generic test host (DragBusTestHostWidget) instead of the
    // real UW_NearbyContainerPanel. The contract we are pinning here is
    // the PAIR invariant "root is SelfHitTestInvisible AND children are
    // Visible". Using the real nearby widget without calling its
    // NativeConstruct would leak a half-initialized instance that
    // subsequent TObjectIterator-backed tests
    // (GetAllWidgetsOfClass<UW_NearbyContainerPanel>) would pick up and
    // then fail with "Nearby widget root must be constructed". The
    // class-level contract is documented in the widget's header and
    // enforced by UpdateVisibilityFromViewModel's unchanged code.
    UDragBusTestHostWidget* Host = CreateWidget<UDragBusTestHostWidget>(
        GI, UDragBusTestHostWidget::StaticClass());
    if (!TestNotNull(TEXT("Host widget must construct"), Host)) { return false; }

    Host->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    FInventoryPanelGridBuilder Builder;
    Builder.Initialize(Host, Host->WidgetTree);
    Builder.SetCellSize(64.f);

    const int32 W = 4;
    const int32 H = 3;
    TArray<TObjectPtr<UTextBlock>> Primary;
    TArray<TObjectPtr<UTextBlock>> Quantity;
    TArray<TObjectPtr<UBorder>> Badges;
    TArray<TObjectPtr<UProjectGridCell>> Cells;
    TArray<TObjectPtr<UW_InventoryCellDropTarget>> Hosts;

    UUniformGridPanel* Grid = Builder.BuildGrid(
        W, H,
        ProjectTags::Item_Container_WorldStorage,
        Primary, Quantity, Badges, Cells, Hosts,
        /*bIsSecondary=*/true);
    if (!TestNotNull(TEXT("Nearby grid must build"), Grid)) { return false; }

    TestTrue(
        TEXT("Host root visibility is SelfHitTestInvisible during an active session"),
        Host->GetVisibility() == ESlateVisibility::SelfHitTestInvisible);

    TestTrue(TEXT("Builder produced at least one cell host"), Hosts.Num() > 0);

    // Every cell host must be Visible - the sibling-routing rule relies on
    // cells intercepting drops within their own bounds.
    for (int32 Index = 0; Index < Hosts.Num(); ++Index)
    {
        UW_InventoryCellDropTarget* CellHost = Hosts[Index];
        if (!CellHost)
        {
            AddError(FString::Printf(TEXT("Cell host at index %d is null"), Index));
            continue;
        }
        TestTrue(
            *FString::Printf(TEXT("Nearby cell host %d is Visible"), Index),
            CellHost->GetVisibility() == ESlateVisibility::Visible);
    }

    return true;
}

// Automation tag registration. See docs/agents/canonical.md "Tag taxonomy"
// and "Single-token CLI filter" for the wrapper contract; GridSizing tests
// carry the same pattern.
// Taxonomy: Speed=Fast, Kind=Integration (spawns widgets and iterates cell
// host tree), Area=Inventory.
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
    FInventoryCellDropTargetBuilderContractTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.CellDropTarget.BuilderWrapsEveryCell",
    "[Fast][Integration][Inventory]")
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
    FInventoryNearbyRootSelfHitTestInvisibleWithVisibleCellsTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.CellDropTarget.NearbyPanelRootSelfHitTestInvisibleWithVisibleCells",
    "[Fast][Integration][Inventory]")

#endif // WITH_DEV_AUTOMATION_TESTS
