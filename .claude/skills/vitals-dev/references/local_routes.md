# Vitals Local Routes

Use this file as quick navigation for vitals tasks. Load only sections needed for the current task.

## Core Source of Truth

- gameplay simulation: `Plugins/Gameplay/ProjectVitals/README.md`
- vitals UI and MVVM contract: `Plugins/UI/ProjectVitalsUI/README.md`
- character and vitals wiring: `Plugins/Gameplay/ProjectCharacter/docs/design.md`

## By Task Type

### Domain Simulation
- `Plugins/Gameplay/ProjectVitals/Source/ProjectVitals/Public/ProjectVitalsComponent.h`
- `Plugins/Gameplay/ProjectVitals/Source/ProjectVitals/Private/ProjectVitalsComponent.cpp`
- `Plugins/Gameplay/ProjectVitals/Source/ProjectVitals/Public/Effects/GE_ThresholdDebuffs.h`
- `Plugins/Gameplay/ProjectVitals/Source/ProjectVitals/Private/Effects/GE_ThresholdDebuffs.cpp`

### UI and MVVM
- `Plugins/UI/ProjectVitalsUI/Source/ProjectVitalsUI/Public/MVVM/VitalsViewModel.h`
- `Plugins/UI/ProjectVitalsUI/Source/ProjectVitalsUI/Private/MVVM/VitalsViewModel.cpp`
- `Plugins/UI/ProjectVitalsUI/Source/ProjectVitalsUI/Public/Widgets/W_VitalsHUD.h`
- `Plugins/UI/ProjectVitalsUI/Source/ProjectVitalsUI/Private/Widgets/W_VitalsHUD.cpp`
- `Plugins/UI/ProjectVitalsUI/Source/ProjectVitalsUI/Public/Widgets/W_VitalsPanel.h`
- `Plugins/UI/ProjectVitalsUI/Source/ProjectVitalsUI/Private/Widgets/W_VitalsPanel.cpp`

### UI Definitions and Layout Data
- `Plugins/UI/ProjectVitalsUI/Data/ui_definitions.json`
- `Plugins/UI/ProjectVitalsUI/Data/VitalsHUD.json`
- `Plugins/UI/ProjectVitalsUI/Data/VitalsPanel.json`
- `Plugins/UI/ProjectHUD/README.md`

### Integration Wiring
- `Plugins/Gameplay/ProjectSinglePlay/Source/ProjectSinglePlayClient/Private/SinglePlayerPlayerController.cpp`
- `Plugins/UI/ProjectUI/README.md`
- `Plugins/UI/ProjectUI/docs/framework_consolidation.md`
- `Plugins/UI/ProjectUI/docs/ui_mvvm.md`

### Gameplay Tags and Coupling
- `Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/ProjectGameplayTags.h`
- `Plugins/Gameplay/ProjectGAS/README.md`
- `Plugins/Features/ProjectInventory/README.md`

## Existing Test Surface

- integration tests root: `Plugins/Test/ProjectIntegrationTests/Source/ProjectIntegrationTests/Private/Integration/`
- vitals integration tests: `Plugins/Test/ProjectIntegrationTests/Source/ProjectIntegrationTests/Private/Integration/ProjectVitalsIntegrationTest.cpp`
- known useful filters:
  - `ProjectIntegrationTests.UI.Vitals.*`
  - `ProjectIntegrationTests.UI.Vitals.ViewModelTagPriority`
  - `ProjectIntegrationTests.UI.Vitals.SharedViewModelContract`
  - `ProjectIntegrationTests.Gameplay.Vitals.TagContract`
  - `ProjectIntegrationTests.Gameplay.Vitals.HysteresisTransitions`
  - `ProjectIntegrationTests.Gameplay.Vitals.DebuffHandleCleanup`
  - `ProjectIntegrationTests.Gameplay.Vitals.DebuffLifecycleASC`
  - `ProjectIntegrationTests.UI.Framework.*`
- smoke script: `scripts/ue/test/smoke/boot_test.bat`
- optional hardening backlog: `Plugins/Gameplay/ProjectVitals/TODO.md`

## Known Stale Paths Mentioned in Some Docs

- `todo/current/gas_ui_mechanics.md`
- `todo/current/gas_character_architecture.md`

If these files are missing, use plugin READMEs and source files above as source of truth.
