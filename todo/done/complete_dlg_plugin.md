# Complete ProjectDialogue Plugin

Implementation tracker for the universal dialogue system.

## Remaining Work

### Integrate DialogueUI with HUD Layer System

Widget exists but is never shown - not registered with layer system, no Show/Hide calls.

- [ ] Create `Plugins/UI/ProjectDialogueUI/Data/ui_definitions.json`
  - Preload + Persistent, layer UI.Layer.GameMenu, input Menu
  - Widget: W_DialoguePanel, VM: DialogueViewModel
  - No slot (fullscreen overlay, shown/hidden on demand)

- [ ] Add dialogue VM bindings to `SinglePlayerPlayerController`
  - File: `Plugins/Gameplay/ProjectSinglePlay/Source/ProjectSinglePlayClient/Public/SinglePlayerPlayerController.h`
  - File: `Plugins/Gameplay/ProjectSinglePlay/Source/ProjectSinglePlayClient/Private/SinglePlayerPlayerController.cpp`
  - Pattern: exact copy of inventory VM binding
  - `InitializeDialogueUI()` called from `OnPossess()`
  - `TryBindDialogueViewModel()` gets VM from Factory
  - `HandleDialogueViewModelPropertyChanged()`:
    - bIsActive=true -> `ShowDefinition("ProjectDialogueUI.DialoguePanel")`
    - bIsActive=false -> `HideDefinition("ProjectDialogueUI.DialoguePanel")`

- [ ] Add `ProjectDialogueUI` to `ProjectSinglePlayClient.Build.cs` PrivateDependencyModuleNames

- [ ] Build and test
  - Place gramophone -> press E -> dialogue panel appears with cursor
  - Click options -> navigate tree
  - Click "Leave" -> panel hides, cursor hides, camera unlocked

### Runtime Flow

```
Press E on Gramophone
  -> OnComponentInteract -> StartConversation()
  -> DialogueInteractionHandler -> Service.SetActiveComponent()
  -> Service broadcasts OnDialogueStateChanged
  -> DialogueViewModel.bIsActive = true
  -> Controller.HandleDialogueViewModelPropertyChanged("bIsActive")
  -> LayerHost->ShowDefinition("ProjectDialogueUI.DialoguePanel")
  -> Panel visible, input mode = Menu (cursor shown)
  -> Player clicks dialogue options via mouse
  -> EndConversation()
  -> bIsActive = false
  -> HideDefinition -> input mode restored to Game
```

## What's Done

### Capability Registration
- [x] `GetPrimaryAssetId()` on UProjectDialogueComponent -> ("CapabilityComponent", "Dialogue")
- [x] `IInteractableComponentTargetInterface` on UProjectDialogueComponent (interaction system discovers it)
- [x] Gramophone.json object definition with Dialogue capability
- [x] DLG_Gramophone_Main.json dialogue tree (menu with Play/Pause/Songs/Leave)
- [x] Asset path fix: TSoftObjectPtr needs full UE object path (Package.AssetName)
- [x] x-alis-notes in object.schema.json documenting asset reference format
- [x] 34 unit tests passing

### Core System
- [x] FDialogueOption, FDialogueNode structs
- [x] UDialogueTreeDefinition UAsset class
- [x] dialogue.schema.json with x-alis-generator (GeneratedContentPath: /ProjectObject)
- [x] DLG_*.json files co-located with objects in ProjectObject/Content/
- [x] UProjectDialogueComponent: tree navigation, GameplayTag conditions
- [x] FDialogueServiceImpl + IDialogueService bridge
- [x] FDialogueInteractionHandler (IInteractionService subscriber)
- [x] ProjectDialogueUI: ViewModel + W_DialoguePanel (blackbox)
- [x] Inline $schema in DLG_ files (IDE-agnostic validation)
- [x] DLG_ prefix convention for asset type identification
- [x] Generator: flat -> hierarchical path (same dir as JSON source)
- [x] Empty directory cleanup in generator
