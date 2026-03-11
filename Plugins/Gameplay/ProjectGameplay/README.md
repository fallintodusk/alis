# ProjectGameplay

Gameplay framework plugin for shared runtime gameplay patterns and GameMode-level orchestration support.

## Purpose

Gameplay framework plugin providing **base classes and shared gameplay patterns**. Orchestrator GameModes and gameplay utilities.

**Location:** `Plugins/Gameplay/ProjectGameplay` (Gameplay tier)

## What Moved to ProjectFeature

Feature registration contracts (`FFeatureRegistry`, `FFeatureInitContext`) moved to `ProjectFeature` plugin so that feature plugins (Combat, Dialogue, Inventory) don't need to depend on the gameplay framework.

See: [../../Features/ProjectFeature/README.md](../../Features/ProjectFeature/README.md)

## Current Contents

- Depends on `ProjectFeature` for feature registration contracts
- Future: Base GameMode classes, shared gameplay utilities

## Dependencies

- ProjectCore (shared types)
- ProjectFeature (feature registration contracts)

## Dependency Structure

```
ProjectFeature (contracts only)
    ^
    |
+---+-------------------+
|                       |
Feature plugins     ProjectGameplay (this - framework)
(Combat, etc.)              ^
                            |
                    ProjectSinglePlay (orchestrator)
```

**Key insight:** Feature plugins depend ONLY on `ProjectFeature`, not on `ProjectGameplay`. This keeps features decoupled from the gameplay framework.

## How Feature Registration Works

1. **Features register** on module startup via `FFeatureRegistry::RegisterFeature()`
2. **Mode config** defines ordered `FeatureNames` + `FeatureConfigs`
3. **GameMode** loops `FeatureNames`, calls `FFeatureRegistry::InitializeFeature()` for each
4. **Features own their init** - attach components, set up subsystems, parse config, etc.

## Current State

**Phase 4 Complete** - Contracts moved to ProjectFeature, features registered (Combat, Dialogue, Inventory), ProjectSinglePlay migrated.

## URL Game Mode Syntax (Critical Reference)

When specifying GameMode via travel URL, follow these rules:

**Format:** `MapName?Option=Value?game=/Script/ModuleName.ClassName`

**Key rules:**
1. **UClass name does NOT include 'A' prefix** - C++ `ASinglePlayerGameMode` -> UClass `SinglePlayerGameMode`
2. **Use `?` separator for ALL options** - NOT `&` like web URLs
3. **`game=` option must come LAST** - UE parser reads this value to end-of-string

**Example:**
```cpp
// Correct
GetWorld()->ServerTravel("City17?Mode=Medium?game=/Script/ProjectSinglePlay.SinglePlayerGameMode");

// Wrong - A prefix
GetWorld()->ServerTravel("City17?game=/Script/ProjectSinglePlay.ASinglePlayerGameMode");

// Wrong - & separator
GetWorld()->ServerTravel("City17?game=/Script/ProjectSinglePlay.SinglePlayerGameMode&Mode=Medium");
```

**Why no 'A' prefix?**
- `A` prefix is C++ naming convention only (AGameModeBase, AActor, etc.)
- UE reflection system uses class name WITHOUT prefix
- `LoadClass<AGameModeBase>()` expects `/Script/Module.ClassName` (no A)

**UE Source references (UE_PATH from scripts/config/ue_path.conf):**
- `UGameInstance::CreateGameModeForURL` - Engine/Source/Runtime/Engine/Private/GameInstance.cpp:1490
- `LoadClass<AGameModeBase>` call at GameInstance.cpp:1521

See: [../ProjectSinglePlay/README.md](../ProjectSinglePlay/README.md) for full details.

## See Also

- [../../Features/ProjectFeature/README.md](../../Features/ProjectFeature/README.md) - Feature registration contracts
- [../ProjectSinglePlay/README.md](../ProjectSinglePlay/README.md) - Uses the feature registry in a concrete game mode flow
