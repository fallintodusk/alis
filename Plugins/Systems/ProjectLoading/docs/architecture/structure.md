# Structure

## Purpose

ProjectLoading implements `ILoadingService` for experience resolution,
preloading, feature activation, travel, warmup, progress, cancellation, and
terminal results.

## Owns

- The active loading request and its terminal state.
- Experience-to-request resolution.
- Ordered phase execution, cancellation, and phase error propagation.
- Preload-handle lifetime through travel and cleanup.
- Loading progress and diagnostic data.

## Does not own

- Downloading, release selection, or installation.
- Widget creation or presentation, owned by
  [ProjectUI](../../../../UI/ProjectUI/README.md).
- Reusable World generation or runtime World policy, owned by
  [ProjectWorld](../../../../World/ProjectWorld/README.md).

## Composition

| Part | Responsibility |
|---|---|
| `UProjectLoadingSubsystem` | Request, state, events, and service lifecycle |
| Loading service adapter | Weak interface boundary for consumers |
| Initial experience loader | Descriptor and Asset Manager resolution |
| Pipeline orchestrator | Ordered phase execution and cancellation |
| Phase executors | Resolve, mount boundary, preload, activate, travel, warmup |
| `ProjectLoadingMoviePlayer` | Optional MoviePlayer-based presentation adapter |

## Relationships

Relationships are drawn once in [the main view](diagrams/main.md).

Through `ILoadingService`, callers submit requests and observe handles without
depending on the subsystem. Loading events expose progress and terminal results
to presentation owners. The frame-pump callback keeps Slate mechanics outside
the core loading module.

## Invariants

1. Every accepted request reaches exactly one terminal success, failure, or
   cancellation state.
2. Unknown or unsupported required work fails; it is not silently skipped.
3. Requested feature readiness is validated through `IOrchestratorRegistry`;
   ProjectLoading neither loads nor inspects modules directly.
4. Explicit runtime content-pack requests currently fail because mounting is
   unsupported; requests with no packs continue to the next phase.
5. Preloaded assets remain referenced through travel and are released on every
   terminal path and shutdown.
6. Asynchronous callbacks use weak owners and cannot complete an obsolete load.
7. ProjectLoading owns loading state; presentation owners only observe it.
8. ProjectLoading-authored travel carries the owner-only
   `ProjectLoadingRoute=1` option. Caller options cannot replace it, and a
   destination carrying it must not start the initial experience again. The
   [travel URL regression](../../Source/ProjectLoadingTests/Private/Unit/ProjectLoadingTravelURLTests.cpp)
   owns the executable URL example.

## Risks and technical debt

- `ProjectLoadingMoviePlayer` and ProjectUI both contain loading-presentation
  adapters. Supported runtime selection and removal criteria need separate
  evidence before either implementation is removed.
- ProjectUI directly depends on the ProjectLoading module for dynamic delegate
  subscription. Replacing that concrete edge requires a contract that preserves
  lifecycle and Blueprint event behavior.
