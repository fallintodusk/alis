# Audio Subsystem Architecture

Detailed architecture for ProjectSoundSystem's audio management.

## UProjectSoundSubsystem

GameInstance subsystem providing global audio control.

### Responsibilities

- Music state management (combat/exploration/menu transitions)
- Ambient zone blending
- Global volume control
- Sound event coordination

### Lifecycle

```cpp
// Created when GameInstance initializes
// Lives for entire game session
// Survives level transitions
```

### API

```cpp
UCLASS()
class PROJECTSOUNDSYSTEM_API UProjectSoundSubsystem : public UGameInstanceSubsystem
{
    // Music control
    UFUNCTION(BlueprintCallable)
    void SetMusicState(EProjectMusicState NewState, float FadeTime = 1.0f);

    UFUNCTION(BlueprintCallable)
    void StopMusic(float FadeTime = 1.0f);

    // Ambient control
    UFUNCTION(BlueprintCallable)
    void SetAmbientZone(FGameplayTag ZoneTag, float BlendTime = 1.5f);

    // Volume control
    UFUNCTION(BlueprintCallable)
    void SetMasterVolume(float Volume);

    UFUNCTION(BlueprintCallable)
    void SetMusicVolume(float Volume);

    UFUNCTION(BlueprintCallable)
    void SetSFXVolume(float Volume);

    // Utility
    UFUNCTION(BlueprintCallable)
    void PlayUISound(USoundBase* Sound);

    UFUNCTION(BlueprintCallable)
    void Play3DSound(USoundBase* Sound, FVector Location, float VolumeMultiplier = 1.0f);
};
```

## Music State Machine

### States

```cpp
UENUM(BlueprintType)
enum class EProjectMusicState : uint8
{
    None,           // Silence
    Menu,           // Main menu music
    Exploration,    // Ambient exploration
    Combat,         // Combat music
    Cinematic       // Cutscene music
};
```

### Transitions

```
       Game Start
           |
           v
        [Menu]
           |
      Start Game
           |
           v
    [Exploration] <----+
           |           |
     Enemy Detected    | Combat Ended (3s delay)
           |           |
           v           |
       [Combat] -------+
           |
      Open Menu
           |
           v
        [Menu]
```

### Crossfade Logic

```cpp
void UMusicManager::TransitionTo(EProjectMusicState NewState, float FadeTime)
{
    if (CurrentState == NewState) return;

    // Fade out current
    if (CurrentAudioComponent)
    {
        CurrentAudioComponent->FadeOut(FadeTime, 0.0f);
    }

    // Start new (faded in)
    USoundBase* NewMusic = GetMusicForState(NewState);
    if (NewMusic)
    {
        CurrentAudioComponent = UGameplayStatics::SpawnSound2D(this, NewMusic);
        CurrentAudioComponent->FadeIn(FadeTime);
    }

    CurrentState = NewState;
}
```

## Ambient Zone System

### Zone Configuration

```cpp
USTRUCT(BlueprintType)
struct FProjectAmbientZone
{
    UPROPERTY(EditAnywhere)
    FGameplayTag ZoneTag;  // e.g., "Ambient.Interior.Apartment"

    UPROPERTY(EditAnywhere)
    USoundBase* AmbientSound;

    UPROPERTY(EditAnywhere)
    float BaseVolume = 1.0f;

    UPROPERTY(EditAnywhere)
    TArray<USoundBase*> RandomSounds;  // One-shots (creaks, drips)

    UPROPERTY(EditAnywhere)
    FFloatRange RandomSoundInterval = FFloatRange(5.0f, 15.0f);
};
```

### Zone Blending

When player moves between zones:

```cpp
void UAmbientManager::BlendToZone(const FProjectAmbientZone& NewZone, float BlendTime)
{
    // Crossfade ambient loops
    if (CurrentAmbientComponent)
    {
        CurrentAmbientComponent->FadeOut(BlendTime, 0.0f);
    }

    CurrentAmbientComponent = SpawnAmbient(NewZone.AmbientSound);
    CurrentAmbientComponent->FadeIn(BlendTime);

    // Update random sound pool
    CurrentRandomSounds = NewZone.RandomSounds;
    RandomSoundInterval = NewZone.RandomSoundInterval;
}
```

### Zone Trigger Component

```cpp
UCLASS()
class UAmbientZoneComponent : public UBoxComponent
{
    UPROPERTY(EditAnywhere)
    FGameplayTag ZoneTag;

    UPROPERTY(EditAnywhere)
    float BlendTime = 1.5f;

    virtual void OnComponentBeginOverlap(...) override
    {
        if (auto* SoundSub = GetGameInstance()->GetSubsystem<UProjectSoundSubsystem>())
        {
            SoundSub->SetAmbientZone(ZoneTag, BlendTime);
        }
    }
};
```

## Volume Management

### Sound Class Hierarchy

```
SCL_Master (user-controlled)
  |- SCL_Music (user-controlled)
  |- SCL_SFX (user-controlled)
  |   |- SCL_Weapons
  |   |- SCL_Footsteps
  |   `- SCL_Impacts
  |- SCL_Voice
  |- SCL_Ambient
  `- SCL_UI (always audible)
```

### Settings Integration

```cpp
void UProjectSoundSubsystem::ApplyAudioSettings(const FProjectAudioSettings& Settings)
{
    // Apply to sound classes
    SetSoundClassVolume(SCL_Master, Settings.MasterVolume);
    SetSoundClassVolume(SCL_Music, Settings.MusicVolume);
    SetSoundClassVolume(SCL_SFX, Settings.SFXVolume);
    SetSoundClassVolume(SCL_Voice, Settings.VoiceVolume);
}
```

## Integration Points

### With ProjectGAS GameplayCues

GameplayCues can use subsystem for managed playback:

```cpp
// Simple: Direct playback (fire and forget)
UGameplayStatics::PlaySoundAtLocation(World, ImpactSound, Location);

// Managed: Via subsystem (respects settings, concurrency)
SoundSub->Play3DSound(ImpactSound, Location);
```

### With ProjectSettings

Audio settings stored in ProjectSettings, applied via subsystem.

### With ProjectLoading

Music state changes during loading:
- Loading starts -> Fade to silence or loading music
- Loading ends -> Restore previous state

## Performance Considerations

- Use Sound Concurrency to limit simultaneous sounds
- Cull distant sounds via attenuation
- Pool AudioComponents for frequently played sounds
- Use 2D sounds for UI (cheaper than 3D)

## Related Documentation

- [ProjectSoundSystem README](../README.md) - Plugin overview
- [ProjectAudio](../../Resources/ProjectAudio/README.md) - Sound assets
- [Audio Assets Guide](../../Resources/ProjectAudio/docs/audio_assets.md) - Asset creation
