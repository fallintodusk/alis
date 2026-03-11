ProjectMenuPlay

Purpose
- Minimal rules/input shell for the menu world (GameMode + PlayerController).

Activation Strategy
- **Boot** - Loaded at startup (needed for menu world)
- Required by MainMenuWorld

Responsibilities
- GameMode: `AMenuPlayGameMode` (spectator-only, no pawn/HUD).
- PlayerController: `AMenuPlayPlayerController` (shows cursor, Game+UI input, asks ProjectMenuMain to show menu).
- Uses ProjectMenuMain composer subsystem to spawn the menu UI in C++ (no UMG shell dependency).
- Menu world declares this GameMode in World Settings.

Non-responsibilities
- No menu UI code (ProjectMenuMain owns it).
- No world composition (MainMenuWorld owns it).
- No content loading (ProjectLoading + descriptors handle it).

Dependencies
- ProjectMenuMain (for composer subsystem + widgets)
- ProjectUI (UI framework)
- Core/Engine (standard)

Lifecycle (World Settings-based)
- MainMenuWorld map sets GameMode override to `AMenuPlayGameMode`.
- Controller auto-sets Game+UI input and calls `UMenuMainComposerSubsystem::ShowMainMenu`.
- UI/content loading is driven by MainMenuWorld descriptor via `ILoadingService`; this plugin does not manage loads.
