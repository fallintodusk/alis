# Data Layout & JSON Structure

This document outlines the standard project structure for data-driven features using JSON.

## 1. Directory Structure

Data files are NO LONGER centralized. They live within their owning plugin.

**Standard Path:**
`Plugins/<Category>/<PluginName>/Data/`

**Examples:**
- **Objects:** `Plugins/Resources/ProjectObject/Data/Objects/Sword.json`
- **Boot:** `Plugins/Boot/Orchestrator/Data/dev_manifest.json`

**Note:**
We prioritize plugin modularity. `Data/` (plugin root) contains the **Source of Truth** (JSON/Schemas). Files are generated/staged from here into `Content/` or packaged builds.

## 2. Relative Paths in JSON

When referencing other assets or files within JSON, use **standard UE soft paths** or relative paths resolved by helper functions.

### Asset References
Use the standard `/Game/...` or `/PluginName/...` soft object path.
```json
{
  "icon": "/ProjectObject/Textures/Icons/Sword_Icon.Sword_Icon",
  "staticMesh": "/ProjectWorld/Meshes/Weapons/SM_Sword.SM_Sword"
}
```

### JSON-to-JSON References
When a JSON file (like a layout) references another JSON file (like a child fragment), the path is **relative to the plugin's `Content/Data` folder**.

**Example:**
`ui_definitions.json`:
```json
{
  "id": "HUD_Main",
  "layout_json": "Layouts/HUD_Layout.json" 
}
```
*Resolves to:* `Plugins/.../Content/Data/Layouts/HUD_Layout.json`

## 3. C++ Path Resolution

Do NOT hardcode paths. Use `FProjectPaths` from `ProjectCore`.

```cpp
#include "ProjectPaths.h"

// Get the base Data directory for a plugin
FString DataDir = FProjectPaths::GetPluginDataDir(TEXT("ProjectUI"));
// Result: ".../Plugins/UI/ProjectUI/Content/Data"

// Check for a file
bool bExists = FProjectPaths::PluginDataFileExists(TEXT("ProjectUI"), TEXT("config.json"));
```

## 4. Packaging
All files in `Content/Data` are automatically staged into the PAK/IoStore in **cooked builds** (Game, Client, Server) via `Build.cs` `RuntimeDependencies`. 

- **Editor:** Reads directly from source `Content/Data`.
- **Shipped:** Reads from staged `Content/Data` (UFS).
