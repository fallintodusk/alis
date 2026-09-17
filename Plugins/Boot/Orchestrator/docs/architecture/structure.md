# Structure

## Purpose

Orchestrator runs at `PostConfigInit` to materialize its current local boot
context, validate manifest inputs, order plugin dependencies, and register/load
selected Unreal modules before normal game lifecycles begin.

## Owns

- Early-boot manifest parsing and compatibility validation.
- Dependency ordering and cycle rejection.
- Unreal plugin registration and module activation.
- Orchestrator-owned activation state and failure reporting.

## Does not own

- The supported public release, signature, and installation workflow, owned by
  [release packaging](../../../../../scripts/ue/package/README.md).
- External Launcher transport, which is not implemented in this repository;
  see the [Launcher boundary](../../../../../tools/Launcher/README.md).
- Runtime map, asset, and feature loading, owned by
  [ProjectLoading](../../../../Systems/ProjectLoading/README.md).
- Loading or boot presentation, owned by
  [ProjectUI](../../../../UI/ProjectUI/README.md).

## Composition

| Part | Responsibility |
|---|---|
| `OrchestratorCore` | Early runtime lifecycle and activation |
| Manifest parser | Installed manifest schema and compatibility checks |
| State owner | Orchestrator activation checkpoints |
| Dependency registry | Ordering, cycles, and feature-module registration |
| Plugin loader | Unreal plugin discovery and module loading |
| `OrchestratorEditor` | Editor-only integration |
| `OrchestratorTests` | Component automation |

## Relationships

Relationships are drawn once in [the main view](diagrams/main.md).

Orchestrator currently materializes its own local development context from the
project directory, `Orchestrator/Data/dev_manifest.json`, and the running engine
build identity. No external Launcher-selected release enters this path. Through
ProjectCore feature contracts, Orchestrator activates modules without depending
on feature internals.

## Invariants

1. Early boot does not create UI or require a World.
2. Invalid manifests, incompatible engine identity, dependency cycles, and
   missing required modules fail visibly.
3. Runtime content loading is not reported as successful by Orchestrator.
4. Feature activation follows the validated dependency order.

## Risks and technical debt

- `ApplyHotUpdates()` currently calls Orchestrator-owned download/extraction
  helpers during startup. Removing that behavior requires a separate change
  proving Launcher selection, last-known-good recovery, and activation failure.
- The public `OrchestratorAPI` remains a compatibility surface. Its real
  consumers must be traced before removal.
- `FOrchestratorIoStore` is present but does not establish a supported runtime
  content-mounting route.
