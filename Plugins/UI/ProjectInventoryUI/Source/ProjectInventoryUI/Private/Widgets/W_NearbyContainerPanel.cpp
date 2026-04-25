// Copyright ALIS. All Rights Reserved.

#include "Widgets/W_NearbyContainerPanel.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "HAL/IConsoleManager.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Input/DragAndDrop.h"
#include "Interaction/IInventorySurfacePolicyProvider.h"
#include "Interaction/InventoryUISurfacePriority.h"
#include "Layout/ProjectWidgetLayoutLoader.h"
#include "MVVM/InventoryViewModel.h"
#include "Presentation/NearbyContainerPresentation.h"
#include "Presentation/ProjectUIWidgetBinder.h"
#include "ProjectGameplayTags.h"
#include "Settings/InventoryUISettings.h"
#include "Subsystems/InventoryUIDragHostSubsystem.h"
#include "Theme/ProjectUIThemeData.h"
#include "Widgets/InventoryDragDropOperation.h"
#include "Widgets/InventoryDragVisualBuilder.h"
#include "Widgets/ProjectGridCell.h"

DEFINE_LOG_CATEGORY(LogNearbyContainerPanel);

namespace
{
    bool NearbyPanel_IsInventoryDragDiagEnabled()
    {
#if !UE_BUILD_SHIPPING
        if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("inv.drag.diag")))
        {
            return Var->GetInt() != 0;
        }
#endif
        return false;
    }

    const TCHAR* VisibilityToString(const ESlateVisibility Visibility)
    {
        switch (Visibility)
        {
        case ESlateVisibility::Visible:
            return TEXT("Visible");
        case ESlateVisibility::Collapsed:
            return TEXT("Collapsed");
        case ESlateVisibility::Hidden:
            return TEXT("Hidden");
        case ESlateVisibility::HitTestInvisible:
            return TEXT("HitTestInvisible");
        case ESlateVisibility::SelfHitTestInvisible:
            return TEXT("SelfHitTestInvisible");
        default:
            return TEXT("Unknown");
        }
    }

    UInventoryUIDragHostSubsystem* ResolveDragHostSubsystem(const UW_NearbyContainerPanel* Widget)
    {
        if (!Widget)
        {
            return nullptr;
        }

        if (APlayerController* PC = Widget->GetOwningPlayer())
        {
            if (ULocalPlayer* LP = PC->GetLocalPlayer())
            {
                return LP->GetSubsystem<UInventoryUIDragHostSubsystem>();
            }
        }

        return nullptr;
    }

    // Single source of truth for the root-visibility rule. See header
    // VISIBILITY CONTRACT: a `Visible` root on this Fill-anchored
    // widget would absorb drag events meant for the sibling
    // W_InventoryPanel. Any other state (Collapsed / Hidden /
    // HitTestInvisible / SelfHitTestInvisible) is left untouched.
    ESlateVisibility NormalizeNearbyRootVisibility(ESlateVisibility InVisibility)
    {
        return (InVisibility == ESlateVisibility::Visible)
            ? ESlateVisibility::SelfHitTestInvisible
            : InVisibility;
    }
}

UW_NearbyContainerPanel::UW_NearbyContainerPanel(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    ConfigFilePath = UProjectWidgetLayoutLoader::GetPluginUIConfigPath(
        TEXT("ProjectInventoryUI"),
        TEXT("NearbyContainerPanel.json"));
    SetIsFocusable(false);
}

void UW_NearbyContainerPanel::NativeConstruct()
{
    Super::NativeConstruct();

    // Cell sizing SOT shared with the main panel.
    CachedCellSize = FInventoryUISettings::Get().CellSize;
    GridBuilder.Initialize(this, WidgetTree);
    GridBuilder.SetCellSize(CachedCellSize);
    // Bind per-cell mouse-down handler. This is what makes drag-from-nearby
    // work even when the user-widget root is SelfHitTestInvisible: the
    // cell receives the click, returns DetectDragIfPressed pointing back
    // at this user widget, and Slate dispatches NativeOnDragDetected on
    // the user widget when the drag threshold is met.
    GridBuilder.SetCellMouseDownHandler(
        UProjectGridCell::FOnGridCellMouseDown::CreateUObject(this, &UW_NearbyContainerPanel::HandleNearbyCellMouseDown));

    if (!RootWidget)
    {
        UE_LOG(LogNearbyContainerPanel, Warning, TEXT("NativeConstruct: RootWidget is null"));
        return;
    }

    FProjectUIWidgetBinder Binder(RootWidget, GetClass()->GetName());
    NearbyGridHost = Binder.FindRequired<UBorder>(TEXT("NearbyGridHost"));
    NearbyGridSizeBox = Binder.FindOptional<USizeBox>(TEXT("NearbyGridSizeBox"));
    NearbyTitleText = Binder.FindOptional<UTextBlock>(TEXT("NearbyTitleText"));
    NearbyStatsText = Binder.FindOptional<UTextBlock>(TEXT("NearbyStatsText"));
    NearbyHintText = Binder.FindOptional<UTextBlock>(TEXT("NearbyHintText"));
    TakeAllButton = Binder.FindOptional<UButton>(TEXT("TakeAllButton"));
    Binder.LogMissingRequired(TEXT("UW_NearbyContainerPanel::NativeConstruct"));

    if (TakeAllButton)
    {
        TakeAllButton->OnClicked.AddUniqueDynamic(this, &UW_NearbyContainerPanel::HandleTakeAllClicked);
    }

    // Start collapsed; RefreshFromViewModel will reveal it when a session
    // opens with a nearby container attached.
    SetVisibility(ESlateVisibility::Collapsed);

    if (InventoryVM)
    {
        RefreshFromViewModel();
    }
}

void UW_NearbyContainerPanel::NativeDestruct()
{
    if (InventoryVM)
    {
        InventoryVM->OnPropertyChanged.RemoveDynamic(this, &UW_NearbyContainerPanel::HandleInventoryVMPropertyChanged);
    }

    if (bSurfaceRegistered)
    {
        if (UInventoryUIDragHostSubsystem* Subsystem = ResolveDragHostSubsystem(this))
        {
            Subsystem->UnregisterSurface(ProjectTags::Item_Container_WorldStorage);
        }
        bSurfaceRegistered = false;
    }

    Super::NativeDestruct();
}

void UW_NearbyContainerPanel::SetInventoryViewModel(UInventoryViewModel* InViewModel)
{
    if (InventoryVM == InViewModel)
    {
        return;
    }

    if (InventoryVM)
    {
        InventoryVM->OnPropertyChanged.RemoveDynamic(this, &UW_NearbyContainerPanel::HandleInventoryVMPropertyChanged);
    }

    InventoryVM = InViewModel;

    // Follow-up #2: when the nearby panel owns the only inventory VM in a
    // test harness (no main panel present), the subsystem still needs a
    // policy provider so its installed EnabledChecker/OccupantChecker
    // closures can answer occupant queries. Setting it here is a no-op
    // when the main panel also set it to the same VM - TScriptInterface
    // equality is by underlying UObject.
    if (UInventoryUIDragHostSubsystem* Subsystem = ResolveDragHostSubsystem(this))
    {
        if (InventoryVM)
        {
            TScriptInterface<IInventorySurfacePolicyProvider> ProviderInterface(InventoryVM);
            Subsystem->SetPolicyProvider(ProviderInterface);
        }
        // On nearby-only teardown we deliberately do NOT clear the
        // provider - the main panel may still own it. Each owner clears
        // on its own teardown path.
    }

    if (InventoryVM)
    {
        InventoryVM->OnPropertyChanged.AddUniqueDynamic(this, &UW_NearbyContainerPanel::HandleInventoryVMPropertyChanged);
        RefreshFromViewModel();
    }
    else
    {
        SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UW_NearbyContainerPanel::OnViewModelChanged_Implementation(UProjectViewModel* /*OldViewModel*/, UProjectViewModel* NewViewModel)
{
    SetInventoryViewModel(Cast<UInventoryViewModel>(NewViewModel));
}

void UW_NearbyContainerPanel::RefreshFromViewModel_Implementation()
{
    if (!InventoryVM)
    {
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    UpdateVisibilityFromViewModel();
    RebuildGrid();
    UpdateTextAndControls();
}

void UW_NearbyContainerPanel::OnThemeChanged_Implementation(UProjectUIThemeData* NewTheme)
{
    GridBuilder.SetTheme(NewTheme);
    if (InventoryVM)
    {
        RefreshFromViewModel();
    }
}

void UW_NearbyContainerPanel::HandleInventoryVMPropertyChanged(FName /*PropertyName*/)
{
    RefreshFromViewModel();
}

void UW_NearbyContainerPanel::HandleTakeAllClicked()
{
    if (InventoryVM)
    {
        InventoryVM->RequestTakeAllNearbyContainer();
    }
}

void UW_NearbyContainerPanel::UpdateVisibilityFromViewModel()
{
    const bool bShouldShow = InventoryVM
        && InventoryVM->GetbPanelVisible()
        && InventoryVM->GetbHasNearbyContainer();
    // SelfHitTestInvisible (not Visible) on the user-widget root: this
    // widget's RootCanvas anchors to Fill (full viewport), so a plain
    // Visible would intercept drag/drop events meant for the sibling
    // W_InventoryPanel anywhere outside the small CenterRight content
    // area. SelfHitTestInvisible keeps the canvas itself out of hit
    // testing while children (NearbyGridHost cells, TakeAllButton)
    // remain interactive. See pitfalls.md "Production trigger spawned
    // only main panel..." section for the sibling routing rules.
    // SelfHitTestInvisible: the user-widget root spans the full viewport
    // (layer host adds it with Fill alignment), so a plain Visible would
    // intercept every click before they reach the sibling W_InventoryPanel.
    // Children (NearbyBackground, grid cells, TakeAllButton) stay Visible
    // by default and intercept clicks within their own bounds. Cell drag
    // detection happens at the cell level via SetCellMouseDownHandler -
    // that callback runs even when the user-widget root is skipped from
    // hit testing.
    const ESlateVisibility NewVisibility = bShouldShow
        ? ESlateVisibility::SelfHitTestInvisible
        : ESlateVisibility::Collapsed;
    SetVisibility(NewVisibility);

    if (NearbyPanel_IsInventoryDragDiagEnabled())
    {
        UE_LOG(
            LogNearbyContainerPanel,
            Log,
            TEXT("UpdateVisibilityFromViewModel: PanelVisible=%d HasNearby=%d -> RootVisibility=%s"),
            InventoryVM && InventoryVM->GetbPanelVisible() ? 1 : 0,
            InventoryVM && InventoryVM->GetbHasNearbyContainer() ? 1 : 0,
            VisibilityToString(NewVisibility));
    }
}

void UW_NearbyContainerPanel::RebuildGrid()
{
    if (!InventoryVM || !NearbyGridHost)
    {
        return;
    }

    const int32 NewW = InventoryVM->GetSecondaryGridWidth();
    const int32 NewH = InventoryVM->GetSecondaryGridHeight();
    const bool bHasGrid = NewW > 0 && NewH > 0 && InventoryVM->GetbHasNearbyContainer();

    if (!bHasGrid)
    {
        GridPanel = nullptr;
        CellPrimaryWidgets.Reset();
        CellQuantityWidgets.Reset();
        CellQuantityBadges.Reset();
        CellBorders.Reset();
        NearbyGridHost->SetContent(nullptr);
        CachedGridWidth = 0;
        CachedGridHeight = 0;

        if (bSurfaceRegistered)
        {
            if (UInventoryUIDragHostSubsystem* Subsystem = ResolveDragHostSubsystem(this))
            {
                Subsystem->UnregisterSurface(ProjectTags::Item_Container_WorldStorage);
            }
            bSurfaceRegistered = false;
        }
        return;
    }

    const bool bHostMissingContent = NearbyGridHost->GetContent() != GridPanel;
    const bool bCellMismatch = CellPrimaryWidgets.Num() != (NewW * NewH) || CellBorders.Num() != (NewW * NewH);

    if (NewW != CachedGridWidth || NewH != CachedGridHeight || !GridPanel || bHostMissingContent || bCellMismatch)
    {
        CachedGridWidth = NewW;
        CachedGridHeight = NewH;
        GridPanel = GridBuilder.BuildGrid(
            NewW,
            NewH,
            ProjectTags::Item_Container_WorldStorage,
            CellPrimaryWidgets,
            CellQuantityWidgets,
            CellQuantityBadges,
            CellBorders,
            true /* bIsSecondary: theme/visual flag */);
        if (GridPanel)
        {
            NearbyGridHost->SetContent(GridPanel);
        }
        for (UProjectGridCell* Cell : CellBorders)
        {
            if (Cell)
            {
                Cell->SetIsGridCell(true);
            }
        }
    }

    ApplyGridHostSize(NewW, NewH);

    GridBuilder.UpdateGridVisuals(
        InventoryVM->GetSecondaryCellVisuals(),
        CellPrimaryWidgets,
        CellQuantityWidgets,
        CellQuantityBadges);

    // Register (or re-register, idempotent) our surface with the shared drag host.
    // Follow-up #2: no widget-side closures. Subsystem installs fan-out
    // to IInventorySurfacePolicyProvider keyed on SurfaceTag; the
    // Item.Container.WorldStorage branch of the VM's provider methods
    // maps to the secondary-cell path that used to live in the widget.
    if (UInventoryUIDragHostSubsystem* Subsystem = ResolveDragHostSubsystem(this))
    {
        FProjectUIGridSurface Surface;
        Surface.SurfaceTag = ProjectTags::Item_Container_WorldStorage;
        Surface.Grid = GridPanel;
        Surface.Dims = FIntPoint(NewW, NewH);
        Surface.Priority = InventoryUISurfacePriority::NearbyWorldStorage;
        Subsystem->RegisterSurface(MoveTemp(Surface));
        bSurfaceRegistered = true;
    }
}

void UW_NearbyContainerPanel::ApplyGridHostSize(int32 GridWidth, int32 GridHeight) const
{
    if (!NearbyGridSizeBox || GridWidth <= 0 || GridHeight <= 0)
    {
        return;
    }
    const FIntPoint HostSize = FInventoryPanelGridBuilder::ComputeGridHostPixelSize(GridWidth, GridHeight, CachedCellSize);
    NearbyGridSizeBox->SetWidthOverride(static_cast<float>(HostSize.X));
    NearbyGridSizeBox->SetHeightOverride(static_cast<float>(HostSize.Y));
}

void UW_NearbyContainerPanel::SetVisibility(ESlateVisibility InVisibility)
{
    const ESlateVisibility Normalized = NormalizeNearbyRootVisibility(InVisibility);
    if (Normalized != InVisibility && NearbyPanel_IsInventoryDragDiagEnabled())
    {
        UE_LOG(
            LogNearbyContainerPanel,
            Warning,
            TEXT("SetVisibility(Visible) coerced to SelfHitTestInvisible (visibility contract)."));
    }
    Super::SetVisibility(Normalized);
}

void UW_NearbyContainerPanel::SynchronizeProperties()
{
    Super::SynchronizeProperties();

    const ESlateVisibility Current = GetVisibility();
    const ESlateVisibility Normalized = NormalizeNearbyRootVisibility(Current);
    if (Normalized != Current)
    {
        // Bypass our own SetVisibility override: we already normalized,
        // and this avoids a redundant diag log during re-application.
        Super::SetVisibility(Normalized);
    }
}

FEventReply UW_NearbyContainerPanel::HandleNearbyCellMouseDown(int32 CellIndex, bool /*bSecondary*/, const FPointerEvent& MouseEvent)
{
    PendingDragCellIndex = INDEX_NONE;

    if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    {
        return UWidgetBlueprintLibrary::Unhandled();
    }
    if (!InventoryVM || !InventoryVM->GetbHasNearbyContainer())
    {
        return UWidgetBlueprintLibrary::Unhandled();
    }

    // Don't initiate a drag from an empty cell.
    FInventoryEntryView Entry;
    if (!InventoryVM->TryGetSecondaryEntryByCellIndex(CellIndex, Entry) || Entry.InstanceId == INDEX_NONE)
    {
        return UWidgetBlueprintLibrary::Unhandled();
    }

    UE_LOG(LogNearbyContainerPanel, Verbose,
        TEXT("HandleNearbyCellMouseDown: cell %d -> InstanceId=%d, requesting drag detection"),
        CellIndex, Entry.InstanceId);

    PendingDragCellIndex = CellIndex;
    return UWidgetBlueprintLibrary::DetectDragIfPressed(MouseEvent, this, EKeys::LeftMouseButton);
}

FReply UW_NearbyContainerPanel::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    // Per-cell handler (HandleNearbyCellMouseDown) does the drag-detect
    // work now. This override stays only as a fall-through that returns
    // Unhandled, so clicks that bubble here from non-cell areas don't
    // capture the click and steal it from sibling widgets. With the user
    // widget set to SelfHitTestInvisible, this rarely fires anyway -
    // empty area clicks fall through to the main panel.
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UW_NearbyContainerPanel::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

    const int32 CellIndex = PendingDragCellIndex;
    PendingDragCellIndex = INDEX_NONE;

    if (!InventoryVM || !InventoryVM->GetbHasNearbyContainer() || CellIndex == INDEX_NONE)
    {
        UE_LOG(LogNearbyContainerPanel, Verbose,
            TEXT("NativeOnDragDetected: no pending cell or VM not ready (cell=%d)"), CellIndex);
        return;
    }

    FInventoryEntryView Entry;
    if (!InventoryVM->TryGetSecondaryEntryByCellIndex(CellIndex, Entry) || Entry.InstanceId == INDEX_NONE)
    {
        UE_LOG(LogNearbyContainerPanel, Verbose,
            TEXT("NativeOnDragDetected: cell %d resolved no entry"), CellIndex);
        return;
    }

    UE_LOG(LogNearbyContainerPanel, Log,
        TEXT("NativeOnDragDetected: starting drag from nearby cell %d (InstanceId=%d)"),
        CellIndex, Entry.InstanceId);

    UInventoryDragDropOperation* DragOp = NewObject<UInventoryDragDropOperation>();
    const int32 DragQuantity = FMath::Max(1, Entry.Quantity);
    DragOp->InstanceId = Entry.InstanceId;
    DragOp->FromContainer = ProjectTags::Item_Container_WorldStorage;
    DragOp->FromPos = Entry.GridPos;
    DragOp->Quantity = DragQuantity;
    DragOp->bRotated = Entry.bRotated;
    DragOp->ItemSize = Entry.bRotated
        ? FIntPoint(FMath::Max(1, Entry.GridSize.Y), FMath::Max(1, Entry.GridSize.X))
        : FIntPoint(FMath::Max(1, Entry.GridSize.X), FMath::Max(1, Entry.GridSize.Y));
    DragOp->EquipSlotTag = Entry.EquipSlotTag;
    DragOp->bFromNearbyContainer = true;

    DragOp->DefaultDragVisual = FInventoryDragVisualBuilder::Build(this, Entry, DragQuantity, /*Theme*/ nullptr);
    DragOp->Pivot = EDragPivot::CenterCenter;
    OutOperation = DragOp;

    // Slice 16: mirror the drag-start into the subsystem session so the
    // event stream carries Started without changing existing drag
    // plumbing. Widget still owns DragOp construction today; a future
    // slice moves that under the subsystem API.
    if (UInventoryUIDragHostSubsystem* Subsystem = ResolveDragHostSubsystem(this))
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

void UW_NearbyContainerPanel::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    if (NearbyPanel_IsInventoryDragDiagEnabled())
    {
        UE_LOG(
            LogNearbyContainerPanel,
            Log,
            TEXT("NativeOnDragLeave: Screen=(%.1f,%.1f) RootVis=%s Enabled=%d Op=%s"),
            InDragDropEvent.GetScreenSpacePosition().X,
            InDragDropEvent.GetScreenSpacePosition().Y,
            VisibilityToString(GetVisibility()),
            GetIsEnabled() ? 1 : 0,
            InOperation ? *InOperation->GetPathName() : TEXT("null"));
    }

    Super::NativeOnDragLeave(InDragDropEvent, InOperation);
}

void UW_NearbyContainerPanel::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    if (NearbyPanel_IsInventoryDragDiagEnabled())
    {
        UE_LOG(
            LogNearbyContainerPanel,
            Warning,
            TEXT("NativeOnDragCancelled: Screen=(%.1f,%.1f) RootVis=%s Enabled=%d Op=%s"),
            InDragDropEvent.GetScreenSpacePosition().X,
            InDragDropEvent.GetScreenSpacePosition().Y,
            VisibilityToString(GetVisibility()),
            GetIsEnabled() ? 1 : 0,
            InOperation ? *InOperation->GetPathName() : TEXT("null"));
    }

    Super::NativeOnDragCancelled(InDragDropEvent, InOperation);
}

// Slice 17: NativeOnDragOver / NativeOnDrop removed. Grid drops are
// owned by UW_InventoryCellDropTarget wrappers (one per grid cell)
// built by FInventoryPanelGridBuilder. Those wrappers receive the
// Slate drop events via the leaf-first bubble and forward to
// UInventoryUIDragHostSubsystem::UpdatePreview / CompleteDrop. The
// nearby widget only originates drags (NativeOnDragDetected above).

void UW_NearbyContainerPanel::UpdateTextAndControls()
{
    if (NearbyTitleText)
    {
        NearbyTitleText->SetText(FNearbyContainerPresentation::BuildTitle(InventoryVM));
    }

    if (NearbyStatsText)
    {
        NearbyStatsText->SetText(FNearbyContainerPresentation::BuildStats(InventoryVM));
    }

    if (NearbyHintText)
    {
        NearbyHintText->SetVisibility(FNearbyContainerPresentation::ShouldShowHint(InventoryVM)
            ? ESlateVisibility::SelfHitTestInvisible
            : ESlateVisibility::Collapsed);
    }

    if (TakeAllButton)
    {
        const bool bShow = FNearbyContainerPresentation::ShouldShowTakeAll(InventoryVM);
        TakeAllButton->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        TakeAllButton->SetIsEnabled(FNearbyContainerPresentation::IsTakeAllEnabled(InventoryVM));
    }
}
