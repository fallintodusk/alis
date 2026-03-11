# Architecture

Public architecture router for ALIS.

This is the best entry point if you want to understand how the repository is structured, how plugin boundaries are enforced, and where the main system responsibilities live.

## Start Here

- Architecture source of truth: [source_of_truth.md](source_of_truth.md)
- Core principles: [principles.md](principles.md)
- UI router: [../ui/README.md](../ui/README.md)
- Data architecture: [../data/README.md](../data/README.md)
- Build router: [../build/README.md](../build/README.md)

## Diagrams

- Workspace DSL: [diagrams/workspace.dsl](diagrams/workspace.dsl)
- Focused views: [diagrams/views/](diagrams/views/)

## Plugin Tier Routers

- Boot: [../../Plugins/Boot/README.md](../../Plugins/Boot/README.md)
- Foundation: [../../Plugins/Foundation/README.md](../../Plugins/Foundation/README.md)
- Systems: [../../Plugins/Systems/README.md](../../Plugins/Systems/README.md)
- UI: [../../Plugins/UI/README.md](../../Plugins/UI/README.md)
- Gameplay framework: [../../Plugins/Gameplay/ProjectGameplay/README.md](../../Plugins/Gameplay/ProjectGameplay/README.md)
- Features: [../../Plugins/Features/README.md](../../Plugins/Features/README.md)
- World: [../../Plugins/World/ProjectWorld/README.md](../../Plugins/World/ProjectWorld/README.md)

## Key System Entry Points

- Boot lifecycle and plugin activation: [../../Plugins/Boot/Orchestrator/README.md](../../Plugins/Boot/Orchestrator/README.md)
- Foundation contracts and service layer: [../../Plugins/Foundation/ProjectCore/README.md](../../Plugins/Foundation/ProjectCore/README.md)
- UI framework and descriptor-driven rendering: [../../Plugins/UI/ProjectUI/README.md](../../Plugins/UI/ProjectUI/README.md)
- Gameplay framework: [../../Plugins/Gameplay/ProjectGameplay/README.md](../../Plugins/Gameplay/ProjectGameplay/README.md)
- World data and shared world tooling: [../../Plugins/World/ProjectWorld/README.md](../../Plugins/World/ProjectWorld/README.md)

## Public Architecture Themes

The public codebase is organized around a few recurring patterns:
- plugin ownership over internal implementation
- contract-first boundaries across plugins
- boot lifecycle separated from runtime content loading
- text-first and generator-backed data workflows
- descriptor-driven UI instead of screen-specific hardcoding
- soft-coupled feature modules

## Notes

- Some older internal architecture references are intentionally not routed from here because they are not part of the public mirror.
- When in doubt, prefer plugin-level READMEs over stale cross-repo architecture notes.
