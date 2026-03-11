ProjectMenuGame

Purpose
- In-game / pause menu as a feature plugin.

Responsibilities
- Pause overlay, in-game options, resume/quit
- Return to main menu via ILoadingService (goes through loading pipeline)
- Quick save/load via ISaveService (resolved from ServiceLocator)
- Settings integration via ProjectSettingsUI

Non-responsibilities
- No framework code; uses ProjectUI
- No direct System dependencies; resolves services via DIP pattern
- No direct OpenLevel/ServerTravel - use ILoadingService for consistency

Framework consolidation rules (critical)
- Keep pause menu code domain-focused (resume/quit/settings intents only).
- Reuse ProjectUI presenters for popup/settings hosting and avoid custom overlay lifecycle logic.
- Share the same settings-root reuse pattern as main menu (single instance, visibility switching).
- See `Plugins/UI/ProjectUI/docs/framework_consolidation.md`.

See ProjectMenuMain for reference implementation pattern with ILoadingService.
See TODO.md for implementation tasks.
