# ProjectOnlinePlay TODO

## Mode Lifecycle Implementation (URL-Based Selection + Networking)

### 1. Define FModeConfig struct (same as ProjectSinglePlay, add replication settings)
- [ ] Create FModeConfig struct with:
  - [ ] TSoftClassPtr<APawn> DefaultPawnClass
  - [ ] TSoftClassPtr<APlayerController> PlayerControllerClass
  - [ ] TArray<FSoftObjectPath> RequiredFeaturePlugins
  - [ ] TArray<FSoftClassPath> FeatureComponents
  - [ ] TMap<FName, FString> FeatureConfigs (JSON/TOML per feature)
  - [ ] Replication settings (NetUpdateFrequency, bReplicates defaults)
- [ ] Store in data asset or config file

### 2. Create AOnlineMultiplayerGameMode class
- [ ] Inherit from AGameMode or AProjectGameplayBase
- [ ] Add FModeConfig member variable
- [ ] Implement InitGame override (SERVER-SIDE)
- [ ] Implement PostLogin override (SERVER-SIDE, per-player)
- [ ] Implement Logout override (cleanup)
- [ ] Implement BeginPlay override (safety net)

### 3. Implement InitGame: Parse URL, load ModeConfig, apply classes, EnsurePluginsLoaded, configure replication
- [ ] Parse URL options using UGameplayStatics::ParseOption(Options, TEXT("Mode"))
- [ ] Load FModeConfig based on Mode parameter
- [ ] Apply UE layer classes:
  - [ ] DefaultPawnClass = Config.DefaultPawnClass.LoadSynchronous()
  - [ ] PlayerControllerClass = Config.PlayerControllerClass.LoadSynchronous()
- [ ] Configure replication settings from config
- [ ] Resolve IOrchestratorRegistry from FProjectServiceLocator
- [ ] Call EnsurePluginsLoaded(Config.RequiredFeaturePlugins)
  - [ ] ProjectCombat
  - [ ] ProjectDialogue
  - [ ] ProjectInventory
- [ ] **SERVER-SIDE ONLY** - verify with check(GetNetMode() != NM_Client)

### 4. Implement PostLogin: Attach components (server-side), ensure replication setup
- [ ] **SERVER-SIDE** - verify with HasAuthority()
- [ ] Get spawned pawn from PlayerController
- [ ] Iterate Config.FeatureComponents:
  - [ ] Load component class: TryLoadClass<UActorComponent>()
  - [ ] Create component: NewObject<UActorComponent>(Pawn, LoadedClass)
  - [ ] Verify component has bReplicates = true
  - [ ] Register component: Component->RegisterComponent()
  - [ ] Attach to pawn: Pawn->AddInstanceComponent(Component)
  - [ ] Call Setup() if component implements IFeatureSetup interface
- [ ] Components auto-replicate to clients (via bReplicates=true)

### 5. Implement Logout: Clean up player components
- [ ] Remove feature components from disconnecting player's pawn
- [ ] Clean up server-side state
- [ ] Notify features of player disconnect (if needed)

### 6. Implement BeginPlay: Verify components (idempotent)
- [ ] Check all required features loaded
- [ ] Verify components attached to player pawns
- [ ] Log warnings if components missing
- [ ] Idempotent check (safe to call multiple times)

### 7. Ensure feature components support replication
- [ ] Verify Combat/Dialogue/Inventory components:
  - [ ] UCLASS specifier includes Replicated
  - [ ] Component has bReplicates = true
  - [ ] GetLifetimeReplicatedProps implemented
  - [ ] Relevant properties marked UPROPERTY(Replicated)
  - [ ] Authority checks in logic: HasAuthority()
  - [ ] Client RPCs for player input -> server validation

### 8. Integrate with Online Subsystem for matchmaking
- [ ] Hook into Online Subsystem session creation
- [ ] Handle session join/leave events
- [ ] Coordinate with matchmaking flow
- [ ] Handle travel from lobby to game world

### 9. Standard UE pawn spawning (NO custom spawner services)
- [ ] Use DefaultPawnClass pattern
- [ ] Let GameMode handle SpawnDefaultPawnFor
- [ ] Server spawns pawns, clients receive replicated copies
- [ ] No IGameplayService for spawning

### 10. Testing
- [ ] Test URL-based travel: ServerTravel("City17?game=/Script/ProjectOnlinePlay.OnlineMultiplayerGameMode&Mode=Online")
- [ ] Test InitGame runs SERVER-SIDE only
- [ ] Test PostLogin runs per-player as they join
- [ ] Test feature components attach on server
- [ ] Test components replicate to clients
- [ ] Test client sees replicated components
- [ ] Test authority checks work correctly
- [ ] Test multiplayer with 2+ players
- [ ] Test player join/leave (Logout cleanup)
- [ ] Test world stays agnostic (no .uplugin dependency)

## Networking Notes
- **InitGame**: SERVER-SIDE ONLY (ensure plugins, apply config)
- **PostLogin**: SERVER-SIDE, per-player (attach components on server)
- **Components replicate**: Setup replication in component registration
- **Authority checks**: HasAuthority() in component logic
- **Client RPCs**: Player input -> server validation -> broadcast to clients
- **Replication**: bReplicates=true, GetLifetimeReplicatedProps, UPROPERTY(Replicated)

## Architecture Notes
- Same lifecycle as ProjectSinglePlay, with networking layer
- URL-based selection (Option B - RECOMMENDED)
- World has NO compile-time dependency on this plugin
- Same world can run Single/Online by changing travel URL
- Uses soft refs (NO hard dependencies on features)
