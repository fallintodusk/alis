// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "OrchestratorManifest.h"
#include "OrchestratorState.h"
#include "Interfaces/IOrchestratorRegistry.h"

// Log category for Orchestrator module (used across all Orchestrator files)
DECLARE_LOG_CATEGORY_EXTERN(LogOrchestrator, Log, All);

/**
 * Launcher IPC context passed to game client.
 * Read from named pipe (--ipc-pipe argument) or environment.
 */
struct FLauncherContext
{
	/** Auth token from OAuth flow */
	FString AuthToken;

	/** Path to verified manifest.json (already downloaded/verified by Launcher) */
	FString ManifestPath;

	/** Installation root (e.g., <install-root>) */
	FString InstallPath;

	/** Engine build identifier - must match manifest.engine_build_id */
	FString EngineBuildId;
};

/**
 * Boot progress update delegate.
 * @param CurrentPlugin Name of plugin currently being loaded
 * @param LoadedCount Number of Boot plugins loaded so far
 * @param TotalBootPlugins Total number of Boot plugins to load
 * @param StatusMessage Human-readable status message
 */
DECLARE_MULTICAST_DELEGATE_FourParams(FOnBootProgressUpdate, const FString& /*CurrentPlugin*/, int32 /*LoadedCount*/, int32 /*TotalBootPlugins*/, const FString& /*StatusMessage*/);

/**
 * Boot complete delegate (all Boot plugins loaded successfully).
 */
DECLARE_MULTICAST_DELEGATE(FOnBootComplete);

/**
 * Orchestrator module implementation.
 * Base plugin loaded early - reads Launcher IPC context and manages plugin lifecycle.
 */
class FOrchestratorCoreModule : public IModuleInterface, public IOrchestratorRegistry
{
public:
	// IModuleInterface implementation
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Event broadcasted as Boot plugins load (updated for each plugin) */
	static FOnBootProgressUpdate OnBootProgressUpdate;

	/** Event broadcasted when all Boot plugins finish loading */
	static FOnBootComplete OnBootComplete;

	// IOrchestratorRegistry implementation
	virtual bool IsFeatureAvailable(FName PluginName) const override;
	virtual TArray<FName> GetLoadedFeatures() const override;
	virtual FString GetFeatureVersion(FName PluginName) const override;

private:
	/** Launcher context (read from IPC) */
	FLauncherContext LauncherContext;

	/** Flag to track if initialization completed */
	bool bInitialized = false;

	/** Current manifest (loaded from disk by Orchestrator) */
	FOrchestratorManifest CurrentManifest;

	/** Latest applied state */
	FOrchestratorState LatestState;

	/** Last known good state (for rollback) */
	FOrchestratorState LastGoodState;

	/** Pending updates staged for next restart */
	FPendingUpdates PendingUpdates;

	/** Resolved activation order (topologically sorted) */
	TArray<FString> ActivationOrder;

	/** Plugins needing hot updates (content-only, no restart) */
	TArray<FString> HotUpdatePlugins;

	/** Plugins needing cold updates (code changed, requires restart) */
	TArray<FString> ColdUpdatePlugins;

	/** New plugins not in latest state */
	TArray<FString> NewPlugins;

	/*
	 * Boot-time Slate widget intentionally disabled (2025-11-23).
	 *
	 * Orchestrator runs during PostConfigInit (before Slate, before GEngine).
	 * Boot completes before PostEngineInit (~0.4s vs ~23s), so a boot widget
	 * would always appear after boot finishes - too late to be useful.
	 *
	 * Boot is fast (<1 second) - engine splash screen provides adequate feedback.
	 * Content loading UI is handled by ProjectLoading after engine initialization.
	 *
	 * See: Plugins/Boot/Orchestrator/README.md (Boot UI Strategy)
	 *      Plugins/Systems/ProjectLoading/README.md (Loading Screen UI)
	 */

	/** Slate boot widget (DISABLED - see comment above) */
	// TSharedPtr<class SOrchestratorBootWidget> BootWidget;

	/** Slate window containing boot widget (DISABLED - see comment above) */
	// TSharedPtr<SWindow> BootWindow;

	/** Pending boot info (DISABLED - cached for widget that no longer exists) */
	// bool bBootInProgress = false;
	// int32 TotalBootPlugins = 0;
	// int32 CurrentBootIndex = 0;
	// FString CurrentPluginName;

	/** Read Launcher IPC context (from named pipe or command line args) */
	bool ReadLauncherContext();

	/** Initialize Orchestrator workflow (called from StartupModule) */
	void Initialize();

	/** Apply pending cold updates from previous session (restart cycle) */
	void ApplyPendingUpdates();

	/** Parse and apply updates from manifest */
	void ProcessManifest();

	/** Resolve dependencies and compute activation order */
	void ResolveDependencies();

	/** Mount required engine plugins before loading features */
	void MountRequiredEnginePlugins();

	/** Create Slate boot widget (DISABLED - see member variable comment above) */
	// void CreateBootWidget();

	/** Apply hot path updates (content-only, no restart) */
	void ApplyHotUpdates();

	/** Stage cold path updates (code changes, requires restart) */
	void StageColdUpdates();

	/** Load feature modules in dependency order (replaces GameFeatures activation) */
	void LoadFeatureModules();

	/** Update latest.json with newly installed/activated plugin */
	bool UpdateLatestState(const FString& PluginName, const FOrchestratorPluginEntry& ManifestEntry, const FString& InstalledPath);

	/** Save current latest.json to last_good.json (after successful activation) */
	bool SaveLastGoodState();

	/** Rollback to last_good.json on failure */
	bool RollbackToLastGood();

	/** Helper: Get LocalRoot path (derived from InstallPath) */
	FString GetLocalRoot() const;

	/** Helper: Get path to state files */
	FString GetLatestStatePath() const;
	FString GetLastGoodStatePath() const;

	/**
	 * Check if two engine build IDs are compatible.
	 * Allows source-built and binary releases of the same major.minor version.
	 *
	 * Examples of compatible pairs:
	 * - "++UE5+Release-5.5-CL-40574608" and "UE5-CL-0" (both 5.5)
	 * - "++UE5+Release-5.5-CL-40574608" and "++UE5+Release-5.5-CL-12345678"
	 *
	 * @param ManifestBuildId The engine_build_id from manifest
	 * @param CurrentBuildId The current engine's build ID (from FApp::GetBuildVersion)
	 * @return true if compatible, false otherwise
	 */
	static bool IsCompatibleEngineBuildId(const FString& ManifestBuildId, const FString& CurrentBuildId);
};
