#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "SinglePlayModeConfig.generated.h"

/**
 * Data-driven configuration for a single-player mode.
 * Defines UE layer classes (Pawn, PlayerController) and features to initialize.
 *
 * Used by ASinglePlayerGameMode to configure gameplay based on URL parameters.
 * Features are initialized via FFeatureRegistry (ProjectFeature) - features self-register
 * on module startup and attach their own components.
 */
USTRUCT(BlueprintType)
struct PROJECTSINGLEPLAY_API FSinglePlayModeConfig
{
	GENERATED_BODY()

	// Mode identifier (e.g., "Single", "Story", "Hardcore")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mode")
	FName ModeName;

	// UE Layer: Default pawn class for this mode (soft ref to avoid hard dependencies)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UE Layer")
	TSoftClassPtr<APawn> DefaultPawnClass;

	// UE Layer: Player controller class for this mode
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UE Layer")
	TSoftClassPtr<APlayerController> PlayerControllerClass;

	// Required feature plugins that must be loaded before gameplay starts
	// These are plugin names (e.g., "ProjectCombat", "ProjectInventory", "ProjectDialogue")
	// Orchestrator ensures these are loaded in InitGame before pawn spawn
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Features")
	TArray<FName> RequiredFeaturePlugins;

	// Game Layer: Ordered list of features to initialize via FFeatureRegistry
	// Features self-register on module startup, GameMode calls InitializeFeature for each
	// Order matters: features initialize in array order (e.g., Combat before Inventory)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Features")
	TArray<FName> FeatureNames;

	// Per-feature configuration data (key = feature name, value = JSON config string)
	// Passed to features via FFeatureInitContext.ConfigJson during initialization
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Features")
	TMap<FName, FString> FeatureConfigs;

	// Returns true if this config has valid essential fields set
	bool IsValid() const
	{
		return !ModeName.IsNone();
	}

	// Returns a default config suitable for fallback
	// Note: ModeName is "Single" to avoid overwriting other registered modes
	static FSinglePlayModeConfig GetDefault()
	{
		FSinglePlayModeConfig Config;
		Config.ModeName = FName(TEXT("Single"));
		// Leave class references null - GameMode will use its own defaults
		return Config;
	}
};
