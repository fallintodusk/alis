# Dialogue Local Routes

Use this file as a quick navigation map for dialogue tasks. Load only sections needed for the current change.

## Core Source of Truth

- dialogue runtime overview: `Plugins/Features/ProjectDialogue/README.md`
- dialogue schema and generator contract: `Plugins/Features/ProjectDialogue/Data/Schemas/dialogue.schema.json`
- object schema and capability property rules: `Plugins/Resources/ProjectObject/Data/Schemas/object.schema.json`
- object architecture and capability composition: `Plugins/Resources/ProjectObject/README.md`
- sections vs capabilities contract: `Plugins/Resources/ProjectObject/docs/layer_contract.md`

## By Task Type

### Dialogue Data Authoring (`DLG_*.json`)
- `Plugins/Resources/ProjectObject/Content/Human/GrandPa/DLG_GrandPa_Test.json`
- `Plugins/Resources/ProjectObject/Content/HumanMade/Device/Music/Rarity/DLG_Gramophone_Main.json`
- `Plugins/Features/ProjectDialogue/Source/ProjectDialogue/Public/Data/ProjectDialogueTypes.h`
- `Plugins/Features/ProjectDialogue/Source/ProjectDialogue/Public/Data/DialogueTreeDefinition.h`

### Object Wiring (`type: Dialogue`)
- `Plugins/Resources/ProjectObject/Content/HumanMade/Device/Music/Rarity/Gramophone.json`
- `Plugins/Resources/ProjectObject/Source/ProjectObject/Public/Data/ObjectDefinition.h`
- `Plugins/Resources/ProjectObject/Source/ProjectObject/Private/Template/Interactable/InteractableActor.cpp`
- `Plugins/Gameplay/ProjectObjectCapabilities/Source/ProjectObjectCapabilities/Public/CapabilityRegistry.h`

### Dialogue Runtime Logic
- `Plugins/Features/ProjectDialogue/Source/ProjectDialogue/Private/ProjectDialogue.cpp`
- `Plugins/Features/ProjectDialogue/Source/ProjectDialogue/Public/Components/ProjectDialogueComponent.h`
- `Plugins/Features/ProjectDialogue/Source/ProjectDialogue/Private/Components/ProjectDialogueComponent.cpp`
- `Plugins/Features/ProjectDialogue/Source/ProjectDialogue/Private/Services/DialogueServiceImpl.cpp`
- `Plugins/Features/ProjectDialogue/Source/ProjectDialogue/Private/Interaction/DialogueInteractionHandler.cpp`
- `Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Interfaces/IDialogueService.h`

### Dialogue UI / MVVM
- `Plugins/UI/ProjectDialogueUI/Data/ui_definitions.json`
- `Plugins/UI/ProjectDialogueUI/Source/ProjectDialogueUI/Public/MVVM/DialogueViewModel.h`
- `Plugins/UI/ProjectDialogueUI/Source/ProjectDialogueUI/Private/MVVM/DialogueViewModel.cpp`
- `Plugins/UI/ProjectDialogueUI/Source/ProjectDialogueUI/Public/Widgets/W_DialoguePanel.h`
- `Plugins/UI/ProjectDialogueUI/Source/ProjectDialogueUI/Private/Widgets/W_DialoguePanel.cpp`
- `Plugins/UI/ProjectUI/Source/ProjectUI/Private/Subsystems/ProjectUIRegistrySubsystem.cpp`
- `Plugins/UI/ProjectUI/Source/ProjectUI/Private/Subsystems/ProjectUIFactorySubsystem.cpp`
- `Plugins/UI/ProjectUI/Source/ProjectUI/Private/Subsystems/ProjectUILayerHostSubsystem.cpp`
- `Plugins/UI/ProjectUI/README.md`
- `Plugins/UI/ProjectUI/docs/ui_mvvm.md`
- `Plugins/UI/ProjectUI/docs/framework_consolidation.md`

### Feature Activation and Mode Wiring
- `Plugins/Features/ProjectFeature/README.md`
- `Plugins/Features/ProjectFeature/Source/ProjectFeature/Public/FeatureRegistry.h`
- `Plugins/Gameplay/ProjectSinglePlay/Source/ProjectSinglePlay/Private/SinglePlayModeDefaults.cpp`
- `Plugins/Gameplay/ProjectSinglePlay/Source/ProjectSinglePlay/Public/SinglePlayModeConfig.h`

## Existing Test Surface

- dialogue unit tests:
  - `Plugins/Features/ProjectDialogue/Source/ProjectDialogueTests/Private/Unit/ProjectDialogueDataTests.cpp`
  - `Plugins/Features/ProjectDialogue/Source/ProjectDialogueTests/Private/Unit/ProjectDialogueComponentTests.cpp`
- useful filters:
  - `ProjectDialogue.Data.*`
  - `ProjectDialogue.Component.*`
- test runner:
  - `scripts/ue/test/unit/run_cpp_tests_safe.ps1`
- smoke:
  - `scripts/ue/test/smoke/boot_test.bat`

## Known Stale References

- `docs/gameplay/dialogue_guide.md` (legacy bridging instructions)
- `docs/gameplay/README.md` -> `Plugins/Features/ProjectDialogue/docs/manual.md` (path missing)
- AGENTS references to `scripts/ue/check/validate_*.bat` (scripts do not exist in current tree)
