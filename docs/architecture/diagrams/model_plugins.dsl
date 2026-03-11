// Plugin Component Definitions
// This file is included inside the projectClient container block in model.dsl
// ASCII only (project rule)

// Boot Infrastructure (Launcher triggers Orchestrator directly)
orchestrator = component "Orchestrator Manager" "Entry point triggered by Launcher: reads manifest, loads all Boot plugin DLLs in dependency order, records manifest.entryPoint (e.g. MainMenuWorld) for ProjectLoading to execute via ILoadingService" "C++" {
    tags "Plugin,Tier:Boot,Domain:Platform"
}

// Orchestrator internal responsibilities (real modules/classes)
orchestratorManifestParser = component "Orchestrator.ManifestParser" "Reads local manifest.json (already verified by Launcher)" "C++" {
    tags "Tier:Boot,Domain:Platform"
}
orchestratorDependencyResolver = component "Orchestrator.DependencyResolver" "Transitive dependency resolution: walks dependency graph from entryPoint/requested plugin, queries PluginRegistry to skip loaded plugins, performs topological sort, outputs optimized load order. For unload: queries reverse dependencies (reference counting) to prevent unloading plugins that other loaded plugins depend on" "C++" {
    tags "Tier:Boot,Domain:Platform"
}
orchestratorPluginRegistry = component "Orchestrator.PluginRegistry" "Tracks plugin state: IsLoaded/Unloaded/Mounting, loaded versions, reverse dependency map (reference counting for safe unload). Query API: IsPluginLoaded, GetLoadedPlugins, GetPluginVersion, GetLoadedDependents (which loaded plugins depend on target)" "C++" {
    tags "Tier:Boot,Domain:Platform"
}
orchestratorPluginLoader = component "Orchestrator.PluginLoader" "Stateless plugin operations: registers plugin metadata with IPluginManager, loads/unloads DLLs via FModuleManager. No IoStore/pak mounting (delegated to ProjectLoading)" "C++" {
    tags "Tier:Boot,Domain:Platform"
}

// Telemetry
clientMonitoringSender = component "Client.MonitoringSender" "Sends game telemetry (gameplay, crashes, perf) to monitoring service" "C++" {
    tags "Tier:Boot,Domain:Platform"
}

// Entry point state (data only; written by manifest/state, read by ProjectLoading)
entryPointState = component "EntryPoint (manifest/state/config)" "Data holder for entryPointExperience/MainMenuWorld. Written from verified manifest/state; fallback from config [/Script/Alis.AlisGI]. Read by ProjectLoading to start loading pipeline." "Data" {
    tags "Data,Tier:Boot,Domain:Platform"
}

// GameInstance root (hosts subsystems, reads entry experience)
alisGI = component "AlisGI (GameInstance)" "GameInstance class instantiated by UE; passive host for UProjectLoadingSubsystem (no loading logic, no entrypoint reads)" "C++" {
    tags "Tier:Boot,Domain:Platform"
}

// Core Foundation Plugins (no dependencies)
projectCore = component "ProjectCore" "DIP abstraction layer: ILoadingService interface, service locator, foundation types" "C++" {
    tags "Plugin,Tier:Foundation,Domain:Platform"
}

// System Plugins (depend on foundation)
projectLoading = component "ProjectLoading" "6-phase loading pipeline (implements ILoadingService)" "C++" {
    tags "Plugin,Tier:Systems,Domain:Platform"
}

// ProjectLoading internals (ILoadingService + phased executors)
projectLoadingSubsystem = component "ProjectLoading (ILoadingService)" "ProjectLoading plugin entrypoint: GameInstance subsystem registers ILoadingService, schedules initial experience, and runs the 6-phase pipeline" "C++" {
    tags "Subsystem,Tier:Systems,Domain:Platform"
}
resolveAssetsPhase = component "ResolveAssetsPhaseExecutor" "Phase 1 - Validates FLoadRequest (MapAssetId or MapSoftPath), resolves map/experience, logs features, supports soft-path fallback" "C++" {
    tags "Phase:1,Domain:Platform"
}
mountContentPhase = component "MountContentPhaseExecutor" "Phase 2 - Stub: content mounting handled by Orchestrator; skips unless ContentPacksToMount provided" "C++" {
    tags "Phase:2,Domain:Platform"
}
preloadCriticalAssetsPhase = component "PreloadCriticalAssetsPhaseExecutor" "Phase 3 - Preloads map/experience via StreamableManager with progress, timeout, and cancellation" "C++" {
    tags "Phase:3,Domain:Platform"
}
activateFeaturesPhase = component "ActivateFeaturesPhaseExecutor" "Phase 4 - Validates requested feature modules are loaded (delegated to Orchestrator when enabled)" "C++" {
    tags "Phase:4,Domain:Platform"
}
travelPhase = component "TravelPhaseExecutor" "Phase 5 - Resolves map path (AssetId first, soft-path fallback), builds travel URL with options, calls ServerTravel" "C++" {
    tags "Phase:5,Domain:Platform"
}
warmupPhase = component "WarmupPhaseExecutor" "Phase 6 - Optional shader precompile plus streaming level precache to reduce hitching" "C++" {
    tags "Phase:6,Domain:Platform"
}
projectSave = component "ProjectSave" "Save/load service: slots, profiles, settings persistence" "C++" {
    tags "Plugin,Tier:Systems,Domain:Platform"
}
projectPCG = component "ProjectPCG" "PCG engine integration: runtime services, custom nodes" "C++" {
    tags "Plugin,Tier:Systems,Domain:World"
}
projectWorld = component "ProjectWorld" "World Partition/HLOD services + UProjectWorldManifest data assets" "C++" {
    tags "Plugin,Tier:Systems,Domain:World"
}
projectSettings = component "ProjectSettings" "Typed settings API: persistence, defaults, validation" "C++" {
    tags "Plugin,Tier:Systems,Domain:Platform"
}
// Gameplay Tier Framework (contracts only - registry + interfaces)
projectGameplay = component "ProjectGameplay" "Gameplay framework: FGameplayFeatureRegistry (FeatureName -> InitFn), FFeatureInitContext. Features self-register, attach own components." "C++" {
    tags "Plugin,Tier:Gameplay,Domain:Gameplay"
}

// UI (framework only)
projectUI = component "ProjectUI" "CommonUI + MVVM framework: widgets, screen stack" "C++" {
    tags "Plugin,Tier:UI,Domain:UI"
}

// UI Feature Plugins (presentation layer: screens, widgets)
projectMenuMain = component "ProjectMenuMain" "Front-end menu (logic + screens)" "C++/UMG" {
    tags "Plugin,Tier:UI,Domain:Menu"
}
projectMenuGame = component "ProjectMenuGame" "In-game/pause menu" "C++/UMG" {
    tags "Plugin,Tier:UI,Domain:Menu"
}
projectSettingsUI = component "ProjectSettingsUI" "Settings screens (uses ProjectSettings)" "C++/UMG" {
    tags "Plugin,Tier:UI,Domain:UI"
}

// Feature Plugins (pure mechanics: combat, dialogue, inventory)
projectInventory = component "ProjectInventory" "Loaded on-demand via ILoadingService when gameplay requests inventory (ProjectLoading mounts content and drives phases)" "C++/UMG" {
    tags "Plugin,Tier:Features,Domain:Gameplay"
}
projectCombat = component "ProjectCombat" "Loaded on-demand via ILoadingService when gameplay requests combat systems (ProjectLoading mounts content and drives phases)" "C++/UMG" {
    tags "Plugin,Tier:Features,Domain:Gameplay"
}
projectDialogue = component "ProjectDialogue" "Loaded on-demand via ILoadingService when gameplay requests dialogue systems (ProjectLoading mounts content and drives phases)" "C++/UMG" {
    tags "Plugin,Tier:Features,Domain:Gameplay"
}

// Gameplay Consolidators (orchestrate UE layer + game layer)
projectMenuPlay = component "ProjectMenuPlay" "Consolidates UE layer (GM/PC/Pawn) + menu features" "C++" {
    tags "Plugin,Tier:Gameplay,Domain:Menu"
}
projectSinglePlay = component "ProjectSinglePlay" "Consolidates UE layer (GM/PC/Pawn) + game layer (Combat/Dialogue/Inventory)" "C++" {
    tags "Plugin,Tier:Gameplay,Domain:Gameplay"
}
projectOnlinePlay = component "ProjectOnlinePlay" "Consolidates UE layer + game layer + networking" "C++" {
    tags "Plugin,Tier:Gameplay,Domain:Gameplay"
}

// World Plugins (composition: maps, data layers, PCG)
mainMenuWorld = component "MainMenuWorld" "Menu/frontend world composition" "Content" {
    tags "Plugin,Tier:World,Domain:Menu"
}
city17 = component "City17" "Example gameplay world with urban environment" "Content" {
    tags "Plugin,Tier:World,Domain:Gameplay"
}
