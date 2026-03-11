// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ProjectLoadRequest.generated.h"

/**
 * Loading mode enum
 */
UENUM(BlueprintType)
enum class ELoadMode : uint8
{
	/** Single player / offline mode */
	SinglePlayer UMETA(DisplayName = "Single Player"),

	/** Multiplayer client joining server */
	MultiplayerClient UMETA(DisplayName = "Multiplayer Client"),

	/** Multiplayer host (listen server) */
	MultiplayerHost UMETA(DisplayName = "Multiplayer Host"),

	/** Dedicated server */
	DedicatedServer UMETA(DisplayName = "Dedicated Server")
};

/**
 * FLoadRequest
 *
 * Data structure describing a complete load operation.
 * Built by UI/menu systems and passed to UProjectLoadingSubsystem::StartLoad().
 *
 * Contains:
 * - Target map/experience to load
 * - Desired mode (SP/MP)
 * - Session data for multiplayer
 * - Feature toggles
 * - Options (skip cinematics, etc.)
 */
USTRUCT(BlueprintType)
struct PROJECTCORE_API FLoadRequest
{
	GENERATED_BODY()

public:
	/**
	 * Primary Asset ID of the world manifest to load (e.g. ProjectWorld:AlisSandbox).
	 * World manifests now live in ProjectWorld plugin and describe World Partition roots plus regions.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading")
	FPrimaryAssetId MapAssetId;

	/**
	 * Fallback: Direct soft object path to the map (e.g. /MainMenuWorld/Maps/MainMenu_Persistent).
	 * Used when Asset Manager hasn't scanned plugin content yet (modular plugin approach).
	 * TravelPhaseExecutor will use this if MapAssetId is invalid.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading")
	FSoftObjectPath MapSoftPath;

	/**
	 * Primary Asset ID of the game mode configuration.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading")
	FPrimaryAssetId ExperienceAssetId;

	/**
	 * Logical experience/descriptor name (used for lookup and telemetry).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading")
	FName ExperienceName;

	/**
	 * Critical assets that must be available before travel (UI/data/config).
	 * These are registered primary assets (resolved via AssetManager).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading")
	TArray<FPrimaryAssetId> CriticalAssetIds;

	/**
	 * Critical assets as soft paths (for assets not registered as primary assets).
	 * Auto-populated by DiscoverMapDependencies() for meshes, textures, materials.
	 * Loaded directly via StreamableManager in Phase 3.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading")
	TArray<FSoftObjectPath> CriticalSoftPaths;

	/**
	 * Assets to warm up during loading (streamed before gameplay starts).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading")
	TArray<FPrimaryAssetId> WarmupAssetIds;

	/**
	 * Assets that can stream in the background after arrival.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading")
	TArray<FPrimaryAssetId> BackgroundAssetIds;

	/**
	 * Loading mode (single player, multiplayer, etc.)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading")
	ELoadMode LoadMode = ELoadMode::SinglePlayer;

	/**
	 * Session join data for multiplayer (optional)
	 * Contains server address, session ID, etc.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Multiplayer")
	FString SessionData;

	/**
	 * Party/lobby data for multiplayer (optional)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Multiplayer")
	TArray<FString> PartyMemberIds;

	/**
	 * Feature plugin identifiers to activate
	 * Example: ["CombatSystem", "DialogueSystem"]
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Features")
	TArray<FString> FeaturesToActivate;

	/**
	 * Content pack identifiers to mount prior to loading
	 * NOTE: Content mounting now handled by Orchestrator before loading pipeline starts.
	 * This field is deprecated and will be removed in a future version.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Content")
	TArray<FString> ContentPacksToMount;

	/**
	 * Skip intro cinematics / cutscenes
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	bool bSkipCinematics = false;

	/**
	 * Perform warmup phase (preload shaders, etc.)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	bool bPerformWarmup = true;

	/**
	 * Custom load options (key-value pairs)
	 * For game-specific configuration
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	TMap<FString, FString> CustomOptions;

	/**
	 * Priority for this load request (higher = more important)
	 * Used for telemetry and optimization
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	int32 Priority = 0;

	/**
	 * Whether to show loading screen UI for this load.
	 * Set to false for background/startup loads that should not display progress.
	 * Defaults to true for user-initiated loads (gameplay).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	bool bShowLoadingScreen = true;

public:
	/** Default constructor */
	FLoadRequest() = default;

	/**
	 * Validate the load request (simple check)
	 * @return True if request has minimum required data (valid map via AssetId OR SoftPath)
	 */
	bool IsValid() const
	{
		return MapAssetId.IsValid() || !MapSoftPath.IsNull();
	}

	/**
	 * Comprehensive validation with detailed error reporting
	 * @param OutErrors Array to receive detailed error messages
	 * @return True if request is fully valid, false otherwise
	 */
	bool Validate(TArray<FText>& OutErrors) const
	{
		OutErrors.Reset();
		bool bIsValid = true;

		// Required: Map must be valid (either via AssetId or SoftPath)
		if (!MapAssetId.IsValid() && MapSoftPath.IsNull())
		{
			OutErrors.Add(FText::FromString(TEXT("MapAssetId or MapSoftPath is required but neither is set")));
			bIsValid = false;
		}

		// Multiplayer modes require session data
		if (LoadMode == ELoadMode::MultiplayerClient && SessionData.IsEmpty())
		{
			OutErrors.Add(FText::FromString(TEXT("MultiplayerClient mode requires SessionData (server address)")));
			bIsValid = false;
		}

		// Validate feature names are not empty
		for (int32 i = 0; i < FeaturesToActivate.Num(); ++i)
		{
			if (FeaturesToActivate[i].IsEmpty())
			{
				OutErrors.Add(FText::FromString(FString::Printf(
					TEXT("FeaturesToActivate[%d] is empty"), i)));
				bIsValid = false;
			}
		}

		// Validate content pack names are not empty
		for (int32 i = 0; i < ContentPacksToMount.Num(); ++i)
		{
			if (ContentPacksToMount[i].IsEmpty())
			{
				OutErrors.Add(FText::FromString(FString::Printf(
					TEXT("ContentPacksToMount[%d] is empty"), i)));
				bIsValid = false;
			}
		}

		// Check for duplicate features
		TSet<FString> UniqueFeatures;
		for (const FString& Feature : FeaturesToActivate)
		{
			if (UniqueFeatures.Contains(Feature))
			{
				OutErrors.Add(FText::FromString(FString::Printf(
					TEXT("Duplicate feature in FeaturesToActivate: %s"), *Feature)));
				bIsValid = false;
			}
			UniqueFeatures.Add(Feature);
		}

		// Check for duplicate content packs
		TSet<FString> UniquePacks;
		for (const FString& Pack : ContentPacksToMount)
		{
			if (UniquePacks.Contains(Pack))
			{
				OutErrors.Add(FText::FromString(FString::Printf(
					TEXT("Duplicate pack in ContentPacksToMount: %s"), *Pack)));
				bIsValid = false;
			}
			UniquePacks.Add(Pack);
		}

		return bIsValid;
	}

	/** Get a debug string representation */
	FString ToString() const
	{
		FString MapDesc;
		if (MapAssetId.IsValid())
		{
			MapDesc = MapAssetId.ToString();
		}
		else if (!MapSoftPath.IsNull())
		{
			MapDesc = MapSoftPath.ToString();
		}
		else
		{
			MapDesc = TEXT("<unset>");
		}

		return FString::Printf(TEXT("LoadRequest(Map=%s, Experience=%s, Mode=%d)"),
			*MapDesc,
			*ExperienceAssetId.ToString(),
			static_cast<int32>(LoadMode));
	}
};
