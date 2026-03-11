// Copyright ALIS. All Rights Reserved.

#include "OrchestratorCoreModule.h"
#include "OrchestratorManifest.h"
#include "OrchestratorState.h"
#include "OrchestratorDownload.h"
#include "OrchestratorExtract.h"
#include "OrchestratorHash.h"
#include "OrchestratorPluginRegistry.h"
#include "ProjectPaths.h"
#include "Interfaces/IProjectFeatureModule.h"
#include "Interfaces/IFeatureModuleRegistry.h"
// Boot widget includes disabled (2025-11-23) - see header comment for architectural reasoning
// #include "UI/SOrchestratorBootWidget.h"
#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Interfaces/IPluginManager.h"
// Slate includes disabled (no longer needed without boot widget)
// #include "Framework/Application/SlateApplication.h"
// #include "Widgets/SWindow.h"
// #include "Misc/CoreDelegates.h"

DEFINE_LOG_CATEGORY(LogOrchestrator);

// Static boot progress delegates definition
FOnBootProgressUpdate FOrchestratorCoreModule::OnBootProgressUpdate;
FOnBootComplete FOrchestratorCoreModule::OnBootComplete;

void FOrchestratorCoreModule::StartupModule()
{
	UE_LOG(LogOrchestrator, Log, TEXT("=== Orchestrator Module Loading ==="));

	// Skip Orchestrator entirely during commandlet execution (build/cook processes)
	// Orchestrator is a runtime-only system for managing plugin updates and loading
	if (IsRunningCommandlet())
	{
		UE_LOG(LogOrchestrator, Log, TEXT("Skipping Orchestrator initialization - running commandlet (build/cook)"));
		return;
	}

	// DESIGN NOTE: Orchestrator runs in editor even though editor already loads plugins
	// Reasons to keep it active:
	// - Consistent logging between editor and shipping (helps debugging plugin load order)
	// - Tests orchestration workflow during development (manifest processing, dependency resolution)
	// - Updates latest.json in dev environment (useful for testing state persistence)
	// - Harmless redundancy: LoadFeatureModules() detects already-loaded plugins and skips (line 923-970)

	// Register this module as the global registry implementation (SOLID - dependency injection)
	SetOrchestratorRegistry(this);

	// Read Launcher IPC context
	if (!ReadLauncherContext())
	{
		UE_LOG(LogOrchestrator, Error, TEXT("Failed to read Launcher IPC context - skipping initialization"));
		return;
	}

	// Initialize Orchestrator workflow (replaces old BootROM.Start() pattern)
	Initialize();
}

/*
 * CreateBootWidget() - DISABLED (2025-11-23)
 *
 * Boot-time Slate widget removed for architectural reasons:
 * - Orchestrator runs during PostConfigInit (before Slate exists, before GEngine)
 * - Boot completes at ~0.4s, Slate initializes at ~23s (PostEngineInit)
 * - Widget would always appear AFTER boot finishes - too late to show progress
 * - Boot is fast (<1 second) - engine splash screen provides adequate feedback
 * - Content loading UI handled by ProjectLoading after engine initialization
 *
 * References:
 * - Plugins/Boot/Orchestrator/README.md (Boot UI Strategy)
 * - Plugins/Systems/ProjectLoading/README.md (Loading Screen UI)
 * - Epic's Lyra sample: No boot UI during plugin loading phase
 */
/*
void FOrchestratorCoreModule::CreateBootWidget()
{
	// This is called via PostEngineInit callback when Slate is guaranteed to be ready
	// NOTE: Boot may have already completed by the time this fires (PostConfigInit -> LoadPlugins -> PostEngineInit)
	// That's OK - we'll show a brief "Boot Complete" message then close the widget

	if (!FSlateApplication::IsInitialized())
	{
		UE_LOG(LogOrchestrator, Error, TEXT("  [CreateBootWidget] Slate STILL not initialized at PostEngineInit! Cannot create widget."));
		return;
	}

	// Check if we have boot info to display (even if boot is already complete)
	if (TotalBootPlugins == 0)
	{
		UE_LOG(LogOrchestrator, Warning, TEXT("  [CreateBootWidget] No boot plugins loaded - skipping widget creation"));
		return;
	}

	const bool bBootAlreadyComplete = !bBootInProgress;
	UE_LOG(LogOrchestrator, Log, TEXT("  [CreateBootWidget] Slate is ready, creating boot widget... (Boot status: %s)"),
		bBootAlreadyComplete ? TEXT("COMPLETE") : TEXT("IN PROGRESS"));

	// Create Slate widget
	BootWidget = SNew(SOrchestratorBootWidget);

	// Create window to contain widget
	BootWindow = SNew(SWindow)
		.Title(FText::FromString(TEXT("Orchestrator Boot")))
		.ClientSize(FVector2D(600, 300))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.HasCloseButton(false)
		.IsTopmostWindow(true)
		.SizingRule(ESizingRule::FixedSize)
		[
			BootWidget.ToSharedRef()
		];

	// Add window to Slate application
	FSlateApplication::Get().AddWindow(BootWindow.ToSharedRef(), true);

	// Initialize widget with current progress
	if (BootWidget.IsValid())
	{
		if (bBootAlreadyComplete)
		{
			// Boot already finished - show completion message
			BootWidget->UpdateProgress(TEXT("Boot Complete"), TotalBootPlugins, TotalBootPlugins,
				FString::Printf(TEXT("Loaded %d plugins successfully"), TotalBootPlugins));
		}
		else
		{
			// Boot still in progress - show current plugin
			BootWidget->UpdateProgress(CurrentPluginName, CurrentBootIndex, TotalBootPlugins,
				FString::Printf(TEXT("Loading %s..."), *CurrentPluginName));
		}
	}

	UE_LOG(LogOrchestrator, Log, TEXT("  [SUCCESS] Created Slate boot widget and window (Total plugins: %d, Current: %d, Status: %s)"),
		TotalBootPlugins, CurrentBootIndex, bBootAlreadyComplete ? TEXT("COMPLETE") : TEXT("IN PROGRESS"));

	// If boot is already complete, close the widget after a brief delay
	if (bBootAlreadyComplete && BootWindow.IsValid())
	{
		UE_LOG(LogOrchestrator, Log, TEXT("  [INFO] Boot already complete - closing widget after brief display"));

		// Close widget immediately since user won't see it anyway (PostEngineInit happens after rendering starts)
		FSlateApplication::Get().RequestDestroyWindow(BootWindow.ToSharedRef());
		BootWindow.Reset();
		BootWidget.Reset();
		UE_LOG(LogOrchestrator, Log, TEXT("  [CLOSED] Boot widget closed (boot was already complete)"));
	}
}
*/

void FOrchestratorCoreModule::ShutdownModule()
{
	UE_LOG(LogOrchestrator, Log, TEXT("Orchestrator module shutdown"));
	// Unregister the global registry
	SetOrchestratorRegistry(nullptr);
	bInitialized = false;
}

bool FOrchestratorCoreModule::ReadLauncherContext()
{
	UE_LOG(LogOrchestrator, Log, TEXT("Reading Launcher IPC context..."));

	// TODO: Implement actual IPC reading from named pipe
	// For now, use fallback paths for development

	// Dev mode fallback: Use local appdata paths
	LauncherContext.InstallPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	LauncherContext.ManifestPath = FProjectPaths::GetPluginDataDir(TEXT("Orchestrator")) / TEXT("dev_manifest.json");
	LauncherContext.EngineBuildId = FApp::GetBuildVersion();
	LauncherContext.AuthToken = TEXT(""); // Empty in dev mode

	UE_LOG(LogOrchestrator, Log, TEXT("  InstallPath: %s"), *LauncherContext.InstallPath);
	UE_LOG(LogOrchestrator, Log, TEXT("  ManifestPath: %s"), *LauncherContext.ManifestPath);
	UE_LOG(LogOrchestrator, Log, TEXT("  EngineBuildId: %s"), *LauncherContext.EngineBuildId);

	return true;
}

void FOrchestratorCoreModule::Initialize()
{
	UE_LOG(LogOrchestrator, Log, TEXT("=== Orchestrator Initialize (Launcher-driven boot) ==="));
	UE_LOG(LogOrchestrator, Log, TEXT("  InstallPath: %s"), *LauncherContext.InstallPath);
	UE_LOG(LogOrchestrator, Log, TEXT("  EngineBuildId: %s"), *LauncherContext.EngineBuildId);

	bInitialized = true;

	// Apply any pending updates from previous restart cycle
	ApplyPendingUpdates();

	// Process manifest and updates
	ProcessManifest();
	ResolveDependencies();
	MountRequiredEnginePlugins();
	ApplyHotUpdates();
	StageColdUpdates();
	LoadFeatureModules();

	UE_LOG(LogOrchestrator, Log, TEXT("Orchestrator initialization complete"));
}

void FOrchestratorCoreModule::ApplyPendingUpdates()
{
	UE_LOG(LogOrchestrator, Log, TEXT("Orchestrator: Checking for pending updates..."));

	const FString PendingUpdatesPath = FPaths::Combine(GetLocalRoot(), TEXT("State/pending_updates.json"));
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Check if pending_updates.json exists
	if (!PlatformFile.FileExists(*PendingUpdatesPath))
	{
		UE_LOG(LogOrchestrator, Log, TEXT("  No pending updates file found"));
		return;
	}

	// Load pending updates
	FPendingUpdates PendingList;
	if (!FPendingUpdates::LoadFromFile(PendingUpdatesPath, PendingList))
	{
		UE_LOG(LogOrchestrator, Error, TEXT("  Failed to load pending_updates.json"));
		return;
	}

	if (PendingList.Updates.Num() == 0)
	{
		UE_LOG(LogOrchestrator, Warning, TEXT("  pending_updates.json is empty"));
		// Delete empty file
		PlatformFile.DeleteFile(*PendingUpdatesPath);
		return;
	}

	UE_LOG(LogOrchestrator, Log, TEXT("  Found %d pending cold updates from previous session"), PendingList.Updates.Num());
	UE_LOG(LogOrchestrator, Log, TEXT("  Manifest version: %s"), *PendingList.ManifestVersion);
	UE_LOG(LogOrchestrator, Log, TEXT("  Created at: %s"), *PendingList.CreatedAt);

	int32 AppliedCount = 0;
	int32 FailedCount = 0;

	for (const FPendingUpdate& Update : PendingList.Updates)
	{
		UE_LOG(LogOrchestrator, Log, TEXT("  Applying: %s v%s -> v%s"),
			*Update.Name, *Update.FromVersion, *Update.ToVersion);
		UE_LOG(LogOrchestrator, Log, TEXT("    Staged path: %s"), *Update.StagedPath);

		// Check if staged files exist
		if (!PlatformFile.DirectoryExists(*Update.StagedPath))
		{
			UE_LOG(LogOrchestrator, Error, TEXT("    Staged directory not found: %s"), *Update.StagedPath);
			FailedCount++;
			continue;
		}

		// 1. Verify .uplugin file exists in staged location
		TArray<FString> UpluginFiles;
		IFileManager::Get().FindFiles(UpluginFiles, *(Update.StagedPath / TEXT("*.uplugin")), true, false);

		if (UpluginFiles.Num() == 0)
		{
			UE_LOG(LogOrchestrator, Error, TEXT("    No .uplugin file found in staged directory"));
			FailedCount++;
			continue;
		}

		if (UpluginFiles.Num() > 1)
		{
			UE_LOG(LogOrchestrator, Warning, TEXT("    Multiple .uplugin files found, using first: %s"), *UpluginFiles[0]);
		}

		const FString UpluginFilePath = Update.StagedPath / UpluginFiles[0];
		UE_LOG(LogOrchestrator, Log, TEXT("    Found .uplugin: %s"), *UpluginFiles[0]);

		// 2. Verify plugin name matches
		FString PluginNameFromFile;
		if (!FOrchestratorPluginRegistry::VerifyUpluginFile(UpluginFilePath, PluginNameFromFile))
		{
			UE_LOG(LogOrchestrator, Error, TEXT("    Invalid .uplugin file"));
			FailedCount++;
			continue;
		}

		if (PluginNameFromFile != Update.Name)
		{
			UE_LOG(LogOrchestrator, Error, TEXT("    Plugin name mismatch: expected %s, found %s"),
				*Update.Name, *PluginNameFromFile);
			FailedCount++;
			continue;
		}

		// 3. Register plugin with IPluginManager
		UE_LOG(LogOrchestrator, Log, TEXT("    Registering plugin with engine..."));
		FPluginRegistrationResult RegResult = FOrchestratorPluginRegistry::RegisterPlugin(UpluginFilePath);

		if (!RegResult.bSuccess)
		{
			UE_LOG(LogOrchestrator, Error, TEXT("    Failed to register plugin: %s"), *RegResult.ErrorMessage);
			FailedCount++;
			continue;
		}

		UE_LOG(LogOrchestrator, Log, TEXT("    [OK] Plugin registered: %s"), *RegResult.PluginName);
		UE_LOG(LogOrchestrator, Log, TEXT("    [OK] Plugin path: %s"), *RegResult.PluginPath);

		// 4. Find manifest entry for this plugin to get hashes
		const FOrchestratorPluginEntry* ManifestEntry = CurrentManifest.FindPlugin(Update.Name);
		if (!ManifestEntry)
		{
			UE_LOG(LogOrchestrator, Warning, TEXT("    Plugin not in current manifest, skipping state update"));
			AppliedCount++;
			continue;
		}

		// 5. Update latest.json with new version
		if (UpdateLatestState(Update.Name, *ManifestEntry, RegResult.PluginPath))
		{
			UE_LOG(LogOrchestrator, Log, TEXT("    [OK] Updated latest.json"));
		}
		else
		{
			UE_LOG(LogOrchestrator, Warning, TEXT("    Failed to update latest.json"));
		}

		AppliedCount++;
	}

	UE_LOG(LogOrchestrator, Log, TEXT("Pending updates processing complete:"));
	UE_LOG(LogOrchestrator, Log, TEXT("  Applied: %d, Failed: %d"), AppliedCount, FailedCount);

	// Clear pending_updates.json if all succeeded
	if (FailedCount == 0)
	{
		if (PlatformFile.DeleteFile(*PendingUpdatesPath))
		{
			UE_LOG(LogOrchestrator, Log, TEXT("  Cleared pending_updates.json"));
		}
		else
		{
			UE_LOG(LogOrchestrator, Warning, TEXT("  Failed to clear pending_updates.json"));
		}
	}
	else
	{
		UE_LOG(LogOrchestrator, Warning, TEXT("  Keeping pending_updates.json due to %d failures"), FailedCount);
	}
}

void FOrchestratorCoreModule::ProcessManifest()
{
	UE_LOG(LogOrchestrator, Log, TEXT("Orchestrator: Processing manifest..."));

	const FString StatePath = FPaths::Combine(GetLocalRoot(), TEXT("State"));
	const FString LatestStatePath = FPaths::Combine(StatePath, TEXT("latest.json"));
	const FString LastGoodStatePath = FPaths::Combine(StatePath, TEXT("last_good.json"));
	const FString PendingUpdatesPath = FPaths::Combine(StatePath, TEXT("pending_updates.json"));

	// Load state files (non-critical if missing - first boot or dev mode)
	if (FOrchestratorState::LoadFromFile(LatestStatePath, LatestState))
	{
		UE_LOG(LogOrchestrator, Log, TEXT("Orchestrator: Loaded latest state (manifest v%s, %d plugins)"),
			*LatestState.ManifestVersion, LatestState.Plugins.Num());
	}
	else
	{
		UE_LOG(LogOrchestrator, Warning, TEXT("Orchestrator: No latest.json found - first boot or dev mode"));
	}

	if (FOrchestratorState::LoadFromFile(LastGoodStatePath, LastGoodState))
	{
		UE_LOG(LogOrchestrator, Log, TEXT("Orchestrator: Loaded last_good state (manifest v%s, %d plugins)"),
			*LastGoodState.ManifestVersion, LastGoodState.Plugins.Num());
	}

	if (FPendingUpdates::LoadFromFile(PendingUpdatesPath, PendingUpdates))
	{
		UE_LOG(LogOrchestrator, Log, TEXT("Orchestrator: Found %d pending updates from previous run"),
			PendingUpdates.Updates.Num());
	}

	// Load manifest from disk (Orchestrator decides location, not BootROM)
	FString ManifestPath;
#if UE_BUILD_SHIPPING
	// Production: Load from OTA manifest directory
	ManifestPath = FPaths::Combine(GetLocalRoot(), TEXT("Manifests/latest.json"));
#else
	// Dev mode: Load from centralized schema path
	ManifestPath = FProjectPaths::GetPluginDataDir(TEXT("Orchestrator")) / TEXT("dev_manifest.json");
#endif

	UE_LOG(LogOrchestrator, Log, TEXT("Orchestrator: Loading manifest from: %s"), *ManifestPath);

	// Read manifest file
	FString ManifestJson;
	if (!FFileHelper::LoadFileToString(ManifestJson, *ManifestPath))
	{
		UE_LOG(LogOrchestrator, Warning, TEXT("Orchestrator: Failed to read manifest file: %s"), *ManifestPath);
		UE_LOG(LogOrchestrator, Warning, TEXT("  Will use built-in plugins only (dev mode)"));
		return;
	}

	// Parse manifest JSON
	if (!FOrchestratorManifest::LoadFromString(ManifestJson, CurrentManifest))
	{
		UE_LOG(LogOrchestrator, Error, TEXT("Orchestrator: Failed to parse manifest JSON from: %s"), *ManifestPath);
		UE_LOG(LogOrchestrator, Error, TEXT("  Cannot proceed with updates"));
		return;
	}

	UE_LOG(LogOrchestrator, Log, TEXT("Orchestrator: Parsed manifest (v%s, engine %s, %d plugins)"),
		*CurrentManifest.ManifestVersion, *CurrentManifest.EngineBuildId, CurrentManifest.Plugins.Num());

	// Validate engine compatibility (allows source-built and binary releases of same major version)
	if (!IsCompatibleEngineBuildId(CurrentManifest.EngineBuildId, LauncherContext.EngineBuildId))
	{
		UE_LOG(LogOrchestrator, Error, TEXT("Orchestrator: CRITICAL - engine_build_id incompatible!"));
		UE_LOG(LogOrchestrator, Error, TEXT("  Manifest requires: %s"), *CurrentManifest.EngineBuildId);
		UE_LOG(LogOrchestrator, Error, TEXT("  Current engine: %s"), *LauncherContext.EngineBuildId);
		UE_LOG(LogOrchestrator, Error, TEXT("  Rejecting manifest and keeping current state"));
		UE_LOG(LogOrchestrator, Error, TEXT("  This prevents incompatible plugin versions from loading"));
		return;
	}

	UE_LOG(LogOrchestrator, Log, TEXT("  [OK] engine_build_id compatible: manifest=%s, current=%s"),
		*CurrentManifest.EngineBuildId, *LauncherContext.EngineBuildId);

	// Compare manifest with latest state and classify updates
	int32 UnchangedPlugins = 0;

	// Clear classification arrays
	NewPlugins.Empty();
	HotUpdatePlugins.Empty();
	ColdUpdatePlugins.Empty();

	for (const FOrchestratorPluginEntry& ManifestEntry : CurrentManifest.Plugins)
	{
		const FPluginState* InstalledState = LatestState.Plugins.Find(ManifestEntry.Name);

		if (!InstalledState)
		{
			// New plugin not in latest state - treat as cold update (needs full install)
			NewPlugins.Add(ManifestEntry.Name);
			UE_LOG(LogOrchestrator, Log, TEXT("  NEW: %s v%s"), *ManifestEntry.Name, *ManifestEntry.Version);
			continue;
		}

		// Apply decision rule: code_hash changed -> cold, content_hash changed -> hot
		const bool bCodeChanged = (InstalledState->CodeHash != ManifestEntry.CodeHash);
		const bool bContentChanged = (InstalledState->ContentHash != ManifestEntry.ContentHash);

		if (bCodeChanged)
		{
			// Code changed -> COLD update (requires restart)
			ColdUpdatePlugins.Add(ManifestEntry.Name);
			UE_LOG(LogOrchestrator, Log, TEXT("  COLD: %s v%s -> v%s (code changed)"),
				*ManifestEntry.Name,
				*InstalledState->Version,
				*ManifestEntry.Version);
		}
		else if (bContentChanged)
		{
			// Content changed, code same -> HOT update (no restart)
			HotUpdatePlugins.Add(ManifestEntry.Name);
			UE_LOG(LogOrchestrator, Log, TEXT("  HOT: %s v%s -> v%s (content changed, code same)"),
				*ManifestEntry.Name,
				*InstalledState->Version,
				*ManifestEntry.Version);
		}
		else
		{
			// Both hashes match -> up-to-date
			UnchangedPlugins++;
		}
	}

	UE_LOG(LogOrchestrator, Log, TEXT("Orchestrator: Comparison complete"));
	UE_LOG(LogOrchestrator, Log, TEXT("  New: %d, Hot: %d, Cold: %d, Unchanged: %d"),
		NewPlugins.Num(), HotUpdatePlugins.Num(), ColdUpdatePlugins.Num(), UnchangedPlugins);

	if (NewPlugins.Num() == 0 && HotUpdatePlugins.Num() == 0 && ColdUpdatePlugins.Num() == 0)
	{
		UE_LOG(LogOrchestrator, Log, TEXT("Orchestrator: All plugins up-to-date"));
	}
}

void FOrchestratorCoreModule::ResolveDependencies()
{
	UE_LOG(LogOrchestrator, Log, TEXT("Orchestrator: Resolving dependencies..."));

	// Build dependency graph
	TMap<FString, TSet<FString>> DependencyGraph;  // Plugin -> Set of dependencies
	TMap<FString, int32> InDegree;                 // Plugin -> Number of plugins that depend on it

	// Initialize graph
	for (const FOrchestratorPluginEntry& Plugin : CurrentManifest.Plugins)
	{
		DependencyGraph.Add(Plugin.Name, TSet<FString>());
		InDegree.Add(Plugin.Name, 0);

		// Add dependencies
		for (const FPluginDependency& Dep : Plugin.DependsOn)
		{
			DependencyGraph[Plugin.Name].Add(Dep.Name);
		}
	}

	// Calculate in-degree (how many plugins depend on each plugin)
	for (const FOrchestratorPluginEntry& Plugin : CurrentManifest.Plugins)
	{
		for (const FPluginDependency& Dep : Plugin.DependsOn)
		{
			if (InDegree.Contains(Dep.Name))
			{
				InDegree[Dep.Name]++;
			}
			else
			{
				UE_LOG(LogOrchestrator, Warning, TEXT("  Plugin '%s' depends on '%s' which is not in manifest"),
					*Plugin.Name, *Dep.Name);
			}
		}
	}

	// Topological sort using Kahn's algorithm
	TArray<FString> Sorted;
	TArray<FString> Queue;

	// Start with plugins that have no dependencies
	for (const FOrchestratorPluginEntry& Plugin : CurrentManifest.Plugins)
	{
		if (DependencyGraph[Plugin.Name].Num() == 0)
		{
			Queue.Add(Plugin.Name);
		}
	}

	while (Queue.Num() > 0)
	{
		// Pop from queue
		FString Current = Queue[0];
		Queue.RemoveAt(0);
		Sorted.Add(Current);

		// Process all plugins that depend on Current
		for (const FOrchestratorPluginEntry& Plugin : CurrentManifest.Plugins)
		{
			if (DependencyGraph[Plugin.Name].Contains(Current))
			{
				// Remove dependency
				DependencyGraph[Plugin.Name].Remove(Current);

				// If no more dependencies, add to queue
				if (DependencyGraph[Plugin.Name].Num() == 0)
				{
					Queue.Add(Plugin.Name);
				}
			}
		}
	}

	// Check for cycles
	if (Sorted.Num() != CurrentManifest.Plugins.Num())
	{
		UE_LOG(LogOrchestrator, Error, TEXT("Orchestrator: CYCLE DETECTED in dependency graph!"));
		UE_LOG(LogOrchestrator, Error, TEXT("  Sorted: %d plugins, Manifest: %d plugins"),
			Sorted.Num(), CurrentManifest.Plugins.Num());

		// Find plugins involved in cycle
		TSet<FString> SortedSet(Sorted);
		UE_LOG(LogOrchestrator, Error, TEXT("  Plugins in cycle:"));
		for (const FOrchestratorPluginEntry& Plugin : CurrentManifest.Plugins)
		{
			if (!SortedSet.Contains(Plugin.Name))
			{
				UE_LOG(LogOrchestrator, Error, TEXT("    - %s"), *Plugin.Name);
			}
		}

		UE_LOG(LogOrchestrator, Error, TEXT("  Cannot proceed with activation"));
		return;
	}

	// Store activation order
	ActivationOrder = Sorted;

	UE_LOG(LogOrchestrator, Log, TEXT("Orchestrator: Dependency resolution complete"));
	UE_LOG(LogOrchestrator, Log, TEXT("  Activation order (%d plugins):"), ActivationOrder.Num());
	for (int32 i = 0; i < ActivationOrder.Num(); ++i)
	{
		UE_LOG(LogOrchestrator, Log, TEXT("    %d. %s"), i + 1, *ActivationOrder[i]);
	}
}

void FOrchestratorCoreModule::MountRequiredEnginePlugins()
{
	UE_LOG(LogOrchestrator, Log, TEXT("=== Orchestrator: Mounting Required Engine Plugins ==="));

	// Collect all unique engine plugins required by manifest plugins
	TSet<FString> RequiredEnginePlugins;
	for (const FOrchestratorPluginEntry& Plugin : CurrentManifest.Plugins)
	{
		for (const FString& EnginePlugin : Plugin.RequiresEnginePlugins)
		{
			RequiredEnginePlugins.Add(EnginePlugin);
		}
	}

	if (RequiredEnginePlugins.Num() == 0)
	{
		UE_LOG(LogOrchestrator, Log, TEXT("Orchestrator: No engine plugins required by manifest"));
		return;
	}

	UE_LOG(LogOrchestrator, Log, TEXT("Orchestrator: %d unique engine plugin(s) required"), RequiredEnginePlugins.Num());

	IPluginManager& PluginManager = IPluginManager::Get();
	int32 MountedCount = 0;
	int32 AlreadyEnabledCount = 0;
	int32 FailedCount = 0;

	for (const FString& EnginePluginName : RequiredEnginePlugins)
	{
		UE_LOG(LogOrchestrator, Log, TEXT("Orchestrator: Checking engine plugin: %s"), *EnginePluginName);

		// Check if already enabled
		TSharedPtr<IPlugin> ExistingPlugin = PluginManager.FindPlugin(EnginePluginName);
		if (ExistingPlugin.IsValid() && ExistingPlugin->IsEnabled())
		{
			UE_LOG(LogOrchestrator, Log, TEXT("  [OK] Already enabled: %s"), *EnginePluginName);
			AlreadyEnabledCount++;
			continue;
		}

		// Attempt to mount
		const bool bMounted = PluginManager.MountExplicitlyLoadedPlugin(EnginePluginName);
		if (!bMounted)
		{
			UE_LOG(LogOrchestrator, Warning, TEXT("  [WARN] Failed to mount: %s"), *EnginePluginName);
			UE_LOG(LogOrchestrator, Warning, TEXT("    Features depending on this may fail to load"));
			FailedCount++;
			continue;
		}

		// Verify plugin is now accessible
		TSharedPtr<IPlugin> Plugin = PluginManager.FindPlugin(EnginePluginName);
		if (!Plugin.IsValid())
		{
			UE_LOG(LogOrchestrator, Error, TEXT("  [ERROR] Plugin mounted but not found: %s"), *EnginePluginName);
			FailedCount++;
			continue;
		}

		UE_LOG(LogOrchestrator, Log, TEXT("  [OK] Mounted: %s"), *EnginePluginName);
		UE_LOG(LogOrchestrator, Log, TEXT("    Path: %s"), *Plugin->GetBaseDir());
		MountedCount++;
	}

	UE_LOG(LogOrchestrator, Log, TEXT("Orchestrator: Engine plugin mounting complete"));
	UE_LOG(LogOrchestrator, Log, TEXT("  Already enabled: %d"), AlreadyEnabledCount);
	UE_LOG(LogOrchestrator, Log, TEXT("  Newly mounted: %d"), MountedCount);
	if (FailedCount > 0)
	{
		UE_LOG(LogOrchestrator, Warning, TEXT("  Failed: %d"), FailedCount);
	}
	UE_LOG(LogOrchestrator, Log, TEXT("=== Orchestrator: Engine Plugins Ready ==="));
}

void FOrchestratorCoreModule::ApplyHotUpdates()
{
	UE_LOG(LogOrchestrator, Log, TEXT("Orchestrator: Applying hot updates (content-only, no restart)..."));

	if (HotUpdatePlugins.Num() == 0)
	{
		UE_LOG(LogOrchestrator, Log, TEXT("  No hot updates needed"));
		return;
	}

	UE_LOG(LogOrchestrator, Log, TEXT("  %d plugins need hot update"), HotUpdatePlugins.Num());

	for (const FString& PluginName : HotUpdatePlugins)
	{
		const FOrchestratorPluginEntry* ManifestEntry = CurrentManifest.FindPlugin(PluginName);
		if (!ManifestEntry)
		{
			UE_LOG(LogOrchestrator, Error, TEXT("    Plugin not found in manifest: %s"), *PluginName);
			continue;
		}

		UE_LOG(LogOrchestrator, Log, TEXT("  HOT UPDATE: %s"), *PluginName);
		UE_LOG(LogOrchestrator, Log, TEXT("    Content URL: %s"), *ManifestEntry->UrlContent);
		UE_LOG(LogOrchestrator, Log, TEXT("    Expected content_hash: %s"), *ManifestEntry->ContentHash);

		// Skip if no content URL
		if (ManifestEntry->UrlContent.IsEmpty())
		{
			UE_LOG(LogOrchestrator, Warning, TEXT("    No content URL - skipping"));
			continue;
		}

		// Download path: <local-app-data>/Alis/Downloads/<Name>_<Version>_content.zip
		const FString DownloadPath = FPaths::Combine(
			GetLocalRoot(),
			TEXT("Downloads"),
			FString::Printf(TEXT("%s_%s_content.zip"), *ManifestEntry->Name, *ManifestEntry->Version));

		// Download content package
		UE_LOG(LogOrchestrator, Log, TEXT("    Downloading to: %s"), *DownloadPath);
		const FDownloadResult DownloadResult = FOrchestratorDownload::DownloadFile(
			ManifestEntry->UrlContent,
			DownloadPath,
			ManifestEntry->ContentHash);

		if (!DownloadResult.bSuccess)
		{
			UE_LOG(LogOrchestrator, Error, TEXT("    Download failed: %s"), *DownloadResult.ErrorMessage);
			continue;
		}

		UE_LOG(LogOrchestrator, Log, TEXT("    Downloaded: %lld bytes in %.2fs"),
			DownloadResult.BytesDownloaded, DownloadResult.DownloadTimeSeconds);

		// Get plugin's installed path from latest state
		const FPluginState* InstalledState = LatestState.Plugins.Find(PluginName);
		if (!InstalledState)
		{
			UE_LOG(LogOrchestrator, Error, TEXT("    Plugin not found in latest state: %s"), *PluginName);
			continue;
		}

		// Extract content to plugin directory
		const FString PluginContentPath = FPaths::GetPath(InstalledState->InstalledPath) / TEXT("Content");
		UE_LOG(LogOrchestrator, Log, TEXT("    Extracting content to: %s"), *PluginContentPath);

		FExtractionResult ContentExtraction = FOrchestratorExtract::ExtractZip(DownloadPath, PluginContentPath, true);

		if (!ContentExtraction.bSuccess)
		{
			UE_LOG(LogOrchestrator, Error, TEXT("    Content extraction failed: %s"), *ContentExtraction.ErrorMessage);
			continue;
		}

		UE_LOG(LogOrchestrator, Log, TEXT("    Extracted %d content files in %.2fs"),
			ContentExtraction.FilesExtracted, ContentExtraction.ExtractionTimeSeconds);

		// TODO: Mount IoStore via FIoStoreUtilities or IFileManager
		// TODO: If plugin has module, ensure it's loaded
		// TODO: Reload feature module (unmount + remount + reload via FModuleManager)

		// Update latest.json with new content_hash
		if (UpdateLatestState(PluginName, *ManifestEntry, InstalledState->InstalledPath))
		{
			UE_LOG(LogOrchestrator, Log, TEXT("    [OK] Hot update complete and saved state: %s"), *PluginName);
		}
		else
		{
			UE_LOG(LogOrchestrator, Error, TEXT("    Hot update succeeded but failed to save state: %s"), *PluginName);
		}
	}

	UE_LOG(LogOrchestrator, Log, TEXT("Orchestrator: Hot update processing complete"));
}

void FOrchestratorCoreModule::StageColdUpdates()
{
	UE_LOG(LogOrchestrator, Log, TEXT("Orchestrator: Staging cold updates (code changes, requires restart)..."));

	// Combine new plugins and cold updates (both require full install)
	TArray<FString> ColdInstallPlugins = NewPlugins;
	ColdInstallPlugins.Append(ColdUpdatePlugins);

	if (ColdInstallPlugins.Num() == 0)
	{
		UE_LOG(LogOrchestrator, Log, TEXT("  No cold updates needed"));
		return;
	}

	UE_LOG(LogOrchestrator, Log, TEXT("  %d plugins need cold install/update"), ColdInstallPlugins.Num());

	// Build pending updates list
	FPendingUpdates NewPendingUpdates;
	NewPendingUpdates.ManifestVersion = CurrentManifest.ManifestVersion;
	NewPendingUpdates.EngineBuildId = CurrentManifest.EngineBuildId;
	NewPendingUpdates.CreatedAt = FDateTime::UtcNow().ToIso8601();

	for (const FString& PluginName : ColdInstallPlugins)
	{
		const FOrchestratorPluginEntry* ManifestEntry = CurrentManifest.FindPlugin(PluginName);
		if (!ManifestEntry)
		{
			UE_LOG(LogOrchestrator, Error, TEXT("    Plugin not found in manifest: %s"), *PluginName);
			continue;
		}

		const FPluginState* InstalledState = LatestState.Plugins.Find(PluginName);
		const bool bIsNew = (InstalledState == nullptr);

		UE_LOG(LogOrchestrator, Log, TEXT("  COLD %s: %s"), bIsNew ? TEXT("INSTALL") : TEXT("UPDATE"), *PluginName);
		UE_LOG(LogOrchestrator, Log, TEXT("    Code URL: %s"), *ManifestEntry->UrlCode);
		UE_LOG(LogOrchestrator, Log, TEXT("    Expected code_hash: %s"), *ManifestEntry->CodeHash);

		// Stage path: <local-app-data>/Alis/Plugins/<Name>/<Version>/
		const FString StagePath = FPaths::Combine(
			GetLocalRoot(),
			TEXT("Plugins"),
			ManifestEntry->Name,
			ManifestEntry->Version);

		UE_LOG(LogOrchestrator, Log, TEXT("    Will stage to: %s"), *StagePath);

		// Skip if no code URL
		if (ManifestEntry->UrlCode.IsEmpty())
		{
			UE_LOG(LogOrchestrator, Warning, TEXT("    No code URL - skipping"));
			continue;
		}

		// Download path: <local-app-data>/Alis/Downloads/<Name>_<Version>_code.zip
		const FString CodeDownloadPath = FPaths::Combine(
			GetLocalRoot(),
			TEXT("Downloads"),
			FString::Printf(TEXT("%s_%s_code.zip"), *ManifestEntry->Name, *ManifestEntry->Version));

		// Download code package
		UE_LOG(LogOrchestrator, Log, TEXT("    Downloading code to: %s"), *CodeDownloadPath);
		FDownloadResult CodeResult = FOrchestratorDownload::DownloadFile(
			ManifestEntry->UrlCode,
			CodeDownloadPath,
			ManifestEntry->CodeHash);

		if (!CodeResult.bSuccess)
		{
			UE_LOG(LogOrchestrator, Error, TEXT("    Code download failed: %s"), *CodeResult.ErrorMessage);
			continue;
		}

		// Handle dev mode (file:// URLs pointing to local plugin directories)
		FString FinalPluginPath = StagePath;
		if (CodeResult.bIsLocalDevMode)
		{
			UE_LOG(LogOrchestrator, Log, TEXT("    Dev mode: Plugin already in place at: %s"), *CodeResult.LocalPluginPath);
			FinalPluginPath = CodeResult.LocalPluginPath;
			// Skip download and extraction - plugin already exists
		}
		else
		{
			UE_LOG(LogOrchestrator, Log, TEXT("    Downloaded code: %lld bytes in %.2fs"),
				CodeResult.BytesDownloaded, CodeResult.DownloadTimeSeconds);

			// Download content package if present
			FString ContentDownloadPath;
			if (!ManifestEntry->UrlContent.IsEmpty())
			{
				ContentDownloadPath = FPaths::Combine(
					GetLocalRoot(),
					TEXT("Downloads"),
					FString::Printf(TEXT("%s_%s_content.zip"), *ManifestEntry->Name, *ManifestEntry->Version));

				UE_LOG(LogOrchestrator, Log, TEXT("    Downloading content to: %s"), *ContentDownloadPath);
				FDownloadResult ContentResult = FOrchestratorDownload::DownloadFile(
					ManifestEntry->UrlContent,
					ContentDownloadPath,
					ManifestEntry->ContentHash);

				if (!ContentResult.bSuccess)
				{
					UE_LOG(LogOrchestrator, Error, TEXT("    Content download failed: %s"), *ContentResult.ErrorMessage);
					// Continue anyway, code-only update possible
				}
				else
				{
					UE_LOG(LogOrchestrator, Log, TEXT("    Downloaded content: %lld bytes in %.2fs"),
						ContentResult.BytesDownloaded, ContentResult.DownloadTimeSeconds);
				}
			}

			// Extract code package to staging directory
			UE_LOG(LogOrchestrator, Log, TEXT("    Extracting code to: %s"), *StagePath);
			FExtractionResult CodeExtraction = FOrchestratorExtract::ExtractZip(CodeDownloadPath, StagePath, true);

			if (!CodeExtraction.bSuccess)
			{
				UE_LOG(LogOrchestrator, Error, TEXT("    Code extraction failed: %s"), *CodeExtraction.ErrorMessage);
				continue;
			}

			UE_LOG(LogOrchestrator, Log, TEXT("    Extracted %d code files in %.2fs"),
				CodeExtraction.FilesExtracted, CodeExtraction.ExtractionTimeSeconds);

			// Extract content package if downloaded
			if (!ContentDownloadPath.IsEmpty())
			{
				UE_LOG(LogOrchestrator, Log, TEXT("    Extracting content to: %s"), *StagePath);
				FExtractionResult ContentExtraction = FOrchestratorExtract::ExtractZip(ContentDownloadPath, StagePath, true);

				if (!ContentExtraction.bSuccess)
				{
					UE_LOG(LogOrchestrator, Warning, TEXT("    Content extraction failed: %s"), *ContentExtraction.ErrorMessage);
					// Continue anyway - code-only plugin is valid
				}
				else
				{
					UE_LOG(LogOrchestrator, Log, TEXT("    Extracted %d content files in %.2fs"),
						ContentExtraction.FilesExtracted, ContentExtraction.ExtractionTimeSeconds);
				}
			}
		}

		// Verify .uplugin file exists in final location
		TArray<FString> UpluginFiles;
		IFileManager::Get().FindFiles(UpluginFiles, *(FinalPluginPath / TEXT("*.uplugin")), true, false);

		if (UpluginFiles.Num() == 0)
		{
			UE_LOG(LogOrchestrator, Error, TEXT("    No .uplugin file found at: %s"), *FinalPluginPath);
			continue;
		}

		UE_LOG(LogOrchestrator, Log, TEXT("    Found .uplugin: %s"), *UpluginFiles[0]);

		// TODO: Verify signature_thumbprint (Authenticode for DLLs)
		// TODO: Compute and verify hashes from extracted files

		// Add to pending updates
		FPendingUpdate PendingUpdate;
		PendingUpdate.Name = PluginName;
		PendingUpdate.FromVersion = InstalledState ? InstalledState->Version : TEXT("(none)");
		PendingUpdate.ToVersion = ManifestEntry->Version;
		PendingUpdate.Change = CodeResult.bIsLocalDevMode ? TEXT("dev_local") : TEXT("code");
		PendingUpdate.StagedPath = FinalPluginPath;
		PendingUpdate.bRequiresRestart = !CodeResult.bIsLocalDevMode;  // Dev mode: plugin already loaded
		NewPendingUpdates.Updates.Add(PendingUpdate);
	}

	// Separate dev mode updates (already in place) from real cold updates (need restart)
	int32 DevModeCount = 0;
	int32 ColdUpdateCount = 0;
	for (const FPendingUpdate& Update : NewPendingUpdates.Updates)
	{
		if (Update.Change == TEXT("dev_local"))
		{
			DevModeCount++;
			// For dev mode, immediately update latest.json - plugin already in place
			FPluginState NewState;
			NewState.Name = Update.Name;
			NewState.Version = Update.ToVersion;
			NewState.InstalledPath = Update.StagedPath;
			LatestState.Plugins.Add(Update.Name, NewState);
		}
		else
		{
			ColdUpdateCount++;
		}
	}

	// Save updated state if dev mode plugins were processed
	if (DevModeCount > 0)
	{
		const FString LatestStatePath = FPaths::Combine(GetLocalRoot(), TEXT("State/latest.json"));
		if (LatestState.SaveToFile(LatestStatePath))
		{
			UE_LOG(LogOrchestrator, Log, TEXT("  Dev mode: Updated latest.json with %d locally-installed plugins"), DevModeCount);
		}
	}

	// Write pending_updates.json only if there are real cold updates
	if (ColdUpdateCount > 0)
	{
		// Filter to only include non-dev updates
		FPendingUpdates ColdUpdatesOnly;
		ColdUpdatesOnly.ManifestVersion = NewPendingUpdates.ManifestVersion;
		ColdUpdatesOnly.EngineBuildId = NewPendingUpdates.EngineBuildId;
		ColdUpdatesOnly.CreatedAt = NewPendingUpdates.CreatedAt;
		for (const FPendingUpdate& Update : NewPendingUpdates.Updates)
		{
			if (Update.Change != TEXT("dev_local"))
			{
				ColdUpdatesOnly.Updates.Add(Update);
			}
		}

		const FString PendingUpdatesPath = FPaths::Combine(GetLocalRoot(), TEXT("State/pending_updates.json"));
		if (ColdUpdatesOnly.SaveToFile(PendingUpdatesPath))
		{
			UE_LOG(LogOrchestrator, Log, TEXT("  Wrote pending_updates.json (%d cold updates)"), ColdUpdateCount);
			UE_LOG(LogOrchestrator, Log, TEXT("  RESTART REQUIRED to apply cold updates"));
		}
		else
		{
			UE_LOG(LogOrchestrator, Error, TEXT("  Failed to write pending_updates.json"));
		}
	}
	else if (DevModeCount > 0)
	{
		UE_LOG(LogOrchestrator, Log, TEXT("  Dev mode: All %d plugins already in place - no restart needed"), DevModeCount);
	}

	UE_LOG(LogOrchestrator, Log, TEXT("Orchestrator: Cold update staging complete"));
}

void FOrchestratorCoreModule::LoadFeatureModules()
{
	UE_LOG(LogOrchestrator, Log, TEXT("Orchestrator: Loading feature modules..."));

	if (ActivationOrder.Num() == 0)
	{
		UE_LOG(LogOrchestrator, Warning, TEXT("Orchestrator: No plugins to load (empty activation order)"));
		return;
	}

	// Debug logs about Slate/GEngine disabled (2025-11-23) - no longer needed without boot widget
	/*
	const bool bSlateInitialized = FSlateApplication::IsInitialized();
	UE_LOG(LogOrchestrator, Log, TEXT("  [DEBUG] Slate initialization status: %s"), bSlateInitialized ? TEXT("TRUE") : TEXT("FALSE"));

	if (GEngine)
	{
		UE_LOG(LogOrchestrator, Log, TEXT("  [DEBUG] GEngine exists: TRUE"));
	}
	else
	{
		UE_LOG(LogOrchestrator, Log, TEXT("  [DEBUG] GEngine exists: FALSE"));
	}
	*/

	// Boot widget callback disabled (2025-11-23) - see CreateBootWidget() comment above
	/*
	bBootInProgress = true;
	UE_LOG(LogOrchestrator, Log, TEXT("  [DEFERRED] Boot widget creation deferred until PostEngineInit (Slate not ready yet)"));

	// Register callback to create widget when engine is initialized
	FCoreDelegates::OnPostEngineInit.AddLambda([this]()
	{
		UE_LOG(LogOrchestrator, Log, TEXT("  [PostEngineInit] Engine initialized, creating boot widget now..."));
		CreateBootWidget();
	});
	*/

	IPluginManager& PluginManager = IPluginManager::Get();
	FModuleManager& ModuleManager = FModuleManager::Get();

	int32 BootLoadedCount = 0;
	int32 OnDemandLoadedCount = 0;
	int32 SkippedCount = 0;

	// Boot widget progress tracking disabled (2025-11-23) - see CreateBootWidget() comment above
	/*
	TotalBootPlugins = 0;  // Use member variable
	for (const FString& PluginName : ActivationOrder)
	{
		const FOrchestratorPluginEntry* Entry = CurrentManifest.FindPlugin(PluginName);
		if (Entry && Entry->ActivationStrategy == TEXT("Boot"))
		{
			TotalBootPlugins++;
		}
	}
	*/

	// Count total Boot plugins for logging (not widget progress)
	int32 TotalBootPlugins = 0;
	for (const FString& PluginName : ActivationOrder)
	{
		const FOrchestratorPluginEntry* Entry = CurrentManifest.FindPlugin(PluginName);
		if (Entry && Entry->ActivationStrategy == TEXT("Boot"))
		{
			TotalBootPlugins++;
		}
	}

	UE_LOG(LogOrchestrator, Log, TEXT("  Total Boot plugins to load: %d"), TotalBootPlugins);

	// Load all plugins at startup (Boot + OnDemand). OnDemand content mounts later via ProjectLoading; modules load here.
	for (const FString& PluginName : ActivationOrder)
	{
		UE_LOG(LogOrchestrator, Log, TEXT("  Loading: %s"), *PluginName);

		// Find plugin entry in manifest
		const FOrchestratorPluginEntry* ManifestEntry = CurrentManifest.FindPlugin(PluginName);
		if (!ManifestEntry)
		{
			UE_LOG(LogOrchestrator, Warning, TEXT("    Plugin not found in manifest - skipping"));
			SkippedCount++;
			continue;
		}

		const bool bIsBoot = ManifestEntry->ActivationStrategy == TEXT("Boot");
		const bool bIsOnDemand = ManifestEntry->ActivationStrategy == TEXT("OnDemand");
		if (!bIsBoot && !bIsOnDemand)
		{
			UE_LOG(LogOrchestrator, Warning, TEXT("    Unknown activation_strategy '%s' - defaulting to Boot"), *ManifestEntry->ActivationStrategy);
		}

		// Boot widget progress tracking disabled (2025-11-23) - see CreateBootWidget() comment above
		/*
		CurrentPluginName = PluginName;
		CurrentBootIndex = BootLoadedCount;

		// Update Slate boot widget (if it exists yet - may be created later)
		if (BootWidget.IsValid())
		{
			BootWidget->UpdateProgress(PluginName, BootLoadedCount, TotalBootPlugins, StatusMessage);
		}
		*/

		// Broadcast progress update (Boot plugin about to load)
		FString StatusMessage = FString::Printf(TEXT("Loading %s..."), *PluginName);
		OnBootProgressUpdate.Broadcast(PluginName, BootLoadedCount, TotalBootPlugins, StatusMessage);

		// Check if plugin is already registered
		TSharedPtr<IPlugin> Plugin = PluginManager.FindPlugin(PluginName);
		if (!Plugin.IsValid())
		{
			UE_LOG(LogOrchestrator, Warning, TEXT("    Plugin not found by IPluginManager"));
			UE_LOG(LogOrchestrator, Log, TEXT("    Attempting to register external plugin..."));

			// Try to find and register the plugin from latest state
			const FPluginState* State = LatestState.Plugins.Find(PluginName);
			if (!State)
			{
				UE_LOG(LogOrchestrator, Error, TEXT("    Plugin not in latest.json - cannot determine path"));
				SkippedCount++;
				continue;
			}

			// Find .uplugin file in installed path
			const FString PluginDir = FPaths::GetPath(State->InstalledPath);
			TArray<FString> UpluginFiles;
			IFileManager::Get().FindFiles(UpluginFiles, *(PluginDir / TEXT("*.uplugin")), true, false);

			if (UpluginFiles.Num() == 0)
			{
				UE_LOG(LogOrchestrator, Error, TEXT("    No .uplugin found in: %s"), *PluginDir);
				SkippedCount++;
				continue;
			}

			const FString UpluginPath = PluginDir / UpluginFiles[0];
			FPluginRegistrationResult RegResult = FOrchestratorPluginRegistry::RegisterPlugin(UpluginPath);

			if (!RegResult.bSuccess)
			{
				UE_LOG(LogOrchestrator, Error, TEXT("    Failed to register: %s"), *RegResult.ErrorMessage);
				SkippedCount++;
				continue;
			}

			UE_LOG(LogOrchestrator, Log, TEXT("    [OK] Plugin registered: %s"), *RegResult.PluginName);

			// Retry finding plugin
			Plugin = PluginManager.FindPlugin(PluginName);
			if (!Plugin.IsValid())
			{
				UE_LOG(LogOrchestrator, Error, TEXT("    Plugin registered but still not found"));
				SkippedCount++;
				continue;
			}
		}

		// Mount plugin (makes content visible)
		if (!Plugin->IsEnabled())
		{
			UE_LOG(LogOrchestrator, Log, TEXT("    Mounting plugin..."));
			PluginManager.MountExplicitlyLoadedPlugin(PluginName);
		}

		// Load module if plugin has code
		if (!ManifestEntry->Module.IsEmpty())
		{
			const FName ModuleName = FName(*ManifestEntry->Module);
			if (!ModuleManager.IsModuleLoaded(ModuleName))
			{
				UE_LOG(LogOrchestrator, Log, TEXT("    Loading module: %s"), *ManifestEntry->Module);
				if (!ModuleManager.LoadModule(ModuleName))
				{
					UE_LOG(LogOrchestrator, Error, TEXT("    Failed to load module - skipping plugin"));
					SkippedCount++;
					continue;
				}

				// Check if module registered itself as a feature module
				// Feature modules call RegisterFeatureModule() in ProjectCore during StartupModule()
				// This registry pattern is safe without RTTI - no unsafe casts involved
				// See: Interfaces/IProjectFeatureModule.h for registration pattern documentation
				IProjectFeatureModule* FeatureModule = GetFeatureModule(ModuleName);
				if (FeatureModule != nullptr)
				{

					// Boot plugins initialize now; OnDemand plugins load code now, boot init deferred
					if (bIsBoot)
					{
						FBootContext BootContext;
						BootContext.LocalRoot = GetLocalRoot();
						BootContext.EngineBuildId = LauncherContext.EngineBuildId;
						BootContext.IsShuttingDown = []() { return IsEngineExitRequested(); };

						UE_LOG(LogOrchestrator, Log, TEXT("    [BOOT] Calling BootInitialize()"));

						try
						{
							FeatureModule->BootInitialize(BootContext);
							UE_LOG(LogOrchestrator, Log, TEXT("    [BOOT] BootInitialize() complete"));
						}
						catch (...)
						{
							UE_LOG(LogOrchestrator, Error, TEXT("    [BOOT] Exception in BootInitialize() - plugin unstable"));
							SkippedCount++;
							continue;
						}
					}
					else
					{
						UE_LOG(LogOrchestrator, Log, TEXT("    OnDemand plugin - BootInitialize() skipped (runtime activation)"));
					}
				}
				else
				{
					UE_LOG(LogOrchestrator, Log, TEXT("    Module not registered as IProjectFeatureModule - no boot callback"));
				}
			}
			else
			{
				UE_LOG(LogOrchestrator, Log, TEXT("    Module already loaded: %s"), *ManifestEntry->Module);
			}
		}

		// Update latest.json with successfully loaded plugin
	if (UpdateLatestState(PluginName, *ManifestEntry, Plugin->GetDescriptorFileName()))
	{
		if (bIsBoot)
		{
			BootLoadedCount++;
		}
		else
		{
			OnDemandLoadedCount++;
		}
		UE_LOG(LogOrchestrator, Log, TEXT("    Loaded and saved state: %s"), *PluginName);
	}
	else
	{
		UE_LOG(LogOrchestrator, Error, TEXT("    Loaded but failed to save state: %s"), *PluginName);
		if (bIsBoot)
		{
			BootLoadedCount++;
		}
		else
		{
			OnDemandLoadedCount++;
		}
	}

	} // end for ActivationOrder

	UE_LOG(LogOrchestrator, Log, TEXT("Orchestrator: Feature module loading complete"));
	UE_LOG(LogOrchestrator, Log, TEXT("  Boot plugins loaded: %d, OnDemand plugins loaded (code only): %d, Skipped: %d, Total manifest entries: %d"),
		BootLoadedCount, OnDemandLoadedCount, SkippedCount, ActivationOrder.Num());

	// Save last_good.json if all loads succeeded
	const int32 TotalLoadedCount = BootLoadedCount + OnDemandLoadedCount;
	if (SkippedCount == 0 && TotalLoadedCount > 0)
	{
		if (SaveLastGoodState())
		{
			UE_LOG(LogOrchestrator, Log, TEXT("Saved last_good.json after successful load batch"));
		}

		// Broadcast boot complete event (all Boot plugins loaded successfully)
		UE_LOG(LogOrchestrator, Log, TEXT("=== Orchestrator Boot Complete ==="));
		UE_LOG(LogOrchestrator, Log, TEXT("  Boot: CODE ONLY (modules/DLLs loaded)"));
		UE_LOG(LogOrchestrator, Log, TEXT("  Content loading deferred to ProjectLoading subsystem (PostEngineInit+)"));
		OnBootComplete.Broadcast();

		// Boot widget closure disabled (2025-11-23) - see CreateBootWidget() comment above
		/*
		// Mark boot as complete
		bBootInProgress = false;

		// Close Slate boot widget
		if (BootWindow.IsValid() && FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().RequestDestroyWindow(BootWindow.ToSharedRef());
			BootWindow.Reset();
			BootWidget.Reset();
			UE_LOG(LogOrchestrator, Log, TEXT("  Closed Slate boot widget"));
		}
		*/
	}
	else if (SkippedCount > 0)
	{
		UE_LOG(LogOrchestrator, Warning, TEXT("Skipped saving last_good.json due to %d failed loads"), SkippedCount);
		UE_LOG(LogOrchestrator, Warning, TEXT("Boot incomplete due to errors - NOT broadcasting OnBootComplete"));
	}
}

FString FOrchestratorCoreModule::GetLocalRoot() const
{
	// LocalRoot = <local-app-data>/Alis for production (from Launcher)
	// For dev mode, use ProjectDir/LocalAppData/Alis
	return FPaths::Combine(LauncherContext.InstallPath, TEXT("LocalAppData/Alis"));
}

FString FOrchestratorCoreModule::GetLatestStatePath() const
{
	return FPaths::Combine(GetLocalRoot(), TEXT("State/latest.json"));
}

FString FOrchestratorCoreModule::GetLastGoodStatePath() const
{
	return FPaths::Combine(GetLocalRoot(), TEXT("State/last_good.json"));
}

bool FOrchestratorCoreModule::UpdateLatestState(
	const FString& PluginName,
	const FOrchestratorPluginEntry& ManifestEntry,
	const FString& InstalledPath)
{
	UE_LOG(LogOrchestrator, Log, TEXT("Updating latest.json for plugin: %s"), *PluginName);

	// Create or update plugin state entry
	FPluginState NewState;
	NewState.Name = ManifestEntry.Name;
	NewState.Version = ManifestEntry.Version;
	NewState.Module = ManifestEntry.Module;
	NewState.CodeHash = ManifestEntry.CodeHash;
	NewState.ContentHash = ManifestEntry.ContentHash;
	NewState.InstalledPath = InstalledPath;
	NewState.Channel = ManifestEntry.Channel;

	// Update in-memory state
	LatestState.Plugins.Add(PluginName, NewState);
	LatestState.ManifestVersion = CurrentManifest.ManifestVersion;
	LatestState.EngineBuildId = CurrentManifest.EngineBuildId;
	LatestState.AppliedAt = FDateTime::UtcNow().ToIso8601();

	// Save to disk
	const FString LatestPath = GetLatestStatePath();
	if (!LatestState.SaveToFile(LatestPath))
	{
		UE_LOG(LogOrchestrator, Error, TEXT("Failed to save latest.json: %s"), *LatestPath);
		return false;
	}

	UE_LOG(LogOrchestrator, Log, TEXT("Updated latest.json successfully"));
	return true;
}

bool FOrchestratorCoreModule::SaveLastGoodState()
{
	UE_LOG(LogOrchestrator, Log, TEXT("Saving last_good.json from latest state"));

	// Copy current latest state to last good
	LastGoodState = LatestState;
	LastGoodState.AppliedAt = FDateTime::UtcNow().ToIso8601();

	// Save to disk
	const FString LastGoodPath = GetLastGoodStatePath();
	if (!LastGoodState.SaveToFile(LastGoodPath))
	{
		UE_LOG(LogOrchestrator, Error, TEXT("Failed to save last_good.json: %s"), *LastGoodPath);
		return false;
	}

	UE_LOG(LogOrchestrator, Log, TEXT("Saved last_good.json successfully (%d plugins)"),
		LastGoodState.Plugins.Num());
	return true;
}

bool FOrchestratorCoreModule::RollbackToLastGood()
{
	UE_LOG(LogOrchestrator, Warning, TEXT("ROLLBACK: Restoring from last_good.json"));

	const FString LastGoodPath = GetLastGoodStatePath();
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Check if last_good.json exists
	if (!PlatformFile.FileExists(*LastGoodPath))
	{
		UE_LOG(LogOrchestrator, Error, TEXT("ROLLBACK FAILED: last_good.json not found: %s"), *LastGoodPath);
		return false;
	}

	// Load last good state
	FOrchestratorState RollbackState;
	if (!FOrchestratorState::LoadFromFile(LastGoodPath, RollbackState))
	{
		UE_LOG(LogOrchestrator, Error, TEXT("ROLLBACK FAILED: Could not load last_good.json"));
		return false;
	}

	UE_LOG(LogOrchestrator, Log, TEXT("Loaded last_good.json: manifest v%s, %d plugins"),
		*RollbackState.ManifestVersion, RollbackState.Plugins.Num());

	// Copy last good state to latest
	LatestState = RollbackState;
	LatestState.AppliedAt = FDateTime::UtcNow().ToIso8601();

	// Save to latest.json
	const FString LatestPath = GetLatestStatePath();
	if (!LatestState.SaveToFile(LatestPath))
	{
		UE_LOG(LogOrchestrator, Error, TEXT("ROLLBACK FAILED: Could not save latest.json"));
		return false;
	}

	UE_LOG(LogOrchestrator, Warning, TEXT("ROLLBACK SUCCESSFUL: Restored %d plugins to last known good state"),
		LatestState.Plugins.Num());

	// TODO: Unload/deactivate any plugins not in rolled-back state
	// TODO: Reload plugins from rolled-back state
	// For now, this requires a restart to take effect

	UE_LOG(LogOrchestrator, Warning, TEXT("RESTART REQUIRED to apply rolled-back state"));

	return true;
}

// ======================================================================
// IOrchestratorRegistry Implementation
// ======================================================================

bool FOrchestratorCoreModule::IsFeatureAvailable(FName PluginName) const
{
	IPluginManager& PluginManager = IPluginManager::Get();
	FModuleManager& ModuleManager = FModuleManager::Get();

	// Check if plugin is registered and mounted
	TSharedPtr<IPlugin> Plugin = PluginManager.FindPlugin(PluginName.ToString());
	if (!Plugin.IsValid() || !Plugin->IsEnabled())
	{
		return false;
	}

	// Find plugin entry in manifest to get module name
	const FOrchestratorPluginEntry* ManifestEntry = CurrentManifest.FindPlugin(PluginName.ToString());
	if (!ManifestEntry || ManifestEntry->Module.IsEmpty())
	{
		// Content-only plugin - available if enabled (UE 5.5+: no IsMounted() method)
		return Plugin->IsEnabled();
	}

	// Check if module is loaded
	const FName ModuleName = FName(*ManifestEntry->Module);
	return ModuleManager.IsModuleLoaded(ModuleName);
}

TArray<FName> FOrchestratorCoreModule::GetLoadedFeatures() const
{
	TArray<FName> LoadedFeatures;

	IPluginManager& PluginManager = IPluginManager::Get();
	FModuleManager& ModuleManager = FModuleManager::Get();

	// Iterate through all plugins in activation order
	for (const FString& PluginName : ActivationOrder)
	{
		if (IsFeatureAvailable(FName(*PluginName)))
		{
			LoadedFeatures.Add(FName(*PluginName));
		}
	}

	return LoadedFeatures;
}

FString FOrchestratorCoreModule::GetFeatureVersion(FName PluginName) const
{
	// Check latest state for version
	const FPluginState* State = LatestState.Plugins.Find(PluginName.ToString());
	if (State)
	{
		return State->Version;
	}

	// Fallback: check plugin descriptor
	IPluginManager& PluginManager = IPluginManager::Get();
	TSharedPtr<IPlugin> Plugin = PluginManager.FindPlugin(PluginName.ToString());
	if (Plugin.IsValid())
	{
		return Plugin->GetDescriptor().VersionName;
	}

	return FString();
}

bool FOrchestratorCoreModule::IsCompatibleEngineBuildId(const FString& ManifestBuildId, const FString& CurrentBuildId)
{
	UE_LOG(LogOrchestrator, Log, TEXT("  Checking engine compatibility: manifest='%s', current='%s'"),
		*ManifestBuildId, *CurrentBuildId);

	// 1. Exact match is always compatible
	if (ManifestBuildId == CurrentBuildId)
	{
		UE_LOG(LogOrchestrator, Log, TEXT("  [COMPATIBLE] Exact match"));
		return true;
	}

	// 2. Extract major.minor version from both strings
	// Patterns we support:
	//   - "++UE5+Release-5.5-CL-40574608" -> "5.5"
	//   - "UE5-CL-0" -> "5" (source-built, no minor)
	//   - "5.5.0" -> "5.5"
	auto ExtractMajorMinor = [](const FString& BuildId) -> FString
	{
		// Look for pattern like "-5.5-" or "-5.4-" (UE release format)
		int32 DashIdx = BuildId.Find(TEXT("-5."));
		if (DashIdx != INDEX_NONE)
		{
			// Extract "5.X" from "-5.X-"
			int32 StartIdx = DashIdx + 1;
			int32 EndIdx = BuildId.Find(TEXT("-"), ESearchCase::CaseSensitive, ESearchDir::FromStart, StartIdx + 2);
			if (EndIdx != INDEX_NONE)
			{
				return BuildId.Mid(StartIdx, EndIdx - StartIdx);
			}
		}

		// Look for standalone version at start like "5.5.0"
		if (BuildId.Len() >= 3 && BuildId[0] >= '4' && BuildId[0] <= '9')
		{
			int32 FirstDot = BuildId.Find(TEXT("."));
			if (FirstDot != INDEX_NONE && FirstDot < 2)
			{
				int32 SecondDot = BuildId.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromStart, FirstDot + 1);
				if (SecondDot != INDEX_NONE)
				{
					return BuildId.Left(SecondDot);
				}
				// Just major.minor
				return BuildId.Left(FMath::Min(3, BuildId.Len()));
			}
		}

		// Check for "UE5" prefix (source-built without explicit version)
		if (BuildId.StartsWith(TEXT("UE5")) || BuildId.Contains(TEXT("+UE5+")))
		{
			return TEXT("5");  // UE5 major version
		}

		return FString();
	};

	FString ManifestVersion = ExtractMajorMinor(ManifestBuildId);
	FString CurrentVersion = ExtractMajorMinor(CurrentBuildId);

	UE_LOG(LogOrchestrator, Log, TEXT("  Extracted versions: manifest='%s', current='%s'"),
		*ManifestVersion, *CurrentVersion);

	// 3. If both have versions, compare them
	if (!ManifestVersion.IsEmpty() && !CurrentVersion.IsEmpty())
	{
		// If one is just major (5) and other is major.minor (5.5), check major matches
		if (ManifestVersion.Len() == 1 || CurrentVersion.Len() == 1)
		{
			// Compare just major version
			const bool bMajorMatch = ManifestVersion.Left(1) == CurrentVersion.Left(1);
			if (bMajorMatch)
			{
				UE_LOG(LogOrchestrator, Log, TEXT("  [COMPATIBLE] Major version match: %s == %s"),
					*ManifestVersion.Left(1), *CurrentVersion.Left(1));
			}
			else
			{
				UE_LOG(LogOrchestrator, Warning, TEXT("  [INCOMPATIBLE] Major version mismatch: %s != %s"),
					*ManifestVersion.Left(1), *CurrentVersion.Left(1));
			}
			return bMajorMatch;
		}

		// Both have major.minor, compare exactly
		const bool bVersionMatch = ManifestVersion == CurrentVersion;
		if (bVersionMatch)
		{
			UE_LOG(LogOrchestrator, Log, TEXT("  [COMPATIBLE] Major.minor version match: %s == %s"),
				*ManifestVersion, *CurrentVersion);
		}
		else
		{
			UE_LOG(LogOrchestrator, Warning, TEXT("  [INCOMPATIBLE] Major.minor version mismatch: %s != %s"),
				*ManifestVersion, *CurrentVersion);
		}
		return bVersionMatch;
	}

	// 4. Fallback: both must be UE5 (lenient for dev/source builds)
#if !UE_BUILD_SHIPPING
	bool bManifestIsUE5 = ManifestBuildId.Contains(TEXT("UE5"));
	bool bCurrentIsUE5 = CurrentBuildId.Contains(TEXT("UE5"));
	if (bManifestIsUE5 && bCurrentIsUE5)
	{
		UE_LOG(LogOrchestrator, Warning, TEXT("  [COMPATIBLE] Lenient UE5 fallback (dev mode only)"));
		return true;
	}
#endif

	UE_LOG(LogOrchestrator, Error, TEXT("  [INCOMPATIBLE] Could not determine compatibility (no version extracted)"));
	return false;
}

IMPLEMENT_MODULE(FOrchestratorCoreModule, OrchestratorCore)
