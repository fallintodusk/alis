// Copyright ALIS. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/IInventoryReadOnly.h"
#include "Modules/ModuleManager.h"
#include "MVVM/InventoryViewModel.h"
#include "ProjectGameplayTags.h"
#include "Subsystems/InventoryUIDragHostSubsystem.h"
#include "Widgets/W_InventoryPanel.h"

#include "Support/ProjectInventoryReadOnlyMock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    UWorld* ResolveWorldForSurfaceLifecycleTest()
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInventoryPreboundViewModelRegistersHandSurfacesTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.SurfaceRegistration.PreboundViewModelRegistersHandSurfaces",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FInventoryPreboundViewModelRegistersHandSurfacesTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    UWorld* World = ResolveWorldForSurfaceLifecycleTest();
    if (!TestNotNull(TEXT("Automation world must resolve"), World)) { return false; }

    UGameInstance* GI = World->GetGameInstance();
    if (!TestNotNull(TEXT("GameInstance must exist"), GI)) { return false; }

    APlayerController* PC = World->GetFirstPlayerController();
    if (!TestNotNull(TEXT("PlayerController must exist"), PC)) { return false; }

    ULocalPlayer* LP = PC->GetLocalPlayer();
    if (!TestNotNull(TEXT("LocalPlayer must exist"), LP)) { return false; }

    FModuleManager::Get().LoadModuleChecked(TEXT("ProjectInventoryUI"));

    UInventoryUIDragHostSubsystem* Subsystem = LP->GetSubsystem<UInventoryUIDragHostSubsystem>();
    if (!TestNotNull(TEXT("Drag host subsystem must exist"), Subsystem)) { return false; }

    Subsystem->UnregisterSurface(ProjectTags::Item_Container_LeftHand);
    Subsystem->UnregisterSurface(ProjectTags::Item_Container_RightHand);

    UProjectInventoryReadOnlyMock* Source = NewObject<UProjectInventoryReadOnlyMock>(GI);
    if (!TestNotNull(TEXT("Mock inventory source must construct"), Source)) { return false; }

    FInventoryContainerView HandsContainer;
    HandsContainer.ContainerId = ProjectTags::Item_Container_Hands;
    HandsContainer.GridSize = FIntPoint(UInventoryViewModel::HandGridSize, UInventoryViewModel::HandGridSize);
    Source->SetContainers({ HandsContainer });
    Source->SetEntries({});
    Source->SetTotals(0.f, 10.f, 0.f, 10.f, 0);

    UInventoryViewModel* VM = NewObject<UInventoryViewModel>(GI);
    if (!TestNotNull(TEXT("InventoryViewModel must construct"), VM)) { return false; }
    VM->Initialize(GI);
    VM->SetInventorySource(Source);
    VM->ShowPanel();

    UW_InventoryPanel* Panel = CreateWidget<UW_InventoryPanel>(PC, UW_InventoryPanel::StaticClass());
    if (!TestNotNull(TEXT("Inventory panel must construct"), Panel)) { return false; }

    Panel->SetInventoryViewModel(VM);

    TestFalse(
        TEXT("Pre-bind alone must not register LeftHand before NativeConstruct"),
        Subsystem->HasSurface(ProjectTags::Item_Container_LeftHand));
    TestFalse(
        TEXT("Pre-bind alone must not register RightHand before NativeConstruct"),
        Subsystem->HasSurface(ProjectTags::Item_Container_RightHand));

    Panel->AddToViewport();
    Panel->ForceLayoutPrepass();

    TestTrue(
        TEXT("LeftHand surface registers even when ViewModel was bound before NativeConstruct"),
        Subsystem->HasSurface(ProjectTags::Item_Container_LeftHand));
    TestTrue(
        TEXT("RightHand surface registers even when ViewModel was bound before NativeConstruct"),
        Subsystem->HasSurface(ProjectTags::Item_Container_RightHand));

    Panel->RemoveFromParent();
    return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
    FInventoryPreboundViewModelRegistersHandSurfacesTest,
    "ProjectIntegrationTests.UI.Framework.Inventory.SurfaceRegistration.PreboundViewModelRegistersHandSurfaces",
    "[Fast][Integration][Inventory]")

#endif // WITH_DEV_AUTOMATION_TESTS
