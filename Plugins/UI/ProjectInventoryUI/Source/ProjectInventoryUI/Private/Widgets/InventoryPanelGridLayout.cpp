// Copyright ALIS. All Rights Reserved.

#include "Widgets/InventoryPanelGridLayout.h"

#include "Widgets/W_InventoryPanel.h"
#include "Widgets/InventoryPanelGridBuilder.h"
#include "Widgets/ProjectGridCell.h"
#include "Widgets/W_InventoryEquipSlotDropTarget.h"
#include "Widgets/InventoryDragDropOperation.h"
#include "MVVM/InventoryViewModel.h"
#include "Interfaces/IInventoryReadOnly.h"
#include "Layout/ProjectWidgetLayoutLoader.h"
#include "Theme/ProjectUIThemeData.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "ProjectGameplayTags.h"

namespace InventoryPanelGridLayout
{

int32 EncodePocketCellIndex(int32 PocketIndex, int32 CellIndex)
{
    if (PocketIndex < 0 || PocketIndex > 0x7FFF || CellIndex < 0 || CellIndex > 0xFFFF)
    {
        return INDEX_NONE;
    }

    return (PocketIndex << 16) | (CellIndex & 0xFFFF);
}

bool DecodePocketCellIndex(int32 EncodedCellIndex, int32& OutPocketIndex, int32& OutCellIndex)
{
    if (EncodedCellIndex < 0)
    {
        return false;
    }

    OutPocketIndex = (EncodedCellIndex >> 16) & 0x7FFF;
    OutCellIndex = EncodedCellIndex & 0xFFFF;
    return true;
}

void ApplyGridHostSize(USizeBox* Host, int32 GridWidth, int32 GridHeight, float CellSize)
{
    if (!Host || GridWidth <= 0 || GridHeight <= 0)
    {
        return;
    }
    const FIntPoint HostSize = FInventoryPanelGridBuilder::ComputeGridHostPixelSize(GridWidth, GridHeight, CellSize);
    Host->SetWidthOverride(static_cast<float>(HostSize.X));
    Host->SetHeightOverride(static_cast<float>(HostSize.Y));
}

void ResetRuntimeWidgetState(FInventoryPanelGridContext& Ctx)
{
    *Ctx.CachedGridWidth = 0;
    *Ctx.CachedGridHeight = 0;
    *Ctx.CachedGridWidthSecondary = 0;
    *Ctx.CachedGridHeightSecondary = 0;

    *Ctx.GridPanel = nullptr;
    *Ctx.GridPanelSecondary = nullptr;
    *Ctx.LeftHandGridPanel = nullptr;
    *Ctx.RightHandGridPanel = nullptr;
    *Ctx.bHandGridsBuilt = false;

    Ctx.ContainerTabCells->Reset();
    Ctx.SecondaryContainerTabCells->Reset();
    Ctx.CellPrimaryWidgets->Reset();
    Ctx.CellQuantityWidgets->Reset();
    Ctx.CellQuantityBadges->Reset();
    Ctx.CellBorders->Reset();
    Ctx.SecondaryCellPrimaryWidgets->Reset();
    Ctx.SecondaryCellQuantityWidgets->Reset();
    Ctx.SecondaryCellQuantityBadges->Reset();
    Ctx.SecondaryCellBorders->Reset();
    Ctx.EquipSlotCells->Reset();
    Ctx.EquipSlotDropTargets->Reset();
    Ctx.LeftHandCellPrimaryWidgets->Reset();
    Ctx.LeftHandCellQuantityWidgets->Reset();
    Ctx.LeftHandCellQuantityBadges->Reset();
    Ctx.RightHandCellPrimaryWidgets->Reset();
    Ctx.RightHandCellQuantityWidgets->Reset();
    Ctx.RightHandCellQuantityBadges->Reset();
    Ctx.LeftHandCells->Reset();
    Ctx.RightHandCells->Reset();
    Ctx.PocketGridRuntime->Reset();

    if (*Ctx.GridHost) { (*Ctx.GridHost)->SetContent(nullptr); }
    if (*Ctx.GridHostSecondary) { (*Ctx.GridHostSecondary)->SetContent(nullptr); }
    if (*Ctx.LeftHandGridHost) { (*Ctx.LeftHandGridHost)->SetContent(nullptr); }
    if (*Ctx.RightHandGridHost) { (*Ctx.RightHandGridHost)->SetContent(nullptr); }
    if (*Ctx.EquipSlotsHost) { (*Ctx.EquipSlotsHost)->ClearChildren(); }
    if (*Ctx.PocketGridsHost) { (*Ctx.PocketGridsHost)->ClearChildren(); }
    if (*Ctx.ContainerTabs) { (*Ctx.ContainerTabs)->ClearChildren(); }
    if (*Ctx.ContainerTabsSecondary) { (*Ctx.ContainerTabsSecondary)->ClearChildren(); }
}

void RebuildGrids(FInventoryPanelGridContext& Ctx)
{
    if (!Ctx.InventoryVM)
    {
        UE_LOG(LogInventoryPanel, Warning, TEXT("RebuildGrids: No InventoryVM"));
        return;
    }

    UInventoryViewModel* InventoryVM = Ctx.InventoryVM;
    FInventoryPanelGridBuilder& GridBuilder = *Ctx.GridBuilder;
    UBorder* GridHost = *Ctx.GridHost;
    UBorder* GridHostSecondary = *Ctx.GridHostSecondary;
    UUniformGridPanel* GridPanel = *Ctx.GridPanel;
    UUniformGridPanel* GridPanelSecondary = *Ctx.GridPanelSecondary;

    const int32 NewW = InventoryVM->GetGridWidth();
    const int32 NewH = InventoryVM->GetGridHeight();
    const bool bHasPrimary = (NewW > 0 && NewH > 0);
    const bool bPrimaryHostMissingContent = GridHost && GridHost->GetContent() != GridPanel;
    const bool bPrimaryCellMismatch = Ctx.CellPrimaryWidgets->Num() != (NewW * NewH) || Ctx.CellBorders->Num() != (NewW * NewH);

    if (bHasPrimary && (NewW != *Ctx.CachedGridWidth || NewH != *Ctx.CachedGridHeight || !GridPanel || bPrimaryHostMissingContent || bPrimaryCellMismatch))
    {
        UE_LOG(LogInventoryPanel, Log, TEXT("RebuildGrids: %dx%d -> %dx%d"), *Ctx.CachedGridWidth, *Ctx.CachedGridHeight, NewW, NewH);
        *Ctx.CachedGridWidth = NewW;
        *Ctx.CachedGridHeight = NewH;
        GridPanel = GridBuilder.BuildGrid(
            NewW,
            NewH,
            InventoryVM->GetSelectedContainerId(),
            *Ctx.CellPrimaryWidgets,
            *Ctx.CellQuantityWidgets,
            *Ctx.CellQuantityBadges,
            *Ctx.CellBorders,
            false);
        *Ctx.GridPanel = GridPanel;
        if (GridHost) { GridHost->SetContent(GridPanel); }
        for (UProjectGridCell* Cell : *Ctx.CellBorders)
        {
            if (!Cell) { continue; }
            Cell->SetIsGridCell(true);
        }
    }

    if (!bHasPrimary)
    {
        *Ctx.GridPanel = nullptr;
        GridPanel = nullptr;
        Ctx.CellPrimaryWidgets->Reset();
        Ctx.CellQuantityWidgets->Reset();
        Ctx.CellQuantityBadges->Reset();
        Ctx.CellBorders->Reset();
        if (GridHost)
        {
            GridHost->SetContent(nullptr);
        }
    }
    if (GridHost) { GridHost->SetVisibility(bHasPrimary ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); }
    if (*Ctx.GridSizeBoxPrimary) { (*Ctx.GridSizeBoxPrimary)->SetVisibility(bHasPrimary ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); }
    if (bHasPrimary) { ApplyGridHostSize(*Ctx.GridSizeBoxPrimary, NewW, NewH, Ctx.CachedCellSize); }

    const int32 NewW2 = InventoryVM->GetSecondaryGridWidth();
    const int32 NewH2 = InventoryVM->GetSecondaryGridHeight();
    // When bHasNearbyContainer is set, the secondary grid data describes
    // the nearby/world surface, which lives in UW_NearbyContainerPanel;
    // the main panel does not render or size it here.
    const bool bHasPlayerSecondary =
        NewW2 > 0 && NewH2 > 0 && !InventoryVM->GetbHasNearbyContainer();
    const bool bSecondaryHostMissingContent =
        GridHostSecondary && GridHostSecondary->GetContent() != GridPanelSecondary;
    const bool bSecondaryCellMismatch = Ctx.SecondaryCellPrimaryWidgets->Num() != (NewW2 * NewH2) || Ctx.SecondaryCellBorders->Num() != (NewW2 * NewH2);

    if (bHasPlayerSecondary && (NewW2 != *Ctx.CachedGridWidthSecondary || NewH2 != *Ctx.CachedGridHeightSecondary || !GridPanelSecondary || bSecondaryHostMissingContent || bSecondaryCellMismatch))
    {
        *Ctx.CachedGridWidthSecondary = NewW2;
        *Ctx.CachedGridHeightSecondary = NewH2;
        GridPanelSecondary = GridBuilder.BuildGrid(
            NewW2,
            NewH2,
            InventoryVM->GetSecondaryContainerId(),
            *Ctx.SecondaryCellPrimaryWidgets,
            *Ctx.SecondaryCellQuantityWidgets,
            *Ctx.SecondaryCellQuantityBadges,
            *Ctx.SecondaryCellBorders,
            true);
        *Ctx.GridPanelSecondary = GridPanelSecondary;
        if (GridHostSecondary) { GridHostSecondary->SetContent(GridPanelSecondary); }
        for (UProjectGridCell* Cell : *Ctx.SecondaryCellBorders)
        {
            if (!Cell) { continue; }
            Cell->SetIsGridCell(true);
        }
    }

    if (!bHasPlayerSecondary)
    {
        *Ctx.GridPanelSecondary = nullptr;
        GridPanelSecondary = nullptr;
        Ctx.SecondaryCellPrimaryWidgets->Reset();
        Ctx.SecondaryCellQuantityWidgets->Reset();
        Ctx.SecondaryCellQuantityBadges->Reset();
        Ctx.SecondaryCellBorders->Reset();
        if (GridHostSecondary && GridHostSecondary->GetContent()) { GridHostSecondary->SetContent(nullptr); }
    }

    if (GridHostSecondary) { GridHostSecondary->SetVisibility(bHasPlayerSecondary ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); }
    if (*Ctx.GridSizeBoxSecondary) { (*Ctx.GridSizeBoxSecondary)->SetVisibility(bHasPlayerSecondary ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); }

    if (bHasPlayerSecondary)
    {
        ApplyGridHostSize(*Ctx.GridSizeBoxSecondary, NewW2, NewH2, Ctx.CachedCellSize);
    }

    // Storage area is shown only when at least one player storage container exists.
    const bool bAnyPlayerGrid = bHasPrimary || bHasPlayerSecondary;
    if (*Ctx.GridRow) { (*Ctx.GridRow)->SetVisibility(bAnyPlayerGrid ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); }
    if (*Ctx.ContainerTabs) { (*Ctx.ContainerTabs)->SetVisibility(bAnyPlayerGrid ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); }
    if (*Ctx.ContainerTabsSecondary) { (*Ctx.ContainerTabsSecondary)->SetVisibility(bHasPlayerSecondary ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); }
    if (*Ctx.EmptyStoragePlaceholder) { (*Ctx.EmptyStoragePlaceholder)->SetVisibility(ESlateVisibility::Collapsed); }

    GridBuilder.UpdateGridVisuals(
        InventoryVM->GetCellVisuals(),
        *Ctx.CellPrimaryWidgets,
        *Ctx.CellQuantityWidgets,
        *Ctx.CellQuantityBadges);

    GridBuilder.UpdateGridVisuals(
        InventoryVM->GetSecondaryCellVisuals(),
        *Ctx.SecondaryCellPrimaryWidgets,
        *Ctx.SecondaryCellQuantityWidgets,
        *Ctx.SecondaryCellQuantityBadges);

    // Keep the shared drag host in sync with the current tabbed tags.
    // The parent's forwarder drives this after the grids are rebuilt;
    // see UW_InventoryPanel::RebuildGrids.
}

void RebuildHandGrids(FInventoryPanelGridContext& Ctx)
{
    if (!Ctx.InventoryVM) { return; }

    UW_InventoryPanel* Panel = Ctx.Panel;
    UInventoryViewModel* InventoryVM = Ctx.InventoryVM;
    FInventoryPanelGridBuilder& GridBuilder = *Ctx.GridBuilder;

    // Hand grids are always 2x2 - only build once
    constexpr int32 HandSize = UInventoryViewModel::HandGridSize;
    UUniformGridPanel* LeftHandGridPanel = *Ctx.LeftHandGridPanel;
    UUniformGridPanel* RightHandGridPanel = *Ctx.RightHandGridPanel;
    UBorder* LeftHandGridHost = *Ctx.LeftHandGridHost;
    UBorder* RightHandGridHost = *Ctx.RightHandGridHost;

    const bool bNeedRebuild =
        !*Ctx.bHandGridsBuilt
        || !LeftHandGridPanel
        || !RightHandGridPanel
        || !LeftHandGridHost
        || !RightHandGridHost
        || LeftHandGridHost->GetContent() != LeftHandGridPanel
        || RightHandGridHost->GetContent() != RightHandGridPanel
        || Ctx.LeftHandCellPrimaryWidgets->Num() != UInventoryViewModel::HandCellCount
        || Ctx.RightHandCellPrimaryWidgets->Num() != UInventoryViewModel::HandCellCount;

    // Only build once - but wait until hosts are valid
    if (bNeedRebuild && LeftHandGridHost && RightHandGridHost)
    {
        *Ctx.LeftHandGridPanel = nullptr;
        *Ctx.RightHandGridPanel = nullptr;
        Ctx.LeftHandCellPrimaryWidgets->Reset();
        Ctx.LeftHandCellQuantityWidgets->Reset();
        Ctx.LeftHandCellQuantityBadges->Reset();
        Ctx.RightHandCellPrimaryWidgets->Reset();
        Ctx.RightHandCellQuantityWidgets->Reset();
        Ctx.RightHandCellQuantityBadges->Reset();
        Ctx.LeftHandCells->Reset();
        Ctx.RightHandCells->Reset();
        LeftHandGridHost->SetContent(nullptr);
        RightHandGridHost->SetContent(nullptr);

        LeftHandGridPanel = GridBuilder.BuildGrid(
            HandSize,
            HandSize,
            ProjectTags::Item_Container_LeftHand,
            *Ctx.LeftHandCellPrimaryWidgets,
            *Ctx.LeftHandCellQuantityWidgets,
            *Ctx.LeftHandCellQuantityBadges,
            *Ctx.LeftHandCells,
            false);
        *Ctx.LeftHandGridPanel = LeftHandGridPanel;
        if (LeftHandGridPanel) { LeftHandGridHost->SetContent(LeftHandGridPanel); }

        RightHandGridPanel = GridBuilder.BuildGrid(
            HandSize,
            HandSize,
            ProjectTags::Item_Container_RightHand,
            *Ctx.RightHandCellPrimaryWidgets,
            *Ctx.RightHandCellQuantityWidgets,
            *Ctx.RightHandCellQuantityBadges,
            *Ctx.RightHandCells,
            false);
        *Ctx.RightHandGridPanel = RightHandGridPanel;
        if (RightHandGridPanel) { RightHandGridHost->SetContent(RightHandGridPanel); }

        for (UProjectGridCell* Cell : *Ctx.LeftHandCells)
        {
            if (!Cell) { continue; }
            Cell->SetIsGridCell(true);
            Cell->SetGridMouseDownHandler(
                UProjectGridCell::FOnGridCellMouseDown::CreateUObject(Panel, &UW_InventoryPanel::HandleLeftHandCellMouseDown));
        }
        for (UProjectGridCell* Cell : *Ctx.RightHandCells)
        {
            if (!Cell) { continue; }
            Cell->SetIsGridCell(true);
            Cell->SetGridMouseDownHandler(
                UProjectGridCell::FOnGridCellMouseDown::CreateUObject(Panel, &UW_InventoryPanel::HandleRightHandCellMouseDown));
        }

        *Ctx.bHandGridsBuilt = (LeftHandGridPanel && RightHandGridPanel);
        UE_LOG(LogInventoryPanel, Log, TEXT("RebuildHandGrids: Built %dx%d grids"), HandSize, HandSize);

        // Once grids exist, publish them to the shared drag host subsystem.
        // The parent's forwarder drives this after hand grids are rebuilt;
        // see UW_InventoryPanel::RebuildHandGrids.
    }

    // Hand SizeBoxes stay authoritative in the panel: their pixel size comes
    // from the same formula every other grid uses.
    ApplyGridHostSize(*Ctx.LeftHandGridSizeBox, HandSize, HandSize, Ctx.CachedCellSize);
    ApplyGridHostSize(*Ctx.RightHandGridSizeBox, HandSize, HandSize, Ctx.CachedCellSize);

    // Update cell visuals from ViewModel
    const TArray<FInventoryCellVisualState>& LeftVisuals = InventoryVM->GetLeftHandCellVisuals();
    const TArray<FInventoryCellVisualState>& RightVisuals = InventoryVM->GetRightHandCellVisuals();
    GridBuilder.UpdateGridVisuals(
        LeftVisuals,
        *Ctx.LeftHandCellPrimaryWidgets,
        *Ctx.LeftHandCellQuantityWidgets,
        *Ctx.LeftHandCellQuantityBadges);
    GridBuilder.UpdateGridVisuals(
        RightVisuals,
        *Ctx.RightHandCellPrimaryWidgets,
        *Ctx.RightHandCellQuantityWidgets,
        *Ctx.RightHandCellQuantityBadges);

    UE_LOG(LogInventoryPanel, Verbose, TEXT("RebuildHandGrids: LHand=%d visuals (%d widgets), RHand=%d visuals (%d widgets)"),
        LeftVisuals.Num(), Ctx.LeftHandCellPrimaryWidgets->Num(), RightVisuals.Num(), Ctx.RightHandCellPrimaryWidgets->Num());
    // Log hex for PUA codepoints - raw PUA chars in Output Log trigger Slate glyph warnings
    for (int32 i = 0; i < LeftVisuals.Num(); ++i)
    {
        if (!LeftVisuals[i].PrimaryText.IsEmpty())
        {
            const FString Str = LeftVisuals[i].PrimaryText.ToString();
            const FString Safe = (!Str.IsEmpty() && Str[0] >= 0xE000)
                ? FString::Printf(TEXT("U+%04X"), static_cast<uint32>(Str[0])) : Str;
            UE_LOG(LogInventoryPanel, Verbose, TEXT("  LHand[%d]: %s"), i, *Safe);
        }
    }
    for (int32 i = 0; i < RightVisuals.Num(); ++i)
    {
        if (!RightVisuals[i].PrimaryText.IsEmpty())
        {
            const FString Str = RightVisuals[i].PrimaryText.ToString();
            const FString Safe = (!Str.IsEmpty() && Str[0] >= 0xE000)
                ? FString::Printf(TEXT("U+%04X"), static_cast<uint32>(Str[0])) : Str;
            UE_LOG(LogInventoryPanel, Verbose, TEXT("  RHand[%d]: %s"), i, *Safe);
        }
    }
}

void RebuildPocketGrids(FInventoryPanelGridContext& Ctx)
{
    UHorizontalBox* PocketGridsHost = *Ctx.PocketGridsHost;
    if (!PocketGridsHost || !Ctx.InventoryVM)
    {
        return;
    }

    UW_InventoryPanel* Panel = Ctx.Panel;
    UInventoryViewModel* InventoryVM = Ctx.InventoryVM;
    FInventoryPanelGridBuilder& GridBuilder = *Ctx.GridBuilder;
    UProjectUIThemeData* CurrentTheme = Ctx.CurrentTheme;
    UWidgetTree* WidgetTree = Ctx.WidgetTree;

    PocketGridsHost->ClearChildren();
    Ctx.PocketGridRuntime->Reset();

    const int32 PocketCount = InventoryVM->GetPocketContainerCount();
    if (PocketCount <= 0)
    {
        PocketGridsHost->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    PocketGridsHost->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    for (int32 PocketIndex = 0; PocketIndex < PocketCount; ++PocketIndex)
    {
        const int32 PocketGridW = InventoryVM->GetPocketGridWidth(PocketIndex);
        const int32 PocketGridH = InventoryVM->GetPocketGridHeight(PocketIndex);
        if (PocketGridW <= 0 || PocketGridH <= 0)
        {
            continue;
        }

        UVerticalBox* PocketColumn = WidgetTree
            ? WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass())
            : NewObject<UVerticalBox>(Panel);

        UTextBlock* PocketLabel = WidgetTree
            ? WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass())
            : NewObject<UTextBlock>(Panel);
        PocketLabel->SetText(InventoryVM->GetPocketContainerLabel(PocketIndex));
        PocketLabel->SetFont(UProjectWidgetLayoutLoader::ResolveThemeFont(TEXT("BodySmall"), CurrentTheme));
        PocketLabel->SetColorAndOpacity(FSlateColor(CurrentTheme ? CurrentTheme->Colors.TextSecondary : FLinearColor::White));
        PocketLabel->SetJustification(ETextJustify::Center);
        PocketColumn->AddChildToVerticalBox(PocketLabel);

        FPocketGridRuntime Runtime;
        Runtime.ViewModelPocketIndex = PocketIndex;
        Runtime.ContainerId = InventoryVM->GetPocketContainerId(PocketIndex);
        Runtime.GridWidth = PocketGridW;
        Runtime.GridHeight = PocketGridH;
        Runtime.GridPanel = GridBuilder.BuildGrid(
            PocketGridW,
            PocketGridH,
            Runtime.ContainerId,
            Runtime.PrimaryWidgets,
            Runtime.QuantityWidgets,
            Runtime.QuantityBadges,
            Runtime.CellBorders,
            false);

        if (Runtime.GridPanel)
        {
            for (int32 CellIndex = 0; CellIndex < Runtime.CellBorders.Num(); ++CellIndex)
            {
                UProjectGridCell* Cell = Runtime.CellBorders[CellIndex];
                if (!Cell)
                {
                    continue;
                }

                Cell->SetIsGridCell(true);
                Cell->SetSecondaryGrid(false);
                Cell->SetCellIndex(EncodePocketCellIndex(Runtime.ViewModelPocketIndex, CellIndex));
                Cell->SetIsEnabled(InventoryVM->IsPocketCellEnabled(PocketIndex, CellIndex));
                Cell->SetGridMouseDownHandler(
                    UProjectGridCell::FOnGridCellMouseDown::CreateUObject(Panel, &UW_InventoryPanel::HandlePocketCellMouseDown));
            }

            GridBuilder.UpdateGridVisuals(
                InventoryVM->GetPocketCellVisuals(PocketIndex),
                Runtime.PrimaryWidgets,
                Runtime.QuantityWidgets,
                Runtime.QuantityBadges);
            PocketColumn->AddChildToVerticalBox(Runtime.GridPanel);
        }

        UHorizontalBoxSlot* PocketSlot = PocketGridsHost->AddChildToHorizontalBox(PocketColumn);
        PocketSlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
        PocketSlot->SetHorizontalAlignment(HAlign_Left);
        PocketSlot->SetVerticalAlignment(VAlign_Top);

        Ctx.PocketGridRuntime->Add(MoveTemp(Runtime));
    }

    // The parent's forwarder republishes pocket surfaces to the shared
    // drag host after this function returns; see
    // UW_InventoryPanel::RebuildPocketGrids.
}

void RebuildTabs(FInventoryPanelGridContext& Ctx)
{
    if (!Ctx.InventoryVM) { return; }

    UW_InventoryPanel* Panel = Ctx.Panel;
    UInventoryViewModel* InventoryVM = Ctx.InventoryVM;
    FInventoryPanelGridBuilder& GridBuilder = *Ctx.GridBuilder;

    const TArray<FText>& Labels = InventoryVM->GetContainerLabels();
    GridBuilder.BuildContainerTabs(*Ctx.ContainerTabs, Labels, *Ctx.ContainerTabCells, false);
    for (UProjectGridCell* Tab : *Ctx.ContainerTabCells)
    {
        if (Tab) { Tab->OnCellClicked.AddUObject(Panel, &UW_InventoryPanel::HandleContainerTabSelected); }
    }

    const bool bHasNearbyContainer = InventoryVM->GetbHasNearbyContainer();
    const bool bHas2 = (InventoryVM->GetSecondaryGridWidth() > 0) && !bHasNearbyContainer;
    if (bHas2)
    {
        GridBuilder.BuildContainerTabs(*Ctx.ContainerTabsSecondary, Labels, *Ctx.SecondaryContainerTabCells, true);
        for (UProjectGridCell* Tab : *Ctx.SecondaryContainerTabCells)
        {
            if (Tab) { Tab->OnCellClicked.AddUObject(Panel, &UW_InventoryPanel::HandleSecondaryContainerTabSelected); }
        }
    }
    else
    {
        Ctx.SecondaryContainerTabCells->Reset();
    }
    if (*Ctx.ContainerTabsSecondary) { (*Ctx.ContainerTabsSecondary)->SetVisibility(bHas2 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); }
}

void RebuildEquipSlots(FInventoryPanelGridContext& Ctx)
{
    UVerticalBox* EquipSlotsHost = *Ctx.EquipSlotsHost;
    if (!Ctx.InventoryVM || !EquipSlotsHost) { return; }

    UW_InventoryPanel* Panel = Ctx.Panel;
    FInventoryPanelGridBuilder& GridBuilder = *Ctx.GridBuilder;

    // Unbind old delegates before rebuild to prevent accumulation
    for (UProjectGridCell* OldCell : *Ctx.EquipSlotCells)
    {
        if (OldCell)
        {
            OldCell->OnCellClicked.RemoveAll(Panel);
            OldCell->OnCellRightClicked.RemoveAll(Panel);
        }
    }

    // Slice 19: old wrappers detach when EquipSlotsHost->ClearChildren()
    // runs inside the builder. Reset the mirrored array here so the
    // UPROPERTY reflection-root tracks the new wrappers only.
    Ctx.EquipSlotDropTargets->Reset();

    GridBuilder.BuildEquipSlots(EquipSlotsHost, Ctx.InventoryVM, *Ctx.EquipSlotCells, *Ctx.EquipSlotDropTargets);
    for (UProjectGridCell* SlotCell : *Ctx.EquipSlotCells)
    {
        if (SlotCell)
        {
            SlotCell->OnCellClicked.AddUObject(Panel, &UW_InventoryPanel::HandleEquipSlotClicked);
            SlotCell->OnCellRightClicked.AddUObject(Panel, &UW_InventoryPanel::HandleEquipSlotRightClicked);
        }
    }
}

bool TryResolveWidgetTopCenterViewportPos(const UWidget* Widget, const UWidget* AbsoluteContext, FVector2D& OutViewportPos)
{
    OutViewportPos = FVector2D::ZeroVector;
    if (!Widget)
    {
        return false;
    }

    const FGeometry Geometry = Widget->GetCachedGeometry();
    const FVector2D LocalSize = Geometry.GetLocalSize();
    if (LocalSize.IsNearlyZero())
    {
        return false;
    }

    const FVector2D AbsoluteTopCenter = Geometry.LocalToAbsolute(FVector2D(LocalSize.X * 0.5f, 0.0f));
    FVector2D PixelPosition;
    USlateBlueprintLibrary::AbsoluteToViewport(AbsoluteContext, AbsoluteTopCenter, PixelPosition, OutViewportPos);
    return true;
}

bool TryResolveTooltipAnchorViewportPos(const UW_InventoryPanel& Panel, const UWidget* AbsoluteContext, const FInventoryEntryView& Entry, FVector2D& OutViewportPos)
{
    OutViewportPos = FVector2D::ZeroVector;
    const UInventoryViewModel* InventoryVM = Panel.InventoryVM;
    if (!InventoryVM || !Entry.ContainerId.IsValid() || Entry.GridPos.X < 0 || Entry.GridPos.Y < 0)
    {
        return false;
    }

    auto TryGridAnchor = [&Entry, &OutViewportPos, AbsoluteContext](
        const FGameplayTag& ContainerId,
        int32 GridWidth,
        const TArray<TObjectPtr<UProjectGridCell>>& Cells) -> bool
    {
        if (!ContainerId.IsValid() || Entry.ContainerId != ContainerId || GridWidth <= 0)
        {
            return false;
        }

        const int32 CellIndex = Entry.GridPos.Y * GridWidth + Entry.GridPos.X;
        if (!Cells.IsValidIndex(CellIndex))
        {
            return false;
        }

        return TryResolveWidgetTopCenterViewportPos(Cells[CellIndex], AbsoluteContext, OutViewportPos);
    };

    if (TryGridAnchor(InventoryVM->GetSelectedContainerId(), Panel.CachedGridWidth, Panel.CellBorders)
        || TryGridAnchor(InventoryVM->GetSecondaryContainerId(), Panel.CachedGridWidthSecondary, Panel.SecondaryCellBorders)
        || TryGridAnchor(ProjectTags::Item_Container_LeftHand, UInventoryViewModel::HandGridSize, Panel.LeftHandCells)
        || TryGridAnchor(ProjectTags::Item_Container_RightHand, UInventoryViewModel::HandGridSize, Panel.RightHandCells))
    {
        return true;
    }

    for (const FPocketGridRuntime& PocketRuntime : Panel.PocketGridRuntime)
    {
        if (TryGridAnchor(PocketRuntime.ContainerId, PocketRuntime.GridWidth, PocketRuntime.CellBorders))
        {
            return true;
        }
    }

    return false;
}

FIntPoint ResolveDragItemFootprint(const UInventoryViewModel* InventoryVM, const UInventoryDragDropOperation* DragOp)
{
    if (!DragOp)
    {
        return FIntPoint(1, 1);
    }

    FIntPoint Footprint = DragOp->ItemSize;

    if (InventoryVM)
    {
        FInventoryEntryView Entry;
        if (InventoryVM->TryGetEntryByInstanceId(DragOp->InstanceId, Entry))
        {
            Footprint = DragOp->bRotated
                ? FIntPoint(Entry.GridSize.Y, Entry.GridSize.X)
                : Entry.GridSize;
        }
    }

    if (Footprint.X <= 0 || Footprint.Y <= 0)
    {
        return FIntPoint(1, 1);
    }

    return Footprint;
}

} // namespace InventoryPanelGridLayout
