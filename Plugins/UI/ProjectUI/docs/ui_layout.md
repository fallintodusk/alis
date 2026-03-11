# ProjectUI Layout System

Data-driven layout loader that builds widget trees from JSON so UI screens stay designer-friendly and hot-reloadable without Blueprint rewiring.

## Scope and Paths
- Plugin: `Plugins/UI/ProjectUI`
- Loader classes: `Source/ProjectUI/Public/Data/ProjectWidgetLayoutLoader.h` and `.../ProjectWidgetLayoutLoader.cpp`
- Base widget: `Source/ProjectUI/Public/Widgets/ProjectUserWidget.h`
- Layout configs: `Config/UI/*.json` (per-screen) plus optional templates under `Config/UI/Common`

## How It Works
1) A `UProjectUserWidget` declares `ConfigFilePath` (e.g. `Config/UI/MainMenu.json`).  
2) `UProjectWidgetLayoutLoader` parses the JSON, spawns widgets, applies properties/layout, and wires children.  
3) Theme lookups resolve color and font strings via `UProjectUIThemeData`.  
4) Hot reload: calling `ReloadLayout()` clears and rebuilds the tree without restarting PIE.

## JSON Shape (condensed)
```json
{
  "version": "1.0",
  "root": {
    "type": "CanvasPanel",
    "name": "RootCanvas",
    "layout": { "anchors": { "minimum": {"x":0,"y":0}, "maximum": {"x":1,"y":1} } },
    "children": [
      {
        "type": "Button",
        "name": "PlayButton",
        "properties": { "text": "Play", "variant": "Primary", "font": "ButtonFont" },
        "layout": { "size": {"x": 300, "y": 60} }
      }
    ]
  }
}
```

### Supported Widget Types
- Containers: `CanvasPanel`, `VerticalBox`, `HorizontalBox`, `Border`
- Content: `Button`, `TextBlock`, `Image`, `ProgressBar`, `Spacer`
- Layout fields: `position`, `size`, `anchors.minimum/maximum`, `alignment`, optional panel-specific padding

### Property Guidelines
- Colors/fonts use theme keys (`PrimaryColor`, `HeadingLargeFont`) or hex as fallback.
- Buttons support `variant` (Primary/Secondary/Success/Warning/Error/Text) and `size` (Small/Medium/Large).
- Keep widget `name` unique for runtime lookups.

## Hot Reload Workflow
- Edit JSON under `Config/UI/`.
- In PIE, run console command registered by the screen (e.g. `ReloadMainMenu`) or call `ReloadLayout(JsonPath, RootWidget, Theme)`.
- Loader clears children then rebuilds; theme lookups are reapplied.

## Layout Hard Rules

### Rule A: Prefer Box Containers Over Canvas

**Canvas is appropriate for:**
- HUD root anchors (screen-relative positioning)
- Absolute overlays (crosshair, debug)
- Floating elements with explicit positioning

**Canvas is NOT appropriate for:**
- Inventory panel internals
- List/grid content
- Responsive layouts

**Preferred containers:**
- `Overlay` - layered stacking
- `HorizontalBox` / `VerticalBox` - flow layouts
- `UniformGridPanel` - structured grids
- `SizeBox` - fixed/min/max constraints
- `Border` - padding + background

### Rule B: Explicit Size Policy

Every slot should have clear sizing intent:
- `Fill` - take available space (slot SizeRule=Fill)
- `Auto` - size to content (slot SizeRule=Auto)
- `Fixed` - explicit size via SizeBox wrapper
- `Clamp` - min/max constraints via SizeBox

**No implicit stretching** - if a widget fills, it should be explicit in JSON.

### Rule C: Z-Order Layer System

Named layers for consistent stacking:

| Layer | Z-Order | Purpose |
|-------|---------|---------|
| Background | 0 | Static backgrounds |
| HUD | 100 | Always-visible game info |
| Panel | 200 | Inventory, menus |
| Modal | 300 | Dialogs, confirmations |
| Toast | 400 | Notifications |
| Tooltip | 500 | Hover info |
| Debug | 900 | Debug overlays |

Toasts/tooltips must NEVER appear behind panels.

### Rule D: Visibility Must Be Explicit

- `Hidden` - not visible, keeps layout space
- `Collapsed` - not visible, no layout space
- Hit-testing must be explicitly declared when needed

---

## Best Practices
- Keep JSON and themes in sync: prefer theme keys over hex literals.
- Avoid mixing manual widget creation with JSON-driven trees for the same root.
- Use anchors + alignment instead of absolute positions for responsive layouts.
- Validate JSON before load; log and early-return on missing files or bad schema.
- Store repeated structures as templates under `Config/UI/Common`.

## References
- Theme system: `docs/ui_theme.md`
- MVVM bindings on top of layouts: `docs/ui_mvvm.md`
- Global router page: `../../../docs/ui.md`
