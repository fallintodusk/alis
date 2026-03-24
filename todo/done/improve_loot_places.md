# Improve Loot Places

Status: done.

This file is a historical delivery note only.
Behavior and architecture SOT live in:
- `../../Plugins/Features/ProjectInventory/docs/design_vision.md#world-storage-and-loot-places`
- `../../Plugins/Resources/ProjectObject/docs/layer_contract.md`

Delivered outcome:
- world storage now uses the same gameplay truth as player storage
- authored world storage uses `sections.storage` only
- `sections.loot` is removed and rejected by the parser
- world-container runtime uses `IWorldContainerSessionSource`
- inventory owns session orchestration through `UProjectContainerSessionSubsystem`
- `QuickLoot` and `FullOpen` are interaction modes over the same world-container truth
- `FullOpen` uses a single-opener rule
- nearby world storage opens in the existing inventory screen
- player <-> world transfer supports exact placement and rotation targets
- nearby-container UI includes `Take All`
- focused integration coverage exists for parser, spawn seeding, identity, session flow, busy rule, quick-loot take-all, nearby take/store, and bridge lifecycle

Key implementation routes:
- object data and spawn:
  `../../Plugins/Resources/ProjectObject/Source/ProjectObject/Public/Data/ObjectDefinition.h`
  `../../Plugins/Editor/ProjectDefinitionGenerator/Source/ProjectDefinitionGenerator/Private/DefinitionJsonParser.cpp`
  `../../Plugins/Resources/ProjectObject/Source/ProjectObject/Private/Spawning/ObjectSpawnUtility.cpp`
- runtime contracts:
  `../../Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Interfaces/IWorldContainerSessionSource.h`
  `../../Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Types/ContainerSessionTypes.h`
  `../../Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Types/WorldContainerKey.h`
- world capability:
  `../../Plugins/Gameplay/ProjectObjectCapabilities/Source/ProjectObjectCapabilities/Public/LootContainer/LootContainerCapabilityComponent.h`
- inventory orchestration:
  `../../Plugins/Features/ProjectInventory/Source/ProjectInventory/Public/Components/ProjectInventoryComponent.h`
  `../../Plugins/Features/ProjectInventory/Source/ProjectInventory/Public/Subsystems/ProjectContainerSessionSubsystem.h`
- inventory UI:
  `../../Plugins/UI/ProjectInventoryUI/Source/ProjectInventoryUI/Public/MVVM/InventoryViewModel.h`
  `../../Plugins/UI/ProjectInventoryUI/Source/ProjectInventoryUI/Public/Widgets/W_InventoryPanel.h`
  `../../Plugins/UI/ProjectInventoryUI/Data/InventoryPanel.json`
- tests:
  `../../Plugins/Test/ProjectIntegrationTests/Source/ProjectIntegrationTests/Private/Integration/InventoryLootPlacesIntegrationTest.cpp`

Validation used during delivery:
- `./scripts/ue/standalone/build.ps1`
- `./scripts/ue/test/unit/run_cpp_tests_safe.ps1 -TestFilter "ProjectIntegrationTests.InventoryLootPlaces" -Game -Map "/MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent" -TimeoutSeconds 300`

If future work starts on persistence adoption or richer multi-user revision rules,
create a new tracker instead of reopening this file as a second SOT.
