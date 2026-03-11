# Build Commands & Targets

Build system reference for the Alis UE 5.5 project.

---

## Build Targets

| Target | Purpose | Server Code | Rendering | Use Case |
|--------|---------|-------------|-----------|----------|
| **AlisEditor** | Development editor | [OK] Yes | [OK] Yes | Development, testing, content creation |
| **Alis** | Full game | [OK] Yes | [OK] Yes | Standalone game, listen server |
| **AlisClient** | Client-only | [X] No | [OK] Yes | Connect to dedicated server, optimized size |
| **AlisServer** | Dedicated server | [OK] Yes | [X] No | Headless server, Linux deployment |

**Target files:** `Source/*.Target.cs` (AlisEditor.Target.cs, Alis.Target.cs, etc.)

---

## Quick Build Commands

### Via Scripts (Recommended)
```bash
# Build editor (Development)
scripts/ue/build/build.bat AlisEditor Win64 Development

# Build editor (Debug)
scripts/ue/build/build.bat AlisEditor Win64 DebugGame

# Build client (Shipping)
scripts/ue/build/build.bat AlisClient Win64 Shipping

# Build server (Development)
scripts/ue/build/build.bat AlisServer Win64 Development

# Fast module rebuild (<5 min)
scripts/ue/build/rebuild_module_safe.ps1 -ModuleName ProjectMenuUI
```

### Via Unreal Build Tool (UBT)
```bash
# Build editor
"<ue-path>\Engine\Build\BatchFiles\Build.bat" AlisEditor Win64 Development "<project-root>\Alis.uproject"

# Build client
"<ue-path>\Engine\Build\BatchFiles\Build.bat" Alis Win64 Development "<project-root>\Alis.uproject"

# Build server
"<ue-path>\Engine\Build\BatchFiles\Build.bat" AlisServer Win64 Development "<project-root>\Alis.uproject"

# Build single module (fast iteration)
"<ue-path>\Engine\Build\BatchFiles\Build.bat" AlisEditor Win64 Development "<project-root>\Alis.uproject" -Module=ProjectBoot
```

### Via Visual Studio
1. Open `Alis.sln`
2. Select build configuration (dropdown):
   - `Development Editor` (recommended for development)
   - `DebugGame Editor` (slower, full debugging)
   - `Shipping` (optimized, no debug symbols)
3. Build: `Ctrl+Shift+B`

### Via Unreal Editor
1. Right-click `Alis.uproject` -> "Generate Visual Studio project files"
2. Launch Unreal Editor (auto-compiles on startup)
3. For packaged builds: File -> Package Project -> Windows/Linux

---

## Build Configurations

| Configuration | Optimization | Debug Symbols | Logging | Use Case |
|---------------|--------------|---------------|---------|----------|
| **DebugGame** | None | Full | Verbose | Deep debugging |
| **Development** | Medium | Yes | Normal | Daily development |
| **Shipping** | Full | No | Minimal | Production release |
| **Test** | Full | Yes | Normal | Automated testing |

---

## Fast Iteration Techniques

### Module-Only Rebuild (5-10 min)
Rebuild single module instead of entire project:

```powershell
# Via script
scripts/ue/build/rebuild_module_safe.ps1 -ModuleName ProjectMenuUI

# Via UBT
"<ue-path>\Engine\Build\BatchFiles\Build.bat" AlisEditor Win64 Development "<project-root>\Alis.uproject" -Module=ProjectMenuUI
```

**When to use:**
- Changed code in single plugin module
- Want quick compile feedback
- Testing C++ changes without full rebuild

**Modules available:**
- `ProjectCore`, `ProjectBoot`, `ProjectLoading`, `ProjectSession`
- `ProjectData`, `ProjectContentPacks`, `ProjectGameFeatures`
- `ProjectUI`, `ProjectMenuUI`, `ProjectMenuUITests`
- `Alis` (main game module)

### Hot Reload (30 sec - 2 min)
Compile C++ changes while editor is running:

1. Make C++ changes
2. In editor: `Ctrl+Alt+F11` or click "Compile" button
3. Wait for compile + reload

**Limitations:**
- Can't change class layouts (new members, inheritance)
- May cause editor instability on complex changes
- Recommend full rebuild for structural changes

### Live Coding (Experimental)
UE 5.5 live coding feature (may be unstable):

```bash
# Enable in editor: Edit -> Editor Preferences -> General -> Live Coding
```

---

## Validation Without Build

Fast syntax/validation checks (no full compile):

```bash
# Validate C++ reflection markup (30 sec)
scripts/ue/check/validate_uht.bat

# Compile all Blueprints (2-5 min)
scripts/ue/check/validate_blueprints.bat

# Run data validation (1-3 min)
scripts/ue/check/validate_assets.bat

# Or via Make (all checks)
make check
make check-uht
make check-blueprints
make check-assets
```

**Use before commits** - catches errors without 15+ min full build.

Details: [scripts/docs/architecture.md](../../scripts/docs/architecture.md)

---

## WSL Usage (Windows Subsystem for Linux)

If you work from WSL, the Makefile includes wrappers to invoke Windows UE tools via `cmd.exe` with correct Windows paths:

- Works automatically from WSL
  - `make check-uht` -> Runs UHT via Windows UnrealBuildTool (cmd.exe)
  - `make generate`, `make build-editor`, `make build-game`, `make build-server`, `make build-module MODULE=<Name>` -> Calls `Build.bat` via cmd.exe
  - `make open` -> Launches `UnrealEditor.exe` with the project path

- Prints PowerShell commands (you run them manually)
  - `make check-blueprints` and `make check-assets` print exact `UnrealEditor-Cmd.exe` commands; run them in Windows PowerShell for reliability. Logs are written under `Saved/Validation/Reports/`.

- UE path configuration
  - Set once in `scripts/config/ue_path.conf` (single source of truth)
  - Verify from PowerShell: `Test-Path "$env:UE_PATH/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe"`

- Quick example (from WSL)
  - `make check` -> Runs UHT, then prints PowerShell commands for BP/DataValidation
  - To execute BP compile in PowerShell:
    - `"<ue-path>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "<project-root>\Alis.uproject" -run=CompileAllBlueprints -unattended -nop4 -CrashForUAT -log="<project-root>\Saved\Validation\Reports\bp_compile.log"`

Note: Some echoed commands may show a stylized `C:` glyph when printed from WSL; this is a display artifact and does not affect execution in PowerShell.

---

## Troubleshooting
 
 ### Quick Diagnosis
 
 **C++ Compiler Errors:**
 ```
 Error C2065: undeclared identifier
 Error C2664: cannot convert argument
 Error C4430: missing type specifier
 ```
 **Resolution:**
 1. Check `#include` statements.
 2. Verify module dependencies in `.Build.cs`.
 3. Rebuild single module: `rebuild_module_safe.ps1 -ModuleName <name>`.
 
 **UHT (Unreal Header Tool) Errors:**
 ```
 Error: UPROPERTY/UFUNCTION issues
 Error: Missing GENERATED_BODY()
 ```
 **Resolution:**
 1. Run fast validation: `scripts/ue/check/validate_uht.bat`.
 2. Check reflection macros (UCLASS, UPROPERTY, UFUNCTION).
 3. Verify API export macros are UPPERCASE: `PROJECTMENUCORE_API`.
 
 **Linker Errors:**
 ```
 Error LNK2019: unresolved external symbol
 ```
 **Resolution:**
 1. Check module is listed in `.Build.cs` dependencies.
 2. Rebuild dependent modules.
 
 **Feature Plugin Not Compiling:**
 - CRITICAL Rules (4):
   1. Asset name = Plugin name EXACTLY.
   2. Location: `Plugins/Features/<Name>/Content/<Name>.uasset`.
   3. .uplugin path: `"FeatureData": "/<Name>/<Name>.<Name>"`.
   4. API macro: `<NAME>_API` (UPPERCASE).
 
 ### Build Fails: "Cannot open file 'UnrealEditor-Alis.lib'"
 **Cause:** Linker can't find intermediate library
 
 **Fix:**
 ```bash
 # Clean intermediate files
 rm -rf Intermediate/ Binaries/
 
 # Regenerate project files
 Right-click Alis.uproject -> "Generate Visual Studio project files"
 
 # Full rebuild
 scripts/ue/build/build.bat AlisEditor Win64 Development
 ```
 
 ### Build Fails: "API macro not found"
 **Cause:** Module API macro missing or wrong case
 
 **Fix:**
 1. Check module's main .h file for `<MODULE>_API` macro
 2. Ensure UPPERCASE: `PROJECTMENUCORE_API`, not `ProjectMenuCore_API`
 3. Verify module listed in `.Build.cs` dependencies
 
 See: [docs/architecture/conventions.md#api-macros](../architecture/conventions.md#api-macros)
 
 ### Build Slow (>30 min)
 **Causes:** Full rebuild, many modules, low RAM
 
 **Optimizations:**
 ```bash
 # Use module-only rebuild
 scripts/ue/build/rebuild_module_safe.ps1 -ModuleName <Module>
 
 # Increase parallel jobs (if 16+ GB RAM)
 # Edit: Engine/Config/ConsoleVariables.ini
 # Add: r.XGEShaderCompile=1
 ```
 
 ### "UE path not found"
 **Cause:** UE 5.5 not detected
 
 **Fix:**
 ```bash
 # Create config
 cp scripts/config/ue_path.conf.example scripts/config/ue_path.conf
 
 # Edit: UE_PATH="<ue-path>"
 ```
 
 ### GameFeature Plugin Not Compiling
 **Cause:** Incorrect asset naming or location
 
 **Fix:** See [docs/architecture/conventions.md#gamefeature-requirements](../architecture/conventions.md#gamefeature-requirements)

---

## Build Artifacts

**Binaries:** `Binaries/Win64/`
- `UnrealEditor-Alis.dll` - Main game module
- `UnrealEditor-ProjectCore.dll` - Core plugin
- `UnrealEditor-ProjectMenuUI.dll` - Menu UI plugin
- (etc. for each module)

**Intermediate:** `Intermediate/Build/Win64/`
- Object files, generated headers
- **Safe to delete** for clean rebuild

**Saved:** `Saved/Logs/`
- Build logs: `Alis.log`

---

## CI/CD Integration

**Overnight builds:** See [docs/guides/ci_setup.md](../guides/ci_setup.md)

**Jenkins/GitHub Actions:**
```bash
# Headless build (CI)
"<ue-path>\Engine\Build\BatchFiles\Build.bat" AlisEditor Win64 Development "<project-root>\Alis.uproject" -NoHotReloadFromIDE

# Package for distribution
"<ue-path>\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="<project-root>/Alis.uproject" -platform=Win64 -configuration=Shipping -cook -stage -pak -archive
```

---

## Quick Reference

**Scripts location:** `scripts/ue/build/`
**Config:** `scripts/config/ue_path.conf`
**Module rebuild:** `scripts/ue/build/rebuild_module_safe.ps1`
**Validation:** `scripts/ue/check/validate_*.bat`

**Build time estimates:**
- Full editor build: 15-30 min (first time), 5-15 min (incremental)
- Module rebuild: 2-5 min
- Hot reload: 30 sec - 2 min
- Validation (no build): 30 sec - 5 min
