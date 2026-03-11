# Blueprint ↔ JSON Converter - Complete Documentation

Bidirectional Blueprint to JSON converter for AI-powered editing.

## Overview

This system enables:
1. Export Blueprint properties to JSON format
2. Edit JSON with Claude or other AI tools
3. Import modified JSON back into Blueprint
4. Automatic Blueprint save and compilation

## Quick Start

### Step 1: Export Blueprint

```python
import sys
sys.path.append("<user-home>/Documents/GitHub/Alis/scripts/ue/editor/blueprint_json_converter/scripts")
exec(open("<user-home>/Documents/GitHub/Alis/scripts/ue/editor/blueprint_json_converter/scripts/export_blueprint.py").read())
```

**Output:** `Saved/AI_Snapshots/blueprint_YYYYMMDD_HHMMSS.json`

### Step 2: Edit with Claude

Example request:
```
"Review this Blueprint JSON and increase MaxHealth by 50%"
```

### Step 3: Import Modified JSON

```python
from import_blueprint import import_from_file, list_available_snapshots

# List available files
list_available_snapshots()

# Import with dry-run validation
import_from_file("blueprint_20250114_120000.json", dry_run=True)

# Apply changes
import_from_file("blueprint_20250114_120000.json", dry_run=False)
```

## JSON Structure

```json
{
  "asset_path": "/Game/Project/BP_Example.BP_Example",
  "asset_type": "Blueprint",
  "class_name": "BP_Example_C",
  "exported_at": "2025-01-14T12:00:00Z",
  "properties": {
    "custom_time_dilation": {
      "value": 1.0,
      "type": "float"
    },
    "can_be_damaged": {
      "value": true,
      "type": "bool"
    },
    "relative_location": {
      "value": {"x": 0, "y": 0, "z": 100},
      "type": "Vector"
    }
  }
}
```

## Supported Types

| UE Type | JSON Representation |
|---------|---------------------|
| `int`, `float`, `bool`, `string` | Primitives |
| `FVector`, `FRotator` | `{"x": 0, "y": 0, "z": 0}` |
| `FVector2D` | `{"x": 0, "y": 0}` |
| `FVector4`, `FLinearColor` | `{"x": 0, "y": 0, "z": 0, "w": 1}` |
| Asset References | `{"_type": "AssetReference", "path": "..."}` |
| Arrays | JSON arrays |

## Workflow

1. **Export** → Generate JSON snapshot
2. **Edit** → Claude reads and modifies JSON
3. **Validate** → Dry-run import to check changes
4. **Import** → Apply changes to Blueprint
5. **Verify** → Blueprint automatically saved

## Limitations

### Not Exported
- Blueprint Graph nodes
- Event Graphs (Blueprint logic)
- Component hierarchy structures
- Compiled bytecode

### Exported Successfully
- Actor properties (28 properties)
- CDO (Class Default Object) values
- Basic data types and references

See [LIMITATIONS.md](LIMITATIONS.md) for detailed information.

## Storage Location

```
ProjectRoot/Saved/AI_Snapshots/
├── blueprint_20250114_120000.json
├── blueprint_20250114_130000.json
└── ...
```

## Use Cases

### 1. Batch Property Updates
Export multiple Blueprints, send to Claude for bulk modification:
```
"Set all enemy Blueprints to have can_be_damaged = true"
```

### 2. Property Validation
```
"Validate all Vector values don't contain NaN or Inf"
```

### 3. Gameplay Balancing
```
"Increase custom_time_dilation by 20% for all slow-motion actors"
```

### 4. Documentation Generation
```
"Generate a markdown table of all exported properties and their purposes"
```

## Claude Integration

Claude can:
- Read JSON and suggest improvements
- Find property inconsistencies
- Validate value ranges and types
- Generate diffs for changes
- Batch edit multiple Blueprints

Claude cannot:
- Directly edit Blueprint nodes (only via JSON properties)
- Access Blueprint-specific variables (API limitation)
- Modify component hierarchies

## Testing

```python
# Round-trip test: Export → Import → Verify
original = export_blueprint_to_json(asset_path)
save_json(original, "test_original.json")
import_from_file("test_original.json")
modified = export_blueprint_to_json(asset_path)

# Compare original == modified (should be identical)
```

## API Reference

### Export Functions
- `export_blueprint_to_json(asset_path)` - Export single Blueprint
- `save_json(data, filename=None)` - Save to AI_Snapshots

### Import Functions
- `import_from_file(filename, dry_run=False)` - Import JSON
- `list_available_snapshots()` - List all saved JSONs
- `import_json_to_blueprint(json_path, dry_run)` - Low-level import

## System Requirements

- Unreal Engine 5.5
- Python 3.x (built into UE)
- Windows OS

## Further Reading

- [QUICKSTART.md](QUICKSTART.md) - 30-second quick start
- [LIMITATIONS.md](LIMITATIONS.md) - What works and what doesn't
- [CLAUDE_WORKFLOW.md](CLAUDE_WORKFLOW.md) - Advanced Claude usage
- [EXAMPLE_SESSION.md](EXAMPLE_SESSION.md) - Real-world examples
- [USAGE.md](USAGE.md) - Detailed usage instructions

## References

- [Unreal Python API](https://docs.unrealengine.com/5.5/en-US/PythonAPI/)
- [EditorAssetLibrary](https://docs.unrealengine.com/5.5/en-US/PythonAPI/class/EditorAssetLibrary.html)
- [Blueprint CDO Documentation](https://docs.unrealengine.com/5.5/en-US/unreal-engine-blueprint-class-default-object/)
