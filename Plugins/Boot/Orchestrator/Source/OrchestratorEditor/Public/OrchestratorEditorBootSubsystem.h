// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "OrchestratorEditorBootSubsystem.generated.h"

/**
 * Editor subsystem that loads external plugins at PIE start.
 * Scans shared and local external plugin roots for .uplugin files and loads
 * their modules.
 */
UCLASS()
class ORCHESTRATOREDITOR_API UOrchestratorEditorBootSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem implementation
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	/** Called after engine init completes (AssetManager guaranteed ready). */
	void OnPostEngineInit();

	/**
	 * Called when PIE is about to begin.
	 * Scans and loads external plugins.
	 */
	void OnBeginPIE(bool bIsSimulating);

	/**
	 * Scan directory recursively for .uplugin files.
	 *
	 * @param DirectoryPath Absolute path to directory to scan
	 * @param OutUpluginFiles Array to fill with absolute paths to .uplugin files
	 */
	void ScanForUpluginFiles(const FString& DirectoryPath, TArray<FString>& OutUpluginFiles);

	/**
	 * Load all plugins from the supported external plugin directories.
	 */
	void LoadExternalPlugins();

	/**
	 * Preload heavy assets (pawn class + deps) in background.
	 * Called at editor startup so deps are warm when PIE starts.
	 */
	void PreloadPawnClass();

	/** Handle keeping the async preload alive */
	TSharedPtr<struct FStreamableHandle> PawnPreloadHandle;

	/** True while pawn preload is in progress - disables CPU throttle so loading runs at full speed in background */
	bool bPawnPreloadInProgress = false;
};
