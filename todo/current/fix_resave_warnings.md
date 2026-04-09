# Fix: Resave Assets With Empty Engine Version

## WARNING: Merge All Branches First

Resaving touches binary .uasset/.umap files. If coworkers have the same assets
modified on their branches (e.g. City17 scene work), resaving on one branch
creates merge conflicts on every touched file.

**Do NOT run until all active branches are merged to main and no one is
working in the affected scenes.**

## Problem

336 warnings in Alis.log:
```
LogLinker: Warning: [AssetLog] ...: Asset has been saved with empty engine version.
The asset will be loaded but may be incompatible.
```

Cause: assets were last saved with an engine build that did not stamp
FEngineVersion into the package header (old UE version or source build
without Build.version populated).

## Affected Areas

| Area | Count | Disk Path |
|------|-------|-----------|
| City17 WP (__ExternalActors__, __ExternalObjects__) | ~260 | Plugins/World/City17/Content/ |
| City17 maps, HLOD, DataLayers | ~8 | Plugins/World/City17/Content/Maps/, DataLayers/ |
| Plugins/Resources (Audio, Material, Object) | ~35 | Plugins/Resources/Project*/Content/ |
| Content/Project (core, audio, NPC, placeables, FP) | ~23 | Content/Project/ |
| Content/InventorySystem (legacy BPs/enums) | ~9 | Content/InventorySystem/ |
| Content/M_S_House | 1 | Content/M_S_House/ |

## Commandlet (close editor first)

```powershell
& "<ue-path>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "<project-root>\Alis.uproject" `
  -run=ResavePackages `
  -PACKAGEFOLDER="<project-root>\Plugins\World\City17\Content" `
  -PACKAGEFOLDER="<project-root>\Content\Project" `
  -PACKAGEFOLDER="<project-root>\Content\InventorySystem" `
  -PACKAGEFOLDER="<project-root>\Content\M_S_House" `
  -PACKAGEFOLDER="<project-root>\Plugins\Resources\ProjectAudio\Content" `
  -PACKAGEFOLDER="<project-root>\Plugins\Resources\ProjectMaterial\Content" `
  -PACKAGEFOLDER="<project-root>\Plugins\Resources\ProjectObject\Content" `
  -unattended -nosplash
```

Note: `-PACKAGEFOLDER` takes a **disk path**, not a UE content path.
Source: `BaseIteratePackagesCommandlet.cpp` -> `FPackageName::FindPackagesInDirectory(RootDir)`.

## Steps

- [ ] Merge all coworker branches to main
- [ ] Confirm no one is working in City17 or affected content folders
- [ ] Close editor
- [ ] Run commandlet above
- [ ] Reopen editor, verify warnings are gone in Alis.log
- [ ] Commit resaved assets (separate commit, binary-only)
- [ ] Consider: delete dead content (Content/InventorySystem, Content/Project/NPC/Dialogue) if no longer referenced

## Notes

- Content/InventorySystem and Content/Project/NPC/Dialogue look like legacy assets - verify references before resaving vs deleting
- City17 __ExternalActors__/__ExternalObjects__ are WP-generated, they inherit version from parent map
- Resaving stamps current engine version into package header, no gameplay changes
