// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPtr.h"
#include "ProjectLoadingMoviePlayerSettings.generated.h"

class UUserWidget;

/** 
 * Settings for MoviePlayer-backed loading screen.
 * Lives in Systems/ProjectLoading to keep loading UI ownership inside the loading system.
 */
UCLASS(Config=ProjectLoading, DefaultConfig, meta=(DisplayName="Project Loading MoviePlayer"))
class PROJECTLOADINGMOVIEPLAYER_API UProjectLoadingMoviePlayerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UProjectLoadingMoviePlayerSettings();

	virtual FName GetCategoryName() const override { return TEXT("Project"); }

	/** Widget used for the MoviePlayer loading screen. Must be a GameInstance-safe widget (no player assumptions). */
	UPROPERTY(Config, EditAnywhere, Category="Loading Screen")
	TSoftClassPtr<UUserWidget> LoadingScreenWidgetClass;

	/** Minimum time in seconds that the loading screen should stay visible. */
	UPROPERTY(Config, EditAnywhere, Category="Loading Screen", meta=(ClampMin="0.0"))
	float MinimumDisplayTime = 0.0f;

	/**
	 * When true, the loading screen will persist until StopLoadingScreen is called manually.
	 * IMPORTANT: Must be true for proper loading screen behavior during World Partition map loads.
	 * With false, MoviePlayer auto-stops too early (before streaming completes), causing dark screen.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Loading Screen")
	bool bWaitForManualStop = true;
};
