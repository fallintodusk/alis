// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "RedirectorAutoFixSubsystem.generated.h"

struct FAssetData;

/**
 * Automatically fixes redirectors created during asset renames.
 *
 * When assets are renamed, UE creates redirectors at the old path to maintain references.
 * This subsystem automatically fixes those references and deletes the redirectors,
 * keeping the content browser clean.
 *
 * Safe for workflows where:
 * - World Partition (isolated actors per file)
 * - Binary merge strategy (always take incoming)
 * - Frequent syncs (no long-lived feature branches)
 * - Data-driven architecture (most refs via JSON)
 *
 * Can be disabled via project settings if team has complex branching workflow.
 */
UCLASS()
class PROJECTASSETSYNC_API URedirectorAutoFixSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	/** Manually trigger redirector fixup for pending renames (for testing) */
	UFUNCTION(BlueprintCallable, Category = "AssetSync|RedirectorFix")
	void FixRedirectorsNow();

	/** Check if auto-fix is enabled in project settings */
	UFUNCTION(BlueprintPure, Category = "AssetSync|RedirectorFix")
	bool IsAutoFixEnabled() const;

private:
	/** Called when an asset is renamed in the editor */
	void OnAssetRenamed(const FAssetData& NewAssetData, const FString& OldObjectPath);

	/** Process all pending renames after debounce timer */
	void ProcessPendingRenames();

	/** Start or reset the debounce timer */
	void StartDebounceTimer();

	/**
	 * Fix redirectors for the given rename mappings.
	 * Uses IAssetTools::FixupReferencers() to handle all the complexity.
	 */
	void FixRedirectors(const TMap<FString, FString>& RenameMappings);

	/** Show notification popup with results */
	void ShowResultsNotification(int32 FixedCount, int32 SavedCount, const TArray<FString>& FixedRedirectors);

	/** Pending renames: OldPath -> NewPath */
	TMap<FString, FString> PendingRenames;

	/** Debounce timer handle */
	FTimerHandle DebounceTimerHandle;

	/**
	 * Debounce delay in seconds (batch folder moves).
	 * Slightly longer than JsonRefSync (0.3s) to run after JSON fixes complete.
	 */
	static constexpr float DebounceDelay = 0.5f;
};
