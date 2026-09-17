# Structure

## Purpose

ProjectUI realizes shared presentation definitions and supplies reusable UI
mechanics without owning feature or gameplay decisions.

## Owns

- UI definition discovery and validation.
- Widget construction and ViewModel injection.
- HUD/layer hosting, visibility, and effective input policy.
- Shared JSON layout, MVVM, theme, animation, dialog, and effect mechanics.
- Loading-screen presentation driven by loading state.

## Does not own

- Which experience or feature a user chooses.
- Loading execution or terminal loading authority, owned by
  [ProjectLoading](../../../../Systems/ProjectLoading/README.md).
- Domain behavior owned by feature and gameplay plugins.

## Composition

| Part | Responsibility |
|---|---|
| Registry subsystem | Discover and validate UI definitions |
| Factory subsystem | Create widgets and inject ViewModels |
| Layer host subsystem | Host persistent/transient layers and input policy |
| Layout loader | Build widget trees from JSON |
| ViewModel base | Observable presentation data and commands |
| Theme manager | Shared visual values and fonts |
| Loading-screen subsystem | Observe loading and manage its widget |

## Relationships

Relationships are drawn once in [the main view](diagrams/main.md).

Through UI definition files, feature presentation owners request shared
realization without bypassing the registry and factory. Through loading events,
the loading-screen adapter observes state but does not decide its outcome.

## Invariants

1. Shared widget creation goes through ProjectUI ownership boundaries.
2. Domain-specific decisions remain in the feature UI owner.
3. A definition is visible only while its widget is attached to the current
   viewport and has visible Slate state.
4. Layer input policy follows the highest active layer for the owning player.
5. Layout and theme data are validated before realization.
