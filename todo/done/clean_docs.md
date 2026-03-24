# Documentation Cleanup Plan (Prioritized)

## Phase 1: High Redundancy & Confusion (P0)
*Fix immediate "multiple sources of truth" issues.*

- [x] **Systems: Loading Pipeline**
    - [x] **Merge** `docs/systems/loading_pipeline.md` (detailed) INTO `Plugins/Systems/ProjectLoading/README.md`.
    - [x] **Reduce** `docs/systems/loading_pipeline.md` to a high-level router (Architecture overview only).
    - [x] **Delete** `docs/systems/loading_pipeline.md` (Legacy Router) - *Updated: Removed entirely as per user feedback.*

- [x] **Systems: Animation & Motion Redundancy**
    - [x] **Consolidate** "Usage" & "GAS Integration" code examples.
        - *Current*: Repeated in `docs/systems/animation.md`, `ProjectAnimation/README.md`, AND `ProjectMotionSystem/README.md`.
        - *Target*: Move full code example to `Plugins/Gameplay/ProjectCharacter/README.md` (or similar) and link.
    - [x] **Strip** folder structures from `docs/systems/animation.md`. (Maintenance burden).
    - [x] **Move** `docs/systems/animation.md` architecture diagrams/tables to `Plugins/Resources/ProjectAnimation/docs/architecture.md` (or similar).

## Phase 2: Wrong Location (P1)
*Move files that are good but in the wrong place.*

- [x] **Gameplay: Character**
    - [x] Move `docs/gameplay/character.md` -> `Plugins/Gameplay/ProjectCharacter/docs/design.md`.
    - [x] Link it from `ProjectCharacter/README.md`.

- [x] **Systems: World Partition**
    - [x] Move `docs/systems/world_partition.md` -> `Plugins/World/ProjectWorld/docs/world_partition.md`.
    - [x] Update `Plugins/World/ProjectWorld/README.md` to link to it.

- [x] **Tools**
    - [x] Move `docs/tools/build_service/*` -> `tools/BuildService/README.md` (and `docs/`).
    - [x] Move `docs/tools/launcher/*` -> `tools/Launcher/README.md` (and `docs/`).

## Phase 3: Metadata & SOT (P1)
*Fix specific README inaccuracies.*

- [x] **World: ProjectWorld README**
    - [x] Fix tier description: "Systems-tier" -> "World-tier".
- [x] **Resources: ProjectItems SOT**
    - [x] Update `ProjectItems/README.md` to point SOT to `Plugins/Resources/ProjectItems/SOT/` (Plugin-contained).
        - *Note*: `ProjectInventory/README.md` alignment still pending check.
    - [x] **Resolved**: `docs/data/specs.md` and `docs/data/workflow.md` updated to officially mandate `Data/` folders (Plugin Root), superseding legacy "Content/Data" instructions.
    - [x] **Fix**: Migrated `ProjectVitalsUI` and `ProjectHUD` data to `Data/` (found missing during initial pass).

## Phase 4: Router Cleanup (P2)
- [x] Create/Update `docs/systems/README.md` to list all systems and point to pugins.
- [x] Update `docs/gameplay/README.md` to list all gameplay modules.
- [x] Verify `docs/README.md` links are all valid.
- [x] **Bonus**: Created routers for `testing/`, `guides/`, `reference/`, `agents/`, `config/` to strictly enforce "README = Router" rule.

## Phase 5: Code Path Verification (New)
*Verify all code paths comply with the new `Plugin/Data` structure.*

- [x] **Scan Codebase**: Grep for `Content/Data` in C++/C# files to find hardcoded legacy paths.
- [x] **Verify Loaders**: Check `ProjectWidgetLayoutLoader`, `ProjectUIRegistrySubsystem`, `SinglePlayModeRegistry`, `ItemDefinitionGenerator` for correct usage of `FProjectPaths`.
- [x] **Verify Staging**: Ensure all `Build.cs` files stage `Data/` and not `Content/Data`.
