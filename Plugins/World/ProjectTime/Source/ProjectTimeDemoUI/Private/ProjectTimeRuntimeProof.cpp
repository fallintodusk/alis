#include "ProjectTimeRuntimeProof.h"

#if !UE_BUILD_SHIPPING
#include "ProjectTimeSubsystem.h"
#include "ProjectTimeDemoUISubsystem.h"
#include "ProjectTimeDemoWidget.h"
#include "ProjectTimeDemoViewModel.h"
#include "ProjectServiceLocator.h"
#include "Services/ILoadingService.h"
#include "Experience/ProjectExperienceRegistry.h"
#include "Experience/ProjectExperienceDescriptorBase.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "ChartCreation.h"
#include "ContentStreaming.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectTimeRuntimeProof, Log, All);

namespace
{
	class FProof final : public IPerformanceDataConsumer, public TSharedFromThis<FProof>
	{
	public:
		FString Root;
		FDateTime Noon, Dusk;
		int32 Phase = -1;
		double Started = FPlatformTime::Seconds(), PhaseStarted = 0;
		bool bCapturePending = false;
		bool bCaptureResolutionRequested = false;
		double TourRate = -1;
		uint64 ResetFrame = 0;
		int32 TransitionCaptures = 0;
		FString Samples = TEXT("phase,frame,frame_ms,game_ms,render_ms,gpu_ms,rhi_ms\n");
		TWeakObjectPtr<UProjectTimeSubsystem> Time;
		TWeakObjectPtr<UProjectTimeDemoUISubsystem> Controls;
		virtual void StartCharting() override {}
		virtual void StopCharting() override {}
		virtual void ProcessFrame(const FFrameData& Frame) override
		{
			Samples += FString::Printf(TEXT("%d,%llu,%.6f,%.6f,%.6f,%.6f,%.6f\n"), Phase, GFrameCounter,
				Frame.TrueDeltaSeconds * 1000, Frame.GameThreadTimeSeconds * 1000,
				Frame.RenderThreadTimeSeconds * 1000, Frame.GPUTimeSeconds * 1000, Frame.RHIThreadTimeSeconds * 1000);
		}
		bool Finish(const TCHAR* Error = nullptr)
		{
			if (Controls.IsValid()) Controls->Close();
			if (Time.IsValid()) Time->ResetToPcTime();
			FFileHelper::SaveStringToFile(Samples, *(Root / TEXT("timings.csv")));
			const FString Result = Error ? FString::Printf(TEXT("status=rejected\nreason=%s\n"), Error) : TEXT("status=accepted\n");
			FFileHelper::SaveStringToFile(Result, *(Root / TEXT("result.txt")));
			FPlatformMisc::RequestExitWithStatus(false, Error ? 1 : 0);
			return false;
		}
		bool SeekPhase()
		{
			const int32 Hours[] = {-6, 0, 6, 12};
			PhaseStarted = FPlatformTime::Seconds();
			if (!Time->SeekUtc(Phase == 2 ? Dusk : Noon + FTimespan::FromHours(Phase < 4 ? Hours[Phase] : 0))) return false;
			const TCHAR* CivilHours[] = {TEXT("06:00:00"), TEXT("12:00:00"), TEXT("18:00:00"), TEXT("00:00:00")};
			return Phase == 2 || Time->GetLocalTimeText().ToString().Contains(CivilHours[Phase < 4 ? Phase : 1]);
		}
		bool Tick(float)
		{
			// The existing product-route gate owns traversal completion and timeout.
			if (Phase == 99) return true;
			const double Now = FPlatformTime::Seconds();
			if (Now - Started > 300) return Finish(TEXT("ready-or-capture-timeout"));
			UWorld* World = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;
			if (!World) return true;
			if (Phase < 0)
			{
				auto* Provider = World->GetSubsystem<UProjectTimeSubsystem>();
				auto* Player = World->GetFirstPlayerController();
				if (!Provider || !Provider->CanControlTime() || !Player || !Player->GetPawn() || !Player->GetLocalPlayer()) return true;
				FProjectTimeSnapshot Snapshot;
				if (!Provider->TryReadTime(Snapshot) || Snapshot.bDemo ||
					FMath::Abs((Snapshot.Utc - FDateTime::UtcNow()).GetTotalSeconds()) > 5) return Finish(TEXT("default-not-pc-live"));
				Time = Provider;
				if (TourRate >= 0)
				{
					if (!Provider->SeekUtc(Noon) || !Provider->SetRate(TourRate)) return Finish(TEXT("tour-time-refused"));
					FFileHelper::SaveStringToFile(FString::Printf(TEXT("utc=%s\nrate=%.3f\n"),
						*Noon.ToIso8601(), TourRate), *(Root / TEXT("tour-clock.txt")));
					Phase = 99;
					return true;
				}
				// Traversal has its own envelope owner; UI capture waits for realized size.
				if (!GEngine->GameViewport->Viewport) return true;
				if (!bCaptureResolutionRequested)
				{
					// Save-load graphics settings can override the startup command line.
					bCaptureResolutionRequested = true;
					FSystemResolution::RequestResolutionChange(2560, 1440, EWindowMode::Windowed);
					return true;
				}
				const auto Size = GEngine->GameViewport->Viewport->GetSizeXY();
				if (Size != FIntPoint(2560, 1440)) return true;
				FFileHelper::SaveStringToFile(FString::Printf(TEXT("world=%s\nwidth=%d\nheight=%d\n"),
					*World->GetOutermost()->GetName(), Size.X, Size.Y), *(Root / TEXT("envelope.txt")));
				GEngine->AddPerformanceDataConsumer(AsShared());
				Controls = Player->GetLocalPlayer()->GetSubsystem<UProjectTimeDemoUISubsystem>();
				if (!Controls.IsValid()) return Finish(TEXT("client-ui-subsystem-absent"));
				Controls->Toggle();
				TArray<UUserWidget*> Panels;
				UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Panels, UProjectTimeDemoWidget::StaticClass(), false);
				auto** Panel = Panels.FindByPredicate([](const UUserWidget* Widget) { return Widget->IsInViewport() && Widget->IsVisible(); });
				if (!Panel) return Finish(TEXT("staged-ui-definition-or-layout-absent"));
				auto* NativePanel = Cast<UProjectTimeDemoWidget>(*Panel);
				auto* Model = Cast<UProjectTimeDemoViewModel>(NativePanel->GetViewModel());
				auto* Rate = Cast<USlider>(NativePanel->GetWidgetFromName(TEXT("RateSlider")));
				auto* Label = Cast<UTextBlock>(NativePanel->GetWidgetFromName(TEXT("TimeLabel")));
				if (!Model || !Rate || !Label || Label->GetText().IsEmpty()) return Finish(TEXT("cooked-widget-contract-incomplete"));
				Rate->OnValueChanged.Broadcast(60);
				Provider->TryReadTime(Snapshot);
				if (Snapshot.Rate != 60 || World->IsPaused()) return Finish(TEXT("cooked-ui-rate-command-failed"));
				FFileHelper::SaveStringToFile(Label->GetText().ToString(), *(Root / TEXT("civil-label.txt")));
				Phase = 0;
				if (!SeekPhase()) return Finish(TEXT("initial-seek-refused"));
				return true;
			}
			if (Time->GetWorld() != World) return Finish(TEXT("world-changed-during-proof"));
			if (bCapturePending)
			{
				if (FScreenshotRequest::IsScreenshotRequested()) return true;
				if (IFileManager::Get().FileSize(*(Root / FString::Printf(TEXT("phase-%d.png"), Phase))) <= 0)
					return Finish(TEXT("screenshot-not-written"));
				Controls->Close();
				bCapturePending = false;
				++Phase;
				if (!SeekPhase()) return Finish(TEXT("phase-seek-refused"));
				return true;
			}
			if (Phase == 7)
			{
				if (GFrameCounter - ResetFrame < 10) return true;
				const auto Loading = FProjectServiceLocator::Resolve<ILoadingService>();
				FName Name; const UWorld* Loaded = nullptr;
				if (!Loading || !Loading->TryGetLoadedExperienceContext(Name, Loaded) || Loaded != World)
					return Finish(TEXT("confirmed-cooked-experience-absent"));
				auto* Descriptor = UProjectExperienceRegistry::Get()->FindDescriptor(Name);
				if (!Descriptor) return Finish(TEXT("resolved-experience-absent"));
				const bool Previous = Descriptor->AllowEnvironmentalTimeControls;
				Descriptor->AllowEnvironmentalTimeControls = false;
				const bool bRefused = !Time->SeekUtc(Noon) && !Time->SetRate(60);
				FProjectTimeSnapshot Value;
				const bool bLive = Time->TryReadTime(Value) && !Value.bDemo;
				Descriptor->AllowEnvironmentalTimeControls = Previous;
				if (!bRefused || !bLive) return Finish(TEXT("cooked-policy-revocation-failed"));
				return Finish();
			}
			if (FScreenshotRequest::IsScreenshotRequested()) return true;
			if (Phase == 6 && TransitionCaptures < 6 && Now - PhaseStarted >= 10 * (TransitionCaptures + 1))
			{
				UE_LOG(LogProjectTimeRuntimeProof, Display, TEXT("Transition capture=%d frame=%llu"), TransitionCaptures, GFrameCounter);
				FScreenshotRequest::RequestScreenshot(Root / FString::Printf(TEXT("transition-%d.png"), TransitionCaptures++), false, false);
				return true;
			}
			if (Now - PhaseStarted < (Phase == 6 ? 60 : 20)) return true;
			if (Phase < 4)
			{
				if (IStreamingManager::Get().GetNumWantingResources() > 0 || IsAsyncLoading()) return true;
				FScreenshotRequest::RequestScreenshot(Root / FString::Printf(TEXT("phase-%d.png"), Phase), Phase == 0, false);
				bCapturePending = true;
				return true;
			}
			if (Phase < 6)
			{
				++Phase;
				if (Phase == 6 && !Time->SeekUtc(Dusk - FTimespan::FromMinutes(10))) return Finish(TEXT("sunset-seek-refused"));
				if (!Time->SetRate(Phase == 5 ? 1 : 60)) return Finish(TEXT("cadence-resume-failed"));
				PhaseStarted = Now;
				return true;
			}
			if (!Time->ResetToPcTime()) return Finish(TEXT("reset-refused"));
			FProjectTimeSnapshot Value;
			if (!Time->TryReadTime(Value) || Value.bDemo) return Finish(TEXT("reset-not-live"));
			Phase = 7; ResetFrame = GFrameCounter;
			return true;
		}
	};
	TSharedPtr<FProof> Proof;
	FTSTicker::FDelegateHandle ProofTicker;
}
#endif

void ProjectTimeRuntimeProof::Start()
{
#if !UE_BUILD_SHIPPING
	if (GIsEditor || IsRunningDedicatedServer() || IsRunningCommandlet()) return;
	FString Root, Utc;
	if (!FParse::Value(FCommandLine::Get(), TEXT("ProjectTimeRuntimeProofRoot="), Root)) return;
	FPaths::NormalizeDirectoryName(Root);
	Root = FPaths::ConvertRelativePathToFull(Root);
	if (!Root.Contains(TEXT("/tmp/world/")) || Root.Contains(TEXT("/../")) ||
		!FParse::Value(FCommandLine::Get(), TEXT("ProjectTimeRuntimeProofNoonUtc="), Utc)) return;
	FDateTime Noon;
	if (!FDateTime::ParseIso8601(*Utc, Noon)) return;
	if (Noon.GetTicks() < FTimespan::FromHours(6).GetTicks() ||
		FDateTime::MaxValue().GetTicks() - Noon.GetTicks() < FTimespan::FromHours(12).GetTicks()) return;
	FDateTime Dusk = Noon + FTimespan::FromHours(6);
	FString DuskText;
	if (FParse::Value(FCommandLine::Get(), TEXT("ProjectTimeRuntimeProofDuskUtc="), DuskText) &&
		!FDateTime::ParseIso8601(*DuskText, Dusk)) return;
	IFileManager::Get().MakeDirectory(*Root, true);
	Proof = MakeShared<FProof>();
	Proof->Root = Root; Proof->Noon = Noon; Proof->Dusk = Dusk;
	FParse::Value(FCommandLine::Get(), TEXT("ProjectTimeRuntimeTourRate="), Proof->TourRate);
	ProofTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(Proof.ToSharedRef(), &FProof::Tick));
#endif
}

void ProjectTimeRuntimeProof::Stop()
{
#if !UE_BUILD_SHIPPING
	FTSTicker::GetCoreTicker().RemoveTicker(ProofTicker);
	if (Proof && GEngine) GEngine->RemovePerformanceDataConsumer(Proof);
	Proof.Reset();
#endif
}
