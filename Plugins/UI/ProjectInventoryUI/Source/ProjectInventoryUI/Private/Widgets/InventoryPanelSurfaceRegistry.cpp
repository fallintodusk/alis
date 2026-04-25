// Copyright ALIS. All Rights Reserved.

#include "Widgets/InventoryPanelSurfaceRegistry.h"

#include "Widgets/W_InventoryPanel.h"
#include "Widgets/InventoryPanelGridLayout.h"
#include "MVVM/InventoryViewModel.h"
#include "MVVM/InventoryDragEvent.h"
#include "Subsystems/InventoryUIDragHostSubsystem.h"
#include "Interaction/InventoryUISurfacePriority.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Components/UniformGridPanel.h"
#include "HAL/IConsoleManager.h"
#include "ProjectGameplayTags.h"

// Named namespace to avoid unity-TU symbol collisions with sibling
// .cpps in the ProjectInventoryUI module that also define
// IsInventoryDragDiagEnabled in anonymous namespaces. See
// Plugins/UI/ProjectInventoryUI/docs/pitfalls.md "Pattern G".
namespace InventoryPanelSurfaceRegistryLocal
{
    bool IsInventoryDragDiagEnabled()
    {
#if !UE_BUILD_SHIPPING
        if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("inv.drag.diag")))
        {
            return Var->GetInt() != 0;
        }
#endif
        return false;
    }
}
// NOTE: do NOT add `using InventoryPanelSurfaceRegistryLocal::...;` at
// file scope - it causes lookup ambiguity in unity TU concat with
// sibling files that have an anonymous-namespace IsInventoryDragDiagEnabled.
// Call sites below qualify with the namespace explicitly.

FInventoryPanelSurfaceRegistry::~FInventoryPanelSurfaceRegistry()
{
    // Defence-in-depth for the lifetime contract documented on BindEventBus:
    // both methods are safe to call when already unbound / unregistered, so
    // this is a no-op on the production path (NativeDestruct already unbound
    // and unregistered).  Only matters for early-shutdown / test-harness
    // paths where NativeDestruct never ran.
    UnbindEventBus();
    UnregisterAll();
}

void FInventoryPanelSurfaceRegistry::Initialize(UW_InventoryPanel* InOwner)
{
    Owner = InOwner;
}

UInventoryUIDragHostSubsystem* FInventoryPanelSurfaceRegistry::ResolveDragHostSubsystem() const
{
    if (UW_InventoryPanel* Panel = Owner.Get())
    {
        APlayerController* PC = Panel->GetOwningPlayer();
        if (PC)
        {
            if (ULocalPlayer* LP = PC->GetLocalPlayer())
            {
                return LP->GetSubsystem<UInventoryUIDragHostSubsystem>();
            }
        }

        if (InventoryPanelSurfaceRegistryLocal::IsInventoryDragDiagEnabled())
        {
            UE_LOG(
                LogInventoryPanel,
                Warning,
                TEXT("SurfaceRegistry.ResolveDragHostSubsystem: no LocalPlayer for Panel=%s OwningPlayer=%s"),
                *GetNameSafe(Panel),
                *GetNameSafe(PC));
        }
    }
    else if (InventoryPanelSurfaceRegistryLocal::IsInventoryDragDiagEnabled())
    {
        UE_LOG(LogInventoryPanel, Warning, TEXT("SurfaceRegistry.ResolveDragHostSubsystem: owner panel expired"));
    }
    return nullptr;
}

void FInventoryPanelSurfaceRegistry::BindEventBus()
{
    UInventoryUIDragHostSubsystem* Subsystem = ResolveDragHostSubsystem();
    if (!Subsystem)
    {
        if (InventoryPanelSurfaceRegistryLocal::IsInventoryDragDiagEnabled())
        {
            UE_LOG(LogInventoryPanel, Warning, TEXT("SurfaceRegistry.BindEventBus: skipped, no drag host subsystem"));
        }
        return;
    }
    if (DragEventHandle.IsValid())
    {
        Subsystem->OnDragEvent.Remove(DragEventHandle);
        DragEventHandle.Reset();
    }
    // Invariant: lifetime contract - UnbindEventBus must run before `this`
    // is destroyed.  UW_InventoryPanel::NativeDestruct() handles this in
    // production; the FInventoryPanelSurfaceRegistry destructor is a
    // safety net for early-shutdown / test-harness paths where
    // NativeDestruct never ran.  AddRaw does not manage lifetime.
    DragEventHandle = Subsystem->OnDragEvent.AddRaw(this, &FInventoryPanelSurfaceRegistry::HandleDragEvent);
}

void FInventoryPanelSurfaceRegistry::UnbindEventBus()
{
    if (UInventoryUIDragHostSubsystem* Subsystem = ResolveDragHostSubsystem())
    {
        if (DragEventHandle.IsValid())
        {
            Subsystem->OnDragEvent.Remove(DragEventHandle);
        }
    }
    DragEventHandle.Reset();
}

void FInventoryPanelSurfaceRegistry::HandleDragEvent(const FInventoryDragEvent& Event)
{
    switch (Event.Kind)
    {
        case EInventoryDragEventKind::PreviewUpdated:
        case EInventoryDragEventKind::PreviewCleared:
        case EInventoryDragEventKind::Completed:
        case EInventoryDragEventKind::Cancelled:
            // Preview highlights and selection visuals read from the
            // subsystem controller's preview state. Any transition that
            // mutates the preview must trigger a repaint on the panel;
            // the panel binds OnPreviewChanged to UpdateAllVisuals in
            // NativeConstruct.
            OnPreviewChanged.ExecuteIfBound();
            break;
        default:
            break;
    }
}

void FInventoryPanelSurfaceRegistry::RegisterHands()
{
    UW_InventoryPanel* Panel = Owner.Get();
    if (!Panel)
    {
        if (InventoryPanelSurfaceRegistryLocal::IsInventoryDragDiagEnabled())
        {
            UE_LOG(LogInventoryPanel, Warning, TEXT("SurfaceRegistry.RegisterHands: skipped, owner panel missing"));
        }
        return;
    }

    UInventoryUIDragHostSubsystem* Subsystem = ResolveDragHostSubsystem();
    UUniformGridPanel* LeftHandGridPanel = Panel->LeftHandGridPanel;
    UUniformGridPanel* RightHandGridPanel = Panel->RightHandGridPanel;
    if (!Subsystem || !Panel->InventoryVM || !LeftHandGridPanel || !RightHandGridPanel)
    {
        if (InventoryPanelSurfaceRegistryLocal::IsInventoryDragDiagEnabled())
        {
            UE_LOG(
                LogInventoryPanel,
                Warning,
                TEXT("SurfaceRegistry.RegisterHands: skipped Subsystem=%d VM=%d LeftGrid=%d RightGrid=%d Panel=%s"),
                Subsystem ? 1 : 0,
                Panel->InventoryVM ? 1 : 0,
                LeftHandGridPanel ? 1 : 0,
                RightHandGridPanel ? 1 : 0,
                *GetNameSafe(Panel));
        }
        return;
    }

    // Follow-up #2: widgets pass data only. EnabledChecker / OccupantChecker
    // / OccupantAllowedChecker are installed by the subsystem and fan out
    // to IInventorySurfacePolicyProvider methods keyed on SurfaceTag. The
    // old [WeakVM] closures duplicated the provider's job.
    FProjectUIGridSurface Left;
    Left.SurfaceTag = ProjectTags::Item_Container_LeftHand;
    Left.Grid = LeftHandGridPanel;
    Left.Dims = FIntPoint(UInventoryViewModel::HandGridSize, UInventoryViewModel::HandGridSize);
    Left.Priority = InventoryUISurfacePriority::PlayerStorage;
    Subsystem->RegisterSurface(MoveTemp(Left));

    FProjectUIGridSurface Right;
    Right.SurfaceTag = ProjectTags::Item_Container_RightHand;
    Right.Grid = RightHandGridPanel;
    Right.Dims = FIntPoint(UInventoryViewModel::HandGridSize, UInventoryViewModel::HandGridSize);
    Right.Priority = InventoryUISurfacePriority::PlayerStorage;
    Subsystem->RegisterSurface(MoveTemp(Right));

    bHandSurfacesRegistered = true;

    if (InventoryPanelSurfaceRegistryLocal::IsInventoryDragDiagEnabled())
    {
        UE_LOG(
            LogInventoryPanel,
            Log,
            TEXT("SurfaceRegistry.RegisterHands: registered LeftHand/RightHand for Panel=%s LeftGrid=%s RightGrid=%s"),
            *GetNameSafe(Panel),
            *GetNameSafe(LeftHandGridPanel),
            *GetNameSafe(RightHandGridPanel));
    }
}

void FInventoryPanelSurfaceRegistry::RegisterPockets()
{
    UW_InventoryPanel* Panel = Owner.Get();
    if (!Panel)
    {
        if (InventoryPanelSurfaceRegistryLocal::IsInventoryDragDiagEnabled())
        {
            UE_LOG(LogInventoryPanel, Warning, TEXT("SurfaceRegistry.RegisterPockets: skipped, owner panel missing"));
        }
        return;
    }

    UInventoryUIDragHostSubsystem* Subsystem = ResolveDragHostSubsystem();
    if (!Subsystem)
    {
        if (InventoryPanelSurfaceRegistryLocal::IsInventoryDragDiagEnabled())
        {
            UE_LOG(LogInventoryPanel, Warning, TEXT("SurfaceRegistry.RegisterPockets: skipped, no drag host subsystem"));
        }
        return;
    }

    // Tags may have changed across rebuilds; unregister the stale set first.
    for (const FGameplayTag& StaleTag : RegisteredPocketSurfaceTags)
    {
        if (StaleTag.IsValid())
        {
            Subsystem->UnregisterSurface(StaleTag);
        }
    }
    RegisteredPocketSurfaceTags.Reset();

    if (!Panel->InventoryVM)
    {
        if (InventoryPanelSurfaceRegistryLocal::IsInventoryDragDiagEnabled())
        {
            UE_LOG(LogInventoryPanel, Warning, TEXT("SurfaceRegistry.RegisterPockets: skipped, no InventoryVM"));
        }
        return;
    }

    // Follow-up #2: widgets pass data only. The subsystem installs the
    // three controller checkers on RegisterSurface and fans out to the
    // policy provider, which uses SurfaceTag (= Pocket.ContainerId here)
    // to find PocketIndex via FindPocketIndexByContainerTag and delegates
    // to IsPocketCellEnabled / GetPocketCellInstanceId.
    for (const FPocketGridRuntime& Pocket : Panel->PocketGridRuntime)
    {
        if (!Pocket.ContainerId.IsValid() || !Pocket.GridPanel
            || Pocket.GridWidth <= 0 || Pocket.GridHeight <= 0)
        {
            continue;
        }

        FProjectUIGridSurface Surface;
        Surface.SurfaceTag = Pocket.ContainerId;
        Surface.Grid = Pocket.GridPanel;
        Surface.Dims = FIntPoint(Pocket.GridWidth, Pocket.GridHeight);
        Surface.Priority = InventoryUISurfacePriority::PlayerStorage;

        Subsystem->RegisterSurface(MoveTemp(Surface));
        RegisteredPocketSurfaceTags.Add(Pocket.ContainerId);

        if (InventoryPanelSurfaceRegistryLocal::IsInventoryDragDiagEnabled())
        {
            UE_LOG(
                LogInventoryPanel,
                Log,
                TEXT("SurfaceRegistry.RegisterPockets: registered %s Dims=%dx%d Grid=%s"),
                *Pocket.ContainerId.ToString(),
                Pocket.GridWidth,
                Pocket.GridHeight,
                *GetNameSafe(Pocket.GridPanel));
        }
    }
}

void FInventoryPanelSurfaceRegistry::RegisterPlayerGrids()
{
    UW_InventoryPanel* Panel = Owner.Get();
    if (!Panel)
    {
        if (InventoryPanelSurfaceRegistryLocal::IsInventoryDragDiagEnabled())
        {
            UE_LOG(LogInventoryPanel, Warning, TEXT("SurfaceRegistry.RegisterPlayerGrids: skipped, owner panel missing"));
        }
        return;
    }

    UInventoryUIDragHostSubsystem* Subsystem = ResolveDragHostSubsystem();
    UInventoryViewModel* InventoryVM = Panel->InventoryVM;
    if (!Subsystem || !InventoryVM)
    {
        if (InventoryPanelSurfaceRegistryLocal::IsInventoryDragDiagEnabled())
        {
            UE_LOG(
                LogInventoryPanel,
                Warning,
                TEXT("SurfaceRegistry.RegisterPlayerGrids: skipped Subsystem=%d VM=%d Panel=%s"),
                Subsystem ? 1 : 0,
                InventoryVM ? 1 : 0,
                *GetNameSafe(Panel));
        }
        return;
    }

    UUniformGridPanel* GridPanel = Panel->GridPanel;
    UUniformGridPanel* GridPanelSecondary = Panel->GridPanelSecondary;

    // Primary tabbed grid: the active tag comes from the VM's selected
    // container and can change on tab-click, so unregister the old tag
    // before registering the new one.
    const FGameplayTag NewPrimaryTag = InventoryVM->GetSelectedContainerId();
    const bool bHasPrimary = (GridPanel != nullptr)
        && InventoryVM->GetGridWidth() > 0 && InventoryVM->GetGridHeight() > 0;

    if (InventoryPanelSurfaceRegistryLocal::IsInventoryDragDiagEnabled())
    {
        UE_LOG(
            LogInventoryPanel,
            Log,
            TEXT("SurfaceRegistry.RegisterPlayerGrids: PrimaryTag=%s HasPrimary=%d Dims=%dx%d Grid=%s SecondaryTag=%s HasNearby=%d SecondaryDims=%dx%d SecondaryGrid=%s"),
            *NewPrimaryTag.ToString(),
            bHasPrimary ? 1 : 0,
            InventoryVM->GetGridWidth(),
            InventoryVM->GetGridHeight(),
            *GetNameSafe(GridPanel),
            *InventoryVM->GetSecondaryContainerId().ToString(),
            InventoryVM->GetbHasNearbyContainer() ? 1 : 0,
            InventoryVM->GetSecondaryGridWidth(),
            InventoryVM->GetSecondaryGridHeight(),
            *GetNameSafe(GridPanelSecondary));
    }

    if (CachedPrimarySurfaceTag.IsValid() && CachedPrimarySurfaceTag != NewPrimaryTag)
    {
        Subsystem->UnregisterSurface(CachedPrimarySurfaceTag);
        CachedPrimarySurfaceTag = FGameplayTag();
    }

    // Follow-up #2: no widget-side [WeakVM] closures. The subsystem
    // installs Enabled/Occupant/OccupantAllowed checkers that fan out to
    // IInventorySurfacePolicyProvider keyed on SurfaceTag; widgets pass
    // pure data here.
    if (bHasPrimary && NewPrimaryTag.IsValid())
    {
        FProjectUIGridSurface Surface;
        Surface.SurfaceTag = NewPrimaryTag;
        Surface.Grid = GridPanel;
        Surface.Dims = FIntPoint(InventoryVM->GetGridWidth(), InventoryVM->GetGridHeight());
        Surface.Priority = InventoryUISurfacePriority::PlayerStorage;
        Subsystem->RegisterSurface(MoveTemp(Surface));
        CachedPrimarySurfaceTag = NewPrimaryTag;

        if (InventoryPanelSurfaceRegistryLocal::IsInventoryDragDiagEnabled())
        {
            UE_LOG(
                LogInventoryPanel,
                Log,
                TEXT("SurfaceRegistry.RegisterPlayerGrids: registered primary %s"),
                *NewPrimaryTag.ToString());
        }
    }
    else if (!bHasPrimary && CachedPrimarySurfaceTag.IsValid())
    {
        Subsystem->UnregisterSurface(CachedPrimarySurfaceTag);
        CachedPrimarySurfaceTag = FGameplayTag();
    }

    // Secondary tabbed grid (player-side only when nearby is absent).
    const FGameplayTag NewSecondaryTag = InventoryVM->GetSecondaryContainerId();
    const bool bHasSecondary = (GridPanelSecondary != nullptr)
        && InventoryVM->GetSecondaryGridWidth() > 0
        && InventoryVM->GetSecondaryGridHeight() > 0
        && !InventoryVM->GetbHasNearbyContainer();

    if (CachedSecondarySurfaceTag.IsValid() && CachedSecondarySurfaceTag != NewSecondaryTag)
    {
        Subsystem->UnregisterSurface(CachedSecondarySurfaceTag);
        CachedSecondarySurfaceTag = FGameplayTag();
    }

    if (bHasSecondary && NewSecondaryTag.IsValid())
    {
        FProjectUIGridSurface Surface;
        Surface.SurfaceTag = NewSecondaryTag;
        Surface.Grid = GridPanelSecondary;
        Surface.Dims = FIntPoint(InventoryVM->GetSecondaryGridWidth(), InventoryVM->GetSecondaryGridHeight());
        Surface.Priority = InventoryUISurfacePriority::PlayerStorage;
        Subsystem->RegisterSurface(MoveTemp(Surface));
        CachedSecondarySurfaceTag = NewSecondaryTag;

        if (InventoryPanelSurfaceRegistryLocal::IsInventoryDragDiagEnabled())
        {
            UE_LOG(
                LogInventoryPanel,
                Log,
                TEXT("SurfaceRegistry.RegisterPlayerGrids: registered secondary %s"),
                *NewSecondaryTag.ToString());
        }
    }
    else if (!bHasSecondary && CachedSecondarySurfaceTag.IsValid())
    {
        Subsystem->UnregisterSurface(CachedSecondarySurfaceTag);
        CachedSecondarySurfaceTag = FGameplayTag();
    }
}

void FInventoryPanelSurfaceRegistry::UnregisterAll()
{
    UInventoryUIDragHostSubsystem* Subsystem = ResolveDragHostSubsystem();
    if (!Subsystem)
    {
        // Subsystem already gone; drop local bookkeeping so repeat calls
        // are no-ops.
        CachedPrimarySurfaceTag = FGameplayTag();
        CachedSecondarySurfaceTag = FGameplayTag();
        RegisteredPocketSurfaceTags.Reset();
        bHandSurfacesRegistered = false;
        return;
    }

    if (bHandSurfacesRegistered)
    {
        Subsystem->UnregisterSurface(ProjectTags::Item_Container_LeftHand);
        Subsystem->UnregisterSurface(ProjectTags::Item_Container_RightHand);
        bHandSurfacesRegistered = false;
    }
    for (const FGameplayTag& PocketTag : RegisteredPocketSurfaceTags)
    {
        if (PocketTag.IsValid())
        {
            Subsystem->UnregisterSurface(PocketTag);
        }
    }
    RegisteredPocketSurfaceTags.Reset();
    if (CachedPrimarySurfaceTag.IsValid())
    {
        Subsystem->UnregisterSurface(CachedPrimarySurfaceTag);
        CachedPrimarySurfaceTag = FGameplayTag();
    }
    if (CachedSecondarySurfaceTag.IsValid())
    {
        Subsystem->UnregisterSurface(CachedSecondarySurfaceTag);
        CachedSecondarySurfaceTag = FGameplayTag();
    }
}
