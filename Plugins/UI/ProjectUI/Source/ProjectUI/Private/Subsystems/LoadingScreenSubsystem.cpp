// Copyright ALIS. All Rights Reserved.

#include "Subsystems/LoadingScreenSubsystem.h"
#include "ProjectLoadingSubsystem.h"
#include "Types/ProjectLoadPhaseState.h"
#include "Types/ProjectLoadRequest.h"
#include "Widgets/W_LoadingScreen.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "Framework/Application/SlateApplication.h" // For frame pumping during blocking waits

DEFINE_LOG_CATEGORY_STATIC(LogLoadingScreenSubsystem, Log, All);

ULoadingScreenSubsystem::ULoadingScreenSubsystem()
{
	// Default to W_LoadingScreen class (can be overridden in config)
	LoadingWidgetClass = TSoftClassPtr<UW_LoadingScreen>(FSoftObjectPath(TEXT("/Script/ProjectUI.W_LoadingScreen")));
}

void ULoadingScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogLoadingScreenSubsystem, Display, TEXT("LoadingScreenSubsystem Initialize"));

	// Ensure loading subsystem is created first
	Collection.InitializeDependency<UProjectLoadingSubsystem>();

	if (UProjectLoadingSubsystem* LoadingSub = GetGameInstance()->GetSubsystem<UProjectLoadingSubsystem>())
	{
		LoadingSub->OnLoadStarted.AddDynamic(this, &ULoadingScreenSubsystem::OnLoadStarted);
		LoadingSub->OnProgress.AddDynamic(this, &ULoadingScreenSubsystem::OnProgress);
		LoadingSub->OnPhaseChanged.AddDynamic(this, &ULoadingScreenSubsystem::OnPhaseChanged);
		LoadingSub->OnCompleted.AddDynamic(this, &ULoadingScreenSubsystem::OnCompleted);
		LoadingSub->OnFailed.AddDynamic(this, &ULoadingScreenSubsystem::OnFailed);

		// Provide frame pump callback so loading pipeline can update UI during blocking waits
		// This keeps ProjectLoading decoupled from Slate - UI layer owns the pumping logic
		LoadingSub->SetPumpFrameCallback([]()
		{
			if (FSlateApplication::IsInitialized())
			{
				FSlateApplication::Get().PumpMessages();
				FSlateApplication::Get().Tick();
			}
		});

		UE_LOG(LogLoadingScreenSubsystem, Log, TEXT("Subscribed to ProjectLoadingSubsystem events (with frame pump callback)"));
	}

}

void ULoadingScreenSubsystem::Deinitialize()
{
	UE_LOG(LogLoadingScreenSubsystem, Display, TEXT("LoadingScreenSubsystem Deinitialize"));

	// Unsubscribe from events (important for PIE)
	if (UProjectLoadingSubsystem* LoadingSub = GetGameInstance()->GetSubsystem<UProjectLoadingSubsystem>())
	{
		LoadingSub->OnLoadStarted.RemoveDynamic(this, &ULoadingScreenSubsystem::OnLoadStarted);
		LoadingSub->OnProgress.RemoveDynamic(this, &ULoadingScreenSubsystem::OnProgress);
		LoadingSub->OnPhaseChanged.RemoveDynamic(this, &ULoadingScreenSubsystem::OnPhaseChanged);
		LoadingSub->OnCompleted.RemoveDynamic(this, &ULoadingScreenSubsystem::OnCompleted);
		LoadingSub->OnFailed.RemoveDynamic(this, &ULoadingScreenSubsystem::OnFailed);
	}

	Super::Deinitialize();
}

void ULoadingScreenSubsystem::OnLoadStarted(const FLoadRequest& Request)
{
	bIgnoringLoad = !Request.bShowLoadingScreen;
	if (bIgnoringLoad)
	{
		UE_LOG(LogLoadingScreenSubsystem, Verbose, TEXT("OnLoadStarted: bShowLoadingScreen=false, ignoring progress for '%s'"), *Request.ToString());
		return;
	}

	UE_LOG(LogLoadingScreenSubsystem, Display, TEXT("OnLoadStarted: %s"), *Request.ToString());

	// Create loading widget if not already exists
	if (!LoadingWidget)
	{
		if (UClass* WidgetClass = LoadingWidgetClass.LoadSynchronous())
		{
			LoadingWidget = CreateWidget<UW_LoadingScreen>(GetGameInstance(), WidgetClass);
			if (LoadingWidget)
			{
				UE_LOG(LogLoadingScreenSubsystem, Log, TEXT("Created loading widget: %s"), *LoadingWidget->GetName());
			}
			else
			{
				UE_LOG(LogLoadingScreenSubsystem, Error, TEXT("Failed to create loading widget from class: %s"), *WidgetClass->GetName());
				return;
			}
		}
		else
		{
			UE_LOG(LogLoadingScreenSubsystem, Error, TEXT("Failed to load widget class: %s"), *LoadingWidgetClass.ToString());
			return;
		}
	}

	// Add widget to viewport at high Z-order (AddToViewport handles fullscreen sizing)
	if (UGameViewportClient* Viewport = GetGameInstance()->GetGameViewportClient())
	{
		// Set visibility to Visible (not SelfHitTestInvisible) to ensure rendering
		LoadingWidget->SetVisibility(ESlateVisibility::Visible);

		// AddToViewport with high Z-order (1000) - automatically fills screen
		LoadingWidget->AddToViewport(1000);

		// Log detailed viewport and widget state
		FVector2D ViewportSize;
		Viewport->GetViewportSize(ViewportSize);

		UE_LOG(LogLoadingScreenSubsystem, Log, TEXT("Added loading widget to viewport (AddToViewport):"));
		UE_LOG(LogLoadingScreenSubsystem, Log, TEXT("  Z-order: 1000 (should be on top)"));
		UE_LOG(LogLoadingScreenSubsystem, Log, TEXT("  Viewport size: %.1f x %.1f"), ViewportSize.X, ViewportSize.Y);
		UE_LOG(LogLoadingScreenSubsystem, Log, TEXT("  Widget visibility: %s"),
			*UEnum::GetValueAsString(LoadingWidget->GetVisibility()));
		UE_LOG(LogLoadingScreenSubsystem, Log, TEXT("  Widget IsVisible: %s, IsInViewport: %s"),
			LoadingWidget->IsVisible() ? TEXT("true") : TEXT("false"),
			LoadingWidget->IsInViewport() ? TEXT("true") : TEXT("false"));

		// Initialize progress
		LoadingWidget->SetProgress(0.0f);
		LoadingWidget->SetPhaseText(FText::FromString(TEXT("Initializing...")));
		LoadingWidget->HideError();
	}
	else
	{
		UE_LOG(LogLoadingScreenSubsystem, Error, TEXT("Failed to get GameViewportClient - cannot show loading screen"));
	}
}

void ULoadingScreenSubsystem::OnProgress(float NormalizedProgress)
{
	if (bIgnoringLoad) { return; }

	UE_LOG(LogLoadingScreenSubsystem, Verbose, TEXT("OnProgress: %.2f%%"), NormalizedProgress * 100.0f);

	// Update widget progress
	if (LoadingWidget)
	{
		LoadingWidget->SetProgress(NormalizedProgress);
	}
}

void ULoadingScreenSubsystem::OnPhaseChanged(const FLoadPhaseState& State, float NormalizedProgress)
{
	if (bIgnoringLoad) { return; }

	FString PhaseName = UEnum::GetValueAsString(State.Phase);
	PhaseName.RemoveFromStart(TEXT("ELoadPhase::"));
	FString StateName = UEnum::GetValueAsString(State.State);
	StateName.RemoveFromStart(TEXT("EPhaseState::"));

	UE_LOG(LogLoadingScreenSubsystem, Display, TEXT("OnPhaseChanged: %s (%s) - Progress: %.2f%%"),
		*PhaseName, *StateName, NormalizedProgress * 100.0f);

	// Update widget phase text
	if (LoadingWidget)
	{
		FText PhaseText = FText::Format(
			FText::FromString(TEXT("{0}...")),
			FText::FromString(PhaseName)
		);
		LoadingWidget->SetPhaseText(PhaseText);
		LoadingWidget->SetProgress(NormalizedProgress);
	}
}

void ULoadingScreenSubsystem::OnCompleted(const FLoadRequest& Request, const FProjectLoadTelemetry& Telemetry)
{
	if (bIgnoringLoad)
	{
		UE_LOG(LogLoadingScreenSubsystem, Verbose, TEXT("Pipeline completed (silent load)"));
		bIgnoringLoad = false;
		return;
	}

	UE_LOG(LogLoadingScreenSubsystem, Log, TEXT("Pipeline completed"));

	// Remove widget from viewport after short delay (for fade-out animation)
	if (LoadingWidget && GetGameInstance())
	{
		if (UWorld* World = GetGameInstance()->GetWorld())
		{
			World->GetTimerManager().SetTimer(
				FadeOutTimerHandle,
				[this]()
				{
					if (LoadingWidget)
					{
						LoadingWidget->RemoveFromParent();
						UE_LOG(LogLoadingScreenSubsystem, Log, TEXT("Removed loading widget from viewport"));
						LoadingWidget = nullptr;
					}
				},
				0.5f,  // 500ms delay for fade-out
				false
			);
		}
	}

	bIgnoringLoad = false;
}

void ULoadingScreenSubsystem::OnFailed(const FLoadRequest& Request, const FText& ErrorMessage, int32 ErrorCode)
{
	// Always log errors, even for silent loads
	UE_LOG(LogLoadingScreenSubsystem, Error, TEXT("Loading failed (code %d): %s"), ErrorCode, *ErrorMessage.ToString());

	// Show error in widget if visible
	if (LoadingWidget && !bIgnoringLoad)
	{
		LoadingWidget->ShowError(ErrorMessage);
	}

	bIgnoringLoad = false;
}
