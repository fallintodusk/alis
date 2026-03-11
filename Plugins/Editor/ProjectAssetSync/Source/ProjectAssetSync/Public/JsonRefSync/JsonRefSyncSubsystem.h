// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "JsonRefSyncSubsystem.generated.h"

/**
 * Syncs JSON asset references when UE assets are renamed/moved.
 *
 * Direction: UE Asset -> JSON
 * Trigger: OnAssetRenamed delegate
 *
 * When an asset is renamed in the editor, this subsystem finds all JSON files
 * that reference the old path and updates them to the new path.
 */
UCLASS()
class PROJECTASSETSYNC_API UJsonRefSyncSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	/** Manually trigger JSON ref fix for pending renames (for testing) */
	UFUNCTION(BlueprintCallable, Category = "AssetSync|JsonRef")
	void ProcessPendingRenamesNow();

	/** Get JSON root directories to scan */
	UFUNCTION(BlueprintPure, Category = "AssetSync|JsonRef")
	TArray<FString> GetJsonRootDirectories() const;

private:
	/** Called when an asset is renamed in the editor */
	void OnAssetRenamed(const FAssetData& NewAssetData, const FString& OldObjectPath);

	/** Process all pending renames after debounce timer */
	void ProcessPendingRenames();

	/** Start or reset the debounce timer */
	void StartDebounceTimer();

	/** Pending renames: OldPath -> NewPath */
	TMap<FString, FString> PendingRenames;

	/** Debounce timer handle */
	FTimerHandle DebounceTimerHandle;

	/** Debounce delay in seconds (batch folder moves) */
	static constexpr float DebounceDelay = 0.3f;
};
