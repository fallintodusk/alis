# InteractionHoldOpensWorldContainerSession - Mid-Search Timing Flake

**Status:** Fixed 2026-04-22 (warm-editor run residual)
**Priority:** Minor
**Date opened:** 2026-04-21
**Date fixed:** 2026-04-22
**Surface:**
- Test: `Plugins/Test/ProjectIntegrationTests/Source/ProjectIntegrationTests/Private/Integration/InventoryLootPlacesIntegrationTest.cpp` (FInventoryLootPlaces_InteractionHoldOpensWorldContainerSessionTest, lines ~3397-3658)
- Production: `Plugins/Gameplay/ProjectInteraction/Source/ProjectInteraction/Private/InteractionComponent.cpp`

## Symptom

Test fails deterministically on 4 mid-search assertions (all at lines 3602-3605):

```
Expected 'Real hold interaction should remain in progress mid-search' to be true.
Expected 'Real hold interaction should advance progress mid-search' to be true.
Expected 'Real hold interaction should show active Searching label mid-search' to be "Searching...", but it was "Search".
Expected 'Inventory view model must stay hidden while timed search is still in progress' to be false.
```

All downstream assertions (completion state, session-open, panel visibility after completion) PASS - so the
hold interaction pipeline + session-open + panel-visibility-on-completion are all functional.

## Root cause (diagnosed, not fixed)

The test's "mid-search" loop ticks 15 iterations at 0.1s each (1.5 simulated seconds), then
checks that the hold is still in progress. But `FocusedExecutionSpec.DurationSeconds = 1.0s`
means `CompleteHoldInteraction` fires at simulated time 1.0s (iteration 10 of the 15), before
the mid-search check runs.

Production log during failure confirms (same simulated frame [682]):
```
BeginInteractInput: Started timed interaction 'Search' (Duration=1.00s)
CompleteHoldInteraction: Timed interaction completed
TryInteract: Has authority, executing locally
```

Production code (`InteractionComponent::TickComponent` around line 290) computes progress as
`(World->GetTimeSeconds() - HoldInteractionStartTime) / HoldDuration` and fires completion
when progress >= 1.0. This is correct production behavior; the test setup is wrong.

## Fix sketches (deferred)

Three viable options, any of which would restore green:

1. **Reduce pre-check loop to ~5 iterations** (0.5 simulated seconds, ~50% progress). Then
   keep the existing post-check loop for completion. Simplest, minimal change.
2. **Make the test tick only a small amount before checking mid-state** and increase
   `FocusedExecutionSpec.DurationSeconds` via `MakeLootContainerJson` template to e.g. 2.0s.
3. **Rewrite the test to drive hold via explicit world-time injection** so it doesn't rely
   on loop count vs. duration arithmetic.

None of these are urgent: the production code path IS covered by the downstream assertions
that pass today (session opens correctly after hold completes, view model becomes visible).

## Fix applied (2026-04-22)

- Pre-check loop count reduced from 15 to 5 iterations in
  `InventoryLootPlacesIntegrationTest.cpp` around line 3606. 5 * 0.1s = 0.5s
  simulated, ~50% of the 1.0s hold duration, so the mid-hold assertions run
  before `CompleteHoldInteraction` fires.
- The four `AddExpectedError` quarantine entries at the top of `RunTest`
  were removed in the same edit. If the fix regresses, the test will fail
  with exactly those four specific messages again - built-in sabotage signal.
- Leading comment rewritten to explain the new invariant and log the history.

### Warm-editor verification (residual)

The fix is a pure loop-count change + quarantine removal. Requires the next
warm-editor session to run:
```
.\scripts\ue\test\unit\iterate.ps1 \
  -TestFilter "ProjectIntegrationTests.InventoryLootPlaces.Session.InteractionHoldOpensWorldContainerSession" \
  -Mode Dev
```
Expected: PASS. If FAIL, the four mid-hold assertion messages will name the
precise regression; roll the loop count back to 15 and restore the quarantine
if the production code has shifted timing meanwhile.

## Current status in test source

Quarantine removed. Test now asserts mid-hold state directly. See fix notes
above for rollback protocol if needed.

## Context

- Flagged as pre-existing (out of scope) in `todo/00_current/inventory_ui_nearby_decouple.md`
  Non-goals section.
- Also referenced in `todo/00_current/mindthought_test_failures.md` Out-of-scope section.
- Triage: 2026-04-21 (this doc) + green-verified `LiveLootContainerDefinitionsCanonicalStorage`
  as a content-data stale-test fix in the same session.
