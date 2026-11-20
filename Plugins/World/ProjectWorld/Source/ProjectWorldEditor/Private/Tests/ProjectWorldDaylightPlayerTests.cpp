#include "ProjectTimeSubsystem.h"
#include "ProjectTimeDemoUISubsystem.h"
#include "ProjectTimeDemoWidget.h"
#include "ProjectLightComponent.h"
#include "GeoReferencingSystem.h"
#include "Components/Slider.h"
#include "Components/Button.h"
#include "Components/CapsuleComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "ChartCreation.h"
#include "ContentStreaming.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "Editor.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameMapsSettings.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Slate/SceneViewport.h"
#include "SceneView.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "ImageUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	class FDaylightFrames final : public IPerformanceDataConsumer
	{
	public:
		int32 Phase = 0;
		FString Rows = TEXT("phase,frame,frame_ms,game_ms,render_ms,gpu_ms,rhi_ms\n");
		virtual void StartCharting() override {}
		virtual void StopCharting() override {}
		virtual void ProcessFrame(const FFrameData& Frame) override
		{
			Rows += FString::Printf(TEXT("%d,%llu,%.6f,%.6f,%.6f,%.6f,%.6f\n"), Phase, GFrameCounter,
				Frame.TrueDeltaSeconds * 1000.0, Frame.GameThreadTimeSeconds * 1000.0,
				Frame.RenderThreadTimeSeconds * 1000.0, Frame.GPUTimeSeconds * 1000.0, Frame.RHIThreadTimeSeconds * 1000.0);
		}
	};

	class FStartDaylightPIE final : public FStartPIEForAutomationCommand
	{
		TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
	public:
		explicit FStartDaylightPIE(const FRequestPlaySessionParams& Params)
			: FStartPIEForAutomationCommand(Params), Settings(Params.EditorPlaySettings.Get()) {}
	};

	class FDaylightPlayerProof final : public IAutomationLatentCommand
	{
		FAutomationTestBase& Test;
		FString Root;
		double Started = FPlatformTime::Seconds();
		double PhaseStarted = 0;
		int32 Phase = -1;
		bool bManhattan;
		bool bCapturePending = false;
		uint64 ResetFrame = 0, ResetApplyCount = 0;
		TWeakObjectPtr<UProjectTimeSubsystem> Time;
		TWeakObjectPtr<UProjectTimeDemoUISubsystem> Controls;
		FString Frames = TEXT("phase,frame,delta_ms,light_apply_count,last_apply_frame\n");
		TWeakObjectPtr<UProjectLightComponent> Light;
		TSharedPtr<FDaylightFrames> Samples;
		bool bStreetView = FParse::Param(FCommandLine::Get(), TEXT("ProjectTimeProofStreet"));
		bool bVisualOnly = FParse::Param(FCommandLine::Get(), TEXT("ProjectTimeProofVisualOnly"));
		bool bStreetPlaced = false;
		bool bUiCaptured = false;
		bool bNightCompare = FParse::Param(FCommandLine::Get(), TEXT("ProjectTimeProofNightCompare"));
		static constexpr double VisualSettleSeconds = 75.0;
		bool bUiOnly = FParse::Param(FCommandLine::Get(), TEXT("ProjectTimeProofUiOnly"));
		bool bSettingsSaved = false, bCloudVisible = true;
		FProjectLightSettings OriginalSettings;
		TOptional<float> NightFloor;
		int32 CaptureCount() const { return bUiOnly ? 1 : bNightCompare ? 5 : 4; }
		bool CaptureUi(UWorld& World)
		{
			TArray<UUserWidget*> Panels;
			UWidgetBlueprintLibrary::GetAllWidgetsOfClass(&World, Panels, UProjectTimeDemoWidget::StaticClass(), false);
			auto** Panel = Panels.FindByPredicate([](const UUserWidget* Widget) { return Widget->IsInViewport() && Widget->IsVisible(); });
			if (!Test.TestTrue(TEXT("UI capture has an actual player panel"), Panel && (*Panel)->GetCachedWidget().IsValid())) return false;
			// A global screenshot request can be consumed by the Editor viewport after a scene shot.
			const auto Window = FSlateApplication::Get().FindWidgetWindow((*Panel)->GetCachedWidget().ToSharedRef());
			if (!Test.TestTrue(TEXT("UI capture targets the panel's own native window"), Window.IsValid())) return false;
			TArray<FColor> Pixels; FIntVector Size;
			if (!Test.TestTrue(TEXT("Targeted Slate screenshot succeeds"), FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(), Pixels, Size))) return false;
			int32 ControlPixels = 0;
			for (FColor& Color : Pixels)
			{
				if (Color.R < 100 && Color.G > 100 && Color.B > 120) ++ControlPixels;
				Color.A = 255;
			}
			// Reject the known empty Editor target; this sanity check is not semantic UI acceptance.
			if (!Test.TestTrue(TEXT("UI image retains visible control pixels"), ControlPixels > 100)) return false;
			TArray64<uint8> Png;
			FImageUtils::PNGCompressImageArray(Size.X, Size.Y, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
			Test.AddInfo(FString::Printf(TEXT("Targeted UI image title=%s width=%d height=%d control_pixels=%d"),
				*Window->GetTitle().ToString(), Size.X, Size.Y, ControlPixels));
			return Test.TestTrue(TEXT("UI original written"), FFileHelper::SaveArrayToFile(Png, *(Root / TEXT("ui.png"))));
		}
		bool PlaceStreetPlayer(UWorld& World, APlayerController& Player)
		{
			auto* Character = Cast<ACharacter>(Player.GetPawn());
			if (!Character) return false;
			FCollisionQueryParams Query(SCENE_QUERY_STAT(DaylightStreetProof), false, Character);
			const FVector Origin = Character->GetActorLocation();
			// Select actual blocking road support, rather than a rooftop under the preview spawn.
			for (int32 Ring = 0; Ring <= 10; ++Ring)
				for (int32 X = -Ring; X <= Ring; ++X)
					for (int32 Y = -Ring; Y <= Ring; ++Y)
					{
						if (FMath::Max(FMath::Abs(X), FMath::Abs(Y)) != Ring) continue;
						const FVector Start = Origin + FVector(X * 2000, Y * 2000, 100000);
						FHitResult Hit;
						if (!World.LineTraceSingleByChannel(Hit, Start, Start - FVector(0, 0, 200000), ECC_Pawn, Query) ||
							!Hit.GetActor() || !Hit.GetActor()->ActorHasTag(TEXT("ProjectWorld.Road.v1"))) continue;
						const FVector Location = Hit.ImpactPoint + FVector(0, 0, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5);
						if (!Character->TeleportTo(Location, FRotator(0, bManhattan ? 25 : 45, 0))) continue;
						Character->GetCharacterMovement()->StopMovementImmediately();
						Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
						Player.SetControlRotation(FRotator(-5, bManhattan ? 25 : 45, 0));
						Test.AddInfo(FString::Printf(TEXT("Street proof support=%s impact=%s pawn=%s"),
							*Hit.GetActor()->GetPathName(), *Hit.ImpactPoint.ToString(), *Character->GetActorLocation().ToString()));
						return true;
					}
			return false;
		}
		bool Finish()
		{
			if (Controls.IsValid()) Controls->Close();
			if (bSettingsSaved && Light.IsValid())
			{
				Light->Settings = OriginalSettings;
				Light->Bindings.Cloud->SetVisibility(bCloudVisible);
			}
			if (Time.IsValid()) Time->ResetToPcTime();
			FFileHelper::SaveStringToFile(Frames, *(Root / TEXT("frames.csv")));
			if (Samples)
			{
				GEngine->RemovePerformanceDataConsumer(Samples);
				FFileHelper::SaveStringToFile(Samples->Rows, *(Root / TEXT("native-timings.csv")));
				Samples.Reset();
			}
			if (GEditor && GEditor->PlayWorld) GEditor->EndPlayMap();
			return true;
		}
		void SeekPhase(int32 Index)
		{
			if (bNightCompare)
			{
				if (!bSettingsSaved)
				{
					OriginalSettings = Light->Settings;
					bCloudVisible = Light->Bindings.Cloud->IsVisible();
					bSettingsSaved = true;
				}
				auto* World = Time->GetWorld();
				AGeoReferencingSystem* Geo = nullptr;
				for (TActorIterator<AGeoReferencingSystem> It(World); It; ++It) Geo = *It;
				if (!Test.TestNotNull(TEXT("Existing geographic authority for night comparison"), Geo)) return;
				FGeographicCoordinates Coordinates;
				FVector East, North, Up;
				Geo->EngineToGeographic(FVector::ZeroVector, Coordinates);
				Geo->GetENUVectorsAtEngineLocation(FVector::ZeroVector, East, North, Up);
				const int32 Offset = bManhattan ? -4 : 3;
				FDateTime Utc = FDateTime(2026, 3, 21) - FTimespan::FromHours(Offset);
				if (Index == 4)
				{
					const FDateTime Noon = FDateTime(2026, 3, 21, 12) - FTimespan::FromHours(Offset);
					bool bFound = false;
					for (int32 Minute = 0; Minute < 720; ++Minute)
					{
						Utc = Noon + FTimespan::FromMinutes(Minute);
						FVector Sun; double Elevation;
						if (ProjectLight::SunDirection(Utc, Coordinates.Latitude, Coordinates.Longitude, East, North, Up, Sun, Elevation) &&
							Elevation <= -3 && Elevation > -4) { bFound = true; break; }
					}
					if (!Test.TestTrue(TEXT("Solar-relative evening twilight found"), bFound)) return;
				}
				Light->Settings = OriginalSettings;
				if (Index == 0 && NightFloor.IsSet()) Light->Settings.NightMinEv100 = NightFloor.GetValue();
				if (Index == 1) Light->Settings.NightFillLux = 0;
				if (Index == 2) Light->Settings.NightMinEv100 -= 2;
				Light->Bindings.Cloud->SetVisibility(Index == 3 ? false : bCloudVisible);
				Test.TestTrue(TEXT("Matched comparison seeks same world clock"), Time->SeekUtc(Utc));
				FString Error;
				Test.TestTrue(TEXT("Existing Light owner applies disposable comparison"), Light->Start(
					*Time, Coordinates.Latitude, Coordinates.Longitude, East, North, Up, Error));
				auto* Player = World->GetFirstPlayerController();
				Player->SetControlRotation(Index == 3 ? (-Light->Bindings.NightFill->GetForwardVector()).Rotation() :
					FRotator(-5, bManhattan ? 25 : 45, 0));
				Test.AddInfo(FString::Printf(TEXT("Night comparison phase=%d utc=%s fill_lux=%.3f min_ev=%.3f sun_priority=%d night_priority=%d"),
					Index, *Utc.ToIso8601(), Light->Settings.NightFillLux, Light->Settings.NightMinEv100,
					Light->Bindings.Sun->ForwardShadingPriority, Light->Bindings.NightFill->ForwardShadingPriority));
				PhaseStarted = FPlatformTime::Seconds();
				return;
			}
			const int32 LocalHours[] = {6, 12, 18, 0};
			const int32 UtcOffset = bManhattan ? -4 : 3;
			const auto Utc = FDateTime(2026, 3, 21) + FTimespan::FromHours(LocalHours[Index] - UtcOffset);
			Test.TestTrue(TEXT("Permitted native calendar seek"), Time->SeekUtc(Utc));
			Test.AddInfo(FString::Printf(TEXT("Daylight comparison UTC=%s"), *Utc.ToIso8601()));
			PhaseStarted = FPlatformTime::Seconds();
		}
	public:
		FDaylightPlayerProof(FAutomationTestBase& InTest, FString InRoot, bool bInManhattan, TOptional<float> InNightFloor)
			: Test(InTest), Root(MoveTemp(InRoot)), bManhattan(bInManhattan), NightFloor(InNightFloor) {}
		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			const double Timeout = bVisualOnly ? 120.0 + VisualSettleSeconds * CaptureCount() : 300.0;
			if (Now - Started > Timeout)
			{
				Test.AddError(TEXT("Daylight player proof timed out waiting for admitted Ready world or captures."));
				return Finish();
			}
			UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
			if (!World) return false;
			auto* Viewport = GEngine->GameViewport ? GEngine->GameViewport->GetGameViewport() : nullptr;
			if (Viewport && Viewport->GetSizeXY() != FIntPoint(2560, 1440)) Viewport->SetFixedViewportSize(2560, 1440);
			if (Phase < 0)
			{
				auto* Provider = World->GetSubsystem<UProjectTimeSubsystem>();
				auto* Player = World->GetFirstPlayerController();
				if (!Provider || !Provider->CanControlTime() || !Player || !Player->GetPawn() || !Viewport) return false;
				Time = Provider;
				if (bStreetView && !bStreetPlaced)
				{
					if (!PlaceStreetPlayer(*World, *Player)) return false;
					bStreetPlaced = true;
				}
				Controls = Player->GetLocalPlayer()->GetSubsystem<UProjectTimeDemoUISubsystem>();
				if (!Test.TestTrue(TEXT("Optional client controls exist"), Controls.IsValid())) return Finish();
				for (TActorIterator<AActor> It(World); It; ++It)
					if (auto* Component = It->FindComponentByClass<UProjectLightComponent>()) Light = Component;
				if (!Test.TestTrue(TEXT("World composed active Light"), Light.IsValid() && Light->GetApplyCount() > 0)) return Finish();
				FProjectTimeSnapshot Snapshot;
				Test.TestTrue(TEXT("Default Live UTC read"), Provider->TryReadTime(Snapshot));
				Test.TestFalse(TEXT("Session starts at PC time"), Snapshot.bDemo);
				Samples = MakeShared<FDaylightFrames>();
				GEngine->AddPerformanceDataConsumer(Samples);
				Phase = 0;
				SeekPhase(Phase);
				Controls->Toggle();
				TArray<UUserWidget*> Panels;
				UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Panels, UProjectTimeDemoWidget::StaticClass(), false);
				auto** Panel = Panels.FindByPredicate([](const UUserWidget* Widget) { return Widget->IsInViewport() && Widget->IsVisible(); });
				if (!Test.TestTrue(TEXT("Actual player controls opened"), Panel != nullptr)) return Finish();
				auto* NativePanel = Cast<UProjectTimeDemoWidget>(*Panel);
				auto* Offset = Cast<USlider>(NativePanel->GetWidgetFromName(TEXT("OffsetSlider")));
				auto* Rate = Cast<USlider>(NativePanel->GetWidgetFromName(TEXT("RateSlider")));
				if (!Test.TestTrue(TEXT("Live slider controls exist"), Offset && Rate)) return Finish();
				Offset->OnMouseCaptureBegin.Broadcast();
				Offset->OnValueChanged.Broadcast(2);
				Provider->TryReadTime(Snapshot);
				Test.TestTrue(TEXT("Player widget scrub pauses calendar"), Snapshot.bDemo && Snapshot.Rate == 0);
				Rate->OnValueChanged.Broadcast(60);
				Provider->TryReadTime(Snapshot);
				Test.TestEqual(TEXT("Player widget speed resumes calendar"), Snapshot.Rate, 60.0);
				auto* Reset = Cast<UButton>(NativePanel->GetWidgetFromName(TEXT("ResetButton")));
				if (!Test.TestNotNull(TEXT("Live Reset button exists"), Reset)) return Finish();
				Reset->OnClicked.Broadcast();
				Provider->TryReadTime(Snapshot);
				Test.TestFalse(TEXT("Player widget Reset restores PC time"), Snapshot.bDemo);
				SeekPhase(Phase);
				return false;
			}
			if (bCapturePending)
			{
				if (FScreenshotRequest::IsScreenshotRequested()) return false;
				if (!bUiCaptured)
				{
					bUiCaptured = true;
					if (!CaptureUi(*World)) return Finish();
					return false;
				}
				bCapturePending = false;
				Controls->Close();
				if (++Phase < CaptureCount()) { SeekPhase(Phase); Controls->Toggle(); }
				else
				{
					if (bVisualOnly)
					{
						ResetApplyCount = Light->GetApplyCount(); ResetFrame = GFrameCounter;
						Test.TestTrue(TEXT("Visual proof Reset restores Live"), Time->ResetToPcTime());
						Phase = 7;
						return false;
					}
					// Daylit frozen control keeps accelerated shadow work observable.
					SeekPhase(1);
				}
				return false;
			}
			Samples->Phase = Phase;
			Frames += FString::Printf(TEXT("%d,%llu,%.6f,%llu,%llu\n"), Phase, GFrameCounter,
				FApp::GetDeltaTime() * 1000.0, Light->GetApplyCount(), Light->GetLastApplyFrame());
			if (Phase == 7)
			{
				if (GFrameCounter - ResetFrame < 8) return false;
				Test.TestTrue(TEXT("Reset reaches the native light consumer"), Light->GetApplyCount() > ResetApplyCount);
				return Finish();
			}
			// Ready textures do not establish settled indirect lighting after a calendar jump.
			if (Now - PhaseStarted < (bVisualOnly ? VisualSettleSeconds : 15.0) || FScreenshotRequest::IsScreenshotRequested()) return false;
			if (Phase < CaptureCount())
			{
				if (!GShaderCompilingManager || GShaderCompilingManager->IsCompiling() ||
					FAssetCompilingManager::Get().GetNumRemainingAssets() > 0 ||
					IStreamingManager::Get().GetNumWantingResources() > 0) return false;
				Test.AddInfo(FString::Printf(TEXT("Ready daylight capture phase=%d viewport=%s"), Phase, *Viewport->GetSizeXY().ToString()));
				Test.TestFalse(TEXT("Demo panel does not pause gameplay"), World->IsPaused());
				TArray<UUserWidget*> Panels;
				UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Panels, UProjectTimeDemoWidget::StaticClass(), false);
				if (Phase == 0) Test.TestTrue(TEXT("Actual demo panel appears in player UI"), Panels.ContainsByPredicate(
					[](UUserWidget* Panel) { return Panel->IsInViewport() && Panel->IsVisible(); }));
				if (bStreetView)
				{
					auto* Character = Cast<ACharacter>(World->GetFirstPlayerController()->GetPawn());
					if (!Test.TestTrue(TEXT("Street capture retains grounded Hero"), Character && Character->GetCharacterMovement()->IsMovingOnGround())) return Finish();
					FVector View; FRotator Rotation;
					World->GetFirstPlayerController()->GetPlayerViewPoint(View, Rotation);
					Test.AddInfo(FString::Printf(TEXT("Player view phase=%d camera=%s rotation=%s civil=%s"),
						Phase, *View.ToString(), *Rotation.ToString(), *Time->GetLocalTimeText().ToString()));
				}
				FSceneViewFamilyContext Family(FSceneViewFamily::ConstructionValues(Viewport, World->Scene,
					GEngine->GameViewport->EngineShowFlags).SetRealtimeUpdate(true));
				FVector Location; FRotator Rotation;
				auto* View = World->GetFirstPlayerController()->GetLocalPlayer()->CalcSceneView(&Family, Location, Rotation, Viewport);
				if (!Test.TestNotNull(TEXT("Final player view settings resolve"), View)) return Finish();
				const auto& Exposure = View->FinalPostProcessSettings;
				Test.AddInfo(FString::Printf(TEXT("Final exposure phase=%d method=%d min=%.3f max=%.3f histogram_min=%.3f histogram_max=%.3f"),
					Phase, static_cast<int32>(Exposure.AutoExposureMethod), Exposure.AutoExposureMinBrightness,
					Exposure.AutoExposureMaxBrightness, Exposure.HistogramLogMin, Exposure.HistogramLogMax));
				FScreenshotRequest::RequestScreenshot(Root / FString::Printf(TEXT("phase-%d.png"), Phase), false, false);
				bCapturePending = true;
				return false;
			}
			if (Phase < 6)
			{
				++Phase;
				Test.TestTrue(TEXT("Daylit cadence comparison resumes"), Time->SetRate(Phase == 5 ? 1 : 60));
				PhaseStarted = Now;
				return false;
			}
			Test.TestTrue(TEXT("Accelerated clock drives additional light applications"), Light->GetApplyCount() > 20);
			ResetApplyCount = Light->GetApplyCount();
			ResetFrame = GFrameCounter;
			Test.TestTrue(TEXT("Reset uses existing provider"), Time->ResetToPcTime());
			FProjectTimeSnapshot Snapshot;
			Time->TryReadTime(Snapshot);
			Test.TestFalse(TEXT("Reset restores Live"), Snapshot.bDemo);
			Phase = 7;
			return false;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldDaylightPlayerTest,
	"Project.World.Realization.Presentation.DaylightPlayer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProjectWorldDaylightPlayerTest::RunTest(const FString&)
{
	FString Root, Experience;
	FString NightFloorText;
	TOptional<float> NightFloor;
	if (FParse::Value(FCommandLine::Get(), TEXT("ProjectTimeProofNightFloor="), NightFloorText))
	{
		float Value;
		if (!LexTryParseString(Value, *NightFloorText) || !FMath::IsFinite(Value) || Value < -5.5f || Value > -3.5f ||
			!FParse::Param(FCommandLine::Get(), TEXT("ProjectTimeProofUiOnly")) ||
			!FParse::Param(FCommandLine::Get(), TEXT("ProjectTimeProofNightCompare")))
		{
			AddError(TEXT("Night floor requires a finite [-5.5,-3.5] value and a single NightCompare/UI-only visual proof."));
			return false;
		}
		NightFloor = Value;
	}
	if ((FParse::Param(FCommandLine::Get(), TEXT("ProjectTimeProofNightCompare")) ||
		FParse::Param(FCommandLine::Get(), TEXT("ProjectTimeProofUiOnly"))) &&
		!FParse::Param(FCommandLine::Get(), TEXT("ProjectTimeProofVisualOnly")))
	{
		AddError(TEXT("Night comparison is a VisualOnly diagnostic, not a cadence/performance gate."));
		return false;
	}
	const FString ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	if (!FParse::Value(FCommandLine::Get(), TEXT("ProjectTimeProofRoot="), Root) ||
		!FParse::Value(FCommandLine::Get(), TEXT("ProjectMenuPlayAutoExperience="), Experience) ||
		(!FPaths::ConvertRelativePathToFull(Root).StartsWith(ProjectRoot / TEXT("tmp/world/")) &&
		 !FPaths::ConvertRelativePathToFull(Root).StartsWith(ProjectRoot / TEXT("tmp/release/"))) ||
		GEditor->PlayWorld || (Experience != TEXT("KazanTerritory") && Experience != TEXT("ManhattanShowcase")))
	{
		AddError(TEXT("Requires fresh Editor, explicit city auto-experience and tmp/world or tmp/release proof root."));
		return false;
	}
	IFileManager::Get().MakeDirectory(*Root, true);
	const FString Menu = GetDefault<UGameMapsSettings>()->GetGameDefaultMap();
	FAutomationEditorCommonUtils::LoadMap(Menu);
	auto* Settings = NewObject<ULevelEditorPlaySettings>();
	Settings->SetPlayNetMode(PIE_Standalone);
	Settings->SetPlayNumberOfClients(1);
	Settings->SetRunUnderOneProcess(true);
	Settings->bLaunchSeparateServer = false;
	Settings->NewWindowWidth = 2560;
	Settings->NewWindowHeight = 1440;
	FRequestPlaySessionParams Request;
	Request.EditorPlaySettings = Settings;
	Request.bAllowOnlineSubsystem = false;
	Request.GlobalMapOverride = Menu;
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FStartDaylightPIE>(Request));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FDaylightPlayerProof>(*this, Root, Experience == TEXT("ManhattanShowcase"), NightFloor));
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldDaylightPlayerTest, "Project.World.Realization.Presentation.DaylightPlayer", "[Slow][Integration][World]")
#endif
