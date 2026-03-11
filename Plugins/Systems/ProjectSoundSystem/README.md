# ProjectSoundSystem

Audio subsystem and sound orchestration for the Alis project.

## Purpose

Provides game-wide audio logic: music management, ambient zones, occlusion, and sound coordination. **This plugin contains logic only - sound assets live in [ProjectAudio](../../Resources/ProjectAudio/README.md).**

## Key Principle

**Logic, not assets.** This plugin:
- Manages music transitions (combat -> exploration -> menu)
- Controls ambient sound zones
- Handles audio occlusion and propagation
- Provides sound playback utilities

Does NOT contain WAV files, Sound Cues, or audio configuration assets.

## Folder Structure

```
Source/ProjectSoundSystem/
|- Public/
|  |- Subsystems/
|  |  `- ProjectSoundSubsystem.h    # GameInstance subsystem
|  |- Components/
|  |  |- AmbientZoneComponent.h     # Triggers ambient changes
|  |  `- OcclusionComponent.h       # Audio occlusion (TODO)
|  |- Managers/
|  |  |- MusicManager.h             # Music state machine
|  |  `- AmbientManager.h           # Ambient layer blending
|  `- Interfaces/
|     `- ISoundProvider.h           # Interface for sound lookup
`- Private/
   `- ...
```

## Deliverables

### Subsystem

| Class | Description |
|-------|-------------|
| `UProjectSoundSubsystem` | GameInstance subsystem for global audio control |

### Managers

| Class | Description |
|-------|-------------|
| `UMusicManager` | Handles music state (Combat, Exploration, Menu, None) with crossfades |
| `UAmbientManager` | Blends ambient layers based on location/time |

### Components

| Component | Description |
|-----------|-------------|
| `UAmbientZoneComponent` | Volume trigger that changes ambient sound profile |
| `UOcclusionComponent` | Ray-traced audio occlusion (TODO) |

### Interfaces

| Interface | Description |
|-----------|-------------|
| `ISoundProvider` | Interface for systems that provide sound assets |

## Hard Rules

1. **No audio assets** - WAVs, Cues, Attenuation go in ProjectAudio
2. **No gameplay dependencies** - Don't import Combat, Vitals, etc.
3. **GameInstance subsystem** - Lives for entire game session
4. **Soft references** - Load sounds from ProjectAudio via soft refs

## Dependencies

- `ProjectCore` - Foundation interfaces, logging

## Consumers

- Game systems - Call subsystem for managed playback
- [ProjectGAS](../../Gameplay/ProjectGAS/README.md) - GameplayCues can use sound utilities
- Level Blueprints - Trigger ambient zone changes
- UI - Menu music control

## Usage

### Music Management

```cpp
// Get subsystem
if (auto* SoundSub = GetGameInstance()->GetSubsystem<UProjectSoundSubsystem>())
{
    // Transition to combat music
    SoundSub->SetMusicState(EProjectMusicState::Combat, 2.0f);

    // Fade to silence
    SoundSub->SetMusicState(EProjectMusicState::None, 1.0f);
}
```

### Ambient Zones

```cpp
// In level Blueprint or zone trigger
UPROPERTY()
UAmbientZoneComponent* AmbientZone;

// Configure
AmbientZone->AmbientProfile = InteriorAmbientProfile;
AmbientZone->BlendTime = 1.5f;

// Player enters zone -> ambient automatically transitions
```

### Direct Sound Playback

```cpp
// For one-shot sounds with project-wide settings
SoundSub->PlaySFX(ImpactSound, Location, VolumeMultiplier);

// For UI sounds (2D)
SoundSub->PlayUISound(ButtonClickSound);
```

## Music State Machine

```
       StartGame
          |
          v
    [Exploration] <---> [Combat]
          |                 |
          v                 v
       [Menu] <-------------|
          |
          v
       [None]
```

Transitions:
- **Exploration -> Combat**: Enemy detected (fast fade 0.5s)
- **Combat -> Exploration**: Combat ended (slow fade 3.0s)
- **Any -> Menu**: Pause menu opened (medium fade 1.0s)
- **Menu -> Previous**: Resume game (medium fade 1.0s)

## Orchestrator Registration

This plugin is code-only and registered via `dev_manifest.json`.

- **AlwaysOn** - Small code footprint, needed throughout game

## Documentation

- [Audio Subsystem Architecture](docs/audio_subsystem.md) - Detailed subsystem design

## Related Documentation

- [ProjectAudio](../../Resources/ProjectAudio/README.md) - Sound assets
- [Audio Assets Guide](../../Resources/ProjectAudio/docs/audio_assets.md) - Asset creation
- [ProjectGAS GameplayCues](../../Gameplay/ProjectGAS/docs/gameplay_cue.md) - GAS audio feedback
