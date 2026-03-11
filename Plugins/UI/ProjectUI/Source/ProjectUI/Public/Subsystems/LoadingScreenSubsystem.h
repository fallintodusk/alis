// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LoadingScreenSubsystem.generated.h"

struct FLoadRequest;
struct FLoadPhaseState;
struct FProjectLoadTelemetry;
class UW_LoadingScreen;

/**
 * Manages loading screen widget lifecycle during ProjectLoading pipeline.
 * Widget is added to viewport (not world) so it survives ServerTravel.
 */
UCLASS(Config=ProjectUI)
class PROJECTUI_API ULoadingScreenSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	ULoadingScreenSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	UFUNCTION()
	void OnLoadStarted(const FLoadRequest& Request);

	UFUNCTION()
	void OnProgress(float NormalizedProgress);

	UFUNCTION()
	void OnPhaseChanged(const FLoadPhaseState& State, float NormalizedProgress);

	UFUNCTION()
	void OnCompleted(const FLoadRequest& Request, const FProjectLoadTelemetry& Telemetry);

	UFUNCTION()
	void OnFailed(const FLoadRequest& Request, const FText& ErrorMessage, int32 ErrorCode);

private:
	/** Widget class to use for loading screen */
	UPROPERTY(Config)
	TSoftClassPtr<UW_LoadingScreen> LoadingWidgetClass;

	/** Current loading widget instance (viewport-bound, survives travel) */
	UPROPERTY()
	TObjectPtr<UW_LoadingScreen> LoadingWidget;

	/** True when current load has bShowLoadingScreen=false - filters log spam */
	bool bIgnoringLoad = false;

	/** Timer handle for fade-out delay */
	FTimerHandle FadeOutTimerHandle;
};
