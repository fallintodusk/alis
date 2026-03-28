# ALIS - Open-Source Unreal Engine 5 Survival Game

Learn. Survive. Connect.

ALIS is an open-source Unreal Engine 5 survival game set in a post-apocalyptic real-world city. Scavenge, learn practical skills through gameplay, and survive in believable locations built from real geography.

This repository is the public code and documentation home for ALIS -- source code, architecture docs, build and release workflows, and modular plugin structure. Private infrastructure, licensed asset payloads, and local machine state are kept out of the public tree.

- Site: https://fall.is/
- Vision: [VISION.md](VISION.md)
- Social contract: [ALIS_PACT.md](ALIS_PACT.md)

## ALIS as an Unreal Engine architecture reference

ALIS is a public Unreal Engine project that exposes modular plugin source across boot, foundation, systems, user interface, gameplay, world, and feature layers.

The repository includes public architecture, data, build, testing, and workflow documentation, together with module-level readmes and automation paths that reflect the real project structure.

These materials provide a direct view of how the project is organized, how responsibilities are separated, and how technical decisions are represented in code and documentation.

## Gameplay and runtime systems

Concrete systems implemented as plugins in this repository:

- **Inventory** - server-authoritative inventory with equip, use, drop, weight, volume, and depth stacking
- **Vitals** - server-side metabolism tick, threshold states, condition regen and drain
- **Dialogue** - universal data-driven dialogue for NPCs, objects, and terminals
- **Mind** - inner-voice thought guidance system
- **Interaction** - player interaction with world objects via trace and prompt
- **Object Capabilities** - composable capability components: lockable, loot container, pickup
- **GAS** - Gameplay Ability System integration layer
- **Character** - base character plugin for gameplay modes
- **Motion** - skeleton-agnostic motion primitives
- **Settings** - typed settings persistence consumed by gameplay and UI
- **Loading** - 6-phase content loading pipeline with loading screens

Each system has its own plugin README with architecture and entry points.

## Why This Repo Is Distinct

The public codebase is organized around explicit technical decisions:
- C++ as the main systems and architecture layer
- text-first workflows for data, configuration, and many iteration paths
- JSON-driven definitions and layouts where binary authoring would slow automation
- universal generators instead of plugin-by-plugin custom authoring pipelines
- modular plugins with clear ownership boundaries
- contract-first integration across plugins
- signed release artifacts with public verification
- public documentation next to code wherever possible
- a public mirror strategy that supports long-term open and decentralized workflows

Architectural themes visible in this repo:
- boot and plugin lifecycle are separated from runtime content loading
- foundation plugins define contracts and shared infrastructure, not gameplay logic
- UI is framework-driven, JSON-driven, and descriptor-driven rather than hardcoded per screen
- the data pipeline is split intentionally into sync, generation, and propagation
- feature plugins are expected to remain optional, bounded, and soft-coupled
- world plugins own world data, while shared world code owns reusable tooling

## Public Repository Map

Top-level public structure:
- `Source/` - Unreal targets and shared game module entry points (`Alis`, `AlisClient`, `AlisEditor`, `AlisServer`)
- `Plugins/Boot/` - early boot and plugin lifecycle management
- `Plugins/Editor/` - editor-side generation, sync, and propagation tools for data-driven workflows
- `Plugins/Foundation/` - contracts, shared types, service location, low-level utilities
- `Plugins/Systems/` - reusable runtime services such as loading, save, settings, and motion systems
- `Plugins/UI/` - UI framework plus feature-facing UI plugins
- `Plugins/Gameplay/` - gameplay framework and shared runtime gameplay patterns
- `Plugins/Features/` - optional domain capabilities such as inventory, combat, and dialogue
- `Plugins/World/` - shared world tools and world-specific implementations
- `docs/` - public architecture, build, UI, testing, and reference docs
- `scripts/` - automation for build, test, release packaging, signing, verification, and mirroring

Plugin tier entry points:
- Boot tier: [Plugins/Boot/README.md](Plugins/Boot/README.md)
- Foundation tier: [Plugins/Foundation/README.md](Plugins/Foundation/README.md)
- Systems tier: [Plugins/Systems/README.md](Plugins/Systems/README.md)
- UI tier: [Plugins/UI/README.md](Plugins/UI/README.md)
- Gameplay framework: [Plugins/Gameplay/ProjectGameplay/README.md](Plugins/Gameplay/ProjectGameplay/README.md)
- Features tier: [Plugins/Features/README.md](Plugins/Features/README.md)
- World utilities: [Plugins/World/ProjectWorld/README.md](Plugins/World/ProjectWorld/README.md)

## Start Here

If you want the best overview first:
- Project-wide docs router: [docs/README.md](docs/README.md)
- Public build and release router: [docs/build/README.md](docs/build/README.md)
- Scripts and automation router: [scripts/README.md](scripts/README.md)

If you want architecture first:
- Architecture router: [docs/architecture/README.md](docs/architecture/README.md)
- Source-of-truth architecture map: [docs/architecture/source_of_truth.md](docs/architecture/source_of_truth.md)
- Data architecture and pipeline: [docs/data/README.md](docs/data/README.md)
- Boot lifecycle and orchestration: [Plugins/Boot/Orchestrator/README.md](Plugins/Boot/Orchestrator/README.md)
- Foundation contracts and shared services: [Plugins/Foundation/ProjectCore/README.md](Plugins/Foundation/ProjectCore/README.md)

If you want gameplay and UI entry points:
- UI router: [docs/ui/README.md](docs/ui/README.md)
- UI hot reload and JSON-first workflow: [docs/ui/hot_reload.md](docs/ui/hot_reload.md)
- UI framework plugin: [Plugins/UI/ProjectUI/README.md](Plugins/UI/ProjectUI/README.md)
- Gameplay framework: [Plugins/Gameplay/ProjectGameplay/README.md](Plugins/Gameplay/ProjectGameplay/README.md)
- Feature tier overview: [Plugins/Features/README.md](Plugins/Features/README.md)
- Inventory design vision: [Plugins/Features/ProjectInventory/docs/design_vision.md](Plugins/Features/ProjectInventory/docs/design_vision.md)

If you want generation and agent-friendly pipelines:
- Data architecture: [docs/data/README.md](docs/data/README.md)
- Universal JSON-to-DataAsset generation: [Plugins/Editor/ProjectDefinitionGenerator/README.md](Plugins/Editor/ProjectDefinitionGenerator/README.md)
- Definition propagation into placed actors: [Plugins/Editor/ProjectPlacementEditor/README.md](Plugins/Editor/ProjectPlacementEditor/README.md)
- Object definitions from JSON: [Plugins/Resources/ProjectObject/README.md](Plugins/Resources/ProjectObject/README.md)

If you want build, test, and release workflows:
- Build router: [docs/build/README.md](docs/build/README.md)
- Testing router: [docs/testing/README.md](docs/testing/README.md)
- Packaging, signing, and verification: [scripts/ue/package/README.md](scripts/ue/package/README.md)
- Public mirror flow: [scripts/git/mirror/README.md](scripts/git/mirror/README.md)

## What This Repository Contains

Published here:
- engine-side and gameplay C++ code
- modular plugin source across boot, foundation, systems, UI, gameplay, world, and feature layers
- public-facing architecture, build, testing, and workflow documentation
- build, test, packaging, signing, verification, and mirror automation
- text-based design and runtime data workflows
- the canonical `Alis.uproject`, so the public repo reflects the real plugin layout

Not published here:
- Unreal content asset payloads
- cooked builds and packaged binaries
- secrets, credentials, and private machine configuration
- non-redistributable third-party payloads

## Can I Run It?

If you want to run ALIS today, use the public release builds.

This public source tree is optimized for code, docs, review, and contribution flow. A full internal asset-complete checkout is still required for complete source-based runtime and release workflows.

## Release Trust

ALIS public releases are intended to be verifiable, not just downloadable.

Relevant entry points:
- Packaging guide: [docs/build/packaging_guide.md](docs/build/packaging_guide.md)
- Packaging scripts: [scripts/ue/package/README.md](scripts/ue/package/README.md)
- Trust page and public key: https://fall.is/trust/

## Contributing and Project Policy

Before contributing, read:
- Contribution guide: [CONTRIBUTING.md](CONTRIBUTING.md)
- License policy: [LICENSE](LICENSE)
- Project pact: [ALIS_PACT.md](ALIS_PACT.md)
- Trademark policy: [TRADEMARKS.md](TRADEMARKS.md)
