# ProjectInventoryUI

Inventory UI plugin built on ProjectUI JSON layout and MVVM.

Behavior SOT
- Inventory behavior and the container layout contract live only in `Plugins/Features/ProjectInventory/docs/design_vision.md`.
- This README covers UI responsibilities and current implementation notes only.
- Do not fork inventory behavior here.

Purpose
- Display inventory data exposed by `IInventoryReadOnly`.
- Forward user commands through `IInventoryCommands`.
- Provide drag/drop, equip targets, command bar, tooltip, and context-menu UI.

Current implementation notes
- `InventoryViewModel` splits containers into dedicated hands, compact storage, and large storage.
- The default runtime `Item.Container.Hands` container is projected into left and right hand grids by the ViewModel.
- Container layout, group ordering, and empty-host collapse behavior are defined
  only in the behavior SOT above.
- Nearby world storage is presented inside the same screen as a distinct
  right-side panel; the UI reuses the same grid, tooltip, drag/drop, and
  capacity primitives rather than creating a second loot framework.

Not responsible for
- Inventory rules, item semantics, or overflow policy.
- ASC or vitals state.
- World pickup or world-storage rules.

Files
- `Data/ui_definitions.json`
- `Data/InventoryPanel.json`
- `Source/ProjectInventoryUI/Public/MVVM/InventoryViewModel.h`
- `Source/ProjectInventoryUI/Public/Widgets/W_InventoryPanel.h`

Dependencies
- ProjectUI
- ProjectCore

Related docs
- Behavior SOT: `Plugins/Features/ProjectInventory/docs/design_vision.md`
- Inventory implementation: `Plugins/Features/ProjectInventory/README.md`
- Inventory architecture: `Plugins/Features/ProjectInventory/docs/architecture.md`
- ProjectUI MVVM: `Plugins/UI/ProjectUI/docs/ui_mvvm.md`
- Runtime gotchas: `docs/pitfalls.md`
- Icon fonts and codepoints: `Plugins/UI/ProjectUI/Content/Slate/Fonts/README.txt`
