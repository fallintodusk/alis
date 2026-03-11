Features Tier

Purpose
- Domain capabilities that can be optional or experience-scoped.

Examples
- ProjectMenuMain: front-end menu (logic + screens).
- ProjectMenuGame: in-game/pause menu.
- ProjectSettingsUI: settings screens (uses ProjectSettings service).
- ProjectInventory, ProjectCombat, ProjectDialogue.
- Content-only packs (e.g., ProjectUrbanRuinsPCGRecipe, ProjectForestBiomesPack).

Notes
- Folder stays flat (orchestrator rule). Group by naming stems (ProjectMenu*, ProjectSettingsUI) and metadata.

Useful entry points
- [ProjectInventory](ProjectInventory/README.md)
- [ProjectCombat](ProjectCombat/README.md)
- [ProjectDialogue](ProjectDialogue/README.md)
- [Inventory Design Vision](ProjectInventory/docs/design_vision.md)
