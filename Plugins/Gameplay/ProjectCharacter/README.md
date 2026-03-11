# ProjectCharacter

Base character plugin for ALIS gameplay modes.

## 1. Purpose

- Provides `AProjectCharacter` - first-person character with visible body
- Handles Enhanced Input setup (move, look, jump)
- OnDemand loaded - only for actual gameplay (not menus)

## 2. Architecture Position

Layer in the client container:

- **Foundation tier**
  - `ProjectCore` - service locator, base interfaces, common utilities.
- **Systems tier**
  - `ProjectGameplay` - base interfaces (minimal).
  - `ProjectCharacter` - character, input, camera (this plugin).
    - [Detailed Design Doc](docs/design.md)
- **Gameplay tier**
  - `ProjectSinglePlay` - single-player orchestrator (references via soft ref).
  - `ProjectOnlinePlay` - online multiplayer (references via soft ref).

Key relationships:

- Referenced via soft refs by ProjectSinglePlay and ProjectOnlinePlay
- Does NOT depend on any gameplay mode plugins
- MenuPlay does NOT use this (uses SpectatorPawn)

## 3. Activation Strategy

- **OnDemand**: Loaded when gameplay modes start (SinglePlay, OnlinePlay)
- Not loaded for menus (ProjectMenuPlay uses SpectatorPawn)

## 4. Key Features

- True first-person camera (can see legs/body when looking down)
- Based on UE5 third person template movement
- Enhanced Input system (MoveAction, LookAction, JumpAction)
- GAS integration (ASC + AttributeSets)
- Replication-ready for online play

## 4a. GAS Integration

Character owns core GAS infrastructure (ASC + AttributeSets):

```
AProjectCharacter : IAbilitySystemInterface
  -> UAbilitySystemComponent (ASC)  // Owner
  -> UHealthAttributeSet     (Health, MaxHealth)
  -> USurvivalAttributeSet   (Thirst, Hunger, Energy, Sleep)
  -> UStaminaAttributeSet    (Stamina, MaxStamina)
```

**Access:** Features get ASC via `IAbilitySystemInterface::GetAbilitySystemComponent()`.

**Init pattern:**
```cpp
// Server - PossessedBy (works for AI too)
ASC->InitAbilityActorInfo(this, this);
if (HasAuthority()) GiveStartupAbilitySets();

// Client - BeginPlay (idempotent)
ASC->InitAbilityActorInfo(this, this);
```

**AI/NPC:** Same pattern - pawn owns ASC (`Owner=this`, `Avatar=this`). No PlayerState needed.

**Future (multiplayer):** For respawn persistence, ASC moves to PlayerState. `GetAbilitySystemComponent()` forwards to PlayerState. Features unchanged.

See [ProjectGAS README](../ProjectGAS/README.md) for AttributeSet composition pattern.

## 5. Camera Setup

- Camera attached at eye height (no spring arm)
- Character rotates with view (`bUseControllerRotationYaw = true`)
- Head bone hidden to prevent clipping (when skeletal mesh assigned)

## 6. Dependencies

- `EnhancedInput` - for input handling
- `GameplayAbilities` - for GAS (ASC, AttributeSets)
- `ProjectGAS` - for project AttributeSets (Health, Survival, Stamina)

## 7. Usage

Referenced via soft class path in mode configs:

```cpp
Config.DefaultPawnClass = TSoftClassPtr<APawn>(
    FSoftClassPath(TEXT("/Script/ProjectCharacter.ProjectCharacter"))
);
```

## 8. Future Extensions

- Character configuration registry (JSON-driven settings)
- Movement presets (walk speed, jump height)
- Camera presets (FOV, head bob)
