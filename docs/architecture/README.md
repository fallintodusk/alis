# Architecture

Source of truth: see [source_of_truth.md](source_of_truth.md) and [plugin_architecture.md](plugin_architecture.md).

Agent note: this is the canonical architecture router; use this instead of duplicating in agent docs.

Purpose
- Quick routing to core architecture topics and diagrams.
- KISS: legacy GameFeatures content and content pack bundling removed.

Sections
- C4 Diagrams: [diagrams/workspace.dsl](diagrams/workspace.dsl) (views in [diagrams/views/](diagrams/views/))
- Plugin Architecture: [plugin_architecture.md](plugin_architecture.md) (Authoritative Plugin Layout)
- Launcher Boot Chain: [boot_chain.md](../loading/boot_chain.md) (Launcher -> Orchestrator)
- Loading Pipeline: [../systems/loading_pipeline.md](../systems/loading_pipeline.md)
- UI/MVVM/Theme: [../ui/README.md](../ui/README.md) (routes to ProjectUI plugin docs)

Plugin System (Authoritative)
- Boot (Plugins/Boot/): Orchestrator
- Foundation (Plugins/Foundation/): ProjectCore
- Systems (Plugins/Systems/): ProjectLoading, ProjectSave, ProjectSettings
- World (Plugins/World/): ProjectWorld, City17, MainMenuWorld
- World/PCG (Plugins/World/PCG/): ProjectPCG, ProjectForestBiomesPack, ProjectUrbanRuinsPCGRecipe
- UI (Plugins/UI/): ProjectUI (framework + extension registry), ProjectHUD (slots), ProjectVitalsUI, ProjectMenuMain, ProjectMenuGame, ProjectSettingsUI
- Gameplay (Plugins/Gameplay/): ProjectGameplay, ProjectMenuPlay, ProjectSinglePlay, ProjectOnlinePlay, ProjectGAS, ProjectCharacter, ProjectVitals
- Features (Plugins/Features/): ProjectFeature (contracts), ProjectCombat, ProjectDialogue, ProjectInventory

Launcher Boot Chain (Summary)
- Launcher verifies manifest/payloads, installs/updates orchestrator if needed, passes IPC context to the game, exits.
- Orchestrator checks and applies plugin updates at app start (boot) before gameplay modules load; resolve deps, stage/apply, activate (install-before-load).

Vitals/GAS/Character Architecture: [../gameplay/character.md](../gameplay/character.md)

Diagrams
- Workspace: [diagrams/workspace.dsl](diagrams/workspace.dsl)
- Focused views: context, container_static_deps, container_update_flow, container_boot_loading, container_plugins, component_client, component_orchestrator, dynamic_*.

External Tools (Rust)
- Build Service: [../../tools/build_service/README.md](../../tools/build_service/README.md)
- Build Service Architecture: [../tools/build_service/architecture.md](../tools/build_service/architecture.md)
- Build Service Diagram: [diagrams/views/component_build_service.dsl](diagrams/views/component_build_service.dsl)
- Launcher: [../../tools/launcher/README.md](../../tools/launcher/README.md)
- Launcher Architecture: [../tools/launcher/architecture.md](../tools/launcher/architecture.md)
- Launcher Diagram: [diagrams/views/component_launcher_boot.dsl](diagrams/views/component_launcher_boot.dsl)
- Build Service Integration: [../integration/BUILD_SERVICE_INTEGRATION.md](../integration/BUILD_SERVICE_INTEGRATION.md)
- Launcher Integration: [../integration/LAUNCHER_INTEGRATION.md](../integration/LAUNCHER_INTEGRATION.md)

Notes
- Features are managed per-plugin (molecular updates). Bundling is not used.
- GameFeatures naming is deprecated in docs; we use Features and a project-owned Feature Activation API.
