# Framework Consolidation (Critical)

This document defines hard rules for keeping UI mechanics in `ProjectUI` and keeping feature plugins domain-focused.

## Objective

Build reusable UI once in `ProjectUI`, then reuse it in inventory, menus, settings, vitals, and future plugins without helper duplication.

## Ownership Rules

1. `ProjectUI` owns generic mechanics.
   - Widget lookup/binding
   - Popup and tooltip lifecycle
   - Grid hit detection, drag/drop validation, visual state coloring
   - Layer host, factory, theme, JSON layout loading

2. Feature plugins own domain meaning only.
   - Inventory item semantics and command routing
   - Menu flow decisions and feature-specific view models
   - Vitals presentation semantics
   - Settings field definitions and settings-specific data flow

3. If logic can be reused by 2+ plugins, it must live in `ProjectUI`.

4. Prefer descriptor-driven rendering over named-container branching.
   - Feature ViewModels should expose generic descriptor lists/maps.
   - Widgets should iterate descriptors and build reusable grid widgets.
   - Avoid hardcoded branches like "if backpack then..." in shared UI mechanics.
   - Descriptor groups with no entries should collapse their host widgets (no empty placeholder panels).

## Required Framework Primitives

Use these first before writing new helpers:

- `FProjectUIWidgetBinder`
- `FProjectUIPopupPresenter`
- `FProjectUIHoverTooltipPresenter`
- `FProjectUIGridHitDetector`
- `FProjectUIGridDragDropController`
- `FProjectUIGridVisualState`
- `UProjectGridCell`

Do not reintroduce inventory-named generic helpers (`FInventoryPanelDragDrop`, `FInventoryGridHitDetector`, `UInventoryGridCell`, etc.).

## Menu + Settings Best Practices

1. Settings content should be created once and reused.
   - Host settings root in a popup presenter-owned container.
   - Toggle visibility/state; do not recreate on every navigation.

2. Menu widgets should emit intent, not call game services directly.
   - Widget -> presenter/composer delegate -> controller -> service (`ILoadingService`, save service, etc.).

3. Keep menu/settings lifecycle in ProjectUI layer flow.
   - No ad-hoc top-level `CreateWidget/AddToViewport` flows outside layer host.

4. Keep popup/tooltip coordinate handling in viewport space.
   - Convert screen space to viewport space before positioning presenter widgets.

## Required Test Gates

The following automation tests must pass for UI framework changes:

- `ProjectIntegrationTests.UI.Framework.*`
- `ProjectIntegrationTests.UI.Layout.InventoryHands.DumpTree`
- `ProjectIntegrationTests.UI.Layout.InventoryHands.MultiResolution`
- `ProjectIntegrationTests.UI.Framework.MainMenu.SettingsPopupPresenterReuse`

## Review Checklist

- Is new code generic and reusable? If yes, move to `ProjectUI`.
- Is container/grid rendering descriptor-driven instead of per-container branch logic?
- Is action visibility/enabling sourced from one view-model/domain policy?
- Are required layout bindings validated once at construct time?
- Are popup and tooltip lifecycles using presenters (not custom widget-local flows)?
- Are new docs/tests updated with the same ownership boundaries?
