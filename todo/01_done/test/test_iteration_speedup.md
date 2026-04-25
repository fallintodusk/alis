# Test Iteration Speedup Plan

**Status:** Planned
**Priority:** Major
**Date:** 2026-04-22
**Supersedes:** `test_speedup_automationdriver.md` (renamed/rewritten — scope is broader than AutomationDriver)

---

## TL;DR

The main drag is not Unreal Engine itself. The main drag is running broad union filters after small changes.

The fastest win is not new engine tech. The fastest win is:

1. exact single-test dispatch by default
2. script-level refusal of broad filters in dev mode
3. Live Coding only for `cpp` body edits
4. union-filter runs only at end-of-slice or when explicitly requested
5. Low Level Tests only after we prove Tier 1 is not enough

AutomationDriver stays in the plan, but as a **correctness** track, not a **speed** track.

---

## Session handoff context

This todo is designed to be picked up by a fresh agent session. Everything needed to start is in this file; cross-refs are listed at the bottom.

### Why this plan exists
Phase 1.5 (split `W_InventoryPanel.cpp` + `InventoryViewModel.cpp`, interface-based drop routing, subsystem-owned drag host) landed, and during that work the single biggest time sink was **the agent reflexively running `ProjectIntegrationTests.UI` (or similar union filters) after every small change**. Each such run is ~20-30s warm (empirically 24s for 83 tests on 2026-04-23; original estimate of 60-70s was pessimistic), much longer cold. The right test set for a single-function edit is usually 1-3 specific tests, not a suite - and the waste adds up across iterations even at the corrected number.

The root cause analysis is explicit: **primary cause is agent behavior, not UE tooling**. UE 5.7 already supports everything we need (exact filters, Live Coding, tag filters, Low Level Tests). We just have not been using them.

### What "done" looks like
A code change results in:
1. editor stays warm (persistent editor trio: start/run/stop PowerShell wrappers)
2. compile is Live Coding when compatible (`cpp` body only)
3. only the affected exact test runs
4. broad filters require an explicit override path

### Rules for the fresh agent
- Do NOT bundle phases. Finish Phase 0 before starting Phase 1.
- Do NOT run broad filters while validating your own work. Test the specific guardrail you just added.
- Do NOT invent new dependencies (new modules, new plugins, new UE versions). The engine already does this.
- Measure. See the metrics section. A plan without numbers is not complete.
- Update `docs/agents/canonical.md` whenever a decision changes — it is the first-read SOT.

---

## Autonomous overnight mode

When this todo is executed without a human in the loop, the operating
rulebook is [docs/agents/overnight_mode.md](../../docs/agents/overnight_mode.md).
Follow it for commit policy, subagent orchestration, token discipline,
watcher cadence, and the mandatory reuse-check before any implementation.

---

## Problem

Common iteration is slower than it needs to be because the workflow keeps paying for broad automation dispatch when the code change usually affects only 1-3 tests.

Typical waste pattern:
- edit one file
- run `ProjectIntegrationTests.UI`
- wait ~20-30s (empirical, 2026-04-23)
- repeat

That is usually unnecessary. Unreal Engine already supports exact test filters, persistent editor reuse, Live Coding for body-only edits, and standalone Low Level Tests for pure logic.

The current bottleneck is mostly workflow discipline and lack of guardrails, not missing engine features.

---

## What is actually causing the slowdown

### Primary cause
Broad union filters are being used reflexively after small changes.

### Secondary causes
- no script-level guardrail that rejects broad filters in dev mode
- no single canonical wrapper for "edit -> compile -> run one test"
- no fast path selection between Live Coding and full compile
- too many pure-logic checks still live only in editor automation

### Non-causes
- AutomationDriver is **not** the speed fix
- AFunctionalTest is **not** the first thing to do
- Git-diff test selection is **not** the first thing to do

---

## Principles

1. **Behavior first, tooling second.**
   Stop the waste before adding new machinery.

2. **Smallest safe verification wins.**
   Default to the narrowest exact test that covers the changed code.

3. **Live Coding is an accelerator, not the source of truth.**
   Use it only when the change shape is compatible.

4. **Correctness and speed are separate tracks.**
   AutomationDriver improves routing confidence. It does not materially reduce loop time by itself.

5. **Enforce with scripts, not just prose.**
   Wrong agent behavior must fail fast.

---

## Verified UE 5.7 capabilities

Verified against `<ue-path>/` at time of writing. Paths are stable across 5.x; only line numbers drift. If a path no longer resolves, grep the symbol name and update this table in the same PR.

| Capability | Works? | Entry point | Notes |
|---|---|---|---|
| Exact test filter | YES | `Automation RunTests <FullName>` | `Engine/Source/Developer/AutomationController/Private/AutomationCommandline.cpp` (~line 307-597). Parses `;`-separated list. One full name = one test. |
| Live Coding (cpp bodies) | YES | Console `LiveCoding.CompileSync` or Ctrl+Alt+F11 in editor | Rejects header/reflection/module changes. Must fall back to full compile cleanly. |
| Tag filter | YES | `Automation SetTagFilter "<expr>"` then `Automation RunTests Group:Tagged` | Boolean expr: `Fast && !Slow`, `Inventory && !E2E`. Tests opt in via `GetTestTags()`. |
| Failed-rerun (built-in) | NO | n/a | Must be reconstructed from log scraping. Automation does not persist a failed-set between invocations. |
| Persistent editor file-IPC | YES (already in repo) | `scripts/ue/test/unit/persistent_editor_{start,run,stop}.ps1` | `ProjectIntegrationTestsModule` watches `command.txt`. Warm-editor cost ~4x better than cold. |
| Low Level Tests (LLT) | YES | `Engine/Source/Developer/LowLevelTestsRunner/` | Catch2-based. Standalone exe. Sub-second. For pure-logic code only (no UObject / no editor). |
| AutomationDriver | YES | `IAutomationDriverModule::Get().CreateDriver()` | `Engine/Source/Developer/AutomationDriver/Public/IAutomationDriver.h`. Selenium-style. Hooks the platform message handler. Correctness, not speed. |
| AFunctionalTest | YES | Map-based `AFunctionalTest`-derived actors | Useful for world-resident multi-test flows. Deferred for this todo. |

### Non-features to avoid assuming
- There is no built-in "rerun only failed tests" flow — we build it ourselves by parsing log sentinels.
- There is no git-diff-based auto-selection — deferred, lower ROI than enforcement.
- Unity-build tuning is not a first-wave optimization here. Revisit only if measurements prove compile parsing, not reload/dispatch, is the real bottleneck.

---

## Explicit decisions

### Decision 1 - default test mode
Default dev loop = **one exact test filter**.

Not allowed by default in dev mode:
- `ProjectIntegrationTests.UI`
- wildcard filters
- prefix filters
- comma unions
- `Group:` filters
- broad tag filters

Broad filters are allowed only when:
- user explicitly asks
- end-of-slice verification
- CI / nightly
- local gate run with an explicit override flag

### Decision 2 - compile mode
- `cpp` body-only edit -> prefer Live Coding if persistent editor is alive
- any header, reflected type, `.Build.cs`, module dependency, or generated-code-sensitive change -> full compile path
- if Live Coding fails -> fall back to full compile automatically

### Decision 3 - rollout order
Do not bundle everything into one "2-day sprint" blob.
Use this order:

1. Phase 0 - immediate guardrails
2. Phase 1 - Live Coding wrapper
3. Phase 2 - focused tag filters
4. Phase 3 - failed-tests rerun helper
5. Phase 4 - Low Level Test pilot
6. Separate correctness track - AutomationDriver

### Decision 4 - rename
The previous filename was too narrow. AutomationDriver is now only one workstream, and the real subject is the full developer iteration loop.

---

## Current baseline state (what already exists)

Write the plan honest to current state — do not pretend future cleanup already happened.

### Already landed (from Phase 1.5 + earlier)
- `docs/agents/canonical.md` exists as the first-read SOT; "Dev Loop Contract" section added in Phase 0.
- `scripts/ue/test/unit/persistent_editor_start.ps1` / `persistent_editor_run.ps1` / `persistent_editor_stop.ps1` exist — file-IPC via `command.txt` polled by `ProjectIntegrationTestsModule`. Cold-to-warm speedup already exists.
- `UInventoryViewModelSpy` implements `IInventoryDropCommandTarget` for VM-dispatch assertions.
- `FInventoryDropRouter::Route` returns `FInventoryDropRouteResolution` carrying the real `VMMethod` name — events no longer predict, they report.

### Landed in Phase 0 (2026-04-22)
- `docs/agents/canonical.md` section 7 now opens with "Dev Loop Contract (dev-mode default)". Maintenance contract extended to trigger on changes to `scripts/ue/test/unit/{run_single,iterate,run_cpp_tests_safe}.ps1`.
- `AGENTS.md` now carries a "Dev Loop Rule (CRITICAL!)" block as the first entry under CRITICAL Rules.
- `scripts/ue/test/unit/Test-FilterShape.ps1` - shared filter-shape validator + rejection-message printer. Single source of truth for what "broad" means and what the rejection contract prints. Hardened against long-prefix bypass: after segment-count passes, requires the filter to appear as a complete quoted literal in `Plugins/Test/**/*.{cpp,h}`. A 4-segment prefix like `ProjectIntegrationTests.UI.Framework.Inventory` has enough dots but is never a real test ID, so the source-scan rejects it. Blob cached per-process.
- `scripts/ue/test/unit/run_single.ps1` - strict exact-only wrapper. Rejection routes users to `iterate.ps1` / `run_cpp_tests_safe.ps1 -Mode Gate` for overrides.
- `scripts/ue/test/unit/run_cpp_tests_safe.ps1` - extended with `-Mode Dev|Gate` (default Dev) and `-AllowBroadFilter`. Dev + no override -> shape-gate refuses broad filters.
- `scripts/ue/test/unit/iterate.ps1` - default dev entrypoint. `-Mode Dev|Gate`, `-AllowBroadFilter`, `-CompileMode Auto|LiveCoding|Full` (CompileMode informational until Phase 1). Prints chosen-path banner: filter shape, mode, broad override, dispatch (warm/cold), compile mode.

### Baseline verification rule
Before executing any phase, grep and confirm the current repo state. Do not trust the bullets above as-is; re-verify against the repo.

Checks to run (fresh agent, Phase 0 entry):
- persistent editor scripts still present:
  `ls scripts/ue/test/unit/persistent_editor_*.ps1`
- whether `W_InventoryPanel.h` (and `.cpp`) still exposes accessor-bloom helpers:
  `grep -nE "Get[A-Za-z0-9_]*(ReadOnly|ForRegistry|ForHelper)" Plugins/UI/ProjectInventoryUI/Source/ProjectInventoryUI/Public/Widgets/W_InventoryPanel.h Plugins/UI/ProjectInventoryUI/Source/ProjectInventoryUI/Private/Widgets/W_InventoryPanel.cpp`
  (zero matches = clean; any match = record actual state here before proceeding)
- whether any dev-loop guardrails already landed:
  `grep -n "Mode Dev|Gate" scripts/ue/test/unit/run_cpp_tests_safe.ps1`
  `grep -n "AllowBroadFilter" scripts/ue/test/unit/*.ps1`
- whether automation-tag plumbing already exists in the current test base:
  `grep -rn "REGISTER_SIMPLE_AUTOMATION_TEST_TAGS\|FAutomationTestTags" Plugins/Test/ProjectIntegrationTests/Source/`

If any of the above changed from what this file claims, update the baseline section first, then continue the phase.

### NOT YET done (this todo's scope)
- No tests are tagged via `REGISTER_SIMPLE_AUTOMATION_TEST_TAGS` (Phase 2).
- No Live Coding integration in `iterate.ps1` yet; `-CompileMode` is informational (Phase 1).
- `ProjectIntegrationTestsModule` does not yet accept `LiveCoding.CompileSync` commands via `command.txt` (Phase 1).
- No Low Level Test suites exist in the project (Phase 4).
- No failed-rerun helper exists (Phase 3).

### Deliberate non-changes
- Do NOT modify persistent editor scripts. They work.
- Do NOT refactor test classes during this work. Tag additions go on existing tests.
- Do NOT migrate to AutomationDriver during speed work. Correctness is a separate track.

---

## Phase 0 - Immediate guardrails

**Goal:** stop the current waste today, before any deeper tooling work.

### Deliverables

#### 0.1 Canonical rule update
Add a new section to `docs/agents/canonical.md`:

### Dev Loop Contract
- Default to one exact test filter.
- Broad filters are forbidden in normal iteration.
- Broad verification is allowed only at end-of-slice, CI, or explicit user request.
- After a small code change, run only the most directly affected test or the smallest explicit set.
- Live Coding is allowed only for `cpp` body edits.
- Header, reflection, module, or build changes must use the normal compile path.
- If uncertain, choose the smaller targeted test set first, then widen only if it fails or the user asks.

#### 0.2 AGENTS rule update
Add a short hard rule to `AGENTS.md`:

### Dev Loop Rule
Agents must not run broad automation filters such as `ProjectIntegrationTests.UI` during normal iteration unless:
- the user explicitly requests it
- the run is an end-of-slice gate
- the agent passes an explicit broad-filter override

#### 0.3 Exact-filter wrapper
Add `scripts/ue/test/unit/run_single.ps1`

Contract:
- requires one **full exact test name**
- rejects:
  - wildcards
  - prefix filters
  - comma unions
  - `Group:` filters
  - `Filter:` filters
  - broad tags
- on rejection, prints the full rejection-message contract (see "Rejection message contract" section below)
- prefers persistent editor if alive
- otherwise falls back to cold one-shot run

Example accepted:
- `ProjectIntegrationTests.UI.Framework.Inventory.CellDropTarget.BuilderWrapsEveryCell`

Example rejected:
- `ProjectIntegrationTests.UI`
- `ProjectIntegrationTests.UI.*`
- `Group:Inventory`
- `Inventory && !Slow`

#### 0.4 Dev-mode refusal in the generic wrapper
Update `scripts/ue/test/unit/run_cpp_tests_safe.ps1`

Add:
- `-Mode Dev|Gate`
- default = `Dev`
- in `Dev` mode, broad filters fail unless `-AllowBroadFilter` is passed
- in `Gate` mode, broad filters are allowed
- on rejection, print the rejection-message contract (see below) including one exact accepted example and both override flags

#### 0.5 Single entrypoint
Add `scripts/ue/test/unit/iterate.ps1`

Purpose:
one command for normal iteration.

Inputs:
- `-TestFilter <exact full test name>`
- `-Mode Dev|Gate`
- `-AllowBroadFilter`
- `-CompileMode Auto|LiveCoding|Full`

Behavior:
- validates filter shape
- decides compile path
- dispatches through persistent editor if available
- prints the chosen path clearly:
  - exact test
  - compile mode
  - persistent vs cold
  - whether broad-filter override was used

### Files to touch (Phase 0)
- `docs/agents/canonical.md` — add "Dev Loop Contract" section.
- `AGENTS.md` — add "Dev Loop Rule" (short hard-rule block under existing CRITICAL Rules).
- `scripts/ue/test/unit/run_single.ps1` (new).
- `scripts/ue/test/unit/run_cpp_tests_safe.ps1` (add `-Mode Dev|Gate` + `-AllowBroadFilter` gating).
- `scripts/ue/test/unit/iterate.ps1` (new, delegates to `run_single.ps1` or full-mode wrapper).

### Verification (Phase 0)
Test each guardrail with a specific change; do NOT run a broad suite to verify.
- Run `run_single.ps1 "ProjectIntegrationTests.UI.Framework.Inventory.CellDropTarget.BuilderWrapsEveryCell"` — must dispatch.
- Run `run_single.ps1 "ProjectIntegrationTests.UI"` — must reject with clear message.
- Run `run_cpp_tests_safe.ps1 -Mode Dev -TestFilter "ProjectIntegrationTests.UI"` — must reject.
- Run `run_cpp_tests_safe.ps1 -Mode Gate -TestFilter "ProjectIntegrationTests.UI"` — must accept.
- Confirm `canonical.md` and `AGENTS.md` both state the same rule.

### Acceptance
- exact single test works
- broad filter in dev mode fails with a clear message
- broad filter in gate mode works
- docs and scripts say the same thing

### Estimated effort
0.5 - 1 day

---

## Phase 1 - Live Coding integration

**Goal:** cut common `cpp` body iteration from cold compile + run down to a warm patch + run loop.

### Deliverables

#### 1.1 Persistent-editor Live Coding path
Extend `iterate.ps1` so `-CompileMode Auto` chooses:
- `LiveCoding` for `cpp` body-only edits
- `Full` for header / reflection / module changes

#### 1.2 File-IPC compile command
Chain:
1. send `LiveCoding.CompileSync`
2. wait for success marker
3. dispatch exact test

#### 1.3 Clear fallback
If Live Coding:
- is unavailable
- rejects the edit
- times out
- reports patch failure

then automatically switch to the normal compile path and continue.

### Constraints
Live Coding path is only valid for:
- implementation-only `cpp` edits
- non-reflected changes
- no header layout changes
- no module dependency changes
- Dev / DebugGame editor flow

### Files to touch (Phase 1)
- `scripts/ue/test/unit/iterate.ps1` — add compile-mode selector.
- `ProjectIntegrationTestsModule` file watcher — extend to accept `LiveCoding.CompileSync` + success/fail status lines.
- `docs/agents/canonical.md` — add one-line note that Live Coding is the default compile path in `iterate.ps1`.

### How Live Coding dispatches
UE 5.7 exposes `LiveCoding.CompileSync` as a console command. In the persistent editor, the existing `command.txt` IPC already routes through `ProjectIntegrationTestsModule`. Extend that module to:
1. Accept a command line like `livecoding.compilesync`.
2. Invoke `GEngine->Exec(nullptr, TEXT("LiveCoding.CompileSync"))`.
3. Write a status line back to a result file (`command.result.txt`).

`iterate.ps1` polls the result file, then dispatches the exact test command.

### Phase 1 hard invariant

`iterate.ps1` must never dispatch a test while Live Coding state is unresolved.

Required terminal statuses:
- `LC_OK`
- `LC_UNAVAILABLE`
- `LC_REJECTED`
- `LC_TIMEOUT`
- `LC_FAILED`

Behavior:
- `LC_OK` -> dispatch exact test
- any non-OK terminal status -> log the reason and fall back to full compile
- no test dispatch is allowed before one of those terminal states is observed

Status meanings:
- `LC_OK` - Live Coding patch applied successfully
- `LC_UNAVAILABLE` - Live Coding not enabled / session not connected
- `LC_REJECTED` - header, reflection, module, or build-dep change detected
- `LC_TIMEOUT` - no terminal status within 60s
- `LC_FAILED` - patch generation or link failed

On any non-OK terminal status, `iterate.ps1` logs "Live Coding fell back: <reason>" and re-enters the normal compile path via the existing cold-editor script. The fallback must itself complete (and its terminal status observed) before any test dispatch.

### Verification (Phase 1)
- Edit one line inside a `.cpp` method body → `iterate.ps1` finishes in ~10-20s common case.
- Edit a `.h` header → must fall back to full compile, logged clearly.
- Edit a `.Build.cs` → must fall back, logged clearly.

### Acceptance
- body-only edit + exact test on warm editor completes in ~10-20s common case
- incompatible change auto-falls back to full compile
- failure mode is loud, not silent

### Estimated effort
0.5 - 1 day

---

## Phase 2 - Focused tag filters

**Goal:** reduce suite cost when exact single-test is too narrow but full union is too broad.

### Deliverables

Tag the most-used automation tests first, not the whole catalog blindly.

Initial tags:
- `Fast` - under 5s
- `Slow` - 5-30s
- `E2E` - 30s+ or full Slate interaction
- optional area tags:
  - `Inventory`
  - `UI`
  - `LootPlaces`
  - `Mind`
  - `Vitals`

### Important rule
Tagging is a **secondary** optimization.
Single exact test remains the default. Tags are for:
- local small-batch checks
- area-level validation
- gate composition

### Files to touch (Phase 2)
- Highest-frequency automation test files under `Plugins/Test/ProjectIntegrationTests/Source/ProjectIntegrationTests/Private/Integration/*.cpp` — register tags via the project's supported automation-tag hook (see below).
- `scripts/ue/test/unit/iterate.ps1` — accept `-Tags "<expr>"` and translate into `Automation SetTagFilter` + `Automation RunTests Group:Tagged`.

### Phase 2 implementation note

Use the project's supported automation-tag hook for the current ALIS test base.

Before tagging any test, verify the exact hook on the current test base:
- grep for existing tag usage: `grep -rn "REGISTER_SIMPLE_AUTOMATION_TEST_TAGS\|FAutomationTestTags\|GetTestTags" Plugins/Test/`
- if the hook is the static macro `REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(TClass, PrettyName, TagsString)` (the standard UE 5.7 pattern verified in `Engine/Source/Runtime/Core/Public/Misc/AutomationTest.h` around line 4269), document that and use it.
- if the hook is a virtual override (e.g. in a custom test base), name the exact method only after grepping confirms it on this repo.
- if the project uses a wrapper base class, follow whatever API that base exposes.

Do not hard-code the tag API shape in this todo unless it has been verified against the current repo and engine version. **UE 5.7 reality (verified 2026-04-23):** the tag CVar `Automation.TestTagGlobalFilter` uses `FTextFilterExpressionEvaluator` in BasicString mode = literal substring match only. Boolean operators (`&&` / `||` / `!`) become literal characters and match nothing. Use single-token tags like `Fast` / `Inventory`. The bracket syntax (`[Fast][Inventory]`) is used in the REGISTER call's tag string, but the filter side matches on substring - `-Tags "Fast"` picks up any test whose registered string contains "Fast".

### Verification (Phase 2)
- Tag a handful of inventory tests with `"[Fast][Inventory]"` (concatenated single string).
- `iterate.ps1 -Mode Gate -Tags "Fast"` narrows the `ProjectIntegrationTests` pool to matching tests.
- Broad untagged filter still runs everything (tags do not hide tests; they filter only when used).

### Acceptance
- agents can run a small tagged subset instead of a union suite
- tags exist on the highest-frequency tests first

### Estimated effort
0.5 day initial
more only if adoption proves useful

---

## Phase 3 - Failed-tests rerun helper

**Goal:** speed up recovery after a gate run, not normal day-to-day iteration.

### Why this is not Tier 1
Once exact single-test discipline is enforced, failed-rerun is no longer the main daily win. It matters more for:
- gate runs
- local suite sweeps
- CI reproduction

### Deliverables
Add a small PowerShell helper that:
1. scrapes the editor log between automation sentinels
2. extracts failed test names
3. rebuilds an explicit filter list
4. reissues only the failed tests

### Files to touch (Phase 3)
- `scripts/ue/test/unit/rerun_failed.ps1` (new).
- Optional: add a marker-only automation sentinel log line to make scraping cheap.

### Log-scrape contract
The automation log format has stable markers:
```
LogAutomationController: ... Test Completed. Result={Fail} ... <TestDisplayName>
LogAutomationController: ... Test Completed. Result={Success} ... <TestDisplayName>
```
The helper reads `Saved/Logs/Alis.log` (or the persistent editor log path), filters `Result={Fail}` lines between the two most recent `Automation RunTests` sentinels, extracts names, and reissues them as a `;`-separated filter.

### Verification (Phase 3)
- Force-fail one test locally, run it in gate mode, then run `rerun_failed.ps1` — must rerun only that test.
- With zero failures, must print "no failed tests" and exit 0.

### Acceptance
- after a gate run, rerun-failed is one command
- zero failed tests returns immediately
- output clearly lists the reconstructed filters

### Estimated effort
0.5 day

---

## Phase 4 - Low Level Test pilot

**Goal:** prove a sub-3s pure-logic tier without committing to a large migration blindly.

### Important correction
Do **not** allocate 1-2 days up front for 4-5 suites.

Do a **2-suite pilot first**.

### Recommended pilot candidates
1. `InventoryViewModelActionRules`
2. `InventoryViewModelPlacement`

Reason:
- most self-contained
- least editor-dependent
- easy to compare against current automation cost

### Candidate list after pilot
- `FInventoryDropRouter::Route`
- `InventoryViewModelSurfaceDispatch`
- `FInventoryDragSession` state transitions

### Pilot exit criteria
Proceed with broader LLT migration only if:
- implementation cost is reasonable
- runtime is materially faster
- assertion quality is at least as good
- dependency pain stays low

### Files to touch (Phase 4)
- `Plugins/Test/ProjectIntegrationTestsLLT/` (new plugin, LowLevelTests target).
- `Plugins/Test/ProjectIntegrationTestsLLT/Source/ProjectIntegrationTestsLLT/Tests/InventoryViewModelActionRulesTests.cpp` (new).
- `Plugins/Test/ProjectIntegrationTestsLLT/Source/ProjectIntegrationTestsLLT/Tests/InventoryViewModelPlacementTests.cpp` (new).

### How LLT differs from automation
- No UObject / no GEngine / no editor. Standalone Catch2 exe.
- Reference: `Engine/Source/Developer/LowLevelTestsRunner/`.
- Build target is `<PluginName>LLTTests.Target.cs`.
- Run via the UE BuildAndTest script or directly from the produced exe. Sub-second is realistic for pure-logic tests.
- Pure-logic namespaces (`InventoryViewModelActionRules`, `InventoryViewModelPlacement`) are ideal candidates — no UObject reflection needed.

### Pilot metric target
- Pilot suite wall-clock time under 3 seconds total.
- If LLT suite takes longer than 10 seconds, the pilot has failed its premise — investigate or abandon.

### Verification (Phase 4)
- `InventoryViewModelActionRules` covered by LLT with at least the same assertions the automation suite currently has.
- Same for `InventoryViewModelPlacement`.
- Measured runtime recorded in the metrics table.

### Acceptance
- 2 suites running in the Low Level Test runner
- measured time and maintenance cost documented
- explicit go / hold decision for broader migration

### Estimated effort
0.5 - 1 day pilot

---

## Phase 5 - Inventory in-action verification harness (3-layer, on-demand)

**Goal:** once dev-loop speedups (Phases 0-4) are in place, we need a
re-usable harness that proves inventory still works end-to-end after any
refactor. The harness runs **in action** (real drag-drop, real pickup, real
equip) — NOT state-only ViewModel asserts. Three layers, each callable
independently, composable into one run.

### Why
Phases 0-4 optimize for narrow single-test iteration. That speeds agents but
makes it easy to miss regressions in cross-feature flows (drag-drop routing,
container view, pickup lifecycle, equip grants). Phase 5 is the
counterbalance: a fast, reusable "full inventory sweep" that catches
integration regressions without bringing back broad-filter reflex.

### Three layers (SOLID blackboxes)

Each layer is a separate module with a stable contract. Layers do NOT reach
into each other's internals. They can be combined via a top-level runner.

#### Layer A - Tests (in-action)
- AutomationDriver-driven E2E tests (real Slate hit-testing, real bubble
  routing), covering:
  - drag-drop: grid cell -> grid cell (move), grid cell -> equip slot, cell
    -> nearby container, cell -> world drop
  - containers view: open/close, scroll, swap containers
  - pickup: world item -> inventory (nearby + direct), deferred-pickup path
    (see `inventory_objectdef_load_race.md`)
  - equip: equip slot grant, unequip, grant-bound container (pockets,
    backpack) appear/disappear
  - stack merging: drag stack onto stack (covered by
    `inventory_stacks_dragdrop_shipping.md` once that lands)
- Tests register a concatenated tag string including `[Phase5]`. A focused
  Phase 5 run is then one single-token filter: `iterate.ps1 -Mode Gate
  -Tags "Phase5"` (substring match via the UE 5.7 CVar - see Phase 2
  note; boolean operators are not supported, so keep tags single-token).
  Today `verify.ps1`'s default is `"Inventory"` because no `[Phase5]`-
  tagged tests exist yet; switch the default to `"Phase5"` when Layer A
  E2E tests land.
- Owner: `Plugins/Test/ProjectIntegrationTests/Source/ProjectIntegrationTests/Private/Integration/InventoryInActionTests.cpp` (new).

#### Layer B - Dump (state snapshot after action)
- Structured JSON/text dump of inventory state + UI widget tree after each
  action. Diffable; output is stable across runs given same seed.
- Extends the existing `tools/agentic/ui/layout_report.py` pipeline: tests
  trigger `Automation DumpUI <path>` at key checkpoints; the helper parses
  the dump and produces a readable report.
- Contract: dump format is versioned; schema change -> minor version bump
  with one regression-verifier update.
- Owner: `tools/agentic/inventory/dump_report.py` (new, composes with
  existing `layout_report.py`).

#### Layer C - Screenshot (visual verification)
- UE's `HighResShot` console command (or `shot` cvar) captures the UI
  at each checkpoint. Output: per-checkpoint PNG under
  `Saved/Screenshots/Phase5/<test>/<step>.png`.
- Pixel-diff against a baseline is deliberately OUT OF SCOPE for v1 (pixel
  diffs are flaky across GPU driver / DPI). v1 only captures; review is
  human. v2 may add structural-diff (region-of-interest comparison).
- Contract: one PNG per checkpoint; deterministic filename; baseline dir
  under version control (small PNGs only).
- Owner: `scripts/ue/test/inventory/capture_phase5.ps1` (new).

### On-demand runner (top-level)
- `scripts/ue/test/inventory/verify.ps1` accepts `-Layers A,B,C` (default
  all three). Delegates to each layer in isolation; aggregates exit codes;
  prints one-line-per-layer summary + paths to artifacts.
- One invocation = one verification run. Cheap enough to run at end of
  slice, nightly, or when a PR touches `Plugins/Features/ProjectInventory`
  or `Plugins/UI/ProjectInventoryUI`.

### SOLID invariants (CI-enforced via a fitness test)
1. Layer A does not import Layer B or C helpers.
2. Layer B runs headless (no renderer). It MUST work with `-NullRHI`.
3. Layer C does NOT parse dumps or assert state; it only captures pixels.
4. `verify.ps1` is the only module that knows all three exist.
5. Each layer's failure mode is loud: non-zero exit + one-line summary.

### Files to touch (Phase 5)
- `Plugins/Test/ProjectIntegrationTests/Source/ProjectIntegrationTests/Private/Integration/InventoryInActionTests.cpp` (new).
- `tools/agentic/inventory/dump_report.py` (new).
- `scripts/ue/test/inventory/capture_phase5.ps1` (new).
- `scripts/ue/test/inventory/verify.ps1` (new; top-level runner).
- `Plugins/Test/ProjectIntegrationTests/Source/ProjectIntegrationTests/Private/Integration/Phase5HarnessFitnessTests.cpp` (new; enforces the 5 invariants above).

### Dependencies on earlier phases
- Phase 2 (tags): Layer A reuses the tag infrastructure. If Phase 2 is not
  yet landed, Layer A uses explicit exact-test filters until it is.
- AutomationDriver correctness track: Layer A builds on top of it. If the
  separate track is not yet landed, Layer A starts with
  `FSlateApplication::ProcessMouseButton*` synthetic input and swaps to
  AutomationDriver when ready.

### Verification (Phase 5)
- Fresh repo + `verify.ps1` completes all three layers within ~60s warm.
- Breaking any one layer (sabotage: mute a drag-drop line in
  `FInventoryDropRouter::Route`) causes Layer A to fail with a specific
  assert, Layer B to show the missing dispatch in the dump, and Layer C
  to capture a visibly-broken frame.
- Phase 5 fitness test catches a contrived invariant violation.

### Acceptance
- 3 layers exist as independent modules
- top-level `verify.ps1` composes them
- CI fitness test enforces the 5 invariants
- end-of-slice gate run includes Phase 5 by default
- sabotage test confirms all 3 layers are actually observing behavior, not
  theater

### Estimated effort
1 - 2 days (Layer A drives the time; B and C are thin wrappers).

---

## Separate correctness track - AutomationDriver

**Goal:** close the Slate-routing coverage gap.

### Position in roadmap
After Phase 0 and preferably after Phase 1.

### Why separate
AutomationDriver helps prove:
- hit testing
- bubble routing
- real Slate delivery

It does **not** materially solve the current iteration-speed problem by itself.

### Deliverables
- one smoke test first
- then 2-3 drag/drop E2Es
- editor-only group if needed

### Do not combine with speed work
Keep this as correctness infrastructure, not speed infrastructure.

### Files to touch (AutomationDriver, separate track)
- `Plugins/Test/ProjectIntegrationTests/Source/ProjectIntegrationTests/Private/Integration/AutomationDriverSmokeTest.cpp` (new) — one dummy button click smoke test.
- Then: port 2-3 drag/drop E2E assertions to driver-based. Do NOT delete the synthetic-input tests until the driver-based pass is proven stable.

### Engine references (UE 5.7)
- `Engine/Source/Developer/AutomationDriver/Public/IAutomationDriver.h` — driver entry point.
- `Engine/Source/Developer/AutomationDriver/Public/IDriverSequence.h` — Press/Move/Release sequence API.
- `Engine/Source/Developer/AutomationDriver/Public/LocateBy.h` — element locators (by tag, name, Slate widget).

---

## Deferred
Not first-wave items unless evidence changes.

### AFunctionalTest migration
Useful later for world-resident multi-test flows, but do it after AutomationDriver if still needed.

### Git-diff-based test selection
Interesting, but lower ROI than exact-filter enforcement and Live Coding.

### Per-module unity tuning
Only revisit if measured compile data proves a real bottleneck remains after earlier phases.

---

## Prevent wrong agent behavior

Docs alone are not enough. Enforce at three layers.

### Layer 1 - policy
`canonical.md` and `AGENTS.md` both state:
- exact single-test is default
- broad filter requires explicit override or gate mode

### Layer 2 - script refusal
`run_single.ps1` and `iterate.ps1` reject broad filters by default.

### Layer 3 - explicit gate path
Broad verification must go through:
- `iterate.ps1 -Mode Gate`
or
- `-AllowBroadFilter`

That makes the expensive choice visible and intentional.

### Rejection message contract (dev-mode broad filter refusal)

Every dev-mode broad-filter rejection (from `run_single.ps1`, `iterate.ps1`, or `run_cpp_tests_safe.ps1 -Mode Dev`) must print:
1. the rejected filter (verbatim)
2. the reason it is considered broad (wildcard / prefix / comma union / `Group:` / `Filter:` / broad tag)
3. one exact accepted example drawn from the current test base
4. how to override intentionally:
   - `-Mode Gate`
   - `-AllowBroadFilter`

The goal is to stop wrong agent behavior fast, not make the user (or an agent) guess the valid syntax. An agent reading the rejection should immediately know what to run instead and what to pass if the broad run really is intended.

### Required agent behavior
For any code change, the agent must:
1. choose the smallest exact affected test first
2. use `iterate.ps1` or `run_single.ps1`
3. avoid broad filters unless user asked or the slice is ending
4. state when a broad gate run is being used and why

---

## Metrics to record

Record before and after. A plan without numbers is not done.

### Template (fill in during execution)

Measured 2026-04-23. Same machine, 3 runs per scenario, median reported
(first run excluded as warmup outlier per measurement rules).

Wrapper invocations:
- Cold: `run_cpp_tests_safe.ps1 -TestFilter <exact> -Map /MainMenuWorld/Maps/MainMenu_Persistent`
- Warm / Warm+LC / Broad gate: `iterate.ps1` against persistent editor

Exact test under measurement: `ProjectIntegrationTests.UI.Framework.Inventory.CellDropTarget.BuilderWrapsEveryCell`

| Scenario | Wall-clock (median) | Raw runs | Notes |
|---|---|---|---|
| Cold boot + exact single test | **42.9s** | 73.9 / 42.3 / 42.9 | First run filesystem/shader-cache warmup outlier. |
| Warm persistent editor + exact single test | **13.5s** | 12.0 / 13.5 / 13.5 / 13.5 (4 runs, excl. run 1) | Steady-state; editor boot ~15s amortized. |
| Warm editor + Live Coding + exact single test | **25.0s** | 36.5 / 24.8 / 25.2 | LC 13-14s + test ~11s. Run 1 is post-rebuild LC warmup (excluded). |
| Broad gate run (`ProjectIntegrationTests.UI`, 83 tests) | **24.1s** | 24.7 / 23.6 / 24.1 | ~0.29s per test; ~1.8x broader than exact-warm per-test. |
| LLT pilot runtime (2 suites) | **n/a** | - | Deferred on launcher engine (see canonical.md Phase 4 decision). |

Observed speedups (daily iteration):
- Cold -> Warm: **3.2x faster** (42.9 -> 13.5s) - the Phase 0/persistent editor win.
- Warm vs Warm+LC on body edit: LC adds ~11s compile cost; still 1.7x faster than cold.
- Exact warm vs broad gate: 1 test in 13.5s vs 83 tests in 24.1s. Broad is faster per-test
  but only wins when you actually need most of them; daily iteration rarely does.

Setup cost notes (not steady-state):
- Editor cold boot (first persistent_editor_start): ~15s.
- Module rebuild (ProjectIntegrationTests): ~2-10 min when PDB stripped by prior
  packaging run. One-time after packaging; not part of normal iteration.
- LC run 1 after a rebuild: extra ~10s over steady-state (warmup).

### Measurement rules
- Use the same machine for all measurements.
- Do three runs, take the median.
- Exclude the first run after editor start (warmup outlier).
- Record wall-clock only; do not try to fragment into compile vs run unless the wrapper already does it.

---

## Risks and mitigations

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| Live Coding patches silently produce stale behavior | Low | High | Always dispatch exact test after a compile-sync — the test is the verification. Never skip the run. |
| Guardrail scripts block a legitimate broad run | Medium | Low | `-AllowBroadFilter` flag + `-Mode Gate` both provide explicit opt-in paths. Document both in the rejection message. |
| LLT pilot reveals pure-logic code still depends on engine types | Medium | Medium | Treat as signal to refactor, not as LLT failure. Pick only the most self-contained namespaces for the pilot. |
| Agent keeps ignoring the dev-loop contract | High | Medium | Three-layer enforcement (docs + script refusal + gate override). If script refusal is bypassed, treat as a bug in the script. |
| `command.txt` IPC race during Live Coding | Low | Medium | Use a status-line protocol (`LC_START`, `LC_OK`, `LC_FAIL_*`) not a bare stdout parse. |
| Tag filter syntax misremembered as boolean | Low | Low | Canonical.md is now explicit: UE 5.7 tag CVar is BasicString substring; single token only. Rechecked 2026-04-23. |

---

## Cross-refs

### Project docs
- `docs/agents/canonical.md` — first-read SOT; must gain "Dev Loop Contract" section in Phase 0.
- `AGENTS.md` — must gain "Dev Loop Rule" hard block in Phase 0.
- `docs/testing/automation.md` — general automation conventions.
- `docs/testing/unit_tests.md`, `integration_tests.md`, `smoke_tests.md` — test-layer docs; do not duplicate content from this todo into them.
- `docs/testing/troubleshooting.md` — test failure triage; link from guardrail rejection messages.

### Existing scripts to reuse / extend
- `scripts/ue/test/unit/persistent_editor_start.ps1` — warm editor launch.
- `scripts/ue/test/unit/persistent_editor_run.ps1` — file-IPC dispatch. Already reads `command.txt`.
- `scripts/ue/test/unit/persistent_editor_stop.ps1` — shutdown.
- `scripts/ue/test/unit/run_cpp_tests_safe.ps1` — current general wrapper; Phase 0 adds `-Mode Dev|Gate`.

### Existing module to extend
- `Plugins/Test/ProjectIntegrationTests/Source/ProjectIntegrationTests/Private/ProjectIntegrationTestsModule.cpp` — file watcher on `command.txt`. Extend for `LiveCoding.CompileSync` handling in Phase 1.

### Related live todos
- `todo/00_current/inventory_ui_nearby_decouple.md` — deferred cleanup items (SetPolicyProvider name underspec, `AddExpectedError` quarantine) that intersect with testing discipline.
- `todo/00_current/interaction_hold_world_container_timing.md` — live known bug; uses `AddExpectedError` quarantine pattern; should not be used as a precedent.
- `todo/00_current/inventory_objectdef_load_race.md` — deferred-pickup + loud-error pattern; already landed.

### Engine paths (UE 5.7 at `<ue-path>/`)
- `Engine/Source/Developer/AutomationController/Private/AutomationCommandline.cpp` — `RunTests` / filter parsing.
- `Engine/Source/Developer/AutomationDriver/Public/IAutomationDriver.h` — driver API.
- `Engine/Source/Developer/LowLevelTestsRunner/` — LLT runner.
- `Engine/Source/Runtime/Core/Public/HAL/ConsoleManager.h` — console variable framework (referenced by `LiveCoding.CompileSync`).

---

## New-session handoff checklist

When a fresh agent picks up this todo, it must:

1. Read `docs/agents/canonical.md` end-to-end. It is the first-read SOT.
2. Read this file end-to-end. Do not skim.
3. Verify the "Current baseline state" section against the actual repo — grep the claims before building on them. If a claim is wrong, update this file first.
4. Start with Phase 0. Do not bundle phases. Do not jump to Phase 4 "because it sounds cool".
5. Before writing any script, confirm the existing script layout under `scripts/ue/test/unit/`. Reuse, do not duplicate.
6. For each phase: finish the "Files to touch" list, then the "Verification" list, then the acceptance criteria. Record metrics in the template.
7. Do not run broad filters while validating your own Phase 0 work. That is the bug this plan fixes.
8. After each phase, update this file's checklist in "Definition of done" to reflect actual state.
9. If a risk in the risks table fires, update the risk row with what was observed and what was done.
10. When the definition of done is fully satisfied, move this todo to `todo/02_backlog/` or delete it — do not leave it stale in `00_current/`.

---

## Recommended execution order

1. Phase 0 - guardrails, docs, exact-filter scripts
2. Phase 1 - Live Coding wrapper
3. Phase 2 - focused tag filters
4. Phase 3 - failed-rerun helper
5. Phase 4 - Low Level Test pilot
6. Separate correctness track - AutomationDriver

---

## Definition of done for this todo

This todo is done when:
- exact-filter iteration is the default and enforced (Phase 0)
- broad filters require explicit opt-in (Phase 0)
- Live Coding fast path works for body-only edits (Phase 1)
- focused tag filters exist on at least the highest-frequency tests (Phase 2)
- failed-rerun helper exists and is documented (Phase 3)
- LLT pilot has measured runtime and an explicit go/hold decision (Phase 4)
- end-of-slice gate path is separate and obvious
- at least one measured before/after metrics table is recorded
- agent docs (`canonical.md`, `AGENTS.md`) and scripts agree
- cross-refs in this file are still valid (nothing stale linked)

### Phase status (update during execution)

- [x] Phase 0 — Immediate guardrails (landed 2026-04-22; see "Landed in Phase 0" above)
  - [x] docs landed (`canonical.md` Dev Loop Contract, `AGENTS.md` Dev Loop Rule)
  - [x] wrapper scripts landed (`run_single.ps1`, `iterate.ps1`, `run_cpp_tests_safe.ps1 -Mode Dev|Gate`)
  - [x] exact-filter validator hardened against long prefix bypass (source-scan for quoted literal, not just segment count)
  - [x] regression checks passed (4-segment prefix `ProjectIntegrationTests.UI.Framework.Inventory` correctly refused by `run_single.ps1` and `run_cpp_tests_safe.ps1 -Mode Dev`; canonical exact filter accepted; `-Mode Gate` override still works)
- [x] Phase 1a — Live Coding sync + terminal-state gating + stale-state protection (landed 2026-04-23)
  - [x] reuse check: `ProjectIntegrationTestsModule` already routes arbitrary `GEngine->Exec` via `command.txt` (line 74). No C++ changes needed.
  - [x] `scripts/ue/test/unit/livecoding_sync.ps1` (new) - dispatches `LiveCoding.CompileSync` via `command.txt`, polls editor log for terminal markers: LC_OK = `LogLiveCoding: (Display|Warning): Live coding succeeded` (verbosity varies by edit kind); LC_FAILED = `LogLiveCoding: Error: Live coding failed` or toolchain `Compile error:` / `Link error:`; LC_TIMEOUT at 60s; LC_UNAVAILABLE when no warm editor. Harmless `LogLiveCodingServer: Error: ... Cannot find image section .voltbl` startup spam is correctly NOT treated as LC_FAILED.
  - [x] `iterate.ps1` wired with `-CompileMode Auto|LiveCoding|None` (`Full` removed from the ValidateSet; see 1b below). Both Auto and LiveCoding modes ABORT with exit 5 on any non-LC_OK status - stale-state protection (a previous version silently dispatched against stale binaries on LC failure, which is fixed).
  - [x] cold-path verification (no live editor): stale-state abort and None-mode skip verified.
  - [x] warm-path verification (2026-04-23): LC_OK proved in 17.8s against a body-only edit; LC_FAILED sabotage returned exit 1 in 13.2s with clear reason; clean-state restore LC_OK in 24.4s.
  - [x] Phase 1 hard invariant: no test dispatch before a terminal LC status is observed, and no dispatch against stale state. Enforced at two layers: `livecoding_sync.ps1` blocks until terminal, then `iterate.ps1` aborts with exit 5 on any non-LC_OK.
- [ ] Phase 1b — Safe automatic fallback when Live Coding cannot succeed (open)
  - Goal: instead of exit 5 + "rebuild manually" instructions, Auto mode could automatically sequence (stop editor) → (rebuild module) → (restart editor) → (dispatch test).
  - Blocker: requires a robust stop/build/start orchestrator that handles partial failures cleanly. Not a one-line change; deserves a dedicated design pass.
  - Workaround today: humans run the 4 manual steps printed by the stale-state abort message, or pass `-CompileMode None` if they have already rebuilt.
  - When 1b lands, document the new auto-sequence in `canonical.md` Dev Loop Contract and drop the 4-step instruction block from iterate.ps1's abort message.
- [x] Phase 2 — Focused tag filters (landed 2026-04-22; warm-editor verified 2026-04-23)
  - [x] reuse check: UE 5.7's `REGISTER_SIMPLE_AUTOMATION_TEST_TAGS` macro at `Core/Misc/AutomationTest.h:4269` is the standard hook; static registration at file scope. No project base-class override needed.
  - [x] 10 inventory tests tagged across 3 files (3x GridSizing `[Fast][Inventory]`; 2x CellDropTargetContract `[Fast][Inventory]`; 5x DragEventBus mix of `[Fast][Inventory]` and `[Slow][Inventory][E2E]` to prove the filter can select subsets).
  - [x] `persistent_editor_run.ps1` extended with `-TagExpression`. When present: emits `Automation SetTagFilter "<expr>"` + `Automation RunTests <pool>` (NOT `Group:Tagged` - that pseudo-target does not exist in UE 5.7). Pool defaults to `ProjectIntegrationTests`. When absent: emits `Automation SetTagFilter ""` first so a tag CVar from a prior run does not silently filter out the exact-filter dispatch.
  - [x] `iterate.ps1` extended with `-Tags <tag>`. Tag runs are implicitly broad so they require `-Mode Gate` or `-AllowBroadFilter`. TestFilter/Tags are mutually exclusive.
  - [x] cold-path verification: mutually-exclusive check, dev-mode refusal, override hint, and "tag runs require warm editor" failure message all print correctly.
  - [x] warm-editor verification (2026-04-23): `iterate.ps1 -Mode Gate -Tags "Fast"` against a warm editor narrowed the `ProjectIntegrationTests` pool to exactly the 8 tests tagged `[Fast][Inventory]` (3 GridSizing + 2 CellDropContract + 3 DragBus fast). All 8 passed in 15s.
  - [x] UE CVar limitation documented: `Automation.TestTagGlobalFilter` uses `FTextFilterExpressionEvaluator` in `BasicString` mode -> **literal substring match only**, no boolean operators. `&&`/`||`/`!` become literal characters and fail to match. The earlier plan wording ("boolean expressions") was wrong; corrected in `canonical.md`. Use single-tag filters and rely on the pool (RunTests prefix) for coarser narrowing.
  - [x] static-init + LC: `REGISTER_SIMPLE_AUTOMATION_TEST_TAGS` is a file-scope static initializer. Live Coding patches function bodies but NOT static initializers, so removing a REGISTER call and LC-patching does NOT drop the test from the tag set. True sabotage-of-tag-registration requires a cold editor restart. Documented as a Phase 2 residual protocol rather than blocking; tag infrastructure is verified working via the count proof above.
  - [x] **Tag taxonomy landed 2026-04-23:** orthogonal 3-dimension schema (Speed / Kind / Area) documented in `canonical.md` "Tag taxonomy" section. All 11 existing tagged tests (10 inventory + 1 AutomationDriver smoke) backfilled with Kind: `[Unit]` for GridSizing math, `[Integration]` for subsystem-only + widget-contract tests, `[E2E]` for painted-grid synthetic-input tests. CLI rule: single-token filters always; taxonomy is metadata, not a filter expression. `Phase5` reserved as a sentinel for the future Layer A harness.
- [x] Phase 3 — Failed-tests rerun helper (landed 2026-04-22)
  - [x] reuse check: fail-marker regex is the same one already used by `run_cpp_tests_safe.ps1:149-150` and `persistent_editor_run.ps1:192-193`; no new parser needed. Dispatch delegated to `iterate.ps1 -Mode Gate` so all polling logic is shared.
  - [x] `scripts/ue/test/unit/rerun_failed.ps1` (new) - auto-detects latest log (persistent editor `editor.log` or overnight `tests.log`), walks backwards to the most-recent `Automation Test Queue Empty` sentinel, extracts all `Result={Fail} ... Path={...}` within that run slice, composes a `;`-separated filter, re-dispatches through `iterate.ps1 -Mode Gate -CompileMode None`.
  - [x] verification: synthetic log with 2 Fail lines correctly parsed; reconstructed filter routed through iterate.ps1 with `FilterShape: broad` + `BroadOverride: True`. Zero-failures path returns "Nothing to rerun." with exit 0.
- [x] Phase 4 — Low Level Test pilot (deferred 2026-04-23; decision moved to canonical.md)
  - **Decision + rationale + revisit conditions: see [docs/agents/canonical.md](../../docs/agents/canonical.md) "Phase 4 Low Level Tests - DEFERRED on launcher engine".** Summary: hard UE limit (`RulesAssembly.cs:679`) blocks TestTargets with `bCompileAgainstCoreUObject=true` on launcher-installed engines. ALIS stays on the launcher engine. Revisit only if the source/launcher coexistence investigation (`source_launcher_coexistence.md`) finds a cheap path, warm-editor iteration becomes insufficient, or a specific hot spot demands it.
  - [x] scaffold deleted 2026-04-23 (was `Plugins/UI/ProjectInventoryUI/Source/ProjectInventoryUI_LLT/`; recipe preserved in the canonical.md decision block).
- [x] Phase 5 — Inventory in-action verification harness (scaffolding + invariants check landed; Layer A scope locked)
  - **Layer A scope + Phase5 tag switchover: see [docs/agents/canonical.md](../../docs/agents/canonical.md) "Phase 5 Layer A coverage - 3 in-action tests max".** Summary: 3 tests only (pickup / equip-grant lifecycle / container swap), dedicated `[Phase5]` tag, no duplicate of existing drag coverage.
  - [x] reuse check: `tools/agentic/ui/layout_report.py` already parses `Saved/Dumps/Inventory.json`; `ProjectUIInventoryDumpTreeTest.cpp` already drives the dump via `Automation DumpUI`. UE's `HighResShot` console command provides the screenshot. All reused via existing channels.
  - [x] `scripts/ue/test/inventory/verify.ps1` - top-level composer. `-Layers A,B,C` selector; aggregates exit codes; one-line-per-layer summary.
  - [x] `scripts/ue/test/inventory/capture_phase5.ps1` - Layer C screenshot capture via persistent-editor `HighResShot`; deterministic output under `Saved/Screenshots/Phase5/<run-id>/`; SKIP path when no warm editor (exit 2).
  - [x] `tools/agentic/inventory/dump_report.py` - Layer B stub that composes with existing `layout_report.py`; prints contract + exits 0 until checkpoints are declared.
  - [x] `scripts/ue/test/inventory/README.md` - harness routing doc; SOLID invariants listed; deferred items named.
  - [x] verification: `verify.ps1 -Layers B` runs the Python stub end-to-end; `verify.ps1 -Layers C` SKIPs cleanly without a warm editor; aggregate exit code propagates failure.
  - [ ] Layer A additional tests (pickup, equip-grant, container-swap) - new `.cpp` files, needs UE compile; deferred like Phase 4 LLT suite construction.
  - [ ] Layer B full dump analyzer - awaits Layer A checkpoints; stub intentionally no-ops until then.
  - [ ] `Phase5HarnessFitnessTests.cpp` (SOLID invariants 1-5) - buildable C++ fitness test; deferred for the same reason.
- [x] Phase 1 warm-path validation (closed 2026-04-23: LC_OK 17.8s + LC_FAILED 13.2s sabotage + restore LC_OK 24.4s)
- [x] Phase 2 warm-path validation (closed 2026-04-23: single-tag narrowing proved `Fast` -> 8 tests; UE CVar BasicString limitation documented; static-init sabotage noted as cold-restart-only)
- [x] AutomationDriver smoke test (landed 2026-04-23: ProjectIntegrationTests.Correctness.AutomationDriver.ModuleLoadsAndCreatesDriver PASSES in 11.2s against warm editor; added `AutomationDriver` module dep to `ProjectIntegrationTests.Build.cs`)
- [x] Phase 5 fitness test equivalent (landed 2026-04-23 as `scripts/ue/check/phase5_harness_invariants.ps1` - PowerShell check is a better tool match for script-level invariants than a C++ fitness test; 5 invariants verified; sabotage-verified that a forbidden cross-layer reference triggers a specific failure)
- [x] Metrics table filled with before/after numbers (landed 2026-04-23; see "Template" table under "Metrics to record" above - Cold 42.9s / Warm 13.5s / Warm+LC 25.0s / Broad gate 24.1s; Cold->Warm = 3.2x speedup)
- [ ] Phase 5 Layer A extra E2E tests + Layer B full analyzer (deferred; scope is [docs/agents/canonical.md](../../docs/agents/canonical.md) "Phase 5 Layer A coverage")
- [ ] Phase 1b — automatic LC-failure recovery sequence (open; manual-only is the standing decision in [docs/agents/canonical.md](../../docs/agents/canonical.md) "Phase 1b Live Coding failure handling")
- [ ] AutomationDriver full migration — dual-run rule in [docs/agents/canonical.md](../../docs/agents/canonical.md) "AutomationDriver migration"

---

## Phase 4 feasibility audit (historical)

**Superseded by the launcher-engine finding of 2026-04-23.** See
[docs/agents/canonical.md](../../docs/agents/canonical.md) section
"Phase 4 Low Level Tests - DEFERRED on launcher engine" for the final
decision, rationale, and revisit conditions.

Preserved here for history only: both candidate namespaces
(`InventoryViewModelActionRules`, `InventoryViewModelPlacement`) were
technically LLT-compatible at the source level (no GEngine / UWorld /
subsystem calls; only USTRUCT reflection touch-points). The blocker
is not code shape - it is the launcher-engine constraint on any
TestTargetRules subclass that sets `bCompileAgainstCoreUObject = true`.
