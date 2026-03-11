# ProjectSinglePlay

Single-player gameplay plugin for ALIS.

Acts as a thin, data-driven orchestrator for single-player sessions:
- Consolidates UE gameplay classes (GameMode, PlayerController, Pawn).
- Composes game-layer features (combat, dialogue, inventory, etc.) via soft references.
- Stays world-agnostic and experience-driven.

Plugin modules:
- `ProjectSinglePlay` - runtime, server-safe orchestrator.
- `ProjectSinglePlayClient` - client-only input + UI bindings.
  - Module type: `ClientOnlyNoCommandlet` (excluded from server builds).


## 1. Purpose

- Consolidator for single-player gameplay (UE layer + game layer).
- Single place where the **single-player mode** is defined and wired to systems and features.
- Does not own content or specific mechanics - only policy, config, and orchestration.


## 2. Position in ALIS architecture

Layer in the client container:

- **Foundation tier**
  - `ProjectCore` - service locator, base interfaces, common utilities.
- **Systems tier**
  - `ProjectLoading` - loading pipeline, IoStore mounting, phases.
  - `ProjectGameplay` - base GameMode/PlayerController/Pawn classes.
- **Gameplay tier**
  - `ProjectSinglePlay` - single-player orchestrator (runtime).
  - `ProjectSinglePlayClient` - client-only PlayerController + UI/input glue.
  - `ProjectOnlinePlay` - online / multiplayer equivalent.
- **World tier**
  - World plugins (for example `City17`) that hold World Partition maps and world-specific content.

Key relationships:

- Worlds (for example City17) **do not own mechanics**.
  - They either:
    - Declare `SinglePlayerGameMode` from this plugin in World Settings, or
    - Remain GameMode-agnostic and let travel URL choose the mode.
- `ProjectSinglePlay`:
  - Owns single-player experience configuration via `FSinglePlayModeConfig`.
  - Uses soft references to load feature plugins at runtime.
  - Mode definitions in `SinglePlayModeDefaults.cpp` (agent-editable C++).

Note: The "experience" concept lives **inside Gameplay plugins** (like this one), not as a separate project-level domain.


## 3. Activation strategy

- **OnDemand**:
  - Lazy-loaded only when a single-player game session is starting.
  - Not part of boot UI or initial launcher flow.
- Required by gameplay worlds (for example City17) when running in single-player mode.
- Loaded via Orchestrator:
  - Orchestrator ensures the plugin is loaded before travel.
  - World can remain **agnostic** of this plugin if GameMode is specified at travel time.

This aligns with the general OnDemand activation patterns defined in `Plugins/Boot/Orchestrator/README.md`.


## 4. Responsibilities

**Thin orchestrator - two layers (policy + config, no mechanics):**

- **UE layer**
  - GameMode and mode config live in `ProjectSinglePlay`.
  - PlayerController and UI/input bindings live in `ProjectSinglePlayClient`.
  - Selected via data-driven `FSinglePlayModeConfig` (soft references).
- **Game layer**
  - Combat, dialogue, inventory, and other features.
  - Activated via soft references (no hard plugin dependencies).

Concrete responsibilities:

- Provide the **SinglePlayerGameMode** class (AGameMode-based).
- Manage single-player game state and match flow:
  - Session start, respawn, pause/resume, exit.
- Orchestrate game-layer features:
  - Consolidate feature packs such as ProjectCombat, ProjectDialogue, ProjectInventory.
  - Attach feature components to the spawned pawn (and/or PlayerState, GameState) based on `ModeConfig`.
- Own experience configuration for single-player:
  - Mode presets (Beginner, Medium, Hardcore).
  - Difficulty settings, feature toggles, spawn rules.
- Keep rules **deterministic and world-agnostic**:
  - Generic single-player rules (movement, health, stamina, etc.) live here or in feature plugins, not in world plugins.


## 5. Non-responsibilities

ProjectSinglePlay explicitly does **not**:

- Implement multiplayer/networking logic (that belongs to `ProjectOnlinePlay`).
- Implement concrete mechanics (combat formulas, dialogue trees, inventory logic):
  - Those live in feature plugins (ProjectCombat, ProjectDialogue, ProjectInventory, etc.).
- Own client UI/input code inside the runtime module:
  - That is isolated in `ProjectSinglePlayClient`.
- Own world composition:
  - Worlds (for example City17) depend on this GameMode or travel URL, not the other way around.
- Control World Partition or tile layout:
  - That is defined in the world plugins and Worlds README.


## 6. Dependencies

Runtime module (`ProjectSinglePlay`) hard dependencies:

- `ProjectCore`
  - For common utilities and base types.
- `ProjectGameplay`
  - For base GameMode and PlayerController classes (if shared base exists).
- `ProjectFeature`
  - For feature registry and initialization flow.

Client module (`ProjectSinglePlayClient`) hard dependencies:
- `ProjectUI`, `ProjectVitalsUI`, `ProjectInventoryUI`
  - UI viewmodels and panel bindings.
- `EnhancedInput`
  - Client input mapping and actions.
- `ProjectSinglePlay`
  - Shared types and logging.

Soft dependencies:

- **No hard compile-time dependencies** on feature plugins:
  - ProjectCombat, ProjectDialogue, ProjectInventory, etc.
  - They are loaded via soft references in `FModeConfig`.

General rule:

- `ProjectSinglePlay` depends only on the **core systems**.
- `ProjectSinglePlayClient` depends on UI and input systems.
- Features depend on ProjectSinglePlay's contracts (GameMode and ModeConfig), not the reverse.


## 7. Mode lifecycle (URL-based selection)

Single-player mode is chosen through URL options and a data-driven `FModeConfig` structure.

### 7.1. Travel example (world stays agnostic)

```cpp
// Travel with GameMode in URL (world stays agnostic)
// IMPORTANT: UClass name does NOT include 'A' prefix (that's C++ only, not in UE reflection)
// IMPORTANT: Use '?' separator for ALL options (not '&' like web URLs)
// IMPORTANT: 'game=' option should come LAST (UE parser reads value to end-of-string)
GetWorld()->ServerTravel("City17?Mode=Medium?game=/Script/ProjectSinglePlay.SinglePlayerGameMode");
```

### 7.1.1. URL Game Mode Syntax Reference

**Full class path format:**
```
?game=/Script/<ModuleName>.<ClassName>
```

**Key rules:**
- UClass name does NOT include 'A' prefix (C++ `ASinglePlayerGameMode` -> UClass `SinglePlayerGameMode`)
- Use `?` separator for ALL URL options (not `&` like web URLs)
- `game=` option must come LAST (UE parser reads this value to end-of-string)

**Correct:**
```
City17?Mode=Medium?game=/Script/ProjectSinglePlay.SinglePlayerGameMode
```

**Wrong (common mistakes):**
```
City17?game=/Script/ProjectSinglePlay.ASinglePlayerGameMode  <- A prefix (WRONG)
City17?game=/Script/ProjectSinglePlay.SinglePlayerGameMode&Mode=Medium  <- & separator (WRONG)
City17?game=/Script/ProjectSinglePlay.SinglePlayerGameMode?Mode=Medium  <- game= not last (WRONG)
```

**UE Source references (UE_PATH from scripts/config/ue_path.conf):**
- `UGameInstance::CreateGameModeForURL` - Engine/Source/Runtime/Engine/Private/GameInstance.cpp:1490
- `UGameMapsSettings::GetGameModeForName` - Engine/Source/Runtime/EngineSettings/Private/EngineSettingsModule.cpp:117
- `LoadClass<AGameModeBase>` call at GameInstance.cpp:1521

### 7.2. GameMode lifecycle hooks

#### InitGame (parse options, load config)

```cpp
void ASinglePlayerGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);

    // Parse URL options (Mode=Medium)
    FString ModeParam = UGameplayStatics::ParseOption(Options, TEXT("Mode"));

    // Load ModeConfig struct (defines UE layer + features)
    ModeConfig = LoadModeConfig(ModeParam);

    // Apply UE layer classes from config
    DefaultPawnClass = ModeConfig.DefaultPawnClass.LoadSynchronous();
    PlayerControllerClass = ModeConfig.PlayerControllerClass.LoadSynchronous();
}
```

#### HandleStartingNewPlayer (attach feature components)

Note: Feature components are attached in `HandleStartingNewPlayer`, not `PostLogin`.
Pawn typically doesn't exist yet in `PostLogin` - it's spawned later.

```cpp
void ASinglePlayerGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    Super::HandleStartingNewPlayer_Implementation(NewPlayer);

    APawn* Pawn = NewPlayer->GetPawn();
    if (Pawn && Pawn->HasAuthority())
    {
        // Initialize features via FFeatureRegistry (features attach own components)
        InitializeFeatures(Pawn);
    }
}

void ASinglePlayerGameMode::InitializeFeatures(APawn* Pawn)
{
    // GameMode controls ORDER, TIMING, SELECTION
    // Features own WHAT to attach and HOW to configure
    for (const FName& FeatureName : ModeConfig.FeatureNames)
    {
        FString ConfigJson = ModeConfig.FeatureConfigs.FindRef(FeatureName);
        FFeatureInitContext Context(GetWorld(), this, Pawn, ModeConfig.ModeName, ConfigJson);
        FFeatureRegistry::InitializeFeature(FeatureName, Context);
    }
}
```

#### BeginPlay (safety net)

```cpp
void ASinglePlayerGameMode::BeginPlay()
{
    Super::BeginPlay();

    // Verify all required features loaded
    // Log warnings if components missing (idempotent check)
}
```

## 8. ModeConfig - Pure C++ Registry (Agent-Editable)

Mode configurations are defined in pure C++ for full agent editability (no UAssets required).

### 8.1. FSinglePlayModeConfig struct

```cpp
USTRUCT(BlueprintType)
struct FSinglePlayModeConfig
{
    GENERATED_BODY()

    FName ModeName;                                    // Mode identifier
    TSoftClassPtr<APawn> DefaultPawnClass;             // Pawn class (soft ref)
    TSoftClassPtr<APlayerController> PlayerControllerClass;  // Controller class (soft ref)
    TArray<FName> RequiredFeaturePlugins;              // Plugin names for Orchestrator to load
    TArray<FName> FeatureNames;                        // Features to init via FFeatureRegistry
    TMap<FName, FString> FeatureConfigs;               // Per-feature JSON config
};
```

### 8.2. Defining modes (SinglePlayModeDefaults.cpp)

Agents edit `Source/ProjectSinglePlay/Private/SinglePlayModeDefaults.cpp` to add/modify modes:

```cpp
DEFINE_SINGLEPLAY_MODE(Medium)
{
    Config.ModeName = FName(TEXT("Medium"));
    Config.PlayerControllerClass = TSoftClassPtr<APlayerController>(
        FSoftClassPath(TEXT("/Script/ProjectSinglePlayClient.SinglePlayerPlayerController")));
    Config.FeatureConfigs.Add(FName(TEXT("Difficulty")), TEXT("{\"level\":\"normal\",\"damageMultiplier\":1.0}"));
}

DEFINE_SINGLEPLAY_MODE(Hardcore)
{
    Config.ModeName = FName(TEXT("Hardcore"));
    Config.PlayerControllerClass = TSoftClassPtr<APlayerController>(
        FSoftClassPath(TEXT("/Script/ProjectSinglePlayClient.SinglePlayerPlayerController")));
    Config.FeatureConfigs.Add(FName(TEXT("Difficulty")), TEXT("{\"level\":\"hard\",\"damageMultiplier\":2.0}"));
}
```

Note: In server-only builds, the class path may not resolve. The GameMode keeps its default PlayerControllerClass.

### 8.3. Adding a new mode

1. Open `SinglePlayModeDefaults.cpp`
2. Add a new `DEFINE_SINGLEPLAY_MODE` block
3. Rebuild the plugin
4. Use via URL: `?Mode=YourModeName`

### 8.4. Registry API

```cpp
// Find a mode by name
const FSinglePlayModeConfig* Config = FSinglePlayModeRegistry::FindMode(FName("Medium"));

// Check if mode exists
bool bExists = FSinglePlayModeRegistry::HasMode(FName("Hardcore"));

// Get all registered modes (Beginner, Medium, Hardcore)
TArray<FName> AllModes = FSinglePlayModeRegistry::GetAllModeNames();
```

### 8.5. JSON Override Support (Optional)

Modes can be extended or overridden via JSON without recompiling.

**Location:** `Config/SinglePlay/ModeOverrides.json`

**Format:**
```json
{
  "modes": [
    {
      "modeName": "Medium",
      "featureConfigs": {
        "Difficulty": "{\"level\":\"normal\",\"damageMultiplier\":1.0}"
      }
    },
    {
      "modeName": "Survival",
      "defaultPawnClass": "/Game/BP/BP_SurvivalPawn.BP_SurvivalPawn_C",
      "featureComponents": [
        "/Script/ProjectCombat.CombatComponent"
      ],
      "featureConfigs": {
        "Hunger": "{\"enabled\":true}",
        "Thirst": "{\"enabled\":true}"
      }
    }
  ]
}
```

**Behavior:**
- Loaded automatically during registry initialization (if file exists)
- Merges with C++ definitions (JSON values override C++ values per-key)
- New modes can be added via JSON (not just overrides)
- Invalid entries are logged and skipped (guardrail)

**Example file:** `Config/SinglePlay/ModeOverrides.json.example`

## 9. World dependency pattern

Two options for how worlds relate to ProjectSinglePlay:

* **Option A** - compile-time binding:
  * World depends on ProjectSinglePlay (.uplugin).
  * GameMode is declared in World Settings.

* **Option B** - runtime binding (recommended):
  * World is agnostic of ProjectSinglePlay.
  * GameMode is specified via URL at travel time.
  * Same world can run single-player or online by changing only the travel URL.

Recommended default: **Option B**, especially for shared tiles that can host both single-player and online experiences.

World plugins:
* Own World Partition maps and data layers.
* Optionally reference single-player GameMode in World Settings (Option A).
* Or remain neutral and let travel URLs decide (Option B).

## 10. Determinism and configuration

ProjectSinglePlay must keep single-player behavior deterministic and predictable:

* Core rules (movement speeds, health, stamina, basic damage handling) live:
  * In this plugin, or
  * In feature plugins controlled here via configuration.
* Tile/world differences (lighting, geometry, spawn positions, encounter density) live:
  * In world manifests and data layers, not in GameMode code.

Configuration guidelines:

* **Mode definitions live in C++** (`SinglePlayModeDefaults.cpp`) for agent editability
* Use `FSinglePlayModeConfig.FeatureConfigs` to pass per-feature JSON settings:
  * Difficulty presets
  * Allowed mechanics (for example toggling permadeath)
  * UI/UX behaviors for single-player
* Keep world-specific values out of `ASinglePlayerGameMode`:
  * They should come from world manifests instead
* Future: JSON override support for runtime config merging (see TODO.md)

## 11. Guidelines for contributors and agents

### 11.1. Human responsibilities

Humans should:

* Define and review:
  * The list of supported single-player modes (for example `Beginner`, `Medium`, `Hardcore`).
  * How `FSinglePlayModeConfig` is structured and where it is stored.
  * What features are allowed in single-player and how they are configured.
* Approve any changes that:
  * Introduce new hard dependencies.
  * Change how worlds bind to GameModes.
  * Change invariants of the single-player flow.

Humans should not:

* Add direct references to specific world plugins inside ProjectSinglePlay.
* Hard-code per-city or per-tile behavior in GameMode or PlayerController.

### 11.2. Agent responsibilities

Agents may:

* **Edit `SinglePlayModeDefaults.cpp`** to add/modify/remove modes:
  * Add new `DEFINE_SINGLEPLAY_MODE` blocks
  * Modify feature configs (`FeatureConfigs` map)
  * Add feature components to attach to pawns
* Propose and adjust:
  * Feature component lists for single-player
  * Feature configuration JSON strings
  * New mode presets (Survival, Creative, etc.)
* Document:
  * Which features are mandatory for single-player

Agents must not:

* Modify `.uplugin` dependencies of ProjectSinglePlay
* Introduce hard references to world plugins (City17, others)
* Change the World Partition or tile configuration (belongs to world plugins)
* Bypass `FSinglePlayModeConfig` by hard-coding classes in GameMode
* Edit registry internals (`SinglePlayModeRegistry.cpp`) - only edit `SinglePlayModeDefaults.cpp`

### 11.3. Invariants

ProjectSinglePlay invariants:

* Worlds choose GameMode (via World Settings or URL), not the other way around.
* Features are loaded via soft references, not hard dependencies.
* Single-player logic remains deterministic and world-agnostic.
* Mode configuration for single-player is defined in `FSinglePlayModeConfig` and its data sources, not scattered across code.

Any change that breaks these invariants should be treated as an architectural change and documented explicitly (for example in an ADR).

## 12. Related docs

* `Plugins/Boot/Orchestrator/README.md` - OnDemand plugin activation patterns.
* `Plugins/Worlds/.../README.md` - World tiling, World Partition, and data layer design.
* `TODO.md` in this plugin - concrete implementation tasks and backlog.
