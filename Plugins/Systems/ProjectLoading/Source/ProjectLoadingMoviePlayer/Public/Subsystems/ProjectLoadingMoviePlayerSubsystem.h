// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "ProjectLoadingMoviePlayerSubsystem.generated.h"

class UProjectLoadingSubsystem;
struct FLoadRequest;
struct FLoadPhaseState;
struct FProjectLoadTelemetry;

/**
 * GameInstance subsystem that bridges ProjectLoading events to the MoviePlayer.
 * Responsible for creating the loading widget and letting MoviePlayer own rendering during travel.
 */
UCLASS()
class PROJECTLOADINGMOVIEPLAYER_API UProjectLoadingMoviePlayerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	// Delegate callbacks from ProjectLoadingSubsystem
	UFUNCTION()
	void HandleLoadStarted(const FLoadRequest& Request);

	UFUNCTION()
	void HandleLoadProgress(float NormalizedProgress);

	UFUNCTION()
	void HandlePhaseChanged(const FLoadPhaseState& State, float NormalizedProgress);

	UFUNCTION()
	void HandleLoadCompleted(const FLoadRequest& Request, const FProjectLoadTelemetry& Telemetry);

	UFUNCTION()
	void HandleLoadFailed(const FLoadRequest& Request, const FText& ErrorMessage, int32 ErrorCode);

	void HandleMapLoaded(UWorld* LoadedWorld);

private:
	void ShowLoadingScreen();
	void StopLoadingScreen();
	void TryStopLoadingScreen();
	void UpdateWidgetProgress(float NormalizedProgress);
	void UpdateWidgetPhase(const FText& InPhaseText);
	void UpdateWidgetStatus(const FText& InStatusMessage);
	void ShowWidgetError(const FText& InErrorMessage);

	UUserWidget* CreateLoadingWidget();

private:
	// Holds the active loading widget alive while MoviePlayer is rendering it
	TStrongObjectPtr<UUserWidget> ActiveWidget;
	bool bLoadingScreenActive = false;
	bool bHasActiveLoad = false;
	bool bPipelineCompleted = false;
	bool bMapLoaded = false;

	// True if current load request has bShowLoadingScreen=true
	bool bShowLoadingScreenForCurrentLoad = false;

	// Delegate handle for map load callback
	FDelegateHandle PostLoadMapHandle;
};
