// Copyright ALIS. All Rights Reserved.

#include "Widgets/W_InventoryPanel.h"
#include "Widgets/W_ItemContextMenu.h"
#include "Widgets/W_ItemTooltip.h"
#include "MVVM/InventoryViewModel.h"
#include "Layout/ProjectWidgetLayoutLoader.h"
#include "Theme/ProjectUIThemeData.h"
#include "Subsystems/ProjectToastSubsystem.h"
#include "Presentation/ProjectUIWidgetBinder.h"
#include "Widgets/ProjectGridCell.h"
#include "Widgets/InventoryDragDropOperation.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY(LogInventoryPanel);

namespace
{
uint32 BuildTooltipContentHash(const FInventoryEntryView& Entry)
{
    uint32 Hash = GetTypeHash(Entry.InstanceId);
    Hash = HashCombine(Hash, GetTypeHash(Entry.Quantity));
    Hash = HashCombine(Hash, GetTypeHash(Entry.Weight));
    Hash = HashCombine(Hash, GetTypeHash(Entry.Volume));
    Hash = HashCombine(Hash, GetTypeHash(Entry.Durability));
    Hash = HashCombine(Hash, GetTypeHash(Entry.MaxDurability));
    Hash = HashCombine(Hash, GetTypeHash(Entry.Ammo));
    Hash = HashCombine(Hash, GetTypeHash(Entry.DisplayName.ToString()));
    Hash = HashCombine(Hash, GetTypeHash(Entry.Description.ToString()));

    for (const FGameplayTag& Modifier : Entry.Modifiers)
    {
        Hash = HashCombine(Hash, GetTypeHash(Modifier));
    }

    return Hash;
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
} // namespace

// ============================================================================
// Lifecycle
// ============================================================================

UW_InventoryPanel::UW_InventoryPanel(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    ConfigFilePath = UProjectWidgetLayoutLoader::GetPluginUIConfigPath(TEXT("ProjectInventoryUI"), TEXT("InventoryPanel.json"));
    SetIsFocusable(true);
    DragDropHandler.Initialize(&HitDetector);
}

void UW_InventoryPanel::NativeConstruct()
{
    Super::NativeConstruct();

    // Read cellSize from JSON settings (SOT for grid dimensions)
    if (!ConfigFilePath.IsEmpty())
    {
        FString JsonString;
        if (FFileHelper::LoadFileToString(JsonString, *ConfigFilePath))
        {
            TSharedPtr<FJsonObject> JsonRoot;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
            if (FJsonSerializer::Deserialize(Reader, JsonRoot) && JsonRoot.IsValid())
            {
                const TSharedPtr<FJsonObject>* Settings = nullptr;
                if (JsonRoot->TryGetObjectField(TEXT("settings"), Settings))
                {
                    double CellSizeVal = 0.0;
                    if ((*Settings)->TryGetNumberField(TEXT("cellSize"), CellSizeVal) && CellSizeVal > 0.0)
                    {
                        CachedCellSize = static_cast<float>(CellSizeVal);
                    }
                }
            }
        }
    }
    HitDetector.SetCellSize(CachedCellSize);
    // Explicit rebind avoids setup drift after construct/hot-reload.
    DragDropHandler.Initialize(&HitDetector);

    if (!RootWidget)
    {
        UE_LOG(LogInventoryPanel, Warning, TEXT("NativeConstruct: RootWidget is null"));
        return;
    }

    FProjectUIWidgetBinder Binder(RootWidget, GetClass()->GetName());

    // Core layout containers
    GridHost = Binder.FindRequiredAny<UBorder>({ TEXT("GridHostPrimary"), TEXT("GridHost") });
    GridHostSecondary = Binder.FindOptional<UBorder>(TEXT("GridHostSecondary"));
    ContainerTabs = Binder.FindRequiredAny<UHorizontalBox>({ TEXT("ContainerTabsPrimary"), TEXT("ContainerTabs") });
    ContainerTabsSecondary = Binder.FindOptional<UHorizontalBox>(TEXT("ContainerTabsSecondary"));
    EquipSlotsHost = Binder.FindRequired<UVerticalBox>(TEXT("EquipSlotsHost"));

    // Grid wrapper controls
    GridRow = Binder.FindOptional<UWidget>(TEXT("GridRow"));
    GridSizeBoxPrimary = Binder.FindOptional<UWidget>(TEXT("GridSizeBoxPrimary"));
    GridSizeBoxSecondary = Binder.FindOptional<UWidget>(TEXT("GridSizeBoxSecondary"));
    EmptyStoragePlaceholder = Binder.FindOptional<UWidget>(TEXT("EmptyStoragePlaceholder"));

    // Hand grids
    LeftHandGridHost = Binder.FindRequired<UBorder>(TEXT("LeftHandGridHost"));
    RightHandGridHost = Binder.FindRequired<UBorder>(TEXT("RightHandGridHost"));
    PocketGridsHost = Binder.FindOptional<UHorizontalBox>(TEXT("PocketGridsHost"));

    // Text widgets
    WeightText = Binder.FindOptional<UTextBlock>(TEXT("WeightText"));
    VolumeText = Binder.FindOptional<UTextBlock>(TEXT("VolumeText"));
    ItemCountText = Binder.FindOptional<UTextBlock>(TEXT("ItemCountText"));
    SelectionText = Binder.FindOptional<UTextBlock>(TEXT("SelectionText"));
    SelectionStatsText = Binder.FindOptional<UTextBlock>(TEXT("SelectionStatsText"));
    ItemIcon = Binder.FindOptional<UTextBlock>(TEXT("ItemIcon"));
    ItemDetailsText = Binder.FindOptional<UTextBlock>(TEXT("ItemDetailsText"));
    StatusText = Binder.FindOptional<UTextBlock>(TEXT("StatusText"));
    RotateStateText = Binder.FindOptional<UTextBlock>(TEXT("RotateStateText"));
    QtyValueText = Binder.FindOptional<UTextBlock>(TEXT("QtyValueText"));

    // Buttons
    UButton* QtyDownButton = Binder.FindOptional<UButton>(TEXT("QtyDownButton"));
    UButton* QtyUpButton = Binder.FindOptional<UButton>(TEXT("QtyUpButton"));
    UButton* UseButton = Binder.FindOptional<UButton>(TEXT("UseButton"));
    UButton* DropButton = Binder.FindOptional<UButton>(TEXT("DropButton"));
    UButton* EquipButton = Binder.FindOptional<UButton>(TEXT("EquipButton"));
    UButton* RotateButton = Binder.FindOptional<UButton>(TEXT("RotateButton"));

    // Initialize TextUpdater helper (SOLID)
    FInventoryPanelTextUpdater::FWidgetRefs TextRefs;
    TextRefs.WeightText = WeightText;
    TextRefs.VolumeText = VolumeText;
    TextRefs.ItemCountText = ItemCountText;
    TextRefs.SelectionText = SelectionText;
    TextRefs.SelectionStatsText = SelectionStatsText;
    TextRefs.ItemDetailsText = ItemDetailsText;
    TextRefs.QtyValueText = QtyValueText;
    TextRefs.RotateStateText = RotateStateText;
    TextRefs.ItemIcon = ItemIcon;
    TextRefs.QtyDownButton = QtyDownButton;
    TextRefs.QtyUpButton = QtyUpButton;
    TextRefs.UseButton = UseButton;
    TextRefs.DropButton = DropButton;
    TextRefs.EquipButton = EquipButton;
    TextUpdater.Initialize(TextRefs);

    // Bind buttons
    if (QtyDownButton) { QtyDownButton->OnClicked.AddUniqueDynamic(this, &UW_InventoryPanel::HandleQtyDownClicked); }
    if (QtyUpButton) { QtyUpButton->OnClicked.AddUniqueDynamic(this, &UW_InventoryPanel::HandleQtyUpClicked); }
    if (UseButton) { UseButton->OnClicked.AddUniqueDynamic(this, &UW_InventoryPanel::HandleUseClicked); }
    if (DropButton) { DropButton->OnClicked.AddUniqueDynamic(this, &UW_InventoryPanel::HandleDropClicked); }
    if (EquipButton) { EquipButton->OnClicked.AddUniqueDynamic(this, &UW_InventoryPanel::HandleEquipClicked); }
    if (RotateButton) { RotateButton->OnClicked.AddUniqueDynamic(this, &UW_InventoryPanel::HandleRotateClicked); }

    // Collapse empty text widgets initially
    if (StatusText) { StatusText->SetVisibility(ESlateVisibility::Collapsed); }
    if (SelectionStatsText) { SelectionStatsText->SetVisibility(ESlateVisibility::Collapsed); }
    if (ItemIcon) { ItemIcon->SetVisibility(ESlateVisibility::Collapsed); }
    if (ItemDetailsText)
    {
        ItemDetailsText->SetAutoWrapText(true);
        ItemDetailsText->SetVisibility(ESlateVisibility::Collapsed);
    }

    // Initialize other helpers
    GridBuilder.Initialize(this, WidgetTree);
    GridBuilder.SetCellSize(CachedCellSize);

    // Initialize context menu helper (SOLID pattern)
    RootCanvas = Binder.FindRequired<UCanvasPanel>(TEXT("RootCanvas"));
    Binder.LogMissingRequired(TEXT("UW_InventoryPanel::NativeConstruct"));
    if (RootCanvas)
    {
        ContextMenuPresenter.Initialize(RootCanvas, this, UW_ItemContextMenu::StaticClass(), 100, 99);
        TooltipPresenter.Initialize(RootCanvas, this, UW_ItemTooltip::StaticClass(), 50);

        // Bind click catcher
        if (UButton* ClickCatcher = ContextMenuPresenter.GetClickCatcher())
        {
            ClickCatcher->OnClicked.AddDynamic(this, &UW_InventoryPanel::HandleClickCatcherClicked);
        }

        // Bind context menu actions
        if (UW_ItemContextMenu* Menu = ContextMenuPresenter.GetPopupWidget<UW_ItemContextMenu>())
        {
            Menu->SetViewModel(InventoryVM);
            Menu->OnUseAction.AddDynamic(this, &UW_InventoryPanel::HandleContextMenuUse);
            Menu->OnEquipAction.AddDynamic(this, &UW_InventoryPanel::HandleContextMenuEquip);
            Menu->OnDropAction.AddDynamic(this, &UW_InventoryPanel::HandleContextMenuDrop);
            Menu->OnSplitAction.AddDynamic(this, &UW_InventoryPanel::HandleContextMenuSplit);
            Menu->OnMenuClosed.AddDynamic(this, &UW_InventoryPanel::HandleContextMenuClosed);
        }
        if (UW_ItemTooltip* Tooltip = TooltipPresenter.GetTooltipWidget<UW_ItemTooltip>())
        {
            Tooltip->SetViewModel(InventoryVM);
        }
    }
    else
    {
        UE_LOG(LogInventoryPanel, Warning, TEXT("RootCanvas not found - context menu/tooltip will not be available"));
    }

    UE_LOG(LogInventoryPanel, Log, TEXT("NativeConstruct complete"));
}

void UW_InventoryPanel::NativeDestruct()
{
    if (InventoryVM)
    {
        InventoryVM->OnPropertyChanged.RemoveDynamic(this, &UW_InventoryPanel::HandleViewModelPropertyChanged);
        InventoryVM->OnInventoryError.RemoveAll(this);
    }
    PanelState.Reset();
    Super::NativeDestruct();
}

void UW_InventoryPanel::BindCallbacks() {}

// ============================================================================
// ViewModel Integration
// ============================================================================

void UW_InventoryPanel::SetInventoryViewModel(UInventoryViewModel* InViewModel)
{
    if (InventoryVM == InViewModel) { return; }

    if (InventoryVM)
    {
        InventoryVM->OnPropertyChanged.RemoveDynamic(this, &UW_InventoryPanel::HandleViewModelPropertyChanged);
        InventoryVM->OnInventoryError.RemoveAll(this);
    }

    InventoryVM = InViewModel;

    // Keep presenter-owned ProjectUserWidgets on the same ViewModel contract so
    // debug dump inspection does not report NO_VIEWMODEL for tooltip/context UI.
    if (UW_ItemContextMenu* Menu = ContextMenuPresenter.GetPopupWidget<UW_ItemContextMenu>())
    {
        Menu->SetViewModel(InventoryVM);
    }
    if (UW_ItemTooltip* Tooltip = TooltipPresenter.GetTooltipWidget<UW_ItemTooltip>())
    {
        Tooltip->SetViewModel(InventoryVM);
    }

    if (InventoryVM)
    {
        InventoryVM->OnPropertyChanged.AddUniqueDynamic(this, &UW_InventoryPanel::HandleViewModelPropertyChanged);
        InventoryVM->OnInventoryError.AddUObject(this, &UW_InventoryPanel::HandleInventoryError);
        RefreshFromViewModel();
        SetVisibility(InventoryVM->GetbPanelVisible() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
    else
    {
        SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UW_InventoryPanel::OnViewModelChanged_Implementation(UProjectViewModel* OldViewModel, UProjectViewModel* NewViewModel)
{
    SetInventoryViewModel(Cast<UInventoryViewModel>(NewViewModel));
}

void UW_InventoryPanel::RefreshFromViewModel_Implementation()
{
    if (!InventoryVM) { return; }
    RebuildTabs();
    RebuildGrids();
    RebuildHandGrids();
    RebuildPocketGrids();
    RebuildEquipSlots();
    UpdateAllVisuals();
    RefreshAllText();
}

void UW_InventoryPanel::OnThemeChanged_Implementation(UProjectUIThemeData* NewTheme)
{
    Super::OnThemeChanged_Implementation(NewTheme);
    CurrentTheme = NewTheme;
    VisualState.UpdateColors(NewTheme);
    GridBuilder.SetTheme(NewTheme);
    CachedGridWidth = CachedGridHeight = CachedGridWidthSecondary = CachedGridHeightSecondary = 0;
    RebuildGrids();
    UpdateAllVisuals();
}

void UW_InventoryPanel::HandleViewModelPropertyChanged(FName PropertyName)
{
    if (!InventoryVM) { return; }

    static const FName NAME_bPanelVisible(TEXT("bPanelVisible"));
    static const FName NAME_GridWidth(TEXT("GridWidth"));
    static const FName NAME_GridHeight(TEXT("GridHeight"));
    static const FName NAME_ContainerLabels(TEXT("ContainerLabels"));
    static const FName NAME_PocketContainerLabels(TEXT("PocketContainerLabels"));
    static const FName NAME_CellTexts(TEXT("CellTexts"));
    static const FName NAME_SecondaryCellTexts(TEXT("SecondaryCellTexts"));
    static const FName NAME_LeftHandCellTexts(TEXT("LeftHandCellTexts"));
    static const FName NAME_RightHandCellTexts(TEXT("RightHandCellTexts"));
    static const FName NAME_PocketCellTexts(TEXT("PocketCellTexts"));
    static const FName NAME_EquipSlotLabels(TEXT("EquipSlotLabels"));
    static const FName NAME_EquipSlotItemIconCodes(TEXT("EquipSlotItemIconCodes"));

    UE_LOG(LogInventoryPanel, Verbose, TEXT("HandleViewModelPropertyChanged: %s"), *PropertyName.ToString());

    if (PropertyName == NAME_bPanelVisible)
    {
        const bool bVisible = InventoryVM->GetbPanelVisible();
        UE_LOG(LogInventoryPanel, Log, TEXT("Panel visibility -> %s"), bVisible ? TEXT("Visible") : TEXT("Collapsed"));
        SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        if (bVisible)
        {
            // Grid data properties fire before bPanelVisible, so the panel
            // may not exist yet when they arrive. Rebuild now with latest state.
            RebuildGrids();
            RebuildHandGrids();
            RebuildPocketGrids();
            RebuildEquipSlots();
            SetFocus();
        }
    }
    else if (PropertyName == NAME_GridWidth || PropertyName == NAME_GridHeight)
    {
        RebuildGrids();
    }
    else if (PropertyName == NAME_ContainerLabels)
    {
        RebuildTabs();
    }
    else if (PropertyName == NAME_PocketContainerLabels)
    {
        RebuildPocketGrids();
    }
    else if (PropertyName == NAME_CellTexts)
    {
        UE_LOG(LogInventoryPanel, Verbose, TEXT("CellTexts updated: %d texts -> %d widgets"),
            InventoryVM->GetCellTexts().Num(), CellWidgets.Num());
        GridBuilder.UpdateGridTexts(InventoryVM->GetCellTexts(), CellWidgets);
    }
    else if (PropertyName == NAME_SecondaryCellTexts)
    {
        GridBuilder.UpdateGridTexts(InventoryVM->GetSecondaryCellTexts(), SecondaryCellWidgets);
    }
    else if (PropertyName == NAME_LeftHandCellTexts)
    {
        UE_LOG(LogInventoryPanel, Verbose, TEXT("LeftHandCellTexts updated: %d texts"),
            InventoryVM->GetLeftHandCellTexts().Num());
        GridBuilder.UpdateGridTexts(InventoryVM->GetLeftHandCellTexts(), LeftHandCellWidgets);
    }
    else if (PropertyName == NAME_RightHandCellTexts)
    {
        UE_LOG(LogInventoryPanel, Verbose, TEXT("RightHandCellTexts updated: %d texts"),
            InventoryVM->GetRightHandCellTexts().Num());
        GridBuilder.UpdateGridTexts(InventoryVM->GetRightHandCellTexts(), RightHandCellWidgets);
    }
    else if (PropertyName == NAME_PocketCellTexts)
    {
        RebuildPocketGrids();
    }
    else if (PropertyName == NAME_EquipSlotLabels || PropertyName == NAME_EquipSlotItemIconCodes)
    {
        RebuildEquipSlots();
    }

    RefreshAllText();
    UpdateAllVisuals();
}

void UW_InventoryPanel::HandleInventoryError(const FText& ErrorMessage)
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UProjectToastSubsystem* ToastSub = GI->GetSubsystem<UProjectToastSubsystem>())
        {
            ToastSub->ShowToast(ErrorMessage, 3.0f, FName("Error"));
        }
    }
}

// ============================================================================
// Build Methods
// ============================================================================

void UW_InventoryPanel::RebuildGrids()
{
    if (!InventoryVM)
    {
        UE_LOG(LogInventoryPanel, Warning, TEXT("RebuildGrids: No InventoryVM"));
        return;
    }

    const int32 NewW = InventoryVM->GetGridWidth();
    const int32 NewH = InventoryVM->GetGridHeight();
    const bool bHasPrimary = (NewW > 0 && NewH > 0);

    if (NewW != CachedGridWidth || NewH != CachedGridHeight)
    {
        UE_LOG(LogInventoryPanel, Log, TEXT("RebuildGrids: %dx%d -> %dx%d"), CachedGridWidth, CachedGridHeight, NewW, NewH);
        CachedGridWidth = NewW;
        CachedGridHeight = NewH;
        if (bHasPrimary)
        {
            GridPanel = GridBuilder.BuildGrid(NewW, NewH, CellWidgets, CellBorders, false);
            if (GridHost) { GridHost->SetContent(GridPanel); }
            for (UProjectGridCell* Cell : CellBorders)
            {
                if (!Cell) { continue; }
                Cell->SetIsGridCell(true);
            }
        }
    }

    if (!bHasPrimary)
    {
        GridPanel = nullptr;
        CellWidgets.Reset();
        CellBorders.Reset();
        if (GridHost)
        {
            GridHost->SetContent(nullptr);
        }
    }
    if (GridHost) { GridHost->SetVisibility(bHasPrimary ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); }
    if (GridSizeBoxPrimary) { GridSizeBoxPrimary->SetVisibility(bHasPrimary ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); }

    const int32 NewW2 = InventoryVM->GetSecondaryGridWidth();
    const int32 NewH2 = InventoryVM->GetSecondaryGridHeight();
    const bool bHasSecondary = (NewW2 > 0 && NewH2 > 0);

    if (NewW2 != CachedGridWidthSecondary || NewH2 != CachedGridHeightSecondary)
    {
        CachedGridWidthSecondary = NewW2;
        CachedGridHeightSecondary = NewH2;
        if (bHasSecondary)
        {
            GridPanelSecondary = GridBuilder.BuildGrid(NewW2, NewH2, SecondaryCellWidgets, SecondaryCellBorders, true);
            if (GridHostSecondary) { GridHostSecondary->SetContent(GridPanelSecondary); }
            for (UProjectGridCell* Cell : SecondaryCellBorders)
            {
                if (!Cell) { continue; }
                Cell->SetIsGridCell(true);
            }
        }
    }

    if (!bHasSecondary)
    {
        GridPanelSecondary = nullptr;
        SecondaryCellWidgets.Reset();
        SecondaryCellBorders.Reset();
        if (GridHostSecondary)
        {
            GridHostSecondary->SetContent(nullptr);
        }
    }
    if (GridHostSecondary) { GridHostSecondary->SetVisibility(bHasSecondary ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); }
    if (GridSizeBoxSecondary) { GridSizeBoxSecondary->SetVisibility(bHasSecondary ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); }

    // Storage area is shown only when at least one equipped storage container exists.
    const bool bAnyGrid = bHasPrimary || bHasSecondary;
    if (GridRow) { GridRow->SetVisibility(bAnyGrid ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); }
    if (ContainerTabs) { ContainerTabs->SetVisibility(bAnyGrid ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); }
    if (ContainerTabsSecondary) { ContainerTabsSecondary->SetVisibility(bHasSecondary ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); }
    if (EmptyStoragePlaceholder) { EmptyStoragePlaceholder->SetVisibility(ESlateVisibility::Collapsed); }

    GridBuilder.UpdateGridTexts(InventoryVM->GetCellTexts(), CellWidgets);
    GridBuilder.UpdateGridTexts(InventoryVM->GetSecondaryCellTexts(), SecondaryCellWidgets);
}

void UW_InventoryPanel::RebuildHandGrids()
{
    if (!InventoryVM) { return; }

    // Hand grids are always 2x2 - only build once
    constexpr int32 HandSize = UInventoryViewModel::HandGridSize;

    // Only build once - but wait until hosts are valid
    if (!bHandGridsBuilt && LeftHandGridHost && RightHandGridHost)
    {
        LeftHandGridPanel = GridBuilder.BuildGrid(HandSize, HandSize, LeftHandCellWidgets, LeftHandCells, false);
        if (LeftHandGridPanel) { LeftHandGridHost->SetContent(LeftHandGridPanel); }

        RightHandGridPanel = GridBuilder.BuildGrid(HandSize, HandSize, RightHandCellWidgets, RightHandCells, false);
        if (RightHandGridPanel) { RightHandGridHost->SetContent(RightHandGridPanel); }

        for (UProjectGridCell* Cell : LeftHandCells)
        {
            if (!Cell) { continue; }
            Cell->OnCellClicked.AddUObject(this, &UW_InventoryPanel::HandleLeftHandCellClicked);
            Cell->OnCellRightClicked.AddUObject(this, &UW_InventoryPanel::HandleLeftHandCellContextRequested);
        }
        for (UProjectGridCell* Cell : RightHandCells)
        {
            if (!Cell) { continue; }
            Cell->OnCellClicked.AddUObject(this, &UW_InventoryPanel::HandleRightHandCellClicked);
            Cell->OnCellRightClicked.AddUObject(this, &UW_InventoryPanel::HandleRightHandCellContextRequested);
        }

        bHandGridsBuilt = (LeftHandGridPanel && RightHandGridPanel);
        UE_LOG(LogInventoryPanel, Log, TEXT("RebuildHandGrids: Built %dx%d grids"), HandSize, HandSize);
    }

    // Update cell texts from ViewModel
    const TArray<FText>& LeftTexts = InventoryVM->GetLeftHandCellTexts();
    const TArray<FText>& RightTexts = InventoryVM->GetRightHandCellTexts();
    GridBuilder.UpdateGridTexts(LeftTexts, LeftHandCellWidgets);
    GridBuilder.UpdateGridTexts(RightTexts, RightHandCellWidgets);

    UE_LOG(LogInventoryPanel, Verbose, TEXT("RebuildHandGrids: LHand=%d texts (%d widgets), RHand=%d texts (%d widgets)"),
        LeftTexts.Num(), LeftHandCellWidgets.Num(), RightTexts.Num(), RightHandCellWidgets.Num());
    // Log hex for PUA codepoints - raw PUA chars in Output Log trigger Slate glyph warnings
    for (int32 i = 0; i < LeftTexts.Num(); ++i)
    {
        if (!LeftTexts[i].IsEmpty())
        {
            const FString Str = LeftTexts[i].ToString();
            const FString Safe = (!Str.IsEmpty() && Str[0] >= 0xE000)
                ? FString::Printf(TEXT("U+%04X"), static_cast<uint32>(Str[0])) : Str;
            UE_LOG(LogInventoryPanel, Verbose, TEXT("  LHand[%d]: %s"), i, *Safe);
        }
    }
    for (int32 i = 0; i < RightTexts.Num(); ++i)
    {
        if (!RightTexts[i].IsEmpty())
        {
            const FString Str = RightTexts[i].ToString();
            const FString Safe = (!Str.IsEmpty() && Str[0] >= 0xE000)
                ? FString::Printf(TEXT("U+%04X"), static_cast<uint32>(Str[0])) : Str;
            UE_LOG(LogInventoryPanel, Verbose, TEXT("  RHand[%d]: %s"), i, *Safe);
        }
    }
}

int32 UW_InventoryPanel::EncodePocketCellIndex(int32 PocketIndex, int32 CellIndex)
{
    if (PocketIndex < 0 || PocketIndex > 0x7FFF || CellIndex < 0 || CellIndex > 0xFFFF)
    {
        return INDEX_NONE;
    }

    return (PocketIndex << 16) | (CellIndex & 0xFFFF);
}

bool UW_InventoryPanel::DecodePocketCellIndex(int32 EncodedCellIndex, int32& OutPocketIndex, int32& OutCellIndex)
{
    if (EncodedCellIndex < 0)
    {
        return false;
    }

    OutPocketIndex = (EncodedCellIndex >> 16) & 0x7FFF;
    OutCellIndex = EncodedCellIndex & 0xFFFF;
    return true;
}

void UW_InventoryPanel::RebuildPocketGrids()
{
    if (!PocketGridsHost || !InventoryVM)
    {
        return;
    }

    PocketGridsHost->ClearChildren();
    PocketGridRuntime.Reset();

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
            : NewObject<UVerticalBox>(this);

        UTextBlock* PocketLabel = WidgetTree
            ? WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass())
            : NewObject<UTextBlock>(this);
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
            Runtime.CellWidgets,
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
                    UProjectGridCell::FOnGridCellMouseDown::CreateUObject(this, &UW_InventoryPanel::HandlePocketCellMouseDown));
            }

            GridBuilder.UpdateGridTexts(InventoryVM->GetPocketCellTexts(PocketIndex), Runtime.CellWidgets);
            PocketColumn->AddChildToVerticalBox(Runtime.GridPanel);
        }

        UHorizontalBoxSlot* PocketSlot = PocketGridsHost->AddChildToHorizontalBox(PocketColumn);
        PocketSlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
        PocketSlot->SetHorizontalAlignment(HAlign_Left);
        PocketSlot->SetVerticalAlignment(VAlign_Top);

        PocketGridRuntime.Add(MoveTemp(Runtime));
    }
}

void UW_InventoryPanel::RebuildTabs()
{
    if (!InventoryVM) { return; }

    const TArray<FText>& Labels = InventoryVM->GetContainerLabels();
    GridBuilder.BuildContainerTabs(ContainerTabs, Labels, ContainerTabCells, false);
    for (UProjectGridCell* Tab : ContainerTabCells)
    {
        if (Tab) { Tab->OnCellClicked.AddUObject(this, &UW_InventoryPanel::HandleContainerTabSelected); }
    }

    const bool bHas2 = (InventoryVM->GetSecondaryGridWidth() > 0);
    if (bHas2)
    {
        GridBuilder.BuildContainerTabs(ContainerTabsSecondary, Labels, SecondaryContainerTabCells, true);
        for (UProjectGridCell* Tab : SecondaryContainerTabCells)
        {
            if (Tab) { Tab->OnCellClicked.AddUObject(this, &UW_InventoryPanel::HandleSecondaryContainerTabSelected); }
        }
    }
    if (ContainerTabsSecondary) { ContainerTabsSecondary->SetVisibility(bHas2 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); }
}

void UW_InventoryPanel::RebuildEquipSlots()
{
    if (!InventoryVM || !EquipSlotsHost) { return; }

    // Unbind old delegates before rebuild to prevent accumulation
    for (UProjectGridCell* OldCell : EquipSlotCells)
    {
        if (OldCell)
        {
            OldCell->OnCellClicked.RemoveAll(this);
            OldCell->OnCellRightClicked.RemoveAll(this);
        }
    }

    GridBuilder.BuildEquipSlots(EquipSlotsHost, InventoryVM, EquipSlotCells);
    for (UProjectGridCell* SlotCell : EquipSlotCells)
    {
        if (SlotCell)
        {
            SlotCell->OnCellClicked.AddUObject(this, &UW_InventoryPanel::HandleEquipSlotClicked);
            SlotCell->OnCellRightClicked.AddUObject(this, &UW_InventoryPanel::HandleEquipSlotRightClicked);
        }
    }
}

// ============================================================================
// Update Methods - Delegate to Helpers
// ============================================================================

void UW_InventoryPanel::UpdateAllVisuals()
{
    const FProjectUIGridDragPreviewResult& Preview = DragDropHandler.GetPreviewResult();

    VisualState.ApplyToGrid(CellBorders,
        PanelState.bSelectedSecondary ? INDEX_NONE : PanelState.SelectedCellIndex,
        PanelState.bHoveredSecondary ? INDEX_NONE : PanelState.HoveredCellIndex,
        Preview.bActive && !Preview.bSecondary ? Preview.PrimaryCells : TSet<int32>(),
        Preview.bValid,
        [](UProjectGridCell* Cell, const FLinearColor& Color, bool bEnabled)
        {
            Cell->SetBrushColor(Color);
            Cell->SetIsEnabled(bEnabled);
        },
        [this](int32 Idx) { return InventoryVM ? InventoryVM->IsCellEnabled(Idx) : true; });

    VisualState.ApplyToGrid(SecondaryCellBorders,
        PanelState.bSelectedSecondary ? PanelState.SelectedCellIndexSecondary : INDEX_NONE,
        PanelState.bHoveredSecondary ? PanelState.HoveredCellIndexSecondary : INDEX_NONE,
        Preview.bActive && Preview.bSecondary ? Preview.SecondaryCells : TSet<int32>(),
        Preview.bValid,
        [](UProjectGridCell* Cell, const FLinearColor& Color, bool bEnabled)
        {
            Cell->SetBrushColor(Color);
            Cell->SetIsEnabled(bEnabled);
        },
        [this](int32 Idx) { return InventoryVM ? InventoryVM->IsSecondaryCellEnabled(Idx) : true; });

    if (InventoryVM)
    {
        VisualState.ApplyToTabs(ContainerTabCells, InventoryVM->GetSelectedContainerIndex(),
            [](UProjectGridCell* Cell, const FLinearColor& Color)
            {
                Cell->SetBrushColor(Color);
            });
        VisualState.ApplyToTabs(SecondaryContainerTabCells, InventoryVM->GetSecondaryContainerIndex(),
            [](UProjectGridCell* Cell, const FLinearColor& Color)
            {
                Cell->SetBrushColor(Color);
            });
    }
}

void UW_InventoryPanel::RefreshAllText()
{
    TextUpdater.UpdateStatsText(InventoryVM);
    TextUpdater.UpdateSelectionInfo(InventoryVM, PanelState);
    TextUpdater.UpdateCommandButtons(InventoryVM, PanelState);
    TextUpdater.UpdateQuantityControls(PanelState);
}

// ============================================================================
// Click Handlers
// ============================================================================

void UW_InventoryPanel::HandleLeftHandCellClicked(int32 CellIndex)
{
    if (!InventoryVM) { return; }
    const int32 InstanceId = InventoryVM->GetLeftHandInstanceId(CellIndex);
    PanelState.SetSelectedByInstanceId(
        InstanceId != UInventoryViewModel::EmptyCellInstanceId ? InstanceId : INDEX_NONE);
    RefreshAllText();
    UpdateAllVisuals();
}

void UW_InventoryPanel::HandleRightHandCellClicked(int32 CellIndex)
{
    if (!InventoryVM) { return; }
    const int32 InstanceId = InventoryVM->GetRightHandInstanceId(CellIndex);
    PanelState.SetSelectedByInstanceId(
        InstanceId != UInventoryViewModel::EmptyCellInstanceId ? InstanceId : INDEX_NONE);
    RefreshAllText();
    UpdateAllVisuals();
}

FEventReply UW_InventoryPanel::HandlePocketCellMouseDown(int32 EncodedCellIndex, bool /*bSecondary*/, const FPointerEvent& MouseEvent)
{
    if (!InventoryVM)
    {
        return UWidgetBlueprintLibrary::Handled();
    }

    int32 PocketIndex = INDEX_NONE;
    int32 CellIndex = INDEX_NONE;
    if (!DecodePocketCellIndex(EncodedCellIndex, PocketIndex, CellIndex))
    {
        return UWidgetBlueprintLibrary::Handled();
    }

    if (!InventoryVM->IsPocketCellEnabled(PocketIndex, CellIndex))
    {
        return UWidgetBlueprintLibrary::Handled();
    }

    const FKey Button = MouseEvent.GetEffectingButton();
    if (Button == EKeys::RightMouseButton)
    {
        PanelState.PendingDragCellIndex = INDEX_NONE;
        PendingDragInstanceId = INDEX_NONE;
        HandlePocketCellContextRequested(EncodedCellIndex);
        return UWidgetBlueprintLibrary::Handled();
    }

    if (Button == EKeys::LeftMouseButton)
    {
        PanelState.PendingDragCellIndex = INDEX_NONE;
        HandlePocketCellClicked(EncodedCellIndex);

        const int32 InstanceId = InventoryVM->GetPocketCellInstanceId(PocketIndex, CellIndex);
        PendingDragInstanceId = InstanceId;
        if (InstanceId != UInventoryViewModel::EmptyCellInstanceId)
        {
            FInventoryEntryView Entry;
            if (InventoryVM->TryGetEntryByInstanceId(InstanceId, Entry))
            {
                return UWidgetBlueprintLibrary::DetectDragIfPressed(MouseEvent, this, EKeys::LeftMouseButton);
            }
        }
    }

    return UWidgetBlueprintLibrary::Handled();
}

void UW_InventoryPanel::HandlePocketCellClicked(int32 EncodedCellIndex)
{
    if (!InventoryVM) { return; }

    int32 PocketIndex = INDEX_NONE;
    int32 CellIndex = INDEX_NONE;
    if (!DecodePocketCellIndex(EncodedCellIndex, PocketIndex, CellIndex))
    {
        return;
    }

    const int32 InstanceId = InventoryVM->GetPocketCellInstanceId(PocketIndex, CellIndex);
    PanelState.SetSelectedByInstanceId(
        InstanceId != UInventoryViewModel::EmptyCellInstanceId ? InstanceId : INDEX_NONE);
    RefreshAllText();
    UpdateAllVisuals();
}

void UW_InventoryPanel::HandleLeftHandCellContextRequested(int32 CellIndex)
{
    if (!InventoryVM) { return; }
    const int32 InstanceId = InventoryVM->GetLeftHandInstanceId(CellIndex);
    if (InstanceId == UInventoryViewModel::EmptyCellInstanceId)
    {
        PanelState.SetSelectedByInstanceId(INDEX_NONE);
        RefreshAllText();
        UpdateAllVisuals();
        HideContextMenu();
        return;
    }

    PanelState.SetSelectedByInstanceId(InstanceId);
    RefreshAllText();
    UpdateAllVisuals();

    FInventoryEntryView Entry;
    if (InventoryVM->TryGetEntryByInstanceId(InstanceId, Entry))
    {
        HideTooltip();
        const FVector2D AbsPos = FSlateApplication::Get().GetCursorPos();
        TArray<FProjectUIActionDescriptor> ActionDescriptors;
        InventoryVM->BuildActionDescriptors(Entry, ActionDescriptors);
        if (UInventoryViewModel::HasEnabledActions(ActionDescriptors))
        {
            ShowContextMenuForEntry(Entry, ActionDescriptors, AbsPos);
        }
        else
        {
            HideContextMenu();
        }
    }
    else
    {
        HideContextMenu();
    }
}

void UW_InventoryPanel::HandleRightHandCellContextRequested(int32 CellIndex)
{
    if (!InventoryVM) { return; }
    const int32 InstanceId = InventoryVM->GetRightHandInstanceId(CellIndex);
    if (InstanceId == UInventoryViewModel::EmptyCellInstanceId)
    {
        PanelState.SetSelectedByInstanceId(INDEX_NONE);
        RefreshAllText();
        UpdateAllVisuals();
        HideContextMenu();
        return;
    }

    PanelState.SetSelectedByInstanceId(InstanceId);
    RefreshAllText();
    UpdateAllVisuals();

    FInventoryEntryView Entry;
    if (InventoryVM->TryGetEntryByInstanceId(InstanceId, Entry))
    {
        HideTooltip();
        const FVector2D AbsPos = FSlateApplication::Get().GetCursorPos();
        TArray<FProjectUIActionDescriptor> ActionDescriptors;
        InventoryVM->BuildActionDescriptors(Entry, ActionDescriptors);
        if (UInventoryViewModel::HasEnabledActions(ActionDescriptors))
        {
            ShowContextMenuForEntry(Entry, ActionDescriptors, AbsPos);
        }
        else
        {
            HideContextMenu();
        }
    }
    else
    {
        HideContextMenu();
    }
}

void UW_InventoryPanel::HandlePocketCellContextRequested(int32 EncodedCellIndex)
{
    if (!InventoryVM) { return; }

    int32 PocketIndex = INDEX_NONE;
    int32 CellIndex = INDEX_NONE;
    if (!DecodePocketCellIndex(EncodedCellIndex, PocketIndex, CellIndex))
    {
        return;
    }

    const int32 InstanceId = InventoryVM->GetPocketCellInstanceId(PocketIndex, CellIndex);
    if (InstanceId == UInventoryViewModel::EmptyCellInstanceId)
    {
        PanelState.SetSelectedByInstanceId(INDEX_NONE);
        RefreshAllText();
        UpdateAllVisuals();
        HideContextMenu();
        return;
    }

    PanelState.SetSelectedByInstanceId(InstanceId);
    RefreshAllText();
    UpdateAllVisuals();

    FInventoryEntryView Entry;
    if (InventoryVM->TryGetEntryByInstanceId(InstanceId, Entry))
    {
        HideTooltip();
        const FVector2D AbsPos = FSlateApplication::Get().GetCursorPos();
        TArray<FProjectUIActionDescriptor> ActionDescriptors;
        InventoryVM->BuildActionDescriptors(Entry, ActionDescriptors);
        if (UInventoryViewModel::HasEnabledActions(ActionDescriptors))
        {
            ShowContextMenuForEntry(Entry, ActionDescriptors, AbsPos);
        }
        else
        {
            HideContextMenu();
        }
    }
    else
    {
        HideContextMenu();
    }
}

void UW_InventoryPanel::HandleEquipSlotClicked(int32 SlotIndex)
{
    if (!InventoryVM) { return; }
    // Left-click on equip slot = select item and show status (same as hand cells)
    const int32 InstanceId = InventoryVM->GetEquipSlotInstanceId(SlotIndex);
    if (InstanceId != 0)
    {
        PanelState.SetSelectedByInstanceId(InstanceId);
    }
    RefreshAllText();
    UpdateAllVisuals();
}

void UW_InventoryPanel::HandleEquipSlotRightClicked(int32 SlotIndex)
{
    if (!InventoryVM) { return; }
    const int32 InstanceId = InventoryVM->GetEquipSlotInstanceId(SlotIndex);
    if (InstanceId == 0) { return; }

    FInventoryEntryView Entry;
    if (!InventoryVM->TryGetEntryByInstanceId(InstanceId, Entry)) { return; }

    TArray<FProjectUIActionDescriptor> ActionDescriptors;
    InventoryVM->BuildActionDescriptors(Entry, ActionDescriptors);
    if (UInventoryViewModel::HasEnabledActions(ActionDescriptors))
    {
        HideTooltip();
        const FVector2D AbsPos = FSlateApplication::Get().GetCursorPos();
        ShowContextMenuForEntry(Entry, ActionDescriptors, AbsPos);
    }
}

void UW_InventoryPanel::HandleContainerTabSelected(int32 TabIndex)
{
    if (InventoryVM) { InventoryVM->SetSelectedContainerIndex(TabIndex); }
}

void UW_InventoryPanel::HandleSecondaryContainerTabSelected(int32 TabIndex)
{
    if (InventoryVM) { InventoryVM->SetSecondaryContainerIndex(TabIndex); }
}

void UW_InventoryPanel::HandleUseClicked()
{
    FInventoryEntryView Entry;
    if (InventoryVM && PanelState.TryGetSelectedEntry(InventoryVM, Entry))
    {
        TArray<FProjectUIActionDescriptor> ActionDescriptors;
        InventoryVM->BuildActionDescriptors(Entry, ActionDescriptors);
        if (UInventoryViewModel::IsActionEnabled(ActionDescriptors, UInventoryViewModel::GetActionIdUse()))
        {
            InventoryVM->RequestUseItem(Entry.InstanceId);
        }
    }
}

void UW_InventoryPanel::HandleDropClicked()
{
    FInventoryEntryView Entry;
    if (InventoryVM && PanelState.TryGetSelectedEntry(InventoryVM, Entry))
    {
        TArray<FProjectUIActionDescriptor> ActionDescriptors;
        InventoryVM->BuildActionDescriptors(Entry, ActionDescriptors);
        if (UInventoryViewModel::IsActionEnabled(ActionDescriptors, UInventoryViewModel::GetActionIdDrop()))
        {
            InventoryVM->RequestDropItem(Entry.InstanceId, FMath::Max(1, PanelState.SelectedQuantity));
        }
    }
}

void UW_InventoryPanel::HandleEquipClicked()
{
    FInventoryEntryView Entry;
    if (InventoryVM && PanelState.TryGetSelectedEntry(InventoryVM, Entry))
    {
        TArray<FProjectUIActionDescriptor> ActionDescriptors;
        InventoryVM->BuildActionDescriptors(Entry, ActionDescriptors);
        if (UInventoryViewModel::IsActionEnabled(ActionDescriptors, UInventoryViewModel::GetActionIdEquip()))
        {
            InventoryVM->RequestEquipItem(Entry.InstanceId, Entry.EquipSlotTag);
        }
    }
}

void UW_InventoryPanel::HandleQtyDownClicked()
{
    if (PanelState.SelectedQuantity > 1)
    {
        --PanelState.SelectedQuantity;
        TextUpdater.UpdateQuantityControls(PanelState);
    }
}

void UW_InventoryPanel::HandleQtyUpClicked()
{
    if (PanelState.SelectedMaxQuantity > 0 && PanelState.SelectedQuantity < PanelState.SelectedMaxQuantity)
    {
        ++PanelState.SelectedQuantity;
        TextUpdater.UpdateQuantityControls(PanelState);
    }
}

void UW_InventoryPanel::HandleRotateClicked()
{
    PanelState.bRotateNextDrop = !PanelState.bRotateNextDrop;
    TextUpdater.UpdateRotateState(PanelState.bRotateNextDrop);
}

// Context menu adapters (popup lifecycle in ProjectUI presenter, commands in ViewModel)
void UW_InventoryPanel::ShowContextMenuForCell(int32 CellIndex, bool bSecondary, const FVector2D& ScreenPos)
{
    if (!InventoryVM) { return; }
    // Hide tooltip when context menu opens - they share screen space
    HideTooltip();
    FInventoryEntryView Entry;
    const bool bFound = bSecondary
        ? InventoryVM->TryGetSecondaryEntryByCellIndex(CellIndex, Entry)
        : InventoryVM->TryGetEntryByCellIndex(CellIndex, Entry);
    if (!bFound)
    {
        HideContextMenu();
        return;
    }

    TArray<FProjectUIActionDescriptor> ActionDescriptors;
    InventoryVM->BuildActionDescriptors(Entry, ActionDescriptors);
    if (UInventoryViewModel::HasEnabledActions(ActionDescriptors))
    {
        ShowContextMenuForEntry(Entry, ActionDescriptors, ScreenPos);
    }
    else
    {
        HideContextMenu();
    }
}

void UW_InventoryPanel::ShowContextMenuForEntry(const FInventoryEntryView& Entry, const TArray<FProjectUIActionDescriptor>& ActionDescriptors, const FVector2D& AbsolutePos)
{
    if (Entry.InstanceId == INDEX_NONE || !UInventoryViewModel::HasEnabledActions(ActionDescriptors))
    {
        HideContextMenu();
        return;
    }

    UW_ItemContextMenu* Menu = ContextMenuPresenter.GetPopupWidget<UW_ItemContextMenu>();
    if (!Menu)
    {
        HideContextMenu();
        return;
    }

    Menu->SetViewModel(InventoryVM);
    HideTooltip();
    ContextMenuPresenter.ShowClickCatcher(true);
    Menu->ShowForItem(Entry, ActionDescriptors, AbsolutePos);
}

void UW_InventoryPanel::HideContextMenu()
{
    if (UW_ItemContextMenu* Menu = ContextMenuPresenter.GetPopupWidget<UW_ItemContextMenu>())
    {
        if (Menu->IsMenuVisible())
        {
            Menu->Hide();
        }
    }
    ContextMenuPresenter.ShowClickCatcher(false);
}

void UW_InventoryPanel::HandleClickCatcherClicked() { HideContextMenu(); }
void UW_InventoryPanel::HandleContextMenuUse(int32 Id)
{
    TArray<FProjectUIActionDescriptor> ActionDescriptors;
    if (InventoryVM
        && InventoryVM->TryGetActionDescriptorsByInstanceId(Id, ActionDescriptors)
        && UInventoryViewModel::IsActionEnabled(ActionDescriptors, UInventoryViewModel::GetActionIdUse()))
    {
        InventoryVM->RequestUseItem(Id);
    }
}

void UW_InventoryPanel::HandleContextMenuEquip(int32 Id)
{
    TArray<FProjectUIActionDescriptor> ActionDescriptors;
    if (InventoryVM
        && InventoryVM->TryGetActionDescriptorsByInstanceId(Id, ActionDescriptors)
        && UInventoryViewModel::IsActionEnabled(ActionDescriptors, UInventoryViewModel::GetActionIdEquip()))
    {
        // Check if item is currently equipped -> unequip, otherwise equip
        FInventoryEntryView Entry;
        if (InventoryVM->TryGetEntryByInstanceId(Id, Entry) && Entry.EquippedSlot.IsValid())
        {
            InventoryVM->RequestUnequipItem(Entry.EquippedSlot);
        }
        else
        {
            InventoryVM->RequestEquipItemAuto(Id);
        }
    }
}

void UW_InventoryPanel::HandleContextMenuDrop(int32 Id)
{
    TArray<FProjectUIActionDescriptor> ActionDescriptors;
    if (InventoryVM
        && InventoryVM->TryGetActionDescriptorsByInstanceId(Id, ActionDescriptors)
        && UInventoryViewModel::IsActionEnabled(ActionDescriptors, UInventoryViewModel::GetActionIdDrop()))
    {
        InventoryVM->RequestDropItem(Id, 1);
    }
}

void UW_InventoryPanel::HandleContextMenuSplit(int32 Id)
{
    TArray<FProjectUIActionDescriptor> ActionDescriptors;
    if (InventoryVM
        && InventoryVM->TryGetActionDescriptorsByInstanceId(Id, ActionDescriptors)
        && UInventoryViewModel::IsActionEnabled(ActionDescriptors, UInventoryViewModel::GetActionIdSplit()))
    {
        InventoryVM->RequestSplitStackHalf(Id);
    }
}

void UW_InventoryPanel::HandleContextMenuClosed() { ContextMenuPresenter.ShowClickCatcher(false); }

void UW_InventoryPanel::UpdateTooltipForHover(const FVector2D& MouseViewportPos)
{
    UW_ItemTooltip* Tooltip = TooltipPresenter.GetTooltipWidget<UW_ItemTooltip>();
    if (!Tooltip || !InventoryVM)
    {
        return;
    }

    Tooltip->SetViewModel(InventoryVM);

    FInventoryEntryView Entry;
    if (!PanelState.TryGetHoveredEntry(InventoryVM, Entry))
    {
        HideTooltip();
        return;
    }

    const uint32 TooltipHash = BuildTooltipContentHash(Entry);
    if (!bHasCachedTooltipContentHash || TooltipHash != CachedTooltipContentHash)
    {
        Tooltip->SetItemData(Entry);
        CachedTooltipContentHash = TooltipHash;
        bHasCachedTooltipContentHash = true;
    }

    Tooltip->SetVisibility(ESlateVisibility::HitTestInvisible);
    TooltipPresenter.PositionNearCursor(MouseViewportPos);
}

void UW_InventoryPanel::HideTooltip()
{
    if (UW_ItemTooltip* Tooltip = TooltipPresenter.GetTooltipWidget<UW_ItemTooltip>())
    {
        Tooltip->Clear();
    }
    TooltipPresenter.Hide();
    CachedTooltipContentHash = 0;
    bHasCachedTooltipContentHash = false;
}

FVector2D UW_InventoryPanel::ScreenToViewportPos(const FVector2D& ScreenPos) const
{
    FVector2D ViewportPos = ScreenPos;
    USlateBlueprintLibrary::ScreenToViewport(this, ScreenPos, ViewportPos);
    return ViewportPos;
}

// ============================================================================
// Input Handlers
// ============================================================================

FReply UW_InventoryPanel::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();

    if (Key == EKeys::R) { HandleRotateClicked(); return FReply::Handled(); }
    if (Key == EKeys::Enter) { HandleUseClicked(); return FReply::Handled(); }

    if (Key == EKeys::Tab)
    {
        const bool bHas2 = (CachedGridWidthSecondary > 0 && CachedGridHeightSecondary > 0);
        if (bHas2)
        {
            if (PanelState.bSelectedSecondary)
                PanelState.SetSelectedPrimary(PanelState.SelectedCellIndex >= 0 ? PanelState.SelectedCellIndex : 0);
            else
                PanelState.SetSelectedSecondary(PanelState.SelectedCellIndexSecondary >= 0 ? PanelState.SelectedCellIndexSecondary : 0);
            RefreshAllText();
            UpdateAllVisuals();
        }
        return FReply::Handled();
    }

    if (Key == EKeys::Up || Key == EKeys::Down || Key == EKeys::Left || Key == EKeys::Right)
    {
        const bool bSec = PanelState.bSelectedSecondary;
        const int32 GridW = bSec ? CachedGridWidthSecondary : CachedGridWidth;
        const int32 GridH = bSec ? CachedGridHeightSecondary : CachedGridHeight;
        if (GridW <= 0 || GridH <= 0) { return FReply::Handled(); }

        int32 Idx = bSec ? PanelState.SelectedCellIndexSecondary : PanelState.SelectedCellIndex;
        if (Idx < 0) { Idx = 0; }
        int32 Col = Idx % GridW, Row = Idx / GridW;

        if (Key == EKeys::Up)    Row = FMath::Max(0, Row - 1);
        if (Key == EKeys::Down)  Row = FMath::Min(GridH - 1, Row + 1);
        if (Key == EKeys::Left)  Col = FMath::Max(0, Col - 1);
        if (Key == EKeys::Right) Col = FMath::Min(GridW - 1, Col + 1);

        const int32 NewIdx = Row * GridW + Col;
        if (bSec) PanelState.SetSelectedSecondary(NewIdx);
        else PanelState.SetSelectedPrimary(NewIdx);

        RefreshAllText();
        UpdateAllVisuals();
        return FReply::Handled();
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UW_InventoryPanel::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    const FVector2D ScreenPos = InMouseEvent.GetScreenSpacePosition();

    FProjectUIGridHitResult Result;
    if (HitDetector.ResolveDualGridHit(GridPanel, CachedGridWidth, CachedGridHeight,
            GridPanelSecondary, CachedGridWidthSecondary, CachedGridHeightSecondary,
            ScreenPos, Result))
    {
        if (Result.bIsSecondary) PanelState.SetHoveredSecondary(Result.CellIndex);
        else PanelState.SetHoveredPrimary(Result.CellIndex);
    }
    else
    {
        PanelState.ClearHover();
    }

    // Show tooltip near cursor when hovering an item cell, hide when hovering empty
    if (!ContextMenuPresenter.IsVisible())
    {
        UpdateTooltipForHover(ScreenToViewportPos(ScreenPos));
    }

    UpdateAllVisuals();
    return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FEventReply UW_InventoryPanel::HandleCellMouseDown(int32 CellIndex, bool bSecondary, const FPointerEvent& MouseEvent)
{
    const bool bEnabled = bSecondary
        ? (InventoryVM && InventoryVM->IsSecondaryCellEnabled(CellIndex))
        : (InventoryVM && InventoryVM->IsCellEnabled(CellIndex));
    if (!bEnabled) { return UWidgetBlueprintLibrary::Handled(); }

    const FKey Button = MouseEvent.GetEffectingButton();
    if (Button == EKeys::RightMouseButton)
    {
        PendingDragInstanceId = INDEX_NONE;
        if (bSecondary) { PanelState.SetSelectedSecondary(CellIndex); }
        else { PanelState.SetSelectedPrimary(CellIndex); }
        RefreshAllText();
        UpdateAllVisuals();

        PanelState.PendingDragCellIndex = INDEX_NONE;
        ShowContextMenuForCell(CellIndex, bSecondary, MouseEvent.GetScreenSpacePosition());
        return UWidgetBlueprintLibrary::Handled();
    }

    if (Button == EKeys::LeftMouseButton)
    {
        if (bSecondary)
        {
            PanelState.SetSelectedSecondary(CellIndex);
        }
        else
        {
            PanelState.SetSelectedPrimary(CellIndex);
        }
        RefreshAllText();
        UpdateAllVisuals();

        PanelState.PendingDragCellIndex = CellIndex;
        PanelState.bPendingDragSecondary = bSecondary;

        FInventoryEntryView Entry;
        if (PanelState.TryGetSelectedEntry(InventoryVM, Entry))
        {
            PendingDragInstanceId = Entry.InstanceId;
            return UWidgetBlueprintLibrary::DetectDragIfPressed(MouseEvent, this, EKeys::LeftMouseButton);
        }
        PendingDragInstanceId = INDEX_NONE;
    }

    return UWidgetBlueprintLibrary::Handled();
}

// ============================================================================
// Drag and Drop
// ============================================================================

void UW_InventoryPanel::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);
    HideTooltip();
    if (!InventoryVM) { return; }

    FInventoryEntryView Entry;
    bool bHas = false;
    if (PendingDragInstanceId != INDEX_NONE)
    {
        bHas = InventoryVM->TryGetEntryByInstanceId(PendingDragInstanceId, Entry);
    }
    else if (PanelState.PendingDragCellIndex != INDEX_NONE)
    {
        bHas = PanelState.bPendingDragSecondary
            ? InventoryVM->TryGetSecondaryEntryByCellIndex(PanelState.PendingDragCellIndex, Entry)
            : InventoryVM->TryGetEntryByCellIndex(PanelState.PendingDragCellIndex, Entry);
    }
    if (!bHas) { return; }

    UInventoryDragDropOperation* DragOp = NewObject<UInventoryDragDropOperation>();
    const bool bRotated = PanelState.bRotateNextDrop ? !Entry.bRotated : Entry.bRotated;
    const int32 DragQuantity = FMath::Clamp(PanelState.SelectedQuantity, 1, Entry.Quantity);

    DragOp->InstanceId = Entry.InstanceId;
    DragOp->FromContainer = Entry.ContainerId;
    DragOp->FromPos = Entry.GridPos;
    DragOp->Quantity = DragQuantity;
    DragOp->bRotated = bRotated;
    DragOp->ItemSize = bRotated ? FIntPoint(Entry.GridSize.Y, Entry.GridSize.X) : Entry.GridSize;
    DragOp->EquipSlotTag = Entry.EquipSlotTag;

    // Create drag visual showing item name and quantity
    UTextBlock* DragVisual = NewObject<UTextBlock>(this);
    const FString DragText = DragQuantity > 1
        ? FString::Printf(TEXT("%s x%d"), *Entry.DisplayName.ToString(), DragQuantity)
        : Entry.DisplayName.ToString();
    DragVisual->SetText(FText::FromString(DragText));
    DragVisual->SetFont(UProjectWidgetLayoutLoader::ResolveThemeFont(TEXT("BodySmall"), CurrentTheme));
    if (CurrentTheme)
    {
        DragVisual->SetColorAndOpacity(FSlateColor(CurrentTheme->Colors.TextPrimary));
    }
    DragOp->DefaultDragVisual = DragVisual;
    DragOp->Pivot = EDragPivot::CenterCenter;

    OutOperation = DragOp;
    PanelState.PendingDragCellIndex = INDEX_NONE;
    PendingDragInstanceId = INDEX_NONE;
}

bool UW_InventoryPanel::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    const UInventoryDragDropOperation* DragOp = Cast<UInventoryDragDropOperation>(InOperation);
    if (!DragOp || !InventoryVM)
    {
        DragDropHandler.ClearPreview();
        return Super::NativeOnDragOver(InGeometry, InDragDropEvent, InOperation);
    }
    FProjectUIGridDragPayload DragPayload;
    DragPayload.InstanceId = DragOp->InstanceId;
    DragPayload.ItemSize = ResolveDragItemFootprint(InventoryVM, DragOp);

    DragDropHandler.UpdatePreview(
        InDragDropEvent.GetScreenSpacePosition(),
        DragPayload,
        GridPanel, InventoryVM->GetGridWidth(), InventoryVM->GetGridHeight(),
        GridPanelSecondary, InventoryVM->GetSecondaryGridWidth(), InventoryVM->GetSecondaryGridHeight(),
        [this](bool bSecondary, int32 CellIndex)
        {
            if (!InventoryVM) { return false; }
            return bSecondary ? InventoryVM->IsSecondaryCellEnabled(CellIndex) : InventoryVM->IsCellEnabled(CellIndex);
        },
        [this](bool bSecondary, int32 CellIndex)
        {
            if (!InventoryVM) { return UInventoryViewModel::EmptyCellInstanceId; }
            return bSecondary ? InventoryVM->GetSecondaryCellInstanceId(CellIndex) : InventoryVM->GetCellInstanceId(CellIndex);
        });
    UpdateAllVisuals();
    return true;
}

void UW_InventoryPanel::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    DragDropHandler.ClearPreview();
    PendingDragInstanceId = INDEX_NONE;
    UpdateAllVisuals();
    Super::NativeOnDragLeave(InDragDropEvent, InOperation);
}

bool UW_InventoryPanel::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    DragDropHandler.ClearPreview();
    PendingDragInstanceId = INDEX_NONE;
    UpdateAllVisuals();

    UInventoryDragDropOperation* DragOp = Cast<UInventoryDragDropOperation>(InOperation);
    if (!DragOp || !InventoryVM) { return false; }
    const FIntPoint DragItemSize = ResolveDragItemFootprint(InventoryVM, DragOp);

    int32 EquipSlotIdx = INDEX_NONE;
    if (HitDetector.TryGetWidgetIndexAtScreenPos(EquipSlotCells, InDragDropEvent.GetScreenSpacePosition(), EquipSlotIdx))
    {
        const FGameplayTag SlotTag = InventoryVM->GetEquipSlotTag(EquipSlotIdx);
        if (SlotTag.IsValid() && DragOp->EquipSlotTag == SlotTag)
        {
            InventoryVM->RequestEquipItem(DragOp->InstanceId, SlotTag);
            return true;
        }
        return false;
    }

    // Pocket targets (compact top-row containers) are validated locally here.
    for (const FPocketGridRuntime& PocketRuntime : PocketGridRuntime)
    {
        const int32 PocketIndex = PocketRuntime.ViewModelPocketIndex;
        if (PocketIndex == INDEX_NONE)
        {
            continue;
        }

        if (!PocketRuntime.GridPanel || PocketRuntime.GridWidth <= 0 || PocketRuntime.GridHeight <= 0)
        {
            continue;
        }

        int32 PocketCellIndex = INDEX_NONE;
        if (!HitDetector.ResolveGridHit(
                PocketRuntime.GridPanel,
                PocketRuntime.GridWidth,
                PocketRuntime.GridHeight,
                InDragDropEvent.GetScreenSpacePosition(),
                PocketCellIndex))
        {
            continue;
        }

        int32 PocketCol = 0;
        int32 PocketRow = 0;
        FProjectUIGridHitDetector::IndexToGridCoords(PocketCellIndex, PocketRuntime.GridWidth, PocketCol, PocketRow);

        if (PocketCol < 0 || PocketRow < 0
            || PocketCol + DragItemSize.X > PocketRuntime.GridWidth
            || PocketRow + DragItemSize.Y > PocketRuntime.GridHeight)
        {
            return false;
        }

        for (int32 OffsetY = 0; OffsetY < DragItemSize.Y; ++OffsetY)
        {
            for (int32 OffsetX = 0; OffsetX < DragItemSize.X; ++OffsetX)
            {
                const int32 TargetCellIndex = (PocketRow + OffsetY) * PocketRuntime.GridWidth + (PocketCol + OffsetX);
                if (!InventoryVM->IsPocketCellEnabled(PocketIndex, TargetCellIndex))
                {
                    return false;
                }

                const int32 OccupantId = InventoryVM->GetPocketCellInstanceId(PocketIndex, TargetCellIndex);
                if (OccupantId != UInventoryViewModel::EmptyCellInstanceId && OccupantId != DragOp->InstanceId)
                {
                    return false;
                }
            }
        }

        const FGameplayTag PocketContainerId = PocketRuntime.ContainerId;
        if (!PocketContainerId.IsValid())
        {
            return false;
        }

        InventoryVM->RequestMoveItem(
            DragOp->InstanceId,
            DragOp->FromContainer,
            DragOp->FromPos,
            PocketContainerId,
            FIntPoint(PocketCol, PocketRow),
            DragOp->Quantity,
            DragOp->bRotated);
        return true;
    }

    int32 Col = INDEX_NONE;
    int32 Row = INDEX_NONE;
    bool bSecondary = false;
    FProjectUIGridDragPayload DragPayload;
    DragPayload.InstanceId = DragOp->InstanceId;
    DragPayload.ItemSize = DragItemSize;

    if (!DragDropHandler.ResolveDropGridTarget(
            InDragDropEvent.GetScreenSpacePosition(),
            DragPayload,
            GridPanel, InventoryVM->GetGridWidth(), InventoryVM->GetGridHeight(),
            GridPanelSecondary, InventoryVM->GetSecondaryGridWidth(), InventoryVM->GetSecondaryGridHeight(),
            [this](bool bSec, int32 CellIndex)
            {
                if (!InventoryVM) { return false; }
                return bSec ? InventoryVM->IsSecondaryCellEnabled(CellIndex) : InventoryVM->IsCellEnabled(CellIndex);
            },
            [this](bool bSec, int32 CellIndex)
            {
                if (!InventoryVM) { return UInventoryViewModel::EmptyCellInstanceId; }
                return bSec ? InventoryVM->GetSecondaryCellInstanceId(CellIndex) : InventoryVM->GetCellInstanceId(CellIndex);
            },
            Col, Row, bSecondary))
    {
        return false;
    }

    const FGameplayTag ContainerId = bSecondary ? InventoryVM->GetSecondaryContainerId() : InventoryVM->GetSelectedContainerId();
    if (!ContainerId.IsValid()) { return false; }

    InventoryVM->RequestMoveItem(DragOp->InstanceId, DragOp->FromContainer, DragOp->FromPos,
        ContainerId, FIntPoint(Col, Row), DragOp->Quantity, DragOp->bRotated);
    return true;
}
