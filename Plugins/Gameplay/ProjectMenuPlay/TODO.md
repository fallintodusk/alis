# ProjectMenuPlay TODO

## Mode Lifecycle Implementation

### 1. Define FMenuModeConfig struct (menu UI plugins only)
- [ ] Create FMenuModeConfig struct with RequiredMenuPlugins array
- [ ] Support soft refs: TArray<FSoftObjectPath> for plugin paths
- [ ] Store in data asset or config file

### 2. Create AMenuFrontEndGameMode class
- [ ] Inherit from AGameMode or AGameModeBase
- [ ] Implement InitGame override
- [ ] Implement BeginPlay override (safety net)

### 3. Implement InitGame: Load ModeConfig, set spectator policy, EnsurePluginsLoaded
- [ ] Parse URL options (if needed)
- [ ] Load FMenuModeConfig from data asset
- [ ] Set spectator policy:
  - [ ] DefaultPawnClass = nullptr
  - [ ] bStartPlayersAsSpectators = true
- [ ] Resolve IOrchestratorRegistry from ServiceLocator
- [ ] Call EnsurePluginsLoaded(Config.RequiredMenuPlugins)
  - [ ] ProjectMenuMain
  - [ ] ProjectSettingsUI

### 4. Implement BeginPlay: Verify menu UI plugins loaded (idempotent)
- [ ] Check UGameFeaturesSubsystem for plugin active state
- [ ] Log warnings if plugins missing
- [ ] Idempotent check (safe to call multiple times)

### 5. Create data asset for default menu mode config
- [ ] Create UDataAsset subclass for FMenuModeConfig
- [ ] Create default config asset in Content/
- [ ] Set default plugin list: ProjectMenuMain, ProjectSettingsUI

### 6. Standard UE spectator mode configuration
- [ ] Verify DefaultPawnClass = nullptr
- [ ] Verify bStartPlayersAsSpectators = true
- [ ] Test: No pawn spawning in menu world
- [ ] Test: Players enter as spectators

## Notes
- No PostLogin component attachment (no pawns in menu)
- Simpler than ProjectSinglePlay/ProjectOnlinePlay (no pawn, no features)
- Loaded at Boot (not OnDemand)
- World Settings binding (not URL-based)
