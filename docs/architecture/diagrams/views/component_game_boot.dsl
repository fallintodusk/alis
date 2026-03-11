// C3b - Component view: Game Boot Chain
// Shows Game Client boot: Launcher triggers Orchestrator Manager -> Plugin Activation

component project.projectClient {
    title "C3b - Game Boot Chain"

    // External systems
    include user

    // Launcher (for context - collapsed)
    include launcher.launcherApp

    // Orchestrator Manager (entry point - local operations only, NO downloads or verification)
    include project.projectClient.orchestrator
    include project.projectClient.orchestratorManifestParser
    include project.projectClient.orchestratorDependencyResolver
    include project.projectClient.orchestratorPluginRegistry
    include project.projectClient.orchestratorPluginLoader
    include project.projectClient.entryPointState

    // Minimal preload (foundation only)
    include project.projectClient.projectCore
    include project.projectClient.alisGI

    // Content loading pipeline (ProjectLoading plugin) represented by the subsystem + phases
    include project.projectClient.projectLoadingSubsystem
    include project.projectClient.resolveAssetsPhase
    include project.projectClient.mountContentPhase
    include project.projectClient.preloadCriticalAssetsPhase
    include project.projectClient.activateFeaturesPhase
    include project.projectClient.travelPhase
    include project.projectClient.warmupPhase

    // Entry point (activated by Orchestrator after loading its dependency closure)
    include project.projectClient.mainMenuWorld

    // ProjectLoading 6-phase ILoadingService pipeline (explicit blocks in this view):
    // 1) ResolveAssets: Validate FLoadRequest (MapAssetId or MapSoftPath), resolve map/experience, log feature list, allow soft-path fallback.
    // 2) MountContent: Mount IoStore/content packs when provided; Orchestrator is code-only (DLL load), content mounting is owned by ProjectLoading.
    // 3) PreloadCriticalAssets: Stream map/experience assets via StreamableManager with progress, 60s timeout, cancellation.
    // 4) ActivateFeatures: Validate requested feature modules are loaded (delegated to Orchestrator when enabled); fails if missing.
    // 5) Travel: Resolve map package path (AssetManager first, soft-path fallback), build travel URL with custom options, call ServerTravel.
    // 6) Warmup: Optional shader precompile + streaming level precache to reduce hitching; hands off to entry world BeginPlay (MainMenuWorld).

    // Relationships auto-rendered from model_plugin_relationships.dsl
    // Shows boot flow: Launcher starts UE -> Orchestrator Manager -> loads preload (ProjectCore) -> resolves entryPoint dependencies transitively.
    // ProjectLoading reads entry point at runtime (target: manifest/state; current code uses config-driven experience registry).
    // manifest.json drives behavior: { "preload": ["ProjectCore"], "entryPoint": "MainMenuWorld" } with DLLs loaded at PostConfigInit, content mounted post-engine-init by ProjectLoading
    // DependencyResolver auto-loads MainMenuWorld dependencies: ProjectMenuPlay -> ProjectMenuMain -> ProjectSettingsUI -> ProjectUI (not shown in view, loaded automatically)

    // autolayout lr
}
