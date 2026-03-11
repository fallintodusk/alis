// Copyright ALIS.

#include "MenuPlayPlayerController.h"

#include "Engine/LocalPlayer.h"
#include "Subsystems/MenuMainComposerSubsystem.h"
#include "ProjectServiceLocator.h"
#include "Services/ILoadingService.h"
#include "Types/ProjectLoadRequest.h"
#include "Interfaces/ProjectLoadingHandle.h"

DEFINE_LOG_CATEGORY_STATIC(LogMenuPlayPC, Log, All);

AMenuPlayPlayerController::AMenuPlayPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

void AMenuPlayPlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);

	if (UMenuMainComposerSubsystem* Composer = GetGameInstance()->GetSubsystem<UMenuMainComposerSubsystem>())
	{
		// Subscribe to game start requests before showing menu
		Composer->OnRequestStartGame.AddDynamic(this, &AMenuPlayPlayerController::OnRequestStartGame);
		Composer->ShowMainMenu(this);
	}
}

void AMenuPlayPlayerController::OnRequestStartGame(const FString& MapId, const FString& GameMode)
{
	UE_LOG(LogMenuPlayPC, Display, TEXT("OnRequestStartGame: MapId=%s, GameMode=%s"), *MapId, *GameMode);
	StartGameLoad(MapId, GameMode);
}

void AMenuPlayPlayerController::StartGameLoad(const FString& MapId, const FString& GameMode)
{
	UE_LOG(LogMenuPlayPC, Display, TEXT("StartGameLoad: MapId=%s, GameMode=%s"), *MapId, *GameMode);

	TSharedPtr<ILoadingService> LoadingService = FProjectServiceLocator::Resolve<ILoadingService>();
	if (!LoadingService.IsValid())
	{
		UE_LOG(LogMenuPlayPC, Error, TEXT("  ILoadingService not available"));
		return;
	}

	if (LoadingService->IsLoadInProgress())
	{
		UE_LOG(LogMenuPlayPC, Warning, TEXT("  Load already in progress, cancelling previous"));
		LoadingService->CancelActiveLoad(false);
	}

	// Build request from experience descriptor (ensures asset scans + MapAssetId resolution)
	FLoadRequest Request;
	FText BuildError;
	if (!LoadingService->BuildLoadRequestForExperience(FName(*MapId), Request, BuildError))
	{
		UE_LOG(LogMenuPlayPC, Error, TEXT("  Failed to build request for experience '%s': %s"),
			*MapId, *BuildError.ToString());
		return;
	}

	// Apply GameMode configuration
	if (GameMode == TEXT("SinglePlayer"))
	{
		Request.LoadMode = ELoadMode::SinglePlayer;
		// Full class path for modular plugin game modes
		// UClass name does NOT include 'A' prefix (that's C++ only, not in UE reflection)
		Request.CustomOptions.Add(TEXT("game"), TEXT("/Script/ProjectSinglePlay.SinglePlayerGameMode"));
		Request.CustomOptions.Add(TEXT("Mode"), TEXT("Medium"));
	}
	else
	{
		UE_LOG(LogMenuPlayPC, Error, TEXT("  Unknown GameMode: %s"), *GameMode);
		return;
	}

	TArray<FText> Errors;
	if (!Request.Validate(Errors))
	{
		UE_LOG(LogMenuPlayPC, Error, TEXT("  FLoadRequest validation failed:"));
		for (const FText& Err : Errors)
		{
			UE_LOG(LogMenuPlayPC, Error, TEXT("    - %s"), *Err.ToString());
		}
		return;
	}

	TSharedPtr<ILoadingHandle> Handle = LoadingService->StartLoad(Request);
	if (!Handle.IsValid())
	{
		UE_LOG(LogMenuPlayPC, Error, TEXT("  StartLoad returned null handle"));
		return;
	}

	UE_LOG(LogMenuPlayPC, Display, TEXT("  Game load started with MapAssetId=%s"), *Request.MapAssetId.ToString());
}
