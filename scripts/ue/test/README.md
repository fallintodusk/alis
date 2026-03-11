# Test Scripts

Scripts for running automated tests (Unit, Integration, Smoke).

## 📂 Scripts

### `test.bat`
**Purpose**: Run Unreal Engine automation tests.
**Usage**: `test.bat [--unit|--integration|--all] [--filter <name>]`

**Examples:**
- `test.bat --unit` (Runs ProjectCore, ProjectData, etc.)
- `test.bat --integration` (Runs ProjectIntegrationTests)
- `test.bat --filter ProjectUI` (Runs specific test suite)

## 📁 Categories

### `smoke/`
**Purpose**: Critical path tests that must pass for a build to be usable.
**Duration**: < 5 minutes.
**Includes**: Basic boot, login flow, map load.

### `integration/`
**Purpose**: Multi-system tests verifying components work together.
**Duration**: 5-30 minutes.

Useful integration validation scripts:
- `integration\validate_spawnclass_cook.bat` - Cook preflight for ObjectDefinition `spawnClass` Blueprint retention (`GrandPa_BP`).

### `unit/`
**Purpose**: Isolated tests for individual functions or classes.
**Duration**: < 1 minute (usually seconds).

## 🔧 Headless Execution
Tests run in **headless mode** (`-NullRHI`) by default to support CI environments.
- Logs are saved to `Saved/Automation/Reports`.
- Use `--filter` to verify specific fixes locally.

## See Also
- [../../docs/testing/index.md](../../docs/testing/index.md) for full Testing Strategy.
