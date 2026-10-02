"""Phase 5 Layer B: UI state dump analyzer for the inventory verification harness.

Wraps the existing `tools/agentic/ui/layout_report.py` for UI widget-tree
parsing, then adds inventory-specific checkpoints.

SOLID contract (enforced by the Phase 5 fitness test, deferred):
  - Layer B is headless. Works with `-NullRHI`; no renderer required.
  - Does NOT call into Layer A (tests) or Layer C (screenshot).
  - Produces a deterministic, diffable report. Same input + same seed ->
    same output.

Reuse status (per the overnight rulebook's reuse check):
  - UI widget-tree parsing: `tools/agentic/ui/layout_report.py` already
    exists and parses Saved/Dumps/Inventory.json. This script composes
    with it; does not reinvent.
  - Dump emission: existing integration test
    `ProjectUIInventoryDumpTreeTest.cpp` already drives the dump inside
    the editor. No new C++ needed for v1.

This v1 is a scaffolding stub. It prints the planned contract + exits 0
so the top-level verify.ps1 can wire all three layers end-to-end. Full
logic lands when Layer A drives actual checkpoints.
"""
from __future__ import annotations

import pathlib
import sys

REPO_ROOT = pathlib.Path(__file__).resolve().parents[3]
DUMP_FILE = REPO_ROOT / "Saved" / "Dumps" / "Inventory.json"
LAYOUT_REPORT = REPO_ROOT / "tools" / "agentic" / "ui" / "layout_report.py"


def main() -> int:
    print("--- dump_report (Phase 5 Layer B) ---")
    print(f" repo:            {REPO_ROOT}")
    print(f" dump source:     {DUMP_FILE}")
    print(f" reused analyzer: {LAYOUT_REPORT}")

    # v1: report what is present without failing. v2 will invoke
    # layout_report for each checkpoint captured by Layer A and diff
    # widget-tree shape + inventory state against a stored baseline.
    if not DUMP_FILE.exists():
        print(f" NOTE: {DUMP_FILE} not present yet; dump not produced in this run.")
        print(" Layer B scaffolding is in place but awaits a live Layer A run")
        print(" that triggers Automation DumpUI at checkpoints.")
        return 0

    if not LAYOUT_REPORT.exists():
        print(f" ERROR: expected analyzer missing: {LAYOUT_REPORT}")
        return 1

    # TODO(phase5-layerb): for each checkpoint, read the corresponding
    # dump slice and delegate to layout_report + inventory-specific checks.
    # Until checkpoints are declared by Layer A, this is a no-op.
    print(" stub: no checkpoints declared yet; exiting 0.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
