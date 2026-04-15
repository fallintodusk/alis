# Implement ADefinitionCharacter -- Data-Driven Pawn Spawning

Parent doc: [create_skeletal_assembly_framework.md](create_skeletal_assembly_framework.md) (Phase 3)

Plan: `~/.claude/plans/staged-marinating-kazoo.md`

Status: implemented behind explicit character-system selection, awaiting clean build + PIE test

---

## Design Decisions (resolved)

Domain boundary: object JSON = world/object composition. Player pawn contract (input, abilities, movement) = C++. No premature config abstraction for a single consumer.

1. **Input actions** -- `ConstructorHelpers` in C++ constructor. One player pawn contract, one consumer, no variation. A DataAsset would add a class + asset + indirection for zero benefit at this stage.

2. **StartupAbilitySets** -- `ConstructorHelpers` in C++ constructor. Same reasoning: one bootstrap set, no per-definition variation. Data-driven path only when real variation appears.

3. **Movement speeds** -- Copy exact ProjectCharacter values into C++ (Walk 180, Run 350, Sprint 550, Crouch 110). Locomotion policy belongs to the character contract, not object composition.

4. **FViewSection** -- Apply `relativeOffset` only. Parse/store/log `defaultMode`, `cameraParent`, `attachmentPolicy` but no runtime logic until a real consumer needs it.

---

## Goal

Introduce a fully data-driven modular pawn path alongside `BP_Hero`. One generic C++ character class (`ADefinitionCharacter`) is populated by `ObjectSpawnUtility::SpawnFromDefinition()` from the generated `UObjectDefinition` DataAsset, while legacy remains the default baseline until parity is proven.

## Design Rules

1. `ADefinitionCharacter` is a clean new class, NOT a copy of `AProjectCharacter`
2. GameMode is selector/orchestrator only -- does NOT interpret definition internals
3. Character self-configures from its definition (view section, camera) via assembly lifecycle delegate
4. Rollout stays explicit: gameplay `Mode` remains gameplay-only, while `CharacterSystem` / `project.character.switch` opt into the modular path until legacy is retired

## Steps

- [x] 1. Create `ADefinitionCharacter` in ProjectCharacter
  - Clean new class: ASC, AttributeSets, VitalsComponent, Camera, Capsule, Movement, Input
  - NO hardcoded mesh subobjects
  - Listens to assembly `OnAssemblyStateChanged` to apply view section when Ready
  - Files: `Public/DefinitionCharacter.h`, `Private/DefinitionCharacter.cpp`

- [x] 2. Add dedicated character-system selection in GameMode
  - Files: `ProjectSinglePlay/Public/SinglePlayerGameMode.h`, `ProjectSinglePlay/Private/SinglePlayerGameMode.cpp`
  - `Mode` stays focused on gameplay presets
  - `CharacterSystem` / `CharacterDefinition` control legacy vs modular spawn
  - `project.character.switch legacy|modular [Hero]` respawns through GameMode

- [x] 3. Override `SpawnDefaultPawnAtTransform` in GameMode
  - File: `ProjectSinglePlay/Private/SinglePlayerGameMode.cpp`
  - If `CharacterSystem=Modular` -> load definition, call SpawnFromDefinition
  - Else -> Super (legacy BP_Hero)

- [x] 4. Update Hero.json spawnClass to C++ class
  - File: `ProjectObject/Content/Human/Hero/Hero.json`
  - `"spawnClass": "/Script/ProjectCharacter.DefinitionCharacter"`

- [x] 5. Keep gameplay modes clean and move rollout to character-system selection
  - File: `ProjectSinglePlay/Private/SinglePlayModeDefaults.cpp`
  - `Medium` stays on legacy `BP_Hero`
  - No debug/rollout-specific gameplay mode
  - Modular path is selected by `?CharacterSystem=Modular?CharacterDefinition=Hero` or `project.character.switch modular`
  - `DefaultPawnClass = BP_Hero` remains the legacy fallback if modular definition spawn fails

- [x] 6. Update Build.cs + .uplugin dependencies (DIP-clean)
  - `ProjectCharacter.Build.cs` -- private dep on `ProjectCore` only
  - `ProjectSinglePlay.Build.cs` -- private dep on `ProjectObject` (composition root)
  - `ProjectCharacter.uplugin` -- `ProjectCore` only (no ProjectObject/ProjectSkeletalAssembly)
  - `ProjectSinglePlay.uplugin` -- add `ProjectObject`

- [ ] 7. Build verification (blocked while editor keeps Live Coding active)
- [ ] 8. PIE test (manual)
  - Medium mode still spawns legacy `BP_Hero`
  - `project.character.switch modular` respawns into `ADefinitionCharacter`
  - `project.character.switch legacy` returns to `BP_Hero`
  - Optional bootstrap path `?CharacterSystem=Modular?CharacterDefinition=Hero` also spawns `ADefinitionCharacter`
  - Assembly lifecycle runs (Idle -> Assembling -> Ready)
  - Camera configured from view section
  - GAS works, input works
  - Legacy fallback works if modular definition spawn fails
  - Existing objects (doors, items) unaffected

## Key Files

| File | Action |
|------|--------|
| `ProjectCharacter/Public/DefinitionCharacter.h` | Create |
| `ProjectCharacter/Private/DefinitionCharacter.cpp` | Create |
| `ProjectSinglePlay/Public/SinglePlayerGameMode.h` | Add character-system selector API |
| `ProjectSinglePlay/Private/SinglePlayerGameMode.cpp` | Override spawn |
| `ProjectSinglePlay/Private/SinglePlayModeDefaults.cpp` | Keep gameplay modes clean |
| `ProjectCharacter/ProjectCharacter.Build.cs` | Add dep |
| `ProjectSinglePlay/ProjectSinglePlay.Build.cs` | Add dep |
| `ProjectObject/Content/Human/Hero/Hero.json` | Update spawnClass |

## Follow-up (non-blocking)

- [x] Split view-config out of `IAssemblyCapability`
  - `IAssemblyCapability` = lifecycle only (state, delegate, managed capabilities)
  - `IAssemblyViewConfigSource` = construction-time data (view config)
  - `USkeletalAssemblyComponent` implements both

## Prerequisites (DONE)

- [x] FRegisteredClassScan kernel in ProjectCore
- [x] FCapabilityRegistry with RegisterCapabilityModule
- [x] USkeletalAssemblyComponent with lifecycle + GetPrimaryAssetId
- [x] IAssemblyCapability interface in ProjectCore
- [x] FObjectMeshEntry extended (Kind, Role, Visibility)
- [x] New sections (Animation, Customization, View) in ObjectDefinition + parser + schema
- [x] ObjectSpawnUtility handles Kind/Role/Visibility + assembly ordering + deferred activation
- [x] Hero.json created and generates successfully
