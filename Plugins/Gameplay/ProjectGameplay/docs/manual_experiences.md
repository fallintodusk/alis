# Experience Manual

Guidelines for adjusting player experiences (game modes, onboarding flows, live ops toggles) while keeping the systems data-driven.

## Quick Start (?5 minutes)

1. Identify the experience manifest (either a data asset such as `Content/Project/Experiences/DA_DefaultExperience.uasset` or a dynamic entry in `Config/DefaultProjectExperience.ini`).
2. Update the **Experience Definition** data asset (edit in-place) to change default map, UI theme, or feature toggles.
3. Open `Config/DefaultExperience.ini` and ensure the `ActiveExperienceId` matches your new asset.
4. Launch PIE and execute `ExperienceManager.DumpState` in the console to validate active features.
5. Run `utility/test/selective_run_tests.sh --filter Features --filter ProjectSettings` for regression coverage.

## Detailed Guide

### 1. Experience Data Assets

- Use `UProjectExperienceDefinition` assets to describe:
  - `DefaultMap` (SoftObjectPtr to the target level).
  - `DefaultTheme` (matches the UI theme PrimaryAssetId).
  - `StartupFeatures` array (feature plugin names to activate).
  - `PostLoginActions` (automations like tutorial popups).
- Keep assets under `Content/Project/Experiences/` with naming pattern `DA_{Context}_{Variant}`.

### 2. Config Binding

- `DefaultExperience.ini` drives the runtime pick. For multiple environments, leverage `DefaultExperience_{Platform}.ini`.
- Update `Config/PrimaryAssetLabel.ini` if you add new experiences so they are tagged for cooking.
- The server handshake references `UProjectSessionSubsystem`; ensure any new experience ID is whitelisted server side.

### 3. Editor Workflow

- Use *Editor Preferences ? Level Editor ? Play ? Launch Separate Server* to simulate multi-client experiences quickly.
- To override features during testing, use the console command `ExperienceManager.Override <ExperienceId>`.
- Avoid editing game mode Blueprints directly; instead, bind to `UProjectExperienceSubsystem::OnExperienceActivated`.

## Hot Reload Settings

### 4. Example: Menu Experience Feature Module

The built-in `ProjectMenuExperience` plugin shows how to bind an experience to a feature module without relying on Blueprints:

- C++ `AMenuExperienceGameMode` lives in the plugin and keeps the boot map minimal.
- The plugin module implements `IProjectFeatureModule` and calls `UProjectExperienceSubsystem::LoadExperience(ProjectExperience:Menu)` during feature activation.
- `Plugins/Features/ProjectMenuExperience/Config/DefaultProjectExperience.ini` contributes the experience definition so hot reload works in editor builds.
- Project-level `Config/DefaultProjectExperience.ini` simply sets `DefaultExperienceName="Menu"`.

When the boot flow loads `ProjectMenuExperience`, the module switches the world into the menu experience: GameMode, pawn, input contexts, and any dependent features all come online together.

- Experience assets respond to live refresh if *Editor Preferences ? Miscellaneous ? Monitor Content Directories* is enabled.
- For INI changes, use the command `ConfigReload` (developer commandlet) or restart the editor; INI hot reload is limited.
- Set `CookOnTheFly` to false in the editor when iterating; the experience subsystem caches Primary Assets.

## Troubleshooting

| Symptom | Cause | Fix |
| --- | --- | --- |
| Experience defaults revert in PIE | PIE using user settings | Check *Editor Preferences ? Play ? Use Single Process*; ensure config overrides aren?t cached. |
| Features missing after swap | Feature plugin not tagged | Verify the plugin is listed in `FeaturesSubsystem` data table. |
| Automation fails `Features.Activation` | Experience asset not saved | Save all modified Data Assets before running tests. |




