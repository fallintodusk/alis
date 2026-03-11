// Plugin Dependency Relationships (Compile-Time + Runtime Interactions)
// This file defines dependencies between plugin components:
// - Arrow direction: Shows WHO depends on WHAT (both compile-time includes + runtime interactions)
// - .uplugin "Plugins" declarations (build-time linking)
// - ServiceLocator interactions (runtime Register/Resolve calls)
//
// Arrow Tags:
// - "Implements" (green dashed) = System REGISTERS interface in Core's ServiceLocator
// - "Uses" (blue solid) = Feature RESOLVES interface from Core's ServiceLocator
// - No tag (gray solid) = Regular module dependency (not interface-related)
//
// For RUNTIME activation order, see manifest.json dependency resolution
// ASCII only (project rule)
//
// PLUGIN ACTIVATION STRATEGIES (Boot vs OnDemand):
// - Boot plugins: Loaded at startup (foundation, systems, essential UI)
// - OnDemand plugins: Lazy-loaded at runtime (gameplay features, content packs)
// See Plugins/Boot/Orchestrator/README.md for activation lifecycle details

// Boot Infrastructure (Launcher triggers Orchestrator directly)
// Entry point: Launcher starts UE exe -> Orchestrator Manager starts at engine PostConfigInit
// (Launcher->Client relationship defined in model_launcher_relationships.dsl)

// Orchestrator internal dependency chain (local operations only, NO verification - Launcher already verified)
// Orchestrator Manager orchestrates the boot sequence (coordinator pattern)
// SRP separation: PluginRegistry (state tracking) vs PluginLoader (stateless operations)
// DependencyResolver queries Registry to optimize load order (skip already-loaded dependencies)
project.projectClient.orchestrator -> project.projectClient.orchestratorManifestParser "Parse local manifest.json" "Uses"
project.projectClient.orchestrator -> project.projectClient.orchestratorDependencyResolver "Resolve plugin dependencies (transitive)" "Uses"
project.projectClient.orchestratorDependencyResolver -> project.projectClient.orchestratorPluginRegistry "Query state to filter already-loaded plugins" "Uses"
project.projectClient.orchestrator -> project.projectClient.orchestratorPluginLoader "Execute load/unload operations on optimized list (DLLs only)" "Uses"
project.projectClient.orchestratorPluginLoader -> project.projectClient.orchestratorPluginRegistry "Update state after load/unload success" "Callback"

// Responsibility split (SOLID-compliant):
// - Launcher: Downloads, verifies (signature + hashes), installs manifest/project/orchestrator/plugins, starts UE exe
// - Orchestrator Manager: Reads local manifest (trusts Launcher verification), resolves dependencies, mounts/loads plugins
// - No runtime downloads or verification: All verification done by Launcher BEFORE UE starts

// Manifest entryPoint handoff is DATA ONLY (no module dependency):
// Orchestrator records manifest.entryPoint during boot; GameInstance/ProjectLoading reads manifest/state after engine init and starts loading via ILoadingService.

// Foundation tier has NO dependencies (zero coupling)
// Activation: Boot (required for all other plugins)

// UI framework has NO dependencies (independent, reusable UI components)
// Activation: Boot (required for all UI features)

// System plugins depend on foundation - ALL register interfaces in ServiceLocator (DIP pattern)
// Activation: Boot (services must be available before features)
project.projectClient.projectLoading -> project.projectClient.projectCore "Registers ILoadingService (via ProjectLoading subsystem)" "C++ API" {
    tags "Implements"
}

project.projectClient.projectSave -> project.projectClient.projectCore "Registers ISaveService" "C++ API" {
    tags "Implements"
}

project.projectClient.projectWorld -> project.projectClient.projectCore "Registers IWorldService" "C++ API" {
    tags "Implements"
}

project.projectClient.projectPCG -> project.projectClient.projectCore "Registers IPCGRuntimeService" "C++ API" {
    tags "Implements"
}

// Stub plugins with minimal dependencies
project.projectClient.projectSettings -> project.projectClient.projectCore "Registers ISettingsService" "C++ API" {
    tags "Implements"
}

project.projectClient.projectGameplay -> project.projectClient.projectCore "Registers IGameplayService" "C++ API" {
    tags "Implements"
}

// Menu plugins: ProjectMenuMain = Boot (required for frontend), ProjectMenuGame = OnDemand (gameplay only)
project.projectClient.projectMenuMain -> project.projectClient.projectCore "Uses ILoadingService" "C++ API" {
    tags "Uses"
}
project.projectClient.projectMenuMain -> project.projectClient.projectUI "MVVM widgets, screen stack, theming" "C++ API"
project.projectClient.projectMenuMain -> project.projectClient.projectSettingsUI "Embed settings screens in main menu" "C++ API"

project.projectClient.projectMenuGame -> project.projectClient.projectCore "Uses ILoadingService, ISaveService" "C++ API" {
    tags "Uses"
}
project.projectClient.projectMenuGame -> project.projectClient.projectUI "MVVM widgets, screen stack, theming" "C++ API"
project.projectClient.projectMenuGame -> project.projectClient.projectSettingsUI "Embed settings screens in pause menu" "C++ API"

// ProjectSettingsUI: Boot (embedded by ProjectMenuMain, required for frontend)
// Currently a stub (no Source code, no .uplugin dependencies)
// FUTURE: When implemented, add this dependency:
project.projectClient.projectSettingsUI -> project.projectClient.projectCore "Uses ISettingsService" "C++ API" {
    tags "Uses"
}
project.projectClient.projectSettingsUI -> project.projectClient.projectUI "MVVM widgets, screen stack for settings screens" "C++ API"

// Gameplay plugins: OnDemand (lazy-loaded when gameplay needs them)
// ProjectInventory has NO dependencies (standalone)
// ProjectCombat has NO dependencies (standalone)
// ProjectDialogue has NO dependencies (standalone)

// GameMode consolidators: Thin orchestrators (consolidate UE layer + game layer)
// ProjectMenuPlay: Boot (needed for menu world)
// Consolidates: UE layer (GM/PC/Pawn via IGameplayService) + menu UI (ProjectMenuMain)
// Uses soft refs to ProjectMenuMain (NO compile-time dependency on ProjectUI)
project.projectClient.projectMenuPlay -> project.projectClient.projectCore "Uses ILoadingService, IGameplayService" "C++ API" {
    tags "Uses"
}
project.projectClient.projectMenuPlay -> project.projectClient.projectMenuMain "Ensures menu UI plugin loaded" "C++ API"

// ProjectSinglePlay: OnDemand (loaded with gameplay worlds)
// Consolidates: UE layer (GM/PC/Pawn via IGameplayService) + game layer (Combat/Dialogue/Inventory via soft refs)
// Uses IGameplayService for base gameplay classes (composition via service, no direct inheritance)
// Uses ILoadingService to load on-demand features (ProjectLoading mounts IoStore/content and manages phases)
project.projectClient.projectSinglePlay -> project.projectClient.projectCore "Uses ILoadingService, IGameplayService" "C++ API" {
    tags "Uses"
}

// ProjectOnlinePlay: OnDemand (loaded with online gameplay)
// Consolidates: UE layer (GM/PC/Pawn via IGameplayService) + game layer (Combat/Dialogue/Inventory via soft refs) + networking
// Uses IGameplayService for base gameplay classes (composition via service, no direct inheritance)
// Uses ILoadingService to load on-demand features (ProjectLoading mounts IoStore/content and manages phases)
project.projectClient.projectOnlinePlay -> project.projectClient.projectCore "Uses ILoadingService, IGameplayService" "C++ API" {
    tags "Uses"
}

// World plugins: Declare GameModes (NO direct feature dependencies)
// MainMenuWorld: Boot (frontend world)
// World depends ONLY on GameMode - GameMode orchestrates features
project.projectClient.mainMenuWorld -> project.projectClient.projectMenuPlay "Declares MenuFrontEndGameMode" "World Settings"
// Entry point data (manifest/state/config)
project.projectClient.orchestrator -> project.projectClient.entryPointState "Persist manifest.entryPoint from verified manifest/state (data only, no calls)" "Data"
project.projectClient.projectLoadingSubsystem -> project.projectClient.entryPointState "Resolves EntryPointExperience from manifest/state (falls back to config)" "Config"
// City17: OnDemand (example gameplay world)
// World depends ONLY on GameMode - GameMode consolidates features via soft refs
project.projectClient.city17 -> project.projectClient.projectSinglePlay "Declares SinglePlayerGameMode" "World Settings"
project.projectClient.city17 -> project.projectClient.projectOnlinePlay "Declares OnlineMultiplayerGameMode" "World Settings"
// NOTE: NO direct arrows to ProjectCombat/ProjectDialogue/ProjectInventory
// ProjectSinglePlay/ProjectOnlinePlay use ILoadingService (ILoadingHandle) to request features at runtime via ProjectLoading

// ProjectLoading pipeline (ILoadingService phases executed in order; data handoff is via manifest/state/config, no direct call from Orchestrator)
project.projectClient.alisGI -> project.projectClient.projectLoadingSubsystem "GameInstance host only (engine-instantiated; no loading logic)" "GameInstance"
project.projectClient.projectLoadingSubsystem -> project.projectClient.resolveAssetsPhase "1 ResolveAssets: validate request; resolve map/experience; soft-path fallback" "Phase"
project.projectClient.resolveAssetsPhase -> project.projectClient.mountContentPhase "2 MountContent: stub; Orchestrator already mounted content" "Phase"
project.projectClient.mountContentPhase -> project.projectClient.preloadCriticalAssetsPhase "3 PreloadCriticalAssets: stream map/experience with timeout + cancellation" "Phase"
project.projectClient.preloadCriticalAssetsPhase -> project.projectClient.activateFeaturesPhase "4 ActivateFeatures: ensure requested feature modules loaded (Orchestrator-backed)" "Phase"
project.projectClient.activateFeaturesPhase -> project.projectClient.travelPhase "5 Travel: build travel URL; ServerTravel entry map" "Phase"
project.projectClient.travelPhase -> project.projectClient.warmupPhase "6 Warmup: shader precompile + streaming level precache" "Phase"
project.projectClient.warmupPhase -> project.projectClient.mainMenuWorld "Entry map ready (BeginPlay continues)" "EntryPoint"

// NOTE: Orchestrator does NOT have hardcoded relationships to individual plugins
// Orchestrator behavior (data-driven from manifest.json):
// - Boot Phase: Load preload list (foundation only, e.g. ProjectCore), then load DLLs for Boot plugins in dependency order
// - Entry Point handoff: After engine init, GameInstance/ProjectLoading resolves manifest.entryPoint (e.g., "MainMenuWorld") via ILoadingService (content + IoStore handled there)
// - Runtime Phase: Lazy-load OnDemand plugins via ILoadingService (ProjectLoading mounts IoStore, loads assets, activates features)
// - Topological sort: Ensures dependencies load before dependents (e.g., ProjectCore before ProjectUI)
// - Optimization: Registry check prevents redundant loads (idempotent LoadPlugin calls)
// - Open/Closed Principle: Orchestrator never changes, manifest.json drives all plugin knowledge + entry point + dependencies
//
// Unload Safety (reference counting prevents breaking loaded plugins):
// - UnloadPlugin("TargetPlugin") flow:
//   1. DependencyResolver queries Registry: GetLoadedDependents("TargetPlugin")
//   2. Registry returns reverse dependencies: which loaded plugins depend on TargetPlugin
//   3. If dependent count > 0: BLOCK unload, log info with dependent list
//   4. If dependent count = 0: ALLOW unload, PluginLoader executes unload operation
// - Example: UnloadPlugin("ProjectCore") when ProjectCombat is loaded:
//   - Registry check: ProjectCombat depends on ProjectCore (loaded) -> BLOCK
//   - Log: "Cannot unload ProjectCore - required by loaded plugins: [ProjectCombat, ProjectDialogue]"
// - Example: UnloadPlugin("ProjectCombat") when no dependents loaded:
//   - Registry check: No loaded plugins depend on ProjectCombat -> ALLOW
//   - PluginLoader unloads ProjectCombat successfully
//
// See dynamic views (dynamic_update_*.dsl) for runtime activation flows
// See Plugins/Boot/Orchestrator/README.md for activation lifecycle details
