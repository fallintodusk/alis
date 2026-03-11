# Quick Start Guide

## Prerequisites

- Unreal Engine 5.5
- Windows OS
- Python (built into UE)

## Step 1: Export Blueprint to JSON

Open UE Python Console (Window → Developer Tools → Output Log → Cmd tab)

```python
import sys
sys.path.append("<user-home>/Documents/GitHub/Alis/scripts/ue/editor/blueprint_json_converter/scripts")
exec(open("<user-home>/Documents/GitHub/Alis/scripts/ue/editor/blueprint_json_converter/scripts/export_blueprint.py").read())
```

**Output:** JSON file saved to `Saved/AI_Snapshots/blueprint_YYYYMMDD_HHMMSS.json`

## Step 2: Edit JSON with Claude

Send the JSON file to Claude with your desired changes:

**Example request:**
```
"Here's my Blueprint JSON. Please change:
- custom_time_dilation to 2.0
- can_be_damaged to false
- relative_location.z to 500"
```

## Step 3: Import modified JSON back

```python
import sys
sys.path.append("<user-home>/Documents/GitHub/Alis/scripts/ue/editor/blueprint_json_converter/scripts")
exec(open("<user-home>/Documents/GitHub/Alis/scripts/ue/editor/blueprint_json_converter/scripts/import_blueprint.py").read())

# List available snapshots
from import_blueprint import list_available_snapshots, import_from_file
list_available_snapshots()

# Import (dry run first for safety)
import_from_file("blueprint_20250114_120000.json", dry_run=True)

# If looks good, apply changes
import_from_file("blueprint_20250114_120000.json", dry_run=False)
```

## Common Use Cases

### 1. Batch Property Changes

Export multiple Blueprints, send to Claude for bulk editing:
```
"Set all enemy Blueprints to:
- custom_time_dilation = 1.5
- can_be_damaged = true"
```

### 2. Validation

Ask Claude to validate property values:
```
"Check this Blueprint JSON:
- Ensure all Vectors don't contain NaN/Inf
- Verify custom_time_dilation is between 0.1 and 5.0"
```

### 3. Property Discovery

```
"Show me all properties related to networking in this Blueprint"
```

## Troubleshooting

### JSON file not found

Check: `ProjectRoot/Saved/AI_Snapshots/`

### Import fails

- Verify asset_path in JSON matches actual Blueprint path
- Run with `dry_run=True` first to diagnose issues
- Check UE Output Log for specific errors

### No properties exported

- Ensure Blueprint is compiled
- Check export script ran without errors
- Verify TEST_ASSET path in export_blueprint.py

## Next Steps

- Read [LIMITATIONS.md](LIMITATIONS.md) to understand what can/cannot be exported
- See [CLAUDE_WORKFLOW.md](CLAUDE_WORKFLOW.md) for advanced Claude usage patterns
- Review [EXAMPLE_SESSION.md](EXAMPLE_SESSION.md) for real-world scenarios
