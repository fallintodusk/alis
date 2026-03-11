#pragma once

#include "CoreMinimal.h"

class UWorld;
class AGameModeBase;
class APawn;

/**
 * Context passed to feature initialization functions.
 * Contains everything a feature needs to set itself up.
 */
struct PROJECTFEATURE_API FFeatureInitContext
{
	/** World in which the feature is being initialized */
	UWorld* World = nullptr;

	/** GameMode that requested the feature initialization */
	AGameModeBase* GameMode = nullptr;

	/** Pawn to attach components to (may be null for non-pawn features) */
	APawn* Pawn = nullptr;

	/** Name of the gameplay mode (e.g., "Medium", "Hardcore") */
	FName ModeName;

	/** JSON configuration string for this feature (from ModeConfig.FeatureConfigs) */
	FString ConfigJson;

	FFeatureInitContext() = default;

	FFeatureInitContext(UWorld* InWorld, AGameModeBase* InGameMode, APawn* InPawn, FName InModeName, const FString& InConfigJson)
		: World(InWorld)
		, GameMode(InGameMode)
		, Pawn(InPawn)
		, ModeName(InModeName)
		, ConfigJson(InConfigJson)
	{
	}
};
