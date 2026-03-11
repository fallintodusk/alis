// C3 - Component view: Plugin Architecture inside UE client container
// Purpose: Show plugin layering pattern (Foundation -> Systems -> Gameplay/UI/Features -> World)
// Shows selective dependencies to demonstrate tier separation and dependency rules
// Full dependency graph available in model_plugin_relationships.dsl

component project.projectClient {
    title "C3 - Plugin Architecture (Layered)"

    // Boot Infrastructure (manages plugin lifecycle)
    include project.projectClient.orchestrator

    // Foundation Tier (no dependencies)
    include project.projectClient.projectCore

    // Systems Tier (most depend on ProjectCore for DIP interfaces)
    include project.projectClient.projectLoading
    include project.projectClient.projectSave
    include project.projectClient.projectSettings
    include project.projectClient.projectPCG
    include project.projectClient.projectWorld

    // Gameplay Tier (framework + consolidators: orchestrate UE layer + game layer)
    include project.projectClient.projectGameplay
    include project.projectClient.projectMenuPlay
    include project.projectClient.projectSinglePlay
    include project.projectClient.projectOnlinePlay

    // UI Tier (framework + UI features)
    include project.projectClient.projectUI
    include project.projectClient.projectMenuMain
    include project.projectClient.projectMenuGame
    include project.projectClient.projectSettingsUI

    // Features Tier (pure mechanics: combat, dialogue, inventory)
    include project.projectClient.projectInventory
    include project.projectClient.projectCombat
    include project.projectClient.projectDialogue

    // World Tier (composition: maps, data layers, declare GameModes)
    include project.projectClient.mainMenuWorld
    include project.projectClient.city17

    // Relationships shown automatically for included components
    // See model_plugin_relationships.dsl for full compile-time dependency graph (based on .uplugin files)
    //
    // WHY Foundation (ProjectCore) exists:
    // - DIP abstraction: ILoadingService (for Features -> Systems decoupling), ILoadingHandle
    // - Service interface locator: FProjectServiceLocator (enables DIP pattern)
    // - Foundation types: FLoadRequest, FLoadPhaseState, etc.
    //
    // WHY Boot Infrastructure (Orchestrator) depends on Foundation:
    // - Orchestrator: Uses foundation types for manifest parsing and dependency resolution; content loading is delegated to ProjectLoading via ILoadingService
    //
    // WHY Systems depend on Foundation (DIP pattern - ALL Systems register interfaces):
    // - ProjectLoading: [DONE] Registers ILoadingService
    // - ProjectSave: Registers ISaveService
    // - ProjectSettings: Registers ISettingsService
    // - ProjectWorld: Registers IWorldService (manifest queries, world management)
    // - ProjectPCG: Registers IPCGRuntimeService (for procedural features: forest, urban, ruins)
    //
    // WHY Gameplay Tier (Framework + Consolidators - orchestrate UE layer + game layer):
    // - ProjectGameplay: Gameplay framework (contracts only, no concrete behavior)
    //   * FGameplayFeatureRegistry: Maps FeatureName -> InitFunction
    //   * FFeatureInitContext: World, GameMode, Pawn, ModeName, ConfigJson
    //   * Features self-register on module startup, attach their own components
    //   * NO IGameplayService - DIP via registry + interfaces
    // - ProjectMenuPlay: Thin orchestrator for menu world
    //   * UE layer: Own GameMode/PC/Pawn classes (spectator mode, UI-only input)
    //   * Game layer: ProjectMenuMain/ProjectSettingsUI via ILoadingService
    //   * Policy only: no pawn, spectator mode, no mechanics implementation
    // - ProjectSinglePlay: Thin orchestrator for single-player
    //   * UE layer: Own GameMode/PC (data-driven via FSinglePlayModeConfig)
    //   * Game layer: Combat/Dialogue/Inventory via soft references (no hard dependencies)
    // - ProjectOnlinePlay: Thin orchestrator for online multiplayer
    //   * Same as ProjectSinglePlay plus networking layer (replication, online subsystem)
    //
    // WHY UI Tier (framework + UI features):
    // - ProjectUI: Framework only - zero dependencies, pure MVVM/CommonUI/theming
    // - ProjectMenuMain: Front-end menu screens
    //   * Uses ILoadingService (ProjectCore), ProjectUI, ProjectSettingsUI
    //   * Presentation layer only - calls Systems APIs, NO gameplay/feature logic
    // - ProjectMenuGame: In-game/pause menu screens
    //   * Uses ILoadingService (return to menu), ISaveService (quick save/load), ProjectUI, ProjectSettingsUI
    // - ProjectSettingsUI: Settings screens using ISettingsService (STUB)
    //   * Uses ISettingsService via FProjectServiceLocator, ProjectUI MVVM widgets
    // RULE: UI never depends on Gameplay or Features tiers - only Systems APIs
    //
    // WHY Features Tier (pure mechanics: standalone, reusable):
    // - ProjectInventory: NO dependencies (standalone stub)
    // - ProjectCombat: NO dependencies (standalone stub)
    // - ProjectDialogue: NO dependencies (standalone stub)
    // RULE: Features never depend on UI or Gameplay tiers
    //
    // WHY World Tier depends on Gameplay (NOT Features/UI directly):
    // - Worlds are content-only (maps, data layers, PCG graphs), no code
    // - Worlds declare ONE GameMode in World Settings (menu -> ProjectMenuPlay, gameplay -> ProjectSinglePlay/OnlinePlay)
    // - GameMode consolidates features via ILoadingService at runtime
    // - NO hard compile-time dependencies from worlds to Features/UI
    // RULE: World -> Gameplay only (GameMode orchestrates everything else)
    //
    // NOTE: BootROM not shown - immutable infrastructure that loads Orchestrator at PostConfigInit
    // NOTE: Orchestrator shown above - loads DLLs at boot, defers IoStore/content to ProjectLoading
    // NOTE: Orchestrator activates boot plugins at startup, lazy-loads OnDemand DLLs at runtime, ProjectLoading handles assets
    // NOTE: ProjectExperience DELETED - redundant with ProjectLoading + GameMode/MatchState
    //
    // PLUGIN ACTIVATION STRATEGIES (see Plugins/Boot/Orchestrator/README.md):
    // - Boot plugins (activationStrategy="Boot"): Loaded at startup
    //   * Foundation: ProjectCore (interfaces, ServiceLocator)
    //   * Systems: ProjectLoading, ProjectSave, ProjectSettings, ProjectWorld, ProjectPCG
    //   * Gameplay: ProjectGameplay (framework), ProjectMenuPlay (menu world consolidator)
    //   * UI: ProjectUI (framework), ProjectMenuMain, ProjectSettingsUI (frontend screens)
    //   * World: MainMenuWorld (menu/frontend world) - mounted by ProjectLoading via manifest.entryPoint
    // - OnDemand plugins (activationStrategy="OnDemand"): Lazy-loaded at runtime
    //   * Gameplay: ProjectSinglePlay, ProjectOnlinePlay (loaded with gameplay worlds)
    //   * Features: ProjectCombat, ProjectDialogue, ProjectInventory (loaded when needed via ILoadingService)
    //   * UI: ProjectMenuGame (only during gameplay, not frontend)
    //   * World: City17 (loaded when player starts game)
    //   * Content Packs: Environment features, cosmetics, optional modes
    // Performance: Boot strategy = ~80% faster startup, ~60% lower baseline memory vs loading all plugins
    //
    // WORLD ACTIVATION PATTERN:
    // - Menu world (Boot): Loaded at startup, ProjectMenuPlay GameMode manages menu features (ProjectLoading handles content)
    // - Gameplay world (OnDemand): Lazy-loaded when "Play" selected
    //   * World declares ProjectSinglePlay/ProjectOnlinePlay GameMode
    //   * GameMode BeginPlay uses ILoadingService to pull Combat/Dialogue/Inventory (OnDemand)

    // autolayout tb
}
