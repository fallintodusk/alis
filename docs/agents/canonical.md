# Agent / Dev Canonical SOT

First-read reference for anyone (agent or human) doing non-trivial work on ALIS.
Captures the architectural rules, UE primitives to prefer, and testing patterns
learned from real incidents. Every claim cites a file; follow the cross-refs
at the bottom for deep dives. Kept as lean as the incident record allows: route
to the owning doc rather than restating it here.

> **Engine citations are symbol-based.** Resolve `%UE_PATH%` from the repository
> configuration and locate the named symbol in the listed engine file. Exact line
> numbers are intentionally not contracts because they drift between engine builds.

> **Maintenance contract.** This file is only valuable if it stays true. A PR that changes ANY of the following MUST update `canonical.md` in the same commit:
> - The drag/drop pipeline (router signatures, event-bus shape, drop-target interface, subsystem API).
> - The test policy (dev-loop, sabotage, spy, AutomationDriver migration state).
> - The Dev Loop Contract (exact-filter default, broad-filter refusal, override flags).
> - Plugin category boundaries or the UI-dependency rule.
> - `IInventoryDropCommandTarget` / `IInventorySurfacePolicyProvider` contract shapes.
> - Pimpl / friend / header-placement invariants.
> - CoreRedirects / migration policy.
>
> If your diff touches `Plugins/UI/ProjectInventoryUI/Source/**/Widgets/W_InventoryPanel.h`, `MVVM/InventoryDropRouter.h`, `Subsystems/InventoryUIDragHostSubsystem.h`, `scripts/ue/test/unit/{run_single,iterate,run_cpp_tests_safe}.ps1`, or any of the interface headers above, the reviewer will look for a matching diff in this file. If you're sure your change is invisible at this abstraction layer, say so in the PR description.

---

## 1. Purpose + audience

- Audience: agents and new contributors about to touch ALIS code, tests, or docs.
- Scope: the rules that save time and prevent rediscovery. Not a tutorial.
- When a section is thin on detail, follow its cited deep-dive.
- Top-level routing stays in the [documentation index](../README.md).

---

## 2. The ALIS mental model

Plugin categories and dependency direction are the load-bearing rules; most
bugs come from violating them.

- Plugin categories: `Foundation`, `Systems`, `Features`, `Gameplay`, `UI`,
  `Resources`, `World`, `Test`. Layout + invariants in
  [docs/architecture/plugin_rules.md](../architecture/plugin_rules.md).
- Dependency direction: `UI`, `Gameplay`, `Systems`, `Features` -> `Foundation`
  (`ProjectCore`) -> engine. Never the reverse.
- Cross-plugin integration = interface in
  `Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Interfaces/`
  (see [Interfaces/README.md](../../Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Interfaces/README.md)).
  Provider plugin implements; consumer plugin depends only on the interface.
- UI plugin -> Feature plugin dependency is ALLOWED for domain types
  (enums, gameplay tags, data structs). Logic must still cross via
  `ProjectCore` interfaces. See
  [docs/architecture/plugin_rules.md](../architecture/plugin_rules.md)
  "UI plugin dependency" section.
- Fail-soft rule: optional providers absent = no crash, no hard dep.
- Orchestration ownership: system A's orchestration MUST live in system A.
  Never in system B. Classic anti-pattern: mind orchestration inside
  Inventory/Vitals. See
  [docs/architecture/plugin_rules.md](../architecture/plugin_rules.md).

---

## 3. Use UE engine primitives. Do not reinvent.

For each area: what UE gives you, where it lives, when to prefer it.

- Drag/drop (Slate + UMG).
  - `FDragDropOperation` owns the cursor decorator; constructor calls
    `CreateCursorDecoratorWindow()` implicitly via `Construct()` - see
    `Engine/Source/Runtime/SlateCore/Public/Input/DragAndDrop.h`.
  - `UDragDropOperation::DefaultDragVisual` is the UMG hook; field at
    `Engine/Source/Runtime/UMG/Public/Blueprint/DragDropOperation.h`.
  - Routing is `FEventRouter::FBubblePolicy` - leaf widget under cursor
    gets first crack, bubbles up. See
    `Engine/Source/Runtime/Slate/Private/Framework/Application/SlateApplication.cpp`.
  - UMG drag-over / drop overrides ONLY compile on `UUserWidget`:
    `NativeOnDragOver` and `NativeOnDrop` at
    `Engine/Source/Runtime/UMG/Public/Blueprint/UserWidget.h`.
  - Rule: smallest semantic target owns the UMG handler. Do not build a
    custom full-screen overlay widget to "catch" drops. Slate's
    `CursorDecoratorWindow` is already that overlay.
- CommonUI.
  - `UCommonUserWidget` (not plain `UUserWidget`) for activatable-stack
    members. `UCommonActivatableWidgetContainerBase` for layer hosts.
    `UCommonButtonBase` for buttons needing gamepad/focus.
  - Plugin at `%UE_PATH%/Engine/Plugins/Runtime/CommonUI/`.
  - ALIS's `UProjectButton` intentionally NOT on `UCommonButtonBase` yet.
    Deferred; revisit when a gamepad/focus-driven button surface appears.
- MVVM.
  - ALIS uses custom `UProjectViewModel` + `VIEWMODEL_PROPERTY(Type, Name)`
    macro. NOT stock `UMVVMViewModelBase`.
  - The macro generates a protected `Update<Name>` setter (sets +
    notifies) and a public `Get<Name>` getter. NO public setter.
    Reflection-writer paths do NOT work. Add explicit `SetX(...)` that
    calls `UpdateX(...)` when a test or external system needs to write.
  - Full reflection-limitation note in
    [docs/testing/agent_ue_inspection.md](../testing/agent_ue_inspection.md).
- Automation driver (UE-native UI input).
  - `IAutomationDriverModule::Get().CreateDriver()` returns a driver that
    produces `IDriverSequence` (Selenium-style Press / MoveToElement /
    Release). Hooks the platform message handler via `PassThroughMessageHandler`
    so real Slate routing runs - timing-safe, no sleeps.
  - Prefer over `FSlateApplication::ProcessMouseButton*` direct calls for
    UI tests once the separate correctness track lands. Dual-run migration
    rule is documented in "AutomationDriver migration" below.
  - Engine files:
    `Engine/Source/Developer/AutomationDriver/Public/IAutomationDriver.h`,
    `IDriverSequence.h`, `LocateBy.h`.
- Slate diagnostics.
  - Widget Reflector + Slate Insights + `FSlateDebugging` broadcasts. Use
    BEFORE adding custom logging. Engine:
    `Engine/Source/Runtime/SlateCore/Public/Debugging/SlateDebugging.h`.
- Config / IO.
  - `FProjectPaths` (ProjectCore) for plugin-relative paths. JSON via
    `FJsonObject` + `FFileHelper`. No hardcoded paths in C++.

---

## 4. ALIS-specific drag/drop pipeline

Canonical flow for any inventory UI change:

```
Cell click -> UProjectGridCell::NativeOnMouseButtonDown -> DetectDragIfPressed
Drop       -> UW_InventoryCellDropTarget::NativeOnDrop (impl IInventoryDropTarget)
           -> Subsystem::CompleteDrop
           -> FInventoryDropRouter::Route(IInventoryDropCommandTarget&, Ctx,
                                          Target, &Resolution)
              -> VM method (real dispatch; Resolution.VMMethod is the SOT for
                 VMInvoked events)
           -> emits DropResolved -> Routed -> VMInvoked{real VMMethod}
              -> Completed
```

Invariants (CI-enforced, see
[Plugins/Test/ProjectIntegrationTests/.../InventoryArchitectureFitnessTests.cpp](../../Plugins/Test/ProjectIntegrationTests/Source/ProjectIntegrationTests/Private/Integration/InventoryArchitectureFitnessTests.cpp)):

1. Widgets MUST NOT `#include "MVVM/InventoryDropRouter.h"`.
2. Only classes implementing `IInventoryDropTarget` may override
   `NativeOnDragOver`/`NativeOnDrop` inside ProjectInventoryUI.
3. Widget-widget `Visibility` contract preserved (no sibling visibility
   poking during drag).
4. Every drag session emits exactly one terminal event
   (`Completed` or `Cancelled`).
5. No widget-side closures capturing global Slate state. Grep enforces.
   `GetDragDroppingContent()` is a debug probe, not a contract source.

---

## 5. Command target + policy provider interfaces

Two tight interfaces keep drag routing testable. Both currently live in
ProjectInventoryUI (not ProjectCore - no other plugin consumes them yet).

- `IInventoryDropCommandTarget`
  ([Plugins/UI/ProjectInventoryUI/.../IInventoryDropCommandTarget.h](../../Plugins/UI/ProjectInventoryUI/Source/ProjectInventoryUI/Public/Interaction/IInventoryDropCommandTarget.h)):
  exactly the 4 methods the router dispatches to: `RequestMoveItem`,
  `RequestTakeNearbyItemToContainer`,
  `RequestStoreItemInNearbyContainerAt`, `RequestEquipItem`.
  Implemented by `UInventoryViewModel`. Tests use `UInventoryViewModelSpy`
  to record dispatch.
- `IInventorySurfacePolicyProvider`
  ([Plugins/UI/ProjectInventoryUI/.../IInventorySurfacePolicyProvider.h](../../Plugins/UI/ProjectInventoryUI/Source/ProjectInventoryUI/Public/Interaction/IInventorySurfacePolicyProvider.h)):
  preview-time validation. Surfaces register with a tag; subsystem looks up
  policy by tag. NO closures over widget state.
- Current trade-off: `SetPolicyProvider` binds one object that satisfies
  BOTH roles. Revisit if a second command-target implementation appears.

---

## 6. Pimpl + internal-helper pattern for UClass split

When a UClass `.cpp` nears the 1000 LoC hard limit (section 10) and natural
split boundaries exist.

- Pure-logic helpers -> `namespace` in `Private/Widgets/` (or similar).
  Take const-ref to owner or a context struct. NO back-references.
  Example: [InventoryPanelGridLayout.h](../../Plugins/UI/ProjectInventoryUI/Source/ProjectInventoryUI/Private/Widgets/InventoryPanelGridLayout.h)
  (stateless namespace).
- Owned state helper -> `TPimplPtr<FHelperStruct>` member on the UClass.
  Helper lives in `Private/`. Forward-declare in owner's Public header.
  `TPimplPtr` handles forward-declared dtor correctly; `TUniquePtr` breaks
  UHT-generated vtable helpers. Example:
  [InventoryPanelSurfaceRegistry.h](../../Plugins/UI/ProjectInventoryUI/Source/ProjectInventoryUI/Private/Widgets/InventoryPanelSurfaceRegistry.h).
- Cross-TU method definitions -> second `.cpp` defining `UOwner::Method`
  bodies. No header change, no new class.
- Friend helpers OK when helper headers live in `Private/`. Avoid
  `Get*ReadOnly`/`Get*ForHelper` accessor blooms - use a context struct
  or `friend` instead.

---

## 7. Testing strategy

### Dev Loop Contract (dev-mode default)

Enforced at three layers: docs (this section), script refusal
([run_single.ps1](../../scripts/ue/test/unit/run_single.ps1),
[iterate.ps1](../../scripts/ue/test/unit/iterate.ps1),
[run_cpp_tests_safe.ps1](../../scripts/ue/test/unit/run_cpp_tests_safe.ps1)
`-Mode Dev`), and explicit override path (`-Mode Gate` / `-AllowBroadFilter`).

- Default to one exact test filter. An "exact full test name" is the full
  dot-separated path to one test (e.g.
  `ProjectIntegrationTests.UI.Framework.Inventory.CellDropTarget.BuilderWrapsEveryCell`).
  Exactness is recognized by matching the filter as a complete quoted
  literal in test source. That haystack covers `Plugins/Test` AND every
  plugin-owned `Tests/` directory (e.g. ProjectWorldEditor
  `Private/Tests`), so plugin-owned exact IDs never need a `-Mode Gate`
  override just to be recognized.
- Broad filters are forbidden in normal iteration:
  wildcards (`*`), comma/semicolon unions, `Group:` / `Filter:` prefixes,
  short prefix paths that match many tests (`ProjectIntegrationTests.UI`),
  and bare tag expressions.
- Broad verification is allowed only at end-of-slice, CI, or explicit user
  request. It must go through `-Mode Gate` or `-AllowBroadFilter` so the
  expensive choice is visible and intentional.
- After a small code change, run only the most directly affected test or
  the smallest explicit set. If uncertain, pick the smaller targeted test
  first and widen only if it fails or the user asks.
- Before an expensive build, generation, cook, or package, identify the changed
  output and the dependency that requires that gate. Reuse an accepted artifact
  when its authenticated inputs and acceptance property are unchanged. A green
  release gate is not an inner-loop requirement for an unrelated edit; a
  release candidate still needs the complete release acceptance route.
- Live Coding is allowed only for `cpp` body edits. Header, reflected
  type, `.Build.cs`, or module-dependency changes must use the full
  compile path. See section 7 deep-dive below.
- Tag filter syntax reference: **single-tag substring match only**.
  The configured engine's tag-filter CVar (`Automation.TestTagGlobalFilter`) uses
  `FTextFilterExpressionEvaluator` in `BasicString` mode, which does
  NOT evaluate `&&` / `||` / `!` - those are literal characters that
  fail to match. Pass one tag token like `Fast` or `Inventory` and let
  the pool filter (`Automation RunTests <prefix>`) narrow separately.

### Checkpoint scope and gate escalation

An operator review boundary is authority, not a suggestion. When the requested
sequence says to show, inspect, or review a candidate before approval:

1. Freeze the current separation of concerns and acceptance criteria before
   implementation. A regression envelope is not a second implementation
   workstream.
2. Iterate with schemas, static checks, exact L0 tests, and only the L1 seam
   needed by the target behavior. Check and Matrix are acceptance gates, not
   debugging loops.
3. Preflight every selected profile, fixture, expected count, and contract with
   the cheapest available validator before starting an expensive gate.
4. Run the target acceptance path before broader regression envelopes when its
   dependencies permit. This finds target orchestration defects before paying
   for unrelated coverage.
5. Before approval, create only isolated or reversibly restored state. Inspect
   that exact candidate, capture the requested runtime/visual evidence, report,
   and stop. Durable promotion, publication, or authority mutation - including
   World L3 enrollment - is forbidden until explicit operator approval.
6. After approval, freeze the tree, run the static plan, one fresh Check, only
   the selected regression Matrices, the target Matrix, and the authorized
   durable transaction. Do not reuse pre-approval evidence as final authority
   when the authenticated tree changed.

The default budget for each expensive gate is one initial run plus one
replacement after a necessary target-scope fix. Implementation authorization
includes standing approval to run the post-implementation checks needed to
finish the requested outcome, including a fresh authenticated gate after each
focused target-scope correction. Do not stop merely to ask permission to test.
A further failure requires an operator scope decision only when evidence calls
for broader product scope, changed acceptance criteria, durable authority not
already authorized, or another unresolved repetition of the same failure.
Monitor long jobs at meaningful stage changes; repeated fixed-interval polling
and transcript tails are not progress evidence.

Fix a regression-envelope failure in the current task only when the current
change directly violates a supported shared contract or the final acceptance
contract requires it. A stale or unrelated fixture, migration, cleanup, or
refactor is recorded with evidence and returned as a separate scope decision.
Required stable contract documentation remains part of implementation;
unrelated documentation polishing does not. Once the requested acceptance
criteria pass, stop changing that area.

### Evidence-driven debugging

When reality contradicts a green gate, an acceptance surface proves wrong,
or two implementation hypotheses are falsified:

- STOP speculative fixes.
- Preserve and measure the broken artifact.
- Acceptance measures the output consumed at the boundary.
- One hypothesis / one discriminating experiment at a time.
- Caches never veto explicit dirtiness.
- Correctness metrics compare output to authority; hashes prove identity only.
- Observe a correctness gate fail before trusting its pass.
- Reproduce the smallest fixture under the exact failing execution envelope
  (executable, flags, RHI/headless mode) before varying domain inputs.
- For unproven third-party/engine semantics, inspect the exact installed source.

Reading [Scientific debugging](scientific_debugging.md) is MANDATORY under those
triggers, before changing behavior.

### Gate scope and missing acceptance dimensions

A green gate proves only the invariant it was designed to test. Do not
report a narrow fidelity result as broader correctness.

Kazan's realization height gate compared Unreal's FINAL heightmap against
canonical elevation using canonical `height_quantization` as tolerance.
That is correct for a fidelity gate:

    "did Unreal faithfully reproduce canonical elevation?"

It passed because Unreal did.

The visible terracing already existed in canonical authority, so detecting it
required a separate surface-quality invariant. The missing gate was:

    "is canonical terrain itself an acceptable surface?"

`surface.py` supplies that independent acceptance dimension.

Before trusting a numeric gate ask:
1. What exact invariant does this gate prove?
2. What authority is it comparing against?
3. Which defect classes are structurally invisible to it?
4. Does another quality dimension need its own independent gate?

A genuinely self-referential gate is still invalid when the value being
tested defines its own acceptance criterion without independent authority.
But source precision/quantization is a legitimate tolerance for a fidelity
comparison against that source.

Corollary: name and report gates narrowly. `FINAL matches canonical` is
honest; `terrain verified` is not unless all required terrain-quality
dimensions have independently passed. A green presence/placement census is
never visual approval.

Worked example and the surface-quality counterpart gate:
[tools/World/VisualVerification/README.md](../../tools/World/VisualVerification/README.md)
and ProjectWorld pitfalls 11-14.

### Tag taxonomy (orthogonal; registered via `REGISTER_SIMPLE_AUTOMATION_TEST_TAGS`)

Three orthogonal required dimensions. Every new test should carry one
from each:

| Dimension | Values | Intent |
|---|---|---|
| **Speed** | `Fast` / `Slow` | Fast = single-digit seconds; Slow = >5s or requires painted widget frames |
| **Kind** | `Unit` / `Integration` / `E2E` / `Architecture` | Unit = pure logic; Integration = uses subsystems/components; E2E = real Slate routing + cached geometry; Architecture = source-tree fitness test |
| **Area** | `Inventory` / `UI` / `Vitals` / `Mind` / `Dialogue` / `Loading` / `AutomationDriver` / ... | Feature or system the test targets; extend the list as new areas land. Keep Area entries to concrete subsystems or plugins - avoid intent-labels like "Correctness" or "Quality" (see Optional classification below). |

Optional, non-required classification (at most one; use only when a test
genuinely belongs to an initiative that spans multiple Areas):

- `Correctness` - infrastructure that proves correctness across the
  project but is not bound to a specific feature (e.g. AutomationDriver
  smoke when it later covers drag/drop across multiple plugins).
- `Performance` - reserved for perf-regression tests.
- Other initiative sentinels as they appear.

Do not stack these with Area redundantly. If a test has a clear Area,
use that; if it is infrastructure spanning Areas, reach for
`Correctness` instead.

Reserved phase-sentinel tag:

- **`Phase5`** - marks tests that belong to the Phase 5 inventory
  verification harness. Not applied to existing tests; reserved for the
  3 new Layer A E2E tests (pickup, equip-grant, container swap) when
  they land. Once present, `verify.ps1`'s default switches from
  `-TagExpression Inventory` to `-TagExpression Phase5`.

Registration shape (concatenated literal; order is arbitrary because CLI
filters match substring):

```cpp
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
    FMyTest,
    "ProjectIntegrationTests.Area.Sub.TestName",
    "[Fast][Integration][Inventory]")
```

### Single-token CLI filter - ALIS wrapper contract, not an engine limit

CLI filtering rule: always pass a **single token** to `-Tags`. Never
compose. Never use boolean operators.

This is the **ALIS wrapper contract**, NOT a claim about every UE tag
API. The engine offers richer APIs at the framework level -
`FAutomationTestFramework::GetTestFullNamesMatchingTagPattern`, advanced
search syntax, complex `FTextFilterExpressionEvaluator` modes. Those are
available to C++ callers inside the editor.

The ALIS test-iteration wrapper (`iterate.ps1` / `run_single.ps1` /
`verify.ps1`) deliberately narrows the surface: it dispatches via the
`Automation.TestTagGlobalFilter` CVar, which uses
`FTextFilterExpressionEvaluator` in `BasicString` mode = literal
substring match. Pretending the wrapper accepts boolean expressions
would be a lie - the CVar would treat `&&` as a literal character.

If you need intersection from the script layer, compose via the pool
filter (`-TestFilter <prefix>` + `-Tags <token>`). If you need more
than substring matching, either (a) extend the wrapper to call a richer
framework API directly, or (b) stay in C++ and call the framework API
from a bespoke test harness - but don't lie about what the CVar does.

The taxonomy is catalog metadata for humans and future slicing; the
wrapper contract is substring-only single-token.

### Deep-dive notes

- Dev loop: single-test filter on a warm editor via
  [run_cpp_tests_safe.ps1](../../scripts/ue/test/unit/run_cpp_tests_safe.ps1)
  (auto-detects the persistent editor via PID file). For strict exact-only
  dispatch use
  [run_single.ps1](../../scripts/ue/test/unit/run_single.ps1);
  for the default dev entrypoint with filter-shape validation and
  (Phase 1+) compile-mode selection use
  [iterate.ps1](../../scripts/ue/test/unit/iterate.ps1).
- Persistent editor trio:
  [persistent_editor_start.ps1](../../scripts/ue/test/unit/persistent_editor_start.ps1),
  [persistent_editor_run.ps1](../../scripts/ue/test/unit/persistent_editor_run.ps1),
  [persistent_editor_stop.ps1](../../scripts/ue/test/unit/persistent_editor_stop.ps1).
  File-IPC via `command.txt` polled by `ProjectIntegrationTestsModule`.
  Cold-to-warm measured 3.2x on 2026-04-23 (cold 42.9s, warm 13.5s on an
  exact single test; warm+LC 25.0s; broad gate 24.1s for 83 tests).
  `iterate.ps1` auto-detects the persistent editor via the PID file and
  prints a tip on the cold path pointing at the start script.
- Union filters (`ProjectIntegrationTests.UI` etc.): CI / nightly only.
- Sabotage verification (MANDATORY for new tests): mute one specific
  emit/line, run the test, confirm a specific-message failure, restore.
  Batch via `#define SLICE_N_SABOTAGE` toggles.
- Spy pattern: `UInventoryViewModelSpy` records
  `LastCall`/`LastInstanceId`/etc. Assert on real VM dispatch, not event
  narration. Spy at
  [Plugins/Test/ProjectIntegrationTests/.../InventoryViewModelSpy.h](../../Plugins/Test/ProjectIntegrationTests/Source/ProjectIntegrationTests/Private/Support/InventoryViewModelSpy.h).
- Test theater trap: event-sequence assertions alone do not prove
  behavior. Pair every dispatch test with a spy `LastCall` assertion.
- AutomationDriver E2E (Slice 2/3, planned): real Slate routing via
  `IAutomationDriver`. One test per drag scenario once it lands.
- Synthetic-input fallback: `CellHost->NativeOnDrop(cached-geometry,
  synthetic-FDragDropEvent, DragOp)` is acceptable for headless CI but
  does NOT prove Slate bubble routing (AutomationDriver closes that gap).
- `AddExpectedError` quarantine: exception for tracked known-issues only.
  Must not become a standard pattern.

### Public repo migration policy

ALIS is a clean public C++-owned repo by default. Do not carry fake legacy
support in public code/docs/config.

If a design has no Blueprint logic, no Blueprint component ownership, no
map-placed overrides, and no JSON/data ownership, migrate in one pass. Remove
obsolete reflected fields, fallback code, compatibility tests, and docs in the
same change. Do not add CoreRedirects, deprecated UPROPERTY compatibility, or
Blueprint compatibility for ownership paths the project does not use.

CoreRedirects are local migration scaffolding ONLY for real tracked content
that cannot be migrated in one pass. Resave affected assets and remove the
redirect before merge. If a feature needs committed legacy support, update this
SOT first because that is not the ALIS default. Procedure:
[docs/editor/class_migration.md](../editor/class_migration.md).

### Dev loop pitfalls (verified 2026-04-23)

Empirical gotchas seen during the speedup pass. Read before your first
LC or cold-path run in a new workspace.

1. **PDB stripped after a packaging run = LC silently disabled for that module.**
   The packaging flow defaults to `-nodebuginfo` which removes the base
   `UnrealEditor-<Module>.pdb`. Next LC attempt against that module logs
   `LogLiveCodingServer: Warning: No PDB file found for module ... Live
   coding will be disabled for this module.` Followed by
   `LogLiveCoding: Error: Live coding canceled`, which `livecoding_sync.ps1`
   correctly treats as LC_TIMEOUT after 60s. `iterate.ps1` then aborts
   with exit 5 (stale-state protection working as designed). **Fix:**
   rebuild the affected module before resuming LC:
   ```powershell
   scripts/ue/build/rebuild_module_safe.ps1 -ModuleName ProjectIntegrationTests
   ```
   Module rebuild can take 2-10 min (first attempt sometimes timeouts at
   the default 600s and auto-retries; this is the `rebuild_module_safe.ps1`
   circuit breaker doing its job, not a hang).

2. **First LC patch after a module rebuild is slower (~24s vs ~14s steady-state).**
   Expected: LC internal caches prime on the first compile. Do NOT
   interpret a 24s first run as a regression; look at runs 2+.

3. **First cold run in a session is a filesystem/shader-cache warmup outlier.**
   Seen 2026-04-23: cold runs 73.9s / 42.3s / 42.9s. Run 1 was ~73s,
   runs 2+ were ~43s. Measurement rules already say "exclude the first
   run after editor start" - this applies to cold sets too.

4. **Broad gate cost is 20-30s on this repo, not 60-70s.**
   The original speedup plan estimated `ProjectIntegrationTests.UI`
   broad-filter gate at 60-70s. Empirically (83 tests, warm editor) it
   is ~24s. Still not right for dev iteration, but the cost delta vs
   exact warm is smaller than the original plan implied.

5. **Tag CVar is BasicString substring, not boolean.** Covered in section 7
   "Single-token CLI filter" above but worth repeating here: `-Tags
   "Fast && !Slow"` matches nothing because `&&` becomes a literal. Pass
   one token; compose via the pool prefix.

6. **Static initializers are NOT reached by Live Coding.** LC patches
   function bodies. Removing a `REGISTER_SIMPLE_AUTOMATION_TEST_TAGS`
   call and LC-patching does NOT drop the test from the tag set -
   requires a cold editor restart. Implications: tag registration
   sabotage tests can only run cold.

7. **NEVER `TaskStop` / Ctrl-C / kill `rebuild_module_safe.ps1` mid-build.**
   The script wraps `Build.bat` -> `cmd.exe` -> `dotnet UnrealBuildTool.dll`.
   Killing the PowerShell wrapper does NOT cascade into the cmd / dotnet
   children - they keep running, and UBT holds the **global UE build
   mutex** (machine-wide, not per-project). Any new `Build.bat`
   invocation (your VS Code preLaunchTask, a fresh
   `rebuild_module_safe.ps1`, an `iterate.ps1` cold path) hangs forever
   on:
   ```
   Build.bat is already running, waiting for existing script to terminate...
   ```
   The orphan UBT eventually finishes (or doesn't), but in the meantime
   the user's editor build is dead-stuck.

   **If you started a wrapper, you own its lifetime.** Either let it
   complete naturally (timeouts + retries are built in) or, if you
   absolutely must terminate, kill the **entire process tree** in one
   shot, child-first:
   ```powershell
   # Identify the chain (parent PID = your wrapper):
   Get-CimInstance Win32_Process | Where-Object {
       $_.Name -in @('dotnet.exe','cmd.exe') -or
       $_.CommandLine -match 'Build\.bat|UnrealBuildTool|rebuild_module'
   } | Format-Table ProcessId,Name,@{n='Parent';e={$_.ParentProcessId}},CommandLine -Auto
   # Kill children (UBT/dotnet) first, wrappers last:
   Stop-Process -Id <UBT-PID>,<cmd-PID>,<wrapper-PID> -Force
   ```
   `TaskStop` on the agent side stops only the **shell that launched**
   the script; it leaves grandchildren behind. `taskkill /F /T /PID
   <wrapper>` from cmd.exe does cascade, but Git Bash mangles the `/F`
   flag - call it through `powershell.exe Stop-Process -Force` instead.

   **Detection:** if a fresh `Build.bat` prints "already running,
   waiting for existing script to terminate" and never moves, dump the
   process tree with the snippet above. Any `dotnet.exe` running
   `UnrealBuildTool.dll` whose parent is a dead PID = orphan, kill it.
   No stale lockfile cleanup needed; the mutex is OS-level and dies
   with the process.

### Testing architecture decisions (standing)

SOT for decisions that outlive any one phase doc. If a todo says "decided
X," the actual decision + rationale + revisit conditions live here.

#### Phase 4 Low Level Tests - DEFERRED on launcher engine

Decided 2026-04-23 after hitting a hard installed-engine limit
(`RulesAssembly.cs:679` - "Targets with a unique build environment cannot
be built with an installed engine"). Any `TestTargetRules` that sets
`bCompileAgainstCoreUObject = true` diverges from the shared build env
and is blocked. The candidate namespaces (`InventoryViewModelActionRules`,
`InventoryViewModelPlacement`) touch USTRUCTs, so this is fundamental.

ALIS uses the launcher-installed engine by default and does not plan to
switch. Not worth the trade:
- ~100-line refactor to strip USTRUCTs from the candidate namespaces =
  invasive to production code just to enable tests.
- The marginal win (~3s LLT vs ~10-15s warm-editor automation) does not
  justify that cost against a KISS workflow.

Revisit ONLY if one becomes true:
1. A coexistence path (source tree at a separate UE root used for unique-
   build-env targets) is proven to NOT invalidate the launcher UBT cache.
2. Warm-editor iteration stops being fast enough in real work.
3. A specific pure-logic hot spot emerges that genuinely cannot be
   tested through the VM / automation path.

Epic's documented position: installed builds use a shared build
environment; Test targets that need unique env require source builds.
See [forum thread](https://forums.unrealengine.com/t/targets-with-a-unique-build-environment-cannot-be-built-with-an-installed-engine/1353217).

#### Phase 5 Layer A coverage - 3 in-action tests max

Decided 2026-04-23. The Phase 5 inventory harness Layer A adds EXACTLY
these three E2E tests, each tagged `[Slow][E2E][Inventory][Phase5]`:

1. Pickup - world item -> inventory via nearby panel (direct + deferred-
   pickup race path from the ObjectDefinition cache work).
2. Equip-grant lifecycle - equip item -> grant-bound container appears
   (pockets/backpack) -> unequip -> container disappears.
3. Container swap - open container A -> close -> open container B -> UI
   rebinds cleanly.

Non-goal: do NOT duplicate drag/drop coverage already in
`InventoryDragE2ESyntheticInputTests.cpp`. Those flows stay in that file.

When Layer A lands, `verify.ps1`'s default `-TagExpression` switches from
`"Inventory"` to `"Phase5"` in the same landing commit.

#### Phase 1b Live Coding failure handling - manual only

Decided 2026-04-23. When Live Coding returns any non-OK terminal status
(LC_FAILED / LC_TIMEOUT / LC_UNAVAILABLE), `iterate.ps1` aborts with
exit 5 and prints the 4-step manual recovery (stop editor / rebuild
module / restart editor / rerun iterate). No automatic "stop->rebuild->
restart" orchestration sequence.

Reason: stale-state dispatch is forbidden (Phase 1 hard invariant), and
the automatic recovery flow would need robust handling for partial
failures (UBT lock contention, editor crashes mid-build, etc.) - real
engineering that is not justified by the incident rate. The 4-step
manual path is simple, honest, and rare enough to be acceptable.

Revisit if the manual recovery becomes a daily pain point.

#### AutomationDriver migration - dual-run for one release cycle

Decided 2026-04-23. Once real AutomationDriver drag/drop coverage lands
(beyond the current `ModuleLoadsAndCreatesDriver` smoke), keep both
driver-based AND the existing synthetic-input tests
(`InventoryDragE2ESyntheticInputTests.cpp`) running for one release
cycle. After that cycle, if the driver-based pass is stable, delete the
synthetic duplicates.

Reason: AutomationDriver hooks the platform message handler for real
Slate bubble routing; synthetic input bypasses that. The driver path is
the long-term correctness gate, but the synthetic path is known-stable.
One cycle of overlap catches driver-specific flakiness before we lose
the synthetic safety net.

---

## 8. Known-bug / pitfall archive

Per-plugin `docs/pitfalls.md` files are the authoritative archive. Read the relevant one
before editing that plugin. Add a new entry after fixing a non-trivial
bug there.

One live known timing bug (interaction-hold + world-container) is in the
active execution queue; the quarantine pattern that keeps it visible
while unfixed is covered in section 7 ("`AddExpectedError` quarantine").

---

## 8.5. Plugin runtime-data staging (mandatory in Build.cs)

Standing rule, validated by CI guard on every dev-loop check and every
package run. Landed 2026-04-28 after a Shipping regression where Mind
journal lost the Grandpa "needs water" mapping.

If a plugin reads JSON (or any raw text/binary) at runtime via
`FProjectPaths::GetPluginDataDir(TEXT("X"))`, then `<X>`'s `.Build.cs`
MUST stage the `Data/` directory through `RuntimeDependencies`. The
canonical helper (used by ProjectUI / Orchestrator / ProjectSinglePlay /
ProjectMind) is:

```csharp
private void StageDataDir(ReadOnlyTargetRules Target)
{
    if (Target.Type == TargetType.Editor) return;
    string DataDir = Path.Combine(PluginDirectory, "Data");
    if (!Directory.Exists(DataDir)) return;
    RuntimeDependencies.Add(Path.Combine(DataDir, "..."), StagedFileType.UFS);
}
```

`UFS` and `NonUFS` are NOT interchangeable -- they declare two different
runtime access policies (Epic SOT). Pick deliberately:

- `StagedFileType.UFS` -- files are accessed through `IFileManager` /
  `FFileHelper`, MAY be packed into the pak/IoStore container, and
  benefit from compression and tamper resistance. Use this for any
  authored runtime data the player should not hand-edit.
- `StagedFileType.NonUFS` -- files MUST stay as loose files on disk
  next to the binary. Use this only when post-cook hand-edit by an
  end user (or designer) is an explicit design goal -- e.g. ProjectVitals
  intentionally exposes `vitals_config.json` for tweaking without a
  recook. The cooked artifact will always have a loose copy.

For ALIS today the canonical helper uses `UFS`, which is correct for
authored gameplay JSON (mappings, layouts, rules). The configured engine happens to
also stage these as loose files when the file extension is unknown to
the cooker (verified 2026-04-28 via `FinalCopyWin64_NonUFSFiles.txt`),
but DO NOT rely on that as a permanent guarantee -- if the cook later
starts packing them into pak the runtime read still works because
`FFileHelper::LoadFileToString` is pak-aware. The "either looks loose"
observation is a current cook-side artifact, not a contract.

The proof chain that the data is actually reachable in Shipping has
three stages, in order:

1. Pre-package validator passes (the gap detector caught the bug).
2. Post-package archive verification passes (the cook actually copied
   or packed the files).
3. Cooked Shipping runtime log shows
   `LogProjectMindService: Loaded N dialogue thought signal mappings ...`
   (or the equivalent loader log for the affected plugin).

Stages 1 and 2 are automated by the validator. Stage 3 requires running
the cooked binary at least once after a packaging change that touches
plugin runtime data. Treat (3) as part of the release gate, not as a
nice-to-have.

Why a dedicated rule: missing staging is silent. The cooked binary boots,
the runtime read returns "file missing", the loader logs a Warning and
falls back to defaults. No crash, no test failure -- the feature just
disappears in Shipping. Do not assume future plugin authors will remember
this; the validator does.

Enforcement:
- [`scripts/ue/check/governance/validate_plugin_data_staging.py`](../../scripts/ue/check/governance/validate_plugin_data_staging.py)
  -- pre-package guard, also runs as part of `validate_all.bat`. Errors
  if any plugin reads runtime data without staging in its `Build.cs`.
- Same script with `--archive-root <dir>` -- post-package smoke check
  invoked from [`scripts/ue/package/package_release.ps1`](../../scripts/ue/package/package_release.ps1)
  after `BuildCookRun` succeeds. Confirms the cook actually copied the
  files into the staged build.

This rule covers exactly the runtime-read-at-disk case. It does NOT cover
UE asset-managed content (`.uasset`, GFP-cooked content); those have
their own pak-via-AssetManager pipeline.

---

## 8.6. Public mirror is text-only (no binaries, no Git LFS pointers)

Standing rule for the public mirror flow
([scripts/git/mirror/mirror_to_github.sh](../../scripts/git/mirror/mirror_to_github.sh)).
Landed 2026-06-29 after a GH008 push rejection (an LFS-tracked README image
survived filtering and was emitted as a pointer the mirror never uploaded).

The public GitHub mirror is a filtered, sanitized, fresh-pushed snapshot of
code, docs, and text data ONLY. The filter is denylist-first: a tracked file is
published unless it (or a parent dir) matches a rule in `mirror.exclude`. Folder
name alone does NOT gate publication -- plugin `Data/` JSON schemas and
data-driven generator inputs (loot, dialogue, UI layouts, vitals) are
intentionally public references, as is source that lives in a folder named
`Data`. Only forbidden file TYPES (UE assets, binaries, media, key material) are
stripped from those dirs, enforced by `validate_filtered_tree`. A generic
binary-content guard in the validator (`grep -I` NUL-byte detection) fails any
non-empty binary file regardless of extension, so "text data only" is enforced by
content, not just an extension denylist. Do NOT re-add a blanket
`Plugins/**/Data/**` exclude: it silently dropped both generator JSON and
`Source/**/Data/*.cpp|.h` code, publishing plugins that could not compile. Two
hard rules:

1. No Git LFS pointers in the mirror. A pointer whose object was never
   uploaded to the mirror remote triggers GH008 ("unknown Git LFS objects")
   and rejects the ENTIRE push. Defenses, both in `mirror_to_github.sh`:
   `neutralize_lfs_attributes` strips `filter=lfs` from the snapshot
   `.gitattributes` so any surviving LFS-tracked file commits as a plain blob;
   `validate_filtered_tree` then hard-fails on any file whose full SHAPE is a
   real LFS pointer, via `is_lfs_pointer_file` (line 1 == the version header,
   line 2 == `oid sha256:<64 hex>`, line 3 == `size <int>`, CR stripped for
   CRLF). Shape-matching is deliberate: an anywhere-in-file substring match
   false-positives on every file that merely names the signature (this doc, the
   foliage-recovery todo, the mirror script's own detector) while a genuine
   leaked pointer always has all three lines. Do not loosen it back to a
   substring or first-line-only match, and do not remove either defense.
   `todo/**` is intentionally public (text), so the validator - not an
   exclude rule - is what must stay precise.

2. No binary media in the mirror. Raster images, video, audio, 3D, and source
   art are excluded by extension in `mirror.exclude` AND hard-failed by
   `validate_filtered_tree`. SVG is intentionally allowed (it is text).
   README/doc images are embedded via absolute `https://fall.is/...` URLs;
   the site repo (fall.is) is the media single source of truth. Never use a
   repo-relative binary path in a mirrored doc.

Related site-repo rule: never Git-LFS-track a file the Jekyll site must serve.
GitHub Pages "deploy from a branch" builds Jekyll itself and does NOT
materialize LFS objects, so an LFS-tracked image deploys as broken pointer
text. Commit small web images as plain blobs under `/assets/images/`; never
LFS-track a file under ~10 MB.

---

## 9. Deferred-pickup + loud-error pattern

Landed 2026-03. See `Plugins/Features/ProjectInventory/docs/pitfalls.md`
("object definition cache" entries) for the plugin-side contract.

- `FInventoryAddOutcome` with `bDeferred` / `Fail` enum.
- Three-layer contract:
  1. Warmup (Phase 6) expands `ObjectCatalog`.
  2. `Internal_AddItem` returns the outcome.
  3. `FInventoryInteractionHandler` queues deferred intents, broadcasts
     `OnInventoryError` on terminal states.
- Use as the canonical pattern whenever a cross-async-load race is
  possible.

---

## 10. Mega-file baseline + file-size guardrail

The hard rule:

- `< 700 LOC`: edit freely.
- `700-1000 LOC`: ok to add, but state WHY the file is still the right
  home and what the next SRP seam would be.
- `>= 1000 LOC AND your edit adds > ~30 net lines`: STOP. Either land
  the new code in a new sibling / helper / subsystem (preferred), or
  open a planning exchange before proceeding.
- The baseline set MUST NOT grow. A new entry (file crossing 1000 LOC
  because of your change, or baseline file growing > 5%) is a commit
  blocker. Reducing or eliminating entries is welcome; update this
  table in the same commit.

**UE-specific guidance:**

- Prefer **callspace split** over "N sibling components each implementing
  one interface": UI intent -> RPC edge -> server authority subsystem
  (`UWorldSubsystem` with `ShouldCreateSubsystem` gated to server) ->
  pure-logic operation object -> storage authority. Each layer is small,
  testable, and honest about its callspace.
- Never call `ULocalPlayerSubsystem` helpers from authority-only paths.
  Use a server-gated `UWorldSubsystem` for authoritative validation.
  See Epic docs on `UWorldSubsystem::ShouldCreateSubsystem`.
- Component RPCs have overhead vs actor RPCs; prefer `APlayerController`
  RPC endpoints for new network edges per Epic docs on "Replicating
  Actor Components".

**ALIS first-party mega-file baseline (UPDATE on every refactor):**

Regenerated 2026-04-24. All seven entries are actual first-party source
files currently over the 1000-LOC limit. Do not touch this table by
hand -- regenerate it from the check command below on every refactor
that changes the set.

| File | LOC | Plugin | Notes |
|---|---:|---|---|
| `Plugins/Resources/ProjectObject/Source/ProjectObject/Private/Spawning/ObjectSpawnUtility.cpp` | 1766 | ProjectObject | Legacy god-helper; not actively refactored. Candidate for callspace split (spawn vs wire-up vs cleanup). |
| `Plugins/Gameplay/ProjectMind/Source/ProjectMind/Private/Services/MindServiceImpl.cpp` | 1738 | ProjectMind | Mind service monolith; not actively refactored. |
| `Plugins/Features/ProjectInventory/Source/ProjectInventory/Private/Components/ProjectInventoryComponent.cpp` | 1663 | ProjectInventory | Down from 4083 after 2026-04-23/24 refactor. **Final authority/session ownership landed** (not "final callspace" -- the RPC-edge placement below is still open): world-container session state + authoritative TryBegin/End/Take/Store/Move/TakeAll all live in `UProjectWorldContainerAuthoritySubsystem` (server-gated UWorldSubsystem). Component holds no session member state; bridge Impls are thin routers that delegate to the authority subsystem on authority, or to a Server RPC on client. `UProjectContainerSessionSubsystem` (ULocalPlayerSubsystem) is a pure client-side UI cache: RegisterOpenedSession from Client RPC, CloseSessionLocal, GetSessionContainerView, Has*/IsSessionActive queries. Instigator identity uses `FObjectKey` so mid-session actor destruction (e.g., tests tearing down the controlling PC) does not break session resolution. **Remaining RPC-edge relocation** (next slice, optional/perf-driven, not a correctness blocker): migrate the 14 `Server_Request*` RPCs off this component onto `APlayerController` or a dedicated net-endpoint component (Epic docs: actor RPCs have less overhead than actor-component RPCs). Further TU reductions (equipment, queries, save) also pending. |
| `Plugins/UI/ProjectInventoryUI/Source/ProjectInventoryUI/Private/MVVM/InventoryViewModel.cpp` | 1656 | ProjectInventoryUI | VM doing double duty (inventory state + drag/drop routing + surface dispatch). Candidate to split by responsibility. |
| `Plugins/Boot/Orchestrator/Source/OrchestratorCore/Private/OrchestratorCoreModule.cpp` | 1598 | Orchestrator | Boot orchestrator monolith; legacy. |
| `Plugins/UI/ProjectInventoryUI/Source/ProjectInventoryUI/Private/Widgets/W_InventoryPanel.cpp` | 1339 | ProjectInventoryUI | Main panel widget; drag/drop wrappers + grid builder + binder logic in one file. Cross-TU split pattern (pimpl per section 6) already used here; continue. |
| `Plugins/Gameplay/ProjectObjectCapabilities/Source/ProjectObjectCapabilities/Private/LootContainer/LootContainerCapabilityComponent.cpp` | 1256 | ProjectObjectCapabilities | Storage authority for loot containers. Candidate to split Consume/Store policy vs data access. |

Test files with many tests are out of scope (accepted by test-file
convention -- split by test category when they grow). Third-party /
vendor code under `Plugins/Local/` and `Plugins/InstanceArrayTool/` is
also out of scope.

**Check before commit (listing, not count):**

```bash
# Lists all first-party source files over 1000 LOC (excludes tests,
# Intermediate/, .gen.*, and third-party Local plugins).
find Plugins/Foundation Plugins/Systems Plugins/Features \
     Plugins/Gameplay Plugins/UI Plugins/Resources Plugins/World \
     Plugins/Boot Source -type f \( -name "*.cpp" -o -name "*.h" \) \
  | grep -v Intermediate/ | grep -v "\.gen\." \
  | grep -v "/Tests/" | grep -v "Tests\.cpp$" \
  | xargs wc -l 2>/dev/null \
  | awk '$1 >= 1000 && $2 != "total" {print}' \
  | sort -rn
```

---

## 11. Cross-refs + deep dives

Project docs:
- [docs/architecture/plugin_rules.md](../architecture/plugin_rules.md) - plugin category + dependency rules.
- [docs/architecture/conventions.md](../architecture/conventions.md) - code conventions + feature-plugin requirements.
- [docs/architecture/principles.md](../architecture/principles.md) - high-level principles.
- [docs/editor/class_migration.md](../editor/class_migration.md) - CoreRedirects + resave procedure.
- [docs/testing/automation.md](../testing/automation.md) - automation test conventions.
- [docs/testing/unit_tests.md](../testing/unit_tests.md), [integration_tests.md](../testing/integration_tests.md), [smoke_tests.md](../testing/smoke_tests.md).
- [docs/testing/troubleshooting.md](../testing/troubleshooting.md) - test failure triage.
- [docs/testing/agent_ue_inspection.md](../testing/agent_ue_inspection.md) - `VIEWMODEL_PROPERTY` reflection limits.
- [docs/build/workflow.md](../build/workflow.md), [packaging_guide.md](../build/packaging_guide.md), [build/README.md](../build/README.md).
- [docs/debugging/crash_investigation.md](../debugging/crash_investigation.md) - crash + hang triage.
- [Plugins/UI/ProjectInventoryUI/docs/architecture.md](../../Plugins/UI/ProjectInventoryUI/docs/architecture.md) - drag/drop + surface registry design.
- [Plugins/Foundation/ProjectCore/.../Interfaces/README.md](../../Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Interfaces/README.md) - contract-first integration.

Execution state lives in the `todo/` tree; its root README owns the lifecycle
categories, including externally blocked parked work. This file stays the
architecture SOT, so do not use task links here as standing references.
Completed and cancelled task records retain execution history only after
durable facts move to their owners.

Top-level routers:
- [README.md](../../README.md) + [docs/README.md](../README.md).

Key engine paths (configured engine at `%UE_PATH%/`):
- `Engine/Source/Developer/AutomationDriver/Public/IAutomationDriver.h` - driver entry point.
- `Engine/Source/Developer/AutomationDriver/Public/IDriverSequence.h` - Press/Move/Release sequence API.
- `Engine/Source/Developer/AutomationDriver/Public/LocateBy.h` - element locators.
- `Engine/Source/Runtime/SlateCore/Public/Input/DragAndDrop.h` - `FDragDropOperation`, `CursorDecoratorWindow`.
- `Engine/Source/Runtime/UMG/Public/Blueprint/DragDropOperation.h` - `DefaultDragVisual`.
- `Engine/Source/Runtime/UMG/Public/Blueprint/UserWidget.h` - `NativeOnDragOver` / `NativeOnDrop`.
- `Engine/Source/Runtime/Slate/Private/Framework/Application/SlateApplication.cpp` - `FEventRouter::FBubblePolicy` and drag routing.
- `Engine/Source/Runtime/SlateCore/Public/Debugging/SlateDebugging.h` - Slate broadcast hooks.
- `Engine/Plugins/Runtime/CommonUI/` - CommonUI base classes.
- `Engine/Source/Developer/AutomationController/Private/AutomationCommandline.cpp` - `RunTests` / filter handling.
