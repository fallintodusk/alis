# UI Manual

Instructions for designers to work with the JSON-driven W_ widgets without breaking the C++ architecture.

## Quick Start (~5 minutes)

1. Generate JSON layouts from the latest branch (`Config/UI/*.json` already seeded).
2. Open the editor, load `WBP_MainMenu`, `WBP_MapBrowser`, `WBP_LoadingScreen`, or `WBP_Settings` (empty Blueprint shells inheriting from the C++ classes).
3. If you edit a JSON file, press `~` and run `W_MainMenu.ReloadLayout` (replace with the active widget) to hot-reload the layout instantly.
4. Use **Widget Reflector** to validate binding names match the JSON `Name` fields.
5. Run `utility/test/selective_run_tests.sh --filter ProjectMenuUI` before checking in.

## Detailed Guide

### 1. Blueprint Shell Setup

- Blueprints should **only** extend the C++ classes (`UW_MainMenu`, etc.) and contain no additional widgets by default.
- Use the `Designer -> Class Settings -> Interfaces` tab to confirm no stray interfaces were added.
- For visual tweaks, add animations or minor overlay widgets in the Blueprint, but keep the root placeholder intact.

### 2. Editing JSON Layouts

- All files live under `Config/UI/`. Respect the naming convention `WidgetName.ElementType.Identifier`.
- Use the `Class` field to match UMG classes (`CanvasPanel`, `Button`, `TextBlock`, etc.).
- Keep callbacks consistent: functions like `NavigateToMapBrowser` are bound automatically when the widget name contains `Button_Play`.

### 3. Previewing & QA

- Enable *Editor Preferences -> General -> Miscellaneous -> Enable Editor Utility Hot Reload*.
- Toggle *Project Settings -> Editor -> Blueprint Compile -> Save on Compile* OFF to avoid blueprint spam when testing.
- Use PIE (`Alt+P`) to confirm navigation flow; the ViewModel mock fills in placeholder data.

## Hot Reload Settings

- Directory Watcher is active by default through the new watcher class. Confirm in *Editor Preferences -> Loading & Saving* that "Monitor Content Directories" remains enabled.
- If hot reload stalls, run `ProjectMenuUI.ResetWatchers` (command added via `W_*` console helpers) or restart PIE.
- For team members without source access, ensure they run the launcher with `-NoLiveCoding -NoSound -NoLoadingScreen` to bypass heavy editor HUDs.

## Troubleshooting

| Symptom | Cause | Fix |
| --- | --- | --- |
| JSON edits not visible | Directory watcher disabled | Re-enable in Editor prefs or call `W_Widget.ReloadLayout` manually. |
| Clicks do nothing | Widget name mismatch | Check the JSON `Name` field matches the substring in `BindCallbacks` (e.g., `Button_Play`). |
| Automation failure `ProjectMenuUI.Widget` tests | Missing required widget in JSON | Run `utility/test/run_tests.sh --filter ProjectMenuUI` to see which child name is missing. |
