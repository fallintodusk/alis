// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProjectProgressSnapshot.generated.h"

/**
 * Read-only snapshot of player progress for UI display.
 * Used by menus to show save slot summaries, completion stats, etc.
 */
USTRUCT(BlueprintType)
struct PROJECTSAVE_API FProjectProgressSnapshot
{
	GENERATED_BODY()

	/** Profile name (character name). */
	UPROPERTY(BlueprintReadOnly, Category = "Progress")
	FString ProfileName;

	/** Total playtime (formatted string, e.g., "12h 34m"). */
	UPROPERTY(BlueprintReadOnly, Category = "Progress")
	FString PlaytimeFormatted;

	/** Total playtime in seconds. */
	UPROPERTY(BlueprintReadOnly, Category = "Progress")
	float PlaytimeSeconds = 0.0f;

	/** Overall completion percentage (0.0 to 1.0). */
	UPROPERTY(BlueprintReadOnly, Category = "Progress")
	float CompletionPercentage = 0.0f;

	/** Current level/chapter. */
	UPROPERTY(BlueprintReadOnly, Category = "Progress")
	int32 CurrentLevel = 1;

	/** Last checkpoint map name. */
	UPROPERTY(BlueprintReadOnly, Category = "Progress")
	FString LastCheckpointMap;

	/** Last checkpoint location name (human-readable). */
	UPROPERTY(BlueprintReadOnly, Category = "Progress")
	FString LastCheckpointLocationName;

	/** Last save timestamp (formatted string, e.g., "2025-01-15 14:30"). */
	UPROPERTY(BlueprintReadOnly, Category = "Progress")
	FString LastSaveTimeFormatted;

	/** Profile thumbnail for UI. */
	UPROPERTY(BlueprintReadOnly, Category = "Progress")
	TSoftObjectPtr<class UTexture2D> ProfileThumbnail;

	/** Total milestones completed across all worlds. */
	UPROPERTY(BlueprintReadOnly, Category = "Progress")
	int32 TotalMilestonesCompleted = 0;

	/** Total locations discovered across all worlds. */
	UPROPERTY(BlueprintReadOnly, Category = "Progress")
	int32 TotalLocationsDiscovered = 0;

	/** Number of worlds visited. */
	UPROPERTY(BlueprintReadOnly, Category = "Progress")
	int32 WorldsVisited = 0;
};

/**
 * Read-only snapshot of per-world progress.
 */
USTRUCT(BlueprintType)
struct PROJECTSAVE_API FProjectWorldProgressSnapshot
{
	GENERATED_BODY()

	/** World ID. */
	UPROPERTY(BlueprintReadOnly, Category = "Progress")
	FName WorldId;

	/** World display name. */
	UPROPERTY(BlueprintReadOnly, Category = "Progress")
	FString WorldDisplayName;

	/** Completion percentage for this world (0.0 to 1.0). */
	UPROPERTY(BlueprintReadOnly, Category = "Progress")
	float CompletionPercentage = 0.0f;

	/** Number of milestones completed. */
	UPROPERTY(BlueprintReadOnly, Category = "Progress")
	int32 MilestonesCompleted = 0;

	/** Number of locations discovered. */
	UPROPERTY(BlueprintReadOnly, Category = "Progress")
	int32 LocationsDiscovered = 0;

	/** World icon/thumbnail. */
	UPROPERTY(BlueprintReadOnly, Category = "Progress")
	TSoftObjectPtr<class UTexture2D> WorldIcon;
};

/**
 * Blueprint function library for creating progress snapshots.
 * These are read-only summaries for UI display.
 */
UCLASS()
class PROJECTSAVE_API UProjectProgressSnapshotLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Create progress snapshot from save data.
	 * @param SaveData The save data to create snapshot from
	 * @return Read-only progress snapshot for UI display
	 */
	UFUNCTION(BlueprintPure, Category = "Project|Progress")
	static FProjectProgressSnapshot CreateProgressSnapshot(const class UProjectSaveGame* SaveData);

	/**
	 * Create world progress snapshot from world progress data.
	 * @param WorldProgress The world progress data
	 * @return Read-only world progress snapshot for UI display
	 */
	UFUNCTION(BlueprintPure, Category = "Project|Progress")
	static FProjectWorldProgressSnapshot CreateWorldProgressSnapshot(const struct FProjectWorldProgress& WorldProgress);

	/**
	 * Get all world progress snapshots from save data.
	 * @param SaveData The save data
	 * @return Array of world progress snapshots
	 */
	UFUNCTION(BlueprintPure, Category = "Project|Progress")
	static TArray<FProjectWorldProgressSnapshot> GetAllWorldSnapshots(const class UProjectSaveGame* SaveData);

	/**
	 * Format playtime seconds to human-readable string (e.g., "12h 34m").
	 * @param Seconds Total playtime in seconds
	 * @return Formatted playtime string
	 */
	UFUNCTION(BlueprintPure, Category = "Project|Progress")
	static FString FormatPlaytime(float Seconds);

	/**
	 * Format DateTime to human-readable string (e.g., "2025-01-15 14:30").
	 * @param DateTime The DateTime to format
	 * @return Formatted date/time string
	 */
	UFUNCTION(BlueprintPure, Category = "Project|Progress")
	static FString FormatDateTime(const FDateTime& DateTime);

	/**
	 * Calculate overall completion percentage from save data.
	 * Aggregates completion across all worlds.
	 * @param SaveData The save data
	 * @return Overall completion percentage (0.0 to 1.0)
	 */
	UFUNCTION(BlueprintPure, Category = "Project|Progress")
	static float CalculateOverallCompletion(const class UProjectSaveGame* SaveData);
};
