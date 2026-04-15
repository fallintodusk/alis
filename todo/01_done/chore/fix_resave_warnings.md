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

**CRITICAL:** Use `-OnlyUnversioned` flag to only resave the ~336 packages with empty
engine version. Without it, ResavePackages resaves ALL packages in the folder (~5300+
files), creating a massive git diff for no reason.

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
  -OnlyUnversioned `
  -unattended -nosplash `
  2>&1 | Tee-Object -FilePath "Saved\Logs\resave_run.log"
```

Notes:
- `-PACKAGEFOLDER` takes a **disk path**, not a UE content path
- `-OnlyUnversioned` skips packages where `SavedByEngineVersion.GetChangelist() != 0`
- Source: `BaseIteratePackagesCommandlet.cpp` lines 1061, 1323
- Multiple `-PACKAGEFOLDER` flags supported (parsed in loop, line 105)
- Editor MUST be closed (Error Code 32 = sharing violation if editor locks files)

## Steps

- [ ] Merge all coworker branches to main
- [ ] Confirm no one is working in City17 or affected content folders
- [ ] Close editor (taskkill UnrealEditor.exe if needed)
- [ ] Run commandlet above (with -OnlyUnversioned)
- [ ] Verify resave count is ~336, not thousands (check resave_run.log)
- [ ] Reopen editor, verify warnings are gone in Alis.log
- [ ] Commit resaved assets (separate commit, binary-only)
- [ ] Consider: delete dead content (Content/InventorySystem, Content/Project/NPC/Dialogue) if no longer referenced

## Notes

- Content/InventorySystem and Content/Project/NPC/Dialogue look like legacy assets - verify references before resaving vs deleting
- City17 __ExternalActors__/__ExternalObjects__ are WP-generated, they inherit version from parent map
- Resaving stamps current engine version into package header, no gameplay changes

## Lessons Learned (2026-04-15)

1. **Without -OnlyUnversioned**: commandlet resaved ALL 5291+ packages in specified folders,
   not just the ~336 with empty version. Created 6968 modified files in git.
2. **Editor must be closed**: Error Code 32 (sharing violation) on every file if editor is open.
   The commandlet loads/resaves but cannot move files to temp -> all saves fail silently.
3. **Log encoding**: Tee-Object produces UTF-16LE log. Use `iconv -f UTF-16LE -t UTF-8` to parse.
4. **Discarded run**: First full run was discarded with `git checkout -- .` (clean state confirmed).
