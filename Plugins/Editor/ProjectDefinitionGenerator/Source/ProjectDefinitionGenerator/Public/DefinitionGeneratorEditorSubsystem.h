// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "SyncEvents.h"
#include "DefinitionGeneratorEditorSubsystem.generated.h"

class IDirectoryWatcher;

/**
 * Validation result for a single definition.
 */
USTRUCT(BlueprintType)
struct FDefinitionValidationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString TypeName;

	UPROPERTY(BlueprintReadOnly)
	FString AssetId;

	UPROPERTY(BlueprintReadOnly)
	FString JsonPath;

	UPROPERTY(BlueprintReadOnly)
	bool bNeedsRegeneration = false;

	UPROPERTY(BlueprintReadOnly)
	FString Reason;
};

/**
 * Universal editor subsystem for definition generation.
 *
 * Handles:
 * - Startup validation for all registered types
 * - Directory watching for JSON changes
 * - Out-of-date notifications with Regenerate/Dismiss actions
 * - Menu action implementations
 */
UCLASS()
class PROJECTDEFINITIONGENERATOR_API UDefinitionGeneratorEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin UEditorSubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End UEditorSubsystem Interface

	/**
	 * Validate all definitions for a type.
	 * @param TypeName The registered type name
	 * @return Array of items that need regeneration.
	 */
	UFUNCTION(BlueprintCallable, Category = "DefinitionGenerator")
	TArray<FDefinitionValidationResult> ValidateType(const FString& TypeName);

	/**
	 * Validate all registered definition types.
	 * @return Array of all items that need regeneration.
	 */
	UFUNCTION(BlueprintCallable, Category = "DefinitionGenerator")
	TArray<FDefinitionValidationResult> ValidateAll();

	/**
	 * Regenerate only changed definitions for a type.
	 * @param TypeName The registered type name
	 * @return Number of items regenerated.
	 */
	UFUNCTION(BlueprintCallable, Category = "DefinitionGenerator")
	int32 RegenerateChanged(const FString& TypeName);

	/**
	 * Force regenerate all definitions for a type.
	 * @param TypeName The registered type name
	 * @return Number of items regenerated.
	 */
	UFUNCTION(BlueprintCallable, Category = "DefinitionGenerator")
	int32 RegenerateAll(const FString& TypeName);

	/**
	 * Delete orphaned assets for a type.
	 * @param TypeName The registered type name
	 * @return Number of orphaned assets deleted.
	 */
	UFUNCTION(BlueprintCallable, Category = "DefinitionGenerator")
	int32 CleanupOrphans(const FString& TypeName);

	/**
	 * Open the SOT folder for a type in file explorer.
	 * @param TypeName The registered type name
	 */
	UFUNCTION(BlueprintCallable, Category = "DefinitionGenerator")
	void OpenSourceFolder(const FString& TypeName);

	/**
	 * Get number of out-of-date items for a type.
	 */
	UFUNCTION(BlueprintPure, Category = "DefinitionGenerator")
	int32 GetOutOfDateCount(const FString& TypeName) const;

	/**
	 * Get total out-of-date items across all types.
	 */
	UFUNCTION(BlueprintPure, Category = "DefinitionGenerator")
	int32 GetTotalOutOfDateCount() const;

private:
	/** Called after engine init */
	void OnPostEngineInit();

	/** Called when AssetRegistry finishes initial scan */
	void OnAssetRegistryFilesLoaded();

	/** Run startup validation for all types */
	void RunStartupValidation();

	/** Setup directory watchers for all registered types */
	void SetupDirectoryWatchers();

	/** Called when files change in a watched directory */
	void OnDirectoryChanged(const TArray<struct FFileChangeData>& FileChanges);

	/** Called when a sync batch modifies JSON files */
	void OnSyncBatchApplied(FName Origin, const TArray<FSyncBatchFile>& ChangedFiles);

	/** Process pending dirty files (debounced) */
	void ProcessDirtyFiles();

	/** Show notification about out-of-date items (startup) */
	void ShowOutOfDateNotification();

	/** Show notification about specific changed files */
	void ShowChangedFilesNotification(int32 FileCount, const FString& FileList);

	/** Handle "Regenerate All" button in startup notification */
	void OnRegenerateClicked();

	/** Handle "Regenerate" button for changed files notification */
	void OnRegenerateChangedFilesClicked();

	/** Handle "Dismiss" button in notification */
	void OnDismissClicked();

	/** Regenerate only the specific files that changed */
	int32 RegenerateSpecificFiles(const TArray<FString>& FilePaths);

	/** Out of date count per type */
	TMap<FString, int32> OutOfDateCounts;

	/** Directory watcher handles per source path */
	TMap<FString, FDelegateHandle> DirectoryWatcherHandles;

	/** Files marked dirty by directory watcher */
	TSet<FString> DirtyFiles;

	/** Sync batch ignore entry */
	struct FSyncIgnoredFile
	{
		FName Origin;
		FString ContentHash;
	};

	/** Files to ignore (sync batches) */
	TMap<FString, FSyncIgnoredFile> IgnoredFiles;

	/** Files pending regeneration (from changed files notification) */
	TArray<FString> PendingRegenerationFiles;

	/** Timer handle for debounced processing */
	FTimerHandle DebounceTimerHandle;

	/** Whether we've shown the startup notification */
	bool bStartupNotificationShown = false;

	/** User dismissed notification - don't re-show until JSON files change */
	bool bDismissedUntilChange = false;

	/** Active notification (for closing after action) */
	TWeakPtr<class SNotificationItem> ActiveNotification;

	/** Sync batch delegate handle */
	FDelegateHandle SyncBatchHandle;
};
