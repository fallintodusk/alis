Boot Tier

Purpose
- Orchestrator (base plugin, loaded early at PostConfigInit phase) manages plugin lifecycle.
- Launcher (external Rust app) handles downloads/verification before starting game.

Scope
- Launcher: Install path selection, manifest download/verification, plugin downloads, payload verification, starts game with IPC context.
- Orchestrator: Reads Launcher IPC context, applies pending updates from previous session, resolves dependencies, mounts/loads plugins in dependency order.

Non-responsibilities
- Launcher: NO plugin activation (Orchestrator does this), NO engine integration.
- Orchestrator: NO downloads/verification (Launcher already did this), NO gameplay logic, NO hard-coded feature knowledge (data-driven by manifest).

Key entry points
- [Orchestrator](Orchestrator/README.md) - early boot lifecycle manager
- [Architecture Router](../../docs/architecture/README.md) - public architecture overview
