# Usage Guide - Blueprint JSON Converter

## Method 1: Python Console (Quick Test)

### Step 1: Open Python Console

1. Window → Developer Tools → Output Log
2. Switch to **Cmd** tab (bottom)
3. Ensure **Python** is enabled in filters

### Step 2: Export Selected Blueprint

```python
import sys
sys.path.append("<user-home>/Documents/GitHub/Alis/scripts/ue/editor/blueprint_json_converter/scripts")
exec(open("<user-home>/Documents/GitHub/Alis/scripts/ue/editor/blueprint_json_converter/scripts/export_blueprint.py").read())
```

### Step 3: Edit JSON with Claude

JSON file location:
```
ProjectRoot/Saved/AI_Snapshots/blueprint_YYYYMMDD_HHMMSS.json
```

Send to Claude:
```
"Here's my Blueprint JSON. Change MaxHealth to 200 and StartLocation.z to 500"
```

### Step 4: Import JSON Back

```python
import sys
sys.path.append("<user-home>/Documents/GitHub/Alis/scripts/ue/editor/blueprint_json_converter/scripts")
exec(open("<user-home>/Documents/GitHub/Alis/scripts/ue/editor/blueprint_json_converter/scripts/import_blueprint.py").read())

from import_blueprint import import_from_file

# Dry run first (validation only)
import_from_file("blueprint_20250114_120000.json", dry_run=True)

# Apply changes
import_from_file("blueprint_20250114_120000.json", dry_run=False)
```

---

## Method 2: Full Workflow (Export → Edit → Import)

### 1. Export Blueprint to JSON

```python
import sys
sys.path.append("<user-home>/Documents/GitHub/Alis/scripts/ue/editor/blueprint_json_converter/scripts")

from export_blueprint import export_blueprint_to_json, save_json

asset_path = "/Game/YourAsset/BP_Example.BP_Example"
data = export_blueprint_to_json(asset_path)
json_path = save_json(data)

print(f"Exported to: {json_path}")
```

### 2. Edit JSON with Claude

Claude can:
- Modify property values
- Validate data consistency
- Generate property documentation
- Batch update multiple properties

Example request:
```
"Review this Blueprint JSON:
1. Increase all health-related values by 50%
2. Validate Vector values don't contain NaN
3. List all asset references"
```

### 3. Import JSON to Blueprint

```python
from import_blueprint import import_from_file, list_available_snapshots

# List all snapshots
list_available_snapshots()

# Import with validation
import_from_file("blueprint_20250114_120000.json", dry_run=True)

# Apply if validation passes
import_from_file("blueprint_20250114_120000.json", dry_run=False)
```

---

## Common Use Cases

### Use Case 1: Batch Update MaxHealth for All Enemies

```python
# 1. Export all enemy Blueprints
enemy_paths = [
    "/Game/Enemies/BP_Goblin.BP_Goblin",
    "/Game/Enemies/BP_Orc.BP_Orc",
    "/Game/Enemies/BP_Dragon.BP_Dragon"
]

for path in enemy_paths:
    data = export_blueprint_to_json(path)
    save_json(data)

# 2. Send all JSON to Claude:
# "Increase MaxHealth by 50% for all enemies"

# 3. Import updated JSONs
import_from_file("BP_Goblin_20250114_120000.json")
import_from_file("BP_Orc_20250114_120000.json")
import_from_file("BP_Dragon_20250114_120000.json")
```

### Use Case 2: Property Validation

```python
# Export Blueprint
data = export_blueprint_to_json("/Game/Project/BP_Player.BP_Player")
save_json(data, "player_validation.json")

# Send to Claude:
# "Validate this Blueprint JSON:
# - Check all Vectors for NaN/Inf
# - Verify MaxHealth > 0 and < 10000
# - List all unused asset references"
```

### Use Case 3: Generate Difficulty Variants

```python
# Send to Claude:
# "Create 3 difficulty variants from this Blueprint:
# - Easy: MaxHealth=50, Damage=10
# - Normal: MaxHealth=100, Damage=20
# - Hard: MaxHealth=200, Damage=40"

# Claude returns 3 JSON files, import them:
import_from_file("BP_Enemy_Easy.json")
import_from_file("BP_Enemy_Normal.json")
import_from_file("BP_Enemy_Hard.json")
```

---

## Troubleshooting

### Error: "Asset not found"

Check asset path format:
```python
import unreal
unreal.EditorAssetLibrary.does_asset_exist("/Game/YourAsset/BP_Example.BP_Example")
```

### Error: "Failed to set property"

Causes:
- Type mismatch between JSON value and Blueprint property
- Property is read-only
- Invalid asset reference path

Solution: Use `dry_run=True` to diagnose before applying.

### No Properties Exported

Verify:
1. Blueprint is compiled (Compile button in Blueprint Editor)
2. Variables are marked as Instance Editable
3. No errors in Output Log

### JSON File Not Found

Default location: `ProjectRoot/Saved/AI_Snapshots/`

List available files:
```python
from import_blueprint import list_available_snapshots
list_available_snapshots()
```

---

## Advanced Usage

### Custom Export Path

Modify `TEST_ASSET` in `export_blueprint.py`:
```python
TEST_ASSET = "/Game/YourProject/BP_CustomAsset.BP_CustomAsset"
```

### Programmatic Batch Export

```python
import unreal

# Get all Blueprints in folder
asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
assets = asset_registry.get_assets_by_path("/Game/Enemies", recursive=True)

for asset_data in assets:
    asset_path = str(asset_data.package_name) + "." + str(asset_data.asset_name)
    if asset_data.asset_class == "Blueprint":
        data = export_blueprint_to_json(asset_path)
        save_json(data)
```

### Rollback Changes

```python
# Export before making changes
original_data = export_blueprint_to_json(asset_path)
save_json(original_data, "backup_before_changes.json")

# Make changes...

# Rollback if needed
import_from_file("backup_before_changes.json", dry_run=False)
```

---

## Best Practices

1. **Always use dry_run first** - Validate before applying changes
2. **Keep backups** - Export original JSON before modifications
3. **Version control** - Commit JSON snapshots to git
4. **Meaningful filenames** - Use descriptive names for manual exports
5. **Validate with Claude** - Ask Claude to check for errors before import

---

## References

- [Unreal Python API](https://docs.unrealengine.com/5.5/en-US/PythonAPI/)
- [EditorAssetLibrary](https://docs.unrealengine.com/5.5/en-US/PythonAPI/class/EditorAssetLibrary.html)
- [Blueprint CDO](https://docs.unrealengine.com/5.5/en-US/unreal-engine-blueprint-class-default-object/)
