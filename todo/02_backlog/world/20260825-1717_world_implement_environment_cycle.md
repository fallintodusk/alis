# Implement Generated-World Environment Cycle

Status: uninvestigated

## Current gap

Generated territories have deterministic static presentation but no selected
runtime day/night provider. ProjectWorld owns reusable presentation semantics;
each territory data plugin owns concrete values.

## Investigation boundary

- Select a reusable provider only when runtime day/night is prioritized.
- Preserve deterministic static presentation as the control and failure mode.
- Keep geography, generated-package identity, streaming, and performance
  budgets unchanged.
- Prove Kazan as the first fixture without adding a Kazan branch to reusable
  code.

Start from [ProjectWorld architecture](../../../Plugins/World/ProjectWorld/docs/architecture/README.md).
