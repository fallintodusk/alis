// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Widgets/W_InventoryPanel.h"
#include "Widgets/InventoryPanelGridLayout.h"
#include "Widgets/InventoryPanelSurfaceRegistry.h"
#include "Widgets/W_ItemContextMenu.h"
#include "Widgets/InventoryDragEntryResolver.h"
#include "Widgets/InventoryDragVisualBuilder.h"
#include "Widgets/W_ItemTooltip.h"
#include "Interaction/IInventorySurfacePolicyProvider.h"
#include "Interaction/InventoryUISurfacePriority.h"
#include "Policies/InventoryUIDropStackPolicy.h"
// InventoryDropRouter.h removed (Slice 17): grid/hand/pocket drops now
// dispatch through UInventoryUIDragHostSubsystem::CompleteDrop, which
// owns the router call internally.
#include "MVVM/InventoryViewModel.h"
#include "Layout/ProjectWidgetLayoutLoader.h"
#include "Theme/ProjectUIThemeData.h"
#include "Subsystems/InventoryUIDragHostSubsystem.h"
#include "Subsystems/ProjectToastSubsystem.h"
#include "Presentation/ProjectUIWidgetBinder.h"
#include "Widgets/ProjectGridCell.h"
#include "Widgets/W_InventoryEquipSlotDropTarget.h"
#include "Widgets/InventoryDragDropOperation.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Misc/FileHelper.h"
#include "ProjectGameplayTags.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TimerManager.h"
#include "Geometry/InventoryGridGeometry.h"

DEFINE_LOG_CATEGORY(LogInventoryPanel);

namespace
{
constexpr float InventoryPanelBaseWidth = 1260.0f;
constexpr float InventoryPanelNearbyWidth = 1500.0f;
constexpr float InventoryPanelViewportMargin = 96.0f;

// BuildTooltipContentHash moved alongside UpdateTooltipForHover into
// Private/Widgets/InventoryPanelInteractionHandlers.cpp (anon ns there).

// Slice 18: the former anonymous-namespace IsPayloadAllowedOnOccupant
// helper (which built a widget-owned closure capturing WeakVM) has been
// removed. Per-surface validation now lives on UInventoryViewModel
// implementing IInventorySurfacePolicyProvider; the drag host subsystem
// wires the provider into FProjectUIGridSurface::OccupantAllowedChecker
// at RegisterSurface time. Widgets supply only data, never closures.

} // namespace

// ============================================================================
// Lifecycle
// ============================================================================

UW_InventoryPanel::UW_InventoryPanel(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    ConfigFilePath = UProjectWidgetLayoutLoader::GetPluginUIConfigPath(TEXT("ProjectInventoryUI"), TEXT("InventoryPanel.json"));
    SetIsFocusable(true);
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
    ResetRuntimeWidgetState();

    if (!RootWidget)
    {
        UE_LOG(LogInventoryPanel, Warning, TEXT("NativeConstruct: RootWidget is null"));
        return;
    }

    FProjectUIWidgetBinder Binder(RootWidget, GetClass()->GetName());

    // Core layout containers
    BackgroundWidthSizer = Binder.FindOptional<USizeBox>(TEXT("Background_AutoSizer"));
    GridHost = Binder.FindRequiredAny<UBorder>({ TEXT("GridHostPrimary"), TEXT("GridHost") });
    GridHostSecondary = Binder.FindOptional<UBorder>(TEXT("GridHostSecondary"));
    ContainerTabs = Binder.FindRequiredAny<UHorizontalBox>({ TEXT("ContainerTabsPrimary"), TEXT("ContainerTabs") });
    ContainerTabsSecondary = Binder.FindOptional<UHorizontalBox>(TEXT("ContainerTabsSecondary"));
    EquipSlotsHost = Binder.FindRequired<UVerticalBox>(TEXT("EquipSlotsHost"));

    // Grid wrapper controls
    GridRow = Binder.FindOptional<UWidget>(TEXT("GridRow"));
    GridSizeBoxPrimary = Binder.FindOptional<USizeBox>(TEXT("GridSizeBoxPrimary"));
    GridSizeBoxSecondary = Binder.FindOptional<USizeBox>(TEXT("GridSizeBoxSecondary"));
    EmptyStoragePlaceholder = Binder.FindOptional<UWidget>(TEXT("EmptyStoragePlaceholder"));

    // Hand grids
    LeftHandGridHost = Binder.FindRequired<UBorder>(TEXT("LeftHandGridHost"));
    RightHandGridHost = Binder.FindRequired<UBorder>(TEXT("RightHandGridHost"));
    LeftHandGridSizeBox = Binder.FindOptional<USizeBox>(TEXT("LeftHandGridBox_SizeBox"));
    RightHandGridSizeBox = Binder.FindOptional<USizeBox>(TEXT("RightHandGridBox_SizeBox"));
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
    GridBuilder.SetCellMouseDownHandler(
        UProjectGridCell::FOnGridCellMouseDown::CreateUObject(this, &UW_InventoryPanel::HandleCellMouseDown));

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

    if (InventoryVM)
    {
        // Live runtime can bind the shared InventoryViewModel before this widget
        // finishes NativeConstruct. Reapply the current VM state now that hosts,
        // grids, and presenter widgets are actually available.
        RefreshFromViewModel();
        SetVisibility(InventoryVM->GetbPanelVisible() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
            {
                if (!InventoryVM)
                {
                    return;
                }

                RefreshFromViewModel();
                SetVisibility(InventoryVM->GetbPanelVisible() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
            }));
        }
    }

    // Slice 17: subscribe to the drag event bus so preview updates and
    // error toasts respond to drag activity driven by per-cell
    // UW_InventoryCellDropTarget wrappers (the widget root no longer
    // overrides NativeOnDragOver).  OnPreviewChanged drives the
    // repaint that previously lived in HandleDragEvent.
    //
    // Lazy-construct the pimpl'd registry here; see the TPimplPtr
    // declaration in W_InventoryPanel.h for rationale.  All Register* /
    // Unregister* paths below assume SurfaceRegistry.IsValid().
    if (!SurfaceRegistry.IsValid())
    {
        SurfaceRegistry = MakePimpl<FInventoryPanelSurfaceRegistry>();
    }
    SurfaceRegistry->Initialize(this);
    SurfaceRegistry->OnPreviewChanged.BindUObject(this, &UW_InventoryPanel::UpdateAllVisuals);
    SurfaceRegistry->BindEventBus();
    if (InventoryVM)
    {
        // Pre-bound ViewModels can rebuild grids earlier in NativeConstruct,
        // before SurfaceRegistry exists. Backfill the current surfaces now so
        // split-panel drag/drop sees hands/pockets/player grids immediately.
        SurfaceRegistry->RegisterPlayerGrids();
        SurfaceRegistry->RegisterHands();
        SurfaceRegistry->RegisterPockets();
    }

    UE_LOG(LogInventoryPanel, Log, TEXT("NativeConstruct complete"));
}

void UW_InventoryPanel::NativeDestruct()
{
    if (SurfaceRegistry.IsValid())
    {
        SurfaceRegistry->UnbindEventBus();
    }
    if (InventoryVM)
    {
        InventoryVM->OnPropertyChanged.RemoveDynamic(this, &UW_InventoryPanel::HandleViewModelPropertyChanged);
        InventoryVM->OnInventoryError.RemoveAll(this);
    }
    if (SurfaceRegistry.IsValid())
    {
        SurfaceRegistry->UnregisterAll();
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

    // Slice 18: the VM implements IInventorySurfacePolicyProvider. Bind
    // it to the shared drag host subsystem so surfaces registered after
    // the VM is known route occupant-allowed queries through the VM
    // instead of widget-captured closures. On VM teardown we unbind.
    if (UInventoryUIDragHostSubsystem* Subsystem = ResolveDragHostSubsystem())
    {
        if (InventoryVM)
        {
            TScriptInterface<IInventorySurfacePolicyProvider> ProviderInterface(InventoryVM);
            Subsystem->SetPolicyProvider(ProviderInterface);
        }
        else
        {
            Subsystem->SetPolicyProvider(TScriptInterface<IInventorySurfacePolicyProvider>());
        }
    }

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
        const bool bWidgetTreeReady = (GridHost != nullptr) || (LeftHandGridHost != nullptr) || (EquipSlotsHost != nullptr);
        if (bWidgetTreeReady)
        {
            ResetRuntimeWidgetState();
            RefreshFromViewModel();
            SetVisibility(InventoryVM->GetbPanelVisible() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        }
    }
    else
    {
        if (GridHost || LeftHandGridHost || EquipSlotsHost)
        {
            SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}

void UW_InventoryPanel::OnViewModelChanged_Implementation(UProjectViewModel* OldViewModel, UProjectViewModel* NewViewModel)
{
    SetInventoryViewModel(Cast<UInventoryViewModel>(NewViewModel));
}

void UW_InventoryPanel::RefreshFromViewModel_Implementation()
{
    if (!InventoryVM) { return; }
    UpdateResponsiveLayout();
    RebuildTabs();
    RebuildGrids();
    RebuildHandGrids();
    RebuildPocketGrids();
    RebuildEquipSlots();
    ReconcilePanelStateWithViewModel();
    UpdateAllVisuals();
    RefreshAllText();
}

void UW_InventoryPanel::OnThemeChanged_Implementation(UProjectUIThemeData* NewTheme)
{
    Super::OnThemeChanged_Implementation(NewTheme);
    CurrentTheme = NewTheme;
    VisualState.UpdateColors(NewTheme);
    GridBuilder.SetTheme(NewTheme);
    ResetRuntimeWidgetState();
    RebuildGrids();
    UpdateAllVisuals();
}

void UW_InventoryPanel::HandleViewModelPropertyChanged(FName PropertyName)
{
    if (!InventoryVM) { return; }

    static const FName NAME_bPanelVisible(TEXT("bPanelVisible"));
    static const FName NAME_GridWidth(TEXT("GridWidth"));
    static const FName NAME_GridHeight(TEXT("GridHeight"));
    static const FName NAME_SecondaryGridWidth(TEXT("SecondaryGridWidth"));
    static const FName NAME_SecondaryGridHeight(TEXT("SecondaryGridHeight"));
    static const FName NAME_ContainerLabels(TEXT("ContainerLabels"));
    static const FName NAME_PocketContainerLabels(TEXT("PocketContainerLabels"));
    static const FName NAME_bHasNearbyContainer(TEXT("bHasNearbyContainer"));
    static const FName NAME_CellVisuals(TEXT("CellVisuals"));
    static const FName NAME_SecondaryCellVisuals(TEXT("SecondaryCellVisuals"));
    static const FName NAME_LeftHandCellVisuals(TEXT("LeftHandCellVisuals"));
    static const FName NAME_RightHandCellVisuals(TEXT("RightHandCellVisuals"));
    static const FName NAME_PocketCellVisuals(TEXT("PocketCellVisuals"));
    static const FName NAME_EquipSlotLabels(TEXT("EquipSlotLabels"));
    static const FName NAME_EquipSlotItemIconCodes(TEXT("EquipSlotItemIconCodes"));

    UE_LOG(LogInventoryPanel, Verbose, TEXT("HandleViewModelPropertyChanged: %s"), *PropertyName.ToString());

    if (PropertyName == NAME_bPanelVisible)
    {
        const bool bVisible = InventoryVM->GetbPanelVisible();
        UE_LOG(LogInventoryPanel, Log, TEXT("Panel visibility -> %s"), bVisible ? TEXT("Visible") : TEXT("Collapsed"));
#if SLICE20_SABOTAGE
        // Slice 20 fitness test 3 sabotage - see Public/Slice20SabotageToggle.h.
        // Flipping the "show" branch away from ESlateVisibility::Visible is
        // the regression the UserWidgetVisibilityContracted fitness test must
        // catch. SelfHitTestInvisible here would make the root pass-through
        // during normal play and silently break input routing.
        SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
#else
        SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
#endif
        if (bVisible)
        {
            if ((InventoryVM->GetGridWidth() > 0 && (!GridPanel || (GridHost && GridHost->GetContent() != GridPanel)))
                // Secondary grid no longer routes to a nearby host here; the
                // nearby surface lives in UW_NearbyContainerPanel.
                || !LeftHandGridPanel
                || !RightHandGridPanel
                || !EquipSlotsHost
                || EquipSlotsHost->GetChildrenCount() == 0)
            {
                ResetRuntimeWidgetState();
            }
            // Grid data properties fire before bPanelVisible, so the panel
            // may not exist yet when they arrive. Rebuild now with latest state.
            RebuildGrids();
            RebuildHandGrids();
            RebuildPocketGrids();
            RebuildEquipSlots();
            SetFocus();
        }
    }
    else if (PropertyName == NAME_GridWidth
        || PropertyName == NAME_GridHeight
        || PropertyName == NAME_SecondaryGridWidth
        || PropertyName == NAME_SecondaryGridHeight
        || PropertyName == NAME_bHasNearbyContainer)
    {
        UpdateResponsiveLayout();
        RebuildGrids();
        RebuildTabs();
    }
    else if (PropertyName == NAME_ContainerLabels)
    {
        RebuildTabs();
    }
    else if (PropertyName == NAME_PocketContainerLabels)
    {
        RebuildPocketGrids();
    }
    else if (PropertyName == NAME_CellVisuals)
    {
        UE_LOG(LogInventoryPanel, Verbose, TEXT("CellVisuals updated: %d visuals -> %d widgets"),
            InventoryVM->GetCellVisuals().Num(), CellPrimaryWidgets.Num());
        GridBuilder.UpdateGridVisuals(
            InventoryVM->GetCellVisuals(),
            CellPrimaryWidgets,
            CellQuantityWidgets,
            CellQuantityBadges);
    }
    else if (PropertyName == NAME_SecondaryCellVisuals)
    {
        const TArray<FInventoryCellVisualState>& Visuals = InventoryVM->GetSecondaryCellVisuals();
        int32 NonEmpty = 0;
        for (const FInventoryCellVisualState& Visual : Visuals) { if (!Visual.PrimaryText.IsEmpty()) { ++NonEmpty; } }
        UE_LOG(LogInventoryPanel, Log,
            TEXT("SecondaryCellVisuals updated: %d visuals (%d non-empty) -> %d widgets"),
            Visuals.Num(), NonEmpty, SecondaryCellPrimaryWidgets.Num());
        GridBuilder.UpdateGridVisuals(
            Visuals,
            SecondaryCellPrimaryWidgets,
            SecondaryCellQuantityWidgets,
            SecondaryCellQuantityBadges);
    }
    else if (PropertyName == NAME_LeftHandCellVisuals)
    {
        UE_LOG(LogInventoryPanel, Verbose, TEXT("LeftHandCellVisuals updated: %d visuals"),
            InventoryVM->GetLeftHandCellVisuals().Num());
        GridBuilder.UpdateGridVisuals(
            InventoryVM->GetLeftHandCellVisuals(),
            LeftHandCellPrimaryWidgets,
            LeftHandCellQuantityWidgets,
            LeftHandCellQuantityBadges);
    }
    else if (PropertyName == NAME_RightHandCellVisuals)
    {
        UE_LOG(LogInventoryPanel, Verbose, TEXT("RightHandCellVisuals updated: %d visuals"),
            InventoryVM->GetRightHandCellVisuals().Num());
        GridBuilder.UpdateGridVisuals(
            InventoryVM->GetRightHandCellVisuals(),
            RightHandCellPrimaryWidgets,
            RightHandCellQuantityWidgets,
            RightHandCellQuantityBadges);
    }
    else if (PropertyName == NAME_PocketCellVisuals)
    {
        RebuildPocketGrids();
    }
    else if (PropertyName == NAME_EquipSlotLabels || PropertyName == NAME_EquipSlotItemIconCodes)
    {
        RebuildEquipSlots();
    }

    ReconcilePanelStateWithViewModel();
    RefreshAllText();
    UpdateAllVisuals();
}

void UW_InventoryPanel::ApplyGridHostSize(USizeBox* Host, int32 GridWidth, int32 GridHeight) const
{
    InventoryPanelGridLayout::ApplyGridHostSize(Host, GridWidth, GridHeight, CachedCellSize);
}

FInventoryPanelGridContext UW_InventoryPanel::BuildGridContext()
{
    FInventoryPanelGridContext Ctx;
    Ctx.Panel = this;
    Ctx.GridBuilder = &GridBuilder;
    Ctx.InventoryVM = InventoryVM;
    Ctx.CurrentTheme = CurrentTheme;
    Ctx.WidgetTree = WidgetTree;
    Ctx.GridHost = &GridHost;
    Ctx.GridHostSecondary = &GridHostSecondary;
    Ctx.ContainerTabs = &ContainerTabs;
    Ctx.ContainerTabsSecondary = &ContainerTabsSecondary;
    Ctx.EquipSlotsHost = &EquipSlotsHost;
    Ctx.GridRow = &GridRow;
    Ctx.GridSizeBoxPrimary = &GridSizeBoxPrimary;
    Ctx.GridSizeBoxSecondary = &GridSizeBoxSecondary;
    Ctx.EmptyStoragePlaceholder = &EmptyStoragePlaceholder;
    Ctx.LeftHandGridHost = &LeftHandGridHost;
    Ctx.RightHandGridHost = &RightHandGridHost;
    Ctx.LeftHandGridSizeBox = &LeftHandGridSizeBox;
    Ctx.RightHandGridSizeBox = &RightHandGridSizeBox;
    Ctx.PocketGridsHost = &PocketGridsHost;
    Ctx.GridPanel = &GridPanel;
    Ctx.GridPanelSecondary = &GridPanelSecondary;
    Ctx.LeftHandGridPanel = &LeftHandGridPanel;
    Ctx.RightHandGridPanel = &RightHandGridPanel;
    Ctx.ContainerTabCells = &ContainerTabCells;
    Ctx.SecondaryContainerTabCells = &SecondaryContainerTabCells;
    Ctx.CellPrimaryWidgets = &CellPrimaryWidgets;
    Ctx.CellQuantityWidgets = &CellQuantityWidgets;
    Ctx.CellQuantityBadges = &CellQuantityBadges;
    Ctx.CellBorders = &CellBorders;
    Ctx.SecondaryCellPrimaryWidgets = &SecondaryCellPrimaryWidgets;
    Ctx.SecondaryCellQuantityWidgets = &SecondaryCellQuantityWidgets;
    Ctx.SecondaryCellQuantityBadges = &SecondaryCellQuantityBadges;
    Ctx.SecondaryCellBorders = &SecondaryCellBorders;
    Ctx.EquipSlotCells = &EquipSlotCells;
    Ctx.EquipSlotDropTargets = &EquipSlotDropTargets;
    Ctx.LeftHandCellPrimaryWidgets = &LeftHandCellPrimaryWidgets;
    Ctx.LeftHandCellQuantityWidgets = &LeftHandCellQuantityWidgets;
    Ctx.LeftHandCellQuantityBadges = &LeftHandCellQuantityBadges;
    Ctx.RightHandCellPrimaryWidgets = &RightHandCellPrimaryWidgets;
    Ctx.RightHandCellQuantityWidgets = &RightHandCellQuantityWidgets;
    Ctx.RightHandCellQuantityBadges = &RightHandCellQuantityBadges;
    Ctx.LeftHandCells = &LeftHandCells;
    Ctx.RightHandCells = &RightHandCells;
    Ctx.PocketGridRuntime = &PocketGridRuntime;
    Ctx.CachedGridWidth = &CachedGridWidth;
    Ctx.CachedGridHeight = &CachedGridHeight;
    Ctx.CachedGridWidthSecondary = &CachedGridWidthSecondary;
    Ctx.CachedGridHeightSecondary = &CachedGridHeightSecondary;
    Ctx.CachedCellSize = CachedCellSize;
    Ctx.bHandGridsBuilt = &bHandGridsBuilt;
    return Ctx;
}

void UW_InventoryPanel::UpdateResponsiveLayout()
{
    if (!BackgroundWidthSizer)
    {
        return;
    }

    // Main panel no longer shifts width based on nearby-container state -
    // nearby loot lives in UW_NearbyContainerPanel, anchored independently.
    // The width we reserve here is just the panel's own content cap.
    float TargetWidth = InventoryPanelBaseWidth;

    const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this);
    if (ViewportSize.X > 0.0f)
    {
        const float MaxUsableWidth = FMath::Max(640.0f, ViewportSize.X - InventoryPanelViewportMargin);
        TargetWidth = FMath::Min(TargetWidth, MaxUsableWidth);
    }

    BackgroundWidthSizer->SetWidthOverride(TargetWidth);
}

void UW_InventoryPanel::ReconcilePanelStateWithViewModel()
{
    if (!InventoryVM)
    {
        return;
    }

    FInventoryEntryView Entry;

    if (PanelState.SelectedInstanceId != INDEX_NONE
        && !InventoryVM->TryGetEntryByInstanceId(PanelState.SelectedInstanceId, Entry))
    {
        PanelState.SetSelectedByInstanceId(INDEX_NONE);
    }
    else if (PanelState.bSelectedSecondary
        && PanelState.SelectedCellIndexSecondary != INDEX_NONE
        && !InventoryVM->TryGetSecondaryEntryByCellIndex(PanelState.SelectedCellIndexSecondary, Entry))
    {
        PanelState.SelectedCellIndexSecondary = INDEX_NONE;
        PanelState.bSelectedSecondary = false;
        PanelState.ResetQuantity();
    }
    else if (!PanelState.bSelectedSecondary
        && PanelState.SelectedCellIndex != INDEX_NONE
        && !InventoryVM->TryGetEntryByCellIndex(PanelState.SelectedCellIndex, Entry))
    {
        PanelState.SelectedCellIndex = INDEX_NONE;
        PanelState.ResetQuantity();
    }

    if (PanelState.bHoveredSecondary)
    {
        if (PanelState.HoveredCellIndexSecondary == INDEX_NONE
            || !InventoryVM->TryGetSecondaryEntryByCellIndex(PanelState.HoveredCellIndexSecondary, Entry))
        {
            PanelState.ClearHover();
            HideTooltip();
        }
    }
    else if (PanelState.HoveredCellIndex != INDEX_NONE
        && !InventoryVM->TryGetEntryByCellIndex(PanelState.HoveredCellIndex, Entry))
    {
        PanelState.ClearHover();
        HideTooltip();
    }

    if (PendingDragInstanceId != INDEX_NONE
        && !InventoryVM->TryGetEntryByInstanceId(PendingDragInstanceId, Entry))
    {
        PendingDragInstanceId = INDEX_NONE;
    }
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

void UW_InventoryPanel::ShowInteractionError(const FText& ErrorMessage)
{
    if (ErrorMessage.IsEmpty())
    {
        return;
    }

    HandleInventoryError(ErrorMessage);
}

// ============================================================================
// Build Methods
// ============================================================================

void UW_InventoryPanel::RebuildGrids()
{
    FInventoryPanelGridContext Ctx = BuildGridContext();
    InventoryPanelGridLayout::RebuildGrids(Ctx);

    // Keep the shared drag host in sync with the current tabbed tags.
    if (SurfaceRegistry.IsValid())
    {
        SurfaceRegistry->RegisterPlayerGrids();
    }
}

void UW_InventoryPanel::RebuildHandGrids()
{
    FInventoryPanelGridContext Ctx = BuildGridContext();
    const UUniformGridPanel* PrevLeft = LeftHandGridPanel;
    const UUniformGridPanel* PrevRight = RightHandGridPanel;

    InventoryPanelGridLayout::RebuildHandGrids(Ctx);

    // Original behavior: RegisterHands runs whenever the helper
    // actually rebuilt the hand panels (panels re-constructed).
    // RegisterHands itself guards on null panels and VM.
    if ((LeftHandGridPanel != PrevLeft || RightHandGridPanel != PrevRight) && SurfaceRegistry.IsValid())
    {
        SurfaceRegistry->RegisterHands();
    }
}

void UW_InventoryPanel::ResetRuntimeWidgetState()
{
    FInventoryPanelGridContext Ctx = BuildGridContext();
    InventoryPanelGridLayout::ResetRuntimeWidgetState(Ctx);
}

int32 UW_InventoryPanel::EncodePocketCellIndex(int32 PocketIndex, int32 CellIndex)
{
    return InventoryPanelGridLayout::EncodePocketCellIndex(PocketIndex, CellIndex);
}

bool UW_InventoryPanel::DecodePocketCellIndex(int32 EncodedCellIndex, int32& OutPocketIndex, int32& OutCellIndex)
{
    return InventoryPanelGridLayout::DecodePocketCellIndex(EncodedCellIndex, OutPocketIndex, OutCellIndex);
}

bool UW_InventoryPanel::ResolveHandDropTargetAtScreenPos(
    const FVector2D& ScreenPos,
    FGameplayTag& OutContainerId,
    FIntPoint& OutGridPos) const
{
    OutContainerId = FGameplayTag();
    OutGridPos = FIntPoint(-1, -1);

    if (!InventoryVM)
    {
        return false;
    }

	int32 HandCellIndex = INDEX_NONE;
	if (LeftHandGridPanel
		&& HitDetector.ResolveGridHit(
			LeftHandGridPanel,
            UInventoryViewModel::HandGridSize,
			UInventoryViewModel::HandGridSize,
			ScreenPos,
			HandCellIndex))
	{
		if (!InventoryVM->ResolveHandDropTarget(true, OutContainerId, OutGridPos))
		{
			return false;
		}

		if (OutContainerId == ProjectTags::Item_Container_LeftHand)
		{
			OutGridPos = FIntPoint(
				HandCellIndex % UInventoryViewModel::HandGridSize,
				HandCellIndex / UInventoryViewModel::HandGridSize);
		}
		return true;
	}

    if (RightHandGridPanel
        && HitDetector.ResolveGridHit(
            RightHandGridPanel,
            UInventoryViewModel::HandGridSize,
			UInventoryViewModel::HandGridSize,
			ScreenPos,
			HandCellIndex))
	{
		if (!InventoryVM->ResolveHandDropTarget(false, OutContainerId, OutGridPos))
		{
			return false;
		}

		if (OutContainerId == ProjectTags::Item_Container_RightHand)
		{
			OutGridPos = FIntPoint(
				HandCellIndex % UInventoryViewModel::HandGridSize,
				HandCellIndex / UInventoryViewModel::HandGridSize);
		}
		return true;
	}

    return false;
}

bool UW_InventoryPanel::IsDropOccupantAllowed(
    const UInventoryDragDropOperation* DragOp,
    bool bSecondary,
    int32 CellIndex,
    int32 OccupantId) const
{
    // Empty cells are always allowed regardless of payload state. This
    // short-circuit MUST run before any DragOp guard - otherwise a single
    // null DragOp would cause every empty cell to be rejected, which is
    // exactly the "unavailable cell" regression that broke equip-backpack
    // drops after the surface registration moved into the subsystem.
    if (OccupantId == UInventoryViewModel::EmptyCellInstanceId || OccupantId == INDEX_NONE)
    {
        return true;
    }

    if (!DragOp)
    {
        return false;
    }

    if (OccupantId == DragOp->InstanceId)
    {
        return true;
    }

    if (!InventoryVM || DragOp->Quantity <= 0)
    {
        return false;
    }

    FInventoryEntryView SourceEntry;
    const bool bHasSource = DragOp->bFromNearbyContainer
        ? InventoryVM->TryGetNearbyEntryByInstanceId(DragOp->InstanceId, SourceEntry)
        : InventoryVM->TryGetEntryByInstanceId(DragOp->InstanceId, SourceEntry);
    if (!bHasSource)
    {
        return false;
    }

    FInventoryEntryView TargetEntry;
    const bool bHasTarget = bSecondary
        ? InventoryVM->TryGetSecondaryEntryByCellIndex(CellIndex, TargetEntry)
        : InventoryVM->TryGetEntryByInstanceId(OccupantId, TargetEntry);
    if (!bHasTarget)
    {
        return false;
    }

    return FInventoryUIDropStackPolicy::CanPreviewStackOnto(SourceEntry, TargetEntry, DragOp->Quantity);
}

bool UW_InventoryPanel::TryResolveWidgetTopCenterViewportPos(const UWidget* Widget, FVector2D& OutViewportPos) const
{
    return InventoryPanelGridLayout::TryResolveWidgetTopCenterViewportPos(Widget, this, OutViewportPos);
}

bool UW_InventoryPanel::TryResolveTooltipAnchorViewportPos(const FInventoryEntryView& Entry, FVector2D& OutViewportPos) const
{
    // Option A (const-correct): pass `*this` directly to a narrow, truly
    // read-only overload instead of building a full mutable grid context
    // via a const_cast.  The helper consumes only Get*ReadOnly accessors.
    return InventoryPanelGridLayout::TryResolveTooltipAnchorViewportPos(*this, this, Entry, OutViewportPos);
}

void UW_InventoryPanel::RebuildPocketGrids()
{
    FInventoryPanelGridContext Ctx = BuildGridContext();
    InventoryPanelGridLayout::RebuildPocketGrids(Ctx);

    // Republish the current pocket surfaces to the shared drag host so
    // Route() can dispatch drops into them by tag.
    if (SurfaceRegistry.IsValid())
    {
        SurfaceRegistry->RegisterPockets();
    }
}

void UW_InventoryPanel::RebuildTabs()
{
    FInventoryPanelGridContext Ctx = BuildGridContext();
    InventoryPanelGridLayout::RebuildTabs(Ctx);
}

void UW_InventoryPanel::RebuildEquipSlots()
{
    FInventoryPanelGridContext Ctx = BuildGridContext();
    InventoryPanelGridLayout::RebuildEquipSlots(Ctx);
}

// ============================================================================
// Update Methods - Delegate to Helpers
// ============================================================================

void UW_InventoryPanel::UpdateAllVisuals()
{
    // Resolve the tag-indexed preview from the shared subsystem; widget-local
    // DragDropHandler no longer exists after the Slice 6e migration.
    static const TSet<int32> EmptyCells;
    const TSet<int32>* PrimaryPreview = &EmptyCells;
    const TSet<int32>* SecondaryPreview = &EmptyCells;
    bool bPreviewValid = true;
    if (const UInventoryUIDragHostSubsystem* Subsystem = ResolveDragHostSubsystem())
    {
        const FProjectUIGridDragPreviewResult& Preview = Subsystem->GetController().GetPreviewResult();
        if (Preview.bActive && SurfaceRegistry.IsValid())
        {
            bPreviewValid = Preview.bValid;
            const FGameplayTag& PrimaryTag = SurfaceRegistry->GetCachedPrimarySurfaceTag();
            const FGameplayTag& SecondaryTag = SurfaceRegistry->GetCachedSecondarySurfaceTag();
            if (PrimaryTag.IsValid())
            {
                if (const TSet<int32>* Cells = Preview.PreviewCellsBySurface.Find(PrimaryTag))
                {
                    PrimaryPreview = Cells;
                }
            }
            if (SecondaryTag.IsValid())
            {
                if (const TSet<int32>* Cells = Preview.PreviewCellsBySurface.Find(SecondaryTag))
                {
                    SecondaryPreview = Cells;
                }
            }
        }
    }

    VisualState.ApplyToGrid(CellBorders,
        PanelState.bSelectedSecondary ? INDEX_NONE : PanelState.SelectedCellIndex,
        PanelState.bHoveredSecondary ? INDEX_NONE : PanelState.HoveredCellIndex,
        *PrimaryPreview,
        bPreviewValid,
        [](UProjectGridCell* Cell, const FLinearColor& Color, bool bEnabled)
        {
            Cell->SetBrushColor(Color);
            Cell->SetIsEnabled(bEnabled);
        },
        [this](int32 Idx) { return InventoryVM ? InventoryVM->IsCellEnabled(Idx) : true; });

    VisualState.ApplyToGrid(SecondaryCellBorders,
        PanelState.bSelectedSecondary ? PanelState.SelectedCellIndexSecondary : INDEX_NONE,
        PanelState.bHoveredSecondary ? PanelState.HoveredCellIndexSecondary : INDEX_NONE,
        *SecondaryPreview,
        bPreviewValid,
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
// Click Handlers, Context Menu, Tooltip
// ============================================================================
//
// All handler bodies (hand / pocket / equip / tab / use / drop / equip /
// rotate / qty / context-menu / tooltip / click-catcher) live in a
// second translation unit so this TU stays close to orchestration only.
// See Private/Widgets/InventoryPanelInteractionHandlers.cpp.
//
// HandleViewModelPropertyChanged intentionally remains in this TU
// because the Slice 20 fitness test text-scans this file for the
// "SetVisibility(...Visible...)" literal.

// ============================================================================
// Input Handlers
// ============================================================================

FReply UW_InventoryPanel::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();

    if (Key == EKeys::E)
    {
        if (InventoryVM && InventoryVM->GetbHasNearbyContainer())
        {
            InventoryVM->HidePanel();
            return FReply::Handled();
        }
    }

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
        // Try hand grids and pocket grids before clearing hover
        int32 HandCellIndex = INDEX_NONE;
        bool bHandOrPocketHit = false;

        if (InventoryVM && LeftHandGridPanel
            && HitDetector.ResolveGridHit(LeftHandGridPanel,
                UInventoryViewModel::HandGridSize, UInventoryViewModel::HandGridSize,
                ScreenPos, HandCellIndex))
        {
            const int32 InstanceId = InventoryVM->GetLeftHandInstanceId(HandCellIndex);
            if (InstanceId != UInventoryViewModel::EmptyCellInstanceId)
            {
                PanelState.SetHoveredByInstanceId(InstanceId);
            }
            else
            {
                PanelState.ClearHover();
            }
            bHandOrPocketHit = true;
        }

        if (!bHandOrPocketHit && InventoryVM && RightHandGridPanel
            && HitDetector.ResolveGridHit(RightHandGridPanel,
                UInventoryViewModel::HandGridSize, UInventoryViewModel::HandGridSize,
                ScreenPos, HandCellIndex))
        {
            const int32 InstanceId = InventoryVM->GetRightHandInstanceId(HandCellIndex);
            if (InstanceId != UInventoryViewModel::EmptyCellInstanceId)
            {
                PanelState.SetHoveredByInstanceId(InstanceId);
            }
            else
            {
                PanelState.ClearHover();
            }
            bHandOrPocketHit = true;
        }

        if (!bHandOrPocketHit && InventoryVM)
        {
            for (const FPocketGridRuntime& PocketRuntime : PocketGridRuntime)
            {
                int32 PocketCellIndex = INDEX_NONE;
                if (PocketRuntime.GridPanel
                    && HitDetector.ResolveGridHit(PocketRuntime.GridPanel,
                        PocketRuntime.GridWidth, PocketRuntime.GridHeight,
                        ScreenPos, PocketCellIndex))
                {
                    const int32 InstanceId = InventoryVM->GetPocketCellInstanceId(
                        PocketRuntime.ViewModelPocketIndex, PocketCellIndex);
                    if (InstanceId != UInventoryViewModel::EmptyCellInstanceId)
                    {
                        PanelState.SetHoveredByInstanceId(InstanceId);
                    }
                    else
                    {
                        PanelState.ClearHover();
                    }
                    bHandOrPocketHit = true;
                    break;
                }
            }
        }

        if (!bHandOrPocketHit)
        {
            PanelState.ClearHover();
        }
    }

    // Show tooltip above the item anchor so drag/drop target cells stay visible.
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
    const bool bHas = FInventoryDragEntryResolver::Resolve(
        InventoryVM,
        PendingDragInstanceId,
        PanelState.PendingDragCellIndex,
        PanelState.bPendingDragSecondary,
        Entry);
    if (!bHas) { return; }

    UInventoryDragDropOperation* DragOp = NewObject<UInventoryDragDropOperation>();
    const bool bRotated = PanelState.bRotateNextDrop ? !Entry.bRotated : Entry.bRotated;
    PanelState.SyncQuantityToEntry(Entry.InstanceId, Entry.Quantity);
    const int32 DragQuantity = FMath::Clamp(PanelState.SelectedQuantity, 1, Entry.Quantity);

    DragOp->InstanceId = Entry.InstanceId;
    DragOp->FromContainer = Entry.ContainerId;
    DragOp->FromPos = Entry.GridPos;
    DragOp->Quantity = DragQuantity;
    DragOp->bRotated = bRotated;
    DragOp->ItemSize = bRotated ? FIntPoint(Entry.GridSize.Y, Entry.GridSize.X) : Entry.GridSize;
    DragOp->EquipSlotTag = Entry.EquipSlotTag;
    DragOp->bFromNearbyContainer = Entry.ContainerId == ProjectTags::Item_Container_WorldStorage;

    DragOp->DefaultDragVisual = FInventoryDragVisualBuilder::Build(this, Entry, DragQuantity, CurrentTheme);
    DragOp->Pivot = EDragPivot::CenterCenter;

    OutOperation = DragOp;
    PanelState.PendingDragCellIndex = INDEX_NONE;
    PendingDragInstanceId = INDEX_NONE;

    // Slice 16: mirror the drag-start into the subsystem session so the
    // structured event stream carries Started. Behavior-preserving; the
    // widget still owns DragOp construction and the existing drag
    // plumbing continues unchanged.
    if (UInventoryUIDragHostSubsystem* Subsystem = ResolveDragHostSubsystem())
    {
        FInventoryDragStartParams Params;
        Params.SourceTag = DragOp->FromContainer;
        Params.SourceCell = DragOp->FromPos;
        Params.InstanceId = DragOp->InstanceId;
        Params.Quantity = DragOp->Quantity;
        Params.bRotated = DragOp->bRotated;
        Subsystem->BeginCellDrag(Params);
    }
}

// Slice 17: NativeOnDragOver removed - cell-host wrappers
// (UW_InventoryCellDropTarget) own preview updates. Preview-driven
// repaint is wired via OnDragEvent subscription in NativeConstruct.

#if SLICE20_SABOTAGE
// Slice 20 fitness test 2 sabotage body - see Public/Slice20SabotageToggle.h.
bool UW_InventoryPanel::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    (void)InGeometry; (void)InDragDropEvent; (void)InOperation;
    return false;
}
#endif

void UW_InventoryPanel::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    if (UInventoryUIDragHostSubsystem* Subsystem = ResolveDragHostSubsystem())
    {
        Subsystem->GetController().ClearPreview();
        // Slice 16: DragLeave is NOT a terminal cancel - Slate fires it on
        // every drag-out and may re-enter with DragOver. The subsystem
        // session lives until NativeOnDrop or NativeOnDragCancelled fires.
    }
    PendingDragInstanceId = INDEX_NONE;
    bHasLastDragScreenPos = false;
    UpdateAllVisuals();
    Super::NativeOnDragLeave(InDragDropEvent, InOperation);
}

void UW_InventoryPanel::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    if (UInventoryUIDragHostSubsystem* Subsystem = ResolveDragHostSubsystem())
    {
        Subsystem->GetController().ClearPreview();
        // Slice 16: emit Cancelled on the subsystem event stream.
        Subsystem->CancelDrag();
    }
    PendingDragInstanceId = INDEX_NONE;
    bHasLastDragScreenPos = false;
    UpdateAllVisuals();
    Super::NativeOnDragCancelled(InDragDropEvent, InOperation);
}

// Slice 19: NativeOnDrop removed. Equip-slot drops are owned by
// UW_InventoryEquipSlotDropTarget wrappers (smallest semantic drop
// target). Grid / hand / pocket drops remain on the Slice 17
// UW_InventoryCellDropTarget wrappers. The root widget no longer
// overrides any drag-drop handler; the drag host subsystem's
// OnDragEvent stream drives preview repaint via HandleDragEvent.

bool UW_InventoryPanel::UpdateDragPreviewFromSubsystem(UInventoryDragDropOperation* DragOp, const FVector2D& ScreenPos)
{
    // Follow-up #1: widgets may not construct FProjectUIGridDragPayload (invariant #3).
    // The widget builds only a read-only domain candidate struct; the subsystem
    // constructs the payload internally and calls the controller. This matches
    // how UW_InventoryCellDropTarget::NativeOnDragOver already forwards.
    UInventoryUIDragHostSubsystem* Subsystem = ResolveDragHostSubsystem();
    if (!DragOp || !InventoryVM || !Subsystem)
    {
        if (Subsystem)
        {
            Subsystem->GetController().ClearPreview();
        }
        return false;
    }

    FInventoryCellCandidate Candidate;
    Candidate.InstanceId = DragOp->InstanceId;
    Candidate.SourceSurfaceTag = DragOp->FromContainer;
    Candidate.SourcePos = DragOp->FromPos;
    Candidate.Quantity = DragOp->Quantity;
    Candidate.bRotated = DragOp->bRotated;
    // ResolveDragItemFootprint reads the rotation-adjusted size from DragOp
    // + VM; DragOp::ApplyRotationFromEntry has already rotated ItemSize by
    // the time HandleRotateClicked reaches here, so the footprint is correct.
    Candidate.ItemSize = InventoryPanelGridLayout::ResolveDragItemFootprint(InventoryVM.Get(), DragOp);

    Subsystem->UpdatePreview(Candidate, ScreenPos);
    return true;
}

// ============================================================================
// Drag Host Subsystem registration (Slice 6e)
// ============================================================================

UInventoryUIDragHostSubsystem* UW_InventoryPanel::ResolveDragHostSubsystem() const
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (ULocalPlayer* LP = PC->GetLocalPlayer())
        {
            return LP->GetSubsystem<UInventoryUIDragHostSubsystem>();
        }
    }
    return nullptr;
}
