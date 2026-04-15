# Content Structure Issues & Recommendations

This document identifies organizational problems in the `Content/` folder and provides recommendations for cleanup and restructuring.

**Created:** 2025-10-16
**Status:** [!] Needs Action

---

## Critical Issues

### 1. [!] Duplicate Asset Locations

**Problem:** Assets exist in multiple locations with unclear ownership.

#### Glass Pack Duplication
```
Content/AdvancedGlassPack/          <- Original plugin location
Content/Packs/AdvancedGlassPack/    <- Duplicate/partial copy
```

**Impact:** Confusion about which version is authoritative, wasted disk space

**Recommendation:**
- Keep ONE location: `Content/AdvancedGlassPack/` (standard plugin location)
- Delete `Content/Packs/AdvancedGlassPack/` entirely
- Update any references to point to original location
- Add migration guide if needed

---

#### Character Asset Duplication
```
Content/MaleActionHeroes/           <- One location
Content/Personage/                  <- Duplicate/extended version
```

**Analysis:**
- Both contain `MaleHeroParts/` and `Mannequin/` folders
- `Personage/` has additional content (ThirdPerson, Animations)
- Unclear which is source of truth

**Recommendation:**
- **Decision needed:** Is `Personage/` the evolved version?
- If YES: Delete `MaleActionHeroes/`, use only `Personage/`
- If NO: Move unique content from `Personage/` to `MaleActionHeroes/`, delete `Personage/`
- Document the decision in CLAUDE.md

---

### 2. [!] Inconsistent Top-Level Organization

**Problem:** Mixed organizational paradigms at Content root level.

```
Content/
|-- Project/                    <- Game content (GOOD - centralized)
|-- InventorySystem/            <- Plugin (GOOD - standard location)
|-- AdvancedGlassPack/          <- Marketplace pack (GOOD)
|-- Custom/                     <- ??? Custom what? Vague name
|-- Materials/                  <- Shared? Project? Unclear ownership
|-- Widgets/                    <- Duplicates Project/UI/ content
|-- Packs/                      <- Redundant with other locations
|-- TEST_MAT/                   <- Temporary test folder (should be in Project/Test)
`-- Collections/                <- Empty or minimal content?
```

**Impact:** Developers don't know where to put new assets

**Recommendation:**

#### Consolidate Widgets
```
Current:
  Content/Widgets/Dialogue/
  Content/Widgets/GameHUD/
  Content/Widgets/Menu/

Move to:
  Content/Project/UI/Dialogue/
  Content/Project/UI/GameHUD/
  Content/Project/UI/Menu/
```

#### Consolidate Materials
```
Current:
  Content/Materials/Road_AsphaultRaceWayLines/

Move to:
  Content/Project/Resources/Materials/Road_AsphaultRaceWayLines/
```

#### Consolidate Custom
```
Current:
  Content/Custom/CustomMaterials/

Move to:
  Content/Project/Resources/Materials/CustomMaterials/
```

#### Move Test Content
```
Current:
  Content/TEST_MAT/

Move to:
  Content/Project/Test/Vehicles/
```

#### Delete or Consolidate Packs/
- If truly needed, rename to `Content/_MarketplacePacks/` (underscore prefix for sorting)
- Otherwise, delete and use original plugin locations

---

### 3. [~] "ToDo" Folders Everywhere

**Problem:** Multiple `ToDo/` folders indicate incomplete organization.

**Locations:**
- `Content/Project/Resources/Objects/Nature/ToDo/Old_Plant/`
- `Content/Project/Resources/Objects/ToDo/Characters/`
- `Content/Project/Resources/Objects/HumanMade/Transport/Land/ToDo/`
- `Content/Project/Resources/Texture/ToDo/`

**Impact:** Unclear if assets are production-ready, abandoned, or in-progress

**Recommendation:**
1. **Audit each ToDo folder:**
   - If assets are used: Move to proper category
   - If deprecated: Delete
   - If WIP: Add date and owner to folder name (e.g., `ToDo_2025_Diana/`)

2. **Create decision log:**
   ```
   todo/content_cleanup_log.md
   - Record what was moved/deleted
   - Document why decisions were made
   ```

3. **Set policy:** No unnamed `ToDo/` folders. Use `WIP_[Feature]_[Date]/` instead

---

### 4. [~] Naming Inconsistencies

**Problem:** Mixed naming conventions break searchability.

#### Cyrillic Character in Folder Name
```
Content/Project/Resources/Objects/HumanMade/WorkTool/Сutting_Sawing/
                                                      ^ Cyrillic 'C' (U+0421)
```

**Impact:**
- Breaks search/grep for "Cutting"
- Path issues on some systems
- Copy/paste errors

**Recommendation:**
- Rename to `Cutting_Sawing/` (Latin C)
- Update all references
- Add pre-commit hook to prevent non-ASCII folder names

---

#### Inconsistent Separators
```
WorkTool/Fasteners_Installation/     <- Underscore
WorkTool/EarthmovingTool/            <- CamelCase
WorkTool/Communication_Gear/         <- Underscore
```

**Recommendation:**
- Standardize on **PascalCase** for folder names (matches UE conventions)
- Or standardize on **Underscore_Separation**
- Document chosen convention in `docs/content_structure.md`

---

#### Typos
```
Content/AdvancedGlassPack/Materials/03_Frozen/_Exmaples/
                                                ^^ Typo: "Examples"
```

**Recommendation:** Rename to `_Examples/` for consistency

---

### 5. [~] Deep Nesting Issues

**Problem:** Some object hierarchies are 8+ levels deep.

**Example:**
```
Content/Project/Resources/Objects/HumanMade/WorkTool/CarpentryTool/Axe/Axe_1/
         1      2         3       4         5        6             7    8
```

**Impact:**
- Windows MAX_PATH limit (260 characters) issues
- Difficult navigation in Content Browser
- Overly specific categorization

**Recommendation:**
1. **Flatten tool categories:**
   ```
   Current:
     Objects/HumanMade/WorkTool/CarpentryTool/Axe/Axe_1/

   Better:
     Objects/Tools/Axe/Axe_1/
     Objects/Tools/Hammer/Hammer_1/
     Objects/Tools/Shovel/Shovel_1/
   ```

2. **Use asset metadata instead of folders:**
   - Add `Category` metadata field
   - Use Content Browser filters
   - Reduces folder depth

---

### 6. [~] Orphaned Demo/Example Content

**Problem:** Demo content from marketplace plugins still present.

**Locations:**
- `InventorySystem/Blueprints/Demo/Example_01/`
- `InventorySystem/Blueprints/Demo/Example_02/`
- `AdvancedGlassPack/Materials/*//_Examples/`
- `InventorySystem/Maps/` (demo maps)

**Impact:**
- Bloats packaged builds
- Confusion about what's production vs example
- Disk space waste

**Recommendation:**
1. **Keep examples for reference:**
   - Move to `Content/_PluginExamples/` folder
   - Exclude from packaging in Project Settings

2. **Or delete entirely:**
   - If team doesn't reference them
   - Can reinstall plugin if needed

---

### 7. [.] Good Patterns to Preserve

**What's working well:**

[OK] **Project/ folder centralization**
- Single source of truth for game content
- Clear separation from marketplace assets

[OK] **Consistent asset type folders**
- Blueprints/, Materials/, Meshes/, Textures/ pattern
- Easy to find related assets

[OK] **Semantic categorization in Resources/Objects/**
- Nature vs HumanMade distinction
- VitalNeed, WorkTool, Furniture categories make sense

[OK] **UI BaseClass pattern**
- `Project/UI/CustomUI/Widgets/*/BaseClass/` inheritance
- Promotes reusability

[OK] **Localization structure**
- Standard UE localization folder
- Proper en-US and ru separation

---

## Recommended Action Plan

### Phase 1: Critical Cleanup (High Priority)

**Week 1:**
- MANUAL: Delete duplicate `Content/Packs/AdvancedGlassPack/`
- MANUAL: Decide on MaleActionHeroes vs Personage, consolidate
- MANUAL: Fix Cyrillic folder name: `Сutting_Sawing` -> `Cutting_Sawing`
- MANUAL: Fix typo: `_Exmaples` -> `_Examples`

**Week 2:**
- MANUAL: Move `Content/TEST_MAT/` -> `Content/Project/Test/Vehicles/`
- MANUAL: Move `Content/Widgets/` -> `Content/Project/UI/`
- MANUAL: Move `Content/Materials/` -> `Content/Project/Resources/Materials/`
- MANUAL: Move `Content/Custom/` -> `Content/Project/Resources/Materials/CustomMaterials/`

---

### Phase 2: Organization Improvements (Medium Priority)

**Week 3:**
- MANUAL: Audit all `ToDo/` folders, move or delete content
- [ ] Document decisions in `todo/content_cleanup_log.md`
- MANUAL: Standardize folder naming convention (choose PascalCase or Underscore_Case)

**Week 4:**
- MANUAL: Move plugin demo content to `_PluginExamples/` or delete
- MANUAL: Exclude examples from packaging
- MANUAL: Update Project Settings cooking filters

---

### Phase 3: Structural Refactoring (Low Priority)

**Future work:**
- MANUAL: Flatten deep tool hierarchies (8+ levels -> 4-5 levels)
- MANUAL: Evaluate if `Collections/` folder is needed
- MANUAL: Consider creating `Content/_Archive/` for old assets
- MANUAL: Add metadata tagging system for better search

---

## Migration Safety

### Before Making Changes:

1. **Create git branch:**
   ```bash
   git checkout -b content-structure-cleanup
   ```

2. **Backup Content folder:**
   ```bash
   # Create zip backup
   7z a Content_Backup_2025-10-16.7z Content/
   ```

3. **Test in isolated environment:**
   - Copy project to separate folder
   - Test changes there first
   - Verify no broken references

4. **Use UE Redirectors:**
   - When moving assets, UE creates redirectors automatically
   - **Don't delete redirectors immediately**
   - Let team update references first
   - Run "Fix Up Redirectors" after 1-2 weeks

5. **Communication:**
   - Notify team before major reorganization
   - Use version control branch
   - Merge during low-activity period

---

## Reference Checker Tool

Before deleting any folder, use UE's Reference Viewer:

1. Right-click folder in Content Browser
2. Select "Reference Viewer"
3. Check for external references
4. Document dependencies before moving

**Command-line check:**
```bash
# Find all references to an asset in .uasset files
grep -r "AdvancedGlassPack" Content/ --include="*.uasset"
```

---

## Maintenance Guidelines

### Prevent Future Issues:

1. **Enforce naming conventions:**
   - Add to CLAUDE.md
   - Code review checklist item
   - Use editor validators (UE5 Asset Validation)

2. **Regular audits:**
   - Monthly review of new Content/ additions
   - Quarterly cleanup of unused assets
   - Annual deep structure review

3. **Clear ownership:**
   - Each major Content/ subfolder has an owner
   - Document in `docs/content_ownership.md`

4. **Pre-commit checks:**
   - No folders with non-ASCII characters
   - No unnamed `ToDo/` folders
   - Max folder depth: 6 levels

---

## Open decisions

These need resolution before restructuring:

1. **Naming convention:** PascalCase vs Underscore_Separation for folders?
2. **Plugin examples:** Keep or delete marketplace demo content?
3. **Character assets:** Use MaleActionHeroes or Personage as primary?
4. **Tool categorization:** Keep deep hierarchy or flatten?

---

## Resources

- [UE5 Content Organization Best Practices](https://docs.unrealengine.com/5.0/en-US/recommended-asset-naming-conventions-in-unreal-engine-projects/)
- [Gamemakin UE4 Style Guide](https://github.com/Allar/ue5-style-guide)
- Project documentation: `docs/content_structure.md`

---

## Status Tracking

| Issue | Priority | Status | Assigned To | ETA |
|-------|----------|--------|-------------|-----|
| Duplicate AdvancedGlassPack | [!] Critical | [P] Planned | - | - |
| Character asset duplication | [!] Critical | [P] Planned | - | - |
| Cyrillic folder name | [!] Critical | [P] Planned | - | - |
| Move Widgets/ | [~] Medium | [P] Planned | - | - |
| Move Materials/ | [~] Medium | [P] Planned | - | - |
| Move Custom/ | [~] Medium | [P] Planned | - | - |
| Move TEST_MAT/ | [~] Medium | [P] Planned | - | - |
| Audit ToDo folders | [~] Medium | [P] Planned | - | - |
| Naming standardization | [~] Medium | [P] Planned | - | - |
| Plugin demos cleanup | [~] Medium | [P] Planned | - | - |
| Flatten tool hierarchy | [.] Low | [?] Proposed | - | - |

**Legend:**
- [!] Critical - Blocks development or causes errors
- [~] Medium - Causes confusion but not blocking
- [.] Low - Nice to have, improves quality of life
- [P] Planned - Approved, ready to implement
- [?] Proposed - Needs team discussion
- [OK] Complete - Done and tested

---

**Last Updated:** 2025-10-16
**Next Review:** 2025-10-30
