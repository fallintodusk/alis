# Inventory In-Action Verification Harness

Top-level composer for the 3-layer inventory verification harness. Scope
and Layer A coverage rule live in
[docs/agents/canonical.md](../../../../docs/agents/canonical.md)
"Phase 5 Layer A coverage - 3 in-action tests max".

## Purpose

Prove inventory drag-drop, container view, pickup, and equip **in action**
(real end-to-end flows) across any code change. Complements the narrow
exact-filter dev loop delivered in Phases 0-3 with a cheap full-sweep.

## Layers

| Layer | Artifact | Owner | Status |
|---|---|---|---|
| A | automation tests (single-tag narrowed via Phase 2 CVar filter) | tagged inventory tests + future Phase 5 E2E tests | partial - existing tagged tests in Phase 2; new Phase 5 E2E tests deferred |
| B | UI state dump | `tools/agentic/inventory/dump_report.py` | scaffold only (awaits Layer A checkpoints) |
| C | screenshot capture | `capture_phase5.ps1` (UE HighResShot via warm editor) | implemented |

## Run

```powershell
# all three layers
.\verify.ps1

# subset
.\verify.ps1 -Layers A,B
.\verify.ps1 -Layers A

# custom tag (default: "Inventory"; single-token per ALIS wrapper contract)
.\verify.ps1 -TagExpression "Fast"
```

### Warm-editor requirements

| Layer | Needs warm persistent editor? | Why |
|---|---|---|
| A (tests) | **YES** | Tag-filtered runs dispatch via `iterate.ps1 -Tags` -> `persistent_editor_run.ps1`, which sets the `Automation.TestTagGlobalFilter` CVar. `iterate.ps1` explicitly refuses tag runs against a cold path (there is no cold analog for `SetTagFilter`). Start the editor with `scripts/ue/test/unit/persistent_editor_start.ps1` before running. |
| B (dump) | No | Pure Python; reads `Saved/Dumps/Inventory.json` if present. Runs headless. |
| C (screenshot) | **YES** | Uses `HighResShot` console command dispatched via the warm editor's `command.txt` IPC. SKIPs with exit 2 if no warm editor. |

So in the current implementation, **both Layer A and Layer C require a
warm editor**. A tick that runs only Layer B can skip it.

### Tag filter syntax (ALIS wrapper contract)

The **wrapper contract exposed by `verify.ps1` and `iterate.ps1`** is
deliberately stricter than the underlying UE framework: pass a single
token only (e.g. `Fast`, `Inventory`, `Phase5`). No boolean operators,
no unions, no brackets.

**Why stricter than UE?** UE's framework-level tag APIs
(`GetTestFullNamesMatchingTagPattern`, advanced search syntax) ARE more
capable. But ALIS's test-iteration path currently routes through the
`Automation.TestTagGlobalFilter` CVar, which uses
`FTextFilterExpressionEvaluator` in `BasicString` mode - literal
substring match only. Wrapping that CVar with a boolean-looking API
would be a lie. The ALIS contract says exactly what the wrapper does:
single-token substring filtering. Full rationale in
`docs/agents/canonical.md` "Dev Loop Contract".

## SOLID invariants

Enforced by [scripts/ue/check/phase5_harness_invariants.ps1](../../check/phase5_harness_invariants.ps1)
(PowerShell check is the right tool match for script-layer invariants;
a C++ fitness test was originally planned but replaced since Phase 5
layers are PowerShell + Python + Markdown).

1. Layer A does not import Layer B or C helpers.
2. Layer B runs headless (works with `-NullRHI`).
3. Layer C does NOT parse dumps or assert state; only captures pixels.
4. `verify.ps1` is the only module that knows all three exist.
5. Each layer's failure mode is loud: non-zero exit + one-line summary.

Run the check:

```powershell
.\scripts\ue\check\phase5_harness_invariants.ps1
```

## Deferred work

- **Layer A new tests:** the existing `InventoryDragE2ESyntheticInputTests.cpp`
  already covers drag flows; Phase 5 Layer A wants additional coverage for
  pickup, equip-grant, and container swap. These are new `.cpp` files and
  need a design pass on which flows add coverage vs overlap existing tests.
  When they land, they should carry a dedicated `[Phase5]` tag and
  `verify.ps1`'s default `-TagExpression` should switch from `"Inventory"`
  to `"Phase5"`.
- **Layer B full dump analyzer:** `dump_report.py` is a scaffolding stub
  until Layer A drives checkpoint dumps (`Automation DumpUI ...`).

## Cross-refs

- [canonical.md](../../../../docs/agents/canonical.md) - Dev Loop Contract + testing strategy.
- [overnight_mode.md](../../../../docs/agents/overnight_mode.md) - autonomous rulebook.
- `scripts/ue/test/unit/iterate.ps1` - Layer A dispatch entrypoint.
- `tools/agentic/ui/layout_report.py` - reused by Layer B.
