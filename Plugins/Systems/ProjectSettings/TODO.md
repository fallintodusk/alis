# ProjectSettings TODO

## DIP Pattern Implementation

### 1. Define ISettingsService interface in ProjectCore
- [ ] Create interface in ProjectCore/Public/Interfaces/ISettingsService.h
- [ ] Define methods:
  - [ ] Get/set settings (GetSetting, SetSetting, etc.)
  - [ ] Validation APIs (ValidateSetting, GetSettingConstraints, etc.)
  - [ ] Persistence APIs (SaveSettings, LoadSettings, ResetToDefaults, etc.)
  - [ ] Setting categories (Graphics, Audio, Gameplay, Controls, etc.)
  - [ ] Change notifications (OnSettingChanged event/delegate)

### 2. Create UProjectSettingsSubsystem
- [ ] Implement as GameInstance subsystem (persistent across levels)
- [ ] Implement ISettingsService interface
- [ ] Manage game settings:
  - [ ] Graphics settings (resolution, quality, vsync, etc.)
  - [ ] Audio settings (master volume, music, sfx, etc.)
  - [ ] Gameplay preferences (difficulty, accessibility, etc.)
  - [ ] Keybindings/input mappings
- [ ] Settings storage:
  - [ ] Default values
  - [ ] User overrides
  - [ ] Platform-specific defaults

### 3. Service registration
- [ ] Register with FProjectServiceLocator in Initialize()
  - [ ] FProjectServiceLocator::Register<ISettingsService>(this)
- [ ] Unregister in Deinitialize()
  - [ ] FProjectServiceLocator::Unregister<ISettingsService>()
- [ ] Ensure registration happens before UI resolves

### 4. Persistence implementation
- [ ] Save settings to disk (SaveGame or config files)
- [ ] Load settings on startup
- [ ] Handle migration from old versions
- [ ] Platform-specific persistence (Steam Cloud, etc.)

### 5. Validation
- [ ] Define setting constraints (min/max, enum values, etc.)
- [ ] Validate settings before applying
- [ ] Revert invalid settings to defaults
- [ ] Log validation errors

### 6. UI integration
- [ ] ProjectSettingsUI resolves ISettingsService
- [ ] UI calls Get/Set methods
- [ ] UI displays validation errors
- [ ] UI shows current vs default values
- [ ] NO compile-time dependency on ProjectSettings

### 7. Testing
- [ ] Test service registration at startup
- [ ] Test UI can resolve ISettingsService
- [ ] Test get/set operations work
- [ ] Test validation works
- [ ] Test persistence works (save/load/reset)
- [ ] Test DIP pattern (UI doesn't compile-depend on ProjectSettings)

## Notes
- Follows DIP pattern (Systems register, Features resolve)
- ProjectSettings REGISTERS ISettingsService
- ProjectSettingsUI RESOLVES ISettingsService (NO compile-time dependency)
- See ProjectLoadingSubsystem for reference implementation pattern
- No UI in this plugin (ProjectSettingsUI provides screens)
