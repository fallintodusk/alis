# MindThought Test Failures

**Status:** DONE
**Priority:** Minor
**Date opened:** 2026-04-20
**Date closed:** 2026-04-20
**Surface:** `Plugins/Test/ProjectIntegrationTests/Source/ProjectIntegrationTests/Private/Integration/MindThoughtViewModelTest.cpp`

## Resolution summary

Both failures were test-side issues, not product regressions.

### 1. Dialogue.RecordResolutionIntegration (FIXED)

The test broadcast `CurrentNodeId = "thanks"` expecting it to resolve the `Record.Dialogue.Grandpa.NeedsWater` record, but the production `dialogue_thought_mappings.json` had no signal tag for `Dialogue.DLG_GrandPa_Entry.thanks`. The `thanks` node doesn't exist in `DLG_GrandPa_Entry.json` either, so adding a mapping for it would pollute production data with a signal that never fires.

**Fix:** Changed the test's second state change from `thanks` -> `open_door`. `open_door` is a real dialogue node that already has a mapping in `dialogue_thought_mappings.json` under the `Resolved` entry. The test now proves the same lifecycle (Active -> Resolved) using production data it is actually coupled to.

### 2. Scan.ToastStaysToast (FIXED)

`CollectIdleScanThoughts` logs showed `Iterated=14 TaggedActors=0` — our spawned `ScanTarget` was at `(0,0,0)` with `Tags=1` even though the test called `SetActorLocation((220,0,5000))`. Root cause: `AActor::StaticClass()` and `APawn::StaticClass()` spawn without a root scene component. Without a root, `SetActorLocation` silently leaves the actor at the origin. The scan iterator then dropped the fixture because 0cm distance collapses to "self" via `DistanceCm <= KINDA_SMALL_NUMBER`.

**Fix:** Added `AttachSceneRootToTestActor(Actor, FName)` helper in the test's anonymous namespace. Attaches a `USceneComponent` as root on both `TestPawn` and `ScanTarget` before `SetActorLocation`. Applied to both `Scan.ToastStaysToast` and `Scan.RequiresLineOfSight` (the LOS test "passed" only because its 0-thought expectation trivially matched the broken 0-thought emission; with the fix, the LOS assertion now means what it says).

### Verified

- `ProjectIntegrationTests.UI.HUD.MindThought.*` - 19/19 pass (was 17/19).
- No product code changed outside temporary diagnostics (removed after diagnosis).

---

## Failing tests

Observed during the inventory UI decouple sweep (slices 1-6b). Both failures existed **before** that work and are unrelated to inventory UI:

1. `ProjectIntegrationTests.UI.HUD.MindThought.Dialogue.RecordResolutionIntegration`
2. `ProjectIntegrationTests.UI.HUD.MindThought.Scan.ToastStaysToast`

---

## Failure details

### 1. Dialogue.RecordResolutionIntegration

File: `Plugins/Test/ProjectIntegrationTests/Source/ProjectIntegrationTests/Private/Integration/MindThoughtViewModelTest.cpp:489` and `:506`

Assertions that fire:
- `Dialogue lifecycle should emit at least active + resolved thoughts`
- `Dialogue completion should emit resolved record state`

Interpretation: the dialogue -> mind thought record integration is not emitting the active and resolved lifecycle events the test expects. Either the dialogue signal source is no longer publishing those transitions, or the mind thought record resolver is not recording them into the VM state the test observes.

### 2. Scan.ToastStaysToast

File: same, line 811.

Assertion that fires:
- `Custom scan rule should emit at least one thought`

Interpretation: the scan rule pipeline doesn't produce a thought when exercised with the test fixture. Either the custom scan rule is not registering, or the thought source is filtering it out.

Both failures live entirely inside the mind/thought plumbing and have no code path through inventory UI, drag-drop, or the decouple-related subsystems.

---

## What's known NOT to be the cause

- Inventory UI decouple (slices 1-6b of `inventory_ui_nearby_decouple.md`). The failures reproduce independently of that work; the affected files never import or interact with the inventory, nearby panel, drag host subsystem, or drop router.
- Data validation (`validate_all.py`) passes - no loot/definition JSON regression.

---

## Suggested investigation path

1. Read `MindThoughtViewModelTest.cpp` around lines 489, 506, 811 to capture the exact fixture setup.
2. Check recent commits on `Plugins/Gameplay/ProjectMind/**` and any dialogue <-> mind bridge (`Plugins/Features/ProjectDialogue/**` crossing into ProjectMind).
3. Run the two tests in isolation with `log LogProjectMind Verbose`, `log LogMindThought Verbose` to see whether thoughts are being emitted but filtered, or not emitted at all.
4. If this is caused by a deliberate contract change on ProjectMind (e.g. a new emitter policy), update the tests. Otherwise fix the emitter so the tests pass.

---

## Out of scope for this ticket

- Inventory UI work (handled in `inventory_ui_nearby_decouple.md`).
- The two **other** pre-existing failures previously triaged:
  - `ProjectIntegrationTests.InventoryLootPlaces.Content.LiveLootContainerDefinitionsCanonicalStorage` - content assertion that conflicts with per-level override design. Needs test relaxation, not data fix.
  - `ProjectIntegrationTests.InventoryLootPlaces.Session.InteractionHoldOpensWorldContainerSession` - panel-visibility timing during interaction-hold search.
  - Both live in separate paths; track separately if they become load-bearing.
