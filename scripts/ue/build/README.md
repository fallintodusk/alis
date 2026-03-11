# Build Scripts

Scripts for compiling C++ code and creating build artifacts.

## Scripts

### `build.bat`

Purpose: Direct Windows wrapper for Unreal Build Tool (UBT).

Usage:

```bat
build.bat [target] [config] [extra_args...]
```

Examples:

- `build.bat` -> defaults to `AlisEditor Development`
- `build.bat AlisEditor DebugGame`
- `build.bat AlisEditor Development -Module=ProjectBoot`

### `rebuild_module_safe.ps1`

Purpose: safely rebuild a specific module after cleaning its intermediate files.

Usage:

```powershell
rebuild_module_safe.ps1 -Module <ModuleName>
```

## Configuration

- UE path is defined in `scripts/config/ue_path.conf`
- project file is detected automatically relative to the script

## Important

- these scripts do NOT cook content or package the game
- for public release packaging, use `scripts/ue/package/README.md`
- ensure `ue_path.conf` points to the intended engine install before building
