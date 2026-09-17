# Harden Loading Handle Lifetimes

Status: investigated; handle-lifecycle implementation pending

## Problem

ProjectLoading's supported pipeline needs a focused lifecycle audit for async
handles and callbacks across success, failure, cancellation, travel, and
shutdown.

The returned handle currently supports queries and cancellation even though
its public contract also claims request-owned completion subscription. Current
global delegate consumers are presentation adapters that intentionally observe
every load; they are not evidence that request-owned callbacks can be omitted.

## Investigation boundary

- Trace every retained handle and callback owner in the six-phase pipeline.
- Put request-specific progress and terminal callbacks on the returned
  `ILoadingHandle`; keep subsystem-wide delegates only for observers that
  intentionally follow every load.
- Prove two sequential requests cannot deliver completion or failure to the
  other request's callbacks, and that each terminal callback fires once.
- Prove obsolete requests cannot complete into a destroyed or newer owner.
- Prove cleanup on every terminal path without weakening visible failures.
- Add focused lifecycle regressions before changing production behavior.

Do not replace the existing loading pipeline without a demonstrated contract
failure.
