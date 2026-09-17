# Harden Loading Handle Lifetimes

Status: uninvestigated

## Problem

ProjectLoading's supported pipeline needs a focused lifecycle audit for async
handles and callbacks across success, failure, cancellation, travel, and
shutdown.

## Investigation boundary

- Trace every retained handle and callback owner in the six-phase pipeline.
- Put request-specific progress and terminal callbacks on the returned
  `ILoadingHandle`; keep subsystem-wide delegates only for observers that
  intentionally follow every load.
- Prove two sequential requests cannot deliver completion or failure to the
  other request's callbacks, and that each terminal callback fires once.
- Prove obsolete requests cannot complete into a destroyed or newer owner.
- Prove cleanup on every terminal path without weakening visible failures.
- Remove ProjectLoading's direct `FModuleManager` inspection from
  `ActivateFeatures`. Orchestrator owns plugin/module lifecycle and readiness;
  ProjectLoading retains ownership of the runtime feature-loading phase.
  Preserve fail-closed validation when a requested feature is unavailable
  without ProjectLoading loading or directly inspecting modules.
- Add a focused regression proving the phase neither loads modules nor depends
  directly on `FModuleManager`, while unavailable requested features still fail
  visibly.
- Add focused lifecycle regressions before changing production behavior.

Do not replace the existing loading pipeline without a demonstrated contract
failure.
