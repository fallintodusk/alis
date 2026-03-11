ProjectOnlinePlay

Purpose
- Consolidator for online multiplayer gameplay (consolidates UE layer + game layer + networking).

Activation Strategy
- **OnDemand** - Lazy-loaded when online multiplayer gameplay starts
- Required by gameplay worlds when playing online

Responsibilities
- **Thin orchestrator**: Consolidates THREE aspects (policy + config, no mechanics)
  - **UE Layer**: GameMode, PlayerController, Pawn (standard UE classes via ModeConfig)
  - **Game Layer**: Combat, Dialogue, Inventory (features via soft refs to Orchestrator)
  - **Networking Layer**: Replication, online subsystem integration
- Provides OnlineMultiplayerGameMode class (AGameMode-based, networked)
- Orchestrates game layer features + networking via soft references
  - Consolidates: ProjectCombat, ProjectDialogue, ProjectInventory (same as ProjectSinglePlay)
  - Uses soft refs (TSoftObjectPtr, FSoftClassPath) - NO hard compile-time dependencies
  - Features attach components to spawned pawn at runtime (server-authoritative)
- Manages client/server replication and authority
- Handles player join/leave events (PostLogin, Logout)
- Manages multiplayer game state and match flow
- Integrates with online subsystem for matchmaking

Non-responsibilities
- No single-player logic (that belongs in ProjectSinglePlay)
- No specific gameplay mechanics implementation (those are separate features)
- No world composition (worlds depend on this GameMode)
- No UI for matchmaking (that belongs in ProjectMenuMain)

Dependencies
- ProjectCore (for IOrchestratorRegistry via FProjectServiceLocator)
- ProjectGameplay (provides base GameMode classes - optional, can inherit AGameMode directly)
- **NO hard dependencies** on ProjectCombat/ProjectDialogue/ProjectInventory (uses soft refs)

Mode Lifecycle (URL-Based Selection + Networking)

**Same lifecycle as ProjectSinglePlay, with networking considerations:**
- InitGame runs on SERVER only (ensure plugins, apply config)
- PostLogin runs per-player as they join (attach components on server)
- Components replicate to clients (setup replication in component registration)

**Travel Example**:
```cpp
// Before travel: Orchestrator ensures plugin loaded
IOrchestratorRegistry* Orchestrator = FProjectServiceLocator::Get<IOrchestratorRegistry>();
Orchestrator->EnsurePluginLoaded(FSoftObjectPath("/ProjectOnlinePlay/"));

// Travel with GameMode in URL
GetWorld()->ServerTravel("City17?game=/Script/ProjectOnlinePlay.OnlineMultiplayerGameMode&Mode=Online");
```

**GameMode Lifecycle** (see ProjectSinglePlay for detailed pseudo-code):

1. **InitGame** (server-side):
   - Parse URL options (Mode=Online)
   - Load ModeConfig (UE layer + features)
   - Apply DefaultPawnClass, PlayerControllerClass
   - EnsurePluginsLoaded for features
   - Configure replication settings (bReplicates, NetUpdateFrequency)

2. **PostLogin** (server-side, per-player):
   - Attach feature components to spawned pawn (server authority)
   - Call Setup(Config) on components
   - Components auto-replicate to client (via bReplicates=true)

3. **BeginPlay** (safety net):
   - Verify components exist
   - Idempotent check

**Networking Considerations**:
- Feature components must be replicated (UCLASS specifier: Replicated)
- Component properties marked UPROPERTY(Replicated) with GetLifetimeReplicatedProps
- Authority checks in component logic (HasAuthority())
- Client RPCs for player input -> server validation

See ProjectSinglePlay for similar single-player GameMode pattern.
See Plugins/Boot/Orchestrator/README.md for OnDemand activation patterns.
See TODO.md for implementation tasks.
