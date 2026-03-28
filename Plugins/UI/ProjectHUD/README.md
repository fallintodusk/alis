# ProjectHUD

Lyra-style HUD composition root with slot-based widget hosting.

## Overview

ProjectHUD provides `W_HUDLayout`, a thin composition widget that defines
named slots and populates them with widgets from ProjectUI definitions.

This enables a modular HUD architecture where UI consumer plugins define
slot widgets in JSON, and the HUD layout hosts them without direct coupling.

Scope
- ProjectHUD owns only HUD slot containers and the HUD layout widget.
- Overlay panels (Inventory, VitalsPanel, menus) are not HUD slots.
- Overlay panels are shown via ProjectUI layer host (ShowDefinition/HideDefinition).
- Interaction prompt progress is part of the shared HUD prompt path, not a
  feature-specific inventory widget.

Framework consolidation rules
- Keep `W_HUDLayout` as a composition host only.
- Do not duplicate popup, tooltip, binding, or grid mechanics in ProjectHUD.
- Reuse ProjectUI framework primitives and follow `Plugins/UI/ProjectUI/docs/framework_consolidation.md`.

## Architecture

```
+-------------------+     +-----------------------+     +------------------+
| UI consumer JSON  |     | ProjectUI Registry    |     | W_HUDLayout      |
| (VitalsHUD, etc.) |---->| Definitions by slot   |---->| RebuildSlot()    |
+-------------------+     +-----------------------+     +------------------+
```

## Slots

| Slot Tag | Container Name | Purpose |
|----------|---------------|---------|
| `HUD.Slot.VitalsMini` | `VitalsMiniSlot` | Condition bar + warning icons |
| `HUD.Slot.StatusIcons` | `StatusIconsSlot` | Status effect icons |
| `HUD.Slot.MindThought` | `MindThoughtSlot` | Temporary internal thought toast (top-right) |
| `HUD.Slot.Compass` | (future) | Compass/bearing |
| `HUD.Slot.Minimap` | (future) | Minimap |

## Usage

### Game Code (Initialize HUD)

```cpp
UProjectUILayerHostSubsystem* LayerHost =
    GetGameInstance()->GetSubsystem<UProjectUILayerHostSubsystem>();

if (LayerHost)
{
    LayerHost->InitializeForPlayer(PlayerController);
}
```

### UI Consumer Plugin (Definition)

Add `Config/UI/ui_definitions.json` with a slot entry:

```json
{
  "id": "ProjectVitalsUI.VitalsHUD",
  "slot": "HUD.Slot.VitalsMini",
  "widget_class": "/Script/ProjectVitalsUI.W_VitalsHUD"
}
```

## Design Decisions

1. **JSON-built layout** - No BindWidget, finds slots by name using `FindWidgetByNameTyped`
2. **Factory sizing rules** - Slot sizing is enforced in ProjectUI factory
3. **ViewModel Outer = Widget** - VM lifecycle tied to widget, not layout
4. **Thin layout** - W_HUDLayout is slot hosting only
5. **Timed interaction prompt lives here** - hold progress for search/open flows
   is rendered by the shared interaction prompt widget and ProjectUI primitives

## Files

- `W_HUDLayout.h/.cpp` - Composition root widget
- `Config/UI/HUDLayout.json` - Layout definition
- `ProjectHUDModule.h/.cpp` - Module (intentionally empty)

## Dependencies

- ProjectCore (gameplay tags)
- ProjectUI (registry, factory, ProjectUserWidget, ProjectWidgetHelpers)

## SOT

`todo/current/gas_ui_mechanics.md` (Phase 10)

Related behavior docs
- Interaction flow: `Plugins/Gameplay/ProjectInteraction/README.md`
- Inventory/world-storage behavior:
  `Plugins/Features/ProjectInventory/docs/design_vision.md`
