# Implement Launcher Boundary

Status: uninvestigated

## Problem

ALIS has a reserved `FLauncherContext`, but no Launcher executable or accepted
transport currently supplies it. Orchestrator always constructs a local
development context.

## Investigation boundary

- Choose the repository and owner for the Launcher executable.
- Define the signed manifest and installed-artifact contract once with the
  release/CDN owners.
- Define a secret-safe game handoff and its lifetime; never pass credentials on
  a command line or store them in logs.
- Specify last-known-good recovery and interruption behavior before removing
  Orchestrator delivery helpers.
- Prove first install, update, no-op, corrupted payload, incompatible engine,
  interrupted promotion, and activation failure through the real executable.
- Update Orchestrator only after the external route supplies equivalent or
  stronger recovery evidence.

Do not treat `FLauncherContext` fields or design prose as implementation
evidence.
