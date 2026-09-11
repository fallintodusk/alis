---
name: dialogue-dev
description: "Implements and validates ALIS dialogue data, runtime, and UI: DLG_*.json authoring, object capability wiring (type=Dialogue + DialogueTreeAsset), ProjectDialogue logic, ProjectDialogueUI MVVM/panel behavior, and dialogue troubleshooting. Use for tasks mentioning dialogue trees, NPC/object conversations, DLG_ files, DialogueTreeAsset, IDialogueService, DialogueViewModel, W_DialoguePanel, or dialogue interaction regressions."
metadata:
  author: alis-team
  version: "1.0"
---

# Dialogue Developer

## Mission

Implement dialogue and dialogue UI changes quickly with low regression risk, while keeping data-driven authoring as the source of truth.

## Core Rule

Before any dialogue logic or dialogue UI behavior change, check:
- `Plugins/Features/ProjectDialogue/README.md`
- `Plugins/Features/ProjectDialogue/Data/Schemas/dialogue.schema.json`
- `Plugins/Resources/ProjectObject/Data/Schemas/object.schema.json`
- `Plugins/Resources/ProjectObject/README.md`
- `Plugins/Resources/ProjectObject/docs/layer_contract.md`

Then load only task-specific files from `references/local_routes.md`.

Use memory-style refs for context hygiene:
- cite relative paths first
- open only files needed for the current task

Known doc drift:
- `docs/gameplay/dialogue_guide.md` is legacy bridging content and includes stale paths and old patterns.
- `docs/gameplay/README.md` references `Plugins/Features/ProjectDialogue/docs/manual.md`, which does not exist in this repo.
- AGENTS quick-check commands mention `scripts/ue/check/validate_*.bat`; current scripts are `check.bat`, `project_validate.bat`, and `data_validate.bat` (with mixed freshness).

Optional runner policy:
- if your loader supports experimental `allowed-tools`, restrict to read/search/git/script execution needed for this skill
- example: `Read`, `Bash(git:*)`, `Bash(rg:*)`, `Bash(powershell:*)`
- if not supported, omit it and keep default tool policy

---

## Fast Start Protocol

1. Classify task quickly:
- dialogue data authoring (`DLG_*.json`)
- object wiring (`capabilities` + `DialogueTreeAsset`)
- runtime dialogue logic (`ProjectDialogue`)
- dialogue UI/MVVM behavior (`ProjectDialogueUI` + `ProjectUI`)
- regression debugging

2. Load minimal context:
- always: dialogue README + dialogue schema + object schema
- data tasks: add real examples (`DLG_GrandPa_Test.json`, `DLG_Gramophone_Main.json`, `Gramophone.json`)
- runtime tasks: add component/service/interaction files
- UI tasks: add `DialogueViewModel`, `W_DialoguePanel`, `ui_definitions.json`, plus ProjectUI registry/factory/layer-host files

3. Define acceptance before editing:
- behavior invariant(s)
- exact test/validation command(s)

4. Debug by evidence first:
- capture exact warnings/errors from `Saved/Logs/Alis.log`
- do not apply speculative fixes before log evidence

5. For data wiring issues:
- verify `type: "Dialogue"` capability exists in object JSON
- verify `properties.DialogueTreeAsset` is full soft object path with `.Asset` suffix
- verify `DLG_*.json` id/startNode/nodes are valid and generated asset exists

---

## Architecture Snapshot

`DLG_*.json` in `Plugins/Resources/ProjectObject/Content/` -> `ProjectDefinitionGenerator` -> `UDialogueTreeDefinition` asset under `/ProjectObject` -> `UProjectDialogueComponent` capability -> `FDialogueServiceImpl` (`IDialogueService`) -> `UDialogueViewModel` -> `W_DialoguePanel`

Interaction path:
player interact -> `IInteractionService::OnInteraction` -> `FDialogueInteractionHandler` -> active component set in service -> conversation starts

---

## Design Invariants (non-negotiable)

### Data and Generation
- Dialogue source files must be named `DLG_*.json`.
- Dialogue source files must live under `Plugins/Resources/ProjectObject/Content/` (schema manifest uses `PluginName: ProjectObject`, `SourceSubDir: ../Content`, `SourceFileGlob: DLG_*.json`).
- Every dialogue JSON must include `$schema` pointing to `Features/ProjectDialogue/Data/Schemas/dialogue.schema.json` via relative path.
- Dialogue `id` should match DLG naming and file stem.
- `startNode` must exist in `nodes`.
- Option semantics:
  - `next` omitted or `"$end"` means end dialogue.
  - node with no options and no `next` (or `next="$end"`) is terminal.

### Object Wiring
- Dialogue is attached as object capability with `type: "Dialogue"` (from `GetPrimaryAssetId` in `UProjectDialogueComponent`).
- Use `scope: ["actor"]` for dialogue capability.
- `properties.DialogueTreeAsset` must be full soft object path:
  - `/ProjectObject/.../DLG_Name.DLG_Name`
- Capability `type` IDs are case-sensitive.

### Runtime Logic
- `StartConversation()` fails if `DialogueTreeAsset` is null/unloadable or invalid start node.
- Option `condition` is GameplayTag string:
  - unknown tag -> option hidden and warning logged
  - tag check uses instigator ASC when available
- One active dialogue at a time in `FDialogueServiceImpl`.
- Interaction handling is server-authoritative (`DialogueInteractionHandler` ignores non-authority instigators).

### UI and MVVM
- `ProjectDialogueUI` is blackbox over `IDialogueService`; widget never talks directly to game entities.
- Dialogue panel definition uses `vm_creation: "Global"` + `auto_visibility: "bIsActive"` in `Plugins/UI/ProjectDialogueUI/Data/ui_definitions.json`.
- **Data-driven layout**: `W_DialoguePanel` inherits `UProjectUserWidget` (NOT `UUserWidget`). Visual layout is defined in `Plugins/UI/ProjectDialogueUI/Data/DialoguePanel.json`. No Blueprint widget needed.
- Widget children resolved by `FProjectUIWidgetBinder` in `NativeConstruct()` (NOT `BindWidgetOptional`). Same pattern as `W_InventoryPanel`.
- `layout_json` field in `ui_definitions.json` tells factory to call `SetConfigFilePath()`.
- Speaker text row collapses when speaker is empty (object dialogues).
- Current panel renders max 6 options (`MaxVisibleOptions = 6`).

---

## Ownership Boundaries

### ProjectDialogue (feature runtime)
- dialogue component state machine
- interaction handler subscription
- `IDialogueService` implementation
- dialogue schema and generated dialogue asset type

### ProjectDialogueUI (feature UI)
- dialogue ViewModel and widget behavior
- dialogue plugin `ui_definitions.json`

### ProjectUI (shared framework)
- UI definition loading/validation
- widget factory and VM injection
- layer host, input arbitration, auto_visibility binding

### ProjectObject (resource data)
- object JSON definitions
- capability attachment and property import
- object-to-capability runtime composition

---

## Implementation Routes

### Route A: Add New Dialogue Tree
Read:
- `Plugins/Features/ProjectDialogue/Data/Schemas/dialogue.schema.json`
- `Plugins/Resources/ProjectObject/Content/Human/GrandPa/DLG_GrandPa_Test.json`
- `Plugins/Resources/ProjectObject/Content/HumanMade/Device/Music/Rarity/DLG_Gramophone_Main.json`

Touch:
- `Plugins/Resources/ProjectObject/Content/<Category>/<Path>/DLG_<Name>.json`

Authoring template:
```json
{
  "$schema": "<relative-path>/Features/ProjectDialogue/Data/Schemas/dialogue.schema.json",
  "id": "DLG_MyDialogue",
  "startNode": "start",
  "nodes": {
    "start": {
      "speaker": "NPC Name",
      "text": "Hello.",
      "options": [
        { "text": "Continue", "next": "next_node" },
        { "text": "Leave", "next": "$end" }
      ]
    },
    "next_node": {
      "text": "Done.",
      "next": "$end"
    }
  }
}
```

Validation focus:
- no missing `startNode`
- no dead `next` IDs unless intentionally terminal
- keep options count <= 6 unless you also update panel rendering strategy

### Route B: Attach Dialogue to Object
Read:
- `Plugins/Resources/ProjectObject/Data/Schemas/object.schema.json`
- `Plugins/Resources/ProjectObject/Content/HumanMade/Device/Music/Rarity/Gramophone.json`
- `Plugins/Resources/ProjectObject/Source/ProjectObject/Private/Template/Interactable/InteractableActor.cpp`

Touch:
- `Plugins/Resources/ProjectObject/Content/<Category>/<Path>/<Object>.json`

Capability snippet:
```json
{
  "type": "Dialogue",
  "scope": ["actor"],
  "properties": {
    "DialogueTreeAsset": "/ProjectObject/<Path>/DLG_MyDialogue.DLG_MyDialogue"
  }
}
```

Hard checks:
- keep full object path with `.AssetName` suffix
- keep `type` exactly `Dialogue`
- keep scope actor for this component

### Route C: Runtime Dialogue Logic Change
Read:
- `Plugins/Features/ProjectDialogue/Source/ProjectDialogue/Public/Components/ProjectDialogueComponent.h`
- `Plugins/Features/ProjectDialogue/Source/ProjectDialogue/Private/Components/ProjectDialogueComponent.cpp`
- `Plugins/Features/ProjectDialogue/Source/ProjectDialogue/Private/Services/DialogueServiceImpl.cpp`
- `Plugins/Features/ProjectDialogue/Source/ProjectDialogue/Private/Interaction/DialogueInteractionHandler.cpp`

Touch:
- `Plugins/Features/ProjectDialogue/Source/ProjectDialogue/`
- `Plugins/Features/ProjectDialogue/Data/Schemas/dialogue.schema.json` when data contract changes
- `Plugins/Features/ProjectDialogue/Source/ProjectDialogue/Public/Data/ProjectDialogueTypes.h` when node/option fields change

Guardrails:
- keep `IDialogueService` contract stable unless explicitly changing UI contract
- update schema + C++ structs together for data field changes
- preserve server-authoritative interaction flow

### Route D: Dialogue UI or MVVM Change
Read:
- `Plugins/UI/ProjectDialogueUI/Source/ProjectDialogueUI/Public/MVVM/DialogueViewModel.h`
- `Plugins/UI/ProjectDialogueUI/Source/ProjectDialogueUI/Private/MVVM/DialogueViewModel.cpp`
- `Plugins/UI/ProjectDialogueUI/Source/ProjectDialogueUI/Public/Widgets/W_DialoguePanel.h`
- `Plugins/UI/ProjectDialogueUI/Source/ProjectDialogueUI/Private/Widgets/W_DialoguePanel.cpp`
- `Plugins/UI/ProjectDialogueUI/Data/ui_definitions.json`
- `Plugins/UI/ProjectDialogueUI/Data/DialoguePanel.json` (JSON layout)
- `Plugins/UI/ProjectUI/Source/ProjectUI/Public/Presentation/ProjectUIWidgetBinder.h`
- `Plugins/UI/ProjectUI/Source/ProjectUI/Private/Subsystems/ProjectUIRegistrySubsystem.cpp`
- `Plugins/UI/ProjectUI/Source/ProjectUI/Private/Subsystems/ProjectUILayerHostSubsystem.cpp`

Touch:
- `Plugins/UI/ProjectDialogueUI/Source/ProjectDialogueUI/`
- `Plugins/UI/ProjectDialogueUI/Data/ui_definitions.json`
- `Plugins/UI/ProjectDialogueUI/Data/DialoguePanel.json`

Guardrails:
- keep widget -> ViewModel -> service flow
- if you rename VM property used by `auto_visibility`, update `ui_definitions.json`
- if you need more than 6 options, update panel click-handler strategy and tests together
- layout changes go in `DialoguePanel.json`, NOT in C++ widget code or Blueprints

---

## Test Tiers

### Tier 1 (fast, required)
```powershell
powershell -ExecutionPolicy Bypass -File scripts/ue/standalone/build.ps1
powershell -ExecutionPolicy Bypass -File scripts/ue/test/unit/run_cpp_tests_safe.ps1 -TestFilter "ProjectDialogue.Data.*"
powershell -ExecutionPolicy Bypass -File scripts/ue/test/unit/run_cpp_tests_safe.ps1 -TestFilter "ProjectDialogue.Component.*"
```

### Tier 2 (when dialogue JSON or object wiring changed)
```powershell
# Optional explicit regeneration if editor watcher is not running:
# Requires UE_PATH in environment.
& "$env:UE_PATH\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" ".\\Alis.uproject" -run=GenerateDefinitions -type=Dialogue -unattended -nop4 -nosplash -log
& "$env:UE_PATH\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" ".\\Alis.uproject" -run=GenerateDefinitions -type=Object -unattended -nop4 -nosplash -log
```

### Tier 3 (startup confidence)
```powershell
scripts/ue/test/smoke/boot_test.bat
```

### Diagnostics
```powershell
rg -n "LogProjectDialogue|LogDialogueComponent|LogDialogueInteraction|LogDialogueVM|LogDialoguePanel|LogDefinitionGeneratorEditor|LogInteractableActor|ProjectDialogueUI.DialoguePanel" Saved/Logs/Alis.log
```

---

## Troubleshooting Shortlist

- Dialogue does not start on interact:
  - verify object has `type: "Dialogue"` capability
  - verify feature list includes `Interaction` and `Dialogue` in `Plugins/Gameplay/ProjectSinglePlay/Source/ProjectSinglePlay/Private/SinglePlayModeDefaults.cpp`
  - check `LogDialogueInteraction` subscription and authority logs

- Warning `No dialogue tree loaded`:
  - `DialogueTreeAsset` path is null, wrong, or unresolved
  - generated `DLG_` asset missing or out of date

- Dialogue active but panel not visible:
  - verify `ProjectDialogueUI.DialoguePanel` definition loaded
  - verify `vm_creation: "Global"` and `auto_visibility: "bIsActive"`
  - verify `layout_json: "DialoguePanel.json"` exists in ui_definitions.json
  - inspect `LogDialogueVM`, `LogProjectUILayerHost`, and `LogProjectUserWidget`

- Dialogue panel visible but empty (no text/buttons):
  - check `LogDialoguePanel` for "RootWidget is null" (missing layout_json)
  - verify DialoguePanel.json exists and has correct widget names
  - verify W_DialoguePanel inherits UProjectUserWidget (NOT UUserWidget)

- Some options not shown:
  - condition tag not present or unknown
  - more than 6 options (panel currently truncates to 6)

---

## References

- Local file map: `references/local_routes.md`
- UE official links: `references/ue_links.md`

---

## Output Contract Per Task

After implementation, always provide:
1. behavior delta (what changed and why)
2. files touched
3. tests run (exact command plus pass/fail)
4. residual risks or follow-ups

---

## Anti-Patterns (reject immediately)

- Adding dialogue JSON outside `ProjectObject/Content` or without `DLG_` prefix.
- Using partial or short soft paths for `DialogueTreeAsset` (must include `.Asset` suffix).
- Typo or case mismatch in capability type (`Dialogue` is exact).
- Widget logic that bypasses `UDialogueViewModel` and reaches game entities directly.
- Assuming all options render when node has >6 choices without panel changes.
- Treating `docs/gameplay/dialogue_guide.md` as source of truth for current implementation.
- Using `BindWidgetOptional` or Blueprint subclass pattern instead of JSON layout + `FProjectUIWidgetBinder`.
- Inheriting `UUserWidget` directly instead of `UProjectUserWidget` (breaks VM injection and JSON layout).
- Unicode in docs/comments (ASCII only).
