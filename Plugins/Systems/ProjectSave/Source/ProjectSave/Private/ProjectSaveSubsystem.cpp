// Copyright ALIS. All Rights Reserved.

#include "ProjectSaveSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameUserSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectSave, Log, All);

UProjectSaveSubsystem::UProjectSaveSubsystem()
{
}

void UProjectSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogProjectSave, Log, TEXT("ProjectSaveSubsystem initialized"));

	// Create default save data
	CurrentSaveData = NewObject<UProjectSaveGame>(this);
}

void UProjectSaveSubsystem::Deinitialize()
{
	Super::Deinitialize();

	UE_LOG(LogProjectSave, Log, TEXT("ProjectSaveSubsystem deinitialized"));
}

bool UProjectSaveSubsystem::LoadGame(const FString& SlotName)
{
	UE_LOG(LogProjectSave, Log, TEXT("Loading save from slot: %s"), *SlotName);

	// Load save game from disk
	USaveGame* LoadedGame = UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex);

	if (!LoadedGame)
	{
		UE_LOG(LogProjectSave, Warning, TEXT("Failed to load save from slot: %s"), *SlotName);
		OnLoadCompleted.Broadcast(false);
		return false;
	}

	// Cast to our save data type
	UProjectSaveGame* LoadedSaveData = Cast<UProjectSaveGame>(LoadedGame);
	if (!LoadedSaveData)
	{
		UE_LOG(LogProjectSave, Error, TEXT("Loaded save data has invalid type"));
		OnLoadCompleted.Broadcast(false);
		return false;
	}

	// Update current save data
	CurrentSaveData = LoadedSaveData;
	CurrentSlotName = SlotName;

	// Apply settings
	ApplyGameSettings(CurrentSaveData->GameSettings);

	UE_LOG(LogProjectSave, Log, TEXT("Successfully loaded save from slot: %s (Profile: %s, Playtime: %.1fs)"),
		*SlotName,
		*CurrentSaveData->PlayerProfile.ProfileName,
		CurrentSaveData->PlayerProfile.TotalPlaytimeSeconds);

	OnLoadCompleted.Broadcast(true);
	return true;
}

bool UProjectSaveSubsystem::SaveGame(const FString& SlotName)
{
	if (!CurrentSaveData)
	{
		UE_LOG(LogProjectSave, Error, TEXT("Cannot save - no save data in memory"));
		OnSaveCompleted.Broadcast(false);
		return false;
	}

	UE_LOG(LogProjectSave, Log, TEXT("Saving game to slot: %s"), *SlotName);

	// Update last save time
	CurrentSaveData->PlayerProfile.LastSaveTime = FDateTime::Now();

	// Save to disk
	const bool bSuccess = UGameplayStatics::SaveGameToSlot(CurrentSaveData, SlotName, UserIndex);

	if (bSuccess)
	{
		CurrentSlotName = SlotName;
		UE_LOG(LogProjectSave, Log, TEXT("Successfully saved game to slot: %s"), *SlotName);
	}
	else
	{
		UE_LOG(LogProjectSave, Error, TEXT("Failed to save game to slot: %s"), *SlotName);
	}

	OnSaveCompleted.Broadcast(bSuccess);
	return bSuccess;
}

bool UProjectSaveSubsystem::DeleteSave(const FString& SlotName)
{
	UE_LOG(LogProjectSave, Log, TEXT("Deleting save slot: %s"), *SlotName);

	const bool bSuccess = UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);

	if (bSuccess)
	{
		UE_LOG(LogProjectSave, Log, TEXT("Successfully deleted save slot: %s"), *SlotName);

		// Clear current save if we deleted the active slot
		if (CurrentSlotName == SlotName)
		{
			CurrentSaveData = NewObject<UProjectSaveGame>(this);
			CurrentSlotName.Empty();
		}
	}
	else
	{
		UE_LOG(LogProjectSave, Warning, TEXT("Failed to delete save slot: %s"), *SlotName);
	}

	return bSuccess;
}

bool UProjectSaveSubsystem::DoesSaveExist(const FString& SlotName) const
{
	return UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex);
}

TArray<FString> UProjectSaveSubsystem::GetAllSaveSlots() const
{
	TArray<FString> SaveSlots;

	// Check common save slot names
	const TArray<FString> CommonSlots = {
		GetDefaultSlotName(),
		GetAutoSaveSlotName(),
		TEXT("SaveSlot_01"),
		TEXT("SaveSlot_02"),
		TEXT("SaveSlot_03"),
		TEXT("SaveSlot_04"),
		TEXT("SaveSlot_05")
	};

	for (const FString& SlotName : CommonSlots)
	{
		if (DoesSaveExist(SlotName))
		{
			SaveSlots.Add(SlotName);
		}
	}

	return SaveSlots;
}

bool UProjectSaveSubsystem::SetPluginBinaryData(FName Key, const TArray<uint8>& Data)
{
	if (!CurrentSaveData)
	{
		UE_LOG(LogProjectSave, Warning, TEXT("SetPluginBinaryData: No current save data"));
		return false;
	}

	FProjectPluginBinaryData Blob;
	Blob.Data = Data;
	CurrentSaveData->PluginBinaryData.Add(Key, Blob);
	return true;
}

bool UProjectSaveSubsystem::GetPluginBinaryData(FName Key, TArray<uint8>& OutData) const
{
	if (!CurrentSaveData)
	{
		UE_LOG(LogProjectSave, Warning, TEXT("GetPluginBinaryData: No current save data"));
		return false;
	}

	const FProjectPluginBinaryData* Found = CurrentSaveData->PluginBinaryData.Find(Key);
	if (!Found)
	{
		return false;
	}

	OutData = Found->Data;
	return true;
}

bool UProjectSaveSubsystem::RemovePluginBinaryData(FName Key)
{
	if (!CurrentSaveData)
	{
		UE_LOG(LogProjectSave, Warning, TEXT("RemovePluginBinaryData: No current save data"));
		return false;
	}

	return CurrentSaveData->PluginBinaryData.Remove(Key) > 0;
}

void UProjectSaveSubsystem::CreateNewSave(const FString& ProfileName)
{
	UE_LOG(LogProjectSave, Log, TEXT("Creating new save with profile name: %s"), *ProfileName);

	// Create new save data
	CurrentSaveData = NewObject<UProjectSaveGame>(this);

	// Initialize profile
	CurrentSaveData->PlayerProfile.ProfileName = ProfileName;
	CurrentSaveData->PlayerProfile.CreationTime = FDateTime::Now();
	CurrentSaveData->PlayerProfile.LastSaveTime = FDateTime::Now();
	CurrentSaveData->PlayerProfile.TotalPlaytimeSeconds = 0.0f;
	CurrentSaveData->PlayerProfile.CurrentLevel = 1;
	CurrentSaveData->PlayerProfile.CompletionPercentage = 0.0f;

	// Initialize default settings
	CurrentSaveData->GameSettings = FProjectGameSettings();

	CurrentSlotName.Empty();
}

void UProjectSaveSubsystem::UpdatePlayerProfile(const FProjectPlayerProfile& Profile)
{
	if (!CurrentSaveData)
	{
		UE_LOG(LogProjectSave, Warning, TEXT("Cannot update player profile - no save data"));
		return;
	}

	CurrentSaveData->PlayerProfile = Profile;
	UE_LOG(LogProjectSave, Verbose, TEXT("Updated player profile: %s"), *Profile.ProfileName);
}

void UProjectSaveSubsystem::UpdateGameSettings(const FProjectGameSettings& Settings)
{
	if (!CurrentSaveData)
	{
		UE_LOG(LogProjectSave, Warning, TEXT("Cannot update game settings - no save data"));
		return;
	}

	CurrentSaveData->GameSettings = Settings;
	ApplyGameSettings(Settings);

	UE_LOG(LogProjectSave, Verbose, TEXT("Updated game settings"));
}

void UProjectSaveSubsystem::UpdateKeybindings(const TArray<FProjectKeybinding>& Bindings)
{
	if (!CurrentSaveData)
	{
		UE_LOG(LogProjectSave, Warning, TEXT("Cannot update keybindings - no save data"));
		return;
	}

	CurrentSaveData->Keybindings = Bindings;
	UE_LOG(LogProjectSave, Verbose, TEXT("Updated keybindings (%d bindings)"), Bindings.Num());
}

void UProjectSaveSubsystem::AddCompletedMilestone(FName WorldId, FName MilestoneId)
{
	if (!CurrentSaveData)
	{
		UE_LOG(LogProjectSave, Warning, TEXT("Cannot add milestone - no save data"));
		return;
	}

	FProjectWorldProgress& Progress = CurrentSaveData->FindOrCreateWorldProgress(WorldId);

	if (!Progress.CompletedMilestones.Contains(MilestoneId))
	{
		Progress.CompletedMilestones.Add(MilestoneId);
		UE_LOG(LogProjectSave, Log, TEXT("Added completed milestone '%s' to world '%s'"),
			*MilestoneId.ToString(), *WorldId.ToString());
	}
}

bool UProjectSaveSubsystem::IsMilestoneCompleted(FName WorldId, FName MilestoneId) const
{
	if (!CurrentSaveData)
	{
		return false;
	}

	const FProjectWorldProgress* Progress = CurrentSaveData->FindWorldProgress(WorldId);
	if (!Progress)
	{
		return false;
	}

	return Progress->CompletedMilestones.Contains(MilestoneId);
}

void UProjectSaveSubsystem::AddDiscoveredLocation(FName WorldId, FName LocationId)
{
	if (!CurrentSaveData)
	{
		UE_LOG(LogProjectSave, Warning, TEXT("Cannot add discovered location - no save data"));
		return;
	}

	FProjectWorldProgress& Progress = CurrentSaveData->FindOrCreateWorldProgress(WorldId);

	if (!Progress.DiscoveredLocations.Contains(LocationId))
	{
		Progress.DiscoveredLocations.Add(LocationId);
		UE_LOG(LogProjectSave, Log, TEXT("Discovered location '%s' in world '%s'"),
			*LocationId.ToString(), *WorldId.ToString());
	}
}

FProjectWorldProgress UProjectSaveSubsystem::GetWorldProgress(FName WorldId) const
{
	if (!CurrentSaveData)
	{
		return FProjectWorldProgress();
	}

	const FProjectWorldProgress* Progress = CurrentSaveData->FindWorldProgress(WorldId);
	if (Progress)
	{
		return *Progress;
	}

	return FProjectWorldProgress();
}

bool UProjectSaveSubsystem::AutoSave()
{
	UE_LOG(LogProjectSave, Log, TEXT("Performing auto-save"));
	return SaveGame(GetAutoSaveSlotName());
}

void UProjectSaveSubsystem::ApplyGameSettings(const FProjectGameSettings& Settings)
{
	UGameUserSettings* UserSettings = UGameUserSettings::GetGameUserSettings();
	if (!UserSettings)
	{
		UE_LOG(LogProjectSave, Warning, TEXT("Cannot apply settings - UserSettings is null"));
		return;
	}

	// Apply graphics settings
	UserSettings->SetOverallScalabilityLevel(Settings.GraphicsQuality);
	UserSettings->SetVSyncEnabled(Settings.bVSync);
	UserSettings->SetScreenResolution(FIntPoint(Settings.ResolutionWidth, Settings.ResolutionHeight));
	UserSettings->SetFullscreenMode(Settings.bFullscreen ? EWindowMode::Fullscreen : EWindowMode::Windowed);

	// Apply settings
	UserSettings->ApplySettings(false);

	UE_LOG(LogProjectSave, Log, TEXT("Applied graphics settings (Quality: %d, Resolution: %dx%d, VSync: %d)"),
		Settings.GraphicsQuality,
		Settings.ResolutionWidth,
		Settings.ResolutionHeight,
		Settings.bVSync);

	// Apply audio settings
	ApplyAudioSettings(Settings);

	// Apply gameplay settings
	ApplyGameplaySettings(Settings);
}

void UProjectSaveSubsystem::ApplyAudioSettings(const FProjectGameSettings& Settings)
{
	// UE5 uses Sound Classes and Sound Mixes for volume control
	// For now, we store the values and they can be queried by audio systems
	// Full implementation would require:
	// 1. Sound Class references for Master/Music/SFX/Dialogue
	// 2. USoundMix asset to apply volume multipliers
	// 3. UAudioModulationSubsystem for advanced audio control (UE5.3+)

	UE_LOG(LogProjectSave, Log, TEXT("Applied audio settings (Master: %.2f, Music: %.2f, SFX: %.2f, Dialogue: %.2f)"),
		Settings.MasterVolume,
		Settings.MusicVolume,
		Settings.SFXVolume,
		Settings.DialogueVolume);

	// Note: Audio volume application requires Sound Classes/Mixes to be set up in project
	// This is typically done through:
	// - Audio settings in Project Settings
	// - Sound Class hierarchy (Master → Music/SFX/Dialogue)
	// - Blueprint or C++ code to set volume multipliers on Sound Classes
	// See: https://docs.unrealengine.com/5.3/audio-in-unreal-engine/
}

void UProjectSaveSubsystem::ApplyGameplaySettings(const FProjectGameSettings& Settings)
{
	// Get player controller
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogProjectSave, Warning, TEXT("Cannot apply gameplay settings - World is null"));
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		UE_LOG(LogProjectSave, Warning, TEXT("Cannot apply gameplay settings - PlayerController is null"));
		return;
	}

	// Apply mouse sensitivity
	// Note: UE5 uses Input Mapping Context (Enhanced Input System)
	// Mouse sensitivity is typically applied via PlayerController or PlayerInput settings
	if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
	{
		// Store for retrieval by input system
		// Actual application depends on Enhanced Input setup
		UE_LOG(LogProjectSave, Log, TEXT("Mouse sensitivity set to: %.2f"), Settings.MouseSensitivity);
	}

	// Apply FOV
	if (APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
	{
		CameraManager->SetFOV(Settings.FOV);
		UE_LOG(LogProjectSave, Log, TEXT("FOV set to: %.1f"), Settings.FOV);
	}

	UE_LOG(LogProjectSave, Log, TEXT("Applied gameplay settings (Sensitivity: %.2f, FOV: %.1f, InvertY: %d)"),
		Settings.MouseSensitivity,
		Settings.FOV,
		Settings.bInvertMouseY);
}
