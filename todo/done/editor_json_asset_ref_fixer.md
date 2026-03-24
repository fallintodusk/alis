# Editor Plugin: Auto-Update JSON on Asset Move/Rename

Editor tool to automatically update JSON asset references when assets are moved/renamed.

**Context:** See [items_objects_relationship_investigate.md](items_objects_relationship_investigate.md) for asset reference strategy.

**Problem:** When an asset is moved/renamed in the Editor, JSON files that reference it by full path become stale.

**Solution:** Editor plugin that listens to asset rename events and auto-updates JSON files.

---

## Phase 1: Build Index (Cached)

Scan JSON roots, parse, collect asset ref strings, build map:

```
OldObjectPath -> [ { JsonFile, JsonKey } ... ]
```

Cache to `Saved/ProjectCache/JsonAssetRefIndex.bin` with per-file mtime/hash.

---

## Phase 2: Listen to Asset Rename/Move

```cpp
// In Editor module StartupModule()
FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
AssetRegistry.Get().OnAssetRenamed().AddRaw(this, &FMyModule::OnAssetRenamed);

void FMyModule::OnAssetRenamed(const FAssetData& Asset, const FString& OldObjectPath)
{
    // 1. Find all JSON entries referencing OldObjectPath
    // 2. Rewrite JSON files with new path
    // 3. Update index
}
```

---

## Phase 3: Validate/Fix Command

```bash
UnrealEditor-Cmd.exe Project.uproject -run=ValidateJsonAssetRefs -nopause
```

- **Validate:** Check all JSON refs exist in AssetRegistry
- **Fix:** Resolve via redirectors if possible

---

## CI Commandlet Validator

`UProjectValidateJsonAssetRefsCommandlet`:

1. Read JSON files
2. For each full path string, check `AssetRegistry.GetAssetByObjectPath()`
3. If missing -> fail build, print: JSON file, key, missing ref

Optional `-fix` mode attempts redirector resolution.

---

## JSON Reference Styles

| Asset Location | JSON Reference Style |
|----------------|---------------------|
| Same folder as JSON | Short name: `"SM_Door"` |
| Cross-plugin | Full path: `"/OtherPlugin/.../SM_X.SM_X"` |

---

## Why This Works

- Most renames/moves happen in Editor - caught immediately
- JSON auto-updated at source - no drift
- No UUIDs or catalogs needed
- CI catches `git mv` or "Fix Up Redirectors" breakage

---

## Implementation Tasks

- [ ] Create `FJsonAssetRefIndex` class for index building/caching
- [ ] Hook into `OnAssetRenamed()` event
- [ ] Implement JSON rewrite with backup/undo support
- [ ] Create `UProjectValidateJsonAssetRefsCommandlet`
- [ ] Add menu entry: Tools -> Validate JSON Asset References
- [ ] Add CI integration docs
