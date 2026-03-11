# ProjectInventoryUI

Inventory UI plugin built on ProjectUI JSON layout + MVVM.

Purpose
- Display inventory stats (weight, volume, item count).
- Render primary + secondary container grids side-by-side.
- Show selection info and a quantity picker for stack actions.
- Show per-item weight and volume for the selected stack.
- Quantity picker uses +/- buttons (no text input yet).
- Provide a basic command bar (Use, Drop, Equip).
- Provide basic drag and drop to move stacks within/between containers.
- Display equip silhouette slots and allow drag-to-slot equip.
- Silhouette is a placeholder panel (no character art asset yet).
- Provide a rotate toggle for drag/drop (R key or Rotate button).
- Show overflow status when unequip is blocked by granted containers.
- Provide tooltip details (icon, description, size, stack, use effects with units and benefit/harm color coding).
- Highlight hovered cells and show drag preview outlines.
- Inventory panel is an overlay layer, not a HUD slot widget.

What it does
- InventoryViewModel reads IInventoryReadOnly (ProjectCore) and exposes primary/secondary container tabs and grid labels.
- InventoryViewModel forwards command intents to IInventoryCommands (ProjectCore).
- W_InventoryPanel builds primary + secondary UniformGridPanel widgets at runtime.
- W_InventoryPanel allows cell selection by click, shows selection info, and runs command bar actions on the selected item.
- W_InventoryPanel supports drag and drop to call RequestMoveItem.
- W_InventoryPanel supports drag and drop onto equip slots to call RequestEquipItem.
- W_InventoryPanel shows hover and drag preview feedback.
- Layout is data-driven (Data/InventoryPanel.json).
- Shown/hidden via ProjectUI layer host (ShowDefinition/HideDefinition).

Layout contract (critical)
- Use one universal container descriptor model from ViewModel data:
  - id, label, width, height, layout group, order
- Hands are fixed and always visible.
- Compact granted containers (pockets and similar) render in top row to the right of hands.
- Large granted containers render in bottom storage row; backpack must be first when present.
- If a descriptor group is empty, collapse its host (no always-on empty storage placeholders).
- Do not add per-container hardcoded widget branches in panel code for pants/jacket/backpack.
- Adding a new equip-granted container should require only descriptor data, not new panel layout logic.
- Canonical flow:
  - naked = hands only
  - pants/jacket grants = compact row grows to the right of hands
  - backpack grant = large grid appears below hands/compact row

What it does NOT do
- Container overflow resolution (auto-move/spill prompts).
- It does not live in ProjectHUD slots (use a small HUD widget if needed).
- Full item card (durability, attachments, comparison).

Files
- Data/ui_definitions.json
- Data/InventoryPanel.json
- Source/ProjectInventoryUI/Public/MVVM/InventoryViewModel.h
- Source/ProjectInventoryUI/Public/Widgets/W_InventoryPanel.h

Dependencies
- ProjectUI (layout, MVVM)
- ProjectCore (IInventoryReadOnly, IInventoryCommands)

Framework consolidation rules (critical)
- Inventory keeps item semantics and command routing only.
- Generic mechanics (grid hit math, drag/drop footprint checks, visual state coloring, popup/tooltip lifecycle, widget binding) belong to ProjectUI.
- New reusable helpers must be added in `ProjectUI` first, then consumed here.
- See `Plugins/UI/ProjectUI/docs/framework_consolidation.md`.

Next steps (planned)
- Add richer tooltips (durability, stats, attachments).

Related Docs
- UI architecture: docs/architecture.md
- ProjectUI MVVM: Plugins/UI/ProjectUI/docs/ui_mvvm.md
- Icon fonts & codepoints: Plugins/UI/ProjectUI/Content/Slate/Fonts/README.txt
- ProjectInventory: Plugins/Features/ProjectInventory/README.md
- Inventory design vision (behavior SOT): Plugins/Features/ProjectInventory/docs/design_vision.md
