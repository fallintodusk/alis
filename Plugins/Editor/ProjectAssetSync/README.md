# ProjectAssetSync

Editor plugin that provides **generic, cross-cutting sync utilities** for the asset pipeline.

## Plugin Scope

This plugin handles **mechanical sync operations** that apply across multiple features.

**Current implementations:**
- [OK] **JsonRefSync**: UE Asset renamed -> Update JSON path references
- [OK] **RedirectorAutoFix**: Automatically clean up redirectors after asset renames

**Future sync types** (when needed):
- Asset metadata sync
- Configuration sync
- Other generic sync patterns

**NOT in scope:**
- Feature-specific generation logic -> See `ProjectDefinitionGenerator`
- Feature-specific propagation logic -> See `ProjectPlacementEditor`

---

## JsonRefSync (Implemented)

**Pattern:** UE Asset -> JSON (mechanical path updates)

When assets are renamed/moved in Content Browser, JSON files referencing those assets are automatically updated.

### Components

- **`UJsonRefSyncSubsystem`** - Hooks `OnAssetRenamed`, debounces (0.3s), triggers fix
- **`FJsonAssetRefFixer`** - Scans JSON files, replaces exact path matches
- **Schema-driven discovery** - Reads `x-alis-generator.SourceSubDir` to find JSON roots

### How It Works

1. Asset renamed in editor
2. Subsystem captures old/new paths, starts debounce timer (batches folder moves)
3. After timer expires: scans plugin schemas for JSON directories
4. Recursively searches JSON files in discovered directories
5. Parses JSON (supports both object and array root), updates matching asset paths (preserves `.AssetName` format)
6. Checks out from source control if needed
7. Saves modified JSON with pretty-print formatting
8. Shows notification: "Updated N JSON asset reference(s)"

### Schema Example

```json
{
  "x-alis-generator": {
    "SourceSubDir": "../Content"  // Resolved from Data/ directory
  }
}
```

**Path Resolution Rule:** `JsonRoot = <PluginBaseDir>/Data + SourceSubDir` (resolved from `Data/` directory, not from schema file location)

**Result:** Plugin discovers `ProjectObject/Content/` directory for JSON scanning.

---

## RedirectorAutoFix (Implemented)

**Pattern:** UE Asset renamed -> Fix redirectors -> Clean content browser

When assets are renamed, Unreal creates redirectors at the old path. This subsystem automatically fixes all references and deletes the redirectors, keeping your content clean.

### Why Auto-Fix is Safe for This Project

**Typical UE projects** (Epic's default): Keep redirectors indefinitely to protect complex branching workflows.

**This project's workflow** makes auto-fix safe:
1. **World Partition** - Actors isolated per file (no complex inter-package dependencies)
2. **Binary merge strategy** - Always "take incoming" on conflicts
3. **Data-driven architecture** - Most refs via JSON (already auto-fixed by JsonRefSync)
4. **Frequent syncs** - No long-lived feature branches with divergent refs

**Result:** Redirectors serve no purpose. Auto-fix = safe cleanup.

### Components

- **`URedirectorAutoFixSubsystem`** - Hooks `OnAssetRenamed`, debounces (0.5s), triggers fix
- **UE's `IAssetTools::FixupReferencers()`** - Epic's battle-tested API for fixing refs
- **Auto-save** - All affected packages saved automatically (handled by FixupReferencers)

### How It Works

1. Asset renamed in editor
2. Subsystem captures old/new paths, starts debounce timer (0.5s, runs after JsonRefSync)
3. After timer expires: tries to load object at old path
4. If it's a `UObjectRedirector`, adds to fixup list
5. Calls `IAssetTools::FixupReferencers()` which:
   - Finds all assets referencing these redirectors
   - Loads referencing packages
   - Replaces redirector refs with direct refs to new asset
   - Saves modified packages (auto-checkout from source control)
   - Deletes redirectors if safe (no locked files, etc.)
6. Shows notification: "Fixed N redirectors"

### What Gets Fixed

**Direct references** (Blueprint variables, components, etc.)
```cpp
UPROPERTY()
UStaticMesh* MyMesh; // Points to redirector -> Fixed to point to new asset
```

**Soft references** (Asset paths in data)
```cpp
FSoftObjectPath Path("/Game/Old/Asset"); // Redirector -> Direct ref
```

**Collections** (Content Browser collections referencing old path)

### What Gets Saved (Automatically)

**Important:** All saves happen automatically without user confirmation.

- All packages that referenced the redirector (blueprints, levels, etc.)
- The redirector's package (deleted after fixup)
- Source control automatically checks out files if needed
- Notification shows what was saved with warning about auto-save

### Safety Mechanisms

Epic's `FixupReferencers()` handles:
- **Locked files**: Skips if can't checkout from source control
- **Map references**: Won't break level streaming
- **Failed saves**: Keeps redirector if any package fails to save
- **Load failures**: Keeps redirector if referencing packages can't be loaded

### When Redirectors Persist

Redirector fixup is skipped if:
- Source control checkout fails (file locked by another user)
- Package save fails (permissions issue)
- Destination asset can't be loaded

**Result**: Log warning, keep redirector for manual cleanup later.

### Configuration

**Location**: `Editor -> Project Settings -> Plugins -> Asset Sync`

**Setting**: `Auto-Fix Redirectors On Rename` (default: ON)

Enable/disable auto-fix based on your workflow:
- **ON (default)**: Safe for World Partition + binary merge + frequent syncs
- **OFF**: If team uses long-lived feature branches (redirectors protect divergent refs)

### Example Flow

```
1. Rename SM_Door_Old -> SM_Door_New in Content Browser
2. [JsonRefSync] Updates JSON files (0.3s debounce)
3. [RedirectorAutoFix] Finds redirector at SM_Door_Old path (0.5s debounce)
4. Calls IAssetTools::FixupReferencers() which:
   - Loads BP_DoorSystem (references redirector)
   - Replaces ref: redirector -> SM_Door_New
   - Saves BP_DoorSystem.uasset (with SCC checkout)
   - Deletes redirector package
5. Notification: "Auto-fixed 1 redirector and saved 1 package(s)"
   Subtext: "SM_Door_Old
   Referencing packages were automatically checked out and saved."
```

### Logs

```
LogRedirectorAutoFix: Checking for redirector at: /ProjectObject/.../SM_Door_Old
LogRedirectorAutoFix: Found redirector to fix: /ProjectObject/.../SM_Door_Old -> SM_Door_New
LogRedirectorAutoFix: Fixing 1 redirector(s) using AssetTools...
LogRedirectorAutoFix: === Fixed 1 redirector(s) ===
```

---

## Related Plugins

**Event Bus:** `FDefinitionEvents` lives in `ProjectEditorCore` (minimal event bus for decoupling)

### Generation Logic -> ProjectDefinitionGenerator
**Direction:** JSON file changed -> Regenerate DataAsset

- File watcher (0.5s debounce)
- JSON -> DataAsset generation
- Hash-based incremental updates
- Broadcasts: `FDefinitionEvents::OnDefinitionRegenerated`

### Propagation Logic -> ProjectPlacementEditor
**Direction:** Definition regenerated -> Update scene actors

- Subscribes to: `FDefinitionEvents::OnDefinitionRegenerated`
- Actor update strategies (Reapply vs Replace)
- DefMeshId tag-based matching

**See:** `ProjectAssetSync/TODO.md` for architecture discussion.

---

## Usage

Plugin auto-initializes. No manual setup required.

**Test JsonRefSync:**
1. Create JSON file referencing an asset (e.g., `/ProjectObject/.../SM_Mesh`)
2. Rename asset in Content Browser
3. JSON file updates automatically
4. Notification appears: "Updated 1 JSON asset reference(s)"

**Logs:**
```
LogJsonRefSync: Discovered JSON root from schema object.schema.json: E:/.../Content
LogJsonAssetRefFixer: Fixed 1 references in: .../object_definition.json
```

---

## Dependencies

- `AssetRegistry` - OnAssetRenamed delegate
- `Json` - JSON parsing/serialization
- `SourceControl` - Optional checkout before modify
- `Projects` - IPluginManager for schema discovery
- `ProjectCore` - FProjectPaths utility

---

## Terminology

| Term | What It Means | Example |
|------|---------------|---------|
| **Sync** | Mechanical updates (this plugin) | Asset renamed -> JSON path updated |
| **Generation** | Transformation with validation | JSON -> DataAsset creation |
| **Propagation** | Cascading dependency updates | Definition -> Actor refresh |

**Key Insight:** This plugin handles true **sync** (mechanical corrections), not generation or propagation.

**See:** [Data Pipeline Architecture](../../../docs/data/README.md#data-pipeline-architecture) for complete flow documentation.
