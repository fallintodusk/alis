# UI Refactor TODO (ProjectUI SOT)

Purpose: plan a refactor so ProjectUI is the single source of truth for widget creation, UI consumer plugins only provide UI data, and ProjectHUD orchestrates in-game layers. No code changes here, this is the work plan.

## Goals
- One creation path: ProjectUI owns CreateWidget/AddToViewport and ViewModel injection.
- Stable sizing and visibility: no 0x0 widgets, consistent anchors and slot policies.
- Clear ownership: ProjectUI = framework, ProjectHUD = layout host, UI consumer plugins = data only.
- SOLID with small files (target < 500 lines each) and single responsibility.
- Keep JSON layout + MVVM + Theme system as the foundation.

## Hard Guardrails (Non-Negotiable)

### Guardrail 1 - Enforce "CreateWidget/AddToViewport only in ProjectUI"

Add a repo check now (before migration):

```bash
# fail if CreateWidget appears outside ProjectUI (tune paths)
rg -n "CreateWidget<|CreateWidget\\(" Plugins/ -S \
  --glob '!Plugins/**/ProjectUI/**' \
  --glob '!Plugins/**/ThirdParty/**'
```

Also check AddToViewport in the same way:

```bash
# fail if AddToViewport appears outside ProjectUI (tune paths)
rg -n "AddToViewport" Plugins/ -S \
  --glob '!Plugins/**/ProjectUI/**' \
  --glob '!Plugins/**/ThirdParty/**'
```

AddChild is allowed inside a widget's own implementation (internal trees, lists).
Ban AddChild that inserts into ProjectHUD slot containers outside ProjectUI.

### Guardrail 2 - Keep ProjectUI Split (No God Class)

Phase 1 must create four small subsystems/classes, not one:

- UProjectUIRegistrySubsystem - definitions and lookups
- UProjectUIFactorySubsystem - widget + ViewModel creation and injection
- UProjectUILayoutValidatorSubsystem - JSON and runtime layout validation
- UProjectUILayerHostSubsystem - layer stacks and activation

### Guardrail 3 - One Sizing Policy + Hard Validation

Factory rules (errors, not guidelines) and validator checks:

- Overlay host slots default to Fill for layout roots.
- Canvas slots require explicit size or autoSize:true, never neither.
- Conflicts are errors: size + autoSize:true, stretched anchors + autoSize:true.
- Definitions must validate at startup: soft class resolves, layout JSON exists, tags exist.

## Current Pain Points (from existing docs and code)
- Multiple widget creation paths (HUD, menus, UI consumer plugins) makes ownership unclear.
- JSON layout and manual widget trees get mixed, causing slot and size bugs.
- Slot configuration is inconsistent (Canvas vs Overlay) and creates invisible widgets.
- ViewModel creation is scattered (HUD creates some, widgets create others).
- Debug logging is large but not structured; no single validator to flag layout issues.

## Target Architecture (Summary)

UI consumer plugin (data only) -> ProjectUI registry -> ProjectUI factory -> Widget + ViewModel + Layout + Theme -> ProjectHUD slots

```
+---------------------+    +-----------------------+    +----------------------+
| UI consumer data    | -> | ProjectUI Registry    | -> | ProjectUI Factory    |
| (definitions only)  |    | (definitions, rules)  |    | (CreateWidget only)  |
+---------------------+    +-----------------------+    +----------------------+
                                                     | Layout loader + Theme |
                                                     | ViewModel injection   |
                                                     +----------------------+
                                                                |
                                                                v
                                                     +----------------------+
                                                     | ProjectHUD slots     |
                                                     +----------------------+
```

## Ownership and Responsibilities

ProjectUI (SOT):
- Owns widget creation and ViewModel creation and injection.
- Owns layout loading, theme resolution, and diagnostics.
- Owns UI definition registry and load/spawn policies.
- LayerHost enforces effective input from the topmost active layer (definitions only request input).

ProjectHUD:
- Defines slot layout only (HUDLayout.json).
- Requests widgets from ProjectUI for each slot and layer.
- No CreateWidget, no ViewModel creation, no custom layout logic.

UI consumer plugins (ProjectVitalsUI, ProjectMenuMain, ProjectMenuGame, ProjectSettingsUI):
- Provide UI definitions (JSON), ViewModel classes, and JSON layouts.
- Ship UI definitions in Config/UI/ui_definitions.json (per plugin).
- No AddToViewport or CreateWidget outside ProjectUI.
- No slot composition AddChild outside ProjectUI (internal widget AddChild is OK).

Game flow:
- Controllers/GameModes request screen changes from ProjectUI (show/hide stacks).
- No direct widget creation in gameplay code.

## UI Definition Contract (Data Only)

Definition format: JSON with soft class paths (definitions and layouts are both text).
Store per plugin: Plugins/UI/<PluginName>/Config/UI/ui_definitions.json.

### JSON Schema (Minimal, KISS)

```json
[
  {
    "id": "ProjectVitalsUI.VitalsHUD",
    "layer": "UI.Layer.Game",
    "slot": "HUD.Slot.VitalsMini",
    "widget_class": "/Game/ProjectVitalsUI/W_VitalsHUD.W_VitalsHUD_C",
    "viewmodel_class": "/Script/ProjectVitalsUI.ProjectVitalsViewModel",
    "layout_json": "VitalsHUD.json",
    "load": "Preload",
    "spawn": "Persistent",
    "scope": "PerPlayer",
    "size_policy": "Fill",
    "input": "Game",
    "priority": 0,
    "vm_creation": "CreateInstance"
  }
]
```

Field notes:
- widget_class and viewmodel_class are soft class paths (BP or native).
- layout_json is relative to the plugin Config/UI folder.
- load = when the first instance is created (Preload or OnDemand).
- spawn = lifetime (Persistent or Transient).
- scope defaults to PerPlayer if omitted.
- input is requested input (Default, Game, Menu, GameAndMenu); LayerHost applies effective input.

Definition fields:
- Id (unique key)
- Layer (UI.Layer.Game, UI.Layer.Notification, UI.Layer.GameMenu, UI.Layer.Modal)
- SlotTag (optional, for HUD slots)
- WidgetClass (UProjectUserWidget derived)
- ViewModelClass (UProjectViewModel derived)
- LayoutPath (JSON layout file path)
- Load (Preload or OnDemand)
- Spawn (Persistent or Transient)
- Scope (PerPlayer, Global) optional, default PerPlayer
- InputPolicy (requested: Default, Game, Menu, GameAndMenu; LayerHost enforces effective input)
- ZOrder / Priority
- SizePolicy (Fill, Desired, AutoSize)
- vm_creation (CreateInstance, Global, PropertyPath) optional, default CreateInstance
- vm_property_path (string) optional, only for PropertyPath
- Activation rules (game state, plugin enabled, player state)

Single rule: definitions are data only. ProjectUI interprets and creates instances.

## Packaging and Staging (JSON must be staged as UFS)

JSON under plugin Config/UI is not guaranteed to stage in packaged builds.
Add explicit staging rules (UFS so FFileHelper can read them):

```ini
[/Script/UnrealEd.ProjectPackagingSettings]
+DirectoriesToAlwaysStageAsUFS=(Path="Plugins/UI/ProjectUI/Config/UI")
+DirectoriesToAlwaysStageAsUFS=(Path="Plugins/UI/ProjectHUD/Config/UI")
+DirectoriesToAlwaysStageAsUFS=(Path="Plugins/UI/ProjectVitalsUI/Config/UI")
+DirectoriesToAlwaysStageAsUFS=(Path="Plugins/UI/ProjectMenuMain/Config/UI")
+DirectoriesToAlwaysStageAsUFS=(Path="Plugins/UI/ProjectMenuGame/Config/UI")
+DirectoriesToAlwaysStageAsUFS=(Path="Plugins/UI/ProjectSettingsUI/Config/UI")
```

## Lifecycle (Target Flow)
1) ProjectUI loads per-plugin ui_definitions.json files at startup.
2) ProjectUI validates definitions (soft class resolve, layout exists, tags valid, size rules).
2) ProjectUI builds widget instances when a layer/slot becomes active.
3) ProjectUI creates ViewModel, calls Initialize/OnViewAttached, then injects.
4) Widgets load JSON layout, apply theme, bind callbacks, and refresh from ViewModel.
5) On hide/unload, ProjectUI calls OnViewDetached and Shutdown, then destroys.

## Refactor Plan (Phased)

### Phase 0 - Audit and Design
- [ ] Inventory all CreateWidget/AddToViewport calls outside ProjectUI.
- [ ] Inventory any AddChild that inserts into ProjectHUD slot containers outside ProjectUI.
- [ ] List all UI widgets and their layout JSON paths per plugin.
- [ ] Document current slot usage (HUD.Slot.VitalsMini, HUD.Slot.StatusIcons, etc.).
- [ ] Decide layer model (HUD, Overlay, Menu, Modal, Debug).
- [ ] Define ui_definitions.json schema and file location per plugin.

### Phase 0.5 - Quick Win (Prove the Pipe)
- [ ] Add ProjectUI API: ProjectUI::Show(Id, Context).
- [ ] Add ProjectUI API: ProjectUI::CreateForSlot(SlotTag, Id).
- [ ] Route one screen and one HUD slot through the new API.

### Phase 1 - ProjectUI Foundation (SOT)
- [ ] Add UI definition types and a registry subsystem in ProjectUI.
- [ ] Add ProjectUI factory subsystem that owns widget creation and ViewModel injection.
- [ ] Add a definition validator (soft class resolve, layout exists, tag validity, size conflicts).
- [ ] Add a layout validator (slot policy, anchors, autoSize, 0x0 checks).
- [ ] Add load + spawn policy handling (Preload/OnDemand, Persistent/Transient).
- [ ] Add a UI layer host API (stack per layer, topmost focus).
- [ ] Enforce Definition Id rules: globally unique, recommend PluginName.ScreenName.
- [ ] Registry rejects duplicate Ids with a loud error log.
- [ ] ProjectUI loads ui_definitions.json from each UI consumer plugin.
- [ ] Add staging rules for JSON (DirectoriesToAlwaysStageAsUFS for plugin Config/UI).

### Phase 2 - HUD Orchestration
- [ ] Refactor ProjectHUD to only host named slots and call ProjectUI.
- [ ] Move slot alignment rules into ProjectUI factory (Canvas vs Overlay policy).
- [ ] Add layer routing for HUD vs Overlay vs Debug (no menu in HUD).
- [ ] Replace any direct ViewModel creation in ProjectHUD with ProjectUI.

### Phase 3 - UI Consumer Plugin Migration
- [ ] ProjectVitalsUI: register definitions only, remove direct CreateWidget calls.
- [ ] ProjectMenuMain: screens become definitions, composer requests activation only.
- [ ] ProjectMenuGame: pause menu and settings use ProjectUI activation flow.
- [ ] ProjectSettingsUI: root widget becomes definition, ViewModel created in ProjectUI.
- [ ] Ensure all widgets use ProjectUserWidget layout load, no manual tree building.

### Phase 4 - Clean Up and Guardrails
- [ ] Remove legacy widget creation paths outside ProjectUI.
- [ ] Add CI or pre-commit check: CreateWidget/AddToViewport only in ProjectUI.
- [ ] Add CI check: slot AddChild only in ProjectUI.
- [ ] Update UI docs (ProjectUI README and UI manual) to match new flow.
- [ ] Add smoke tests: HUD builds, menu builds, layout reload, theme reload.
- [ ] Add a runtime UI health report (not in viewport, 0x0, missing layout root, missing theme keys, size conflicts).

## Size and Visibility Fixes (Must-Have Outcomes)
- Root widget always has fill anchors or explicit size (no 0x0 root).
- Overlay slots default to Fill when hosting layout roots.
- Canvas slots default to AutoSize or explicit Size, never 0x0.
- Layout loader validates conflicting fields (size + autoSize + stretched anchors).
- Diagnostics report: not in viewport, 0x0, missing RootWidget, missing theme keys.

## File Size and SOLID Rules
- Split large classes into small, focused ones (factory, registry, validator, layer host).
- Target < 500 lines per file, no "god classes".
- Each class has one reason to change (creation, layout validation, layer routing).

## Decisions Locked
- Definition storage: JSON with soft class paths (text SOT), no DataAssets for definitions.
- Provider model: per-plugin ui_definitions.json loaded by ProjectUI (no Modular Features).
- Load policy: preload minimal HUD shell (HUDLayout + VitalsMini), on-demand everything else.
- Layers: UI.Layer.Game, UI.Layer.GameMenu, UI.Layer.Modal, UI.Layer.Notification (4 layers, no Debug layer - use CVar toggle for dev widgets).
- ViewModel: CreateInstance by default. Global only when multiple widgets must share state (e.g., Vitals - VitalsHUD and VitalsPanel share VitalsViewModel).

## ViewModel Creation Policy (Epic MVVM Guidance)

Default to per-widget instances. Shared ViewModels must be explicit and rare.

Definition fields:
- vm_creation: CreateInstance (default), Global, PropertyPath
- vm_property_path: used only when vm_creation=PropertyPath

Runtime override:
- cvar: ui.vm.force_create_instance=1 (forces CreateInstance for debugging)

Practical picks (actual):
- Vitals (HUD + Panel share state): Global (both bind to same VitalsViewModel)
- Settings screens: Global (shared settings state)
- Independent HUD widgets (StatusIcons, Compass): CreateInstance
- Menus without shared state: CreateInstance

## UI Layers (Best Practice)

Layers are named stacks that control Z-order, input ownership, and lifetime. Keep the count small and consistent.

Implemented layers (4 total, see ProjectGameplayTags.h):
- UI.Layer.Game (Z=0) - gameplay HUD, input passthrough, persistent.
- UI.Layer.Notification (Z=50) - toasts/hints, input passthrough, short-lived.
- UI.Layer.GameMenu (Z=100) - inventory/pause/settings, blocks gameplay input.
- UI.Layer.Modal (Z=200) - confirm/error dialogs, blocks everything below.

Debug widgets: use CVar toggle (ui.debug=1), not a separate layer.

Simple mental model:

```
Top
  Modal        (Z=200, blocks all)
  GameMenu     (Z=100, blocks gameplay)
  Notification (Z=50, passthrough)
  Game         (Z=0, passthrough)
Bottom
```

Rules:
- Each definition picks exactly one layer.
- Factory records requested input; LayerHost enforces effective input from the topmost active layer.
- Modal must be exclusive or strictly stacked (no random overlays on top).

## Load Policy (Best Practice)

Verdict: preload only the tiny persistent HUD shell, load everything else on demand.

Preload:
- UI.Layer.Game root host (W_HUDLayout or equivalent).
- 1-2 critical HUD widgets that must always exist (vitals, crosshair).

OnDemand:
- Menus, modals, overlays, debug panels.

Rules:
- Preloaded HUD widgets are constructed once and then shown/hidden.
- Do not destroy/recreate preloaded HUD widgets during gameplay.
- Optional debug cvar: ui.preload_hud=0/1 to toggle behavior.
- load = when the first instance is created (Preload or OnDemand).
- spawn = lifetime (Persistent or Transient).

## Success Criteria
- All widgets are created through ProjectUI with full params (owner, layout, VM, theme).
- No hidden or 0x0 widgets caused by slot misconfig.
- UI consumer plugins are data only plus ViewModel logic.
- ProjectHUD is a thin slot host with no creation logic.
- UI docs clearly explain the creation and lifecycle flow end-to-end.
