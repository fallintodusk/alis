# Launcher Integration

## Current Status

External Launcher handoff is not implemented in this repository. The current
game-side input is owned and documented by
[Orchestrator architecture](../../Plugins/Boot/Orchestrator/docs/architecture/README.md);
reserved context fields do not establish an IPC or trust boundary.

## Boundary

Orchestrator owns current manifest validation, dependency ordering, and module
activation. Public release packaging owns the supported downloadable build and
developer payload. The external Launcher executable and transport are not
implemented in this repository.

See [Launcher architecture](../../tools/Launcher/docs/architecture.md) for the
unimplemented external side of this boundary.
