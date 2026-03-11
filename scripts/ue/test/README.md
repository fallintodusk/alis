# Test Scripts

Scripts for running Unreal automation tests in ALIS.

## Main Entry Point

### `test.bat`

Runs Unreal automation tests through grouped modes.

Usage:

```bat
test.bat [--unit|--integration|--all] [--filter <name>]
```

Examples:
- `test.bat --unit`
- `test.bat --integration`
- `test.bat --filter ProjectUI`

## Test Categories

### `smoke/`

Critical-path health checks.

- Example: `smoke\\boot_test.bat`

### `integration/`

Multi-system validation.

- Example: `integration\\autonomous_boot_test.bat`
- Additional preflight helpers live in this folder as needed

### `unit/`

Fast plugin or subsystem tests.

## Execution Notes

- Tests are typically run headless with `-NullRHI`
- Reports are written under `Saved/Automation/Reports`
- Full UE logs go to `Saved/Logs/`

## Related Docs

- Public testing router: [../../../docs/testing/README.md](../../../docs/testing/README.md)
- Build router: [../../../docs/build/README.md](../../../docs/build/README.md)
