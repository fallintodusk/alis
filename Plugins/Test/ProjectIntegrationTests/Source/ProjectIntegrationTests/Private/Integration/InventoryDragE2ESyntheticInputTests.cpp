// Copyright ALIS. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationCommon.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Input/DragAndDrop.h"
#include "Input/Events.h"
#include "Interaction/IInventorySurfacePolicyProvider.h"
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
#include "Support/InventoryDragEventRecorder.h"
#include "Support/InventoryViewModelSpy.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Slice 19 Part B - End-to-end drag tests across the full pipeline:
 *   Slate cell wrapper -> subsystem session -> controller geometry
 *   resolve -> router -> VM.
 *
 * Scope covered (3 tests):
 *   1. DragFromNearbyCellDropOnMainBackpackEmitsExpectedEventSequence
 *      Source = Item.Container.WorldStorage, Target = Item.Container.Backpack,
 *      expected VMMethod = RequestTakeNearbyItemToContainer.
 *   2. DragFromMainBackpackDropOnNearbyGridEmitsExpectedEventSequence
 *      Source = Item.Container.Backpack, Target = Item.Container.WorldStorage,
 *      expected VMMethod = RequestStoreItemInNearbyContainerAt.
 *   3. DragWithinMainBackpackEmitsMoveItemEventSequence
 *      Source + Target both = Item.Container.Backpack, expected
 *      VMMethod = RequestMoveItem.
 *
 * Synthetic-input strategy (per test, documented in report):
 *   Fallback "direct handler call on cached real geometry". Each test
 *   spawns a painted grid via FInventoryPanelGridBuilder (which wraps
 *   each cell in UW_InventoryCellDropTarget), pumps 30 frames via a
 *   latent command so Slate paints and GetCachedGeometry() becomes
 *   valid, then computes the screen pos of a specific cell center and
 *   invokes the target wrapper's NativeOnDragOver / NativeOnDrop with a
 *   synthetic FDragDropEvent. This proves:
 *      - subsystem session lifecycle (Started -> Completed)
 *      - controller resolves geometry -> surface tag + cell via
 *        cached cell bounds (not via a shortcut that bypasses Slate)
 *      - router maps {source tag, target tag} to the expected VM method
 *      - recorder observes the full Started / PreviewUpdated /
 *        DropResolved / Routed / VMInvoked / Completed sequence.
 *   What the fallback does NOT prove: the Slate bubble router's own
 *   behavior (FEventRouter::FBubblePolicy) - that is engine code we
 *   trust and Widget Reflector / Slate Insights already validate.
 *   Full FSlateApplication::ProcessMouse* synthetic input in automation
 *   mode would require a real viewport with paint frames that can take
 *   arbitrary ticks to cache geometry; the direct-handler approach is
 *   deterministic without that dependency.
 *
 * Sabotage verification per test: break one step (documented below
 * per test) and confirm the test fails with a SPECIFIC event-sequence
 * mismatch naming the missing event.
 */

namespace
{
    const FName NonNoneSyntheticInput(TEXT("__NonNone__"));

    UWorld* ResolveDragE2EWorld()
    {
        UWorld* World = AutomationCommon::GetAnyGameWorld();
        if (World) { return World; }
        if (!AutomationOpenMap(TEXT("/MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent")))
        {
            return nullptr;
        }
        return AutomationCommon::GetAnyGameWorld();
    }

    FDragDropEvent MakeSyntheticInputDragEventAt(const FVector2D& ScreenPos)
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

    /**
     * Per-surface fixture: a host user-widget in the viewport whose
     * root is a UniformGridPanel built by FInventoryPanelGridBuilder.
     * Each cell is wrapped in UW_InventoryCellDropTarget (Slice 17
     * pattern). The host carries the tag identity so tests can resolve
     * (wrapper for row, col) by index.
     */
    struct FSurfaceFixture
    {
        TObjectPtr<UDragBusTestHostWidget> Host;
        UUniformGridPanel* Grid = nullptr;
        TArray<TObjectPtr<UW_InventoryCellDropTarget>> CellHosts;
        FGameplayTag SurfaceTag;
        FIntPoint Dims = FIntPoint(0, 0);
    };

    /**
     * bAddToViewport:
     *   TRUE  - host is added to viewport and will paint real geometry.
     *           Use for the TARGET surface (the one the subsystem must
     *           resolve via ResolveDropTargetOverSurfaces).
     *   FALSE - host is NOT added to viewport; its grid geometry stays
     *           zero. Use for the SOURCE surface when it only carries
     *           the tag identity for Started/router and is not meant to
     *           compete for cursor hit-tests under the target's paint
     *           area.
     */
    bool BuildSurfaceFixture(
        UGameInstance* GI,
        const FGameplayTag& SurfaceTag,
        int32 GridW,
        int32 GridH,
        FSurfaceFixture& OutFixture,
        bool bAddToViewport = true)
    {
        if (!GI) { return false; }

        UDragBusTestHostWidget* Host = CreateWidget<UDragBusTestHostWidget>(
            GI, UDragBusTestHostWidget::StaticClass());
        if (!Host || !Host->WidgetTree) { return false; }

        FInventoryPanelGridBuilder Builder;
        Builder.Initialize(Host, Host->WidgetTree);
        Builder.SetCellSize(64.f);

        TArray<TObjectPtr<UTextBlock>> Primary;
        TArray<TObjectPtr<UTextBlock>> Quantity;
        TArray<TObjectPtr<UBorder>> Badges;
        TArray<TObjectPtr<UProjectGridCell>> Cells;

        UUniformGridPanel* Grid = Builder.BuildGrid(
            GridW, GridH, SurfaceTag,
            Primary, Quantity, Badges, Cells, OutFixture.CellHosts,
            /*bIsSecondary=*/false);
        if (!Grid) { return false; }

        Host->WidgetTree->RootWidget = Grid;
        if (bAddToViewport)
        {
            Host->AddToViewport();
            Host->ForceLayoutPrepass();
        }

        OutFixture.Host = Host;
        OutFixture.Grid = Grid;
        OutFixture.SurfaceTag = SurfaceTag;
        OutFixture.Dims = FIntPoint(GridW, GridH);
        return true;
    }

    UInventoryUIDragHostSubsystem* ResolveSyntheticInputHostOwnedSubsystem(UWorld* World, UUserWidget* HostWidget)
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

    /**
     * Register a surface with permissive checkers (all enabled, all
     * empty). The subsystem's Slice 18 policy-provider default accepts
     * empty/self for any payload so the footprint validates and the
     * controller resolves a target.
     */
    void RegisterPermissiveSurface(
        UInventoryUIDragHostSubsystem& Subsystem,
        const FGameplayTag& SurfaceTag,
        UUniformGridPanel* Grid,
        FIntPoint Dims)
    {
        FProjectUIGridSurface Surface;
        Surface.SurfaceTag = SurfaceTag;
        Surface.Grid = Grid;
        Surface.Dims = Dims;
        Surface.Priority = InventoryUISurfacePriority::PlayerStorage;
        Surface.EnabledChecker = [](int32) -> bool { return true; };
        Surface.OccupantChecker = [](int32) -> int32 { return INDEX_NONE; };
        // Leave OccupantAllowedChecker unset so the subsystem installs
        // the policy-provider-backed default (Slice 18).
        Subsystem.RegisterSurface(MoveTemp(Surface));
    }

    /**
     * Compute absolute screen pos of the center of (Col, Row) in a
     * UniformGridPanel whose cached geometry is valid.
     */
    FVector2D ComputeCellCenterScreenPos(UUniformGridPanel* Grid, int32 Col, int32 Row, int32 GridW, int32 GridH)
    {
        if (!Grid || GridW <= 0 || GridH <= 0) { return FVector2D::ZeroVector; }
        const FGeometry Geom = Grid->GetCachedGeometry();
        const FVector2D LocalSize = Geom.GetLocalSize();
        if (LocalSize.X <= 0.f || LocalSize.Y <= 0.f) { return FVector2D::ZeroVector; }
        const float StrideX = LocalSize.X / static_cast<float>(GridW);
        const float StrideY = LocalSize.Y / static_cast<float>(GridH);
        const FVector2D LocalCenter(
            StrideX * (static_cast<float>(Col) + 0.5f),
            StrideY * (static_cast<float>(Row) + 0.5f));
        return Geom.LocalToAbsolute(LocalCenter);
    }

    /**
     * Common shape of all three tests: build source + target grids,
     * register both surfaces on the subsystem, pump frames, then drive
     * the target wrapper's NativeOnDragOver + NativeOnDrop directly and
     * assert the recorded event sequence.
     */
    struct FCrossGridDragLatent : public IAutomationLatentCommand
    {
        FAutomationTestBase* Test;
        FSurfaceFixture Source;
        FSurfaceFixture Target;
        int32 InstanceId;
        FName ExpectedVMMethod;
        int32 FramesRemaining;

        FCrossGridDragLatent(
            FAutomationTestBase* InTest,
            FSurfaceFixture InSource,
            FSurfaceFixture InTarget,
            int32 InInstanceId,
            FName InExpectedVMMethod,
            int32 InFrames = 30)
            : Test(InTest)
            , Source(MoveTemp(InSource))
            , Target(MoveTemp(InTarget))
            , InstanceId(InInstanceId)
            , ExpectedVMMethod(InExpectedVMMethod)
            , FramesRemaining(InFrames)
        {
        }

        virtual bool Update() override
        {
            if (FramesRemaining > 0) { --FramesRemaining; return false; }

            if (!Source.Host || !Target.Host || !Source.Grid || !Target.Grid)
            {
                Test->AddError(TEXT("Fixture lost before latent could run"));
                return true;
            }

            UWorld* World = Target.Host->GetWorld();
            UInventoryUIDragHostSubsystem* Subsystem = ResolveSyntheticInputHostOwnedSubsystem(World, Target.Host.Get());
            if (!Test->TestNotNull(TEXT("Subsystem must resolve"), Subsystem))
            {
                return true;
            }

            // Clean slate so prior tests don't bleed session / surface state.
            Subsystem->ClearSurfaces();
            Subsystem->CancelDrag();

            // Bind a spy VM as the policy provider so the router's real
            // dispatch reaches a test-double that records the call. Post
            // the grid-drop double-source fix, CompleteDrop actually
            // invokes FInventoryDropRouter::Route, which needs a VM.
            UInventoryViewModelSpy* Spy = NewObject<UInventoryViewModelSpy>();
            Subsystem->SetPolicyProvider(TScriptInterface<IInventorySurfacePolicyProvider>(Spy));

            ON_SCOPE_EXIT
            {
                if (Source.Host) { Source.Host->RemoveFromParent(); }
                if (Target.Host) { Target.Host->RemoveFromParent(); }
                if (Subsystem)
                {
                    Subsystem->SetPolicyProvider(TScriptInterface<IInventorySurfacePolicyProvider>());
                    Subsystem->CancelDrag();
                    Subsystem->ClearSurfaces();
                }
            };

            // Register the two surfaces so the subsystem's controller can
            // resolve the target tag from the cursor's cell.
            RegisterPermissiveSurface(*Subsystem, Source.SurfaceTag, Source.Grid, Source.Dims);
            RegisterPermissiveSurface(*Subsystem, Target.SurfaceTag, Target.Grid, Target.Dims);

            // Compute target cell center via cached geometry. Pick cell (1, 1)
            // so it's inside the grid regardless of which dims (tests use >= 2x2).
            const int32 TargetCol = 1;
            const int32 TargetRow = 1;
            const FVector2D ScreenPos = ComputeCellCenterScreenPos(
                Target.Grid, TargetCol, TargetRow, Target.Dims.X, Target.Dims.Y);

            if (ScreenPos.IsNearlyZero())
            {
                Test->AddError(TEXT("Target cell center resolved to origin; cached geometry not valid"));
                return true;
            }

            // Subscribe the recorder BEFORE driving any subsystem activity.
            FInventoryDragEventRecorder Recorder(Subsystem);

            // Seed the drag session as if the Slate drag had just started
            // at a cell in the source grid. The subsystem uses these as
            // session identity; the candidate on UpdatePreview/CompleteDrop
            // carries the same identity for the router.
            FInventoryDragStartParams Params;
            Params.SourceTag = Source.SurfaceTag;
            Params.SourceCell = FIntPoint(0, 0);
            Params.InstanceId = InstanceId;
            Params.Quantity = 1;
            Subsystem->BeginCellDrag(Params);

            // Pick the target wrapper at the (col,row) inside the target grid.
            const int32 TargetFlatIdx = TargetRow * Target.Dims.X + TargetCol;
            UW_InventoryCellDropTarget* TargetWrapper = Target.CellHosts.IsValidIndex(TargetFlatIdx)
                ? Target.CellHosts[TargetFlatIdx]
                : nullptr;
            if (!Test->TestNotNull(TEXT("Target wrapper at (1,1) must exist"), TargetWrapper))
            {
                return true;
            }

            // Build a drag payload matching the Started params so the
            // subsystem's controller can compute the footprint.
            UInventoryDragDropOperation* DragOp = NewObject<UInventoryDragDropOperation>();
            DragOp->InstanceId = InstanceId;
            DragOp->FromContainer = Source.SurfaceTag;
            DragOp->FromPos = FIntPoint(0, 0);
            DragOp->Quantity = 1;
            DragOp->ItemSize = FIntPoint(1, 1);

            // Direct handler call on the TARGET wrapper with cached
            // real geometry (fallback strategy, documented in header).
            // FDragDropEvent has no default/move ctor, so direct-init.
            static const TSet<FKey> EmptyPressedButtons;
            const FPointerEvent Pointer(
                /*PointerIndex=*/0, ScreenPos, ScreenPos,
                EmptyPressedButtons, FKey(), /*WheelDelta=*/0.0f,
                FModifierKeysState());
            const FDragDropEvent Event(Pointer, /*Content=*/nullptr);

            // DragOver emits PreviewUpdated on resolve.
            const bool bOver = TargetWrapper->NativeOnDragOver(
                TargetWrapper->GetCachedGeometry(), Event, DragOp);
            Test->TestTrue(TEXT("Cell wrapper NativeOnDragOver must return true"), bOver);

            // Drop emits the full five-event sequence.
            const bool bDropped = TargetWrapper->NativeOnDrop(
                TargetWrapper->GetCachedGeometry(), Event, DragOp);
            Test->TestTrue(TEXT("Cell wrapper NativeOnDrop must return true on valid target"), bDropped);

            // Expected: [Started, PreviewUpdated, DropResolved, Routed,
            // VMInvoked(VMMethod=expected), Completed]. Partial-match on
            // kind + key fields; the recorder names the missing step on
            // sabotage failure.
            FInventoryDragEvent Started;
            Started.Kind = EInventoryDragEventKind::Started;
            Started.SourceTag = Source.SurfaceTag;
            Started.InstanceId = InstanceId;

            FInventoryDragEvent Preview;
            Preview.Kind = EInventoryDragEventKind::PreviewUpdated;
            Preview.TargetTag = Target.SurfaceTag;

            FInventoryDragEvent Resolved;
            Resolved.Kind = EInventoryDragEventKind::DropResolved;
            Resolved.TargetTag = Target.SurfaceTag;

            FInventoryDragEvent Routed;
            Routed.Kind = EInventoryDragEventKind::Routed;
            Routed.TargetTag = Target.SurfaceTag;

            FInventoryDragEvent VMInvoked;
            VMInvoked.Kind = EInventoryDragEventKind::VMInvoked;
            VMInvoked.TargetTag = Target.SurfaceTag;
            VMInvoked.VMMethod = ExpectedVMMethod;

            FInventoryDragEvent Completed;
            Completed.Kind = EInventoryDragEventKind::Completed;

            const FInventoryDragEvent Seq[6] = {
                Started, Preview, Resolved, Routed, VMInvoked, Completed
            };
            Recorder.AssertSequence(*Test, Seq);

            // Spy proves the router ACTUALLY called the VM, not just that
            // the event stream narrated it. Without this assertion the
            // event bus could lie (like it did before the grid-drop
            // double-source fix).
            using ELastCall = UInventoryViewModelSpy::ELastCall;
            ELastCall ExpectedLastCall = ELastCall::None;
            if (ExpectedVMMethod == FName(TEXT("RequestMoveItem")))                     ExpectedLastCall = ELastCall::MoveItem;
            else if (ExpectedVMMethod == FName(TEXT("RequestTakeNearbyItemToContainer"))) ExpectedLastCall = ELastCall::TakeNearbyItemToContainer;
            else if (ExpectedVMMethod == FName(TEXT("RequestStoreItemInNearbyContainerAt"))) ExpectedLastCall = ELastCall::StoreItemInNearbyContainerAt;
            else if (ExpectedVMMethod == FName(TEXT("RequestEquipItem")))               ExpectedLastCall = ELastCall::EquipItem;
            Test->TestEqual(TEXT("Spy VM must have received the expected command from router dispatch"),
                static_cast<int32>(Spy->LastCall), static_cast<int32>(ExpectedLastCall));
            Test->TestEqual(TEXT("Spy VM must have received the expected InstanceId"),
                Spy->LastInstanceId, InstanceId);
            return true;
        }
    };
}

// ---------------------------------------------------------------------------
// E2E 1 - Nearby -> Main (Backpack). Router must dispatch
// RequestTakeNearbyItemToContainer. Sabotage path: if the subsystem's
// CompleteDrop does not emit VMInvoked (e.g. the router rejects the
// pair), the recorder fires "AssertSequence: step 4 expected kind
// VMInvoked, got Cancelled" (or a length mismatch naming the missing
// tail events).
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDragFromNearbyCellDropOnMainBackpackEmitsExpectedEventSequenceTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.DragE2E.DragFromNearbyCellDropOnMainBackpackEmitsExpectedEventSequence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FDragFromNearbyCellDropOnMainBackpackEmitsExpectedEventSequenceTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UWorld* World = ResolveDragE2EWorld();
    if (!TestNotNull(TEXT("Automation world must resolve"), World)) { return false; }
    UGameInstance* GI = World->GetGameInstance();
    if (!TestNotNull(TEXT("GameInstance must exist"), GI)) { return false; }

    FSurfaceFixture SourceFx; // Nearby (tag identity only, not painted)
    FSurfaceFixture TargetFx; // Backpack (painted + resolved)
    if (!BuildSurfaceFixture(GI, ProjectTags::Item_Container_WorldStorage, 4, 4, SourceFx, /*bAddToViewport=*/false))
    {
        AddError(TEXT("Could not build source (nearby) fixture"));
        return false;
    }
    if (!BuildSurfaceFixture(GI, ProjectTags::Item_Container_Backpack, 4, 4, TargetFx, /*bAddToViewport=*/true))
    {
        AddError(TEXT("Could not build target (backpack) fixture"));
        return false;
    }

    ADD_LATENT_AUTOMATION_COMMAND(FCrossGridDragLatent(
        this, MoveTemp(SourceFx), MoveTemp(TargetFx),
        /*InstanceId=*/1001,
        /*ExpectedVMMethod=*/FName(TEXT("RequestTakeNearbyItemToContainer"))));
    return true;
}

// ---------------------------------------------------------------------------
// E2E 2 - Main (Backpack) -> Nearby. Router must dispatch
// RequestStoreItemInNearbyContainerAt. Sabotage: same shape as E2E 1
// but the expected VMMethod is different; if the router rows regress
// (e.g. WorldStorage-as-target branch misses), VMInvoked.VMMethod will
// not match and the recorder names the step + axis.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDragFromMainBackpackDropOnNearbyGridEmitsExpectedEventSequenceTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.DragE2E.DragFromMainBackpackDropOnNearbyGridEmitsExpectedEventSequence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FDragFromMainBackpackDropOnNearbyGridEmitsExpectedEventSequenceTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UWorld* World = ResolveDragE2EWorld();
    if (!TestNotNull(TEXT("Automation world must resolve"), World)) { return false; }
    UGameInstance* GI = World->GetGameInstance();
    if (!TestNotNull(TEXT("GameInstance must exist"), GI)) { return false; }

    FSurfaceFixture SourceFx; // Backpack (tag identity only, not painted)
    FSurfaceFixture TargetFx; // Nearby (painted + resolved)
    if (!BuildSurfaceFixture(GI, ProjectTags::Item_Container_Backpack, 4, 4, SourceFx, /*bAddToViewport=*/false))
    {
        AddError(TEXT("Could not build source (backpack) fixture"));
        return false;
    }
    if (!BuildSurfaceFixture(GI, ProjectTags::Item_Container_WorldStorage, 4, 4, TargetFx, /*bAddToViewport=*/true))
    {
        AddError(TEXT("Could not build target (nearby) fixture"));
        return false;
    }

    ADD_LATENT_AUTOMATION_COMMAND(FCrossGridDragLatent(
        this, MoveTemp(SourceFx), MoveTemp(TargetFx),
        /*InstanceId=*/1002,
        /*ExpectedVMMethod=*/FName(TEXT("RequestStoreItemInNearbyContainerAt"))));
    return true;
}

// ---------------------------------------------------------------------------
// E2E 3 - Within main (Backpack -> Backpack). Router must dispatch
// RequestMoveItem. Sabotage path: if the source/target tags accidentally
// stop matching (e.g. the subsystem stamps a different target tag than
// the session's source), router maps to the wrong VM method and
// VMInvoked.VMMethod asserts name mismatch.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDragWithinMainBackpackEmitsMoveItemEventSequenceTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.DragE2E.DragWithinMainBackpackEmitsMoveItemEventSequence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FDragWithinMainBackpackEmitsMoveItemEventSequenceTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UWorld* World = ResolveDragE2EWorld();
    if (!TestNotNull(TEXT("Automation world must resolve"), World)) { return false; }
    UGameInstance* GI = World->GetGameInstance();
    if (!TestNotNull(TEXT("GameInstance must exist"), GI)) { return false; }

    // Intra-backpack move: ONE backpack grid registered, drag starts
    // at the backpack cell (0,0) and drops on the backpack cell (1,1).
    // The source "fixture" is unused as a painted surface - it only
    // carries the source tag identity via the Started params. To keep
    // the shared latent command happy we reuse the same fixture pointer
    // for source and target; the latent only renders once from target's
    // perspective and registers the Backpack tag once.
    FSurfaceFixture BackpackFx;
    if (!BuildSurfaceFixture(GI, ProjectTags::Item_Container_Backpack, 4, 4, BackpackFx))
    {
        AddError(TEXT("Could not build backpack fixture"));
        return false;
    }

    // Source fixture only carries the tag; its Host widget is also used
    // as the ticking viewport member (it is the only host, so the
    // latent cleans up one host). We re-use BackpackFx by value (shallow
    // copy of TObjectPtr + raw ptrs) - both copies point at the same
    // live widgets and the latent's ON_SCOPE_EXIT removes them once.
    FSurfaceFixture SourceAlias;
    SourceAlias.Host = BackpackFx.Host;
    SourceAlias.Grid = BackpackFx.Grid;
    SourceAlias.CellHosts = BackpackFx.CellHosts;
    SourceAlias.SurfaceTag = BackpackFx.SurfaceTag;
    SourceAlias.Dims = BackpackFx.Dims;

    ADD_LATENT_AUTOMATION_COMMAND(FCrossGridDragLatent(
        this, MoveTemp(SourceAlias), MoveTemp(BackpackFx),
        /*InstanceId=*/1003,
        /*ExpectedVMMethod=*/FName(TEXT("RequestMoveItem"))));
    return true;
}

// ---------------------------------------------------------------------------
// Follow-up #3 - Mid-drag rotation. The drag begins with bRotated=false,
// fires a first PreviewUpdated, then a rotation toggle pushes a second
// UpdatePreview with Candidate.bRotated=true. On CompleteDrop the router
// must carry bTargetRotated=true (and the subsystem mirrors that into
// Session.bRotated), so RequestMoveItem receives bRotated=true.
//
// Expected event sequence (length 7):
//   [Started, PreviewUpdated, PreviewUpdated (rotated),
//    DropResolved, Routed, VMInvoked{RequestMoveItem}, Completed]
//
// Rotation toggle goes through FInventoryCellCandidate::bRotated per the
// architect's preference (simpler than a SetSessionRotated setter; keeps
// the subsystem API single-point-of-entry).
//
// Sabotage path: removing the `Session.bRotated = Candidate.bRotated;`
// line in UInventoryUIDragHostSubsystem::UpdatePreview makes the spy
// receive bRotated=false, and the assertion "Spy expected rotated=true"
// fires with the specific axis named.
// ---------------------------------------------------------------------------
namespace
{
    struct FRotateMidDragLatent : public IAutomationLatentCommand
    {
        FAutomationTestBase* Test;
        FSurfaceFixture BackpackFx;
        int32 InstanceId;
        int32 FramesRemaining;

        FRotateMidDragLatent(FAutomationTestBase* InTest, FSurfaceFixture InBackpack, int32 InInstanceId, int32 InFrames = 30)
            : Test(InTest)
            , BackpackFx(MoveTemp(InBackpack))
            , InstanceId(InInstanceId)
            , FramesRemaining(InFrames)
        {
        }

        virtual bool Update() override
        {
            if (FramesRemaining > 0) { --FramesRemaining; return false; }

            if (!BackpackFx.Host || !BackpackFx.Grid)
            {
                Test->AddError(TEXT("Fixture lost before latent could run"));
                return true;
            }

            UWorld* World = BackpackFx.Host->GetWorld();
            UInventoryUIDragHostSubsystem* Subsystem = ResolveSyntheticInputHostOwnedSubsystem(World, BackpackFx.Host.Get());
            if (!Test->TestNotNull(TEXT("Subsystem must resolve"), Subsystem))
            {
                return true;
            }

            // Clean slate so prior tests don't bleed session/surface state.
            Subsystem->ClearSurfaces();
            Subsystem->CancelDrag();

            UInventoryViewModelSpy* Spy = NewObject<UInventoryViewModelSpy>();
            Subsystem->SetPolicyProvider(TScriptInterface<IInventorySurfacePolicyProvider>(Spy));

            ON_SCOPE_EXIT
            {
                if (BackpackFx.Host) { BackpackFx.Host->RemoveFromParent(); }
                if (Subsystem)
                {
                    Subsystem->SetPolicyProvider(TScriptInterface<IInventorySurfacePolicyProvider>());
                    Subsystem->CancelDrag();
                    Subsystem->ClearSurfaces();
                }
            };

            RegisterPermissiveSurface(*Subsystem, BackpackFx.SurfaceTag, BackpackFx.Grid, BackpackFx.Dims);

            // Target cell (2, 2): pick something strictly inside the 4x4
            // grid so rotation doesn't push the 1x1 footprint off-grid
            // even if the item were asymmetric - test uses a 1x1 footprint
            // so orientation does not change the occupied cell count, but
            // the bRotated axis still flows end-to-end.
            const int32 TargetCol = 2;
            const int32 TargetRow = 2;
            const FVector2D ScreenPos = ComputeCellCenterScreenPos(
                BackpackFx.Grid, TargetCol, TargetRow, BackpackFx.Dims.X, BackpackFx.Dims.Y);
            if (ScreenPos.IsNearlyZero())
            {
                Test->AddError(TEXT("Target cell center resolved to origin; cached geometry not valid"));
                return true;
            }

            FInventoryDragEventRecorder Recorder(Subsystem);

            // Started: drag starts in non-rotated orientation at (0, 0).
            FInventoryDragStartParams Params;
            Params.SourceTag = BackpackFx.SurfaceTag;
            Params.SourceCell = FIntPoint(0, 0);
            Params.InstanceId = InstanceId;
            Params.Quantity = 1;
            Params.bRotated = false;
            Subsystem->BeginCellDrag(Params);

            // Build the common candidate fields. Rotation flips between
            // the two UpdatePreview calls - the first carries the
            // non-rotated footprint, the second carries the rotated one.
            FInventoryCellCandidate Candidate;
            Candidate.InstanceId = InstanceId;
            Candidate.SourceSurfaceTag = BackpackFx.SurfaceTag;
            Candidate.SourcePos = FIntPoint(0, 0);
            Candidate.Quantity = 1;
            Candidate.ItemSize = FIntPoint(1, 1);

            // First UpdatePreview: non-rotated. Emits PreviewUpdated.
            Candidate.bRotated = false;
            Subsystem->UpdatePreview(Candidate, ScreenPos);

            // Second UpdatePreview: rotated. Emits PreviewUpdated. The
            // subsystem folds bRotated into Session.bRotated so the next
            // CompleteDrop carries rotated=true to the router.
            Candidate.bRotated = true;
            Subsystem->UpdatePreview(Candidate, ScreenPos);

            // Complete drop at the same cell. Emits
            // [DropResolved, Routed, VMInvoked, Completed].
            Candidate.bRotated = true;
            const bool bDispatched = Subsystem->CompleteDrop(Candidate, ScreenPos);
            Test->TestTrue(TEXT("CompleteDrop must dispatch on a valid target"), bDispatched);

            // Expected: 7 events, partial-field match on kind + target tag
            // where helpful. The recorder names the offending step on
            // sabotage failure.
            FInventoryDragEvent Started;
            Started.Kind = EInventoryDragEventKind::Started;
            Started.SourceTag = BackpackFx.SurfaceTag;
            Started.InstanceId = InstanceId;

            FInventoryDragEvent Preview1;
            Preview1.Kind = EInventoryDragEventKind::PreviewUpdated;
            Preview1.TargetTag = BackpackFx.SurfaceTag;

            FInventoryDragEvent Preview2;
            Preview2.Kind = EInventoryDragEventKind::PreviewUpdated;
            Preview2.TargetTag = BackpackFx.SurfaceTag;

            FInventoryDragEvent Resolved;
            Resolved.Kind = EInventoryDragEventKind::DropResolved;
            Resolved.TargetTag = BackpackFx.SurfaceTag;

            FInventoryDragEvent Routed;
            Routed.Kind = EInventoryDragEventKind::Routed;
            Routed.TargetTag = BackpackFx.SurfaceTag;

            FInventoryDragEvent VMInvoked;
            VMInvoked.Kind = EInventoryDragEventKind::VMInvoked;
            VMInvoked.TargetTag = BackpackFx.SurfaceTag;
            VMInvoked.VMMethod = FName(TEXT("RequestMoveItem"));

            FInventoryDragEvent Completed;
            Completed.Kind = EInventoryDragEventKind::Completed;

            const FInventoryDragEvent Seq[7] = {
                Started, Preview1, Preview2, Resolved, Routed, VMInvoked, Completed
            };
            Recorder.AssertSequence(*Test, Seq);

            // Real-VM proof: the spy must have observed bRotated=true. This
            // is the load-bearing axis for the rotation fix; without it the
            // event stream could narrate rotation while the VM silently
            // received the non-rotated flag.
            Test->TestEqual(TEXT("Spy VM must have received RequestMoveItem"),
                static_cast<int32>(Spy->LastCall),
                static_cast<int32>(UInventoryViewModelSpy::ELastCall::MoveItem));
            Test->TestEqual(TEXT("Spy expected rotated=true"), Spy->LastRotated, true);
            Test->TestEqual(TEXT("Spy expected InstanceId"), Spy->LastInstanceId, InstanceId);
            return true;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRotateMidDragAndDropEmitsMoveItemEventSequenceWithRotatedFlagTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.DragE2E.RotateMidDragAndDropEmitsMoveItemEventSequenceWithRotatedFlag",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FRotateMidDragAndDropEmitsMoveItemEventSequenceWithRotatedFlagTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UWorld* World = ResolveDragE2EWorld();
    if (!TestNotNull(TEXT("Automation world must resolve"), World)) { return false; }
    UGameInstance* GI = World->GetGameInstance();
    if (!TestNotNull(TEXT("GameInstance must exist"), GI)) { return false; }

    FSurfaceFixture BackpackFx;
    if (!BuildSurfaceFixture(GI, ProjectTags::Item_Container_Backpack, 4, 4, BackpackFx))
    {
        AddError(TEXT("Could not build backpack fixture"));
        return false;
    }

    ADD_LATENT_AUTOMATION_COMMAND(FRotateMidDragLatent(
        this, MoveTemp(BackpackFx), /*InstanceId=*/1004));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
