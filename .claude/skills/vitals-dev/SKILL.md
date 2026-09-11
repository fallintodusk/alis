---
name: vitals-dev
description: "Implements ALIS vitals gameplay and vitals UI across ProjectVitals and ProjectVitalsUI: metabolism tick, hysteresis state tags, threshold debuffs, ViewModel bindings, HUD/panel JSON definitions, and integration wiring. Use for tasks mentioning vitals, condition, stamina, calories, hydration, fatigue, State.* tags, VitalsViewModel, VitalsHUD, or vitals UI regressions."
metadata:
  author: alis-team
  version: "1.0"
---

# Vitals Developer

## Mission

Implement vitals changes quickly with low regression risk while enforcing strict ownership boundaries between gameplay simulation and UI presentation.

## Core Rule

Before any vitals logic or vitals UI behavior change, check:
- `Plugins/Gameplay/ProjectVitals/README.md`
- `Plugins/Gameplay/ProjectVitals/docs/design_vision.md`
- `Plugins/UI/ProjectVitalsUI/README.md`
- `Plugins/Gameplay/ProjectCharacter/docs/design.md`

Then load only what the task needs:
- gameplay primitives and tag contracts: `Plugins/Gameplay/ProjectGAS/README.md`, `Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/ProjectGameplayTags.h`
- UI framework seams: `Plugins/UI/ProjectUI/docs/framework_consolidation.md`, `Plugins/UI/ProjectUI/docs/ui_mvvm.md`
- HUD slot contract: `Plugins/UI/ProjectHUD/README.md`
- weight debuff coupling: `Plugins/Features/ProjectInventory/README.md`

Use memory-style refs for context hygiene:
- cite relative paths first
- open only files needed for the current task

Known stale references (fixed 2026-02-18):
- `todo/current/gas_ui_mechanics.md` and `gas_character_architecture.md` were removed; refs updated to plugin docs
- If any doc ref 404s, treat plugin READMEs and source code as source of truth

Optional runner policy:
- if your loader supports experimental `allowed-tools`, restrict to read/search/git/script execution needed for this skill
- example: `Read`, `Bash(git:*)`, `Bash(rg:*)`, `Bash(powershell:*)`
- if not supported, omit it and keep default tool policy

---

## Fast Start Protocol

1. Classify task quickly:
- domain simulation logic
- vitals UI or MVVM behavior
- integration wiring (controller/layer host/definitions)
- test gap or regression debugging

2. Load minimal context:
- always: ProjectVitals README + ProjectVitalsUI README + Character design doc
- design rationale: `Plugins/Gameplay/ProjectVitals/docs/design_vision.md` (load when questioning intent)
- domain tasks: add Vitals component header/cpp + VitalsConfig.h + gameplay tags header
- UI tasks: add VitalsViewModel, widget files, `ProjectVitalsUI/Data/*.json`, ProjectUI MVVM/framework docs
- wiring tasks: add `ProjectSinglePlayClient/SinglePlayerPlayerController.cpp`, `ProjectVitalsUI/Data/ui_definitions.json`, ProjectHUD README

3. Define acceptance before editing:
- behavior invariants
- exact commands that validate those invariants

4. Debug using evidence first:
- collect exact logs/errors before proposing fixes
- avoid speculative behavior changes

5. UI visual/wiring regressions:
- verify `ProjectVitalsUI.VitalsHUD` and `ProjectVitalsUI.VitalsPanel` entries in `Plugins/UI/ProjectVitalsUI/Data/ui_definitions.json`
- verify layer/slot/input/vm_creation values
- inspect `Saved/Logs/Alis.log` for `LogVitalsHUD`, `LogVitalsPanel`, `LogVitalsViewModel`, `LogProjectSinglePlay`

---

## Design Invariants (non-negotiable)

### Gameplay Simulation
- `UProjectVitalsComponent` ticks on server authority only.
- `ProjectVitals` is the only writer of vitals `State.*` tags.
- Tick order stays: metabolism -> bleeding drain -> state tags -> threshold debuffs -> condition delta.
- Threshold debuffs modify multipliers only; regen/drain math is computed in `TickVitals`.
- Hysteresis buffer (2 percent) remains active to prevent threshold flicker.
- `ProjectVitals` must not depend on `ProjectCharacter` (no cycle).

### UI and MVVM
- Widgets do not access pawn/character/ASC directly for game logic.
- `UVitalsViewModel` is the bridge layer and may cache ASC.
- Vitals state in UI should read `State.*` tags first, then fallback to percent thresholds.
- HUD and expanded panel share one global `VitalsViewModel` instance (`vm_creation: Global`).
- `W_VitalsHUD` is slot-hosted (`HUD.Slot.VitalsMini`); `W_VitalsPanel` is layer-hosted on demand.
- Use ProjectUI helpers for fonts/colors/styling - never hardcode paths. Key API: `UProjectWidgetLayoutLoader::ResolveThemeFont(Name, Theme)`, `ResolveThemeColor()`.

### Cross-Plugin Coupling
- Weight debuffs in vitals rely on inventory-owned weight tags (`State.Weight.Heavy`, `State.Weight.Overweight`).
- Do not move weight tag ownership into vitals.

---

## Ownership Boundaries

### ProjectVitals (gameplay)
- metabolism tick and rates
- hysteresis threshold state computation
- threshold debuff apply/remove lifecycle
- condition regen/drain net delta

### ProjectVitalsUI (feature UI)
- ASC/tag to UI property mapping in `UVitalsViewModel`
- vitals HUD and panel widgets
- vitals plugin JSON layouts and definitions

### ProjectUI and ProjectHUD (framework/composition)
- definition loading, factory, layer host, slot host
- reusable widget-binding and interaction primitives
- no vitals-specific behavior in framework layer

### ProjectCharacter and ProjectSinglePlay
- ASC lifecycle and VitalsComponent start/stop
- input and panel toggle flow for local player

### ProjectCore and ProjectGAS
- gameplay tag declarations
- attribute sets, generic GE/magnitude utilities

---

## Implementation Routes

### Route A: Domain Simulation Change
Read:
- `Plugins/Gameplay/ProjectVitals/README.md`
- `Plugins/Gameplay/ProjectVitals/Source/ProjectVitals/Public/ProjectVitalsComponent.h`
- `Plugins/Gameplay/ProjectVitals/Source/ProjectVitals/Private/ProjectVitalsComponent.cpp`
- `Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/ProjectGameplayTags.h`

Touch:
- `Plugins/Gameplay/ProjectVitals/Source/ProjectVitals/`
- optionally `Plugins/Gameplay/ProjectVitals/Source/ProjectVitals/Public/Effects/GE_ThresholdDebuffs.h` and corresponding cpp/h if debuff policy changes

Validate:
- build editor target
- run focused automation filters from existing integration suite
- run smoke test if runtime behavior changed

### Route B: Vitals UI or MVVM Change
Read:
- `Plugins/UI/ProjectVitalsUI/README.md`
- `Plugins/UI/ProjectUI/docs/framework_consolidation.md`
- `Plugins/UI/ProjectUI/docs/ui_mvvm.md`
- `Plugins/UI/ProjectHUD/README.md`

Touch:
- `Plugins/UI/ProjectVitalsUI/Source/ProjectVitalsUI/`
- `Plugins/UI/ProjectVitalsUI/Data/VitalsHUD.json`
- `Plugins/UI/ProjectVitalsUI/Data/VitalsPanel.json`
- `Plugins/UI/ProjectVitalsUI/Data/ui_definitions.json`

Validate:
- UI framework integration tests
- theme/widget integration tests
- smoke test when registration/layer behavior changed

### Route C: Integration Wiring Change
Read:
- `Plugins/Gameplay/ProjectSinglePlay/Source/ProjectSinglePlayClient/Private/SinglePlayerPlayerController.cpp`
- `Plugins/UI/ProjectVitalsUI/Data/ui_definitions.json`

Touch:
- controller binding/toggle logic
- ui definitions (id/layer/slot/input/load/spawn/vm_creation)

Validate:
- targeted integration filters for UI framework and theme
- smoke test for startup and layer host lifecycle

---

## Test Tiers

### Tier 1 (fast, required)
```powershell
scripts/ue/standalone/build.ps1
powershell -ExecutionPolicy Bypass -File scripts/ue/test/unit/run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.UI.Framework.*"
```

### Tier 2 (when UI/MVVM changed)
```powershell
powershell -ExecutionPolicy Bypass -File scripts/ue/test/unit/run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.UI.Vitals.*"
powershell -ExecutionPolicy Bypass -File scripts/ue/test/unit/run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.UI.Vitals.ViewModelTagPriority"
powershell -ExecutionPolicy Bypass -File scripts/ue/test/unit/run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.UI.Vitals.SharedViewModelContract"
powershell -ExecutionPolicy Bypass -File scripts/ue/test/unit/run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.Gameplay.Vitals.TagContract"
powershell -ExecutionPolicy Bypass -File scripts/ue/test/unit/run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.Gameplay.Vitals.HysteresisTransitions"
powershell -ExecutionPolicy Bypass -File scripts/ue/test/unit/run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.Gameplay.Vitals.DebuffHandleCleanup"
powershell -ExecutionPolicy Bypass -File scripts/ue/test/unit/run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.Gameplay.Vitals.DebuffLifecycleASC"
```

### Tier 3 (when gameplay wiring or startup flow changed)
```powershell
scripts/ue/test/smoke/boot_test.bat
```

### Diagnostics
```powershell
rg -n "LogVitalsHUD|LogVitalsPanel|LogVitalsViewModel|LogProjectSinglePlay" Saved/Logs/Alis.log
```

---

## References

- Local routing and file map: `references/local_routes.md`
- UE official links: `references/ue_links.md`

---

## Output Contract Per Task

After implementation, always provide:
1. behavior delta (what changed and why)
2. files touched
3. tests run (exact command plus pass/fail)
4. residual risks, especially vitals test coverage gaps

---

## Anti-Patterns (reject immediately)

- Client-side vitals simulation logic.
- UI writing `State.*` tags or gameplay attributes.
- Restoring periodic GE-based condition drains as primary logic path.
- Widget code directly reading game entities instead of ViewModel bridge.
- Duplicate generic UI helpers in `ProjectVitalsUI` when reusable in `ProjectUI`.
- Diverging thresholds between `ProjectVitalsComponent` and `VitalsViewModel`.
- Breaking global shared VM contract between HUD and panel.
- Unicode in docs/comments (ASCII only).
