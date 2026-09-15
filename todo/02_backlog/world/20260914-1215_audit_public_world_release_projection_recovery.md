# Audit Public World Release Projection Recovery

Status: BACKLOG - after the 2.0.0 release
Priority: Follow-up reliability
Created: 2026-09-14 12:15 Europe/Moscow

## Goal

Determine the smallest safe recovery boundary for the release-only public World
projection before the first post-2.0 release input generation. Do not reopen the
2.0.0 release solely for this investigation.

## Current evidence

- `scripts/ue/package/public_world_projection.ps1` snapshots and temporarily
  replaces live ProjectWorldData generated content, then restores it in
  `finally`.
- Each child realization owns its normal transaction, but the outer two-world
  projection does not currently hold the project-global generated-content lock
  or own a durable interruption-recovery journal.
- `scripts/ue/package/tests/test_public_world_projection.ps1` proves digest
  sensitivity and routing only. It does not exercise concurrent mutation or
  forced-process interruption.
- The prior 2.0.0 preparation restored the private generated tree. This audit
  was therefore deferred and is not a 2.0.0 release blocker.

## Investigation

- [ ] Reproduce the concurrent-operation and forced-interruption boundaries in
  isolated ProjectWorldTestData or disposable project-local fixtures.
- [ ] Compare an isolated-checkout projection with the existing outer lock-token
  delegation and transaction-journal mechanisms.
- [ ] Prefer isolated generation if it avoids mutating the private working tree
  without duplicating World authority; otherwise define one recoverable outer
  transaction owner.
- [ ] Add regression evidence that a concurrent generator cannot interleave,
  interruption has a documented recovery path, and successful completion
  restores every protected byte.
- [ ] Update the World realization/release SOT only after the implementation is
  proven.

## Non-goals

- No World architecture redesign.
- No change to the accepted 2.0.0 release bytes or signing sequence.
- No WSL release enablement; native Windows remains the accepted release route.
