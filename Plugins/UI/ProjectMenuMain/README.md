ProjectMenuMain

Purpose
- Front-end menu (logic + screens) as a feature plugin.

Responsibilities
- Menu navigation, main screens, integration with ProjectSession/ProjectSave, theme usage.
- Composer subsystem (`UMenuMainComposerSubsystem`) that requests ProjectUI to show `UW_MainMenu` (no WBP shell required).

Non-responsibilities
- No framework code (that is ProjectUI).

Framework consolidation rules (critical)
- Keep ProjectMenuMain focused on menu semantics and flow orchestration.
- Reuse ProjectUI for widget binding, popup/tooltip lifecycle, layer activation, and reusable interaction mechanics.
- Settings screen hosting should reuse a single presenter-owned settings root (show/hide, no recreate on navigation).
- See `Plugins/UI/ProjectUI/docs/framework_consolidation.md`.

Notes
- Internally keep Core/ (domain) and Shell/ (composition) folders to enable a future split.
- UI is spawned after MainMenuWorld loads; GameMode/PC (ProjectMenuPlay) just calls the composer. Content loading remains descriptor-driven (ProjectLoading).
- UI definitions live in `Config/UI/ui_definitions.json`.

Dialog Binding
- Dialog widgets broadcast OnButtonClicked(Action) when any button is clicked
- Bind to the Dialog's delegate in BindCallbacks, not to individual buttons
- Route action strings to handlers via OnDialogButtonClicked switch/map

Game Actions Architecture
- UI widgets do NOT call system services (ILoadingService, etc.) directly
- Flow: Widget -> Composer delegate -> Controller -> ILoadingService
- Widget detects user intent (button click)
- Widget calls Composer->RequestStartGame(MapId, GameMode)
- Controller subscribes to OnRequestStartGame delegate in BeginPlay
- Controller owns: map paths, GameMode selection, loading orchestration
- Controller calls ILoadingService via ServiceLocator
- Why: UI stays presentation-focused. Gameplay layer owns "how to start game".

Dependency Note (Critical)
- ProjectMenuPlay depends on ProjectMenuMain (for UMenuMainComposerSubsystem)
- ProjectMenuMain CANNOT depend on ProjectMenuPlay (would create cycle)
- Solved via callback: Widget -> Composer delegate -> Controller (no include needed)
- Controller subscribes to Composer's OnRequestStartGame in BeginPlay

## Menu Architecture: KISS (Keep It Simple)

### Decision
- **ProjectMenuMain** is a Feature plugin that handles all menu UI and coordination.
- **No separate "experience" layer** - menu directly uses `ILoadingService` to initiate loading.
- **Native GameMode/MatchState** handles match lifecycle.

### Architecture
- **ProjectMenuMain** (Feature plugin):
  - Owns menu UI widgets (title screen, map browser, settings).
  - Coordinates user actions (New Game, Continue, Load Game).
  - Uses `ILoadingService` (ProjectCore interface) to trigger loading.
  - Queries `ProjectSave` for save slots.
  - **Does NOT** depend directly on `ProjectLoading` (DIP).

### Loading Flow
1. User clicks "New Game" -> `ProjectMenuMain` handles button click.
2. `ProjectMenuMain` resolves `ILoadingService` from `FProjectServiceLocator`.
3. `ProjectMenuMain` builds `FLoadRequest` (MapAssetId, LoadMode, etc.).
4. `ProjectMenuMain` calls `LoadingService->StartLoad(Request)`.
5. `ProjectLoading` (implements `ILoadingService`) executes 6-phase pipeline.
6. `ProjectMenuMain` subscribes to `OnCompleted`/`OnFailed` for feedback.

### Optional: Data-Driven Map Catalog
- JSON catalog files: `Content/Catalog/menu.json`.
- Hot-reload in editor, no DataAssets needed.
- Schema: `worlds[]`, `modes[]`, `defaults{}`.
- Menu UI reads catalog to populate map browser.
