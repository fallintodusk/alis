# ProjectSettingsUI

User settings system with SOLID architecture and delta-based application.

## Purpose

Settings screens and viewmodels for graphics, audio, and gameplay controls.

## Responsibilities

- UI for settings controls, binding to typed settings APIs
- Delta computation for efficient change detection
- Persistence via engine APIs and custom INI sections

## Non-responsibilities

- Does not own game logic; UI-only concerns
- Does not own generic UI framework mechanics (those belong to ProjectUI)

## Framework Consolidation Rules (Critical)

- Keep this plugin focused on settings semantics, delta computation, and settings widgets.
- Reuse ProjectUI framework primitives for binding, popup lifecycle, layout loading, and theme resolution.
- Do not add local generic helpers that duplicate ProjectUI capabilities.
- Menu hosts should reuse one settings-root instance through presenter-driven show/hide behavior.
- See `Plugins/UI/ProjectUI/docs/framework_consolidation.md`.

---

## Design Decisions

### Single Source of Truth

All settings defined via `PROJECT_SETTINGS_FIELDS` X-macro in `ProjectUserSettings.h`.

The macro defines for each setting:
- Type
- Member name
- Config key (INI)
- Category
- Default value

Adding a new setting: update macro + add UPROPERTY field (UHT limitation).

### Delta-Based Application

Settings are not applied individually. Instead:

1. Widget reads current UI state into `FProjectUserSettings`
2. `FProjectSettingsDelta::Compute(saved, pending)` determines what changed
3. Service applies only changed values via `Apply(settings, delta)`
4. No CVar spam, no redundant engine calls

### Category Flags

Delta tracks both individual changes (`TSet<FName>`) and category flags.

Category flags enable O(1) checks like `HasScalability()` before touching engine systems.

### Persistence Strategy

| Category | API | INI Section | Owner |
|----------|-----|-------------|-------|
| Display | UGameUserSettings | Engine-managed | UE |
| Scalability | Scalability API | [ScalabilityGroups] | UE |
| Audio | GConfig | [AlisAudio] | ALIS |
| Game | GConfig | [AlisGame] | ALIS |

Rule: Don't duplicate what UE already persists. Only create ALIS sections for settings UE doesn't handle natively.

### Compile-Time Safety

Service uses `GET_MEMBER_NAME_CHECKED` for all property queries. If a field is renamed/removed, compilation fails immediately.

---

## Architecture

```
FProjectUserSettings     - Data struct (UPROPERTY fields for Blueprint)
FProjectSettingsDelta    - What changed (TSet + category flags)
UProjectSettingsService  - Load/Apply/Reset (singleton, no UI knowledge)
UProjectSettingsRootWidget - UI only (caching, sync, tabs)
```

## SOLID Principles

- **S**: Service handles persistence only, Widget handles UI only
- **O**: Add settings via macro without modifying Delta/Service logic
- **L**: N/A (no inheritance hierarchy)
- **I**: Delta exposes minimal query interface (IsChanged, Has*)
- **D**: Widget depends on Service abstraction, not INI details

---

## Files

| File | Purpose |
|------|---------|
| ProjectUserSettings.h | Macro + struct definition |
| ProjectSettingsDelta.h/cpp | Change detection |
| ProjectSettingsService.h/cpp | Persistence and application |
| ProjectSettingsRootWidget.h/cpp | UI widget |

---

## Related Plugins

| Plugin | Relationship |
|--------|--------------|
| ProjectUI | Base framework (widgets, themes, MVVM) |
| ProjectMenuMain | Hosts settings screen from main menu |
| ProjectMenuGame | Hosts settings screen from pause menu |
