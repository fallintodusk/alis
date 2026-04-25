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
   - Idempotent generic layer operations, such as hiding a known definition
     that has no active widget instance yet

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

5. Keep feature-specific data resolution out of `ProjectUI`.
   - Generic widgets may render text, badges, colors, and layout state supplied
     by feature view models.
   - Generic widgets must not load feature assets, resolve gameplay
     definitions, or infer feature semantics.
   - Feature plugins should provide small presentation DTOs when one primitive
     value is not expressive enough.
   - Generic drag/drop may accept feature-supplied predicates for occupancy
     allowance, but the predicate must live in the feature plugin. ProjectUI
     must not learn inventory stack semantics.

6. Shared low-level geometry belongs below feature plugins.
   - If both gameplay and UI need the same primitive math, put the primitive in
     ProjectSharedTypes or another neutral shared-types module.
   - Do not copy rectangle/grid overlap rules into widgets.
   - Do not put data-only shared shapes into ProjectCore unless they are part
     of a ProjectCore interface contract.

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

## Tooltip and Drag/Drop Extension Points

- `FProjectUIHoverTooltipPresenter` supports anchor/pivot positioning for
  tooltips that should avoid covering the hovered target. Feature widgets pass
  viewport-space anchors; ProjectUI only clamps and applies the resulting
  position.
- `FProjectUIGridDragDropController` owns grid hit, footprint, and preview
  mechanics. Feature widgets may pass an occupancy allowance predicate so a
  domain can preview legal drops onto occupied cells without ProjectUI knowing
  domain rules.
- If a rule needs item ids, stack limits, capabilities, or feature state, keep
  that rule outside ProjectUI and pass the result through the extension point.

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
- Are layer show/hide operations idempotent for expected auto-visibility flows?
- Does `ProjectUI` remain free of feature asset resolution and gameplay
  semantics?
- Are popup and tooltip lifecycles using presenters (not custom widget-local flows)?
- Are new docs/tests updated with the same ownership boundaries?
