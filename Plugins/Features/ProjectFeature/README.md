# ProjectFeature

## Purpose

Minimal contracts for feature self-registration. Contains only:
- `FFeatureInitContext` - Context struct for feature initialization
- `FFeatureRegistry` - Global registry for feature registration and initialization

**Location:** `Plugins/Features/ProjectFeature` (Features tier - contracts)

**Category:** `Project.Features`

## Design Decision: Registration vs Initialization

**Why separate registration from initialization?**

```
REGISTRATION (module startup)          INITIALIZATION (GameMode controlled)
---------------------------------      ------------------------------------
- Happens when UE loads module         - Happens when GameMode decides
- Unordered (race conditions)          - Ordered by FeatureNames array
- Just stores lambda in registry       - Actually runs the lambda
- No pawn exists yet                   - Pawn exists, passed in context
- Feature says "here's my init"        - GameMode says "init now"
```

**GameMode controls:**
- **ORDER** - `FeatureNames` array defines init sequence (Combat before Inventory, etc.)
- **TIMING** - After pawn spawn (`HandleStartingNewPlayer`)
- **SELECTION** - Only configured features init, not every registered one

**Features own:**
- **WHAT to attach** - Components, subsystems, whatever they need
- **HOW to configure** - Parse `ConfigJson` from context
- **WHERE to attach** - Grab `Context.Pawn` and add their stuff

## Flow

```
1. Module loads (UE decides when)
   |
   v
2. Feature::StartupModule()
   -> FFeatureRegistry::RegisterFeature("Combat", lambda)
   -> Just registration, no init yet
   |
   v
3. GameMode::HandleStartingNewPlayer() (pawn now exists)
   |
   v
4. GameMode::InitializeFeatures(Pawn)
   -> Loop FeatureNames in ORDER
   -> FFeatureRegistry::InitializeFeature("Combat", Context)
   |
   v
5. Feature's lambda runs
   -> APawn* Pawn = Context.Pawn
   -> Attach components, parse config, etc.
```

## Dependency Structure

```
Foundation/ProjectCore (base types, service locator)
        |
        v
Features/ProjectFeature (contracts) <-- THIS PLUGIN
        ^
        |
+-------+-------+
|               |
Features/*      Gameplay/*
(Combat,        (orchestrates Features)
 Dialogue,          |
 Inventory)         v
                ProjectSinglePlay, ProjectOnlinePlay
```

**Key insight:** Feature plugins depend ONLY on `ProjectFeature`, not on Gameplay. Gameplay orchestrates Features but Features don't depend on Gameplay. This keeps features as self-contained black boxes.

## Core Contracts

### FFeatureInitContext

Context passed to feature init functions:
- `World` - UWorld pointer
- `GameMode` - AGameModeBase pointer
- `Pawn` - APawn pointer (feature attaches components here)
- `ModeName` - FName of the gameplay mode
- `ConfigJson` - JSON configuration string for this feature

### FFeatureRegistry

Static registry for feature registration:
- `RegisterFeature(Name, InitFn)` - Called by features on module startup (registration only)
- `InitializeFeature(Name, Context)` - Called by GameMode (actual init)
- `IsFeatureRegistered(Name)` - Query
- `GetRegisteredFeatures()` - List all

## Usage

### Feature Side (module StartupModule)

```cpp
#include "FeatureRegistry.h"
#include "FeatureInitContext.h"

void FProjectCombatModule::StartupModule()
{
    // REGISTRATION ONLY - no init happens here
    // GameMode will call this lambda later, in order, with pawn
    FFeatureRegistry::RegisterFeature(TEXT("Combat"), [](const FFeatureInitContext& Context)
    {
        // NOW we init - GameMode called us with context
        APawn* Pawn = Context.Pawn;

        // Feature owns what to attach
        UCombatComponent* Combat = NewObject<UCombatComponent>(Pawn);
        Pawn->AddInstanceComponent(Combat);
        Combat->RegisterComponent();

        // Feature owns how to configure
        if (!Context.ConfigJson.IsEmpty())
        {
            // Parse feature-specific config
        }
    });
}
```

### GameMode Side (orchestrator)

```cpp
#include "FeatureRegistry.h"
#include "FeatureInitContext.h"

void ASinglePlayerGameMode::InitializeFeatures(APawn* Pawn)
{
    // GameMode controls ORDER via FeatureNames array
    for (const FName& FeatureName : ModeConfig.FeatureNames)
    {
        FFeatureInitContext Context(
            GetWorld(),
            this,
            Pawn,  // Feature will attach to this
            ModeConfig.ModeName,
            ModeConfig.FeatureConfigs.FindRef(FeatureName)
        );

        // GameMode knows nothing about what feature does
        // Just calls registry, feature handles the rest
        FFeatureRegistry::InitializeFeature(FeatureName, Context);
    }
}
```

## Dependencies

- Core, CoreUObject, Engine (UE basics only)

## Non-responsibilities

- **No gameplay code** - Just contracts
- **No prescriptive init** - Features decide what "initialize" means
- **No framework classes** - Those live in ProjectGameplay
- **No order logic** - GameMode controls order via FeatureNames

## See Also

- `Plugins/Foundation/ProjectCore/README.md` - Foundation types and service locator
- `Plugins/Features/ProjectCombat/README.md` - Combat feature (uses this registry)
- `Plugins/Features/ProjectDialogue/README.md` - Dialogue feature (uses this registry)
- `Plugins/Features/ProjectInventory/README.md` - Inventory feature (uses this registry)
- `Plugins/Gameplay/ProjectGameplay/README.md` - Gameplay framework (base classes)
- `Plugins/Gameplay/ProjectSinglePlay/README.md` - Single-player orchestrator (calls this registry)
