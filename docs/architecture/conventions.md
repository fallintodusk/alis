# Project Conventions & Critical Rules

Naming conventions, GameFeature requirements, and code style for the Alis project.

---

## Documentation Encoding (ASCII Only)

- Use ASCII characters only in all `.md` and script files.
- Replace Unicode symbols with ASCII equivalents:
  - Arrow symbols (Unicode) -> use `->`
  - Em/en dashes (Unicode) -> use `-`
  - Curly double quotes -> use `"`
  - Curly apostrophes -> use '\''
- Reason: prevents garbled text in consoles, CI logs, Windows tools, and UE parsers.
- **Common Pitfall:** Copying text from Jira/Slack often pastes "smart quotes" or invisible spaces. These break automation.
- If you see replacement glyphs (black diamond with question mark), convert the file to plain ASCII.

## GameFeature Plugin Requirements (CRITICAL)

**For GameFeature plugins to register successfully in UE, you MUST follow these rules:**

### Rule 1: Asset Name = Plugin Name (EXACTLY)
[X] **WRONG:**
```
Plugin: ProjectMenuCore
Asset:  DA_MenuCore.uasset
Result: Plugin fails to register
```

[OK] **CORRECT:**
```
Plugin: ProjectMenuCore
Asset:  ProjectMenuCore.uasset
Result: Plugin registers successfully
```

### Rule 2: Asset Location (Plugin Content Root)
[X] **WRONG:**
```
Plugins/GameFeatures/ProjectMenuCore/Content/GameFeatureData/SomeOther.uasset
```

[OK] **CORRECT:**
```
Plugins/GameFeatures/ProjectMenuCore/Content/ProjectMenuCore.uasset
```

### Rule 3: .uplugin GameFeatureData Path
Must match actual asset location:

```json
// Plugins/GameFeatures/ProjectMenuCore/ProjectMenuCore.uplugin
{
  "GameFeatureData": "/ProjectMenuCore/ProjectMenuCore.ProjectMenuCore"
}
```

**Format:** `/<PluginName>/<AssetName>.<AssetName>`

### Rule 4: Module API Macro (UPPERCASE)
[X] **WRONG:**
```cpp
// ProjectMenuCore.h
class ProjectMenuCore_API UMyClass { };  // Wrong case!
```

[OK] **CORRECT:**
```cpp
// ProjectMenuCore.h
class PROJECTMENUCORE_API UMyClass { };  // All UPPERCASE
```

**Rule:** Module API macros MUST be UPPERCASE (UE convention)

### Verification
```bash
# Run diagnostic
scripts/ue/test/smoke/gamefeature_registration.bat
```

**Troubleshooting guide:** [docs/guides/gamefeature_setup.md](../guides/gamefeature_setup.md)

---

## Naming Conventions

### Modules
**Pattern:** `Project<Name>` prefix (avoid UE engine name clashes)

[OK] **CORRECT:**
- `ProjectMenuUI`
- `ProjectBoot`
- `ProjectLoading`
- `ProjectCore`

[X] **WRONG:**
- `MenuUI` (conflicts with UE module)
- `Boot` (too generic)
- `Loading` (conflicts with UE)

### GameFeature Plugins
**Pattern:** `Project<Name>` prefix, **NO "GF" suffix**

[OK] **CORRECT:**
- `ProjectMenuCore`
- `ProjectMenuExperience`
- `ProjectCombat`

[X] **WRONG (Deprecated):**
- `ProjectMenuCoreGF` (redundant suffix)
- `ProjectMenuExperienceGF` (redundant suffix)

**Reason:** `Plugins/GameFeatures/` folder already indicates type (DRY principle)

### C++ Classes

| Type | Prefix | Example |
|------|--------|---------|
| **UObject-derived** | `U` | `UProjectLoadingSubsystem` |
| **Actor-derived** | `A` | `AProjectBootFlowController` |
| **Struct** | `F` | `FLoadRequest`, `FLoadPhaseState` |
| **Enum** | `E` | `EAlisLoginType`, `ELoadPhase` |
| **Interface** | `I` | `ILoadingHandle` |

[OK] **Examples:**
```cpp
class UProjectBootSubsystem : public UGameInstanceSubsystem { };
class AProjectBootFlowController : public AActor { };
struct FLoadRequest { };
enum class ELoadPhase : uint8 { };
class ILoadingHandle { };
```

### API Export Macros

| Module | Macro | Case |
|--------|-------|------|
| ProjectCore | `PROJECTCORE_API` | UPPERCASE |
| ProjectBoot | `PROJECTBOOT_API` | UPPERCASE |
| ProjectMenuUI | `PROJECTMENUUI_API` | UPPERCASE |
| Alis | `ALIS_API` | UPPERCASE |

**Rule:** ALWAYS UPPERCASE (UE linker requirement)

---

## Code Style

### Blueprint Integration
**Expose functions and properties** for Blueprint access:

```cpp
// Blueprint-callable function
UFUNCTION(BlueprintCallable, Category = "ProjectLoading")
void StartLoad(const FLoadRequest& Request);

// Blueprint-pure function (no side effects)
UFUNCTION(BlueprintPure, Category = "ProjectLoading")
bool IsLoading() const;

// Blueprint read-write property
UPROPERTY(BlueprintReadWrite, Category = "ProjectLoading")
float LoadingProgress;

// Blueprint read-only property
UPROPERTY(BlueprintReadOnly, Category = "ProjectLoading")
FString CurrentPhase;

// ? Avoid: Default delegate parameters in UFUNCTIONs (UHT parser failure)
// WRONG: void Func(FOnComplete Delegate = FOnComplete());
// RIGHT: void Func(FOnComplete Delegate); + Use AdvancedDisplay or C++ overload
```

### Critical Blueprint Rules

**1. Private Property Access:**
`BlueprintReadOnly/ReadWrite` properties MUST be `protected` or `public`. UHT forbids exposing `private` members to Blueprints (child BP classes cannot access them).

**2. UHT Delegate Defaults:**
Do NOT enforce default values for Delegate parameters in `UFUNCTION`s. It crashes UHT. Use `Meta = (AdvancedDisplay="DelegateParam")` instead.
```

**Categories:** Follow existing patterns:
- `"ProjectLoading"`, `"ProjectBoot"`, `"ProjectSession"`
- `"AlisGI|Registration"`, `"Alis CPP Library"`

### Commenting Standard

**High-signal comments** when behavior is not obvious:

[OK] **GOOD:**
```cpp
// Consumed by ProjectLoadingSubsystem during ResolveAssets phase
UFUNCTION(BlueprintCallable)
TArray<FName> GetRequiredContentPacks() const;

// TODO(Phase 6): Replace with ProjectSave integration (see todo/create_architecture.md)
void SaveMilestone(const FProjectSessionMilestone& Milestone);
```

[X] **AVOID (noisy):**
```cpp
// Gets the loading progress
float GetLoadingProgress() const;  // Obvious from name
```

**Rules:**
- Reference consuming system when not obvious
- Document deferred TODOs with roadmap references
- Avoid redundant/obvious comments
- Ensure future agents can understand data-driven flows

### Server-Only Code Pattern

**Guard server-only code** with `WITH_SERVER_CODE`:

```cpp
// AlisServer.h
#if WITH_SERVER_CODE
class ALIS_API UAlisServer : public UBlueprintFunctionLibrary
{
    UFUNCTION(BlueprintCallable, Category = "Alis Server")
    static bool ConnectToDatabase();

    // Database credentials (NEVER in client builds)
    static const FString DB_HOST;
    static const FString DB_PASSWORD;
};
#endif // WITH_SERVER_CODE
```

**Benefits:**
- Client builds exclude server code (smaller binary)
- Protects server credentials from client exposure
- Enables server-specific optimizations

**Targets:**
- `AlisServer` - WITH_SERVER_CODE = 1
- `AlisClient` - WITH_SERVER_CODE = 0
- `Alis` - WITH_SERVER_CODE = 1
- `AlisEditor` - WITH_SERVER_CODE = 1

---

## Scripts Architecture (SOLID)

**Structure:**
```
scripts/
|-- config/          # Single source of truth (ue_path.conf)
|-- ue/
|   |-- build/       # Build operations ONLY
|   |-- check/       # Validation WITHOUT build
|   |-- test/        # Testing (smoke/integration/unit)
|   |-- run/         # Runtime operations
|-- ci/              # CI/CD orchestration
|-- utils/           # General utilities
```

### Decision Tree: Where Does This Script Belong?

```
START: I need to create/move a script

|-- Does it BUILD C++ code?
|  |-- scripts/ue/build/

|-- Does it VALIDATE without building?
|  |-- scripts/ue/check/

|-- Does it TEST functionality?
|  |-- Critical path, <5 min? -> scripts/ue/test/smoke/
|  |-- Multi-system, 5-30 min? -> scripts/ue/test/integration/
|  |-- Single system, <1 min? -> scripts/ue/test/unit/

|-- Does it RUN the game/editor?
|  |-- scripts/ue/run/

|-- Does it ORCHESTRATE CI/CD?
|  |-- scripts/ci/

|-- General utility?
   |-- scripts/utils/
```

### Naming Rules
- **Lowercase with underscores:** `boot_test.bat`, `validate_uht.bat`
- **Descriptive:** `gamefeature_registration.bat`, not `test1.bat`
- **Platform extension:** `.bat` (Windows), `.sh` (Linux/macOS), `.ps1` (PowerShell)

**Full guide:** [docs/helpers.md](../helpers.md)

---

## UI Conventions

### MVVM Architecture
**Pattern:** C++ base + BP cosmetics

**Rule:** UI logic in C++, Blueprint for presentation only

[OK] **CORRECT:**
```cpp
// Pure C++ ViewModel (zero UMG dependencies)
UCLASS()
class PROJECTMENUUI_API UProjectMapListViewModel : public UProjectViewModel
{
    VIEWMODEL_PROPERTY(TArray<FProjectMapEntry>, MapList)

    UFUNCTION(BlueprintCallable)
    void RefreshMapList();  // Logic in C++
};
```

```
// Blueprint widget (presentation only)
W_MapBrowser (inherits UProjectUserWidget)
  - Binds to UProjectMapListViewModel
  - Displays MapList in UI
  - Calls RefreshMapList() on button click
```

### Widget Naming
- **C++ widgets:** `UProjectUserWidget`, `UProjectButton`, `UProjectProgressBar`
- **Blueprint widgets:** `W_MainMenu`, `W_LoadingScreen`, `W_MapBrowser`
- **Widget components:** `WBP_ButtonStyle`, `WBP_TextStyle` (if needed for cosmetics)

**Details:** [docs/ui/README.md](../ui/README.md), [docs/verification/widget_naming.md](../verification/widget_naming.md)

---

## Configuration Files

### Central UE Path Config
**Single source of truth:** `scripts/config/ue_path.conf`

```ini
# scripts/config/ue_path.conf
UE_PATH="<ue-path>"
```

**All scripts auto-detect from:**
1. `scripts/config/ue_path.conf` (if exists)
2. Windows Registry
3. Common installation paths

**Benefits:**
- Update UE version in ONE place
- All scripts use this automatically
- No hardcoded paths in scripts

### Engine Configuration
**Location:** `Config/DefaultEngine.ini`

**Key settings:**
- Rendering: Lumen, Nanite, Ray Tracing enabled
- Physics channels: Grass, Concrete, Wood
- Custom trace channels: HighlightLT, InterLT, Indoor
- Target FPS: 22-120 (unclamped)

### Game Configuration
**Location:** `Config/DefaultGame.ini`

**Key settings:**
- Editor startup map: `/Game/Project/Maps/KazanMain/KazanMain`
- Default game map: `/Game/Project/Maps/Map_Main_Menu`
- Game mode: `/Game/BP/AlisGameMode.AlisGameMode_C`
- Localization: Russian (primary), English

---

## Module Dependencies

**Defined in:** `Source/<Module>/<Module>.Build.cs`

**Common dependencies:**
```csharp
// Alis.Build.cs
PublicDependencyModuleNames.AddRange(new string[] {
    "Core",
    "CoreUObject",
    "Engine",
    "InputCore",
    "UMG",           // UI system
    "Slate",         // Low-level UI
    "SlateCore",
    "HTTP",          // Web requests
    "Sockets",       // Networking
    "Networking",
    "Icmp",          // Ping
    "OpenSSL",       // Cryptography
});

PrivateDependencyModuleNames.AddRange(new string[] {
    "LowEntryExtendedStandardLibrary",  // SHA-512 hashing
});
```

**Plugin modules:**
```csharp
// ProjectMenuUI.Build.cs
PublicDependencyModuleNames.AddRange(new string[] {
    "Core",
    "CoreUObject",
    "Engine",
    "ProjectCore",    // Foundation
    "ProjectUI",      // MVVM base classes
    "ProjectLoading", // Loading subsystem
    "ProjectData",    // Manifests
});

### Critical Dependency Rules

**1. UMG is a MODULE, not a Plugin:**
- **Add to:** `.Build.cs` (`PublicDependencyModuleNames`)
- **Do NOT add to:** `.uplugin` (It is a built-in engine module)
- *Error:* `Unable to find plugin 'UMG'`

**2. Transitive Dependencies:**
- If Module A depends on Module B...
- AND Module B belongs to "Plugin B"...
- THEN "Plugin A" MUST list "Plugin B" in its `.uplugin` file.
- *Error:* `Plugin 'A' does not list plugin 'B' as a dependency`
```

---

## Client-Only Module Pattern

Use this pattern when a plugin has both runtime logic and client UI/input code:

- `Project<Name>` (Runtime) - server-safe logic, facades, config.
- `Project<Name>Client` (ClientOnlyNoCommandlet) - UI, input, FX.

Rules:
- Runtime module must not include UI headers or UI modules.
- Client module can depend on UI plugins and EnhancedInput.
- Runtime module references client classes via soft class paths or interfaces.

Example (ProjectSinglePlay):
- `ProjectSinglePlay` - GameMode, mode config, feature orchestration.
- `ProjectSinglePlayClient` - PlayerController, UI bindings.

---

## UE-Specific Workflows

### Adding New C++ Classes
1. Add header to `Source/Alis/Public/`, implementation to `Source/Alis/Private/`
2. Update `Alis.Build.cs` if new module dependencies required
3. Regenerate project files: right-click `Alis.uproject` -> "Generate Visual Studio project files"
4. Build the module in Visual Studio
5. Restart Unreal Editor to load new code

### Server-Side Development
- Use `UAlisServer` static methods for server-only functionality
- Wrap implementation in `#if WITH_SERVER_CODE` guards
- Test with AlisServer build target to ensure server-only builds work

### Blueprint Integration
- Expose C++ functions: `UFUNCTION(BlueprintCallable)` or `BlueprintPure`
- Expose properties: `UPROPERTY(BlueprintReadWrite)` or `BlueprintReadOnly`
- Use Category specifier for organization: `UFUNCTION(Category = "Alis|Network")`
- Follow existing categories: `"AlisGI|Registration"`, `"Alis CPP Library"`

### UI Data Assets (CRITICAL)
- **DO NOT create .uasset files directly** - agents cannot create binary UE assets
- **DO edit C++ constructors:** `UDA_DefaultTheme`, `UDA_MainMenuLayout`, `UDA_FadeTransition`
- Theme/layout/transition assets are C++ classes in `Plugins/Core/ProjectUI/Source/ProjectUI/Public/DataAssets/`
- Designers inherit from C++ classes in Blueprint for cosmetic overrides only
- Follow "C++ base + BP cosmetics" rule (see [docs/ui/README.md](../ui/README.md))

---

## Quick Reference

**GameFeature requirements:** 4 critical rules (asset name, location, .uplugin path, API macro)
**Naming:** `Project` prefix, NO "GF" suffix, UPPERCASE API macros
**Code style:** Blueprint integration, high-signal comments, server-only guards
**Scripts:** SOLID structure, decision tree, lowercase_underscore naming
**UI:** MVVM (C++ logic, BP presentation), widget naming (W_ prefix)
**Config:** Central UE path (`scripts/config/ue_path.conf`)

**Deep dives:**
- GameFeature troubleshooting: [docs/guides/gamefeature_setup.md](../guides/gamefeature_setup.md)
- Core principles: [docs/architecture/core_principles.md](../architecture/core_principles.md)
- Scripts architecture: [scripts/docs/architecture.md](../scripts/docs/architecture.md)
- UI system: [docs/ui/README.md](../ui/README.md)
