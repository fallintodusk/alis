// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "InputCoreTypes.h"
#include "ProjectSaveData.generated.h"

/**
 * Player profile metadata (character info, playtime, etc.)
 */
USTRUCT(BlueprintType)
struct PROJECTSAVE_API FProjectPlayerProfile
{
	GENERATED_BODY()

	/** Profile display name (character name). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
	FString ProfileName;

	/** Profile creation timestamp. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
	FDateTime CreationTime;

	/** Last save timestamp. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
	FDateTime LastSaveTime;

	/** Total playtime in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
	float TotalPlaytimeSeconds = 0.0f;

	/** Current level/chapter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
	int32 CurrentLevel = 1;

	/** Overall completion percentage (0.0 to 1.0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
	float CompletionPercentage = 0.0f;

	/** Last checkpoint location (map name). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
	FString LastCheckpointMap;

	/** Last checkpoint location (world coordinates). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
	FVector LastCheckpointLocation = FVector::ZeroVector;

	/** Profile thumbnail (for save slot UI). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
	TSoftObjectPtr<class UTexture2D> ProfileThumbnail;
};

/**
 * Game settings data (graphics, audio, controls).
 */
USTRUCT(BlueprintType)
struct PROJECTSAVE_API FProjectGameSettings
{
	GENERATED_BODY()

	// Graphics
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics")
	int32 GraphicsQuality = 2; // 0-4 (Low to Cinematic). Save-applied gameplay quality is capped at High (2).

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics")
	bool bVSync = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics")
	int32 ResolutionWidth = 1920;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics")
	int32 ResolutionHeight = 1080;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics")
	bool bFullscreen = true;

	// Audio
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	float MasterVolume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	float MusicVolume = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	float SFXVolume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	float DialogueVolume = 1.0f;

	// Gameplay
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
	float MouseSensitivity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
	bool bInvertMouseY = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
	float FOV = 90.0f;
};

/**
 * Keybinding data (action name -> key).
 */
USTRUCT(BlueprintType)
struct PROJECTSAVE_API FProjectKeybinding
{
	GENERATED_BODY()

	/** Action name (e.g., "Jump", "Fire"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	FName ActionName;

	/** Primary key. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	FKey PrimaryKey;

	/** Secondary key (alternative binding). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	FKey SecondaryKey;
};

/**
 * World progress data (milestones, completion state).
 */
USTRUCT(BlueprintType)
struct PROJECTSAVE_API FProjectWorldProgress
{
	GENERATED_BODY()

	/** World/map identifier (e.g., "KazanMain"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progress")
	FName WorldId;

	/** Completed milestones (quest IDs, checkpoint IDs, etc.). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progress")
	TArray<FName> CompletedMilestones;

	/** Discovered locations (for map reveal). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progress")
	TArray<FName> DiscoveredLocations;

	/** World-specific completion percentage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progress")
	float CompletionPercentage = 0.0f;

	/** Custom key-value data for world-specific state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progress")
	TMap<FString, FString> CustomData;
};

/**
 * Plugin-owned binary blob (wrapped to satisfy UHT).
 */
USTRUCT(BlueprintType)
struct PROJECTSAVE_API FProjectPluginBinaryData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	TArray<uint8> Data;
};

/**
 * Main save game data container.
 * This is the root object that gets serialized to disk.
 */
UCLASS(BlueprintType)
class PROJECTSAVE_API UProjectSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** Save file version (for migration support). */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	int32 SaveVersion = 1;

	/** Player profile data. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FProjectPlayerProfile PlayerProfile;

	/** Game settings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FProjectGameSettings GameSettings;

	/** Keybindings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	TArray<FProjectKeybinding> Keybindings;

	/** World progress data (per-map). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	TArray<FProjectWorldProgress> WorldProgressData;

	/** Plugin-owned binary blobs (feature save data). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	TMap<FName, FProjectPluginBinaryData> PluginBinaryData;

	/** Find world progress by ID. */
	FProjectWorldProgress* FindWorldProgress(FName WorldId);

	/** Find or create world progress by ID. */
	FProjectWorldProgress& FindOrCreateWorldProgress(FName WorldId);
};
