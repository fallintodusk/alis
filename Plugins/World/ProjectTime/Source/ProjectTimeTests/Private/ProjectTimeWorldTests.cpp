#include "ProjectTimeTestPolicyMode.h"
#include "ProjectTimeSubsystem.h"
#include "ProjectTimeWorldContextComponent.h"
#include "ProjectTimeDemoViewModel.h"
#include "ProjectTimeDemoWidget.h"
#include "Components/Slider.h"
#include "Components/Button.h"
#include "TimerManager.h"
#include "IAutomationDriverModule.h"
#include "IAutomationDriver.h"
#include "IDriverSequence.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "UObject/StrongObjectPtr.h"
#include "InputCoreTypes.h"
#include "ProjectTimeZone.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"

namespace
{
	struct FTimeWorld
	{
		UWorld* World;
		explicit FTimeWorld(EWorldType::Type Type = EWorldType::Game)
		{
			World = UWorld::CreateWorld(Type, false, FName(*FGuid::NewGuid().ToString()));
			auto& Context = GEngine->CreateNewWorldContext(Type);
			Context.SetCurrentWorld(World);
			if (Type == EWorldType::Game)
			{
				auto* Instance = NewObject<UGameInstance>(World);
				World->SetGameInstance(Instance);
				Context.OwningGameInstance = Instance;
				World->GetWorldSettings()->DefaultGameMode = AProjectTimeTestPolicyMode::StaticClass();
				World->SetGameMode(FURL());
				World->InitializeActorsForPlay(FURL());
			}
		}
		~FTimeWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
		UProjectTimeWorldContextComponent* AddContext()
		{
			auto* Owner = World->SpawnActor<AActor>();
			auto* Context = NewObject<UProjectTimeWorldContextComponent>(Owner);
			Owner->AddInstanceComponent(Context);
			Context->ZoneId = TEXT("Europe/Moscow");
			Context->ProfileId = TEXT("time_fixture");
			Context->ProfileHash = FString::ChrN(64, TCHAR('a'));
			Context->RegisterComponent();
			return Context;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectTimeWorldTest, "Project.Time.Clock.WorldIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProjectTimeWorldTest::RunTest(const FString&)
{
	FTimeWorld A, B, Editor(EWorldType::Editor);
	TestNull(TEXT("No runtime clock in Editor"), Editor.World->GetSubsystem<UProjectTimeSubsystem>());
	auto* TimeA = A.World->GetSubsystem<UProjectTimeSubsystem>();
	auto* TimeB = B.World->GetSubsystem<UProjectTimeSubsystem>();
	if (!TestNotNull(TEXT("A clock"), TimeA) || !TestNotNull(TEXT("B clock"), TimeB)) return false;
	FString Error;
	TestTrue(TEXT("Context A"), TimeA->BindContext(*A.AddContext(), Error));
	TestTrue(TEXT("Context B"), TimeB->BindContext(*B.AddContext(), Error));
	TestFalse(TEXT("Foreign context refuses"), TimeA->BindContext(*B.AddContext(), Error));
	const FDateTime Start(2026, 6, 15, 12);
	TestFalse(TEXT("Mode defaults to refusal"), TimeA->SeekUtc(Start));
	A.World->GetAuthGameMode<AProjectTimeTestPolicyMode>()->bAllowControls = true;
	B.World->GetAuthGameMode<AProjectTimeTestPolicyMode>()->bAllowControls = true;
	TestTrue(TEXT("A admitted seek"), TimeA->SeekUtc(Start));
	TestTrue(TEXT("B admitted seek"), TimeB->SeekUtc(Start + FTimespan::FromDays(1)));
	FProjectTimeSnapshot Value;
	TimeA->TryReadTime(Value);
	TestEqual(TEXT("A owns its paused instant"), Value.Utc, Start);
	TimeB->TryReadTime(Value);
	TestEqual(TEXT("B is independent"), Value.Utc, Start + FTimespan::FromDays(1));
	TimeA->UnbindContext();
	TestFalse(TEXT("Unbound A refuses reads"), TimeA->TryReadTime(Value));
	TestTrue(TEXT("B still reads"), TimeB->TryReadTime(Value));
	B.World->GetAuthGameMode<AProjectTimeTestPolicyMode>()->bAllowControls = false;
	TimeB->TryReadTime(Value);
	TestFalse(TEXT("Revocation restores Live"), Value.bDemo);
	TestFalse(TEXT("Revocation refuses rate changes"), TimeB->SetRate(60));
	return true;
}

namespace
{
	class FPausedRevocation final : public IAutomationLatentCommand
	{
	public:
		FAutomationTestBase& Test;
		FTimeWorld Fixture;
		int32 Frames = 0;
		int32 Notifications = 0;
		explicit FPausedRevocation(FAutomationTestBase& InTest) : Test(InTest)
		{
			auto* Time = Fixture.World->GetSubsystem<UProjectTimeSubsystem>();
			FString Error;
			Time->BindContext(*Fixture.AddContext(), Error);
			Fixture.World->GetAuthGameMode<AProjectTimeTestPolicyMode>()->bAllowControls = true;
			Time->SeekUtc(FDateTime(2026, 6, 15, 12));
			Time->TimeChanged().AddRaw(this, &FPausedRevocation::Notified);
			Fixture.World->GetAuthGameMode<AProjectTimeTestPolicyMode>()->bAllowControls = false;
		}
		void Notified() { ++Notifications; }
		virtual bool Update() override
		{
			// Native timers activate pending entries after their first engine frame.
			Fixture.World->GetTimerManager().Tick(0.6f);
			if (++Frames < 3) return false;
			auto* Time = Fixture.World->GetSubsystem<UProjectTimeSubsystem>();
			Test.TestTrue(TEXT("Paused preview revokes without UI or clock reads"), Notifications > 0);
			FProjectTimeSnapshot Value;
			Time->TryReadTime(Value);
			Test.TestFalse(TEXT("Revoked paused clock returns Live"), Value.bDemo);
			Time->TimeChanged().RemoveAll(this);
			return true;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectTimePausedRevocationTest, "Project.Time.Clock.PausedRevocation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProjectTimePausedRevocationTest::RunTest(const FString&)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FPausedRevocation>(*this));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectTimeUiTest, "Project.Time.DemoUI.Controls.Session",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProjectTimeUiTest::RunTest(const FString&)
{
	FTimeWorld Fixture;
	auto* Time = Fixture.World->GetSubsystem<UProjectTimeSubsystem>();
	if (!TestNotNull(TEXT("Time"), Time)) return false;
	FString Error;
	TestTrue(TEXT("Context binds"), Time->BindContext(*Fixture.AddContext(), Error));
	auto* Model = NewObject<UProjectTimeDemoViewModel>(Fixture.World);
	Model->Initialize(Fixture.World);
	Model->ChangeRate(60);
	FProjectTimeSnapshot Value;
	Time->TryReadTime(Value);
	TestFalse(TEXT("UI cannot grant mode permission"), Value.bDemo);
	Fixture.World->GetAuthGameMode<AProjectTimeTestPolicyMode>()->bAllowControls = true;
	Model->BeginScrub();
	Model->SeekHours(3);
	Time->TryReadTime(Value);
	const auto Scrubbed = Value.Utc;
	TestTrue(TEXT("Scrub enters Demo"), Value.bDemo);
	TestEqual(TEXT("Scrub pauses calendar"), Value.Rate, 0.0);
	Model->Refresh();
	Time->TryReadTime(Value);
	TestEqual(TEXT("Model refresh cannot resend a command"), Value.Utc, Scrubbed);
	Model->ChangeRate(30);
	Time->TryReadTime(Value);
	TestEqual(TEXT("Rate command reaches clock"), Value.Rate, 30.0);
	auto* Widget = CreateWidget<UProjectTimeDemoWidget>(Fixture.World->GetGameInstance());
	if (!TestNotNull(TEXT("Native panel creates"), Widget)) return false;
	Widget->SetViewModel(Model);
	const auto SlateWidget = Widget->TakeWidget();
	auto* Offset = Cast<USlider>(Widget->GetWidgetFromName(TEXT("OffsetSlider")));
	auto* Rate = Cast<USlider>(Widget->GetWidgetFromName(TEXT("RateSlider")));
	auto* Reset = Cast<UButton>(Widget->GetWidgetFromName(TEXT("ResetButton")));
	if (!TestNotNull(TEXT("JSON offset slider"), Offset) || !TestNotNull(TEXT("JSON rate slider"), Rate) ||
		!TestNotNull(TEXT("JSON reset button"), Reset)) return false;
	Offset->OnMouseCaptureBegin.Broadcast();
	Offset->OnValueChanged.Broadcast(-2);
	Time->TryReadTime(Value);
	TestEqual(TEXT("Actual offset widget command pauses preview"), Value.Rate, 0.0);
	Rate->OnValueChanged.Broadcast(10);
	Time->TryReadTime(Value);
	TestEqual(TEXT("Actual rate widget command reaches clock"), Value.Rate, 10.0);
	Widget->RefreshFromViewModel();
	Time->TryReadTime(Value);
	TestEqual(TEXT("Widget refresh cannot feed back into a seek"), Value.Rate, 10.0);
	Reset->OnClicked.Broadcast();
	Time->TryReadTime(Value);
	TestFalse(TEXT("Actual reset button returns Live"), Value.bDemo);
	Widget->SetViewModel(nullptr);
	Widget->NativeDestruct();
	Model->Reset();
	Time->TryReadTime(Value);
	TestFalse(TEXT("Reset leaves Demo"), Value.bDemo);
	Model->Shutdown();
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectTimeWorldTest, "Project.Time.Clock.WorldIsolation", "[Fast][Integration][Time]")
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectTimePausedRevocationTest, "Project.Time.Clock.PausedRevocation", "[Fast][Integration][Time]")
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectTimeUiTest, "Project.Time.DemoUI.Controls.Session", "[Fast][Integration][UI]")

namespace
{
	class FTimeGestureProof final : public IAutomationLatentCommand
	{
		FAutomationTestBase& Test;
		FTimeWorld Fixture;
		TStrongObjectPtr<UProjectTimeDemoWidget> Widget;
		UProjectTimeDemoViewModel* Model = nullptr;
		TSharedPtr<SWindow> Window;
		TSharedPtr<IAsyncAutomationDriver, ESPMode::ThreadSafe> Driver;
		TSharedPtr<IAsyncDriverSequence, ESPMode::ThreadSafe> Sequence;
		TAsyncResult<bool> Result;
		double Started = FPlatformTime::Seconds();
		int32 Step = 0, Closes = 0;
		double PreviousRate = 0;
		bool bWasEnabled = false;
		void Closed() { ++Closes; }
		FVector2D Center(const TCHAR* Name) const
		{
			const auto& Geometry = Widget->GetWidgetFromName(Name)->GetCachedGeometry();
			return Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f);
		}
		void Drag(const TCHAR* Name)
		{
			Sequence = Driver->CreateSequence();
			Sequence->Actions().MoveToPosition(Center(Name)).Press(EMouseButtons::Left)
				.MoveByOffset(60, 0).Release(EMouseButtons::Left);
			Result = Sequence->Perform();
		}
		bool Finish()
		{
			if (Model) { Model->CloseRequested.RemoveAll(this); Model->Shutdown(); }
			if (Widget.IsValid()) Widget->SetViewModel(nullptr);
			if (Window) FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());
			if (!bWasEnabled && Driver) IAutomationDriverModule::Get().Disable();
			return true;
		}
	public:
		explicit FTimeGestureProof(FAutomationTestBase& InTest) : Test(InTest),
			Widget(CreateWidget<UProjectTimeDemoWidget>(Fixture.World->GetGameInstance()))
		{
			auto* Time = Fixture.World->GetSubsystem<UProjectTimeSubsystem>();
			FString Error;
			Time->BindContext(*Fixture.AddContext(), Error);
			Fixture.World->GetAuthGameMode<AProjectTimeTestPolicyMode>()->bAllowControls = true;
			Model = NewObject<UProjectTimeDemoViewModel>(Fixture.World);
			Model->Initialize(Fixture.World);
			Model->CloseRequested.AddRaw(this, &FTimeGestureProof::Closed);
			Widget->SetViewModel(Model);
			Window = SNew(SWindow).Title(FText::FromString(TEXT("Time controls fixture")))
				.ClientSize(FVector2D(800, 400)).SupportsMaximize(false).SupportsMinimize(false)[Widget->TakeWidget()];
			FSlateApplication::Get().AddWindow(Window.ToSharedRef());
			auto& Module = IAutomationDriverModule::Get();
			bWasEnabled = Module.IsEnabled();
			if (!bWasEnabled) Module.Enable();
			Driver = Module.CreateAsyncDriver();
		}
		virtual bool Update() override
		{
			if (FPlatformTime::Seconds() - Started > 30)
			{
				Test.AddError(TEXT("Native Time gesture routing timed out."));
				return Finish();
			}
			if (Step == 0)
			{
				if (Widget->GetWidgetFromName(TEXT("OffsetSlider"))->GetCachedGeometry().GetLocalSize().X <= 0) return false;
				Drag(TEXT("OffsetSlider"));
				Step = 1;
				return false;
			}
			if (!Result.GetFuture().IsReady()) return false;
			Test.TestTrue(TEXT("Native Slate input sequence completes"), Result.GetFuture().Get());
			auto* Time = Fixture.World->GetSubsystem<UProjectTimeSubsystem>();
			FProjectTimeSnapshot Snapshot;
			Time->TryReadTime(Snapshot);
			if (Step == 1)
			{
				Test.TestTrue(TEXT("Mouse drag seeks paused Demo"), Snapshot.bDemo && Snapshot.Rate == 0 && Model->GetOffset() > 1);
				Drag(TEXT("RateSlider"));
			}
			else if (Step == 2)
			{
				Test.TestTrue(TEXT("Speed mouse drag resumes calendar"), Snapshot.Rate > 1);
				PreviousRate = Snapshot.Rate;
				Sequence = Driver->CreateSequence();
				Sequence->Actions().Press(EKeys::Enter).Release(EKeys::Enter).Press(EKeys::Right).Release(EKeys::Right)
					.Press(EKeys::Enter).Release(EKeys::Enter);
				Result = Sequence->Perform();
			}
			else if (Step == 3)
			{
				Test.TestTrue(TEXT("Focused slider accepts keyboard adjustment"), Snapshot.Rate > PreviousRate);
				Sequence = Driver->CreateSequence();
				Sequence->Actions().MoveToPosition(Center(TEXT("ResetButton"))).Press(EMouseButtons::Left).Release(EMouseButtons::Left);
				Result = Sequence->Perform();
			}
			else if (Step == 4)
			{
				Test.TestFalse(TEXT("Real Reset click restores Live"), Snapshot.bDemo);
				Sequence = Driver->CreateSequence();
				Sequence->Actions().Press(EKeys::Escape).Release(EKeys::Escape);
				Result = Sequence->Perform();
			}
			else
			{
				Test.TestTrue(TEXT("Escape bubbles to panel Close"), Closes > 0);
				return Finish();
			}
			++Step;
			return false;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectTimeGestureTest, "Project.Time.DemoUI.GestureRouting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProjectTimeGestureTest::RunTest(const FString&)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FTimeGestureProof>(*this));
	return true;
}
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectTimeGestureTest, "Project.Time.DemoUI.GestureRouting", "[Slow][E2E][UI]")
