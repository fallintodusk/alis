# ProjectAudio

Sound assets and audio utilities for the Alis project.

## Purpose

Contains sound assets (WAV, Cues, Attenuation, SoundClasses) plus audio-specific utilities. **This plugin holds assets only - playback logic lives in [ProjectSoundSystem](../../Systems/ProjectSoundSystem/README.md).**

## Key Principle

**Assets, not logic.** If you need audio managers, music transitions, or occlusion systems, put them in ProjectSoundSystem. This plugin only stores sound files and their immediate UE configurations.

## Folder Structure

```
Content/
|- SFX/                    # Sound effects
|  |- Impact/              # Hit, collision sounds
|  |- Footsteps/           # Surface-based footsteps
|  |- UI/                  # UI feedback sounds
|  |- Weapons/             # Weapon sounds
|  `- Environment/         # World object sounds
|- Ambient/                # Environmental ambient sounds
|  |- Weather/             # Rain, wind, thunder
|  |- Interior/            # Room tones, HVAC
|  `- Exterior/            # Birds, traffic, nature
|- Music/                  # Music tracks
|  |- Combat/              # Combat music
|  |- Exploration/         # Ambient exploration
|  `- Menu/                # Menu music
|- Voice/                  # Voice lines (TODO)
`- Config/                 # Audio configuration assets
   |- Attenuation/         # Sound attenuation settings
   |- Classes/             # Sound classes
   |- Mixes/               # Sound mixes
   `- Concurrency/         # Concurrency settings
```

## Asset Types

| Asset Type | Prefix | Description |
|------------|--------|-------------|
| Sound Wave | `SW_` | Raw audio file (WAV import) |
| Sound Cue | `SC_` | Layered/randomized sound |
| Sound Class | `SCL_` | Volume/priority grouping |
| Sound Mix | `SMX_` | EQ/volume adjustments |
| Sound Attenuation | `SA_` | Distance falloff settings |
| Sound Concurrency | `SCC_` | Max simultaneous sounds |
| MetaSound | `MS_` | Procedural audio (TODO) |

## Hard Rules

1. **No playback logic** - Managers and subsystems go in ProjectSoundSystem
2. **No gameplay dependencies** - Sound assets don't know about combat, vitals, etc.
3. **Use Sound Cues for variation** - Don't duplicate WAVs for randomization
4. **Consistent attenuation** - Use shared SA_ assets, don't duplicate settings

## Dependencies

- `ProjectCore` - Foundation (logging only)

## Consumers

- [ProjectSoundSystem](../../Systems/ProjectSoundSystem/README.md) - Audio subsystem, music manager
- [ProjectGAS](../../Gameplay/ProjectGAS/README.md) - GameplayCues reference sounds from here
- UI widgets - Direct sound playback for UI feedback
- Blueprints - Direct sound playback for simple cases

## Usage

### Direct Playback (Simple)

```cpp
// For simple one-shot sounds
UGameplayStatics::PlaySoundAtLocation(World, SoundCue, Location);
UGameplayStatics::PlaySound2D(World, SoundCue);
```

### Via ProjectSoundSystem (Managed)

```cpp
// For music, ambient, managed playback
if (auto* SoundSystem = GetGameInstance()->GetSubsystem<UProjectSoundSubsystem>())
{
    SoundSystem->PlayMusic(MusicTrack, FadeInTime);
    SoundSystem->SetAmbientZone(ZoneTag);
}
```

### Via GameplayCues (GAS Integration)

```cpp
// GameplayCue Notify references sound from here
// /ProjectGAS/GameplayCues/GC_Impact_Physical.uasset
// -> References /ProjectAudio/SFX/Impact/SC_Impact_Physical.uasset
```

## Orchestrator Registration

This plugin is content-only and registered via `dev_manifest.json`.

- **OnDemand** - Heavy audio content, loaded when needed

## Documentation

- [Audio Assets Guide](docs/audio_assets.md) - Detailed asset creation guide

## Related Documentation

- [ProjectSoundSystem](../../Systems/ProjectSoundSystem/README.md) - Audio playback logic
- [ProjectGAS GameplayCues](../../Gameplay/ProjectGAS/docs/gameplay_cue.md) - GAS audio feedback
