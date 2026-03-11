# Audio Assets Guide

Detailed guide for creating and organizing sound assets in ProjectAudio.

## Asset Types

### Sound Wave (SW_)

Raw audio imported from WAV files.

**Import settings:**
- Sample rate: 44100 Hz or 48000 Hz
- Bit depth: 16-bit or 24-bit
- Channels: Mono for 3D sounds, Stereo for music/UI

**Naming:** `SW_<Category>_<Name>`
- `SW_Impact_Metal_01`
- `SW_Footstep_Concrete_Walk`
- `SW_UI_ButtonClick`

### Sound Cue (SC_)

Node-based sound with randomization, layering, and modulation.

**When to use:**
- Multiple variations (randomized)
- Layered sounds (base + detail)
- Parameter-driven (pitch, volume)

**Naming:** `SC_<Category>_<Name>`
- `SC_Impact_Physical` (randomizes between SW_Impact variants)
- `SC_Footstep_Generic` (selects by surface type)

### Sound Attenuation (SA_)

Distance falloff settings for 3D spatialization.

**Presets:**
| Preset | Inner | Outer | Use Case |
|--------|-------|-------|----------|
| `SA_Close` | 100 | 500 | Footsteps, small objects |
| `SA_Medium` | 200 | 1500 | Voices, weapons |
| `SA_Far` | 500 | 5000 | Explosions, vehicles |
| `SA_Ambient` | 1000 | 10000 | Environmental |

### Sound Class (SCL_)

Volume/priority grouping for mix control.

**Hierarchy:**
```
SCL_Master
  |- SCL_SFX
  |   |- SCL_Impact
  |   |- SCL_Footsteps
  |   `- SCL_Weapons
  |- SCL_Voice
  |- SCL_Music
  |- SCL_Ambient
  `- SCL_UI
```

### Sound Concurrency (SCC_)

Limits simultaneous sounds of same type.

| Setting | Max Count | Use Case |
|---------|-----------|----------|
| `SCC_Footsteps` | 4 | Prevent footstep spam |
| `SCC_Impacts` | 8 | Allow combat chaos |
| `SCC_Ambient` | 2 | One ambient per zone |

## Folder Organization

```
Content/
|- SFX/
|  |- Impact/
|  |  |- SW_Impact_Metal_01.uasset
|  |  |- SW_Impact_Metal_02.uasset
|  |  `- SC_Impact_Metal.uasset      # Randomizes above
|  |- Footsteps/
|  |  |- Concrete/
|  |  |  |- SW_Footstep_Concrete_Walk_01.uasset
|  |  |  `- SC_Footstep_Concrete.uasset
|  |  `- Grass/
|  `- Weapons/
|- Config/
|  |- Attenuation/
|  |  |- SA_Close.uasset
|  |  `- SA_Medium.uasset
|  |- Classes/
|  |  `- SCL_Master.uasset
|  `- Concurrency/
|     `- SCC_Footsteps.uasset
`- Music/
   |- Combat/
   `- Exploration/
```

## Best Practices

### DO

- Use Sound Cues for variation (not duplicate WAVs)
- Share attenuation settings (don't duplicate per sound)
- Organize by category first, then by type
- Use mono for 3D sounds (saves memory, better spatialization)

### DON'T

- Import stereo for 3D sounds (wastes memory)
- Create one-off attenuation per sound
- Mix naming conventions (pick one, stick to it)
- Forget to set Sound Class (breaks mix control)

## Integration

### With GameplayCues

GameplayCue Notifies reference sounds from here:

```cpp
// In GC_Impact_Physical
UPROPERTY()
USoundCue* ImpactSound;  // -> /ProjectAudio/SFX/Impact/SC_Impact_Physical
```

### With ProjectSoundSystem

Music and managed playback goes through subsystem:

```cpp
// Don't play music directly
// DO: Use ProjectSoundSubsystem
SoundSub->PlayMusic(MusicTrack);
```

## Related Documentation

- [ProjectAudio README](../README.md) - Plugin overview
- [ProjectSoundSystem](../../Systems/ProjectSoundSystem/README.md) - Audio logic
- [GameplayCue Architecture](../../Gameplay/ProjectGAS/docs/gameplay_cue.md) - GAS integration
