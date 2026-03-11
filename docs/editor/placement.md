# Editor Placement System

Centralized Place Actors panel registration for project templates and items.

## Quick Reference

| Task | Location |
|------|----------|
| Add new placeable actor | [ProjectPlacementEditor](../../Plugins/Editor/ProjectPlacementEditor/README.md) |
| Door configuration | [ProjectObject](../../Plugins/Resources/ProjectObject/README.md) |
| Item definitions | [ProjectObject](../../Plugins/Resources/ProjectObject/README.md) |
| Pickup/Loot actors | [ProjectObject](../../Plugins/Resources/ProjectObject/README.md) |

## Categories in Place Actors Panel

| Category | Priority | Contents |
|----------|----------|----------|
| PROJECT_Template | 55 | Base template actors (pickup, loot, door) |
| PROJECT_Pickup | 56 | Pickup-capable ObjectDefinitions |

## Architecture Decision

### Why Centralized Placement?

**Decision:** All Place Actors registration lives in `ProjectPlacementEditor` plugin.

**Rationale:**
1. **Single source of truth** - One place to add/modify placeable actors
2. **Proper dependencies** - Editor plugin depends on Runtime plugins (not reverse)
3. **No code duplication** - Category registration code exists once
4. **Easy extension** - Add new actor = add one line to registration

**Alternative considered:** Each runtime plugin has its own *Editor module
- Rejected because: Scattered code, duplicate registration logic, harder to maintain consistency

### Dependency Direction

```
ProjectPlacementEditor (Editor tier)
    |
    v depends on (correct direction)
ProjectObject (Runtime tier)
```

Editor modules should depend on runtime modules, never the reverse. This ensures runtime code ships without editor dependencies.

## Implementation Details

See [ProjectPlacementEditor README](../../Plugins/Editor/ProjectPlacementEditor/README.md) for:
- How to add new placeable actors
- Category configuration
- ObjectDefinition pickup auto-registration
- Files changed during centralization

## Related Documentation

- [plugins.md](../plugins.md) - Plugin tier system
- [conventions.md](../conventions.md) - Plugin naming and structure
