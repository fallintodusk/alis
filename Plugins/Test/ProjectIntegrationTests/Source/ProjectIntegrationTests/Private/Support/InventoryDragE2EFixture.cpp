// Copyright ALIS. All Rights Reserved.

#include "Support/InventoryDragE2EFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Engine/GameInstance.h"
#include "Widgets/InventoryPanelGridBuilder.h"
#include "Widgets/ProjectGridCell.h"
#include "Widgets/W_InventoryCellDropTarget.h"

#include "Support/DragBusTestHostWidget.h"

bool FInventoryDragE2EFixture::BuildCommon(
    UGameInstance* GI,
    const FGameplayTag& SurfaceTag,
    int32 GridW,
    int32 GridH,
    bool bAddToViewport,
    FInventoryDragE2EFixture& Out)
{
    if (!GI) { return false; }

    UDragBusTestHostWidget* Host = CreateWidget<UDragBusTestHostWidget>(
        GI, UDragBusTestHostWidget::StaticClass());
    if (!Host || !Host->WidgetTree) { return false; }

    FInventoryPanelGridBuilder Builder;
    Builder.Initialize(Host, Host->WidgetTree);
    Builder.SetCellSize(64.f);

    // Out-arrays the builder fills. We only keep the CellHosts on the
    // fixture because tests only need to reach into the wrapper widgets
    // by row/col. The visual sub-widgets stay scoped to this call.
    TArray<TObjectPtr<UTextBlock>> Primary;
    TArray<TObjectPtr<UTextBlock>> Quantity;
    TArray<TObjectPtr<UBorder>> Badges;
    TArray<TObjectPtr<UProjectGridCell>> Cells;

    UUniformGridPanel* Grid = Builder.BuildGrid(
        GridW, GridH, SurfaceTag,
        Primary, Quantity, Badges, Cells, Out.CellHosts,
        /*bIsSecondary=*/false);
    if (!Grid) { return false; }

    Host->WidgetTree->RootWidget = Grid;
    if (bAddToViewport)
    {
        Host->AddToViewport();
        Host->ForceLayoutPrepass();
    }

    Out.Host = Host;
    Out.Grid = Grid;
    Out.SurfaceTag = SurfaceTag;
    Out.Dims = FIntPoint(GridW, GridH);
    Out.bInViewport = bAddToViewport;
    return true;
}

bool FInventoryDragE2EFixture::BuildPainted(
    UGameInstance* GI,
    const FGameplayTag& SurfaceTag,
    int32 GridW,
    int32 GridH,
    FInventoryDragE2EFixture& Out)
{
    return BuildCommon(GI, SurfaceTag, GridW, GridH, /*bAddToViewport=*/true, Out);
}

bool FInventoryDragE2EFixture::BuildTagOnly(
    UGameInstance* GI,
    const FGameplayTag& SurfaceTag,
    int32 GridW,
    int32 GridH,
    FInventoryDragE2EFixture& Out)
{
    return BuildCommon(GI, SurfaceTag, GridW, GridH, /*bAddToViewport=*/false, Out);
}

void FInventoryDragE2EFixture::ReleaseHostFromViewport()
{
    if (bInViewport && Host)
    {
        Host->RemoveFromParent();
    }
    bInViewport = false;
    // Do not null Host/Grid: some tests still need to call GetCachedGeometry
    // or inspect CellHosts in their own post-teardown asserts. Letting GC
    // reap them naturally is fine because the fixture's destructor already
    // ran by then.
}

#endif // WITH_DEV_AUTOMATION_TESTS
