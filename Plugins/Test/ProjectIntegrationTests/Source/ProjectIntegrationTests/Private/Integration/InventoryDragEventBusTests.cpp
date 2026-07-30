// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/IInventorySurfacePolicyProvider.h"
#include "Interaction/ProjectUIGridDragDropController.h"
#include "MVVM/InventoryDragEvent.h"
#include "ProjectGameplayTags.h"
#include "Subsystems/InventoryUIDragHostSubsystem.h"

#include "Support/DragBusTestHostWidget.h"
#include "Support/InventoryDragEventRecorder.h"
#include "Support/InventoryViewModelSpy.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    // Non-None sentinel for partial-field matching in the recorder.
    // When an expected entry carries this value in RejectReason or
    // VMMethod the recorder asserts "the actual value is non-None"
    // instead of comparing for equality.
    const FName NonNoneEventBus(TEXT("__NonNone__"));

    UWorld* ResolveDragBusAutomationWorld()
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

    /**
     * Construct a subsystem instance whose Outer is a valid ULocalPlayer.
     * ClassWithin on ULocalPlayerSubsystem refuses the transient package,
     * so tests share the automation world's LocalPlayer (creating a
     * transient one on the GameInstance if necessary).
     */
    UInventoryUIDragHostSubsystem* NewBareSubsystem(UWorld* World)
    {
        ULocalPlayer* LP = nullptr;
        if (World)
        {
            if (APlayerController* PC = World->GetFirstPlayerController())
            {
                LP = PC->GetLocalPlayer();
            }
            if (!LP)
            {
                if (UGameInstance* GI = World->GetGameInstance())
                {
                    LP = GI->GetFirstGamePlayer();
                }
            }
        }
        if (!LP && World && World->GetGameInstance())
        {
            LP = NewObject<ULocalPlayer>(World->GetGameInstance());
        }
        if (!LP)
        {
            return nullptr;
        }
        return NewObject<UInventoryUIDragHostSubsystem>(LP);
    }

    /**
     * Build a host widget containing a UniformGridPanel and add to
     * viewport. Returns the grid. After this call the caller must pump
     * frames via a latent command before the grid's cached geometry is
     * valid for hit-testing.
     */
    UUniformGridPanel* BuildGridWidgetInViewport(UGameInstance* GI, int32 Columns, int32 Rows, UUserWidget*& OutHost)
    {
        OutHost = nullptr;
        if (!GI)
        {
            return nullptr;
        }

        UUserWidget* Host = CreateWidget<UDragBusTestHostWidget>(GI, UDragBusTestHostWidget::StaticClass());
        if (!Host || !Host->WidgetTree)
        {
            return nullptr;
        }

        UUniformGridPanel* Grid = Host->WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("TestGrid"));
        if (!Grid)
        {
            return nullptr;
        }
        Host->WidgetTree->RootWidget = Grid;

        for (int32 Y = 0; Y < Rows; ++Y)
        {
            for (int32 X = 0; X < Columns; ++X)
            {
                UTextBlock* CellText = Host->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
                if (!CellText) { continue; }
                CellText->SetText(FText::FromString(TEXT("XXXX")));
                UUniformGridSlot* Slot = Grid->AddChildToUniformGrid(CellText, Y, X);
                (void)Slot;
            }
        }

        Host->AddToViewport();
        Host->ForceLayoutPrepass();

        OutHost = Host;
        return Grid;
    }
}

// ---------------------------------------------------------------------------
// Test 1 - BeginCellDrag emits Started with the correct payload.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInventoryDragBusBeginEmitsStartedTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.DragEventBus.BeginCellDrag_EmitsStartedWithCorrectPayload",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FInventoryDragBusBeginEmitsStartedTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    UWorld* World = ResolveDragBusAutomationWorld();
    if (!TestNotNull(TEXT("Automation world must resolve"), World)) { return false; }

    UInventoryUIDragHostSubsystem* Subsystem = NewBareSubsystem(World);
    if (!TestNotNull(TEXT("Subsystem must construct"), Subsystem)) { return false; }

    FInventoryDragEventRecorder Recorder(Subsystem);

    const FGameplayTag& Backpack = ProjectTags::Item_Container_Backpack;

    FInventoryDragStartParams Params;
    Params.SourceTag = Backpack;
    Params.SourceCell = FIntPoint(0, 0);
    Params.InstanceId = 7;
    Params.Quantity = 1;
    Subsystem->BeginCellDrag(Params);

    FInventoryDragEvent Expected;
    Expected.Kind = EInventoryDragEventKind::Started;
    Expected.SourceTag = Backpack;
    Expected.SourceCell = FIntPoint(0, 0);
    Expected.InstanceId = 7;
    Expected.Quantity = 1;

    const FInventoryDragEvent ExpectedArr[1] = { Expected };

    Recorder.AssertEventCount(*this, 1);
    Recorder.AssertSequence(*this, ExpectedArr);
    return true;
}

// ---------------------------------------------------------------------------
// Tests 2 and 3 need a painted grid with non-zero cached geometry. Use a
// latent command that waits frames after AddToViewport + ForceLayoutPrepass
// before driving the subsystem. Without the wait the grid's CachedGeometry
// is still (0,0) and the controller rejects every hit.
// ---------------------------------------------------------------------------

class FInventoryDragBusLatentPreviewCheck : public IAutomationLatentCommand
{
public:
    FInventoryDragBusLatentPreviewCheck(FAutomationTestBase* InTest,
        UInventoryUIDragHostSubsystem* InSubsystem,
        UUniformGridPanel* InGrid,
        UUserWidget* InHost,
        int32 InFrames = 5)
        : Test(InTest), Subsystem(InSubsystem), Grid(InGrid), Host(InHost), FramesRemaining(InFrames)
    {
    }

    virtual bool Update() override
    {
        if (FramesRemaining > 0) { --FramesRemaining; return false; }

        if (!Subsystem.IsValid())
        {
            Test->AddError(TEXT("Subsystem went away before latent preview check could run"));
            return true;
        }
        if (!Grid.IsValid())
        {
            Test->AddError(TEXT("Grid widget went away before latent preview check could run"));
            return true;
        }

        const FGeometry Geom = Grid->GetCachedGeometry();
        const FVector2D LocalSize = Geom.GetLocalSize();
        if (LocalSize.X <= 0.f || LocalSize.Y <= 0.f)
        {
            Test->AddError(TEXT("Cached grid geometry still zero after layout frames - cannot validate preview events"));
            return true;
        }

        const FGameplayTag& Backpack = ProjectTags::Item_Container_Backpack;
        FProjectUIGridSurface Surface;
        Surface.SurfaceTag = Backpack;
        Surface.Grid = Grid.Get();
        Surface.Dims = FIntPoint(4, 4);
        Surface.Priority = 0;
        Subsystem->RegisterSurface(MoveTemp(Surface));

        FInventoryDragEventRecorder Recorder(Subsystem.Get());

        FInventoryDragStartParams Params;
        Params.SourceTag = Backpack;
        Params.SourceCell = FIntPoint(0, 0);
        Params.InstanceId = 11;
        Params.Quantity = 1;
        Subsystem->BeginCellDrag(Params);

        const FVector2D CellStride(LocalSize.X / 4.f, LocalSize.Y / 4.f);
        const FVector2D LocalCenter(CellStride.X * 1.5f, CellStride.Y * 1.5f);
        const FVector2D ScreenPos = Geom.LocalToAbsolute(LocalCenter);

        FInventoryCellCandidate Candidate;
        Candidate.InstanceId = 11;
        Candidate.SourceSurfaceTag = Backpack;
        Candidate.SourcePos = FIntPoint(0, 0);
        Candidate.Quantity = 1;
        Candidate.ItemSize = FIntPoint(1, 1);
        Subsystem->UpdatePreview(Candidate, ScreenPos);

        FInventoryDragEvent StartedExpect;
        StartedExpect.Kind = EInventoryDragEventKind::Started;
        StartedExpect.SourceTag = Backpack;

        FInventoryDragEvent PreviewExpect;
        PreviewExpect.Kind = EInventoryDragEventKind::PreviewUpdated;
        PreviewExpect.TargetTag = Backpack;
        PreviewExpect.TargetCell = FIntPoint(1, 1);

        const FInventoryDragEvent Seq[2] = { StartedExpect, PreviewExpect };
        Recorder.AssertSequence(*Test, Seq);

        if (Host.IsValid()) { Host->RemoveFromParent(); }
        return true;
    }

private:
    FAutomationTestBase* Test;
    TWeakObjectPtr<UInventoryUIDragHostSubsystem> Subsystem;
    TWeakObjectPtr<UUniformGridPanel> Grid;
    TWeakObjectPtr<UUserWidget> Host;
    int32 FramesRemaining;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInventoryDragBusPreviewResolvedTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.DragEventBus.UpdatePreview_ResolvedCell_EmitsPreviewUpdated",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FInventoryDragBusPreviewResolvedTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    UWorld* World = ResolveDragBusAutomationWorld();
    if (!TestNotNull(TEXT("Automation world must resolve"), World)) { return false; }
    UGameInstance* GI = World->GetGameInstance();
    if (!TestNotNull(TEXT("GameInstance must exist"), GI)) { return false; }

    UInventoryUIDragHostSubsystem* Subsystem = NewBareSubsystem(World);
    if (!TestNotNull(TEXT("Subsystem must construct"), Subsystem)) { return false; }

    UUserWidget* Host = nullptr;
    UUniformGridPanel* Grid = BuildGridWidgetInViewport(GI, /*Cols=*/4, /*Rows=*/4, Host);
    if (!TestNotNull(TEXT("Grid widget must build"), Grid)) { return false; }

    ADD_LATENT_AUTOMATION_COMMAND(FInventoryDragBusLatentPreviewCheck(this, Subsystem, Grid, Host, /*Frames=*/5));
    return true;
}

// ---------------------------------------------------------------------------
// Test 3 - CompleteDrop on a valid target emits the four-event sequence.
// ---------------------------------------------------------------------------

class FInventoryDragBusLatentCompleteValidCheck : public IAutomationLatentCommand
{
public:
    FInventoryDragBusLatentCompleteValidCheck(FAutomationTestBase* InTest,
        UInventoryUIDragHostSubsystem* InSubsystem,
        UUniformGridPanel* InGrid,
        UUserWidget* InHost,
        int32 InFrames = 5)
        : Test(InTest), Subsystem(InSubsystem), Grid(InGrid), Host(InHost), FramesRemaining(InFrames)
    {
    }

    virtual bool Update() override
    {
        if (FramesRemaining > 0) { --FramesRemaining; return false; }

        if (!Subsystem.IsValid())
        {
            Test->AddError(TEXT("Subsystem went away before latent complete-valid check could run"));
            return true;
        }
        if (!Grid.IsValid())
        {
            Test->AddError(TEXT("Grid widget went away before latent complete-valid check could run"));
            return true;
        }

        const FGeometry Geom = Grid->GetCachedGeometry();
        const FVector2D LocalSize = Geom.GetLocalSize();
        if (LocalSize.X <= 0.f || LocalSize.Y <= 0.f)
        {
            Test->AddError(TEXT("Cached grid geometry still zero after layout frames - cannot validate complete-drop events"));
            return true;
        }

        // Source is Backpack (player-side); target surface is WorldStorage
        // so the router will map to RequestStoreItemInNearbyContainerAt.
        const FGameplayTag& Backpack = ProjectTags::Item_Container_Backpack;
        const FGameplayTag& WorldStorage = ProjectTags::Item_Container_WorldStorage;

        FProjectUIGridSurface Surface;
        Surface.SurfaceTag = WorldStorage;
        Surface.Grid = Grid.Get();
        Surface.Dims = FIntPoint(4, 4);
        Surface.Priority = 0;
        Subsystem->RegisterSurface(MoveTemp(Surface));

        // Bind a spy VM: CompleteDrop now truly dispatches via
        // FInventoryDropRouter::Route, so without a VM the router path
        // fails-closed with DropRejected{NoCommandTarget}. Spy also proves
        // the router actually called RequestStoreItemInNearbyContainerAt.
        UInventoryViewModelSpy* Spy = NewObject<UInventoryViewModelSpy>();
        Subsystem->SetPolicyProvider(TScriptInterface<IInventorySurfacePolicyProvider>(Spy));

        FInventoryDragEventRecorder Recorder(Subsystem.Get());

        FInventoryDragStartParams Params;
        Params.SourceTag = Backpack;
        Params.SourceCell = FIntPoint(0, 0);
        Params.InstanceId = 22;
        Params.Quantity = 1;
        Subsystem->BeginCellDrag(Params);

        const FVector2D CellStride(LocalSize.X / 4.f, LocalSize.Y / 4.f);
        const FVector2D LocalCenter(CellStride.X * 2.5f, CellStride.Y * 2.5f);
        const FVector2D ScreenPos = Geom.LocalToAbsolute(LocalCenter);

        FInventoryCellCandidate Candidate;
        Candidate.InstanceId = 22;
        Candidate.SourceSurfaceTag = Backpack;
        Candidate.SourcePos = FIntPoint(0, 0);
        Candidate.Quantity = 1;
        Candidate.ItemSize = FIntPoint(1, 1);

        const bool bDispatched = Subsystem->CompleteDrop(Candidate, ScreenPos);
        Test->TestTrue(TEXT("CompleteDrop must report success on a valid target"), bDispatched);

        FInventoryDragEvent StartedExpect;
        StartedExpect.Kind = EInventoryDragEventKind::Started;

        FInventoryDragEvent ResolvedExpect;
        ResolvedExpect.Kind = EInventoryDragEventKind::DropResolved;
        ResolvedExpect.TargetTag = WorldStorage;
        ResolvedExpect.TargetCell = FIntPoint(2, 2);

        FInventoryDragEvent RoutedExpect;
        RoutedExpect.Kind = EInventoryDragEventKind::Routed;
        RoutedExpect.TargetTag = WorldStorage;

        FInventoryDragEvent VMInvokedExpect;
        VMInvokedExpect.Kind = EInventoryDragEventKind::VMInvoked;
        VMInvokedExpect.TargetTag = WorldStorage;
        // Partial-match sentinel: VMMethod must be non-None.
        VMInvokedExpect.VMMethod = NonNoneEventBus;

        FInventoryDragEvent CompletedExpect;
        CompletedExpect.Kind = EInventoryDragEventKind::Completed;
        CompletedExpect.TargetTag = WorldStorage;

        const FInventoryDragEvent Seq[5] = {
            StartedExpect, ResolvedExpect, RoutedExpect, VMInvokedExpect, CompletedExpect };
        Recorder.AssertSequence(*Test, Seq);

        // Real behavior assertion: spy VM received the store command.
        Test->TestEqual(TEXT("Spy VM must record StoreItemInNearbyContainerAt from router dispatch"),
            static_cast<int32>(Spy->LastCall),
            static_cast<int32>(UInventoryViewModelSpy::ELastCall::StoreItemInNearbyContainerAt));
        Test->TestEqual(TEXT("Spy VM must record the correct InstanceId"), Spy->LastInstanceId, 22);

        if (Subsystem.IsValid())
        {
            Subsystem->SetPolicyProvider(TScriptInterface<IInventorySurfacePolicyProvider>());
        }
        if (Host.IsValid()) { Host->RemoveFromParent(); }
        return true;
    }

private:
    FAutomationTestBase* Test;
    TWeakObjectPtr<UInventoryUIDragHostSubsystem> Subsystem;
    TWeakObjectPtr<UUniformGridPanel> Grid;
    TWeakObjectPtr<UUserWidget> Host;
    int32 FramesRemaining;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInventoryDragBusCompleteValidTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.DragEventBus.CompleteDrop_ValidTarget_EmitsFourEventsInOrder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FInventoryDragBusCompleteValidTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    UWorld* World = ResolveDragBusAutomationWorld();
    if (!TestNotNull(TEXT("Automation world must resolve"), World)) { return false; }
    UGameInstance* GI = World->GetGameInstance();
    if (!TestNotNull(TEXT("GameInstance must exist"), GI)) { return false; }

    UInventoryUIDragHostSubsystem* Subsystem = NewBareSubsystem(World);
    if (!TestNotNull(TEXT("Subsystem must construct"), Subsystem)) { return false; }

    UUserWidget* Host = nullptr;
    UUniformGridPanel* Grid = BuildGridWidgetInViewport(GI, /*Cols=*/4, /*Rows=*/4, Host);
    if (!TestNotNull(TEXT("Grid widget must build"), Grid)) { return false; }

    ADD_LATENT_AUTOMATION_COMMAND(FInventoryDragBusLatentCompleteValidCheck(this, Subsystem, Grid, Host, /*Frames=*/5));
    return true;
}

// ---------------------------------------------------------------------------
// Test 4 - CompleteDrop with no target under cursor emits DropRejected +
// Cancelled. No geometry / no surfaces, so controller resolve fails.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInventoryDragBusCompleteInvalidTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.DragEventBus.CompleteDrop_InvalidTarget_EmitsDropRejectedAndCancelled",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FInventoryDragBusCompleteInvalidTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    UWorld* World = ResolveDragBusAutomationWorld();
    if (!TestNotNull(TEXT("Automation world must resolve"), World)) { return false; }

    UInventoryUIDragHostSubsystem* Subsystem = NewBareSubsystem(World);
    if (!TestNotNull(TEXT("Subsystem must construct"), Subsystem)) { return false; }

    FInventoryDragEventRecorder Recorder(Subsystem);

    const FGameplayTag& Backpack = ProjectTags::Item_Container_Backpack;

    FInventoryDragStartParams Params;
    Params.SourceTag = Backpack;
    Params.SourceCell = FIntPoint(0, 0);
    Params.InstanceId = 33;
    Params.Quantity = 1;
    Subsystem->BeginCellDrag(Params);

    FInventoryCellCandidate Candidate;
    Candidate.InstanceId = 33;
    Candidate.SourceSurfaceTag = Backpack;
    Candidate.SourcePos = FIntPoint(0, 0);
    Candidate.Quantity = 1;
    Candidate.ItemSize = FIntPoint(1, 1);
    const bool bDispatched = Subsystem->CompleteDrop(Candidate, FVector2D(0.f, 0.f));
    TestFalse(TEXT("CompleteDrop must fail with no registered surface under cursor"), bDispatched);

    FInventoryDragEvent StartedExpect;
    StartedExpect.Kind = EInventoryDragEventKind::Started;

    FInventoryDragEvent RejectedExpect;
    RejectedExpect.Kind = EInventoryDragEventKind::DropRejected;
    RejectedExpect.RejectReason = NonNoneEventBus;

    FInventoryDragEvent CancelledExpect;
    CancelledExpect.Kind = EInventoryDragEventKind::Cancelled;

    const FInventoryDragEvent Seq[3] = { StartedExpect, RejectedExpect, CancelledExpect };
    Recorder.AssertSequence(*this, Seq);
    return true;
}

// ---------------------------------------------------------------------------
// Test 5 - CancelDrag after BeginCellDrag emits a single Cancelled event.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInventoryDragBusCancelAfterBeginTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.DragEventBus.CancelDrag_AfterStart_EmitsCancelled",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FInventoryDragBusCancelAfterBeginTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    UWorld* World = ResolveDragBusAutomationWorld();
    if (!TestNotNull(TEXT("Automation world must resolve"), World)) { return false; }

    UInventoryUIDragHostSubsystem* Subsystem = NewBareSubsystem(World);
    if (!TestNotNull(TEXT("Subsystem must construct"), Subsystem)) { return false; }

    FInventoryDragStartParams Params;
    Params.SourceTag = ProjectTags::Item_Container_Backpack;
    Params.SourceCell = FIntPoint(0, 0);
    Params.InstanceId = 44;
    Params.Quantity = 1;
    Subsystem->BeginCellDrag(Params);

    FInventoryDragEventRecorder Recorder(Subsystem);  // attach AFTER Started
    Subsystem->CancelDrag();

    FInventoryDragEvent CancelledExpect;
    CancelledExpect.Kind = EInventoryDragEventKind::Cancelled;
    const FInventoryDragEvent Seq[1] = { CancelledExpect };
    Recorder.AssertEventCount(*this, 1);
    Recorder.AssertSequence(*this, Seq);
    return true;
}

// Automation tag registration. See docs/agents/canonical.md "Tag taxonomy"
// and "Single-token CLI filter" for the wrapper contract; GridSizing tests
// carry the same pattern. This file deliberately mixes [Fast] subsystem-
// only tests with [Slow][E2E] painted-grid tests so the -Tags filter can
// prove it selects a subset.
// Taxonomy:
//   Fast + Integration: bare subsystem session lifecycle, no painted widgets.
//   Slow + E2E: painted grid + cached geometry + latent frames.
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
    FInventoryDragBusBeginEmitsStartedTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.DragEventBus.BeginCellDrag_EmitsStartedWithCorrectPayload",
    "[Fast][Integration][Inventory]")
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
    FInventoryDragBusPreviewResolvedTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.DragEventBus.UpdatePreview_ResolvedCell_EmitsPreviewUpdated",
    "[Slow][E2E][Inventory]")
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
    FInventoryDragBusCompleteValidTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.DragEventBus.CompleteDrop_ValidTarget_EmitsFourEventsInOrder",
    "[Slow][E2E][Inventory]")
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
    FInventoryDragBusCompleteInvalidTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.DragEventBus.CompleteDrop_InvalidTarget_EmitsDropRejectedAndCancelled",
    "[Fast][Integration][Inventory]")
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
    FInventoryDragBusCancelAfterBeginTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.DragEventBus.CancelDrag_AfterStart_EmitsCancelled",
    "[Fast][Integration][Inventory]")

#endif // WITH_DEV_AUTOMATION_TESTS
