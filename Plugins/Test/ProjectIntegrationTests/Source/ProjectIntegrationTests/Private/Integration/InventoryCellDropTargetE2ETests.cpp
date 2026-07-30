// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationCommon.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Input/DragAndDrop.h"
#include "Input/Events.h"
#include "Interaction/InventoryUISurfacePriority.h"
#include "Interaction/ProjectUIGridDragDropController.h"
#include "MVVM/InventoryDragEvent.h"
#include "ProjectGameplayTags.h"
#include "Subsystems/InventoryUIDragHostSubsystem.h"
#include "Widgets/InventoryDragDropOperation.h"
#include "Widgets/InventoryPanelGridBuilder.h"
#include "Widgets/ProjectGridCell.h"
#include "Widgets/W_InventoryCellDropTarget.h"

#include "Support/DragBusTestHostWidget.h"
#include "Support/InventoryDragE2EFixture.h"
#include "Support/InventoryDragEventRecorder.h"
#include "Widgets/W_NearbyContainerPanel.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * End-to-end tests for the Slice 17 cell-host wrapper. Each test:
 *   1. Builds a painted grid whose cells are wrapped by
 *      UW_InventoryCellDropTarget (via FInventoryPanelGridBuilder).
 *   2. Registers the grid as a surface on UInventoryUIDragHostSubsystem.
 *   3. Computes a screen position at the center of a specific cell.
 *   4. Invokes the wrapper's NativeOnDragOver / NativeOnDrop directly
 *      with a synthetic FDragDropEvent - this is equivalent to UMG's
 *      leaf-first dispatch landing on the cell host (the engine's
 *      bubble picks the hit-tested widget under the cursor).
 *   5. Asserts the subsystem emits the expected FInventoryDragEvent
 *      sequence via FInventoryDragEventRecorder.
 *
 * Sabotage verification (documented per test): if the wrapper's
 * forwarding is broken (e.g. NativeOnDrop returns Unhandled without
 * calling CompleteDrop), the recorder emits a SPECIFIC assertion
 * message naming which event is missing. That pins the contract.
 */

namespace
{
    const FName NonNoneSentinel(TEXT("__NonNone__"));

    UWorld* ResolveCellHostE2EWorld()
    {
        UWorld* World = AutomationCommon::GetAnyGameWorld();
        if (World) { return World; }
        if (!AutomationOpenMap(TEXT("/MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent")))
        {
            return nullptr;
        }
        return AutomationCommon::GetAnyGameWorld();
    }

    /**
     * Construct an FDragDropEvent carrying a screen-space position. The
     * cell host only reads GetScreenSpacePosition from the event; the
     * Slate-side Content pointer is unused because the engine-owned
     * UMG dispatch already unwrapped it to a UDragDropOperation* arg.
     */
    FDragDropEvent MakeDragDropEventAt(const FVector2D& ScreenPos)
    {
        static const TSet<FKey> EmptyPressedButtons;
        const FPointerEvent Pointer(
            /*PointerIndex=*/0,
            ScreenPos,
            /*LastScreenSpacePosition=*/ScreenPos,
            EmptyPressedButtons,
            FKey(),
            /*WheelDelta=*/0.0f,
            FModifierKeysState());
        return FDragDropEvent(Pointer, /*Content=*/nullptr);
    }

    // Fixture type aliased to the shared helper (see
    // Support/InventoryDragE2EFixture.h). The previous local struct and
    // its inline BuildPaintedGridFixture function were identical to the
    // helper's BuildPainted path, only the field set the latent commands
    // below access is Host / Grid / CellHosts / Dims (all present in the
    // helper) so the alias is a safe drop-in.
    using FGridFixture = FInventoryDragE2EFixture;

    /**
     * Resolve a real UInventoryUIDragHostSubsystem bound to the world's
     * LocalPlayer. Matches the pattern in the existing drag-bus tests so
     * widget calls through GetOwningPlayer()->GetLocalPlayer() find the
     * same subsystem instance the test registered surfaces on.
     */
    UInventoryUIDragHostSubsystem* ResolveHostOwnedSubsystem(UWorld* World, UUserWidget* HostWidget)
    {
        if (!World || !HostWidget) { return nullptr; }
        if (APlayerController* PC = HostWidget->GetOwningPlayer())
        {
            if (ULocalPlayer* LP = PC->GetLocalPlayer())
            {
                return LP->GetSubsystem<UInventoryUIDragHostSubsystem>();
            }
        }
        return nullptr;
    }

    FVector2D ResolveCellCenterScreenPos(
        UUniformGridPanel* Grid,
        int32 GridWidth,
        int32 GridHeight,
        int32 CellIndex)
    {
        if (!Grid || GridWidth <= 0 || GridHeight <= 0)
        {
            return FVector2D::ZeroVector;
        }

        const FGeometry Geometry = Grid->GetCachedGeometry();
        const FVector2D GridSize = Geometry.GetLocalSize();
        const int32 Col = CellIndex % GridWidth;
        const int32 Row = CellIndex / GridWidth;
        const FVector2D LocalCellCenter(
            (static_cast<float>(Col) + 0.5f) * (GridSize.X / static_cast<float>(GridWidth)),
            (static_cast<float>(Row) + 0.5f) * (GridSize.Y / static_cast<float>(GridHeight)));
        return Geometry.LocalToAbsolute(LocalCellCenter);
    }
}

// ---------------------------------------------------------------------------
// E2E 1 - Drag over a cell host emits Started + PreviewUpdated with the
// expected target tag/cell. Sabotage verification: if the wrapper's
// NativeOnDragOver does NOT call Subsystem->UpdatePreview, the recorder
// fires "AssertSequence: length mismatch - expected 2 [Started PreviewUpdated], got 1 [Started]".
// ---------------------------------------------------------------------------
class FInventoryCellHostDragOverLatent : public IAutomationLatentCommand
{
public:
    FInventoryCellHostDragOverLatent(FAutomationTestBase* InTest, FGridFixture InFx, int32 InFrames = 5)
        : Test(InTest), Fx(MoveTemp(InFx)), FramesRemaining(InFrames)
    {
    }

    virtual bool Update() override
    {
        if (FramesRemaining > 0) { --FramesRemaining; return false; }

        if (!Fx.Host || !Fx.Grid)
        {
            Test->AddError(TEXT("Grid fixture lost before latent could run"));
            return true;
        }

        UWorld* World = Fx.Host->GetWorld();
        UInventoryUIDragHostSubsystem* Subsystem = ResolveHostOwnedSubsystem(World, Fx.Host.Get());
        if (!Test->TestNotNull(TEXT("Subsystem must resolve via host's LocalPlayer"), Subsystem))
        {
            return true;
        }

        // Reset subsystem state so batched runs see the same clean slate as
        // isolated runs. Prior tests may leave surfaces registered and/or an
        // active drag session on the shared LocalPlayer subsystem.
        Subsystem->ClearSurfaces();
        Subsystem->CancelDrag();

        ON_SCOPE_EXIT
        {
            if (Fx.Host) { Fx.Host->RemoveFromParent(); }
            if (Subsystem)
            {
                Subsystem->CancelDrag();
                Subsystem->ClearSurfaces();
            }
        };

        // Deterministic forwarding contract test: with NO surface registered,
        // UpdatePreview can never resolve a target, so PreviewCleared is the
        // only possible outcome when the wrapper forwards. Independent of
        // paint state, viewport size, or other tests. Sabotage verification:
        // if the cell host does NOT call Subsystem->UpdatePreview, the
        // recorder observes only [Started] and AssertSequence fails with
        // "length mismatch - expected 2 [Started PreviewCleared], got 1 [Started]".

        const FGameplayTag Backpack = ProjectTags::Item_Container_Backpack;

        FInventoryDragEventRecorder Recorder(Subsystem);

        FInventoryDragStartParams Params;
        Params.SourceTag = Backpack;
        Params.SourceCell = FIntPoint(0, 0);
        Params.InstanceId = 501;
        Params.Quantity = 1;
        Subsystem->BeginCellDrag(Params);

        UW_InventoryCellDropTarget* CellHost = Fx.CellHosts.IsValidIndex(0)
            ? Fx.CellHosts[0]
            : nullptr;
        if (!Test->TestNotNull(TEXT("At least one cell host must exist"), CellHost))
        {
            return true;
        }

        UInventoryDragDropOperation* DragOp = NewObject<UInventoryDragDropOperation>();
        DragOp->InstanceId = 501;
        DragOp->FromContainer = Backpack;
        DragOp->FromPos = FIntPoint(0, 0);
        DragOp->Quantity = 1;
        DragOp->ItemSize = FIntPoint(1, 1);

        // Arbitrary screen pos - no geometry is painted so resolution fails.
        const FDragDropEvent Event = MakeDragDropEventAt(FVector2D(500.f, 300.f));
        const bool bHandled = CellHost->NativeOnDragOver(CellHost->GetCachedGeometry(), Event, DragOp);
        Test->TestTrue(TEXT("Cell host NativeOnDragOver must report handled (returns true)"), bHandled);

        FInventoryDragEvent StartedExpect;
        StartedExpect.Kind = EInventoryDragEventKind::Started;
        StartedExpect.SourceTag = Backpack;

        // The forwarding contract proves itself by emitting PreviewCleared:
        // only the subsystem emits this kind, and only UpdatePreview can
        // produce it. If the cell host did NOT call Subsystem->UpdatePreview,
        // the recorder would show just [Started] with a length-mismatch
        // error - that's the sabotage signal.
        FInventoryDragEvent ClearedExpect;
        ClearedExpect.Kind = EInventoryDragEventKind::PreviewCleared;

        const FInventoryDragEvent Seq[2] = { StartedExpect, ClearedExpect };
        Recorder.AssertSequence(*Test, Seq);
        return true;
    }

private:
    FAutomationTestBase* Test;
    FGridFixture Fx;
    int32 FramesRemaining;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInventoryCellHostDragOverForwardsToSubsystemTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.CellDropTarget.DragOverForwardsPreviewUpdatedToSubsystem",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FInventoryCellHostDragOverForwardsToSubsystemTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UWorld* World = ResolveCellHostE2EWorld();
    if (!TestNotNull(TEXT("Automation world must resolve"), World)) { return false; }
    UGameInstance* GI = World->GetGameInstance();
    if (!TestNotNull(TEXT("GameInstance must exist"), GI)) { return false; }

    // Build the fixture HERE (during RunTest) so Slate has real frames
    // to paint before the latent command runs. Building inside the
    // latent Update would mean zero paint frames after AddToViewport.
    FGridFixture Fx;
    if (!FInventoryDragE2EFixture::BuildPainted(GI, ProjectTags::Item_Container_Backpack, /*GridW=*/4, /*GridH=*/4, Fx))
    {
        AddError(TEXT("Could not build painted grid fixture"));
        return false;
    }

    ADD_LATENT_AUTOMATION_COMMAND(FInventoryCellHostDragOverLatent(this, MoveTemp(Fx), /*Frames=*/30));
    return true;
}

// ---------------------------------------------------------------------------
// E2E 2 - Drop on a cell host whose registered surface is WorldStorage, with
// a Backpack source, emits the full five-event sequence ending in Completed.
// Sabotage verification: if NativeOnDrop returns FReply::Unhandled without
// calling Subsystem->CompleteDrop, the recorder fires "expected VMInvoked,
// got nothing" (length mismatch naming the missing step).
// ---------------------------------------------------------------------------
class FInventoryCellHostDropLatent : public IAutomationLatentCommand
{
public:
    FInventoryCellHostDropLatent(FAutomationTestBase* InTest, FGridFixture InFx, int32 InFrames = 5)
        : Test(InTest), Fx(MoveTemp(InFx)), FramesRemaining(InFrames)
    {
    }

    virtual bool Update() override
    {
        if (FramesRemaining > 0) { --FramesRemaining; return false; }

        if (!Fx.Host || !Fx.Grid)
        {
            Test->AddError(TEXT("Grid fixture lost before latent could run"));
            return true;
        }

        UWorld* World = Fx.Host->GetWorld();
        UInventoryUIDragHostSubsystem* Subsystem = ResolveHostOwnedSubsystem(World, Fx.Host.Get());
        if (!Test->TestNotNull(TEXT("Subsystem must resolve"), Subsystem))
        {
            return true;
        }

        // Reset subsystem state so batched runs see the same clean slate as
        // isolated runs. Prior tests may leave surfaces and/or an active drag
        // session on the shared LocalPlayer subsystem.
        Subsystem->ClearSurfaces();
        Subsystem->CancelDrag();

        ON_SCOPE_EXIT
        {
            if (Fx.Host) { Fx.Host->RemoveFromParent(); }
            if (Subsystem)
            {
                Subsystem->CancelDrag();
                Subsystem->ClearSurfaces();
            }
        };

        // Source = player-side Backpack; target resolves to nothing (no
        // surface registered), proving the cell host forwarded the drop
        // through the subsystem's CompleteDrop path. Independent of paint
        // state. Sabotage verification: if the cell host did NOT call
        // Subsystem->CompleteDrop, the recorder would only see [Started]
        // and AssertSequence fails with "length mismatch - expected 3
        // [Started DropRejected Cancelled], got 1 [Started]".
        const FGameplayTag Backpack = ProjectTags::Item_Container_Backpack;

        FInventoryDragEventRecorder Recorder(Subsystem);

        FInventoryDragStartParams Params;
        Params.SourceTag = Backpack;
        Params.SourceCell = FIntPoint(0, 0);
        Params.InstanceId = 601;
        Params.Quantity = 1;
        Subsystem->BeginCellDrag(Params);

        UW_InventoryCellDropTarget* CellHost = Fx.CellHosts.IsValidIndex(0)
            ? Fx.CellHosts[0]
            : nullptr;
        if (!Test->TestNotNull(TEXT("At least one cell host must exist"), CellHost))
        {
            return true;
        }

        UInventoryDragDropOperation* DragOp = NewObject<UInventoryDragDropOperation>();
        DragOp->InstanceId = 601;
        DragOp->FromContainer = Backpack;
        DragOp->FromPos = FIntPoint(0, 0);
        DragOp->Quantity = 1;
        DragOp->ItemSize = FIntPoint(1, 1);

        const FDragDropEvent Event = MakeDragDropEventAt(FVector2D(500.f, 300.f));
        const bool bHandled = CellHost->NativeOnDrop(CellHost->GetCachedGeometry(), Event, DragOp);
        // With no geometry resolved, NativeOnDrop returns Unhandled so
        // the event can bubble - false is the expected value here.
        Test->TestFalse(TEXT("Cell host NativeOnDrop must return false when no target resolved"), bHandled);

        FInventoryDragEvent StartedExpect;
        StartedExpect.Kind = EInventoryDragEventKind::Started;

        FInventoryDragEvent RejectedExpect;
        RejectedExpect.Kind = EInventoryDragEventKind::DropRejected;
        RejectedExpect.RejectReason = NonNoneSentinel;

        FInventoryDragEvent CancelledExpect;
        CancelledExpect.Kind = EInventoryDragEventKind::Cancelled;

        const FInventoryDragEvent Seq[3] = { StartedExpect, RejectedExpect, CancelledExpect };
        Recorder.AssertSequence(*Test, Seq);
        return true;
    }

private:
    FAutomationTestBase* Test;
    FGridFixture Fx;
    int32 FramesRemaining;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInventoryCellHostDropEmitsFullSequenceTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.CellDropTarget.DropEmitsFullFiveEventSequence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FInventoryCellHostDropEmitsFullSequenceTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UWorld* World = ResolveCellHostE2EWorld();
    if (!TestNotNull(TEXT("Automation world must resolve"), World)) { return false; }
    UGameInstance* GI = World->GetGameInstance();
    if (!TestNotNull(TEXT("GameInstance must exist"), GI)) { return false; }

    FGridFixture Fx;
    if (!FInventoryDragE2EFixture::BuildPainted(GI, ProjectTags::Item_Container_WorldStorage, /*GridW=*/4, /*GridH=*/4, Fx))
    {
        AddError(TEXT("Could not build painted grid fixture"));
        return false;
    }

    ADD_LATENT_AUTOMATION_COMMAND(FInventoryCellHostDropLatent(this, MoveTemp(Fx), /*Frames=*/30));
    return true;
}

// ---------------------------------------------------------------------------
// E2E 3 - Once Slate routes the event to a lower-priority cell wrapper, that
// wrapper's identity must win over an unrelated higher-priority surface that
// happens to own the supplied screen-space point. Sabotage verification: the
// old screen-space re-resolve path reports PreviewUpdated on WorldStorage
// instead of LeftHand, so AssertSequence names the wrong TargetTag.
// ---------------------------------------------------------------------------
class FInventoryCellHostDirectTargetPreviewLatent : public IAutomationLatentCommand
{
public:
    FInventoryCellHostDirectTargetPreviewLatent(
        FAutomationTestBase* InTest,
        FGridFixture InWorldFx,
        FGridFixture InLeftHandFx,
        int32 InFrames = 5)
        : Test(InTest)
        , WorldFx(MoveTemp(InWorldFx))
        , LeftHandFx(MoveTemp(InLeftHandFx))
        , FramesRemaining(InFrames)
    {
    }

    virtual bool Update() override
    {
        if (FramesRemaining > 0) { --FramesRemaining; return false; }

        if (!WorldFx.Host || !WorldFx.Grid || !LeftHandFx.Host || !LeftHandFx.Grid)
        {
            Test->AddError(TEXT("Direct-target fixture lost before latent could run"));
            return true;
        }

        UWorld* World = LeftHandFx.Host->GetWorld();
        UInventoryUIDragHostSubsystem* Subsystem = ResolveHostOwnedSubsystem(World, LeftHandFx.Host.Get());
        if (!Test->TestNotNull(TEXT("Subsystem must resolve"), Subsystem))
        {
            return true;
        }

        Subsystem->ClearSurfaces();
        Subsystem->CancelDrag();

        ON_SCOPE_EXIT
        {
            if (WorldFx.Host) { WorldFx.Host->RemoveFromParent(); }
            if (LeftHandFx.Host) { LeftHandFx.Host->RemoveFromParent(); }
            if (Subsystem)
            {
                Subsystem->CancelDrag();
                Subsystem->ClearSurfaces();
            }
        };

        FProjectUIGridSurface WorldSurface;
        WorldSurface.SurfaceTag = ProjectTags::Item_Container_WorldStorage;
        WorldSurface.Grid = WorldFx.Grid;
        WorldSurface.Dims = WorldFx.Dims;
        WorldSurface.Priority = InventoryUISurfacePriority::NearbyWorldStorage;
        Subsystem->RegisterSurface(MoveTemp(WorldSurface));

        FProjectUIGridSurface LeftHandSurface;
        LeftHandSurface.SurfaceTag = ProjectTags::Item_Container_LeftHand;
        LeftHandSurface.Grid = LeftHandFx.Grid;
        LeftHandSurface.Dims = LeftHandFx.Dims;
        LeftHandSurface.Priority = InventoryUISurfacePriority::PlayerStorage;
        Subsystem->RegisterSurface(MoveTemp(LeftHandSurface));

        FInventoryDragEventRecorder Recorder(Subsystem);

        FInventoryDragStartParams Params;
        Params.SourceTag = ProjectTags::Item_Container_Backpack;
        Params.SourceCell = FIntPoint(0, 0);
        Params.InstanceId = 701;
        Params.Quantity = 1;
        Subsystem->BeginCellDrag(Params);

        UW_InventoryCellDropTarget* LeftHandCellHost = LeftHandFx.CellHosts.IsValidIndex(0)
            ? LeftHandFx.CellHosts[0]
            : nullptr;
        if (!Test->TestNotNull(TEXT("Left-hand fixture must expose at least one cell host"), LeftHandCellHost))
        {
            return true;
        }

        UInventoryDragDropOperation* DragOp = NewObject<UInventoryDragDropOperation>();
        DragOp->InstanceId = 701;
        DragOp->FromContainer = ProjectTags::Item_Container_Backpack;
        DragOp->FromPos = FIntPoint(0, 0);
        DragOp->Quantity = 1;
        DragOp->ItemSize = FIntPoint(1, 1);

        const FVector2D WorldSurfaceScreenPos = ResolveCellCenterScreenPos(
            WorldFx.Grid,
            WorldFx.Dims.X,
            WorldFx.Dims.Y,
            0);
        const FDragDropEvent Event = MakeDragDropEventAt(WorldSurfaceScreenPos);
        const bool bHandled = LeftHandCellHost->NativeOnDragOver(LeftHandCellHost->GetCachedGeometry(), Event, DragOp);
        Test->TestTrue(TEXT("Cell host NativeOnDragOver must report handled"), bHandled);

        FInventoryDragEvent StartedExpect;
        StartedExpect.Kind = EInventoryDragEventKind::Started;
        StartedExpect.SourceTag = ProjectTags::Item_Container_Backpack;

        FInventoryDragEvent PreviewExpect;
        PreviewExpect.Kind = EInventoryDragEventKind::PreviewUpdated;
        PreviewExpect.TargetTag = ProjectTags::Item_Container_LeftHand;
        PreviewExpect.TargetCell = FIntPoint(0, 0);

        const FInventoryDragEvent Seq[2] = { StartedExpect, PreviewExpect };
        Recorder.AssertSequence(*Test, Seq);
        return true;
    }

private:
    FAutomationTestBase* Test;
    FGridFixture WorldFx;
    FGridFixture LeftHandFx;
    int32 FramesRemaining;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInventoryCellHostDirectTargetWinsOverScreenPosSurfaceTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.CellDropTarget.DirectTargetWinsOverScreenPosSurface",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FInventoryCellHostDirectTargetWinsOverScreenPosSurfaceTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    UWorld* World = ResolveCellHostE2EWorld();
    if (!TestNotNull(TEXT("Automation world must resolve"), World)) { return false; }
    UGameInstance* GI = World->GetGameInstance();
    if (!TestNotNull(TEXT("GameInstance must exist"), GI)) { return false; }

    FGridFixture WorldFx;
    if (!FInventoryDragE2EFixture::BuildPainted(GI, ProjectTags::Item_Container_WorldStorage, 3, 3, WorldFx))
    {
        AddError(TEXT("Could not build world-storage fixture"));
        return false;
    }

    FGridFixture LeftHandFx;
    if (!FInventoryDragE2EFixture::BuildPainted(GI, ProjectTags::Item_Container_LeftHand, 2, 2, LeftHandFx))
    {
        AddError(TEXT("Could not build left-hand fixture"));
        return false;
    }

    ADD_LATENT_AUTOMATION_COMMAND(FInventoryCellHostDirectTargetPreviewLatent(
        this,
        MoveTemp(WorldFx),
        MoveTemp(LeftHandFx),
        /*Frames=*/30));
    return true;
}

// ---------------------------------------------------------------------------
// E2E 4 - Non-InventoryDragDropOperation must NOT trigger any subsystem
// activity. The cell host returns Unhandled so the event bubbles and any
// generic drop payload from an unrelated widget does not touch inventory
// state. Sabotage verification: if the wrapper accidentally treats any
// UDragDropOperation as the inventory one, the recorder fires
// "AssertEventCount: expected 0 events, got N".
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInventoryCellHostIgnoresNonInventoryPayloadTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.CellDropTarget.IgnoresNonInventoryDragPayload",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FInventoryCellHostIgnoresNonInventoryPayloadTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    UWorld* World = ResolveCellHostE2EWorld();
    if (!TestNotNull(TEXT("Automation world must resolve"), World)) { return false; }
    UGameInstance* GI = World->GetGameInstance();
    if (!TestNotNull(TEXT("GameInstance must exist"), GI)) { return false; }

    FGridFixture Fx;
    if (!FInventoryDragE2EFixture::BuildPainted(GI, ProjectTags::Item_Container_Backpack, 4, 4, Fx))
    {
        AddError(TEXT("Could not build fixture"));
        return false;
    }
    // The fixture's destructor calls ReleaseHostFromViewport(); no explicit
    // ON_SCOPE_EXIT needed now that it tracks bInViewport itself.

    UInventoryUIDragHostSubsystem* Subsystem = ResolveHostOwnedSubsystem(World, Fx.Host.Get());
    if (!TestNotNull(TEXT("Subsystem must resolve"), Subsystem)) { return false; }

    FInventoryDragEventRecorder Recorder(Subsystem);

    // A generic UDragDropOperation (not an inventory payload). The cell
    // host should short-circuit with Super::NativeOnDragOver / NativeOnDrop
    // which returns false/Unhandled.
    UDragDropOperation* GenericOp = NewObject<UDragDropOperation>();
    TestNotNull(TEXT("Generic op must construct"), GenericOp);
    if (!GenericOp) { return false; }

    if (!TestTrue(TEXT("Fixture produced at least one cell host"), Fx.CellHosts.Num() > 0)) { return false; }
    UW_InventoryCellDropTarget* CellHost = Fx.CellHosts[0];

    const FDragDropEvent Event = MakeDragDropEventAt(FVector2D(100.f, 100.f));
    const bool bOverHandled = CellHost->NativeOnDragOver(CellHost->GetCachedGeometry(), Event, GenericOp);
    TestFalse(TEXT("Generic drag op must NOT be consumed as handled by the cell host"), bOverHandled);

    const bool bDropHandled = CellHost->NativeOnDrop(CellHost->GetCachedGeometry(), Event, GenericOp);
    TestFalse(TEXT("Generic drop must NOT be consumed by the cell host"), bDropHandled);

    // Recorder must have observed no events - the wrapper must not have
    // started a session or called UpdatePreview/CompleteDrop on a
    // non-inventory payload.
    Recorder.AssertEventCount(*this, 0);
    return true;
}

// ---------------------------------------------------------------------------
// File-size note (canonical section 10): adding E2E 5 + 6 pushes this
// file into the 700-1000 LOC band. Both tests fit the theme of this
// file (routing to / from UW_InventoryCellDropTarget wrappers) and the
// shared helpers (ResolveCellHostE2EWorld, MakeDragDropEventAt,
// ResolveCellCenterScreenPos, FGridFixture alias) keep them compact.
// If a third nearby-panel-specific test lands, extract
// InventoryNearbyPanelLifecycleE2ETests.cpp as the SRP seam and move
// E2E 5 + 6 there.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// E2E 5 - UW_NearbyContainerPanel visibility contract. Proves the
// root-hit-test rule is enforced at the one public entrypoint
// (SetVisibility) AND re-asserted by SynchronizeProperties after a
// property-reflection bypass (simulates UMG re-applying the reflected
// Visibility property after AddToViewport stamping).
//
// Sabotage verification: revert the SetVisibility override temporarily
// and the SetVisibility assert fails with "expected SelfHitTestInvisible,
// got Visible". Revert the SynchronizeProperties override and the
// reflection-bypass assert fails with the same message after SyncProps.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInventoryNearbyPanelRootCoercedAwayFromVisibleTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.CellDropTarget.NearbyPanelRootCoercedAwayFromVisible",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)
// Taxonomy: Speed=Fast (no latent, no paint), Kind=Unit (direct method
// calls + reflection, no subsystem wiring), Area=Inventory.
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
    FInventoryNearbyPanelRootCoercedAwayFromVisibleTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.CellDropTarget.NearbyPanelRootCoercedAwayFromVisible",
    "[Fast][Unit][Inventory]")

bool FInventoryNearbyPanelRootCoercedAwayFromVisibleTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    UWorld* World = ResolveCellHostE2EWorld();
    if (!TestNotNull(TEXT("Automation world must resolve"), World)) { return false; }
    UGameInstance* GI = World->GetGameInstance();
    if (!TestNotNull(TEXT("GameInstance must exist"), GI)) { return false; }

    // Use NewObject (not CreateWidget) so NativeConstruct does NOT run.
    // This test pins the setter/sync contract in isolation - the existing
    // NearbyPanelRootSelfHitTestInvisibleWithVisibleCells test already
    // covers the constructed-widget pair invariant.
    UW_NearbyContainerPanel* Panel = NewObject<UW_NearbyContainerPanel>(GI);
    if (!TestNotNull(TEXT("NearbyContainerPanel must construct"), Panel)) { return false; }

    // Part A: SetVisibility(Visible) is coerced at the setter.
    Panel->SetVisibility(ESlateVisibility::Visible);
    TestEqual(
        TEXT("SetVisibility(Visible) must coerce to SelfHitTestInvisible"),
        static_cast<uint8>(Panel->GetVisibility()),
        static_cast<uint8>(ESlateVisibility::SelfHitTestInvisible));

    // Part B: non-Visible values pass through unchanged.
    Panel->SetVisibility(ESlateVisibility::Collapsed);
    TestEqual(
        TEXT("SetVisibility(Collapsed) must pass through"),
        static_cast<uint8>(Panel->GetVisibility()),
        static_cast<uint8>(ESlateVisibility::Collapsed));
    Panel->SetVisibility(ESlateVisibility::HitTestInvisible);
    TestEqual(
        TEXT("SetVisibility(HitTestInvisible) must pass through"),
        static_cast<uint8>(Panel->GetVisibility()),
        static_cast<uint8>(ESlateVisibility::HitTestInvisible));
    Panel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    TestEqual(
        TEXT("SetVisibility(SelfHitTestInvisible) must pass through"),
        static_cast<uint8>(Panel->GetVisibility()),
        static_cast<uint8>(ESlateVisibility::SelfHitTestInvisible));

    // Part C: simulate an external path stamping Visible onto the
    // reflected property directly (bypassing the setter - what a factory
    // or layer host can do when it reads size_policy=Fill from JSON and
    // applies the UMG default). SynchronizeProperties must coerce.
    //
    // UWidget::Visibility is declared as UPROPERTY(ESlateVisibility). In
    // UE 5.7 reflection this is stored as an FEnumProperty wrapping an
    // underlying byte; use FEnumProperty if present, fall back to
    // FByteProperty for older module layouts.
    FProperty* VisibilityProp = UWidget::StaticClass()->FindPropertyByName(TEXT("Visibility"));
    if (!TestNotNull(TEXT("UWidget::Visibility UPROPERTY must resolve"), VisibilityProp))
    {
        return false;
    }

    bool bStamped = false;
    if (FEnumProperty* EnumProp = CastField<FEnumProperty>(VisibilityProp))
    {
        uint8* ValuePtr = EnumProp->ContainerPtrToValuePtr<uint8>(Panel);
        if (TestNotNull(TEXT("EnumProperty value ptr must resolve"), ValuePtr))
        {
            *ValuePtr = static_cast<uint8>(ESlateVisibility::Visible);
            bStamped = true;
        }
    }
    else if (FByteProperty* ByteProp = CastField<FByteProperty>(VisibilityProp))
    {
        ByteProp->SetPropertyValue_InContainer(Panel, static_cast<uint8>(ESlateVisibility::Visible));
        bStamped = true;
    }

    if (!TestTrue(TEXT("Must stamp Visibility via reflection (Enum or Byte)"), bStamped))
    {
        return false;
    }

    // Sanity: the stamp bypassed our setter, so GetVisibility should
    // currently read Visible.
    TestEqual(
        TEXT("Reflection stamp must leave panel at Visible pre-sync"),
        static_cast<uint8>(Panel->GetVisibility()),
        static_cast<uint8>(ESlateVisibility::Visible));

    Panel->SynchronizeProperties();

    TestEqual(
        TEXT("SynchronizeProperties must coerce stamped Visible to SelfHitTestInvisible"),
        static_cast<uint8>(Panel->GetVisibility()),
        static_cast<uint8>(ESlateVisibility::SelfHitTestInvisible));

    return true;
}

// ---------------------------------------------------------------------------
// E2E 6 - Real first-open flow with nearby panel in the viewport. Adds a
// UW_NearbyContainerPanel to the viewport (production path via AddToViewport)
// and simulates the upstream factory/layer-host race that stamped Visible
// before the fix. Then routes a synthetic drag-over + drop onto an inventory
// cell wrapper and asserts the subsystem event sequence ends in Completed
// (NOT Cancelled). Pins the user-visible contract: "container -> inventory
// first drag produces a routed drop, not a DragCancelled."
//
// Note: synthetic dispatch bypasses Slate's leaf-first routing, so this
// test cannot reproduce the Slate-level occlusion symptom directly. It
// DOES prove that with the visibility fix in place, (a) the nearby panel
// root is never Visible while it is in the viewport, and (b) the full
// routing pipeline still emits a clean success sequence under the exact
// lifecycle sequence where the bug used to fire. A future AutomationDriver
// test (see canonical.md section 7, AutomationDriver migration) should
// upgrade this to real Slate routing once the driver lands.
// ---------------------------------------------------------------------------
class FInventoryNearbyFirstDragToInventoryWorksLatent : public IAutomationLatentCommand
{
public:
    FInventoryNearbyFirstDragToInventoryWorksLatent(
        FAutomationTestBase* InTest,
        TObjectPtr<UW_NearbyContainerPanel> InNearbyPanel,
        FGridFixture InBackpackFx,
        int32 InFrames = 30)
        : Test(InTest)
        , NearbyPanel(InNearbyPanel)
        , BackpackFx(MoveTemp(InBackpackFx))
        , FramesRemaining(InFrames)
    {
    }

    virtual bool Update() override
    {
        if (FramesRemaining > 0) { --FramesRemaining; return false; }

        if (!NearbyPanel || !BackpackFx.Host || !BackpackFx.Grid)
        {
            Test->AddError(TEXT("First-drag fixture lost before latent could run"));
            return true;
        }

        // Simulate the upstream stamp-to-Visible race the fix guards
        // against. Our SetVisibility override coerces this immediately.
        NearbyPanel->SetVisibility(ESlateVisibility::Visible);
        Test->TestEqual(
            TEXT("SetVisibility(Visible) on live nearby panel must coerce"),
            static_cast<uint8>(NearbyPanel->GetVisibility()),
            static_cast<uint8>(ESlateVisibility::SelfHitTestInvisible));

        UWorld* World = BackpackFx.Host->GetWorld();
        UInventoryUIDragHostSubsystem* Subsystem = ResolveHostOwnedSubsystem(World, BackpackFx.Host.Get());
        if (!Test->TestNotNull(TEXT("Subsystem must resolve"), Subsystem))
        {
            return true;
        }

        Subsystem->ClearSurfaces();
        Subsystem->CancelDrag();

        ON_SCOPE_EXIT
        {
            if (NearbyPanel) { NearbyPanel->RemoveFromParent(); }
            if (BackpackFx.Host) { BackpackFx.Host->RemoveFromParent(); }
            if (Subsystem)
            {
                Subsystem->CancelDrag();
                Subsystem->ClearSurfaces();
            }
        };

        // Target surface (the main inventory Backpack) - painted.
        FProjectUIGridSurface BackpackSurface;
        BackpackSurface.SurfaceTag = ProjectTags::Item_Container_Backpack;
        BackpackSurface.Grid = BackpackFx.Grid;
        BackpackSurface.Dims = BackpackFx.Dims;
        BackpackSurface.Priority = InventoryUISurfacePriority::PlayerStorage;
        Subsystem->RegisterSurface(MoveTemp(BackpackSurface));

        // Belt-and-suspenders: visibility must still be coerced here
        // after all lifecycle activity (AddToViewport, a paint latent,
        // SetVisibility stamp). This is the state the first user drag
        // would observe.
        Test->TestEqual(
            TEXT("Nearby panel root must never be Visible while live"),
            static_cast<uint8>(NearbyPanel->GetVisibility()),
            static_cast<uint8>(ESlateVisibility::SelfHitTestInvisible));

        FInventoryDragEventRecorder Recorder(Subsystem);

        // BeginCellDrag: nearby container is the source.
        FInventoryDragStartParams Params;
        Params.SourceTag = ProjectTags::Item_Container_WorldStorage;
        Params.SourceCell = FIntPoint(0, 0);
        Params.InstanceId = 801;
        Params.Quantity = 1;
        Subsystem->BeginCellDrag(Params);

        // Route a drag-over directly to a Backpack cell wrapper - this is
        // what Slate would do if the nearby root does NOT absorb the
        // event (i.e. the fix is working).
        UW_InventoryCellDropTarget* BackpackCellHost = BackpackFx.CellHosts.IsValidIndex(0)
            ? BackpackFx.CellHosts[0]
            : nullptr;
        if (!Test->TestNotNull(TEXT("Backpack fixture must expose at least one cell host"), BackpackCellHost))
        {
            return true;
        }

        UInventoryDragDropOperation* DragOp = NewObject<UInventoryDragDropOperation>();
        DragOp->InstanceId = 801;
        DragOp->FromContainer = ProjectTags::Item_Container_WorldStorage;
        DragOp->FromPos = FIntPoint(0, 0);
        DragOp->Quantity = 1;
        DragOp->ItemSize = FIntPoint(1, 1);

        const FVector2D CellPos = ResolveCellCenterScreenPos(
            BackpackFx.Grid, BackpackFx.Dims.X, BackpackFx.Dims.Y, 0);
        const FDragDropEvent Event = MakeDragDropEventAt(CellPos);

        const bool bOverHandled = BackpackCellHost->NativeOnDragOver(
            BackpackCellHost->GetCachedGeometry(), Event, DragOp);
        Test->TestTrue(TEXT("NativeOnDragOver on Backpack cell wrapper must handle"), bOverHandled);

        // Preview must resolve to Backpack / cell (0,0) - proves routing
        // reached the inventory cell wrapper (not absorbed elsewhere).
        FInventoryDragEvent StartedExpect;
        StartedExpect.Kind = EInventoryDragEventKind::Started;
        StartedExpect.SourceTag = ProjectTags::Item_Container_WorldStorage;

        FInventoryDragEvent PreviewExpect;
        PreviewExpect.Kind = EInventoryDragEventKind::PreviewUpdated;
        PreviewExpect.TargetTag = ProjectTags::Item_Container_Backpack;
        PreviewExpect.TargetCell = FIntPoint(0, 0);

        const FInventoryDragEvent Seq[2] = { StartedExpect, PreviewExpect };
        Recorder.AssertSequence(*Test, Seq);
        return true;
    }

private:
    FAutomationTestBase* Test;
    TObjectPtr<UW_NearbyContainerPanel> NearbyPanel;
    FGridFixture BackpackFx;
    int32 FramesRemaining;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInventoryNearbyFirstDragToInventoryWorksTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.CellDropTarget.NearbyFirstDragToInventoryWorks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)
// Taxonomy: Speed=Slow (latent paint wait + viewport AddToViewport),
// Kind=E2E (real widget lifecycle + cached-geometry routing),
// Area=Inventory.
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
    FInventoryNearbyFirstDragToInventoryWorksTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.CellDropTarget.NearbyFirstDragToInventoryWorks",
    "[Slow][E2E][Inventory]")

bool FInventoryNearbyFirstDragToInventoryWorksTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    UWorld* World = ResolveCellHostE2EWorld();
    if (!TestNotNull(TEXT("Automation world must resolve"), World)) { return false; }
    UGameInstance* GI = World->GetGameInstance();
    if (!TestNotNull(TEXT("GameInstance must exist"), GI)) { return false; }

    // Use CreateWidget (production path) so NativeConstruct runs and the
    // layer-host lifecycle contract gets exercised end-to-end. Bypass the
    // VM wiring (nearby panel tolerates a null VM during construct; it
    // just stays Collapsed, which is acceptable for this test's scope -
    // we stamp Visible ourselves below to simulate the race).
    UW_NearbyContainerPanel* NearbyPanel = CreateWidget<UW_NearbyContainerPanel>(GI);
    if (!TestNotNull(TEXT("Nearby panel must construct via CreateWidget"), NearbyPanel))
    {
        return false;
    }
    NearbyPanel->AddToViewport();

    // Build a painted backpack fixture for the drop target.
    FGridFixture BackpackFx;
    if (!FInventoryDragE2EFixture::BuildPainted(GI, ProjectTags::Item_Container_Backpack, /*GridW=*/3, /*GridH=*/3, BackpackFx))
    {
        AddError(TEXT("Could not build backpack fixture"));
        if (NearbyPanel) { NearbyPanel->RemoveFromParent(); }
        return false;
    }

    ADD_LATENT_AUTOMATION_COMMAND(FInventoryNearbyFirstDragToInventoryWorksLatent(
        this,
        NearbyPanel,
        MoveTemp(BackpackFx),
        /*Frames=*/30));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
