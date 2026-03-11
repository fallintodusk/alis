# UI

Public UI router for ALIS.

The UI stack in this repository is built around ProjectUI, JSON layout definitions, MVVM-style data flow, and descriptor-driven rendering.

## Start Here

- UI framework plugin: [../../Plugins/UI/ProjectUI/README.md](../../Plugins/UI/ProjectUI/README.md)
- JSON layout system: [../../Plugins/UI/ProjectUI/docs/ui_layout.md](../../Plugins/UI/ProjectUI/docs/ui_layout.md)
- MVVM guide: [../../Plugins/UI/ProjectUI/docs/ui_mvvm.md](../../Plugins/UI/ProjectUI/docs/ui_mvvm.md)
- Theme system: [../../Plugins/UI/ProjectUI/docs/ui_theme.md](../../Plugins/UI/ProjectUI/docs/ui_theme.md)
- Hot reload workflow: [hot_reload.md](hot_reload.md)

## Feature UI Entry Points

- Inventory UI: [../../Plugins/UI/ProjectInventoryUI/README.md](../../Plugins/UI/ProjectInventoryUI/README.md)
- Dialogue UI: [../../Plugins/UI/ProjectDialogueUI/README.md](../../Plugins/UI/ProjectDialogueUI/README.md)
- Main menu UI: [../../Plugins/UI/ProjectMenuMain/README.md](../../Plugins/UI/ProjectMenuMain/README.md)
- HUD framework: [../../Plugins/UI/ProjectHUD/README.md](../../Plugins/UI/ProjectHUD/README.md)

## Public UI Themes

- JSON-first layout definition
- hot reload for layout iteration
- descriptor-driven rendering instead of container-specific branches
- ProjectUI as the reusable framework layer
- feature plugins owning domain semantics, not framework internals
