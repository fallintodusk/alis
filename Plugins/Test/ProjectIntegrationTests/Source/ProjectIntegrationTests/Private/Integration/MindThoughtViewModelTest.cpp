// Copyright ALIS. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "ProjectServiceLocator.h"
#include "Interfaces/IMindService.h"
#include "Interfaces/IDialogueService.h"
#include "Interfaces/IMindThoughtSource.h"
#include "Services/MindServiceImpl.h"
#include "MVVM/MindJournalViewModel.h"
#include "MVVM/MindThoughtViewModel.h"
#include "Widgets/W_MindJournalPanel.h"
#include "Subsystems/ProjectUIDebugSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Tests/AutomationCommon.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "ProjectGameplayTags.h"
#include "Components/BoxComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FName BuildDialogueSignalTagForTests(const FName TreeId, const FName NodeId)
	{
		if (TreeId.IsNone() || NodeId.IsNone())
		{
			return NAME_None;
		}

		return FName(*FString::Printf(TEXT("Dialogue.%s.%s"), *TreeId.ToString(), *NodeId.ToString()));
	}

	FMindThoughtView MakeThought(
		const TCHAR* ThoughtId,
		const TCHAR* ThoughtText,
		float TimeToLiveSec,
		EMindThoughtChannel Channel,
		EMindThoughtSourceType SourceType,
		int32 Priority)
	{
		FMindThoughtView Thought;
		Thought.ThoughtId = FName(ThoughtId);
		Thought.Text = FText::FromString(ThoughtText);
		Thought.TimeToLiveSec = TimeToLiveSec;
		Thought.Channel = Channel;
		Thought.SourceType = SourceType;
		Thought.Priority = Priority;
		Thought.CreatedAtUtc = FDateTime::UtcNow();
		Thought.CreatedAtSec = static_cast<float>(FPlatformTime::Seconds());
		return Thought;
	}

	class FMockMindService : public IMindService
	{
	public:
		virtual void GetThoughtHistory(TArray<FMindThoughtView>& OutThoughts) const override
		{
			OutThoughts = ThoughtHistory;
		}

		virtual bool GetLatestThought(FMindThoughtView& OutThought) const override
		{
			if (ThoughtHistory.Num() == 0)
			{
				return false;
			}

			OutThought = ThoughtHistory.Last();
			return true;
		}

		virtual FOnMindThoughtAdded& OnThoughtAdded() override
		{
			return ThoughtAddedDelegate;
		}

		virtual FOnMindFeedChanged& OnMindFeedChanged() override
		{
			return FeedChangedDelegate;
		}

		void EmitThought(const FMindThoughtView& Thought)
		{
			ThoughtHistory.Add(Thought);
			ThoughtAddedDelegate.Broadcast(Thought);
			FeedChangedDelegate.Broadcast();
		}

		TArray<FMindThoughtView> ThoughtHistory;
		FOnMindThoughtAdded ThoughtAddedDelegate;
		FOnMindFeedChanged FeedChangedDelegate;
	};

	class FMockMindDialogueService : public IDialogueService
	{
	public:
		virtual bool IsDialogueActive() const override { return bIsActive; }
		virtual FName GetCurrentTreeId() const override { return CurrentTreeId; }
		virtual FName GetCurrentNodeId() const override { return CurrentNodeId; }
		virtual FText GetCurrentSpeaker() const override { return FText::GetEmpty(); }
		virtual FText GetCurrentText() const override { return FText::GetEmpty(); }
		virtual void GetCurrentOptions(TArray<FDialogueOptionView>& OutOptions) const override { OutOptions.Reset(); }
		virtual bool IsCurrentNodeTerminal() const override { return false; }
		virtual bool CurrentNodeHasOptions() const override { return false; }
		virtual void SelectOption(int32 /*OptionIndex*/) override {}
		virtual void AdvanceOrEnd() override {}
		virtual void EndDialogue() override {}
		virtual const AActor* GetActiveDialogueOwner() const override { return ActiveOwner.Get(); }
		virtual FOnDialogueStateChanged& OnDialogueStateChanged() override { return StateChangedDelegate; }
		virtual FOnDialogueSignal& OnDialogueSignal() override { return SignalDelegate; }

		void BroadcastStateChanged()
		{
			const FName SignalTag = BuildDialogueSignalTagForTests(CurrentTreeId, CurrentNodeId);
			if (!SignalTag.IsNone())
			{
				SignalDelegate.Broadcast(SignalTag);
			}

			StateChangedDelegate.Broadcast();
		}

		void BroadcastSignal(const FName SignalTag)
		{
			if (!SignalTag.IsNone())
			{
				SignalDelegate.Broadcast(SignalTag);
			}
		}

		bool bIsActive = false;
		FName CurrentTreeId = NAME_None;
		FName CurrentNodeId = NAME_None;
		TWeakObjectPtr<const AActor> ActiveOwner;

	private:
		FOnDialogueStateChanged StateChangedDelegate;
		FOnDialogueSignal SignalDelegate;
	};

	FString MakeTempMappingPath(const FString& FileName)
	{
		const FString TempDir = FPaths::ProjectSavedDir() / TEXT("Automation/ProjectMind");
		IFileManager::Get().MakeDirectory(*TempDir, true);
		return TempDir / FileName;
	}

	int32 CountLinesContaining(const FString& Text, const FString& Needle)
	{
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines, true);

		int32 Matches = 0;
		for (const FString& Line : Lines)
		{
			if (Line.Contains(Needle))
			{
				++Matches;
			}
		}

		return Matches;
	}

	FString MakeDialogueMappingJson(
		const TCHAR* FirstThoughtId,
		const TCHAR* SecondThoughtId,
		double CooldownSec,
		double DedupeWindowSec,
		const TCHAR* FirstDedupeKey,
		const TCHAR* SecondDedupeKey)
	{
		return FString::Printf(
			TEXT("{")
			TEXT("\"$schema\":\"dialogue_thought_mappings.schema.json\",")
			TEXT("\"entries\":[")
			TEXT("{")
			TEXT("\"signal_tag\":\"Dialogue.DLG_GrandPa_Door.greeting\",")
			TEXT("\"thought_id\":\"%s\",")
			TEXT("\"text\":\"First thought\",")
			TEXT("\"time_to_live_sec\":8.0,")
			TEXT("\"cooldown_sec\":%.3f,")
			TEXT("\"dedupe_window_sec\":%.3f,")
			TEXT("\"dedupe_key\":\"%s\",")
			TEXT("\"channel\":\"Toast\",")
			TEXT("\"priority\":90,")
			TEXT("\"source_type\":\"Dialogue\"")
			TEXT("},")
			TEXT("{")
			TEXT("\"signal_tag\":\"Dialogue.DLG_GrandPa_Door.backstory\",")
			TEXT("\"thought_id\":\"%s\",")
			TEXT("\"text\":\"Second thought\",")
			TEXT("\"time_to_live_sec\":8.0,")
			TEXT("\"cooldown_sec\":%.3f,")
			TEXT("\"dedupe_window_sec\":%.3f,")
			TEXT("\"dedupe_key\":\"%s\",")
			TEXT("\"channel\":\"Toast\",")
			TEXT("\"priority\":80,")
			TEXT("\"source_type\":\"Dialogue\"")
			TEXT("}")
			TEXT("]")
			TEXT("}"),
			FirstThoughtId,
			CooldownSec,
			DedupeWindowSec,
			FirstDedupeKey,
			SecondThoughtId,
			CooldownSec,
			DedupeWindowSec,
			SecondDedupeKey);
	}

	UWorld* ResolveMindTestWorld()
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

	class FDumpMindJournalAfterLayout : public IAutomationLatentCommand
	{
	public:
		FDumpMindJournalAfterLayout(
			FAutomationTestBase* InTest,
			UProjectUIDebugSubsystem* InDebugSub,
			UW_MindJournalPanel* InPanel,
			UMindJournalViewModel* InViewModel,
			int32 InWaitFrames)
			: Test(InTest)
			, DebugSub(InDebugSub)
			, Panel(InPanel)
			, ViewModel(InViewModel)
			, WaitFrames(FMath::Max(1, InWaitFrames))
			, RemainingFrames(WaitFrames)
			, Stage(0)
		{
		}

		virtual bool Update() override
		{
			if (RemainingFrames-- > 0)
			{
				return false;
			}

			if (!Test)
			{
				return true;
			}

			if (!DebugSub.IsValid() || !Panel.IsValid() || !ViewModel.IsValid())
			{
				Test->AddError(TEXT("Mind journal dump latent command lost required objects"));
				return true;
			}

			if (Stage == 0)
			{
				Panel->ForceLayoutPrepass();
				const bool bImportantDumpOk = DebugSub->DumpWidgetTreeEx(
					TEXT("Dumps/MindJournal_Important.json"),
					TEXT("json"),
					TEXT("MindJournal"));
				Test->TestTrue(TEXT("Mind journal important tab dump should succeed"), bImportantDumpOk);

				ViewModel->SetJournalTabActive(false);
				RemainingFrames = WaitFrames;
				Stage = 1;
				return false;
			}

			Panel->ForceLayoutPrepass();
			const bool bRecentDumpOk = DebugSub->DumpWidgetTreeEx(
				TEXT("Dumps/MindJournal_Recent.json"),
				TEXT("json"),
				TEXT("MindJournal"));
			Test->TestTrue(TEXT("Mind journal recent tab dump should succeed"), bRecentDumpOk);

			ViewModel->HidePanel();
			ViewModel->Shutdown();
			Panel->RemoveFromParent();

			return true;
		}

	private:
		FAutomationTestBase* Test = nullptr;
		TWeakObjectPtr<UProjectUIDebugSubsystem> DebugSub;
		TWeakObjectPtr<UW_MindJournalPanel> Panel;
		TWeakObjectPtr<UMindJournalViewModel> ViewModel;
		int32 WaitFrames = 2;
		int32 RemainingFrames = 2;
		int32 Stage = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMindThoughtViewModelBasicFlowTest,
	"ProjectIntegrationTests.UI.HUD.MindThought.ViewModel.BasicFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMindThoughtViewModelBasicFlowTest::RunTest(const FString& Parameters)
{
	TSharedPtr<IMindService> OriginalService = FProjectServiceLocator::Resolve<IMindService>();
	TSharedRef<FMockMindService> MockService = MakeShared<FMockMindService>();
	FProjectServiceLocator::Register<IMindService>(StaticCastSharedRef<IMindService>(MockService));

	UMindThoughtViewModel* VM = NewObject<UMindThoughtViewModel>(GetTransientPackage());
	VM->Initialize(nullptr);

	TestFalse(TEXT("VM starts hidden without thoughts"), VM->GetbHasThought());

	const FMindThoughtView Thought = MakeThought(
		TEXT("Mind.Test.Thought"),
		TEXT("Test thought"),
		4.0f,
		EMindThoughtChannel::Toast,
		EMindThoughtSourceType::System,
		10);
	MockService->EmitThought(Thought);

	TestTrue(TEXT("VM should show thought after emit"), VM->GetbHasThought());
	TestEqual(TEXT("Thought text should match"), VM->GetThoughtText().ToString(), FString(TEXT("Test thought")));

	VM->ClearCurrentThought();
	TestFalse(TEXT("VM should hide after clear"), VM->GetbHasThought());

	VM->Shutdown();
	FProjectServiceLocator::Unregister<IMindService>();
	if (OriginalService.IsValid())
	{
		FProjectServiceLocator::Register<IMindService>(OriginalService.ToSharedRef());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMindThoughtViewModelLatestSnapshotTest,
	"ProjectIntegrationTests.UI.HUD.MindThought.ViewModel.LatestSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMindThoughtViewModelLatestSnapshotTest::RunTest(const FString& Parameters)
{
	TSharedPtr<IMindService> OriginalService = FProjectServiceLocator::Resolve<IMindService>();
	TSharedRef<FMockMindService> MockService = MakeShared<FMockMindService>();

	FMindThoughtView Thought = MakeThought(
		TEXT("Mind.Test.Snapshot"),
		TEXT("Persisted thought"),
		6.0f,
		EMindThoughtChannel::Toast,
		EMindThoughtSourceType::System,
		10);
	Thought.CreatedAtUtc = FDateTime::UtcNow() - FTimespan::FromSeconds(2.0);
	Thought.CreatedAtSec = static_cast<float>(FPlatformTime::Seconds() - 2.0);
	MockService->ThoughtHistory.Add(Thought);

	FProjectServiceLocator::Register<IMindService>(StaticCastSharedRef<IMindService>(MockService));

	UMindThoughtViewModel* VM = NewObject<UMindThoughtViewModel>(GetTransientPackage());
	VM->Initialize(nullptr);

	TestTrue(TEXT("VM should load latest thought snapshot"), VM->GetbHasThought());
	TestEqual(TEXT("Snapshot text should match"), VM->GetThoughtText().ToString(), FString(TEXT("Persisted thought")));
	TestTrue(TEXT("Remaining TTL should stay positive"), VM->GetThoughtTimeToLiveSec() > 0.0f);

	VM->Shutdown();
	FProjectServiceLocator::Unregister<IMindService>();
	if (OriginalService.IsValid())
	{
		FProjectServiceLocator::Register<IMindService>(OriginalService.ToSharedRef());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMindDialogueToThoughtViewModelIntegrationTest,
	"ProjectIntegrationTests.UI.HUD.MindThought.Dialogue.MappingIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMindDialogueToThoughtViewModelIntegrationTest::RunTest(const FString& Parameters)
{
	TSharedPtr<IMindService> OriginalMindService = FProjectServiceLocator::Resolve<IMindService>();
	TSharedPtr<IDialogueService> OriginalDialogueService = FProjectServiceLocator::Resolve<IDialogueService>();

	TSharedRef<FMockMindDialogueService> MockDialogueService = MakeShared<FMockMindDialogueService>();
	FProjectServiceLocator::Register<IDialogueService>(StaticCastSharedRef<IDialogueService>(MockDialogueService));

	TSharedRef<FMindServiceImpl> RuntimeMindService = MakeShared<FMindServiceImpl>();
	RuntimeMindService->Start();
	FProjectServiceLocator::Register<IMindService>(StaticCastSharedRef<IMindService>(RuntimeMindService));

	UMindThoughtViewModel* VM = NewObject<UMindThoughtViewModel>(GetTransientPackage());
	VM->Initialize(nullptr);

	MockDialogueService->bIsActive = true;
	MockDialogueService->CurrentTreeId = FName(TEXT("DLG_GrandPa_Door"));
	MockDialogueService->CurrentNodeId = FName(TEXT("greeting"));
	MockDialogueService->BroadcastStateChanged();

	TestTrue(TEXT("Dialogue mapping should emit thought to VM"), VM->GetbHasThought());
	TestEqual(
		TEXT("Dialogue mapping should emit configured text"),
		VM->GetThoughtText().ToString(),
		FString(TEXT("Grandpa needs water. I should find some.")));

	VM->Shutdown();
	RuntimeMindService->Stop();

	FProjectServiceLocator::Unregister<IMindService>();
	if (OriginalMindService.IsValid())
	{
		FProjectServiceLocator::Register<IMindService>(OriginalMindService.ToSharedRef());
	}

	FProjectServiceLocator::Unregister<IDialogueService>();
	if (OriginalDialogueService.IsValid())
	{
		FProjectServiceLocator::Register<IDialogueService>(OriginalDialogueService.ToSharedRef());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMindDialogueRecordResolutionIntegrationTest,
	"ProjectIntegrationTests.UI.HUD.MindThought.Dialogue.RecordResolutionIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMindDialogueRecordResolutionIntegrationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TSharedPtr<IDialogueService> OriginalDialogueService = FProjectServiceLocator::Resolve<IDialogueService>();
	TSharedRef<FMockMindDialogueService> MockDialogueService = MakeShared<FMockMindDialogueService>();
	FProjectServiceLocator::Register<IDialogueService>(StaticCastSharedRef<IDialogueService>(MockDialogueService));

	TSharedRef<FMindServiceImpl> RuntimeMindService = MakeShared<FMindServiceImpl>();
	RuntimeMindService->SetGlobalEmitCooldownForTests(0.0);
	RuntimeMindService->Start();

	MockDialogueService->bIsActive = true;
	MockDialogueService->CurrentTreeId = FName(TEXT("DLG_GrandPa_Door"));

	MockDialogueService->CurrentNodeId = FName(TEXT("greeting"));
	MockDialogueService->BroadcastStateChanged();

	MockDialogueService->CurrentNodeId = FName(TEXT("thanks"));
	MockDialogueService->BroadcastStateChanged();

	TArray<FMindThoughtView> Thoughts;
	RuntimeMindService->GetThoughtHistory(Thoughts);
	TestTrue(TEXT("Dialogue lifecycle should emit at least active + resolved thoughts"), Thoughts.Num() >= 2);

	bool bFoundResolvedRecord = false;
	for (const FMindThoughtView& Thought : Thoughts)
	{
		if (Thought.RecordId == FName(TEXT("Record.Dialogue.Grandpa.NeedsWater"))
			&& Thought.RecordState == EMindRecordState::Resolved)
		{
			bFoundResolvedRecord = true;
			TestEqual(
				TEXT("Resolved record uses resolved text"),
				Thought.Text.ToString(),
				FString(TEXT("Grandpa has water now.")));
			break;
		}
	}

	TestTrue(TEXT("Dialogue completion should emit resolved record state"), bFoundResolvedRecord);

	RuntimeMindService->Stop();
	FProjectServiceLocator::Unregister<IDialogueService>();
	if (OriginalDialogueService.IsValid())
	{
		FProjectServiceLocator::Register<IDialogueService>(OriginalDialogueService.ToSharedRef());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMindDialogueOptionSignalIntegrationTest,
	"ProjectIntegrationTests.UI.HUD.MindThought.Dialogue.OptionSignalIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMindDialogueOptionSignalIntegrationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TSharedPtr<IDialogueService> OriginalDialogueService = FProjectServiceLocator::Resolve<IDialogueService>();
	TSharedRef<FMockMindDialogueService> MockDialogueService = MakeShared<FMockMindDialogueService>();
	FProjectServiceLocator::Register<IDialogueService>(StaticCastSharedRef<IDialogueService>(MockDialogueService));

	TSharedRef<FMindServiceImpl> RuntimeMindService = MakeShared<FMindServiceImpl>();
	RuntimeMindService->SetGlobalEmitCooldownForTests(0.0);
	RuntimeMindService->Start();

	MockDialogueService->BroadcastSignal(FName(TEXT("Dialogue.Option.DLG_GrandPa_Door.greeting.Next.give_water")));

	TArray<FMindThoughtView> Thoughts;
	RuntimeMindService->GetThoughtHistory(Thoughts);
	TestTrue(TEXT("Option signal should emit mapped thought"), Thoughts.Num() >= 1);
	if (Thoughts.Num() > 0)
	{
		const FMindThoughtView& LastThought = Thoughts.Last();
		TestEqual(TEXT("Option signal should resolve grandpa water record"), LastThought.RecordId, FName(TEXT("Record.Dialogue.Grandpa.NeedsWater")));
		TestEqual(TEXT("Option signal should set resolved state"), LastThought.RecordState, EMindRecordState::Resolved);
	}

	RuntimeMindService->Stop();
	FProjectServiceLocator::Unregister<IDialogueService>();
	if (OriginalDialogueService.IsValid())
	{
		FProjectServiceLocator::Register<IDialogueService>(OriginalDialogueService.ToSharedRef());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMindDialogueMappingFailSoftTest,
	"ProjectIntegrationTests.UI.HUD.MindThought.Dialogue.MappingFailSoft",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMindDialogueMappingFailSoftTest::RunTest(const FString& Parameters)
{
	TSharedPtr<IDialogueService> OriginalDialogueService = FProjectServiceLocator::Resolve<IDialogueService>();
	TSharedRef<FMockMindDialogueService> MockDialogueService = MakeShared<FMockMindDialogueService>();
	FProjectServiceLocator::Register<IDialogueService>(StaticCastSharedRef<IDialogueService>(MockDialogueService));

	MockDialogueService->bIsActive = true;
	MockDialogueService->CurrentTreeId = FName(TEXT("DLG_GrandPa_Door"));
	MockDialogueService->CurrentNodeId = FName(TEXT("greeting"));

	{
		TSharedRef<FMindServiceImpl> RuntimeMindService = MakeShared<FMindServiceImpl>(
			TEXT("Data/Schema/Gameplay/ProjectMind/does_not_exist.json"));
		RuntimeMindService->Start();
		MockDialogueService->BroadcastStateChanged();

		TArray<FMindThoughtView> Thoughts;
		RuntimeMindService->GetThoughtHistory(Thoughts);
		TestEqual(TEXT("Missing mapping should fail soft and emit no thoughts"), Thoughts.Num(), 0);

		RuntimeMindService->Stop();
	}

	const FString InvalidJsonPath = MakeTempMappingPath(TEXT("dialogue_thought_mappings_invalid.json"));
	FFileHelper::SaveStringToFile(TEXT("{ not-valid-json "), *InvalidJsonPath);

	{
		TSharedRef<FMindServiceImpl> RuntimeMindService = MakeShared<FMindServiceImpl>(InvalidJsonPath);
		RuntimeMindService->Start();
		MockDialogueService->BroadcastStateChanged();

		TArray<FMindThoughtView> Thoughts;
		RuntimeMindService->GetThoughtHistory(Thoughts);
		TestEqual(TEXT("Invalid mapping should fail soft and emit no thoughts"), Thoughts.Num(), 0);

		RuntimeMindService->Stop();
	}

	IFileManager::Get().Delete(*InvalidJsonPath);

	FProjectServiceLocator::Unregister<IDialogueService>();
	if (OriginalDialogueService.IsValid())
	{
		FProjectServiceLocator::Register<IDialogueService>(OriginalDialogueService.ToSharedRef());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMindDialogueServiceUnavailableFailSoftTest,
	"ProjectIntegrationTests.UI.HUD.MindThought.Dialogue.ServiceUnavailableFailSoft",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMindDialogueServiceUnavailableFailSoftTest::RunTest(const FString& Parameters)
{
	TSharedPtr<IDialogueService> OriginalDialogueService = FProjectServiceLocator::Resolve<IDialogueService>();
	if (OriginalDialogueService.IsValid())
	{
		FProjectServiceLocator::Unregister<IDialogueService>();
	}

	TSharedRef<FMindServiceImpl> RuntimeMindService = MakeShared<FMindServiceImpl>();
	RuntimeMindService->Start();
	RuntimeMindService->PumpProvidersForTests();

	TArray<FMindThoughtView> Thoughts;
	RuntimeMindService->GetThoughtHistory(Thoughts);
	TestEqual(TEXT("Mind runtime should fail soft without dialogue service"), Thoughts.Num(), 0);

	RuntimeMindService->Stop();

	if (OriginalDialogueService.IsValid())
	{
		FProjectServiceLocator::Register<IDialogueService>(OriginalDialogueService.ToSharedRef());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMindScanRulesFailSoftTest,
	"ProjectIntegrationTests.UI.HUD.MindThought.Scan.RulesFailSoft",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMindScanRulesFailSoftTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = ResolveMindTestWorld();
	if (!TestNotNull(TEXT("Automation world should exist"), World))
	{
		return false;
	}

	auto SpawnScanFixture = [World, this]() -> TPair<APawn*, AActor*>
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.ObjectFlags |= RF_Transient;

		// Keep fixture above world geometry so LOS-dependent fallback rules remain deterministic.
		const FVector FixtureOrigin(0.0f, 0.0f, 5000.0f);

		APawn* TestPawn = World->SpawnActor<APawn>(APawn::StaticClass(), FTransform::Identity, SpawnParams);
		if (!TestNotNull(TEXT("Scan test pawn should spawn"), TestPawn))
		{
			return TPair<APawn*, AActor*>(nullptr, nullptr);
		}
		TestPawn->SetActorLocation(FixtureOrigin);
		TestPawn->SetActorRotation(FRotator::ZeroRotator);

		AActor* ScanTarget = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
		if (!TestNotNull(TEXT("Scan target actor should spawn"), ScanTarget))
		{
			TestPawn->Destroy();
			return TPair<APawn*, AActor*>(nullptr, nullptr);
		}
		ScanTarget->SetActorLocation(FixtureOrigin + FVector(250.0f, 0.0f, 0.0f));
		ScanTarget->Tags.Add(FName(TEXT("World.Interactable")));
		ScanTarget->Tags.Add(FName(TEXT("Item.Pickup")));
		ScanTarget->Tags.Add(FName(TEXT("World.POI")));
		return TPair<APawn*, AActor*>(TestPawn, ScanTarget);
	};

	const FString MissingRulesPath = TEXT("Data/Schema/Gameplay/ProjectMind/does_not_exist_scan_rules.json");
	{
		TPair<APawn*, AActor*> Fixture = SpawnScanFixture();
		APawn* TestPawn = Fixture.Key;
		AActor* ScanTarget = Fixture.Value;
		if (!TestPawn || !ScanTarget)
		{
			return false;
		}

		TSharedRef<FMindServiceImpl> RuntimeMindService = MakeShared<FMindServiceImpl>(FString(), FString(), MissingRulesPath);
		RuntimeMindService->SetGlobalEmitCooldownForTests(0.0);
		RuntimeMindService->SetLocalPlayerPawn(TestPawn);
		RuntimeMindService->Start();
		RuntimeMindService->PumpProvidersForTests(true);

		TestTrue(TEXT("Missing scan rule file should fallback to default rules"), RuntimeMindService->GetIdleScanRuleCountForTests() >= 1);

		RuntimeMindService->Stop();
		ScanTarget->Destroy();
		TestPawn->Destroy();
	}

	const FString InvalidRulesPath = MakeTempMappingPath(TEXT("scan_thought_rules_invalid.json"));
	FFileHelper::SaveStringToFile(TEXT("{ invalid-json "), *InvalidRulesPath);
	{
		TPair<APawn*, AActor*> Fixture = SpawnScanFixture();
		APawn* TestPawn = Fixture.Key;
		AActor* ScanTarget = Fixture.Value;
		if (!TestPawn || !ScanTarget)
		{
			IFileManager::Get().Delete(*InvalidRulesPath);
			return false;
		}

		TSharedRef<FMindServiceImpl> RuntimeMindService = MakeShared<FMindServiceImpl>(FString(), FString(), InvalidRulesPath);
		RuntimeMindService->SetGlobalEmitCooldownForTests(0.0);
		RuntimeMindService->SetLocalPlayerPawn(TestPawn);
		RuntimeMindService->Start();
		RuntimeMindService->PumpProvidersForTests(true);

		TestTrue(TEXT("Invalid scan rule file should fallback to default rules"), RuntimeMindService->GetIdleScanRuleCountForTests() >= 1);

		RuntimeMindService->Stop();
		ScanTarget->Destroy();
		TestPawn->Destroy();
	}

	IFileManager::Get().Delete(*InvalidRulesPath);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMindScanToastDoesNotPromoteToImportantTest,
	"ProjectIntegrationTests.UI.HUD.MindThought.Scan.ToastStaysToast",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMindScanToastDoesNotPromoteToImportantTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = ResolveMindTestWorld();
	if (!TestNotNull(TEXT("Automation world should exist"), World))
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;

	const FVector FixtureOrigin(0.0f, 0.0f, 5000.0f);
	APawn* TestPawn = World->SpawnActor<APawn>(APawn::StaticClass(), FTransform::Identity, SpawnParams);
	if (!TestNotNull(TEXT("Scan test pawn should spawn"), TestPawn))
	{
		return false;
	}
	TestPawn->SetActorLocation(FixtureOrigin);
	TestPawn->SetActorRotation(FRotator::ZeroRotator);

	AActor* ScanTarget = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
	if (!TestNotNull(TEXT("Scan target actor should spawn"), ScanTarget))
	{
		TestPawn->Destroy();
		return false;
	}
	ScanTarget->SetActorLocation(FixtureOrigin + FVector(220.0f, 0.0f, 0.0f));
	ScanTarget->Tags.Add(FName(TEXT("World.Interactable")));

	const FString RulesPath = MakeTempMappingPath(TEXT("scan_thought_rules_no_promo.json"));
	const FString RulesJson = TEXT("{")
		TEXT("\"$schema\":\"scan_thought_rules.schema.json\",")
		TEXT("\"entries\":[")
		TEXT("{")
		TEXT("\"rule_id\":\"test_high_priority_scan\",")
		TEXT("\"match_any_tags\":[\"World.Interactable\"],")
		TEXT("\"distance_cm\":{\"min\":0.0,\"max\":1000.0},")
		TEXT("\"min_view_dot\":0.1,")
		TEXT("\"min_screen_area_ratio\":0.0,")
		TEXT("\"los_required\":true,")
		TEXT("\"thought_id_prefix\":\"Mind.Scan.TestHigh\",")
		TEXT("\"text_template\":\"Test high scan: {actor_label}\",")
		TEXT("\"channel\":\"Toast\",")
		TEXT("\"priority_base\":400,")
		TEXT("\"source_type\":\"Scan\",")
		TEXT("\"time_to_live_sec\":6.0,")
		TEXT("\"cooldown_sec\":0.0,")
		TEXT("\"dedupe_window_sec\":0.0,")
		TEXT("\"dedupe_key_prefix\":\"Mind.Scan.TestHigh\"")
		TEXT("}")
		TEXT("]")
		TEXT("}");
	FFileHelper::SaveStringToFile(RulesJson, *RulesPath);

	TSharedRef<FMindServiceImpl> RuntimeMindService = MakeShared<FMindServiceImpl>(FString(), FString(), RulesPath);
	RuntimeMindService->SetGlobalEmitCooldownForTests(0.0);
	RuntimeMindService->SetLocalPlayerPawn(TestPawn);
	RuntimeMindService->Start();
	RuntimeMindService->PumpProvidersForTests(true);

	TArray<FMindThoughtView> Thoughts;
	RuntimeMindService->GetThoughtHistory(Thoughts);
	TestTrue(TEXT("Custom scan rule should emit at least one thought"), Thoughts.Num() >= 1);
	if (Thoughts.Num() > 0)
	{
		const FMindThoughtView& LastThought = Thoughts.Last();
		TestEqual(TEXT("High-priority scan thought should stay temporary toast"), LastThought.Channel, EMindThoughtChannel::Toast);
		TestEqual(TEXT("High-priority scan thought should stay Scan source"), LastThought.SourceType, EMindThoughtSourceType::Scan);
	}

	RuntimeMindService->Stop();
	IFileManager::Get().Delete(*RulesPath);
	ScanTarget->Destroy();
	TestPawn->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMindScanRequiresLineOfSightTest,
	"ProjectIntegrationTests.UI.HUD.MindThought.Scan.RequiresLineOfSight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMindScanRequiresLineOfSightTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = ResolveMindTestWorld();
	if (!TestNotNull(TEXT("Automation world should exist"), World))
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;

	const FVector FixtureOrigin(0.0f, 0.0f, 5000.0f);
	APawn* TestPawn = World->SpawnActor<APawn>(APawn::StaticClass(), FTransform::Identity, SpawnParams);
	if (!TestNotNull(TEXT("Scan test pawn should spawn"), TestPawn))
	{
		return false;
	}
	TestPawn->SetActorLocation(FixtureOrigin);
	TestPawn->SetActorRotation(FRotator::ZeroRotator);

	AActor* ScanTarget = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
	if (!TestNotNull(TEXT("Occluded scan target actor should spawn"), ScanTarget))
	{
		TestPawn->Destroy();
		return false;
	}
	ScanTarget->SetActorLocation(FixtureOrigin + FVector(250.0f, 0.0f, 0.0f));
	ScanTarget->Tags.Add(FName(TEXT("World.Interactable")));

	AActor* BlockerActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
	if (!TestNotNull(TEXT("LOS blocker actor should spawn"), BlockerActor))
	{
		ScanTarget->Destroy();
		TestPawn->Destroy();
		return false;
	}

	UBoxComponent* BlockerBox = NewObject<UBoxComponent>(BlockerActor, TEXT("MindTestLOSBlocker"));
	if (!TestNotNull(TEXT("LOS blocker component should be created"), BlockerBox))
	{
		BlockerActor->Destroy();
		ScanTarget->Destroy();
		TestPawn->Destroy();
		return false;
	}
	BlockerActor->SetRootComponent(BlockerBox);
	BlockerBox->SetBoxExtent(FVector(35.0f, 120.0f, 120.0f));
	BlockerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BlockerBox->SetCollisionObjectType(ECC_WorldStatic);
	BlockerBox->SetCollisionResponseToAllChannels(ECR_Block);
	BlockerBox->RegisterComponent();
	BlockerActor->SetActorLocation(FixtureOrigin + FVector(125.0f, 0.0f, 0.0f));

	const FString RulesPath = MakeTempMappingPath(TEXT("scan_thought_rules_los_only.json"));
	const FString RulesJson = TEXT("{")
		TEXT("\"$schema\":\"scan_thought_rules.schema.json\",")
		TEXT("\"entries\":[")
		TEXT("{")
		TEXT("\"rule_id\":\"test_los_required_scan\",")
		TEXT("\"match_any_tags\":[\"World.Interactable\"],")
		TEXT("\"distance_cm\":{\"min\":0.0,\"max\":1000.0},")
		TEXT("\"min_view_dot\":0.1,")
		TEXT("\"min_screen_area_ratio\":0.0,")
		TEXT("\"los_required\":true,")
		TEXT("\"thought_id_prefix\":\"Mind.Scan.TestLOS\",")
		TEXT("\"text_template\":\"Test LOS scan: {actor_label}\",")
		TEXT("\"channel\":\"Toast\",")
		TEXT("\"priority_base\":220,")
		TEXT("\"source_type\":\"Scan\",")
		TEXT("\"time_to_live_sec\":6.0,")
		TEXT("\"cooldown_sec\":0.0,")
		TEXT("\"dedupe_window_sec\":0.0,")
		TEXT("\"dedupe_key_prefix\":\"Mind.Scan.TestLOS\"")
		TEXT("}")
		TEXT("]")
		TEXT("}");
	FFileHelper::SaveStringToFile(RulesJson, *RulesPath);

	TSharedRef<FMindServiceImpl> RuntimeMindService = MakeShared<FMindServiceImpl>(FString(), FString(), RulesPath);
	RuntimeMindService->SetGlobalEmitCooldownForTests(0.0);
	RuntimeMindService->SetLocalPlayerPawn(TestPawn);
	RuntimeMindService->Start();
	RuntimeMindService->PumpProvidersForTests(true);

	TArray<FMindThoughtView> Thoughts;
	RuntimeMindService->GetThoughtHistory(Thoughts);
	TestEqual(TEXT("LOS-required scan should emit nothing when target is occluded"), Thoughts.Num(), 0);

	RuntimeMindService->Stop();
	IFileManager::Get().Delete(*RulesPath);
	BlockerActor->Destroy();
	ScanTarget->Destroy();
	TestPawn->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMindQuestSourceUnavailableFailSoftTest,
	"ProjectIntegrationTests.UI.HUD.MindThought.Quest.SourceUnavailableFailSoft",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMindQuestSourceUnavailableFailSoftTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TSharedPtr<IMindQuestThoughtSource> OriginalQuestSource = FProjectServiceLocator::Resolve<IMindQuestThoughtSource>();
	if (OriginalQuestSource.IsValid())
	{
		FProjectServiceLocator::Unregister<IMindQuestThoughtSource>();
	}

	TSharedRef<FMindServiceImpl> RuntimeMindService = MakeShared<FMindServiceImpl>();
	RuntimeMindService->Start();
	RuntimeMindService->PumpProvidersForTests(false);

	TArray<FMindThoughtView> Thoughts;
	RuntimeMindService->GetThoughtHistory(Thoughts);
	TestEqual(TEXT("Mind runtime should fail soft when quest source is unavailable"), Thoughts.Num(), 0);

	RuntimeMindService->Stop();

	if (OriginalQuestSource.IsValid())
	{
		FProjectServiceLocator::Register<IMindQuestThoughtSource>(OriginalQuestSource.ToSharedRef());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMindDialogueCooldownBehaviorTest,
	"ProjectIntegrationTests.UI.HUD.MindThought.Dialogue.CooldownBehavior",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMindDialogueCooldownBehaviorTest::RunTest(const FString& Parameters)
{
	TSharedPtr<IDialogueService> OriginalDialogueService = FProjectServiceLocator::Resolve<IDialogueService>();
	TSharedRef<FMockMindDialogueService> MockDialogueService = MakeShared<FMockMindDialogueService>();
	FProjectServiceLocator::Register<IDialogueService>(StaticCastSharedRef<IDialogueService>(MockDialogueService));

	TSharedRef<FMindServiceImpl> RuntimeMindService = MakeShared<FMindServiceImpl>();
	RuntimeMindService->Start();

	MockDialogueService->bIsActive = true;
	MockDialogueService->CurrentTreeId = FName(TEXT("DLG_GrandPa_Door"));
	MockDialogueService->CurrentNodeId = FName(TEXT("greeting"));
	MockDialogueService->BroadcastStateChanged();

	// Wait beyond global rate limiter to ensure this assertion specifically checks per-thought cooldown.
	FPlatformProcess::Sleep(1.6f);
	MockDialogueService->CurrentNodeId = FName(TEXT("backstory"));
	MockDialogueService->BroadcastStateChanged();

	TArray<FMindThoughtView> Thoughts;
	RuntimeMindService->GetThoughtHistory(Thoughts);
	TestEqual(TEXT("Cooldown should suppress duplicate thought_id emissions"), Thoughts.Num(), 1);

	RuntimeMindService->Stop();

	FProjectServiceLocator::Unregister<IDialogueService>();
	if (OriginalDialogueService.IsValid())
	{
		FProjectServiceLocator::Register<IDialogueService>(OriginalDialogueService.ToSharedRef());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMindDialogueDedupeBehaviorTest,
	"ProjectIntegrationTests.UI.HUD.MindThought.Dialogue.DedupeBehavior",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMindDialogueDedupeBehaviorTest::RunTest(const FString& Parameters)
{
	TSharedPtr<IDialogueService> OriginalDialogueService = FProjectServiceLocator::Resolve<IDialogueService>();
	TSharedRef<FMockMindDialogueService> MockDialogueService = MakeShared<FMockMindDialogueService>();
	FProjectServiceLocator::Register<IDialogueService>(StaticCastSharedRef<IDialogueService>(MockDialogueService));

	const FString MappingPath = MakeTempMappingPath(TEXT("dialogue_thought_mappings_dedupe.json"));
	const FString MappingJson = MakeDialogueMappingJson(
		TEXT("Mind.Dialogue.Test.First"),
		TEXT("Mind.Dialogue.Test.Second"),
		0.0,
		30.0,
		TEXT("Mind.Dialogue.Test.Shared"),
		TEXT("Mind.Dialogue.Test.Shared"));
	FFileHelper::SaveStringToFile(MappingJson, *MappingPath);

	TSharedRef<FMindServiceImpl> RuntimeMindService = MakeShared<FMindServiceImpl>(MappingPath);
	RuntimeMindService->Start();

	MockDialogueService->bIsActive = true;
	MockDialogueService->CurrentTreeId = FName(TEXT("DLG_GrandPa_Door"));
	MockDialogueService->CurrentNodeId = FName(TEXT("greeting"));
	MockDialogueService->BroadcastStateChanged();

	// Wait beyond global rate limiter to isolate dedupe behavior.
	FPlatformProcess::Sleep(1.6f);
	MockDialogueService->CurrentNodeId = FName(TEXT("backstory"));
	MockDialogueService->BroadcastStateChanged();

	TArray<FMindThoughtView> Thoughts;
	RuntimeMindService->GetThoughtHistory(Thoughts);
	TestEqual(TEXT("Shared dedupe key should suppress second thought"), Thoughts.Num(), 1);

	RuntimeMindService->Stop();
	IFileManager::Get().Delete(*MappingPath);

	FProjectServiceLocator::Unregister<IDialogueService>();
	if (OriginalDialogueService.IsValid())
	{
		FProjectServiceLocator::Register<IDialogueService>(OriginalDialogueService.ToSharedRef());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMindDialogueGlobalRateLimitBehaviorTest,
	"ProjectIntegrationTests.UI.HUD.MindThought.Dialogue.GlobalRateLimitBehavior",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMindDialogueGlobalRateLimitBehaviorTest::RunTest(const FString& Parameters)
{
	TSharedPtr<IDialogueService> OriginalDialogueService = FProjectServiceLocator::Resolve<IDialogueService>();
	TSharedRef<FMockMindDialogueService> MockDialogueService = MakeShared<FMockMindDialogueService>();
	FProjectServiceLocator::Register<IDialogueService>(StaticCastSharedRef<IDialogueService>(MockDialogueService));

	const FString MappingPath = MakeTempMappingPath(TEXT("dialogue_thought_mappings_global_rate.json"));
	const FString MappingJson = MakeDialogueMappingJson(
		TEXT("Mind.Dialogue.Test.Rate.First"),
		TEXT("Mind.Dialogue.Test.Rate.Second"),
		0.0,
		0.0,
		TEXT("Mind.Dialogue.Test.Rate.First"),
		TEXT("Mind.Dialogue.Test.Rate.Second"));
	FFileHelper::SaveStringToFile(MappingJson, *MappingPath);

	TSharedRef<FMindServiceImpl> RuntimeMindService = MakeShared<FMindServiceImpl>(MappingPath);
	RuntimeMindService->Start();

	MockDialogueService->bIsActive = true;
	MockDialogueService->CurrentTreeId = FName(TEXT("DLG_GrandPa_Door"));
	MockDialogueService->CurrentNodeId = FName(TEXT("greeting"));
	MockDialogueService->BroadcastStateChanged();

	// Immediate second state change should be blocked by global rate limiter.
	MockDialogueService->CurrentNodeId = FName(TEXT("backstory"));
	MockDialogueService->BroadcastStateChanged();

	TArray<FMindThoughtView> Thoughts;
	RuntimeMindService->GetThoughtHistory(Thoughts);
	TestEqual(TEXT("Global rate limiter should suppress immediate second thought"), Thoughts.Num(), 1);

	RuntimeMindService->Stop();
	IFileManager::Get().Delete(*MappingPath);

	FProjectServiceLocator::Unregister<IDialogueService>();
	if (OriginalDialogueService.IsValid())
	{
		FProjectServiceLocator::Register<IDialogueService>(OriginalDialogueService.ToSharedRef());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMindVitalsProviderBehaviorTest,
	"ProjectIntegrationTests.UI.HUD.MindThought.Vitals.ProviderBehavior",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMindVitalsProviderBehaviorTest::RunTest(const FString& Parameters)
{
	{
		TSharedRef<FMindServiceImpl> RuntimeMindService = MakeShared<FMindServiceImpl>();
		RuntimeMindService->Start();
		RuntimeMindService->PumpProvidersForTests();

		TArray<FMindThoughtView> Thoughts;
		RuntimeMindService->GetThoughtHistory(Thoughts);
		TestEqual(TEXT("Vitals provider should fail soft when no pawn context is available"), Thoughts.Num(), 0);

		RuntimeMindService->Stop();
	}

	{
		UWorld* World = ResolveMindTestWorld();
		if (!TestNotNull(TEXT("Automation world should exist"), World))
		{
			return false;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.ObjectFlags |= RF_Transient;
		APawn* TestPawn = World->SpawnActor<APawn>(APawn::StaticClass(), FTransform::Identity, SpawnParams);
		if (!TestNotNull(TEXT("Test pawn should spawn"), TestPawn))
		{
			return false;
		}

		UAbilitySystemComponent* ASC = NewObject<UAbilitySystemComponent>(TestPawn, TEXT("MindTestASC"));
		TestPawn->AddInstanceComponent(ASC);
		ASC->RegisterComponent();
		if (!TestNotNull(TEXT("ASC should be created"), ASC))
		{
			TestPawn->Destroy();
			return false;
		}

		ASC->InitAbilityActorInfo(TestPawn, TestPawn);
		ASC->AddLooseGameplayTag(ProjectTags::State_Hydration_Critical.GetTag());

		TSharedRef<FMindServiceImpl> RuntimeMindService = MakeShared<FMindServiceImpl>();
		RuntimeMindService->SetLocalPlayerPawn(TestPawn);
		RuntimeMindService->Start();
		RuntimeMindService->PumpProvidersForTests();

		TArray<FMindThoughtView> Thoughts;
		RuntimeMindService->GetThoughtHistory(Thoughts);
		TestTrue(TEXT("Vitals provider should emit at least one thought for critical hydration tag"), Thoughts.Num() >= 1);
		if (Thoughts.Num() > 0)
		{
			TestEqual(TEXT("Vitals provider should mark source type"), Thoughts.Last().SourceType, EMindThoughtSourceType::Vitals);
		}

		RuntimeMindService->Stop();
		TestPawn->Destroy();
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMindJournalViewModelRoutingVisibilityTest,
	"ProjectIntegrationTests.UI.HUD.MindThought.Journal.RoutingAndVisibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMindJournalViewModelRoutingVisibilityTest::RunTest(const FString& Parameters)
{
	TSharedPtr<IMindService> OriginalService = FProjectServiceLocator::Resolve<IMindService>();
	TSharedRef<FMockMindService> MockService = MakeShared<FMockMindService>();
	FProjectServiceLocator::Register<IMindService>(StaticCastSharedRef<IMindService>(MockService));

	UMindJournalViewModel* VM = NewObject<UMindJournalViewModel>(GetTransientPackage());
	VM->Initialize(nullptr);

	TestFalse(TEXT("Journal starts hidden"), VM->GetbPanelVisible());
	VM->TogglePanel();
	TestTrue(TEXT("TogglePanel shows journal"), VM->GetbPanelVisible());
	VM->TogglePanel();
	TestFalse(TEXT("Second TogglePanel hides journal"), VM->GetbPanelVisible());

	MockService->EmitThought(MakeThought(
		TEXT("Mind.Test.JournalOnly"),
		TEXT("Journal note"),
		8.0f,
		EMindThoughtChannel::Journal,
		EMindThoughtSourceType::Quest,
		80));
	MockService->EmitThought(MakeThought(
		TEXT("Mind.Test.ToastOnly"),
		TEXT("Toast hint"),
		4.0f,
		EMindThoughtChannel::Toast,
		EMindThoughtSourceType::System,
		20));
	MockService->EmitThought(MakeThought(
		TEXT("Mind.Test.Both"),
		TEXT("Durable both"),
		6.0f,
		EMindThoughtChannel::ToastAndJournal,
		EMindThoughtSourceType::Dialogue,
		90));

	const FString JournalText = VM->GetJournalText().ToString();
	const FString RecentText = VM->GetRecentText().ToString();

	TestTrue(TEXT("Journal tab should include journal-routed thoughts"), JournalText.Contains(TEXT("Journal note")));
	TestTrue(TEXT("Journal tab should include toast+journal thoughts"), JournalText.Contains(TEXT("Durable both")));
	TestFalse(TEXT("Journal tab should exclude toast-only thoughts"), JournalText.Contains(TEXT("Toast hint")));
	TestTrue(TEXT("Recent tab should include toast-routed thoughts"), RecentText.Contains(TEXT("Toast hint")));
	TestFalse(TEXT("Recent tab should exclude journal-only thoughts"), RecentText.Contains(TEXT("Journal note")));
	TestFalse(TEXT("Recent tab should exclude toast+journal durable thoughts"), RecentText.Contains(TEXT("Durable both")));

	VM->Shutdown();

	FProjectServiceLocator::Unregister<IMindService>();
	if (OriginalService.IsValid())
	{
		FProjectServiceLocator::Register<IMindService>(OriginalService.ToSharedRef());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMindJournalRecordLifecycleOrderingTest,
	"ProjectIntegrationTests.UI.HUD.MindThought.Journal.RecordLifecycleOrdering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMindJournalRecordLifecycleOrderingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TSharedPtr<IMindService> OriginalService = FProjectServiceLocator::Resolve<IMindService>();
	TSharedRef<FMockMindService> MockService = MakeShared<FMockMindService>();
	FProjectServiceLocator::Register<IMindService>(StaticCastSharedRef<IMindService>(MockService));

	UMindJournalViewModel* VM = NewObject<UMindJournalViewModel>(GetTransientPackage());
	VM->Initialize(nullptr);

	FMindThoughtView ActiveA = MakeThought(
		TEXT("Mind.Dialogue.Grandpa.NeedsWater"),
		TEXT("Grandpa needs water."),
		8.0f,
		EMindThoughtChannel::ToastAndJournal,
		EMindThoughtSourceType::Dialogue,
		90);
	ActiveA.RecordId = FName(TEXT("Record.Dialogue.Grandpa.NeedsWater"));
	ActiveA.RecordState = EMindRecordState::Active;
	MockService->EmitThought(ActiveA);

	FMindThoughtView ActiveB = MakeThought(
		TEXT("Mind.Scan.SOS.Actor"),
		TEXT("Someone nearby may need help."),
		8.0f,
		EMindThoughtChannel::ToastAndJournal,
		EMindThoughtSourceType::Quest,
		120);
	ActiveB.RecordId = FName(TEXT("Record.World.SOS.Nearby"));
	ActiveB.RecordState = EMindRecordState::Active;
	MockService->EmitThought(ActiveB);

	FMindThoughtView ResolvedA = MakeThought(
		TEXT("Mind.Dialogue.Grandpa.WaterDone"),
		TEXT("Grandpa has water now."),
		8.0f,
		EMindThoughtChannel::ToastAndJournal,
		EMindThoughtSourceType::Dialogue,
		90);
	ResolvedA.RecordId = FName(TEXT("Record.Dialogue.Grandpa.NeedsWater"));
	ResolvedA.RecordState = EMindRecordState::Resolved;
	MockService->EmitThought(ResolvedA);

	const FString JournalText = VM->GetJournalText().ToString();
	TestTrue(TEXT("Important should include active record"), JournalText.Contains(TEXT("Someone nearby may need help.")));
	TestTrue(TEXT("Important should include resolved record latest text"), JournalText.Contains(TEXT("Grandpa has water now.")));
	TestTrue(TEXT("Resolved marker should be visible"), JournalText.Contains(TEXT("[x]")));

	const int32 ActiveIndex = JournalText.Find(TEXT("Someone nearby may need help."));
	const int32 ResolvedIndex = JournalText.Find(TEXT("Grandpa has water now."));
	TestTrue(TEXT("Resolved records should be listed below active records"), ActiveIndex != INDEX_NONE && ResolvedIndex != INDEX_NONE && ResolvedIndex > ActiveIndex);

	VM->Shutdown();

	FProjectServiceLocator::Unregister<IMindService>();
	if (OriginalService.IsValid())
	{
		FProjectServiceLocator::Register<IMindService>(OriginalService.ToSharedRef());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMindJournalViewModelCollapseDuplicatesTest,
	"ProjectIntegrationTests.UI.HUD.MindThought.Journal.CollapseDuplicates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMindJournalViewModelCollapseDuplicatesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TSharedPtr<IMindService> OriginalService = FProjectServiceLocator::Resolve<IMindService>();
	TSharedRef<FMockMindService> MockService = MakeShared<FMockMindService>();
	FProjectServiceLocator::Register<IMindService>(StaticCastSharedRef<IMindService>(MockService));

	UMindJournalViewModel* VM = NewObject<UMindJournalViewModel>(GetTransientPackage());
	VM->Initialize(nullptr);

	MockService->EmitThought(MakeThought(
		TEXT("Mind.Test.JournalDup"),
		TEXT("Journal duplicate"),
		8.0f,
		EMindThoughtChannel::Journal,
		EMindThoughtSourceType::Quest,
		80));
	MockService->EmitThought(MakeThought(
		TEXT("Mind.Test.JournalDup"),
		TEXT("Journal duplicate"),
		8.0f,
		EMindThoughtChannel::Journal,
		EMindThoughtSourceType::Quest,
		80));
	MockService->EmitThought(MakeThought(
		TEXT("Mind.Test.JournalDup"),
		TEXT("Journal duplicate"),
		8.0f,
		EMindThoughtChannel::Journal,
		EMindThoughtSourceType::Quest,
		80));

	MockService->EmitThought(MakeThought(
		TEXT("Mind.Test.ToastDup"),
		TEXT("Toast duplicate"),
		4.0f,
		EMindThoughtChannel::Toast,
		EMindThoughtSourceType::Vitals,
		20));
	MockService->EmitThought(MakeThought(
		TEXT("Mind.Test.ToastDup"),
		TEXT("Toast duplicate"),
		4.0f,
		EMindThoughtChannel::Toast,
		EMindThoughtSourceType::Vitals,
		20));

	const FString JournalText = VM->GetJournalText().ToString();
	const FString RecentText = VM->GetRecentText().ToString();

	TestEqual(TEXT("Journal duplicates should be collapsed into a single line"), CountLinesContaining(JournalText, TEXT("Journal duplicate")), 1);
	TestTrue(TEXT("Journal collapse marker should include count"), JournalText.Contains(TEXT("Journal duplicate x3")));
	TestEqual(TEXT("Recent duplicates should be collapsed into a single line"), CountLinesContaining(RecentText, TEXT("Toast duplicate")), 1);
	TestTrue(TEXT("Recent collapse marker should include count"), RecentText.Contains(TEXT("Toast duplicate x2")));

	VM->Shutdown();

	FProjectServiceLocator::Unregister<IMindService>();
	if (OriginalService.IsValid())
	{
		FProjectServiceLocator::Register<IMindService>(OriginalService.ToSharedRef());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMindJournalLayoutDumpTest,
	"ProjectIntegrationTests.UI.HUD.MindThought.Journal.LayoutDump",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter
)

bool FMindJournalLayoutDumpTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* World = ResolveMindTestWorld();
	if (!TestNotNull(TEXT("Automation world should exist"), World))
	{
		return false;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!TestNotNull(TEXT("GameInstance should exist"), GameInstance))
	{
		return false;
	}

	UProjectUIDebugSubsystem* DebugSub = GameInstance->GetSubsystem<UProjectUIDebugSubsystem>();
	if (!TestNotNull(TEXT("ProjectUIDebugSubsystem should exist"), DebugSub))
	{
		return false;
	}

	APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!TestNotNull(TEXT("PlayerController should exist"), PlayerController))
	{
		return false;
	}

	UW_MindJournalPanel* Panel = CreateWidget<UW_MindJournalPanel>(PlayerController, UW_MindJournalPanel::StaticClass());
	if (!TestNotNull(TEXT("MindJournalPanel should be created"), Panel))
	{
		return false;
	}

	UMindJournalViewModel* ViewModel = NewObject<UMindJournalViewModel>(Panel);
	ViewModel->Initialize(nullptr);
	Panel->SetViewModel(ViewModel);
	ViewModel->ShowPanel();
	ViewModel->SetJournalTabActive(true);

	Panel->AddToViewport(50);
	Panel->SetVisibility(ESlateVisibility::Visible);
	Panel->ForceLayoutPrepass();

	ADD_LATENT_AUTOMATION_COMMAND(FDumpMindJournalAfterLayout(this, DebugSub, Panel, ViewModel, 2));

	return true;
}

#endif
