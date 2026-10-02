# Current Boot Chain

## Runtime Flow

1. Unreal loads Orchestrator at `PostConfigInit`.
2. Orchestrator completes its documented local boot and module-activation
   lifecycle.
3. Normal game, loading, UI, and World lifecycles continue through their own
   component owners.

An external Launcher-to-game IPC step is not part of the current implemented
chain. See [Launcher integration](launcher_integration.md).

## Owners

- [Orchestrator](../../Plugins/Boot/Orchestrator/README.md) - early manifest and
  module lifecycle.
- [ProjectLoading](../../Plugins/Systems/ProjectLoading/README.md) - runtime
  experience loading after boot.
- [ProjectUI](../../Plugins/UI/ProjectUI/README.md) - user-facing presentation.
- [Launcher boundary](../../tools/Launcher/docs/architecture.md) - missing
  external handoff scope.

## Diagrams

- [Project architecture](../architecture/diagrams/main.md)
- [Orchestrator architecture](../../Plugins/Boot/Orchestrator/docs/architecture/diagrams/main.md)
- [ProjectLoading architecture](../../Plugins/Systems/ProjectLoading/docs/architecture/diagrams/main.md)
