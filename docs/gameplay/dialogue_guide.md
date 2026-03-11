# Dialogue Manual

Bridging instructions while ProjectDialogue replaces the legacy DialoguePlugin.

## Quick Start (?5 minutes)

1. Check out the latest branch with `ProjectDialogue` scaffolding (`Plugins/Features/ProjectDialogue`).
2. In the editor, open `DLG_Template` map or the dialogue test level (listed in `docs/architecture/plugin_architecture.md`).
3. Create dialogue scripts in `Content/Project/Dialogue/` using the **Dialogue Data Table** templates.
4. Wire NPC actors to `UProjectDialogueComponent` and assign the new data assets.
5. Run `utility/test/selective_run_tests.sh --filter ProjectDialogue` (once tests exist) or `--filter ProjectUIFramework` to ensure UI prompts still work.

## Detailed Guide

### 1. Asset Authoring

- Prefer **Data Assets** (`UProjectDialogueTree`) instead of Blueprint nodes. This keeps binary churn low and enables automation tests.
- Use tags for branching: `Tag = Quest/Stage`, `Tag = Reputation/Threshold`. Document new tags in `docs/architecture/plugin_architecture.md`.
- Populate localized text via the existing `Localization/Dialogue` PO files; avoid hardcoding FText in Blueprint.

### 2. Editor Workflow

- Enable *Editor Preferences ? Scripting ? Enable Editor Utility Blueprints* for batch asset edits.
- When migrating from DialoguePlugin assets, use the migration script (`python Scripts/dialogue_migrate.py`, see README) to convert to the new data format.
- For cinematic triggers, bind to `UProjectDialogueComponent::OnDialogueNodeChanged`.

### 3. Runtime Validation

- PIE Shortcut: Run `Automation RunTests ProjectDialogue.Smoke` to ensure prompts show.
- Use `stat ProjectDialogue` to monitor active conversations and ensure cleanup is occurring.

## Hot Reload Settings

- Dialogue JSON/data tables do not currently hot reload automatically; press `Ctrl+Alt+Shift+F` (Reimport) after editing source data.
- Set *Project Settings ? Editor ? BP/Macro Library* to automatically open the last edited dialogue blueprint for faster iteration.
- For developers editing config, ensure `Config/DialogueSettings.ini` remains in sync with server-driven localization.

## Troubleshooting

| Symptom | Cause | Fix |
| --- | --- | --- |
| Dialogue stops midway | Missing connection in data asset | Run the validation ED utility (Window ? Project Dialogue ? Validate Trees). |
| Old DialoguePlugin nodes still spawn | Level blueprint reference | Replace with `UProjectDialogueSubsystem::StartDialogue`. |
| Automation test fails | asset path mismatch | Check `ProjectDialogue/DefaultDialogueSet.uasset` is referenced in `DefaultGame.ini`. |

