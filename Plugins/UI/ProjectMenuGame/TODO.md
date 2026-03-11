# ProjectMenuGame TODO

## DIP Pattern Implementation

### 1. Resolve ILoadingService for "Return to Main Menu"
- [ ] Resolve ILoadingService from FProjectServiceLocator
- [ ] Call ILoadingService::LoadLevel("MainMenuWorld") or similar
- [ ] Use loading pipeline (6-phase) for consistency
- [ ] NO direct OpenLevel/ServerTravel calls
- [ ] Handle loading state UI (loading screen, progress)

### 2. Resolve ISaveService for quick save/load
- [ ] Resolve ISaveService from FProjectServiceLocator
- [ ] Implement quick save button:
  - [ ] Call ISaveService::QuickSave() or SaveToSlot(QuickSaveSlot)
  - [ ] Show save confirmation UI
  - [ ] Handle save errors
- [ ] Implement quick load button:
  - [ ] Call ISaveService::QuickLoad() or LoadFromSlot(QuickSaveSlot)
  - [ ] Show load confirmation UI
  - [ ] Handle load errors (no save exists, corrupted, etc.)

### 3. Pause menu UI
- [ ] Create pause overlay widget (UMG)
- [ ] Buttons:
  - [ ] Resume (unpause)
  - [ ] Settings (open ProjectSettingsUI)
  - [ ] Quick Save
  - [ ] Quick Load
  - [ ] Return to Main Menu
  - [ ] Quit Game
- [ ] Use ProjectUI framework (MVVM, CommonUI, theming)
- [ ] Handle input (pause key, gamepad, etc.)

### 4. Settings integration
- [ ] Embed ProjectSettingsUI screens in pause menu
- [ ] Navigate to settings from pause menu
- [ ] Return to pause menu from settings
- [ ] ProjectSettingsUI handles ISettingsService interaction

### 5. Input handling
- [ ] Register pause input (Escape, Start button, etc.)
- [ ] Toggle pause state
- [ ] Block game input while paused
- [ ] Handle input mode switching (UI focus)

### 6. Testing
- [ ] Test pause menu opens/closes
- [ ] Test Resume works
- [ ] Test Settings navigation works
- [ ] Test Quick Save works
- [ ] Test Quick Load works
- [ ] Test Return to Main Menu uses ILoadingService
- [ ] Test Quit Game works
- [ ] Test DIP pattern (NO compile-time dependencies on Systems)

## Notes
- Follows DIP pattern (UI resolves services, no compile-dependencies)
- Resolves ILoadingService for "Return to Main Menu"
- Resolves ISaveService for quick save/load
- Uses ProjectUI framework (MVVM, CommonUI, theming)
- Embeds ProjectSettingsUI for settings screens
- See ProjectMenuMain for reference implementation with ILoadingService
