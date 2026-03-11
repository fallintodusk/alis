// Copyright ALIS. All Rights Reserved.

#include "Subsystems/ProjectLoadingMoviePlayerSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "MoviePlayer.h"
#include "ProjectLoadingLog.h"
#include "ProjectLoadingMoviePlayerSettings.h"
#include "ProjectLoadingSubsystem.h"
#include "Types/ProjectLoadRequest.h"
#include "Types/ProjectLoadPhaseState.h"
#include "UObject/UObjectGlobals.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

// Temporary debug flag - set to 1 to use brutal Slate test screen instead of UMG widget
// TESTED: MoviePlayer rendering works in Standalone (2025-12-10)
#define MOVIEPLAYER_DEBUG_SLATE_TEST 0

namespace
{
	// Helper to call optional widget functions without a hard dependency on ProjectUI types.
	template <typename TParam>
	void InvokeWidgetFunction(UUserWidget& Widget, const FName& FunctionName, const TParam& Param)
	{
		if (UFunction* Fn = Widget.FindFunction(FunctionName))
		{
			struct FDynamicParams
			{
				TParam Arg;
			};

			FDynamicParams Params{Param};
			Widget.ProcessEvent(Fn, &Params);
		}
	}

	void InvokeWidgetFunction(UUserWidget& Widget, const FName& FunctionName)
	{
		if (UFunction* Fn = Widget.FindFunction(FunctionName))
		{
			Widget.ProcessEvent(Fn, nullptr);
		}
	}
}

// DISABLED: MoviePlayer + Ray Tracing causes engine crash in UE 5.5
// FRayTracingGeometryManager::Tick() assertion: "should only be called once per frame"
// MoviePlayer causes extra render tick while RT is active.
// See: Plugins/Systems/ProjectLoading/todo/current.md
#define MOVIEPLAYER_SUBSYSTEM_ENABLED 0

void UProjectLoadingMoviePlayerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

#if !MOVIEPLAYER_SUBSYSTEM_ENABLED
	UE_LOG(LogProjectLoading, Log, TEXT("ProjectLoadingMoviePlayerSubsystem DISABLED due to MoviePlayer+RayTracing engine bug (see README.md)"));
#else
	// Ensure the loading subsystem is available before we bind delegates.
	Collection.InitializeDependency<UProjectLoadingSubsystem>();

	UE_LOG(LogProjectLoading, Display, TEXT("ProjectLoadingMoviePlayerSubsystem Initialize"));

	if (UProjectLoadingSubsystem* LoadingSub = GetGameInstance()->GetSubsystem<UProjectLoadingSubsystem>())
	{
		LoadingSub->OnLoadStarted.AddDynamic(this, &UProjectLoadingMoviePlayerSubsystem::HandleLoadStarted);
		LoadingSub->OnProgress.AddDynamic(this, &UProjectLoadingMoviePlayerSubsystem::HandleLoadProgress);
		LoadingSub->OnPhaseChanged.AddDynamic(this, &UProjectLoadingMoviePlayerSubsystem::HandlePhaseChanged);
		LoadingSub->OnCompleted.AddDynamic(this, &UProjectLoadingMoviePlayerSubsystem::HandleLoadCompleted);
		LoadingSub->OnFailed.AddDynamic(this, &UProjectLoadingMoviePlayerSubsystem::HandleLoadFailed);
		UE_LOG(LogProjectLoading, Display, TEXT("MoviePlayer subsystem subscribed to ProjectLoadingSubsystem events"));
	}
	else
	{
		UE_LOG(LogProjectLoading, Warning, TEXT("MoviePlayer subsystem could not find ProjectLoadingSubsystem; loading screens will be skipped"));
	}

	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UProjectLoadingMoviePlayerSubsystem::HandleMapLoaded);
#endif
}

void UProjectLoadingMoviePlayerSubsystem::Deinitialize()
{
	if (UProjectLoadingSubsystem* LoadingSub = GetGameInstance() ? GetGameInstance()->GetSubsystem<UProjectLoadingSubsystem>() : nullptr)
	{
		LoadingSub->OnLoadStarted.RemoveDynamic(this, &UProjectLoadingMoviePlayerSubsystem::HandleLoadStarted);
		LoadingSub->OnProgress.RemoveDynamic(this, &UProjectLoadingMoviePlayerSubsystem::HandleLoadProgress);
		LoadingSub->OnPhaseChanged.RemoveDynamic(this, &UProjectLoadingMoviePlayerSubsystem::HandlePhaseChanged);
		LoadingSub->OnCompleted.RemoveDynamic(this, &UProjectLoadingMoviePlayerSubsystem::HandleLoadCompleted);
		LoadingSub->OnFailed.RemoveDynamic(this, &UProjectLoadingMoviePlayerSubsystem::HandleLoadFailed);
	}

	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);

	StopLoadingScreen();

	Super::Deinitialize();

	UE_LOG(LogProjectLoading, Display, TEXT("ProjectLoadingMoviePlayerSubsystem Deinitialize"));
}

void UProjectLoadingMoviePlayerSubsystem::HandleLoadStarted(const FLoadRequest& Request)
{
	bHasActiveLoad = true;
	bPipelineCompleted = false;
	bMapLoaded = false;
	bShowLoadingScreenForCurrentLoad = Request.bShowLoadingScreen;

	UE_LOG(LogProjectLoading, Display, TEXT("MoviePlayer HandleLoadStarted: %s (bShowLoadingScreen=%s)"),
		*Request.ToString(), Request.bShowLoadingScreen ? TEXT("true") : TEXT("false"));

	// Skip showing loading screen if request doesn't want it
	if (!bShowLoadingScreenForCurrentLoad)
	{
		UE_LOG(LogProjectLoading, Display, TEXT("MoviePlayer: Skipping loading screen (bShowLoadingScreen=false)"));
		return;
	}

	// Clear any stale error text on the widget before showing.
	if (ActiveWidget.IsValid())
	{
		InvokeWidgetFunction(*ActiveWidget.Get(), TEXT("HideError"));
	}

	ShowLoadingScreen();
}

void UProjectLoadingMoviePlayerSubsystem::HandleLoadProgress(float NormalizedProgress)
{
	if (!bShowLoadingScreenForCurrentLoad) { return; }
	UpdateWidgetProgress(NormalizedProgress);
}

void UProjectLoadingMoviePlayerSubsystem::HandlePhaseChanged(const FLoadPhaseState& State, float NormalizedProgress)
{
	if (!bShowLoadingScreenForCurrentLoad) { return; }
	const FName PhaseName = StaticEnum<ELoadPhase>()->GetValueAsName(State.Phase);
	UpdateWidgetPhase(FText::FromName(PhaseName));
	UpdateWidgetStatus(State.StatusMessage);
	UpdateWidgetProgress(NormalizedProgress);
}

void UProjectLoadingMoviePlayerSubsystem::HandleLoadCompleted(const FLoadRequest& Request, const FProjectLoadTelemetry& Telemetry)
{
	bPipelineCompleted = true;
	UE_LOG(LogProjectLoading, Display, TEXT("MoviePlayer HandleLoadCompleted: TotalProgress=%.2f (bShowLoadingScreen=%s)"),
		Telemetry.TotalProgress, bShowLoadingScreenForCurrentLoad ? TEXT("true") : TEXT("false"));

	if (bShowLoadingScreenForCurrentLoad)
	{
		TryStopLoadingScreen();
	}
	bShowLoadingScreenForCurrentLoad = false;
}

void UProjectLoadingMoviePlayerSubsystem::HandleLoadFailed(const FLoadRequest& Request, const FText& ErrorMessage, int32 ErrorCode)
{
	UE_LOG(LogProjectLoading, Error, TEXT("MoviePlayer HandleLoadFailed: Code=%d Message=%s (bShowLoadingScreen=%s)"),
		ErrorCode, *ErrorMessage.ToString(), bShowLoadingScreenForCurrentLoad ? TEXT("true") : TEXT("false"));

	if (bShowLoadingScreenForCurrentLoad)
	{
		ShowWidgetError(ErrorMessage);
		StopLoadingScreen();
	}

	// On failure, reset gating.
	bHasActiveLoad = false;
	bPipelineCompleted = false;
	bMapLoaded = false;
	bShowLoadingScreenForCurrentLoad = false;
}

void UProjectLoadingMoviePlayerSubsystem::HandleMapLoaded(UWorld* LoadedWorld)
{
	if (LoadedWorld)
	{
		const bool bIsPlaying = GetMoviePlayer() && GetMoviePlayer()->IsMovieCurrentlyPlaying();
		UE_LOG(LogProjectLoading, Display, TEXT("MoviePlayer HandleMapLoaded: %s (IsPlaying=%s, bHasActiveLoad=%s)"),
			*LoadedWorld->GetName(),
			bIsPlaying ? TEXT("YES") : TEXT("NO"),
			bHasActiveLoad ? TEXT("Yes") : TEXT("No"));
		bMapLoaded = true;
		TryStopLoadingScreen();
	}
}

void UProjectLoadingMoviePlayerSubsystem::ShowLoadingScreen()
{
	if (bLoadingScreenActive)
	{
		UE_LOG(LogProjectLoading, Verbose, TEXT("ShowLoadingScreen skipped (already active)"));
		return;
	}

	if (IsRunningDedicatedServer())
	{
		return;
	}

	if (!FSlateApplication::IsInitialized() || !GetMoviePlayer())
	{
		UE_LOG(LogProjectLoading, Warning, TEXT("MoviePlayer unavailable; cannot show loading screen"));
		return;
	}

	const UProjectLoadingMoviePlayerSettings* Settings = GetDefault<UProjectLoadingMoviePlayerSettings>();
	if (!Settings)
	{
		return;
	}

	FLoadingScreenAttributes Attr;
	Attr.bAutoCompleteWhenLoadingCompletes = false;
	Attr.bWaitForManualStop = true;
	Attr.MinimumLoadingScreenDisplayTime = 0.0f;
	Attr.bAllowEngineTick = true;

#if MOVIEPLAYER_DEBUG_SLATE_TEST
	// Brutal test - no UMG, just big text to verify MoviePlayer rendering works
	UE_LOG(LogProjectLoading, Warning, TEXT("ShowLoadingScreen: USING DEBUG SLATE TEST (MOVIEPLAYER_DEBUG_SLATE_TEST=1)"));
	Attr.WidgetLoadingScreen =
		SNew(SBorder)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.BorderBackgroundColor(FLinearColor::Black)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("MOVIEPLAYER TEST")))
			.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 48))
			.ColorAndOpacity(FLinearColor::White)
		];
#else
	UE_LOG(LogProjectLoading, Display, TEXT("ShowLoadingScreen: WidgetClass=%s, MinDisplay=%.2fs, WaitForManualStop=%s"),
		*Settings->LoadingScreenWidgetClass.ToString(),
		Settings->MinimumDisplayTime,
		Settings->bWaitForManualStop ? TEXT("true") : TEXT("false"));

	UUserWidget* Widget = ActiveWidget.Get();
	if (!Widget)
	{
		Widget = CreateLoadingWidget();
	}

	if (!Widget)
	{
		UE_LOG(LogProjectLoading, Warning, TEXT("No loading widget configured; skipping MoviePlayer screen"));
		return;
	}

	UE_LOG(LogProjectLoading, Display, TEXT("Created loading widget: %s (Valid: %s)"),
		*Widget->GetClass()->GetName(), Widget->IsValidLowLevel() ? TEXT("Yes") : TEXT("No"));

#if WITH_EDITOR
	if (GIsEditor)
	{
		UE_LOG(LogProjectLoading, Warning, TEXT("MoviePlayer loading screen not visible in PIE - use Standalone Game mode for testing"));
	}
#endif

	Attr.bAutoCompleteWhenLoadingCompletes = !Settings->bWaitForManualStop;
	Attr.bWaitForManualStop = Settings->bWaitForManualStop;
	Attr.MinimumLoadingScreenDisplayTime = Settings->MinimumDisplayTime;

	TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();
	UE_LOG(LogProjectLoading, Display, TEXT("TakeWidget returned Slate type: %s (Valid: %s)"),
		*SlateWidget->GetType().ToString(),
		SlateWidget->GetType() != NAME_None ? TEXT("Yes") : TEXT("Unknown"));
	Attr.WidgetLoadingScreen = SlateWidget;
#endif

	GetMoviePlayer()->SetupLoadingScreen(Attr);
	GetMoviePlayer()->PlayMovie();

	bLoadingScreenActive = true;

	// Diagnostic: verify MoviePlayer is actually rendering
	const bool bIsPlaying = GetMoviePlayer()->IsMovieCurrentlyPlaying();
	UE_LOG(LogProjectLoading, Display, TEXT("MoviePlayer loading screen started (IsPlaying=%s, WaitForManualStop=%s, AutoComplete=%s)"),
		bIsPlaying ? TEXT("YES") : TEXT("NO"),
		Attr.bWaitForManualStop ? TEXT("true") : TEXT("false"),
		Attr.bAutoCompleteWhenLoadingCompletes ? TEXT("true") : TEXT("false"));
}

void UProjectLoadingMoviePlayerSubsystem::StopLoadingScreen()
{
	if (IsRunningDedicatedServer())
	{
		return;
	}

	const bool bWasPlaying = GetMoviePlayer() && GetMoviePlayer()->IsMovieCurrentlyPlaying();

	if (bWasPlaying)
	{
		GetMoviePlayer()->StopMovie();
	}

	UE_LOG(LogProjectLoading, Display, TEXT("MoviePlayer StopLoadingScreen: WasPlaying=%s, Now stopped"),
		bWasPlaying ? TEXT("YES") : TEXT("NO"));

	ActiveWidget.Reset();
	bLoadingScreenActive = false;
}

void UProjectLoadingMoviePlayerSubsystem::TryStopLoadingScreen()
{
	if (!bHasActiveLoad)
	{
		return;
	}

	// Diagnostic: show MoviePlayer rendering state
	const bool bIsPlaying = GetMoviePlayer() && GetMoviePlayer()->IsMovieCurrentlyPlaying();
	UE_LOG(LogProjectLoading, Display, TEXT("MoviePlayer TryStopLoadingScreen: Pipeline=%s, MapLoaded=%s, IsPlaying=%s, WillStop=%s"),
		bPipelineCompleted ? TEXT("Yes") : TEXT("No"),
		bMapLoaded ? TEXT("Yes") : TEXT("No"),
		bIsPlaying ? TEXT("YES") : TEXT("NO"),
		(bPipelineCompleted && bMapLoaded) ? TEXT("YES") : TEXT("NO"));

	if (bPipelineCompleted && bMapLoaded)
	{
		StopLoadingScreen();
		bHasActiveLoad = false;
		bPipelineCompleted = false;
		bMapLoaded = false;
	}
}

void UProjectLoadingMoviePlayerSubsystem::UpdateWidgetProgress(float NormalizedProgress)
{
	if (ActiveWidget.IsValid())
	{
		UE_LOG(LogProjectLoading, Verbose, TEXT("UpdateWidgetProgress: %.1f%%"), NormalizedProgress * 100.0f);
		InvokeWidgetFunction(*ActiveWidget.Get(), TEXT("SetProgress"), NormalizedProgress);
	}
	else
	{
		UE_LOG(LogProjectLoading, Warning, TEXT("UpdateWidgetProgress: ActiveWidget is invalid!"));
	}
}

void UProjectLoadingMoviePlayerSubsystem::UpdateWidgetPhase(const FText& InPhaseText)
{
	if (ActiveWidget.IsValid())
	{
		InvokeWidgetFunction(*ActiveWidget.Get(), TEXT("SetPhaseText"), InPhaseText);
	}
}

void UProjectLoadingMoviePlayerSubsystem::UpdateWidgetStatus(const FText& InStatusMessage)
{
	if (ActiveWidget.IsValid())
	{
		InvokeWidgetFunction(*ActiveWidget.Get(), TEXT("SetStatusMessage"), InStatusMessage);
	}
}

void UProjectLoadingMoviePlayerSubsystem::ShowWidgetError(const FText& InErrorMessage)
{
	if (ActiveWidget.IsValid())
	{
		InvokeWidgetFunction(*ActiveWidget.Get(), TEXT("ShowError"), InErrorMessage);
	}
}

UUserWidget* UProjectLoadingMoviePlayerSubsystem::CreateLoadingWidget()
{
	UE_LOG(LogProjectLoading, Display, TEXT("CreateLoadingWidget: Starting widget creation"));

	const UProjectLoadingMoviePlayerSettings* Settings = GetDefault<UProjectLoadingMoviePlayerSettings>();
	if (!Settings || Settings->LoadingScreenWidgetClass.IsNull())
	{
		UE_LOG(LogProjectLoading, Warning, TEXT("CreateLoadingWidget: LoadingScreenWidgetClass is not set in settings"));
		return nullptr;
	}

	UE_LOG(LogProjectLoading, Display, TEXT("CreateLoadingWidget: Loading class from path: %s"),
		*Settings->LoadingScreenWidgetClass.ToString());

	TSubclassOf<UUserWidget> WidgetClass = Settings->LoadingScreenWidgetClass.LoadSynchronous();
	if (!WidgetClass)
	{
		UE_LOG(LogProjectLoading, Error, TEXT("CreateLoadingWidget: Failed to load LoadingScreenWidgetClass from path: %s"),
			*Settings->LoadingScreenWidgetClass.ToString());
		return nullptr;
	}

	UE_LOG(LogProjectLoading, Display, TEXT("CreateLoadingWidget: Loaded class: %s"), *WidgetClass->GetName());

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		UE_LOG(LogProjectLoading, Error, TEXT("CreateLoadingWidget: GameInstance is null"));
		return nullptr;
	}

	UUserWidget* Widget = CreateWidget<UUserWidget>(GameInstance, WidgetClass);
	if (!Widget)
	{
		UE_LOG(LogProjectLoading, Error, TEXT("CreateLoadingWidget: CreateWidget failed for class: %s"), *WidgetClass->GetName());
		return nullptr;
	}

	UE_LOG(LogProjectLoading, Display, TEXT("CreateLoadingWidget: Successfully created widget instance: %s"), *Widget->GetName());

	ActiveWidget.Reset(Widget);
	return Widget;
}
