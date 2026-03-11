// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/ProjectSaveData.h"
#include "ProjectSaveSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FProjectSaveCompleted, bool /* bSuccess */);
DECLARE_MULTICAST_DELEGATE_OneParam(FProjectLoadCompleted, bool /* bSuccess */);

/**
 * GameInstance subsystem responsible for save/load operations.
 * Manages player profiles, game settings, and world progress.
 *
 * TODO: DIP Pattern Implementation
 * - Define ISaveService interface in ProjectCore (save/load/delete operations)
 * - Make this subsystem implement ISaveService
 * - Register with FProjectServiceLocator::Register<ISaveService>() in Initialize()
 * - Unregister in Deinitialize()
 * - See ProjectLoadingSubsystem for reference implementation pattern
 */
UCLASS()
class PROJECTSAVE_API UProjectSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UProjectSaveSubsystem();

	//~UGameInstanceSubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Load save data from slot. Returns true if loaded successfully. */
	UFUNCTION(BlueprintCallable, Category = "Project|Save")
	bool LoadGame(const FString& SlotName);

	/** Save current data to slot. Returns true if saved successfully. */
	UFUNCTION(BlueprintCallable, Category = "Project|Save")
	bool SaveGame(const FString& SlotName);

	/** Delete save file. */
	UFUNCTION(BlueprintCallable, Category = "Project|Save")
	bool DeleteSave(const FString& SlotName);

	/** Check if save file exists. */
	UFUNCTION(BlueprintPure, Category = "Project|Save")
	bool DoesSaveExist(const FString& SlotName) const;

	/** Get list of all save slot names. */
	UFUNCTION(BlueprintCallable, Category = "Project|Save")
	TArray<FString> GetAllSaveSlots() const;

	/** Get current save data (read-only). */
	UFUNCTION(BlueprintPure, Category = "Project|Save")
	const UProjectSaveGame* GetCurrentSave() const { return CurrentSaveData; }

	/** Get current save data (mutable). */
	UProjectSaveGame* GetCurrentSaveMutable() { return CurrentSaveData; }

	/** Store plugin-owned binary data in the current save. */
	bool SetPluginBinaryData(FName Key, const TArray<uint8>& Data);

	/** Read plugin-owned binary data from the current save. */
	bool GetPluginBinaryData(FName Key, TArray<uint8>& OutData) const;

	/** Remove plugin-owned binary data from the current save. */
	bool RemovePluginBinaryData(FName Key);

	/** Create new save data with default values. */
	UFUNCTION(BlueprintCallable, Category = "Project|Save")
	void CreateNewSave(const FString& ProfileName);

	/** Update player profile. */
	UFUNCTION(BlueprintCallable, Category = "Project|Save")
	void UpdatePlayerProfile(const FProjectPlayerProfile& Profile);

	/** Update game settings. */
	UFUNCTION(BlueprintCallable, Category = "Project|Save")
	void UpdateGameSettings(const FProjectGameSettings& Settings);

	/** Update keybindings. */
	UFUNCTION(BlueprintCallable, Category = "Project|Save")
	void UpdateKeybindings(const TArray<FProjectKeybinding>& Bindings);

	/** Add completed milestone to world progress. */
	UFUNCTION(BlueprintCallable, Category = "Project|Save")
	void AddCompletedMilestone(FName WorldId, FName MilestoneId);

	/** Check if milestone is completed. */
	UFUNCTION(BlueprintPure, Category = "Project|Save")
	bool IsMilestoneCompleted(FName WorldId, FName MilestoneId) const;

	/** Add discovered location to world progress. */
	UFUNCTION(BlueprintCallable, Category = "Project|Save")
	void AddDiscoveredLocation(FName WorldId, FName LocationId);

	/** Get world progress for specific world. */
	UFUNCTION(BlueprintPure, Category = "Project|Save")
	FProjectWorldProgress GetWorldProgress(FName WorldId) const;

	/** Auto-save current state (uses auto-save slot). */
	UFUNCTION(BlueprintCallable, Category = "Project|Save")
	bool AutoSave();

	/** Get default save slot name. */
	static FString GetDefaultSlotName() { return TEXT("SaveSlot_Default"); }

	/** Get auto-save slot name. */
	static FString GetAutoSaveSlotName() { return TEXT("SaveSlot_AutoSave"); }

public:
	/** Event fired when save completes. */
	FProjectSaveCompleted OnSaveCompleted;

	/** Event fired when load completes. */
	FProjectLoadCompleted OnLoadCompleted;

protected:
	/** Current save data in memory. */
	UPROPERTY()
	TObjectPtr<UProjectSaveGame> CurrentSaveData;

	/** Currently loaded slot name. */
	UPROPERTY()
	FString CurrentSlotName;

	/** User index for save system (default: 0). */
	UPROPERTY()
	int32 UserIndex = 0;

	/** Apply loaded settings to game. */
	void ApplyGameSettings(const FProjectGameSettings& Settings);

	/** Apply audio settings (Master/Music/SFX/Dialogue volumes). */
	void ApplyAudioSettings(const FProjectGameSettings& Settings);

	/** Apply gameplay settings (mouse sensitivity, FOV, etc.). */
	void ApplyGameplaySettings(const FProjectGameSettings& Settings);
};
