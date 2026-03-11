# GameplayCue Architecture

GameplayCues are UE's mechanism for connecting GAS events to presentation feedback (audio, visual, camera).

## Overview

```
GAS Event (Ability/Effect)
    |
    | fires GameplayTag
    v
GameplayCue.Impact.Physical
    |
    | matched by tag
    v
GC_Impact_Physical (Notify Blueprint)
    |
    | orchestrates feedback
    +---> ProjectAudio: Play SC_Impact_Physical
    +---> ProjectVFX: Spawn NS_Impact_Sparks (future)
    +---> Camera: Apply CameraShake_Small
```

## Key Concepts

### GameplayCue Tags

Tags follow hierarchy: `GameplayCue.<Category>.<Specific>`

| Tag | Description |
|-----|-------------|
| `GameplayCue.Impact.Physical` | Physical damage hit |
| `GameplayCue.Impact.Fire` | Fire damage hit |
| `GameplayCue.Impact.Explosion` | Explosive damage |
| `GameplayCue.Status.Bleeding` | Bleeding status applied |
| `GameplayCue.Status.Poisoned` | Poison status applied |
| `GameplayCue.Heal.Instant` | Instant heal feedback |
| `GameplayCue.Ability.Dash` | Dash ability activated |

### Notify Types

| Type | Use Case | Lifetime |
|------|----------|----------|
| `GameplayCueNotify_Static` | One-shot effects (hit, heal) | Instant |
| `GameplayCueNotify_Actor` | Persistent effects (burning, bleeding) | Duration-based |

**Rule:** Use Static for instant feedback, Actor for looping/persistent effects.

## Architecture Decisions

### Why GameplayCues Live in ProjectGAS

1. **GAS-native** - Triggered by GameplayTags from abilities/effects
2. **Orchestration layer** - Bridges gameplay -> presentation
3. **Single config path** - One folder for all cues
4. **References, not owns** - Cues reference assets from other plugins

### Asset Ownership

| Plugin | Owns | Referenced By |
|--------|------|---------------|
| ProjectAudio | Sound assets (WAV, Cues) | GameplayCues |
| ProjectVFX (future) | Particle systems (Niagara) | GameplayCues |
| ProjectGAS | GameplayCue Notify Blueprints | GAS system |

GameplayCue Notifies **reference** assets, they don't **contain** them.

## Configuration

### DefaultGame.ini

```ini
[/Script/GameplayAbilities.AbilitySystemGlobals]
+GameplayCueNotifyPaths=/ProjectGAS/GameplayCues
```

This tells GAS: "Only scan `/ProjectGAS/GameplayCues/` for cue notifies."

Without this config, GAS scans ALL of `/Game/` which is slow on large projects.

## Creating GameplayCues

### Step 1: Define the Tag

Preferred (project-owned tags): define native tags in `ProjectCore`:
- `Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/ProjectGameplayTags.h`
- `Plugins/Foundation/ProjectCore/Source/ProjectCore/Private/ProjectGameplayTags.cpp`

Optional (non-native engine/plugin/imported tags): `Config/DefaultGameplayTags.ini`:

```ini
+GameplayTagList=(Tag="GameplayCue.Impact.Physical",DevComment="Physical damage feedback")
```

### Step 2: Create the Notify Blueprint

1. Navigate to `/ProjectGAS/Content/GameplayCues/`
2. Right-click -> Blueprint Class
3. Search for `GameplayCueNotify_Static` (or `_Actor`)
4. Name: `GC_Impact_Physical` (must match tag pattern)

### Step 3: Configure the Notify

In the Blueprint:
1. Set `GameplayCue Tag` = `GameplayCue.Impact.Physical`
2. Override `OnExecute` (Static) or `OnActive` (Actor)

### Step 4: Add Feedback

```cpp
// In OnExecute (Blueprint or C++)
void AGC_Impact_Physical::OnExecute(AActor* Target, const FGameplayCueParameters& Parameters)
{
    // Play sound (from ProjectAudio)
    UGameplayStatics::PlaySoundAtLocation(
        Target,
        ImpactSound,  // Soft ref to /ProjectAudio/SFX/Impact/SC_Impact_Physical
        Parameters.Location
    );

    // Spawn particles (from ProjectVFX - future)
    UNiagaraFunctionLibrary::SpawnSystemAtLocation(
        Target,
        ImpactParticle,  // Soft ref to /ProjectVFX/Impact/NS_Impact_Sparks
        Parameters.Location
    );

    // Camera shake
    if (APlayerController* PC = Cast<APlayerController>(Target->GetInstigatorController()))
    {
        PC->ClientStartCameraShake(ImpactShake);
    }
}
```

## Triggering GameplayCues

### From GameplayEffect

```cpp
// In GE Blueprint or C++
UPROPERTY()
TArray<FGameplayEffectCue> GameplayCues;

// Add cue to effect
FGameplayEffectCue DamageCue;
DamageCue.GameplayCueTags.AddTag(FGameplayTag::RequestGameplayTag("GameplayCue.Impact.Physical"));
GameplayCues.Add(DamageCue);
```

### From Ability

```cpp
// In GameplayAbility
void UGA_MeleeAttack::ActivateAbility(...)
{
    // Apply damage effect (triggers cue automatically if configured)
    ApplyGameplayEffectToTarget(DamageEffect, Target);

    // Or manually trigger cue
    FGameplayCueParameters CueParams;
    CueParams.Location = HitLocation;
    CueParams.Normal = HitNormal;

    TargetASC->ExecuteGameplayCue(
        FGameplayTag::RequestGameplayTag("GameplayCue.Impact.Physical"),
        CueParams
    );
}
```

### From Code (Direct)

```cpp
// Any system with ASC access
ASC->ExecuteGameplayCue(
    FGameplayTag::RequestGameplayTag("GameplayCue.Heal.Instant"),
    FGameplayCueParameters()
);
```

## Best Practices

### DO

- Use soft references for assets (don't hard-load in constructor)
- Match notify name to tag (`GC_Impact_Physical` -> `GameplayCue.Impact.Physical`)
- Use Static notifies for instant effects
- Use Actor notifies for persistent/looping effects
- Keep notifies lightweight (orchestrate, don't compute)

### DON'T

- Put gameplay logic in GameplayCues (they're presentation only)
- Hard-reference assets (causes unnecessary loading)
- Create duplicate cues for minor variations (use parameters)
- Forget to set the GameplayCue Tag in the Blueprint

## Folder Structure

```
/ProjectGAS/Content/GameplayCues/
|- Impact/
|  |- GC_Impact_Physical.uasset
|  |- GC_Impact_Fire.uasset
|  `- GC_Impact_Explosion.uasset
|- Status/
|  |- GC_Status_Bleeding.uasset
|  |- GC_Status_Poisoned.uasset
|  `- GC_Status_Burning.uasset
|- Heal/
|  `- GC_Heal_Instant.uasset
`- Ability/
   |- GC_Ability_Dash.uasset
   `- GC_Ability_Sprint.uasset
```

## Related Documentation

- [ProjectGAS README](../README.md) - GAS wrapper overview
- [ProjectAudio](../../Resources/ProjectAudio/README.md) - Sound assets
- [ProjectSoundSystem](../../Systems/ProjectSoundSystem/README.md) - Audio playback
- [GAS Documentation (GitHub)](https://github.com/tranek/GASDocumentation#gameplaycues)
- [Epic GameplayCue Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-cues-in-unreal-engine)
