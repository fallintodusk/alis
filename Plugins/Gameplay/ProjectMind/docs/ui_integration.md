# ProjectMind UI Integration

## Responsibility

UI integration should use existing framework:
- ProjectUI registry/factory/layer host
- ProjectHUD composition host patterns
- MVVM view models for state projection
- dedicated UI plugin: `Plugins/UI/ProjectMindUI`

ProjectMind UI should not reinvent:
- custom popup systems
- custom layer routing
- custom widget lifecycle outside ProjectUI

## Integration Pattern

Recommended:
- define UI entries in `ui_definitions.json`
- implement mind widgets/view models in `ProjectMindUI` (not in `ProjectHUD`)
- use ProjectUI layers for feed and journal presentation
- bind via view models
- consume thought stream through abstractions

## Layout Direction

- temporary thought feed: top-right overlay-style presentation
- popup content split:
- header (`INNER VOICE`)
- source/meta line (e.g. `Vitals | Critical`, `Dialogue`, `Scan`)
- concise body thought text
- journal screen/panel: menu-layer presentation with two history tabs
- `Important` tab: durable records (Journal + ToastAndJournal channel thoughts)
- `Recent Log` tab: temporary activity log (Toast channel thoughts only)
- journal content uses scrollable regions to keep long sessions readable
- durable records support lifecycle:
- `Active` records stay at top of Important
- `Resolved` records remain visible but move below active records

Note:
- follow ProjectUI framework consolidation rules
- keep ProjectHUD as composition host, not feature logic host

## Contracts

UI should depend on:
- ProjectUI framework primitives
- ProjectCore contracts for thought stream access

UI should avoid:
- direct runtime-class calls into ProjectMind internals
- adding one-off mechanics that belong to ProjectUI
