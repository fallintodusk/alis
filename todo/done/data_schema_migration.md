# Data Schema Migration Plan

Consolidate all JSON data files into `Content/Data/Schema/` for reliable packaging and plugin modularity.

---

## Problem Statement

**Current issues:**
1. `DirectoriesToAlwaysStageAsUFS` is deprecated/broken in UE5 (see [Epic Forums](https://forums.unrealengine.com/t/re-add-functionality-for-directoriestoalwaysstageasufs-nonufs-or-remove-the-variables/153507))
2. Each new UI plugin requires editing core `DefaultGame.ini`
3. Plugin updates require project core rebuild
4. JSON files scattered across plugin folders

**Target state:**
- Single `Content/Data/Schema/` folder (auto-staged by UE)
- No `DirectoriesToAlwaysStageAsUFS` entries needed
- Plugins remain modular for Build Service packaging
- Clear SOT (Source of Truth) location for designers
- Dynamic path resolution via `FProjectPaths` utility

---

## New Directory Structure

**Mirrors `Plugins/` folder structure exactly:**

```
Content/Data/Schema/
|-- UI/
|   |-- ProjectUI/
|   |   |-- ui_definitions.json
|   |   +-- LoadingScreen.json
|   |-- ProjectHUD/
|   |   +-- HUDLayout.json
|   |-- ProjectVitalsUI/
|   |   |-- ui_definitions.json
|   |   |-- VitalsPanel.json
|   |   +-- VitalsHUD.json
|   |-- ProjectMenuMain/
|   |   |-- ui_definitions.json
|   |   |-- MainMenu.json
|   |   |-- MapBrowser.json
|   |   +-- Settings.json
|   +-- ProjectSettingsUI/
|       +-- Settings.json
|-- Resources/
|   +-- ProjectItems/
|       |-- consumable/
|       |   +-- *.json
|       |-- currency/
|       |-- equipment/
|       |-- misc/
|       |-- quest/
|       +-- weapon/
|-- Boot/
|   +-- Orchestrator/
|       +-- dev_manifest.json
+-- Gameplay/
    +-- ProjectSinglePlay/
        +-- ModeOverrides.json
```

**Mapping rule:** `Plugins/<Category>/<PluginName>/` -> `Content/Data/Schema/<Category>/<PluginName>/`

---

## FProjectPaths Utility (CREATED)

**Location:** `Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/ProjectPaths.h`

**API:**
```cpp
class PROJECTCORE_API FProjectPaths final
{
public:
    // Base schema directory: Content/Data/Schema/
    static FString GetSchemaDir();

    // Plugin schema folder: Content/Data/Schema/<Category>/<PluginName>/
    static FString GetPluginSchemaDir(const FString& Category, const FString& PluginName);

    // Specific file: Content/Data/Schema/<Category>/<PluginName>/<RelativePath>
    static FString GetPluginSchemaFile(const FString& Category, const FString& PluginName, const FString& RelativePath);

    // Auto-resolve category from plugin name via IPluginManager
    static FString ResolvePluginSchemaFile(const FString& PluginName, const FString& RelativePath);

    // Extract category from plugin base directory
    static FString GetPluginCategory(const FString& PluginBaseDir);

    // Check if file exists
    static bool PluginSchemaFileExists(const FString& Category, const FString& PluginName, const FString& RelativePath);
};
```

**Usage examples:**
```cpp
// Direct path (when category is known)
FString Path = FProjectPaths::GetPluginSchemaFile(TEXT("UI"), TEXT("ProjectUI"), TEXT("LoadingScreen.json"));
// -> Content/Data/Schema/UI/ProjectUI/LoadingScreen.json

// Auto-resolve (looks up plugin category)
FString Path = FProjectPaths::ResolvePluginSchemaFile(TEXT("ProjectItems"), TEXT("consumable/food.json"));
// -> Content/Data/Schema/Resources/ProjectItems/consumable/food.json
```

---

## Migration Tasks

### Phase 1: Create Directory Structure

- [x] Create `FProjectPaths` utility class in ProjectCore
- [ ] Create `Content/Data/Schema/` folder structure mirroring Plugins

### Phase 2: Move JSON Files

**UI Plugin JSONs:**

| Source | Destination |
|--------|-------------|
| `Plugins/UI/ProjectUI/Config/UI/*.json` | `Content/Data/Schema/UI/ProjectUI/` |
| `Plugins/UI/ProjectHUD/Config/UI/*.json` | `Content/Data/Schema/UI/ProjectHUD/` |
| `Plugins/UI/ProjectVitalsUI/Config/UI/*.json` | `Content/Data/Schema/UI/ProjectVitalsUI/` |
| `Plugins/UI/ProjectMenuMain/Config/UI/*.json` | `Content/Data/Schema/UI/ProjectMenuMain/` |
| `Plugins/UI/ProjectSettingsUI/Config/UI/*.json` | `Content/Data/Schema/UI/ProjectSettingsUI/` |

**Items JSONs:**

| Source | Destination |
|--------|-------------|
| `Plugins/Resources/ProjectItems/SOT/Items/*` | `Content/Data/Schema/Resources/ProjectItems/` |

**Other JSONs:**

| Source | Destination |
|--------|-------------|
| `Config/Manifest/dev_manifest.json` | `Content/Data/Schema/Boot/Orchestrator/dev_manifest.json` |
| `Config/SinglePlay/ModeOverrides.json` | `Content/Data/Schema/Gameplay/ProjectSinglePlay/ModeOverrides.json` |

### Phase 3: Update Code References

#### 3.1 ProjectWidgetLayoutLoader.cpp

**File:** `Plugins/UI/ProjectUI/Source/ProjectUI/Private/Layout/ProjectWidgetLayoutLoader.cpp`

```cpp
// OLD (line 233):
return Plugin->GetBaseDir() / TEXT("Config/UI") / RelativePath;

// NEW:
#include "ProjectPaths.h"
return FProjectPaths::ResolvePluginSchemaFile(PluginName, RelativePath);
```

#### 3.2 ProjectUIRegistrySubsystem.cpp

**File:** `Plugins/UI/ProjectUI/Source/ProjectUI/Private/Subsystems/ProjectUIRegistrySubsystem.cpp`

```cpp
// OLD (line 81):
const FString DefPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Config/UI/ui_definitions.json"));

// NEW: Scan centralized schema folder
#include "ProjectPaths.h"
const FString SchemaUIDir = FProjectPaths::GetSchemaDir() / TEXT("UI");
TArray<FString> PluginDirs;
IFileManager::Get().FindFiles(PluginDirs, *(SchemaUIDir / TEXT("*")), false, true);

for (const FString& PluginDir : PluginDirs)
{
    const FString DefPath = FProjectPaths::GetPluginSchemaFile(TEXT("UI"), PluginDir, TEXT("ui_definitions.json"));
    // ... existing loading logic
}
```

```cpp
// OLD (line 197):
OutDefinition.LayoutJsonPath = FPaths::Combine(
    FPaths::Combine(Plugin->GetBaseDir(), TEXT("Config/UI")),
    OutDefinition.LayoutJson);

// NEW:
OutDefinition.LayoutJsonPath = FProjectPaths::GetPluginSchemaFile(TEXT("UI"), PluginName, OutDefinition.LayoutJson);
```

#### 3.3 ItemDefinitionGenerator.cpp

**File:** `Plugins/Resources/ProjectItems/Source/ProjectItemsEditor/Private/ItemDefinitionGenerator.cpp`

```cpp
// OLD (line 80-84):
FString UItemDefinitionGenerator::GetSourceJsonDirectory()
{
    return FPaths::ProjectPluginsDir() / TEXT("Resources/ProjectItems/SOT/Items");
}

// NEW:
#include "ProjectPaths.h"
FString UItemDefinitionGenerator::GetSourceJsonDirectory()
{
    return FProjectPaths::GetPluginSchemaDir(TEXT("Resources"), TEXT("ProjectItems"));
}
```

#### 3.4 OrchestratorCoreModule.cpp

**File:** `Plugins/Boot/Orchestrator/Source/OrchestratorCore/Private/OrchestratorCoreModule.cpp`

```cpp
// OLD (line 171):
LauncherContext.ManifestPath = FPaths::Combine(LauncherContext.InstallPath, TEXT("Config/Manifest/dev_manifest.json"));

// NEW:
LauncherContext.ManifestPath = FPaths::Combine(LauncherContext.InstallPath, TEXT("Content/Data/Schema/Boot/Orchestrator/dev_manifest.json"));
```

```cpp
// OLD (line 387):
ManifestPath = FPaths::ProjectConfigDir() / TEXT("Manifest/dev_manifest.json");

// NEW:
#include "ProjectPaths.h"
ManifestPath = FProjectPaths::GetPluginSchemaFile(TEXT("Boot"), TEXT("Orchestrator"), TEXT("dev_manifest.json"));
```

#### 3.5 SinglePlayModeRegistry.cpp

**File:** `Plugins/Gameplay/ProjectSinglePlay/Source/ProjectSinglePlay/Private/SinglePlayModeRegistry.cpp`

```cpp
// OLD (line 129):
const FString DefaultPath = FPaths::ProjectConfigDir() / TEXT("SinglePlay") / TEXT("ModeOverrides.json");

// NEW:
#include "ProjectPaths.h"
const FString DefaultPath = FProjectPaths::GetPluginSchemaFile(TEXT("Gameplay"), TEXT("ProjectSinglePlay"), TEXT("ModeOverrides.json"));
```

### Phase 4: Update Config Files

#### 4.1 DefaultGame.ini

**Remove these lines (76-81):**
```ini
+DirectoriesToAlwaysStageAsUFS=(Path="Plugins/UI/ProjectUI/Config/UI")
+DirectoriesToAlwaysStageAsUFS=(Path="Plugins/UI/ProjectHUD/Config/UI")
+DirectoriesToAlwaysStageAsUFS=(Path="Plugins/UI/ProjectVitalsUI/Config/UI")
+DirectoriesToAlwaysStageAsUFS=(Path="Plugins/UI/ProjectMenuMain/Config/UI")
+DirectoriesToAlwaysStageAsUFS=(Path="Plugins/UI/ProjectMenuGame/Config/UI")
+DirectoriesToAlwaysStageAsUFS=(Path="Plugins/UI/ProjectSettingsUI/Config/UI")
```

**No replacement needed** - Content folder is automatically staged.

### Phase 5: Update Tests

#### 5.1 ProjectWidgetLayoutLoaderTests.cpp

**File:** `Plugins/UI/ProjectUI/Source/ProjectUITests/Private/Unit/ProjectWidgetLayoutLoaderTests.cpp`

```cpp
// OLD (line 297):
TestTrue(TEXT("Config path should contain '/Config/UI/'"), ConfigPath.Contains(TEXT("/Config/UI/")));

// NEW:
TestTrue(TEXT("Config path should contain '/Data/Schema/'"), ConfigPath.Contains(TEXT("/Data/Schema/")));
```

### Phase 6: Update Documentation

- [ ] Update `CLAUDE.md` with new schema paths and FProjectPaths usage
- [ ] Update `docs/ui.md` with new JSON locations
- [ ] Update `Plugins/Resources/ProjectItems/README.md` with new Items path

### Phase 7: Cleanup

- [ ] Delete empty `Plugins/*/Config/UI/` folders
- [ ] Delete `Plugins/Resources/ProjectItems/SOT/` folder
- [ ] Delete `Config/Manifest/` folder
- [ ] Delete `Config/SinglePlay/` folder
- [ ] Verify build succeeds
- [ ] Run smoke tests

---

## Code Changes Summary

| File | Lines Changed | Type |
|------|--------------|------|
| `ProjectPaths.h` (NEW) | ~50 | New utility class |
| `ProjectPaths.cpp` (NEW) | ~70 | Implementation |
| `ProjectWidgetLayoutLoader.cpp` | ~5 | Use FProjectPaths |
| `ProjectUIRegistrySubsystem.cpp` | ~15 | Discovery refactor |
| `ItemDefinitionGenerator.cpp` | ~3 | Use FProjectPaths |
| `OrchestratorCoreModule.cpp` | ~4 | Use FProjectPaths |
| `SinglePlayModeRegistry.cpp` | ~2 | Use FProjectPaths |
| `ProjectWidgetLayoutLoaderTests.cpp` | ~2 | Update assertion |
| `DefaultGame.ini` | -6 | Remove staging config |

---

## Build Service Impact

After migration, Build Service can package per-plugin data:
- `Content/Data/Schema/UI/ProjectHUD/` -> packaged with ProjectHUD plugin
- `Content/Data/Schema/Resources/ProjectItems/` -> packaged with ProjectItems plugin

This enables true plugin independence for CDN updates.

---

## Rollback Plan

If issues arise:
1. `git revert` the migration commit
2. JSONs return to original locations
3. Code reverts to old paths
4. Re-add staging config to DefaultGame.ini

---

## Verification Checklist

- [ ] FProjectPaths compiles and works correctly
- [ ] All JSONs accessible at runtime
- [ ] UI definitions discovered correctly
- [ ] Item generator finds JSON sources
- [ ] Orchestrator loads manifest
- [ ] SinglePlay loads mode overrides
- [ ] Hot reload still works in editor
- [ ] Packaged build includes all JSONs
- [ ] Build Service can package plugin subfolders
