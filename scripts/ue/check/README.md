# Validation Scripts

Scripts for fast validation and compile-adjacent checks without running a full packaged build.

## Main Entry Points

### `check.bat`

Runs grouped validation modes.

Usage:

```bat
check.bat [--uht|--syntax|--blueprints|--assets|--all]
```

Modes:
- `--uht` - Unreal Header Tool validation
- `--syntax` - C++ syntax checks without full linking
- `--blueprints` - Blueprint compile validation
- `--assets` - asset validation
- `--all` - all fast checks in sequence

### Direct wrappers

- `bp_compile.bat` - Blueprint compilation shortcut
- `project_validate.bat` - asset validation shortcut
- `validate.sh` - shell wrapper for validation flow

## When To Use

- before pushing code
- after header changes
- after refactors that can break Blueprint references
- after asset moves or data updates

## Related Docs

- Build router: [../../../docs/build/README.md](../../../docs/build/README.md)
- Testing router: [../../../docs/testing/README.md](../../../docs/testing/README.md)
