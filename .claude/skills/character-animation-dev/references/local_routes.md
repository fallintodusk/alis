# Character Animation Local Routes

Use this file as a quick navigation map for modular character animation tasks. Load only sections needed for the current change.

## Single Source Of Truth

- parity and debug flow:
  - `docs/testing/character_parity.md`

## Authored Data

- modular hero definition:
  - `Plugins/Resources/ProjectObject/Content/Human/Hero/Hero.json`
- object data contract:
  - `Plugins/Resources/ProjectObject/docs/layer_contract.md`
- object definition types:
  - `Plugins/Resources/ProjectObject/Source/ProjectObject/Public/Data/ObjectDefinition.h`

## Runtime Character Wiring

- modular character class:
  - `Plugins/Gameplay/ProjectCharacter/Source/ProjectCharacter/Private/DefinitionCharacter.cpp`
- legacy character baseline:
  - `Plugins/Gameplay/ProjectCharacter/Source/ProjectCharacter/Private/ProjectCharacter.cpp`
- object spawn utility:
  - `Plugins/Resources/ProjectObject/Source/ProjectObject/Private/Spawning/ObjectSpawnUtility.cpp`
- single-player switching and routing:
  - `Plugins/Gameplay/ProjectSinglePlay/README.md`

## Skeletal Assembly And Capabilities

- assembly architecture:
  - `Plugins/Systems/ProjectSkeletalAssembly/docs/architecture.md`
- capture component:
  - `Plugins/Systems/ProjectSkeletalAssembly/Source/ProjectSkeletalAssembly/Public/CharacterDebugCaptureComponent.h`
  - `Plugins/Systems/ProjectSkeletalAssembly/Source/ProjectSkeletalAssembly/Private/CharacterDebugCaptureComponent.cpp`
- capability rationale:
  - `Plugins/Gameplay/ProjectSkeletalCapabilities/docs/architecture.md`
- Motion Matching capability and bridge:
  - `Plugins/Gameplay/ProjectSkeletalCapabilities/Source/ProjectSkeletalCapabilities/Public/MotionMatchingCapability.h`
  - `Plugins/Gameplay/ProjectSkeletalCapabilities/Source/ProjectSkeletalCapabilities/Private/MotionMatchingCapability.cpp`
  - `Plugins/Gameplay/ProjectSkeletalCapabilities/Source/ProjectSkeletalCapabilities/Public/MotionMatchingBridgeAnimInstance.h`
  - `Plugins/Gameplay/ProjectSkeletalCapabilities/Source/ProjectSkeletalCapabilities/Private/MotionMatchingBridgeAnimInstance.cpp`
- Mutable customization:
  - `Plugins/Gameplay/ProjectSkeletalCapabilities/Source/ProjectSkeletalCapabilities/Public/MutableCustomizationCapability.h`
  - `Plugins/Gameplay/ProjectSkeletalCapabilities/Source/ProjectSkeletalCapabilities/Private/MutableCustomizationCapability.cpp`
- local first-person body:
  - `Plugins/Gameplay/ProjectSkeletalCapabilities/Source/ProjectSkeletalCapabilities/Public/LocalFirstPersonCapability.h`
  - `Plugins/Gameplay/ProjectSkeletalCapabilities/Source/ProjectSkeletalCapabilities/Private/LocalFirstPersonCapability.cpp`
  - `Plugins/Gameplay/ProjectSkeletalCapabilities/Source/ProjectSkeletalCapabilities/Public/LocalBodyAnimInstance.h`

## Existing Test Surface

- parity wrapper:
  - `scripts/ue/test/character/capture_parity.ps1`
- generic test runners:
  - `scripts/ue/test/unit/run_cpp_tests_safe.ps1`
  - `scripts/ue/test/unit/run_ue_test_safe.ps1`
- C++ parity test:
  - `Plugins/Test/ProjectIntegrationTests/Source/ProjectIntegrationTests/Private/Integration/CharacterParityTest.cpp`

## Saved Artifacts

- parity captures:
  - `Saved/Validation/CharacterDebug/`
- legacy CDO dump:
  - `Saved/Inspection/BP_Hero_CDO_2026-04-03.json`
- runtime log:
  - `Saved/Logs/Alis.log`

## Active TODOs

- skeletal assembly tracker:
  - `todo/current/create_skeletal_assembly_framework.md`
- first-person clipping:
  - `todo/current/fix_fp_body_clipping.md`
- first-person body system:
  - `todo/current/fp_body_system.md`
