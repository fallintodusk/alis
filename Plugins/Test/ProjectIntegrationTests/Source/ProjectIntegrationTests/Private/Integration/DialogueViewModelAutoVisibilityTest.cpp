// Copyright ALIS. All Rights Reserved.
//
// Validates DialogueViewModel and W_DialoguePanel integration:
// - GetBoolProperty works around VIEWMODEL_PROPERTY UHT limitation
// - Mock IDialogueService drives ViewModel state correctly
// - Widget tree dump verifies data-driven JSON layout loads correctly

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "UnrealClient.h"
#include "Engine/GameInstance.h"
#include "Widgets/ProjectUserWidget.h"
#include "MVVM/DialogueViewModel.h"
#include "MVVM/ProjectViewModel.h"
#include "Widgets/W_DialoguePanel.h"
#include "Interfaces/IDialogueService.h"
#include "ProjectServiceLocator.h"
#include "Subsystems/ProjectUIDebugSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/**
	 * Minimal mock IDialogueService for test harness.
	 * All state fields are public for direct test control.
	 * Call BroadcastStateChanged() after modifying state to notify subscribers.
	 */
	class FMockDialogueService : public IDialogueService
	{
	public:
		bool bActive = false;
		FName TreeId = NAME_None;
		FName NodeId = NAME_None;
		FText Speaker;
		FText Text;
		TArray<FDialogueOptionView> Options;
		bool bTerminal = false;

		void BroadcastStateChanged() { StateChangedDelegate.Broadcast(); }

		virtual bool IsDialogueActive() const override { return bActive; }
		virtual FName GetCurrentTreeId() const override { return TreeId; }
		virtual FName GetCurrentNodeId() const override { return NodeId; }
		virtual FText GetCurrentSpeaker() const override { return Speaker; }
		virtual FText GetCurrentText() const override { return Text; }
		virtual void GetCurrentOptions(TArray<FDialogueOptionView>& Out) const override { Out = Options; }
		virtual bool IsCurrentNodeTerminal() const override { return bTerminal; }
		virtual bool CurrentNodeHasOptions() const override { return Options.Num() > 0; }
		virtual void SelectOption(int32) override {}
		virtual void AdvanceOrEnd() override {}
		virtual void EndDialogue() override { bActive = false; BroadcastStateChanged(); }
		virtual FOnDialogueStateChanged& OnDialogueStateChanged() override { return StateChangedDelegate; }
		virtual FOnDialogueSignal& OnDialogueSignal() override { return SignalDelegate; }

	private:
		FOnDialogueStateChanged StateChangedDelegate;
		FOnDialogueSignal SignalDelegate;
	};
}

// ---- Unit Tests (EditorContext, no game world needed) ----

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDialogueViewModelGetBoolPropertyTest,
	"ProjectIntegrationTests.UI.Dialogue.ViewModel.GetBoolProperty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FDialogueViewModelGetBoolPropertyTest::RunTest(const FString& Parameters)
{
	UDialogueViewModel* VM = NewObject<UDialogueViewModel>(GetTransientPackage());
	if (!TestNotNull(TEXT("ViewModel should be created"), VM))
	{
		return false;
	}

	VM->Initialize(nullptr);

	TestFalse(TEXT("bIsActive should default to false"), VM->GetBoolProperty(FName("bIsActive")));
	TestFalse(TEXT("Unknown property should return false"), VM->GetBoolProperty(FName("NonExistent")));

	VM->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDialogueViewModelNotificationTest,
	"ProjectIntegrationTests.UI.Dialogue.ViewModel.PropertyNotification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FDialogueViewModelNotificationTest::RunTest(const FString& Parameters)
{
	UDialogueViewModel* VM = NewObject<UDialogueViewModel>(GetTransientPackage());
	if (!TestNotNull(TEXT("ViewModel should be created"), VM))
	{
		return false;
	}

	VM->Initialize(nullptr);

	TArray<FName> ReceivedChanges;
	FDelegateHandle Handle = VM->OnPropertyChangedNative.AddLambda(
		[&ReceivedChanges](FName PropName)
		{
			ReceivedChanges.Add(PropName);
		});

	TestTrue(TEXT("OnPropertyChangedNative should be bound"), VM->OnPropertyChangedNative.IsBound());

	VM->OnPropertyChangedNative.Remove(Handle);
	TestFalse(TEXT("OnPropertyChangedNative should be unbound after Remove"),
		VM->OnPropertyChangedNative.IsBound());

	VM->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAutoVisibilityBaseClassTest,
	"ProjectIntegrationTests.UI.Dialogue.ViewModel.BaseClassGetBoolProperty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FAutoVisibilityBaseClassTest::RunTest(const FString& Parameters)
{
	UProjectViewModel* BaseVM = NewObject<UProjectViewModel>(GetTransientPackage());
	if (!TestNotNull(TEXT("Base ViewModel should be created"), BaseVM))
	{
		return false;
	}

	TestFalse(TEXT("Base GetBoolProperty should return false"),
		BaseVM->GetBoolProperty(FName("bIsActive")));

	UDialogueViewModel* DialogueVM = NewObject<UDialogueViewModel>(GetTransientPackage());
	if (!TestNotNull(TEXT("DialogueViewModel should be created"), DialogueVM))
	{
		return false;
	}

	// Polymorphic call through base pointer (same path auto_visibility uses)
	UProjectViewModel* AsBase = DialogueVM;
	TestFalse(TEXT("Polymorphic GetBoolProperty should return false for default bIsActive"),
		AsBase->GetBoolProperty(FName("bIsActive")));

	return true;
}

// Mock service drives ViewModel through the full IDialogueService contract.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDialogueViewModelMockServiceTest,
	"ProjectIntegrationTests.UI.Dialogue.ViewModel.MockServiceIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FDialogueViewModelMockServiceTest::RunTest(const FString& Parameters)
{
	TSharedPtr<IDialogueService> Original = FProjectServiceLocator::Resolve<IDialogueService>();
	TSharedRef<FMockDialogueService> Mock = MakeShared<FMockDialogueService>();
	FProjectServiceLocator::Register<IDialogueService>(StaticCastSharedRef<IDialogueService>(Mock));

	UDialogueViewModel* VM = NewObject<UDialogueViewModel>(GetTransientPackage());
	VM->Initialize(nullptr);

	// Initially inactive
	TestFalse(TEXT("VM should start inactive"), VM->GetbIsActive());
	TestFalse(TEXT("GetBoolProperty should be false"), VM->GetBoolProperty(FName("bIsActive")));

	// Activate dialogue
	Mock->bActive = true;
	Mock->Speaker = FText::FromString(TEXT("Gramophone"));
	Mock->Text = FText::FromString(TEXT("A dusty gramophone sits on the table..."));
	Mock->BroadcastStateChanged();

	TestTrue(TEXT("VM should be active"), VM->GetbIsActive());
	TestTrue(TEXT("GetBoolProperty(bIsActive) should be true"), VM->GetBoolProperty(FName("bIsActive")));
	TestEqual(TEXT("Speaker should be Gramophone"),
		VM->GetSpeakerName().ToString(), FString(TEXT("Gramophone")));
	TestEqual(TEXT("DialogueText should match mock"),
		VM->GetDialogueText().ToString(), FString(TEXT("A dusty gramophone sits on the table...")));
	TestTrue(TEXT("bHasSpeaker should be true"), VM->GetbHasSpeaker());
	TestFalse(TEXT("bHasOptions should be false"), VM->GetbHasOptions());

	// Add options and re-broadcast
	FDialogueOptionView Opt1;
	Opt1.Index = 0;
	Opt1.Text = FText::FromString(TEXT("Listen to music"));
	FDialogueOptionView Opt2;
	Opt2.Index = 1;
	Opt2.Text = FText::FromString(TEXT("Leave"));
	Mock->Options = { Opt1, Opt2 };
	Mock->BroadcastStateChanged();

	TestTrue(TEXT("bHasOptions should be true"), VM->GetbHasOptions());
	TestEqual(TEXT("Options count should be 2"), VM->GetOptions().Num(), 2);
	TestEqual(TEXT("Option 0 text"),
		VM->GetOptions()[0].Text.ToString(), FString(TEXT("Listen to music")));

	// End dialogue
	Mock->EndDialogue();
	TestFalse(TEXT("VM should be inactive after EndDialogue"), VM->GetbIsActive());

	// Cleanup
	VM->Shutdown();
	FProjectServiceLocator::Unregister<IDialogueService>();
	if (Original.IsValid())
	{
		FProjectServiceLocator::Register<IDialogueService>(Original.ToSharedRef());
	}

	return true;
}

// ---- Integration Test: Widget Tree Dump ----

// Waits for Slate layout, verifies widget state, dumps tree to JSON.
class FDumpDialogueTreeAfterLayout : public IAutomationLatentCommand
{
public:
	FDumpDialogueTreeAfterLayout(
		FAutomationTestBase* InTest,
		UProjectUIDebugSubsystem* InDebugSub,
		UW_DialoguePanel* InPanel,
		UDialogueViewModel* InVM,
		TSharedPtr<IDialogueService> InOriginalService,
		int32 InFrames = 2)
		: Test(InTest)
		, DebugSub(InDebugSub)
		, Panel(InPanel)
		, VM(InVM)
		, OriginalService(InOriginalService)
		, FramesRemaining(InFrames) {}

	virtual bool Update() override
	{
		if (FramesRemaining > 0) { --FramesRemaining; return false; }

		if (!Panel || !DebugSub || !VM)
		{
			Test->AddError(TEXT("Panel, DebugSub, or VM became invalid"));
			Cleanup();
			return true;
		}

		// Verify ViewModel state (mock service drives bIsActive=true)
		Test->TestTrue(TEXT("VM.GetbIsActive() should be true"), VM->GetbIsActive());
		Test->TestTrue(TEXT("VM.GetBoolProperty(bIsActive) should be true"),
			VM->GetBoolProperty(FName("bIsActive")));
		Test->TestTrue(TEXT("VM.GetbHasSpeaker() should be true"), VM->GetbHasSpeaker());
		Test->TestTrue(TEXT("VM.GetbHasOptions() should be true"), VM->GetbHasOptions());

		// Widget should be visible (RefreshFromViewModel sets SelfHitTestInvisible when active)
		Test->TestEqual(TEXT("Panel visibility should be SelfHitTestInvisible"),
			Panel->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);

		// Dump widget tree to JSON for agent analysis
		const FString OutPath = TEXT("Dumps/DialoguePanel.json");
		const bool bDumpOk = DebugSub->DumpWidgetTreeEx(OutPath, TEXT("json"), TEXT("Dialogue"));
		Test->TestTrue(TEXT("DumpWidgetTreeEx should succeed"), bDumpOk);

		if (!bDumpOk)
		{
			Test->AddWarning(TEXT("Widget tree dump failed - check DialoguePanel.json layout"));
		}

		FScreenshotRequest::RequestScreenshot(TEXT("Dumps/DialoguePanel_screenshot.png"), false, false);

		Cleanup();
		return true;
	}

private:
	void Cleanup()
	{
		// Shutdown VM before unregistering mock (VM unsubscribes from mock's delegate)
		if (VM) { VM->Shutdown(); }

		FProjectServiceLocator::Unregister<IDialogueService>();
		if (OriginalService.IsValid())
		{
			FProjectServiceLocator::Register<IDialogueService>(OriginalService.ToSharedRef());
		}
	}

	FAutomationTestBase* Test;
	UProjectUIDebugSubsystem* DebugSub;
	UW_DialoguePanel* Panel;
	UDialogueViewModel* VM;
	TSharedPtr<IDialogueService> OriginalService;
	int32 FramesRemaining;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDialogueWidgetDumpTreeTest,
	"ProjectIntegrationTests.UI.Layout.DialoguePanel.DumpTree",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FDialogueWidgetDumpTreeTest::RunTest(const FString& Parameters)
{
	UWorld* World = AutomationCommon::GetAnyGameWorld();
	if (!TestNotNull(TEXT("World should exist"), World))
	{
		AddError(TEXT("No game world available - run with -game flag"));
		return false;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!TestNotNull(TEXT("GameInstance should exist"), GameInstance))
	{
		return false;
	}

	UProjectUIDebugSubsystem* DebugSub = GameInstance->GetSubsystem<UProjectUIDebugSubsystem>();
	if (!TestNotNull(TEXT("DebugSubsystem should exist"), DebugSub))
	{
		return false;
	}

	// Inject mock service
	TSharedPtr<IDialogueService> OriginalService = FProjectServiceLocator::Resolve<IDialogueService>();
	TSharedRef<FMockDialogueService> Mock = MakeShared<FMockDialogueService>();
	FProjectServiceLocator::Register<IDialogueService>(StaticCastSharedRef<IDialogueService>(Mock));

	// Create ViewModel (subscribes to mock service on Initialize)
	UDialogueViewModel* VM = NewObject<UDialogueViewModel>(GameInstance);
	VM->Initialize(GameInstance);

	// Drive mock: simulate conversation with speaker, text, and options
	Mock->bActive = true;
	Mock->Speaker = FText::FromString(TEXT("Gramophone"));
	Mock->Text = FText::FromString(TEXT("A dusty gramophone sits on the table..."));
	FDialogueOptionView Opt;
	Opt.Index = 0;
	Opt.Text = FText::FromString(TEXT("Listen to music"));
	Mock->Options = { Opt };
	Mock->BroadcastStateChanged();

	// Verify VM received state before creating widget
	TestTrue(TEXT("VM should be active after mock broadcast"), VM->GetbIsActive());

	// Create widget (data-driven layout from DialoguePanel.json)
	UW_DialoguePanel* Panel = CreateWidget<UW_DialoguePanel>(GameInstance, UW_DialoguePanel::StaticClass());
	if (!TestNotNull(TEXT("DialoguePanel should be created"), Panel))
	{
		VM->Shutdown();
		FProjectServiceLocator::Unregister<IDialogueService>();
		if (OriginalService.IsValid())
		{
			FProjectServiceLocator::Register<IDialogueService>(OriginalService.ToSharedRef());
		}
		return false;
	}

	Panel->SetViewModel(VM);
	Panel->AddToViewport();
	Panel->SetVisibility(ESlateVisibility::Visible);
	Panel->ForceLayoutPrepass();

	// Wait 2 frames for Slate layout, then dump + verify
	FAutomationTestFramework::Get().EnqueueLatentCommand(
		MakeShared<FDumpDialogueTreeAfterLayout>(
			this, DebugSub, Panel, VM, OriginalService));

	return true;
}

#endif
