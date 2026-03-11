# Core Architecture Principles

**Purpose:** Define universal architectural principles that apply to any UE5 project, not just Alis.

---

## Universal Naming Convention
 
 > **Concrete Rules:** See [conventions.md](conventions.md) for approved prefixes, case rules, and examples.
 
 ### Philosophy: Project Agnosticism
 
 Don't hardcode project names into architecture components. Use a generic prefix (e.g., `ProjectCore` instead of `AlisCore`) to ensure:
 1. **Portability** - Systems can be lifted into other projects.
 2. **Clarity** - Distinguishes project-specific code from engine code.
 3. **Modularity** - Easier to rename or fork.
 
 ---

## Project Branding Layer

### Where Project Names Belong

[OK] **Use project-specific names for:**
- Game content (maps, characters, items)
- Marketing materials
- User-facing text
- Configuration files
- Content-only plugins

### Examples

**Good:**
```
Content/Alis/                  # Content folder
Plugins/ContentPacks/Kazan/    # Map-specific content
Config/Alis_DefaultGame.ini    # Project configs
```

**Bad:**
```
Plugins/Core/AlisCore/         # [X] Architecture shouldn't be branded
Source/AlisLoading/            # [X] System code shouldn't be branded
FAlisLoadRequest               # [X] Data types shouldn't be branded
```

---

## Abstraction Principle

### Core Systems = Universal

Foundation systems should work in **any** UE5 game:
- Loading pipeline -> Works for FPS, RPG, racing game
- Session management -> Works for any multiplayer game
- Feature registry -> Works for any modular architecture
- Save system -> Works for any persistence needs

### Game Logic = Specific

Gameplay systems are naturally project-specific:
- `Plugins/Features/CombatSystem/` - Specific to this game's combat
- `Plugins/Features/DialogueSystem/` - Specific to this game's NPCs
- `Plugins/ContentPacks/Kazan/` - Specific location

---

## Loose Coupling & Event-Driven Architecture

**Principle:** Modules should communicate through events and interfaces, NOT direct dependencies.

### The Black Box Pattern

Each module is a **black box** that:
- **Knows:** Its own responsibilities and public interfaces
- **Doesn't know:** Implementation details of other modules
- **Communicates:** Via events, delegates, and subsystem interfaces

```
+--------------+  Event   +--------------+  Interface  +--------------+
||   Module A   | ------->|   Module B   | ----------> |   Module C   ||
|  (Producer)  |          |  (Observer)  |             |  (Consumer)  |
+----------------------+          +----------------------+             +----------------------+
     |                          |                            |
     |                          |                            |
     \___ Fires event           \___ Receives event         \___ Called via interface
          No knowledge               Knows event type             No knowledge of A or B
          of B or C                  Doesn't know C's impl        Only knows interface
```

### Timing-Independent Design

**Problem:** Modules load in unpredictable order. You can't assume dependencies are ready.

**Solution:** Use deferred initialization with engine delegates.

#### [X] Bad Example: Assumes Load Order

```cpp
class FMyModule : public IModuleInterface
{
    virtual void StartupModule() override
    {
        // [X] WRONG: Assumes GEngine exists
        if (UGameFeaturesSubsystem* GF = GEngine->GetEngineSubsystem<UGameFeaturesSubsystem>())
        {
            GF->AddObserver(Observer);  // Crashes if GEngine not ready!
        }
    }
};
```

#### [OK] Good Example: Event-Driven Init

```cpp
class FMyModule : public IModuleInterface
{
    virtual void StartupModule() override
    {
        // [OK] CORRECT: Defer until engine ready
        FCoreDelegates::OnPostEngineInit.AddRaw(this, &FMyModule::OnEngineReady);
    }

    void OnEngineReady()
    {
        // Now safe to access GEngine and subsystems
        if (UGameFeaturesSubsystem* GF = GEngine->GetEngineSubsystem<UGameFeaturesSubsystem>())
        {
            GF->AddObserver(Observer);
        }
    }

    virtual void ShutdownModule() override
    {
        // Clean up delegate
        FCoreDelegates::OnPostEngineInit.RemoveAll(this);
    }

private:
    FDelegateHandle EngineInitHandle;
};
```

### Common Timing Patterns

**1. Engine Initialization:**
```cpp
FCoreDelegates::OnPostEngineInit.AddRaw(this, &FMyClass::OnEngineReady);
```

**2. World Creation:**
```cpp
FWorldDelegates::OnPostWorldInitialization.AddRaw(this, &FMyClass::OnWorldCreated);
```

**3. Feature Module Lifecycle (via IProjectFeatureModule):**
```cpp
// Feature modules implement IProjectFeatureModule interface
virtual void OnFeatureActivating(const FOrchestratorContext& Context) override
{
    // Called BEFORE feature module completes initialization
    // Subsystems are available, world may be available
}

virtual void OnFeatureActivated(const FOrchestratorContext& Context) override
{
    // Called AFTER feature module initialization completes
    // All dependencies have been loaded
}
```

**4. GameInstance/Subsystem Dependencies:**
```cpp
// In subsystem
virtual void Initialize(FSubsystemCollectionBase& Collection) override
{
    Super::Initialize(Collection);

    // Safe to access other subsystems via GetGameInstance()->GetSubsystem<>()
    // Use null checks - other subsystems might not be loaded yet
}
```

### Interface-Based Communication

**Instead of:** Direct class references
**Use:** Interface contracts

```cpp
// [X] Bad: Tight coupling
class UMenuWidget : public UUserWidget
{
    UPROPERTY()
    UProjectLoadingSubsystem* LoadingSubsystem;  // Direct dependency!

    void OnPlayClicked()
    {
        LoadingSubsystem->StartLoad(Request);  // Knows implementation
    }
};

// [OK] Good: Loose coupling via interface
class UMenuWidget : public UUserWidget
{
    void OnPlayClicked()
    {
        // Resolve via service locator or subsystem query
        if (UGameInstance* GI = GetGameInstance())
        {
            if (ILoadingInterface* Loading = Cast<ILoadingInterface>(GI->GetSubsystem<UProjectLoadingSubsystem>()))
            {
                Loading->RequestLoad(Request);  // Talks to interface only
            }
        }
    }
};
```

### Feature Module Pattern with Orchestrator

**Real Example:** Menu experience feature module

```cpp
/**
 * Feature module implements IProjectFeatureModule interface.
 * Does NOT know:
 * - What UI will spawn
 * - How experience loading works
 * - When/where it will be called
 *
 * ONLY knows:
 * - Module interface (IProjectFeatureModule)
 * - Subsystem interface (UProjectExperienceSubsystem)
 * - Data contract (experience ID)
 */
class FMenuExperienceModule : public IModuleInterface, public IProjectFeatureModule
{
    virtual void OnFeatureActivating(const FOrchestratorContext& Context) override
    {
        // Check if this is our feature
        if (IsMenuFeature(Context.FeatureName))
        {
            // Call subsystem via interface - don't know what it does
            if (UProjectExperienceSubsystem* Experience = GetExperienceSubsystem())
            {
                Experience->LoadExperience(FPrimaryAssetId("ProjectExperience", "Menu"));
            }
        }
    }
};
```

**Benefits:**
- [OK] UMenuExperienceObserver doesn't know about UI widgets
- [OK] `UProjectExperienceSubsystem` doesn't know about GameFeatures
- [OK] Either can be replaced without affecting the other
- [OK] Testing is easy - mock the interface

### Module Load Order Independence

**Rule:** Never assume module X loads before module Y.

**Pattern:** Use service locator with null checks:

```cpp
void FMyModule::DoWork()
{
    // [OK] Good: Resolve dependency lazily
    if (const TSharedPtr<FMyService> Service = FProjectServiceLocator::Resolve<FMyService>())
    {
        Service->DoSomething();
    }
    else
    {
        UE_LOG(LogMyModule, Warning, TEXT("Service not available yet - deferring work"));
        // Queue work for later or skip gracefully
    }
}
```

### Documentation Requirements

**When creating event-driven systems, document:**

1. **Event timing:** When does this fire relative to engine/world/subsystem init?
2. **Dependencies:** What must exist before this event handler runs?
3. **Guarantees:** What can you safely assume when this callback fires?
4. **Order:** If multiple observers exist, what order do they fire in?

**Example:**

```cpp
/**
 * OnGameFeatureActivating timing guarantees:
 *
 * BEFORE callback:
 * - Plugin mounted
 * - Module loaded (if BuiltInAutoLoad=true)
 * - StartupModule() called
 *
 * DURING callback:
 * - GEngine available
 * - Subsystems accessible
 * - World may or may not exist (check IsValid())
 *
 * AFTER callback:
 * - Feature transitions to Active state
 * - GameFeatureActions execute
 *
 * Agent Note: Use OnGameFeatureActivating (NOT OnGameFeatureActivated) if you need to
 * prepare systems before the feature goes fully active.
 */
```

---

## Check UE Built-In Solutions First

**Before implementing any new system, module, or feature, investigate if Unreal Engine already provides a built-in solution.**

### Investigation Process

1. **Check Engine Plugins** (`Engine/Plugins/`)
   - Search for relevant plugins in `Runtime/`, `Experimental/`, `Developer/`
   - Example: ChunkDownloader, GameFeatures, ModularGameplay, EnhancedInput

2. **Read Documentation** (Epic Developer Documentation)
   - Search Epic's docs for your use case
   - Check version-specific docs (5.5, 5.6, etc.)
   - Look for "Best Practices" and "How To" guides

3. **Examine Engine Source**
   - Engine subsystems (`Engine/Source/Runtime/`)
   - Asset Manager integration
   - Lyra/CitySample reference projects

4. **Evaluate Before Building Custom**
   - **Use built-in if**: 80%+ feature match, well-documented, actively maintained
   - **Wrap built-in if**: Good core but needs project-specific abstraction
   - **Build custom only if**: No built-in exists or built-in is fundamentally incompatible

### Examples

**[OK] Good - Uses Built-In:**
```cpp
// Phase 7: Content Packs
// Found: UE has ChunkDownloader plugin for PAK mounting
// Decision: Use ChunkDownloader, wrap with ProjectContentPacks for manifest integration
#include "ChunkDownloader.h"

class UProjectContentPackSubsystem : public UGameInstanceSubsystem
{
    // Wraps FChunkDownloader with project-specific manifest format
    TSharedPtr<FChunkDownloader> ChunkDownloader;
};
```

**Note:** Use this wrapper for ContentPacks/DLC. Do not use for GameFeature plugin updates; Orchestrator handles GameFeature downloads and IoStore mounting.

---

## Data-Driven Editing (Agent Workflow)

- Keep tunables, feature flags, matchmaking rules, and content metadata in text sources (INI/JSON/CSV) with runtime reload paths.
- Provide zero-restart hooks (`ReloadConfig`, live JSON reparse, CVAR overrides) so agents iterate without recook.
- Maintain the canonical structure outlined in [Data-Driven Agent Workflow](data_agentic_driven.md) for directory layout, reload commands, and security checks.

> Architecture owns reload/application logic; agents own the data files. Respect the split to stay fast and safe.

---

**[OK] Good - Custom Pattern with FModuleManager:**
```cpp
// Phase 4: Feature Modules
// Decision: Use FModuleManager with Orchestrator for feature loading
// Provides manifest-based dependency resolution and update management
#include "Modules/ModuleManager.h"

void FActivateFeaturesPhaseExecutor::Execute()
{
    UGameFeaturesSubsystem& GameFeatures = UGameFeaturesSubsystem::Get();
    GameFeatures.LoadAndActivateGameFeaturePlugin(PluginURL, Callback);
}
```

**Note:** Activation typically follows Orchestrator download/mount; Orchestrator decides code vs content path based on manifest hashes.

**[X] Bad - Reinvents Wheel:**
```cpp
// Custom chunk downloader when UE already provides one
class FCustomChunkDownloader
{
    // Reimplements HTTP download, PAK mounting, manifest parsing
    // Wastes time, bugs, maintenance burden
};
```

### Documentation Requirements

When using or wrapping UE built-in systems:

**1. Document the Investigation**
Add to architecture docs (`todo/create_architecture.md`):
```markdown
## Phase 7.1: Content Packs

**UE Built-In Investigation:**
- **Found**: ChunkDownloader plugin (`Engine/Plugins/Runtime/ChunkDownloader/`)
- **Capabilities**: HTTP download, PAK/IOStore mounting, build manifest, caching
- **Decision**: Use ChunkDownloader, wrap with ProjectContentPacks for:
  - Integration with ProjectData manifest system
  - Asset Manager primary asset type scanning
  - UI-friendly progress events
- **References**:
  - [ChunkDownloader Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-the-chunkdownloader-plugin-in-unreal-engine)
  - `Engine/Plugins/Runtime/ChunkDownloader/Source/Public/ChunkDownloader.h`
```

**2. Comment Wrapper Classes**
```cpp
/**
 * Wraps UE's built-in FChunkDownloader with project-specific manifest integration.
 *
 * UE Built-In: FChunkDownloader (Engine/Plugins/Runtime/ChunkDownloader/)
 * - Handles HTTP download, PAK mounting, caching
 * - Uses chunk IDs and pak file manifests
 *
 * This Wrapper Adds:
 * - Integration with UProjectContentPackManifest (ProjectData)
 * - Asset Manager primary asset type scanning
 * - Blueprint-friendly events (OnProgressChanged, OnPackReady)
 * - Automatic chunk priority based on manifest dependencies
 *
 * Agent Note: Do NOT reimplement chunk downloading logic. FChunkDownloader
 * is production-tested and handles edge cases (resume, retry, validation).
 * Only add thin abstraction layer for project-specific needs.
 */
class PROJECTCONTENTPACKS_API UProjectContentPackSubsystem : public UGameInstanceSubsystem
{
    // ...
};
```

**3. Add to Core Principles (this document)**
- Keep investigation notes
- Link to UE documentation
- Explain why wrapper was needed (or why built-in wasn't used)

### Benefits

**Time Savings:**
- Don't reinvent HTTP download, PAK mounting, async loading
- Leverage Epic's production-tested code
- Avoid debugging edge cases Epic already solved

**Maintenance:**
- Epic maintains and updates built-in systems
- Bug fixes and optimizations come free with engine updates
- Better integration with future UE features

**Documentation:**
- Epic provides official documentation
- Community knowledge and examples
- Easier onboarding for new team members

### Common UE Built-Ins to Check

| Feature | UE Built-In | Location |
|---------|-------------|----------|
| Content Packs/DLC | ChunkDownloader | `Engine/Plugins/Runtime/ChunkDownloader/` |
| Feature Modules | FModuleManager + Custom Orchestrator | `Engine/Source/Runtime/Core/` + `Plugins/Orchestrator/` |
| Save System | SaveGame API | `Engine/Source/Runtime/Engine/` |
| Input System | EnhancedInput | `Engine/Plugins/Runtime/EnhancedInput/` |
| UI Framework | CommonUI | `Engine/Plugins/Runtime/CommonUI/` |
| Asset Management | AssetManager | `Engine/Source/Runtime/Engine/` |
| Modular Gameplay | ModularGameplay | `Engine/Plugins/Runtime/ModularGameplay/` |
| Online Subsystem | OnlineSubsystem | `Engine/Plugins/Online/` |
| HTTP Requests | HTTP Module | `Engine/Source/Runtime/Online/HTTP/` |
| Analytics | Analytics | `Engine/Source/Runtime/Analytics/` |

### Investigation Checklist

Before implementing any new system:

- [ ] Searched `Engine/Plugins/` for related plugins
- [ ] Read Epic's documentation for this feature area
- [ ] Checked Lyra/CitySample for Epic's recommended approach
- [ ] Evaluated if built-in covers 80%+ of requirements
- [ ] Documented investigation findings in architecture docs
- [ ] If using built-in: Added wrapper class with clear comments
- [ ] If building custom: Documented why built-in wasn't suitable

---

## GameInstance Architecture

**Modern UE5 Best Practice: Use Default `UGameInstance` + Modular Subsystems**

### [OK] Recommended Pattern (Lyra-Style)

**Use engine's default `UGameInstance` with all logic in `UGameInstanceSubsystem` classes:**

```ini
# Config/DefaultEngine.ini
[/Script/EngineSettings.GameMapsSettings]
; Use default UGameInstance - all logic lives in GameInstanceSubsystems
; This works universally (PIE, Standalone, Packaged) and follows Lyra best practices
GameInstanceClass=/Script/Engine.GameInstance
```

```cpp
// Your game logic lives in subsystems, NOT in GameInstance
UCLASS()
class UProjectBootSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    // ... your logic here
};
```

### Why This Is Best

**1. Modularity**
- Each subsystem = one responsibility
- Easy to add/remove/test individual systems
- Plugins can contribute subsystems without touching core code

**2. Universal Support**
- Works in PIE, Standalone Game, Packaged builds, Dedicated Server
- No Blueprint initialization issues in `-game` mode
- UE automatically manages lifetime and initialization order

**3. Zero Boilerplate**
- Don't write custom `Init()`, `Shutdown()`, or constructors
- UE handles initialization order via plugin dependencies
- Subsystems can resolve dependencies via `FProjectServiceLocator`

**4. Industry Standard**
- **Lyra** uses default `UGameInstance` with subsystems (`UCommonUserSubsystem`, experience manager, etc.)
- **Epic's recommendation** for UE5 modular architecture
- Better hot-reload support (subsystems reinitialize, GameInstance doesn't)

### Example Subsystem Architecture

```
+-------------------------------------------------+
|   UGameInstance (Engine Default)                |
|                                                 |
|  Auto-managed subsystems:                      |
|  +------------------------------------------+ |
|  | UProjectBootSubsystem                    | |  Boot flow
|  | UProjectLoadingSubsystem                 | |  Loading pipeline
|  | UProjectSessionSubsystem                 | |  Session tracking
|  | UProjectContentPackSubsystem             | |  Content packs
|  | UProjectFeatureActivationSubsystem       | |  Feature plugins
|  | UProjectFeatureRegistrySubsystem         | |  Feature discovery
|  |                                          | |
|  | Future (add as needed):                  | |
|  | UProjectExperienceSubsystem              | |  Lyra-style experiences
|  | UProjectOnlineSubsystem                  | |  Multiplayer
|  | UProjectSaveSubsystem                    | |  Save/load
|  | UProjectAnalyticsSubsystem               | |  Telemetry
|  +------------------------------------------+ |
+-------------------------------------------------+
```

### [X] What NOT to Do

**Don't create monolithic custom GameInstance:**

```cpp
// [X] BAD - Monolithic GameInstance
class UAlisGameInstance : public UGameInstance
{
    void HandleBooting();
    void HandleLoading();
    void HandleSessions();
    void HandleContentPacks();
    void HandleFeatures();
    // ... 1000+ lines of mixed responsibilities
};
```

**Don't use Blueprint GameInstance:**

```ini
# [X] BAD - Blueprint GameInstance doesn't initialize in standalone -game mode
GameInstanceClass=/Game/Project/Core/System/BP_AlisGInstance.BP_AlisGInstance_C
```

### When You Might Need Custom GameInstance

**Rare cases only:**

1. **Overriding virtual functions** - e.g., custom `HandleNetworkError()`, `OnStart()`
2. **Very core initialization** - Pre-subsystem setup (almost never needed)

**In these cases, keep it minimal:**

```cpp
// Minimal custom GameInstance (if absolutely needed)
UCLASS()
class UProjectGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    virtual void HandleNetworkError(UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString) override;

    // That's it! All other logic goes in subsystems
};
```

### Migration from Custom GameInstance

**If you have existing custom GameInstance code:**

| Old Location (GameInstance) | New Location (Subsystem) |
|-----------------------------|--------------------------|
| Boot logic | `UProjectBootSubsystem` |
| Loading logic | `UProjectLoadingSubsystem` |
| Session tracking | `UProjectSessionSubsystem` |
| Network/online code | `UProjectOnlineSubsystem` |
| Server-specific code | `UGameModeBase` (server authority) or subsystem with `#if WITH_SERVER_CODE` |

**Agent Note:** When migrating, don't just copy-paste code into subsystems. Each subsystem should have a **single responsibility**. If your GameInstance has 10 different responsibilities, create 10 subsystems.

### Testing Across Execution Contexts

**Your boot flow and subsystems must work universally:**

- [OK] PIE (Play In Editor) - Editor gameplay testing
- [OK] Standalone Game - `UnrealEditor.exe Project.uproject -game`
- [OK] Packaged Build - Cooked game executable
- [OK] Dedicated Server - `ProjectServer.exe` with `#if WITH_SERVER_CODE`

**Test all three contexts** to ensure subsystems initialize correctly. Default `UGameInstance` guarantees this; Blueprint GameInstances do not.

### Documentation

- **[Lyra Sample](https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-sample-game-in-unreal-engine)** - Epic's reference implementation
- **[Programming Subsystems](https://dev.epicgames.com/documentation/en-us/unreal-engine/programming-subsystems-in-unreal-engine)** - Official UE5 docs
- **[Plugin Architecture](plugin_architecture.md)** - This project's subsystem organization

---

## Data-Driven First

- Prefer configuration (INI, data assets, developer settings) over manual Blueprint/UAsset wiring.
- Runtime systems should discover behavior via data, not hard references or hand-placed actors.
- If an editor asset is required, keep it engine-supplied or auto-generated; customize via config, not by duplicating `.umap` files.
- Example: `ProjectBoot` plugin loads the engine `Minimal_Default` map and spawns UI from settings-teams only edit `/Script/ProjectBoot.ProjectBootSettings` entries.
- Reserve manual Blueprint changes for cosmetic tweaks when no data-driven alternative exists; document them and isolate in content-only plugins.

---

## Naming Patterns

### Plugins

| Category | Pattern | Example | Notes |
|----------|---------|---------|-------|
| Core | `[Function]` | `ProjectCore`, `ProjectLoading`, `ProjectSession` | Generic system name |
| Features | `[Feature]System` or `[Feature]Manager` | `CombatSystem`, `QuestManager` | Describes purpose |
| UI | `[Component]UI` | `MenuUI`, `HUDUI` | Clear UI designation |
| Data | `[Type]Manager` | `SaveManager`, `Analytics` | Data handling |
| Content | `[Location]` or `[Theme]` | `Kazan`, `UrbanAssets` | Content-specific OK |

### Types (Structs/Classes)

| Type | Pattern | Example | Notes |
|------|---------|---------|-------|
| Structs | `F[Descriptor][Type]` | `FProjectLoadRequest`, `FSessionInfo` | No project prefix |
| Enums | `E[Descriptor]` | `EProjectLoadMode`, `EPhaseState` | Descriptive enum |
| Interfaces | `I[Capability]` | `IProjectLoadingHandle`, `IActivatable` | Interface capability |
| Subsystems | `U[Domain]Subsystem` | `UProjectLoadingSubsystem` | Standard UE pattern |
| Actors | `A[Type]` | `ASessionBeacon`, `ALoadingManager` | Standard UE prefix |

### Logging

| Category | Pattern | Example |
|----------|---------|---------|
| Log declaration | `LogProjectCore`, `LogProjectLoading` | `DECLARE_LOG_CATEGORY_EXTERN(LogProjectCore, Log, All);` |
| Macro usage | `UE_LOG(Log[Category], ...)` | `UE_LOG(LogProjectLoading, Display, TEXT("..."));` |

### Macros (Optional Project Prefix)

**If you want project-specific macros for convenience:**

```cpp
// Option 1: Generic (preferred)
#define LOADING_LOG(Verbosity, Format, ...) \
    UE_LOG(LogProjectLoading, Verbosity, TEXT(Format), ##__VA_ARGS__)

// Option 2: Project-branded (acceptable for macros only)
#define ALIS_LOG(Category, Verbosity, Format, ...) \
    UE_LOG(Log##Category, Verbosity, TEXT("[Alis::%s] " Format), *FString(__FUNCTION__), ##__VA_ARGS__)
```

**Rationale:** Macros are preprocessor-level and don't pollute class/namespace structure. A branded macro is acceptable if it aids debugging (e.g., prefixes logs with project name).

---

## Module Organization
 
 > **Concrete Hierarchy:** See [plugin_rules.md](plugin_rules.md) for the authoritative folder structure and dependency graph.
 
 ### Universal Hierarchy Principles
 
 1. **Core (Foundation):** Systems that would exist in *any* game (Loading, Session, User).
 2. **Systems (Gameplay Framework):** Reusable but game-specific mechanics (Combat, Inventory).
 3. **World:** Geography and maps. Depends on Systems.
 4. **UI:** Presentation layer. Depends on MVVM logic in plugins, never flow logic.
 5. **Content:** Assets only. Validated by manifests.
 
 ---

## File Naming

### Headers

```cpp
// [OK] Good (generic)
Core.h
Loading.h
LoadRequest.h
LoadingHandle.h
SessionInfo.h

// [X] Bad (project-specific)
AlisCore.h
AlisLoading.h
AlisLoadRequest.h
```

### Modules

```cpp
// [OK] Good (module names in .Build.cs)
public class Core : ModuleRules { }
public class Loading : ModuleRules { }
public class SessionManager : ModuleRules { }

// [X] Bad
public class AlisCore : ModuleRules { }
public class AlisLoading : ModuleRules { }
```

### Plugin Descriptors

```json
// [OK] Good (.uplugin)
{
    "FriendlyName": "Core Foundation",
    "Description": "Core interfaces and types for modular game architecture",
    "Category": "Framework.Core"
}

// [X] Bad
{
    "FriendlyName": "Alis Core",
    "Description": "Core module for the Alis project",
    "Category": "Alis.Core"
}
```

---

## Documentation Examples

### [OK] Generic Documentation

```markdown
# Loading Subsystem

The Loading subsystem manages asynchronous map transitions with a multi-phase pipeline.

## Usage

Create a load request:
```cpp
FProjectLoadRequest Request;
Request.MapAssetId = FPrimaryAssetId(TEXT("GameMap"), TEXT("Level_01"));
Request.LoadMode = EProjectLoadMode::SinglePlayer;
```
```

### [X] Project-Specific Documentation

```markdown
# Alis Loading Subsystem

The Alis loading system loads Kazan and Hrush maps for the Alis game.

## Usage in Alis

Create an Alis load request:
```cpp
FAlisLoadRequest Request;
Request.MapAssetId = FPrimaryAssetId(TEXT("AlisMap"), TEXT("KazanMain"));
```
```

---

## Configuration and Setup

### Project Setup (Where to Brand)

**In DefaultEngine.ini:**
```ini
[/Script/Engine.Engine]
GameInstanceClass=/Script/Alis.AlisGameInstance    # [OK] Project-specific OK

[/Script/Loading.LoadingSubsystem]                  # [OK] Generic system name
DefaultTransitionDuration=2.0
```

**In DefaultGame.ini:**
```ini
[/Script/EngineSettings.GeneralProjectSettings]
ProjectID=...
ProjectName=Alis                                    # [OK] Project name
CompanyName=ALIS

[/Script/Core.AssetManager]                         # [OK] Generic system
PrimaryAssetTypesToScan=(PrimaryAssetType="GameMap", ...)
```

---

## Migration Guide

If you have existing project-specific names, migrate gradually:

### Phase 1: Create Generic Versions
1. Create new plugin `ProjectCore` alongside `AlisCore`
2. Copy code, rename classes/types
3. Update includes in new code only

### Phase 2: Deprecate Old Names
1. Add `UE_DEPRECATED` macros to old classes
2. Create type aliases: `using FAlisLoadRequest = FProjectLoadRequest;`
3. Update documentation to recommend new names

### Phase 3: Remove Old Names
1. Delete deprecated plugins
2. Remove type aliases
3. Clean up documentation

### Example Migration

```cpp
// Old (deprecated)
USTRUCT(BlueprintType, meta=(DeprecatedNode, DeprecationMessage="Use FProjectLoadRequest instead"))
struct FAlisLoadRequest
{
    GENERATED_BODY()
    // ... old code
};

// New (preferred)
USTRUCT(BlueprintType)
struct FProjectLoadRequest
{
    GENERATED_BODY()
    // ... same functionality
};

// Compatibility alias (temporary)
using FAlisLoadRequest = FProjectLoadRequest;
```

---

## Benefits Summary

### For This Project
- [OK] Cleaner, more professional codebase
- [OK] Easier to understand for new team members
- [OK] Follows industry best practices

### For Future Projects
- [OK] Can reuse plugins without renaming
- [OK] Can extract as standalone framework
- [OK] Can share on marketplace/GitHub

### For Documentation
- [OK] Examples apply to any game
- [OK] No confusing project-specific jargon
- [OK] Can cite as reference architecture

---

## Exceptions

**When project-specific naming is OK:**

1. **Top-level game module** - `Source/Alis/` is fine (it's the game)
2. **Content assets** - `Content/Alis/` for organization
3. **Macros** - `ALIS_LOG()` macro is acceptable (preprocessor level)
4. **Config categories** - `[Alis.GameSettings]` for clarity
5. **Marketing/UI text** - User-facing strings
6. **Content plugins** - `Plugins/Content/Alis_Kazan/` (it's content, not code)

**Everything else should be generic.**

---

## Quick Reference

| Element | Use "Project" Prefix | Example | Can Use Project Name? |
|---------|---------------------|---------|----------------------|
| Core plugin | [OK] Yes | `ProjectCore` | [X] No |
| Loading plugin | [OK] Yes | `ProjectLoading` | [X] No |
| Subsystem | [OK] Yes | `UProjectLoadingSubsystem` | [X] No |
| Load request type | [OK] Yes | `FProjectLoadRequest` | [X] No |
| Log category | [OK] Yes | `LogProjectCore`, `LogProjectLoading` | [X] No |
| Game module | [X] No | `Alis` | [OK] Yes |
| Content folder | [X] No | `Content/Alis/` | [OK] Yes |
| Map name | [X] No | `Kazan_Main` | [OK] Yes |
| Content plugin | [X] No | `Alis_KazanPack` | [OK] Yes |
| Config section | [X] No | `[Alis.Settings]` | [OK] Yes |

---

## Plugin Documentation Requirements

**Every plugin MUST include a README.md file** in its root directory documenting:

### Required Sections

**1. Overview**
- Plugin purpose and key features
- What problem does it solve?
- When should developers use it?

**2. Architecture**
- Plugin structure (file/folder organization)
- Dependency graph showing relationships to other plugins
- Data flow diagrams (if applicable)

**3. Dependencies**
- Engine modules required
- Project plugins required (with explanation why)
- External dependencies
- **Explicitly state**: "No dependencies on [X]" for clarity

**4. Relationship to Other Plugins**
- Diagram showing how this plugin fits in the architecture
- Which plugins depend on this one?
- Which plugins does this one integrate with?
- Data/event flow between plugins

**5. API Documentation**
- Key classes/functions with usage examples
- Integration patterns and workflows
- Common use cases with code examples

**6. Agent Implementation Guide**
- **Agent Notes** throughout code explaining:
  - Design decisions and why they were made
  - Gotchas and common mistakes to avoid
  - Alternative approaches and trade-offs
  - Where to extend functionality
- Step-by-step implementation guides for common tasks
- Code comment standards (templates provided)

**7. Testing**
- Location of test module
- How to run tests
- Coverage expectations
- Link to main testing guide

**8. Documentation Links**
- Links to related docs (architecture, guides)
- Links to dependent plugin READMEs
- Links to integration examples

### Agent-Friendly Guidelines

**Inline Agent Notes in Code:**
```cpp
/**
 * [Function Description]
 *
 * @param Param - Description
 * @return Description
 *
 * Agent Note: [Implementation guidance - why this approach, alternatives, gotchas]
 */
```

**Example:**
```cpp
/**
 * Build FLoadRequest from selected map manifest
 *
 * @return Load request ready for ProjectLoadingSubsystem, or invalid request if no selection
 *
 * Agent Note: This builds the request with bSkipMountContent=true because Phase 7
 * (Content Packs) is not implemented yet. When implementing Phase 7, remove this flag
 * and implement content pack selection logic in MapListViewModel.
 */
FLoadRequest BuildLoadRequest() const;
```

**Relationship Diagrams:**

Include ASCII diagrams showing plugin relationships:
```
+-------------------------------------+
|     ProjectMenuUI (This Plugin)     |
|                                     |
|  +----------------------------+    |
|  |      ViewModels            |    |
|  |  (Pure C++ Logic)          |    |
|  +----------+-----------------+    |
+-------------+----------------------+
              | depends on
              v
   +----------------------+
   |    ProjectLoading    |
   |  (Loading Events)    |
   +----------------------+
```

**Integration Examples:**

Show complete workflows, not just isolated functions:
```cpp
// Example: Map Browser Flow
// 1. User opens map browser
MapListVM->RefreshMapList(); // Queries ProjectData manifests

// 2. User selects map
MapListVM->SetSelectedMapIndex(2);

// 3. User clicks "Load"
FLoadRequest Request = MapListVM->BuildLoadRequest();
LoadingSubsystem->StartLoad(Request); // Triggers ProjectLoading

// 4. Loading screen subscribes
LoadingVM->Initialize(GameInstance); // Auto-subscribes to events
```

### Example Plugin README Structure

```markdown
# [PluginName] Plugin

**[Brief Description]** - [Key Purpose]

## Overview
[What it does, key features, when to use]

## Architecture
[Structure diagram, file organization]

## Dependencies
**Engine Modules:**
- Module1 - Why needed

**Project Plugins:**
- Plugin1 - What integration (with link to Plugin1 README)
- Plugin2 - What data flow (with link to Plugin2 README)

**No dependencies on:**
- Thing1 (explicitly state what it doesn't depend on)

## Relationship to Other Plugins
[Dependency graph, data flow diagram]

## API Documentation
[Classes, functions, usage examples]

## Agent Implementation Guide
**When Creating [Feature]:**
1. Step with code example
2. Step with code example
   Agent Note: [Why this approach, alternatives]

## Testing
[Test location, how to run, link to testing guide]

## Documentation
**Primary:** [Links to main docs]
**Examples:** [Links to code examples]
```

### Plugin README Examples

See these complete examples:
- [ProjectUI README](../Plugins/Core/ProjectUI/README.md) - MVVM base classes, theme system
- [ProjectMenuUI README](../Plugins/UI/ProjectMenuUI/README.md) - ViewModels with integration
- [ProjectMenuUITests README](../Plugins/UI/ProjectMenuUI/Source/ProjectMenuUITests/README.md) - Testing guide

### Enforcement

**All new plugins MUST:**
- [ ] Include README.md in plugin root
- [ ] Document dependencies with relationship diagrams
- [ ] Include Agent Notes in code for design decisions
- [ ] Provide integration examples showing data flow
- [ ] Link to related plugin READMEs
- [ ] Document testing approach

**Pull requests will be rejected if:**
- Plugin has no README.md
- Dependencies are not documented
- No relationship diagram showing plugin architecture
- Agent implementation guidance is missing

---

## Checklist for New Code

Before creating a new plugin/module/type, ask:

- [ ] Could this be used in a different game?
  - **Yes** -> Use generic name
  - **No** -> Project-specific OK

- [ ] Is this core infrastructure or game logic?
  - **Infrastructure** -> Generic name required
  - **Game logic** -> Can be specific

- [ ] Will this be in documentation/tutorials?
  - **Yes** -> Use generic name for clarity
  - **No** -> Either is fine

- [ ] Could this be extracted as a library?
  - **Yes** -> Generic name essential
  - **No** -> Project-specific acceptable

- [ ] **Does plugin have README.md with:**
  - [ ] Dependency graph showing relationships
  - [ ] Agent implementation guide with inline notes
  - [ ] Integration examples with data flow
  - [ ] Links to related plugin documentation

---

**Last Updated:** 2025-10-17
**Applies To:** All new code going forward
**Legacy Code:** Migrate gradually as needed
