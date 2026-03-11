UI Tier

Purpose
- UI framework and base building blocks only.

Scope
- ProjectUI: MVVM, JSON layouts, theming, CommonUI integration (see ProjectUI/docs/commonui_integration.md).
- UI consumer plugins: JSON definitions and layout configs (Config/UI/*.json).

Critical framework contract
- Reusable mechanics must be implemented in `ProjectUI` and reused by feature UI plugins.
- Feature plugins keep domain-specific logic only (inventory/menu/vitals/settings semantics).
- Descriptor-driven views must collapse empty groups instead of leaving placeholder panels.
- See `Plugins/UI/ProjectUI/docs/framework_consolidation.md` for mandatory ownership rules and test gates.

Non-responsibilities
- No front-end flow logic (which screen to show when).
- UI consumer plugins live under Plugins/UI and define ui_definitions.json.
