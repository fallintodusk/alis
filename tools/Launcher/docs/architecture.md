# Launcher Boundary

## Current Repository State

This repository contains no Launcher executable, Cargo workspace, or Launcher
test implementation. It owns only the intended external boundary with ALIS.
The [game-side integration owner](../../../Plugins/Boot/Orchestrator/docs/README.md)
documents the input that exists today.

## Ownership

| Concern | Current owner |
|---|---|
| Local boot context and manifest processing | [Orchestrator](../../../Plugins/Boot/Orchestrator/README.md) |
| Cross-boundary integration status | [Launcher integration](../../../docs/loading/launcher_integration.md) |
| Public player/developer release installation | [Release packaging](../../../docs/build/packaging_guide.md) |
| Launcher implementation and delivery acceptance | Not implemented in this repository |

The public GitHub release route is independent of an unimplemented Launcher.
Stable ALIS documentation must not claim download, authentication, IPC,
rollback, or update behavior until an executable owner and real acceptance
route prove it.
