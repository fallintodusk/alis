# ProjectUI

Common UI base plugin providing reusable UI infrastructure.

ProjectUI is the shared UI framework layer for ALIS: JSON-driven layout loading, MVVM-style data flow, theming, registry-based widget creation, and descriptor-driven rendering rules.

## Docs Router

- Layout system: `docs/ui_layout.md`
- MVVM pattern: `docs/ui_mvvm.md`
- Theme system: `docs/ui_theme.md`
- CommonUI integration: `docs/commonui_integration.md`
- Framework ownership rules: `docs/framework_consolidation.md`
- Troubleshooting & Debug: `docs/troubleshooting.md` (includes `UI.Debug.DumpTree` command)
- Icon font asset notes live in the full project content tree and are not mirrored as part of the public source snapshot
- Flow: Layout builds widget trees from JSON, MVVM supplies data/commands, Theme paints the widgets.

## Critical Usage Notes

- Layouts are data-driven: set `ConfigFilePath` in your widget and let `ProjectWidgetLayoutLoader` build the tree; do not hand-construct UMG hierarchies in code.
- Buttons should declare an `"action"` string in JSON and be bound in code via a small action-to-handler map (no per-name hardcoding) to keep layouts swappable.
- Themes are the single source for colors/fonts/spacing; resolve via `ProjectUIThemeData` instead of literal values to avoid visual drift.
- Keep UI JSON/MD files ASCII-only (no smart quotes/arrows/dashes) to satisfy cross-platform tooling and CI.
- Widget creation lives in ProjectUI subsystems (no CreateWidget/AddToViewport outside ProjectUI).
- Reusable mechanics must be centralized in ProjectUI; feature plugins should only add domain adapters and view logic.
- Prefer descriptor-driven UI rendering:
  - Feature ViewModels provide generic descriptor lists/maps.
  - Shared widgets iterate descriptors instead of hardcoding container-specific branches.
  - Empty descriptor groups should collapse host widgets instead of rendering placeholder panels.
- Menu/settings integration should reuse popup presenter lifecycle and avoid recreating settings root widgets on every screen switch.

## Definitions and Creation Flow

- UI definitions live in `Plugins/<Category>/<PluginName>/Data/ui_definitions.json` per plugin (centralized schema).
- ProjectUI scans the schema folder, loads definitions into registry, validates them, and creates widgets via factory.
- LayerHost owns HUD/layout creation and applies effective input based on topmost layer.
- HUD layout class comes from ProjectUISettings (HUDLayoutClass), default `/Script/ProjectHUD.W_HUDLayout`.
- See [../../../docs/data/README.md](../../../docs/data/README.md) for the public data architecture overview.

## Provides

- ProjectViewModel - MVVM base class
- ProjectUserWidget - Base widget class
- ProjectButton - Themed button component
- ProjectWidgetLayoutLoader - JSON layout loading
- ProjectUIThemeData/Manager - Theme system
- ProjectUIRegistrySubsystem - Definition loading and lookup
- ProjectUIFactorySubsystem - Widget creation and ViewModel injection
- ProjectUILayoutValidatorSubsystem - Definition and layout validation
- ProjectUILayerHostSubsystem - HUD/layer hosting and input enforcement
- ProjectUIAnimations - Animation utilities
- ProjectEasingFunctions - Easing helpers
- Dialogs/ - Universal dialog widget (overlay + buttons from JSON)
- Effects/ - Procedural effects (FireSparks, etc.)

## Dialog System

- Dialogs build their own button widgets dynamically from JSON config
- Button click binding uses underlying SButton lambdas (not UMG dynamic delegates) because dynamic delegates require UFUNCTIONs and cannot capture data
- Binding happens in NativeConstruct after Slate widgets exist
- Font/color resolution must use LayoutThemeResolver (not local fallbacks) to load custom fonts correctly

## Used By

- ProjectMenuMain - Main menu screens
- ProjectMenuGame - In-game/pause menu
- ProjectSettingsUI - Settings screens

## Non-responsibilities

- No front-end flow logic (which screen to show when)
- Feature-specific UIs live with their feature plugins
