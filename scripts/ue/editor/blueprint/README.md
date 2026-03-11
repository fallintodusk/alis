# Blueprint ↔ JSON Converter

System for exporting Blueprint properties to JSON and importing them back into Unreal Engine 5.5.

## Project Structure

```
blueprint_json_converter/
├── scripts/              # Production scripts
│   ├── export_blueprint.py   # Export Blueprint → JSON
│   └── import_blueprint.py   # Import JSON → Blueprint
├── docs/                 # Documentation
│   ├── README.md              # Detailed documentation
│   ├── QUICKSTART.md          # Quick start guide
│   ├── LIMITATIONS.md         # System limitations
│   ├── CLAUDE_WORKFLOW.md     # Working with Claude
│   └── EXAMPLE_SESSION.md     # Usage examples
├── tests/                # Test and diagnostic scripts
│   ├── DIAGNOSTIC.py          # Blueprint API diagnostics
│   ├── FIND_VARIABLES.py      # Search for Blueprint variables
│   ├── EXPORT_ALL_VARIABLES.py
│   ├── EXPORT_WITH_COMPONENTS.py
│   ├── EXPORT_SCS_COMPONENTS.py
│   └── ...
└── deprecated/           # Deprecated scripts
    ├── bp_to_json.py          # Old export version
    └── quick_test.py          # Old test

```

## Quick Start

### 1. Export Blueprint to JSON

Open UE Python Console and execute:

```python
import sys
sys.path.append("<user-home>/Documents/GitHub/Alis/scripts/ue/editor/blueprint_json_converter/scripts")
exec(open("<user-home>/Documents/GitHub/Alis/scripts/ue/editor/blueprint_json_converter/scripts/export_blueprint.py").read())
```

JSON file will be saved to: `Saved/AI_Snapshots/blueprint_YYYYMMDD_HHMMSS.json`

### 2. Edit with Claude

Send the JSON file to Claude and ask to modify the desired properties.

### 3. Import changes back

```python
import sys
sys.path.append("<user-home>/Documents/GitHub/Alis/scripts/ue/editor/blueprint_json_converter/scripts")
exec(open("<user-home>/Documents/GitHub/Alis/scripts/ue/editor/blueprint_json_converter/scripts/import_blueprint.py").read())
```

## What Works ✅

- **28 Actor Properties**: can_be_damaged, custom_time_dilation, net_priority, relative_location, etc.
- **Data Types**: bool, float, int, Vector, Rotator, Color, Guid, Object References
- **Dry-run mode**: validation before applying changes

## What Doesn't Work ❌

- **Blueprint Variables**: CurrentSkyIntensity, TargetSkyIntensity and other custom variables
- **Components**: DirectionalLight, SkyLight and their properties
- **SCS (SimpleConstructionScript)**: Blueprint components not accessible via Python API

**Reason**: UE 5.5 Python API doesn't provide access to Blueprint-specific variables and components.

## Documentation

- [docs/README.md](docs/README.md) - Complete documentation
- [docs/QUICKSTART.md](docs/QUICKSTART.md) - Quick start with examples
- [docs/LIMITATIONS.md](docs/LIMITATIONS.md) - Detailed limitations and alternative solutions
- [docs/CLAUDE_WORKFLOW.md](docs/CLAUDE_WORKFLOW.md) - Examples of working with Claude

## Testing

Diagnostic scripts in `tests/` folder:
- `DIAGNOSTIC.py` - shows all available Blueprint API methods
- `FIND_VARIABLES.py` - searches for Blueprint variables in CDO
- `EXPORT_WITH_COMPONENTS.py` - tests access to components

## System Requirements

- Unreal Engine 5.5
- Python 3.x (built into UE)
- Windows (paths adapted for Windows)

## Usage

See [docs/USAGE.md](docs/USAGE.md) for detailed instructions.
