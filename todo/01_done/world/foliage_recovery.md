# Foliage Recovery After UE 5.5→5.7 Upgrade

## Problems

1. **KazanMain.umap deleted** (commit d2caf2010, Dec 30 2025)
2. **Git LFS silent merge conflict** - `merge=lfs` discards local changes
3. **Broken asset paths** - Nature assets moved to `Plugins/Resources/ProjectObject/Content/Nature/`
4. **Foliage materials deleted** - `MSPresets/.../Foliage/*.uasset` removed in merge
5. **Version serialization** - UE 5.7 format incompatibility

## Solutions

### Restore Map from Git History
```powershell
# Find last good commit (before Dec 30, 2025)
git log --oneline Artur --before="2025-12-30" -- "Content/Project/Maps/KazanMain/*.umap"

# Restore map
git checkout <good-commit> -- Content/Project/Maps/KazanMain/KazanMain.umap
git checkout <good-commit> -- Content/Project/Maps/KazanMain/Streaming/*.umap

# Restore foliage materials
git checkout 68a85f276 -- Content/MSPresets/MS_Foliage_Material_LATEST/
git checkout 68a85f276 -- Content/Custom/CustomMaterials/.../M_MS_Foliage_Overwrite/
```

### Fix Asset References
1. Open map in UE Editor
2. Tools → Redirect References
3. Remap: `Content/MSPresets/Nature/` → `Plugins/Resources/ProjectObject/Content/Nature/`

## Prevention

### 1. Fix .gitattributes
```gitattributes
# Change from:
*.umap filter=lfs merge=lfs diff=lfs -text

# To:
*.umap filter=lfs merge=binary diff=lfs -text lockable
```

### 2. Enable Git LFS Locking
```powershell
# Before editing maps
git lfs lock Content/Project/Maps/KazanMain/KazanMain.umap

# After commit
git lfs unlock Content/Project/Maps/KazanMain/KazanMain.umap
```

### 3. Pre-Merge Validation Script
Create `scripts/git/pre-merge-check.ps1`:
```powershell
$conflicts = git diff --name-only --diff-filter=D origin/dev...HEAD | Select-String "\.umap"
if ($conflicts) {
    Write-Warning "Binary files will be deleted: $conflicts"
    Read-Host "Continue? (yes/no)"
}
```

### 4. Asset Move Protocol
- Coordinate in team chat before moving assets
- Use UE Editor "Fix Up Redirectors" after moves
- Commit redirector files

### 5. Branch Strategy
- Feature branches for map editing
- Use `git lfs lock` for all .umap files
- Merge to dev only after testing

## Action Checklist
- [ ] Find last good commit with KazanMain.umap
- [ ] Restore map from git (before Dec 30, 2025)
- [ ] Restore foliage materials from commit 68a85f276
- [ ] Fix asset path redirectors (Content → Plugins/Resources)
- [ ] Update .gitattributes merge strategy
- [ ] Setup git lfs lock workflow
- [ ] Create pre-merge validation script
- [ ] Document asset move protocol
- [ ] Test map in UE 5.7 Editor
- [ ] Commit: "fix: restore foliage and prevent merge data loss"
