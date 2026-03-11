// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/ProjectSaveData.h"
#include "ProjectWorldProgressSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FProjectMilestoneCompleted, FName /* WorldId */, FName /* MilestoneId */);
DECLARE_MULTICAST_DELEGATE_TwoParams(FProjectLocationDiscovered, FName /* WorldId */, FName /* LocationId */);
DECLARE_MULTICAST_DELEGATE_OneParam(FProjectProgressUpdated, float /* CompletionPercentage */);

/**
 * World subsystem responsible for tracking map-specific progress.
 * Normalizes progress data to structs and pushes updates to ProjectSave.
 */
UCLASS()
class PROJECTSAVE_API UProjectWorldProgressSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UProjectWorldProgressSubsystem();

	//~UWorldSubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Record a completed milestone (e.g., quest completed, checkpoint reached). */
	UFUNCTION(BlueprintCallable, Category = "Project|Progress")
	void RecordMilestone(FName MilestoneId, const FString& Context = TEXT(""));

	/** Check if milestone is completed in current world. */
	UFUNCTION(BlueprintPure, Category = "Project|Progress")
	bool HasMilestone(FName MilestoneId) const;

	/** Discover a location (for map reveal, fast travel unlock, etc.). */
	UFUNCTION(BlueprintCallable, Category = "Project|Progress")
	void DiscoverLocation(FName LocationId);

	/** Check if location is discovered in current world. */
	UFUNCTION(BlueprintPure, Category = "Project|Progress")
	bool IsLocationDiscovered(FName LocationId) const;

	/** Set custom progress data (key-value pairs for world-specific state). */
	UFUNCTION(BlueprintCallable, Category = "Project|Progress")
	void SetCustomData(const FString& Key, const FString& Value);

	/** Get custom progress data. */
	UFUNCTION(BlueprintPure, Category = "Project|Progress")
	FString GetCustomData(const FString& Key) const;

	/** Calculate overall completion percentage for current world (0.0 to 1.0). */
	UFUNCTION(BlueprintPure, Category = "Project|Progress")
	float CalculateCompletionPercentage() const;

	/** Get all completed milestones for current world. */
	UFUNCTION(BlueprintPure, Category = "Project|Progress")
	TArray<FName> GetCompletedMilestones() const;

	/** Get all discovered locations for current world. */
	UFUNCTION(BlueprintPure, Category = "Project|Progress")
	TArray<FName> GetDiscoveredLocations() const;

	/** Flush current progress to save system. */
	UFUNCTION(BlueprintCallable, Category = "Project|Progress")
	void FlushToSave();

	/** Get current world ID (derived from world name). */
	UFUNCTION(BlueprintPure, Category = "Project|Progress")
	FName GetCurrentWorldId() const;

public:
	/** Event fired when milestone is completed. */
	FProjectMilestoneCompleted OnMilestoneCompleted;

	/** Event fired when location is discovered. */
	FProjectLocationDiscovered OnLocationDiscovered;

	/** Event fired when overall progress changes. */
	FProjectProgressUpdated OnProgressUpdated;

protected:
	/** Cached world ID for current world. */
	UPROPERTY()
	FName CachedWorldId;

	/** In-memory progress data (synced to save on flush). */
	FProjectWorldProgress InMemoryProgress;

	/** Total number of milestones in this world (for completion calculation). */
	UPROPERTY()
	int32 TotalMilestones = 100;

	/** Total number of locations in this world (for completion calculation). */
	UPROPERTY()
	int32 TotalLocations = 50;

	/** Get save subsystem. */
	class UProjectSaveSubsystem* GetSaveSubsystem() const;

	/** Load progress from save system. */
	void LoadProgressFromSave();

	/** Ensure progress is loaded (lazy initialization). */
	void EnsureProgressLoaded() const;

private:
	/** Track if progress has been loaded from save. */
	mutable bool bProgressLoaded = false;
};
