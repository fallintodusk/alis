# UI Hot Reload

ProjectUI can reload JSON layout data while an editor widget instance is
alive. `UProjectUserWidget` owns the file watcher and the `ReloadLayout()`
entry point.

## Supported Loop

1. Run the relevant UI in Editor or PIE.
2. Edit the JSON layout owned by that UI component.
3. Let the configured editor watcher reload it, or invoke that widget's
   `ReloadLayout()` route.
4. Verify the realized widget, input policy, and bindings through the owning
   UI test or inspection route.

JSON reload does not reload C++ binaries, Blueprint bytecode, config read at
startup, or experience assets. Use the owning compile, editor restart, or
travel lifecycle for those changes.

| Concern | Owner |
|---|---|
| Widget reload mechanics | `UProjectUserWidget` |
| JSON layout contract | [ProjectUI layout](../../Plugins/UI/ProjectUI/docs/ui_layout.md) |
| Theme behavior | [ProjectUI theme](../../Plugins/UI/ProjectUI/docs/ui_theme.md) |
| Agent-visible UI evidence | [UE inspection](../testing/agent_ue_inspection.md) |
