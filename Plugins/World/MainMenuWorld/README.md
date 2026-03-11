MainMenuWorld

Purpose
- Frontend/main menu world composition (maps, lighting, data layers).

Activation Strategy
- **Boot** - Loaded at startup (required for frontend)
- Modules loaded at boot by Orchestrator (code only)
- Content mounted at runtime by ProjectLoading via `ILoadingService`

Responsibilities
- Menu world map(s)
- Lighting and atmosphere setup
- Data layer composition for menu environment
- Declares ProjectMenuPlay GameMode as override in World Settings (spectator-only, shows cursor)
- Registers main menu load descriptor (CDO) in module startup (uses ProjectLoading descriptor base/registry)
- Descriptor loads maps/streaming only; UI is spawned by ProjectMenuMain composer after travel

Non-responsibilities
- No gameplay logic (that belongs in ProjectMenuPlay GameMode)
- No UI code (that belongs in ProjectMenuMain/ProjectUI)
- No menu feature behavior (that belongs in ProjectMenuMain)

Dependencies
- ProjectMenuPlay (provides MenuPlayGameMode)
- ProjectLoading (descriptor base/registry)
- ProjectMenuMain shown via composer (menu UI remains in UI plugin; descriptor stays UI-free)

Thin Orchestrator Pattern
- World depends ONLY on GameMode
- GameMode (ProjectMenuPlay) asks ProjectMenuMain composer to show UI
- Descriptor-driven content via ProjectLoading; no runtime Orchestrator calls

World Settings
- Set GameMode Override to MenuPlayGameMode from ProjectMenuPlay
- GameMode handles all feature orchestration

See Plugins/Boot/Orchestrator/README.md for activation lifecycle details.
