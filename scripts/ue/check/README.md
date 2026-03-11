# Validation Scripts

Scripts for static analysis, validation, and compilation checks WITHOUT running a full game instance or tests.

## 📂 Scripts

### `check.bat`
**Purpose**: Main entry point for all fast validation checks.
**Usage**: `check.bat [--uht|--syntax|--blueprints|--assets|--all]`

**Modes:**
- `--uht`: Validates Unreal Header Tool reflection data (very fast).
- `--syntax`: Checks C++ syntax without full linking (fast).
- `--blueprints`: Compiles all Blueprints to catch broken references.
- `--assets`: Runs asset validation logic.
- `--all`: Runs all of the above in sequence.

### `bp_compile.bat`
**Purpose**: Compiles all Blueprints. Equivalent to `check.bat --blueprints`.
**Duration**: 2-5 minutes.

### `project_validate.bat`
**Purpose**: Validates project assets. Equivalent to `check.bat --assets`.
**Duration**: 1-3 minutes.

### `validate.sh`
**Purpose**: Linux wrapper for validation checks.

## 💡 When to Use
- Run `check.bat --all` before pushing code.
- Run `check.bat --uht` when changing header files (`.h`).
- Run `check.bat --blueprints` after major refactors or deleting assets.
