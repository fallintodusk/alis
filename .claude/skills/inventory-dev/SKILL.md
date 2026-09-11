---
name: inventory-dev
description: "Implements ALIS inventory behavior and UI: grid placement, pockets/backpack equip grants, weight/volume, depth stacking, container descriptors, drag-drop, UMG MVVM, and inventory automation tests. Use for tasks mentioning inventory, containers, pockets, backpack, grid, drag/drop, equip grants, descriptors, ViewModels, or inventory test failures."
metadata:
  author: alis-team
  version: "1.0"
---

# Inventory Developer

## Mission

Implement inventory changes fast, with low regression risk, while enforcing the design vision as behavior source of truth.

## Core Vision

The inventory should feel realistic, but not tedious.
- Keep meaningful constraints:
  - footprint, rotation, weight/volume, and equipment-granted containers
- Avoid routine that kills gameplay pace:
  - no forced repetitive micromanagement for common actions
  - no UI flows that add clicks without meaningful decisions
- Favor decisions over chores:
  - planning loadout and space should matter
  - routine handling should stay lightweight and predictable

## Core Rule

Before any inventory logic or UI behavior change, check:
- `Plugins/Features/ProjectInventory/docs/design_vision.md`
- `todo/current/implement_inventory_vision.md`

Use memory-style refs for context hygiene:
- cite relative paths first
- open only the docs/files required for the current task

Optional runner policy:
- if your loader supports experimental `allowed-tools`, restrict to read/search/git/script execution needed for this skill
- example: `Read`, `Bash(git:*)`, `Bash(rg:*)`, `Bash(powershell:*)`
- if not supported, omit it and keep default tool policy

---

## Fast Start Protocol

1. Classify task quickly (guideline, not a hard limit):
- domain logic
- inventory UI layout/interaction
- data schema/config
- bug/crash/regression

2. Load minimal context:
- always: vision + tracker
- UI tasks: add `Plugins/UI/ProjectUI/docs/framework_consolidation.md`
- data tasks: add `Content/Data/Schema/README.md`

3. Define acceptance before editing:
- behavior invariant(s)
- test(s) to prove invariant(s)

4. If unsure while debugging:
- get the exact failing test command output or UE log snippet first
- avoid speculative fixes before evidence

5. **UI visual bugs - DUMP FIRST, code second:**
- Run naked dump: `run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.UI.Layout.InventoryNaked.DumpTree" -Game -Map "//MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent"`
- Run equipped dump: `run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.UI.Layout.InventoryHands.DumpTree" -Game -Map "//MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent"`
- Read the dump JSON (`Saved/Dumps/InventoryNaked.json` or `Saved/Dumps/Inventory.json`)
- Check: widget sizes (width/height vs desiredSize), visibility, slot info
- Identify the EXACT widget causing the visual issue before writing any fix
- Common traps: wrong test state (naked vs equipped), guessing layout from code instead of measuring

---

## Design Invariants (non-negotiable)

### Grid + Placement
- Uniform cell semantics across containers.
- Rectangular footprint, 90 degree rotation.
- Multi-cell occupancy is real (not slot-list emulation).

### Container Grants
- Naked baseline: hands only.
- Pockets/backpack/storage only from equipped grants.
- No always-on storage containers unless explicitly requested.
- Unequip blocked when granted container is non-empty.
- No backpack nesting.

### Depth Stacking
- `gridW*gridH > 1` -> classic 2D occupancy.
- `gridW=1 && gridH=1` -> depth-constrained stack height.
- MVP remains one stack per cell.

### Hands
- Always visible, separate from descriptor-driven storage.
- 2x2 hand grids with width-constrained handling.

### UI Layout Contract
- Descriptor-driven rendering only.
- Descriptor fields: `ContainerId`, `Label`, `GridWidth`, `GridHeight`, `LayoutGroup`, `OrderKey`.
- `TopCompact` near hands, `BottomLarge` below/right, backpack first in large group.
- Empty descriptor group must collapse its host.
- No per-clothing hardcoded branches in panel code.

---

## Ownership Boundaries

### ProjectInventory (domain)
- item/container semantics
- placement/weight/depth rules
- command execution rules

### ProjectInventoryUI (feature UI)
- inventory-specific orchestration
- binds ViewModel output to widgets

### ProjectUI (shared framework)
- reusable mechanics only:
  - grid hit math
  - drag/drop generic controller
  - visual state mapping
  - popup/tooltip presenters
  - widget binders
- Use ProjectUI helpers for fonts/colors/styling - never hardcode paths. Key API: `UProjectWidgetLayoutLoader::ResolveThemeFont(Name, Theme)`, `ResolveThemeColor()`.

If logic can be reused by 2+ UI modules, move it to `ProjectUI`.

---

## Implementation Routes

### Route A: Domain Logic Change
Read:
- `Plugins/Features/ProjectInventory/docs/design_vision.md`
- `Plugins/Features/ProjectInventory/docs/architecture.md`

Touch:
- `Plugins/Features/ProjectInventory/Source/ProjectInventory/`
- `ProjectInventory/Public/InventoryTypes.h` when schema/types change

Validate:
- targeted inventory tests
- then integration tests if UI-visible behavior changed

### Route B: Inventory UI Change
Read:
- `Plugins/UI/ProjectUI/README.md`
- `Plugins/UI/ProjectUI/docs/framework_consolidation.md`
- `Plugins/UI/ProjectUI/docs/ui_mvvm.md`

Touch:
- `Plugins/UI/ProjectInventoryUI/Source/ProjectInventoryUI/`
- `Plugins/UI/ProjectInventoryUI/Data/InventoryPanel.json` when layout changes

Validate:
- `ProjectIntegrationTests.UI.Framework.*` (for framework seams)
- `ProjectIntegrationTests.UI.Layout.InventoryHands.*` (for layout contract)

### Route C: Data Schema Change
Read:
- `Content/Data/Schema/README.md`

Touch:
- `Content/Data/Schema/Inventory/`

Validate:
- project validation scripts
- inventory integration tests that load schema

---

## Test Tiers

### Tier 1 (fast, required)
```powershell
scripts/ue/build/build.bat AlisEditor Win64 Development
powershell -ExecutionPolicy Bypass -File scripts/ue/test/unit/run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.UI.Framework.GridDragDrop.FootprintValidation"
```

### Tier 2 (when inventory UI changed)
```powershell
powershell -ExecutionPolicy Bypass -File scripts/ue/test/unit/run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.UI.Layout.InventoryHands.PocketOrdering" -Game -Map "/MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent"
powershell -ExecutionPolicy Bypass -File scripts/ue/test/unit/run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.UI.Layout.InventoryNaked.DumpTree" -Game -Map "/MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent"
```

### Tier 3 (pre-merge confidence)
```powershell
scripts/ue/test/smoke/boot_test.bat
```

### Optional diagnostics
```powershell
scripts/ue/test/ui/check_inventory_layout.ps1
tools/agentic/ui/layout_report.py Saved/Dumps/Inventory.json
```

---

## References

- UE official links are in `references/ue_links.md`.

---

## Output Contract Per Task

After implementation, always provide:
1. behavior delta (what changed, why)
2. files touched
3. tests run (exact command + pass/fail)
4. residual risks or follow-ups

---

## Known Pitfalls

- Domain: `Plugins/Features/ProjectInventory/docs/pitfalls.md`
- UI: `Plugins/UI/ProjectInventoryUI/docs/common_pitfalls.md`

---

## Anti-Patterns (reject immediately)

- Hardcoded per-clothing UI branches in inventory panel.
- Always-on storage visuals in naked state.
- Duplicate generic UI helpers in feature plugin when ProjectUI already owns the concern.
- UI widget directly reading game entities instead of ViewModel/domain bridge.
- Skipping vision/tracker check before behavior edits.
- Unicode in docs/comments (ASCII only).
- Using `TryAddItem` return value as an InstanceId.
- Assuming empty world containers stay empty after spawn (loot profiles generate content).
