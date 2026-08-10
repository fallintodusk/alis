# Legacy City17 Inventory (read-only, Slice 1 record)

File-level inventory of the City17 legacy map plugin, taken 2026-08-06
before any migration decision. Nothing was modified. Deep asset-registry
reference analysis still requires an editor commandlet pass (see final
section). Migration decisions are owned by the territory milestone's
content-integration slice, not by this record.

## Layout and size

- Plugin: `Plugins/World/City17/` - one runtime module (`City17`,
  deps Core/CoreUObject/Engine/ProjectCore), `.uplugin` depends on
  ProjectSinglePlay and ProjectOnlinePlay, `EnabledByDefault: false`.
- Tracked content: 29,158,749 bytes (27.8 MiB), 2,888 files.
- One map: `Content/Maps/City17_Persistent_WP.umap` (World Partition,
  `WorldPartitionRuntimeHashSet`; editor spatial hash CellSize=12800).
  No sublevels or streaming-level umaps.
- External actors: 2,825 files; external objects: 51 files.
- Actor-class census (string scan): 2,528 StaticMeshActor,
  203 ObjectDefinition, 7 InstancedFoliageActor, 1 PlayerStart,
  1 Landscape, 1 each of the sky/light/fog/postprocess set.
- HLOD: two layer assets exist (`_Merged`, `_Instanced`) despite the
  plugin TODO stating "No HLOD in ALIS" - reconcile at decision time.
- Data Layers: 7 assets in `Content/DataLayers/` (flat); the WP convert
  ini points at `/City17/DataLayers/City17_Persistent_WP/` which does
  NOT exist - config/layout mismatch.
- BuiltData: 2.75 MB `City17_Persistent_WP_BuiltData.uasset` of unknown
  freshness.

## Gameplay and config coupling (inbound)

The file and string sweep found executable coupling only in the UI tier
plus root config. This does NOT establish confinement: binary assets
outside the plugin, Level Blueprint logic, and map-object references can
only be ruled out by the editor pass listed at the end.

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

## Outbound content dependencies (string scan of name tables)

Counts are name-table occurrences, not resolved references:

- `/ProjectObject/`: 3,579 occurrences across 878 distinct packages -
  the heaviest cross-plugin coupling by far.
- `/Game/`: 75 distinct paths - AdvancedGlassPack (22), MetaHumans
  GrandPa + Common (22), Project decals/environment placeables,
  `/Game/Project/Core/Templates/Attacher` (660 occurrences),
  M_S_House materials, toys, noise textures.
- Smaller: `/ProjectMesh/` 89, `/ProjectMaterial/` 63,
  `/ProjectElement/` 29, `/ProjectAudio/` 7, `/MotionMatching/` 4.
- Script imports include ProjectObject, ProjectMotionSystem,
  ProjectObjectCapabilities, ProjectSinglePlay, ProjectDialogue,
  ProjectVitals, NavigationSystem, Foliage.

## Licensing flags for kit reuse

`AdvancedGlassPack`, `TextureNoiseAndPatternPack01`, and MetaHuman
content carry third-party licenses; verify redistribution terms before
any kit is fed into the procedural polish route.

## Requires editor pass (not resolvable from files)

1. True hard/soft dependency graph (AssetRegistry query, both directions;
   especially reverse referencers of `/City17/Maps/City17_Persistent_WP`).
2. Per-actor identity, transforms, and Data Layer membership (external
   actor files are content-hash GUID names).
3. WorldSettings exact GameMode binding, KillZ, navigation config.
4. The single PlayerStart's location and tag.
5. Runtime WP grid parameters (README claims 256 m cells, unverified).
6. HLOD reality (generated proxies or dead assets).
7. Foliage instance counts in the 7 InstancedFoliageActor files
   (foliage-carrier cells 0/61, 0/SZ, 8/3H, 9/4D, A/JP).
8. Landscape component/layer layout.
9. BuiltData staleness.
