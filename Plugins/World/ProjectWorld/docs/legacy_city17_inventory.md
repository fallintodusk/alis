# Legacy City17 Inventory (read-only, Slice 1 record)

Read-only inventory of the City17 legacy map plugin. The file pass was taken
2026-08-06 and the live editor pass was completed 2026-08-12. No asset was
modified or saved. The full 2,814-actor record is transient evidence at
`Saved/Validation/World/legacy_city17_editor_inventory.json`; this SOT keeps
only migration-relevant facts. Migration decisions remain owned by the
territory milestone's content-integration slice.

## Layout and size

- Plugin: `Plugins/World/City17/` - one runtime module (`City17`,
  deps Core/CoreUObject/Engine/ProjectCore), `.uplugin` depends on
  ProjectSinglePlay and ProjectOnlinePlay, `EnabledByDefault: false`.
- Tracked content: 29,158,749 bytes (27.8 MiB), 2,888 files.
- One map: `Content/Maps/City17_Persistent_WP.umap` (World Partition,
  `WorldPartitionRuntimeHashSet`). The live object tree contains two
  `RuntimePartitionLHGrid` objects and one persistent partition. The serialized
  editor spatial hash cell size is 12,800 cm; the exact runtime loading ranges
  are legacy inputs, not defaults for ProjectWorld. No sublevels or streaming
  level maps exist.
- External actors: 2,825 files; external objects: 51 files.
- The original name-table census found 2,528 StaticMeshActor and 203
  ObjectDefinition records. The authoritative loaded-class census below
  resolves those serialized identities to the current 2,511 StaticMeshActor,
  202 InteractableActor, and one DefinitionCharacter classes.
- HLOD: `_Merged` and `_Instanced` layer definitions are hard/management
  dependencies, but there are no generated HLOD proxy assets or loaded HLOD
  actors. They are configuration, not reusable generated geometry.
- Data Layers: 7 flat assets exist under `Content/DataLayers/`, but the live
  world reports zero active Data Layers and all 2,814 loaded actors have empty
  membership. The WP convert ini also points at a nonexistent nested folder.
  Treat these assets as unintegrated legacy data, not a layer contract.
- BuiltData: the 2.75 MB `City17_Persistent_WP_BuiltData.uasset` is a hard map
  dependency. A live Map Check completed with 0 errors and 0 warnings, including
  no lighting-rebuild warning; this is operational evidence, not provenance.

## Gameplay and config coupling (inbound)

The file/string sweep and live Asset Registry pass found executable coupling
only in the UI tier plus root config. No package outside `/City17/` refers to
the map. Its 2,876 package referencers are its own external actors/objects, so
there is no hidden binary consumer that blocks later map retirement.

- `Alis.uproject`: plugin enabled.
- `Config/DefaultEngine.ini`: `EditorStartupMap=/City17/Maps/...`.
- `Config/DefaultGame.ini`: MapsToCook entry, Map scan path, ChunkId=10
  primary asset rule.
- `W_MainMenu.cpp`: `RequestStartGame(TEXT("City17"), TEXT("SinglePlayer"))`.
- `ProjectMapListViewModel.cpp`: hardcoded STALE soft path
  `/Game/Maps/City17/City17_Persistent.City17_Persistent` (asset moved).
- `MainMenu.json`: `"action": "LoadCity17"` with NO handler anywhere.
- Zero City17 strings in `Source/`, `Plugins/Features/*`, or any
  Gameplay-tier C++ (docs prose only).
- Doc/code contradiction: City17 README claims no experience descriptor
  registration; `City17Module.cpp` registers `UCity17ExperienceDescriptor`
  (map path `/City17/Maps/City17_Persistent_WP`).
- `scripts/ue/cinematic/_drive_mrq_render.py` pins the City17 map path.
- The Level Blueprint contains only disconnected, disabled default BeginPlay
  and Tick nodes. It owns no gameplay logic.

## Outbound content dependencies (resolved editor closure)

The live transitive Asset Registry closure resolves 1,104 external content
packages. Project-owned resolved bytes total 789.73 MiB: ProjectObject owns
1,001 packages/726.32 MiB, `/Game` owns 59/56.02 MiB, and MotionMatching owns
2/6.19 MiB. The remaining 38 project packages total about 1.2 MiB across
ProjectElement, ProjectMaterial, ProjectMesh, ProjectAudio, ProjectTexture,
and ProjectSkeletalCapabilities. Three DatasmithContent and one SunPosition
packages are engine/plugin content and are excluded from the project byte sum.

The City17 plugin itself owns 2,888 content files/27.81 MiB. The map directly
owns all of its external actors/objects and all City17 map packages except the
seven inactive Data Layer assets. It therefore has a small exclusive shell but
a large dependency on shared ProjectObject and `/Game` art; retained art must
move by explicit asset provenance, never by copying the map package.

## Licensing flags for kit reuse

`AdvancedGlassPack`, `TextureNoiseAndPatternPack01`, and MetaHuman
content carry third-party licenses; verify redistribution terms before
any kit is fed into the procedural polish route.

## Live world facts and migration boundary

- Loaded actor census: 2,814 actors in 31 classes. The dominant presentation
  content is 2,511 StaticMeshActors. Gameplay-bearing content is explicit:
  202 `InteractableActor`, one `DefinitionCharacter` (`GrandPa`), one
  untagged `PlayerStart`, and the registered SinglePlayer GameMode.
- WorldSettings: `/Script/ProjectSinglePlay.SinglePlayerGameMode`, KillZ
  -1,048,575 cm, world-bounds checks enabled, and a
  `NavigationSystemModuleConfig`. There are zero NavMeshBoundsVolumes, zero
  RecastNavMesh actors, and zero nav links; City17 owns no usable nav coverage.
- PlayerStart: location `[-4990, 12090, 270]` cm, yaw `-40` degrees,
  no tag, no Data Layer, and always loaded.
- Foliage: 7 InstancedFoliageActors with 3,073 resolved instances. Per-carrier
  counts are 465, 657, 2, 1,486, 454, 7, and 2. This is legacy baked placement,
  not a generation profile.
- Landscape: one actor, 49 LandscapeComponents, 49 collision components,
  229 spline-mesh components, and 3 grass ISM components. Bounds are about
  441 x 504 m; the material comes from ProjectObject.
- No actor or landmark carries a geodetic anchor. PlayerStart and GrandPa are
  only local transforms, and no actor label identifies a Kazan hero landmark.
  Any retained gameplay/art placement must be re-authored against the
  ProjectWorld coordinate contract; City17 transforms are not 1:1 evidence.

This closes the Slice 1 read-only inventory. City17 stays supported as the
legacy menu experience, but contributes no territory authority, runtime-grid
defaults, generated layers, navigation coverage, or coordinate truth.
