// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Data/ProjectProgressSnapshot.h"
#include "Data/ProjectSaveData.h"

FProjectProgressSnapshot UProjectProgressSnapshotLibrary::CreateProgressSnapshot(const UProjectSaveGame* SaveData)
{
	FProjectProgressSnapshot Snapshot;

	if (!SaveData)
	{
		return Snapshot;
	}

	const FProjectPlayerProfile& Profile = SaveData->PlayerProfile;

	// Basic profile info
	Snapshot.ProfileName = Profile.ProfileName;
	Snapshot.PlaytimeSeconds = Profile.TotalPlaytimeSeconds;
	Snapshot.PlaytimeFormatted = FormatPlaytime(Profile.TotalPlaytimeSeconds);
	Snapshot.CurrentLevel = Profile.CurrentLevel;
	Snapshot.LastCheckpointMap = Profile.LastCheckpointMap;
	Snapshot.LastSaveTimeFormatted = FormatDateTime(Profile.LastSaveTime);
	Snapshot.ProfileThumbnail = Profile.ProfileThumbnail;

	// Calculate overall completion
	Snapshot.CompletionPercentage = CalculateOverallCompletion(SaveData);

	// Aggregate world progress stats
	int32 TotalMilestones = 0;
	int32 TotalLocations = 0;

	for (const FProjectWorldProgress& WorldProgress : SaveData->WorldProgressData)
	{
		TotalMilestones += WorldProgress.CompletedMilestones.Num();
		TotalLocations += WorldProgress.DiscoveredLocations.Num();
	}

	Snapshot.TotalMilestonesCompleted = TotalMilestones;
	Snapshot.TotalLocationsDiscovered = TotalLocations;
	Snapshot.WorldsVisited = SaveData->WorldProgressData.Num();

	// TODO: Convert last checkpoint location to human-readable name
	Snapshot.LastCheckpointLocationName = Profile.LastCheckpointMap;

	return Snapshot;
}

FProjectWorldProgressSnapshot UProjectProgressSnapshotLibrary::CreateWorldProgressSnapshot(const FProjectWorldProgress& WorldProgress)
{
	FProjectWorldProgressSnapshot Snapshot;

	Snapshot.WorldId = WorldProgress.WorldId;
	Snapshot.WorldDisplayName = WorldProgress.WorldId.ToString(); // TODO: Look up localized name
	Snapshot.CompletionPercentage = WorldProgress.CompletionPercentage;
	Snapshot.MilestonesCompleted = WorldProgress.CompletedMilestones.Num();
	Snapshot.LocationsDiscovered = WorldProgress.DiscoveredLocations.Num();

	// TODO: Look up world icon from asset registry or manifest

	return Snapshot;
}

TArray<FProjectWorldProgressSnapshot> UProjectProgressSnapshotLibrary::GetAllWorldSnapshots(const UProjectSaveGame* SaveData)
{
	TArray<FProjectWorldProgressSnapshot> Snapshots;

	if (!SaveData)
	{
		return Snapshots;
	}

	for (const FProjectWorldProgress& WorldProgress : SaveData->WorldProgressData)
	{
		Snapshots.Add(CreateWorldProgressSnapshot(WorldProgress));
	}

	return Snapshots;
}

FString UProjectProgressSnapshotLibrary::FormatPlaytime(float Seconds)
{
	const int32 TotalSeconds = FMath::FloorToInt(Seconds);
	const int32 Hours = TotalSeconds / 3600;
	const int32 Minutes = (TotalSeconds % 3600) / 60;

	if (Hours > 0)
	{
		return FString::Printf(TEXT("%dh %dm"), Hours, Minutes);
	}
	else if (Minutes > 0)
	{
		return FString::Printf(TEXT("%dm"), Minutes);
	}
	else
	{
		return TEXT("< 1m");
	}
}

FString UProjectProgressSnapshotLibrary::FormatDateTime(const FDateTime& DateTime)
{
	// Format: "2025-01-15 14:30"
	return FString::Printf(TEXT("%04d-%02d-%02d %02d:%02d"),
		DateTime.GetYear(),
		DateTime.GetMonth(),
		DateTime.GetDay(),
		DateTime.GetHour(),
		DateTime.GetMinute());
}

float UProjectProgressSnapshotLibrary::CalculateOverallCompletion(const UProjectSaveGame* SaveData)
{
	if (!SaveData || SaveData->WorldProgressData.Num() == 0)
	{
		return 0.0f;
	}

	// Average completion across all worlds
	float TotalCompletion = 0.0f;
	for (const FProjectWorldProgress& WorldProgress : SaveData->WorldProgressData)
	{
		TotalCompletion += WorldProgress.CompletionPercentage;
	}

	return TotalCompletion / static_cast<float>(SaveData->WorldProgressData.Num());
}
