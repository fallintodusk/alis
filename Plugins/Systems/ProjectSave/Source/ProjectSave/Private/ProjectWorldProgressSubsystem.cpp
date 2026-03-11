// Copyright ALIS. All Rights Reserved.

#include "ProjectWorldProgressSubsystem.h"
#include "ProjectSaveSubsystem.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectProgress, Log, All);

UProjectWorldProgressSubsystem::UProjectWorldProgressSubsystem()
{
}

void UProjectWorldProgressSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Determine current world ID from world name
	if (UWorld* World = GetWorld())
	{
		const FString WorldName = World->GetName();
		CachedWorldId = FName(*WorldName);

		UE_LOG(LogProjectProgress, Log, TEXT("ProjectWorldProgressSubsystem initialized for world: %s"), *WorldName);
	}

	// Note: Progress will be loaded lazily on first access (deferred initialization)
}

void UProjectWorldProgressSubsystem::Deinitialize()
{
	// Only flush if SaveSubsystem is still available
	// During session end (editor close/game exit), GameInstance subsystems
	// are already destroyed before World subsystems - skip flush in that case
	if (UProjectSaveSubsystem* SaveSubsystem = GetSaveSubsystem())
	{
		FlushToSave();
	}

	Super::Deinitialize();

	UE_LOG(LogProjectProgress, Log, TEXT("ProjectWorldProgressSubsystem deinitialized"));
}

void UProjectWorldProgressSubsystem::RecordMilestone(FName MilestoneId, const FString& Context)
{
	EnsureProgressLoaded();

	if (InMemoryProgress.CompletedMilestones.Contains(MilestoneId))
	{
		UE_LOG(LogProjectProgress, Verbose, TEXT("Milestone '%s' already completed"), *MilestoneId.ToString());
		return;
	}

	// Add to in-memory progress
	InMemoryProgress.CompletedMilestones.Add(MilestoneId);

	UE_LOG(LogProjectProgress, Log, TEXT("Milestone completed: %s (Context: %s)"),
		*MilestoneId.ToString(), *Context);

	// Update completion percentage
	const float NewCompletion = CalculateCompletionPercentage();
	InMemoryProgress.CompletionPercentage = NewCompletion;

	// Fire events
	OnMilestoneCompleted.Broadcast(CachedWorldId, MilestoneId);
	OnProgressUpdated.Broadcast(NewCompletion);

	// Flush to save
	FlushToSave();
}

bool UProjectWorldProgressSubsystem::HasMilestone(FName MilestoneId) const
{
	EnsureProgressLoaded();
	return InMemoryProgress.CompletedMilestones.Contains(MilestoneId);
}

void UProjectWorldProgressSubsystem::DiscoverLocation(FName LocationId)
{
	EnsureProgressLoaded();

	if (InMemoryProgress.DiscoveredLocations.Contains(LocationId))
	{
		UE_LOG(LogProjectProgress, Verbose, TEXT("Location '%s' already discovered"), *LocationId.ToString());
		return;
	}

	// Add to in-memory progress
	InMemoryProgress.DiscoveredLocations.Add(LocationId);

	UE_LOG(LogProjectProgress, Log, TEXT("Location discovered: %s"), *LocationId.ToString());

	// Update completion percentage
	const float NewCompletion = CalculateCompletionPercentage();
	InMemoryProgress.CompletionPercentage = NewCompletion;

	// Fire events
	OnLocationDiscovered.Broadcast(CachedWorldId, LocationId);
	OnProgressUpdated.Broadcast(NewCompletion);

	// Flush to save
	FlushToSave();
}

bool UProjectWorldProgressSubsystem::IsLocationDiscovered(FName LocationId) const
{
	EnsureProgressLoaded();
	return InMemoryProgress.DiscoveredLocations.Contains(LocationId);
}

void UProjectWorldProgressSubsystem::SetCustomData(const FString& Key, const FString& Value)
{
	EnsureProgressLoaded();

	InMemoryProgress.CustomData.Add(Key, Value);

	UE_LOG(LogProjectProgress, Verbose, TEXT("Set custom data: %s = %s"), *Key, *Value);
}

FString UProjectWorldProgressSubsystem::GetCustomData(const FString& Key) const
{
	EnsureProgressLoaded();
	const FString* Value = InMemoryProgress.CustomData.Find(Key);
	return Value ? *Value : FString();
}

float UProjectWorldProgressSubsystem::CalculateCompletionPercentage() const
{
	EnsureProgressLoaded();

	if (TotalMilestones == 0 && TotalLocations == 0)
	{
		return 0.0f;
	}

	// Calculate milestone completion (60% weight)
	const float MilestoneCompletion = TotalMilestones > 0
		? static_cast<float>(InMemoryProgress.CompletedMilestones.Num()) / static_cast<float>(TotalMilestones)
		: 0.0f;

	// Calculate location discovery (40% weight)
	const float LocationCompletion = TotalLocations > 0
		? static_cast<float>(InMemoryProgress.DiscoveredLocations.Num()) / static_cast<float>(TotalLocations)
		: 0.0f;

	// Weighted average
	const float OverallCompletion = (MilestoneCompletion * 0.6f) + (LocationCompletion * 0.4f);

	return FMath::Clamp(OverallCompletion, 0.0f, 1.0f);
}

TArray<FName> UProjectWorldProgressSubsystem::GetCompletedMilestones() const
{
	EnsureProgressLoaded();
	return InMemoryProgress.CompletedMilestones;
}

TArray<FName> UProjectWorldProgressSubsystem::GetDiscoveredLocations() const
{
	EnsureProgressLoaded();
	return InMemoryProgress.DiscoveredLocations;
}

void UProjectWorldProgressSubsystem::FlushToSave()
{
	UProjectSaveSubsystem* SaveSubsystem = GetSaveSubsystem();
	if (!SaveSubsystem)
	{
		UE_LOG(LogProjectProgress, Warning, TEXT("Cannot flush progress - SaveSubsystem not found"));
		return;
	}

	UProjectSaveGame* SaveData = SaveSubsystem->GetCurrentSaveMutable();
	if (!SaveData)
	{
		UE_LOG(LogProjectProgress, Warning, TEXT("Cannot flush progress - no save data"));
		return;
	}

	// Update world progress in save data
	FProjectWorldProgress& SaveProgress = SaveData->FindOrCreateWorldProgress(CachedWorldId);
	SaveProgress = InMemoryProgress;

	UE_LOG(LogProjectProgress, Verbose, TEXT("Flushed progress to save (Milestones: %d, Locations: %d, Completion: %.1f%%)"),
		InMemoryProgress.CompletedMilestones.Num(),
		InMemoryProgress.DiscoveredLocations.Num(),
		InMemoryProgress.CompletionPercentage * 100.0f);
}

FName UProjectWorldProgressSubsystem::GetCurrentWorldId() const
{
	return CachedWorldId;
}

UProjectSaveSubsystem* UProjectWorldProgressSubsystem::GetSaveSubsystem() const
{
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			return GameInstance->GetSubsystem<UProjectSaveSubsystem>();
		}
	}
	return nullptr;
}

void UProjectWorldProgressSubsystem::LoadProgressFromSave()
{
	// Force reload even if already loaded (explicit reload support)
	bProgressLoaded = false;
	EnsureProgressLoaded();
}

void UProjectWorldProgressSubsystem::EnsureProgressLoaded() const
{
	// Already loaded? Skip.
	if (bProgressLoaded)
	{
		return;
	}

	// Mark as loaded immediately to prevent recursive calls
	bProgressLoaded = true;

	// Need non-const self to call GetSaveSubsystem() and write InMemoryProgress
	UProjectWorldProgressSubsystem* Self = const_cast<UProjectWorldProgressSubsystem*>(this);

	UProjectSaveSubsystem* SaveSubsystem = Self->GetSaveSubsystem();
	if (!SaveSubsystem)
	{
		UE_LOG(LogProjectProgress, Warning, TEXT("SaveSubsystem not ready. Progress remains empty."));
		return;
	}

	// Get existing progress from save
	const FProjectWorldProgress SavedProgress = SaveSubsystem->GetWorldProgress(Self->CachedWorldId);

	// Load into memory
	Self->InMemoryProgress = SavedProgress;
	Self->InMemoryProgress.WorldId = Self->CachedWorldId;

	UE_LOG(LogProjectProgress, Log, TEXT("Loaded progress (Milestones: %d, Locations: %d)"),
		Self->InMemoryProgress.CompletedMilestones.Num(),
		Self->InMemoryProgress.DiscoveredLocations.Num());
}
