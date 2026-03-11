# Lighting System Refactor

**Status:** PENDING APPROVAL
**Priority:** Medium
**Created:** 2024-12-22

---

## Overview

Refactor monolithic `ALightingSetupActor` into modular C++ architecture:
- `ULightingSubsystem` - WorldSubsystem for time-of-day management
- `ULightingPresetDataAsset` - Data-driven presets
- Simplified `ALightingSetupActor` - Component registration only

---

## Current Issues

| Issue | Severity | Description |
|-------|----------|-------------|
| Exposure conflict | High | Actor uses `AEM_Manual` but DefaultEngine.ini uses Auto Exposure |
| bUnbound PostProcess | Medium | Overrides level-specific PP volumes |
| Hardcoded values | Low | FogMaxOpacity, VolumetricFogDistance not exposed |

---

## Proposed Architecture

```
ULightingSubsystem (WorldSubsystem)
├── Manages time-of-day
├── Smooth transitions between presets
├── Component registration from any actor
└── Built-in presets (Day/Sunset/Night/Sunrise)

ULightingPresetDataAsset (DataAsset)
├── FLightingPreset struct with all settings
├── Transition duration
└── Created in Content Browser

ALightingSetupActor (Actor)
├── Creates all lighting components
├── Registers them with Subsystem
├── bUnbound=false, Priority=-1 (respects level PP)
└── Initial preset selection in editor
```

---

## Key Improvements

1. **No exposure conflict** - Remove Manual mode, use engine Auto Exposure
2. **PP priority** - `bUnbound=false`, `Priority=-1` - level volumes take precedence
3. **Data-driven** - Presets via DataAsset
4. **Smooth transitions** - `TransitionToPreset()` with interpolation

---

## Next Steps

1. [ ] Review and approve architecture
2. [ ] Test draft code compiles
3. [ ] Verify no exposure conflicts with DefaultEngine.ini settings
4. [ ] Create default DataAsset presets in Content
5. [ ] Update existing levels to use new system

---

## Draft Code

### LightingPresetDataAsset.h

```cpp
// LightingPresetDataAsset.h
// Data-driven lighting presets for day/night cycle
// Replaces hardcoded values in LightingSetupActor

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LightingPresetDataAsset.generated.h"

/**
 * Lighting preset configuration
 * Used by LightingSubsystem for time-of-day transitions
 */
USTRUCT(BlueprintType)
struct ALIS_API FLightingPreset
{
	GENERATED_BODY()

	// ============================================
	// DIRECTIONAL LIGHT (SUN/MOON)
	// ============================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun")
	float SunIntensity = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun")
	FLinearColor SunColor = FLinearColor(1.0f, 0.98f, 0.94f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun")
	FRotator SunRotation = FRotator(-60.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float SunSourceAngle = 2.0f;

	// ============================================
	// SKY LIGHT
	// ============================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkyLight")
	float SkyLightIntensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkyLight")
	FLinearColor LowerHemisphereColor = FLinearColor(0.376f, 0.376f, 0.376f, 1.0f);

	// ============================================
	// FOG
	// ============================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog")
	float FogDensity = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog")
	float FogHeightFalloff = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog")
	FLinearColor FogInscatteringColor = FLinearColor(0.69f, 0.77f, 0.87f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog")
	float FogMaxOpacity = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog")
	bool bEnableVolumetricFog = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog")
	float VolumetricFogScatteringDistribution = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog")
	float VolumetricFogExtinctionScale = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog")
	float VolumetricFogDistance = 6000.0f;

	// ============================================
	// POST PROCESS (Lumen)
	// ============================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess|Lumen", meta = (ClampMin = "1.0", ClampMax = "8.0"))
	float FinalGatherQuality = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess|Lumen", meta = (ClampMin = "0.5", ClampMax = "4.0"))
	float FinalGatherLightingUpdateSpeed = 2.0f;

	// ============================================
	// POST PROCESS (AO)
	// ============================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess|AO", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AmbientOcclusionIntensity = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess|AO")
	float AmbientOcclusionRadius = 200.0f;

	// ============================================
	// POST PROCESS (Bloom)
	// ============================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess|Bloom", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float BloomIntensity = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcess|Bloom")
	float BloomThreshold = 2.0f;
};

/**
 * Data asset containing lighting presets
 * Create instances in Content Browser for different times of day
 */
UCLASS(BlueprintType)
class ALIS_API ULightingPresetDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Display name for UI/debugging
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset")
	FName PresetName;

	// Preset configuration
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset")
	FLightingPreset Settings;

	// Transition duration when blending to this preset (seconds)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transition", meta = (ClampMin = "0.0"))
	float TransitionDuration = 2.0f;

	// Asset Manager support
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId("LightingPreset", GetFName());
	}
};
```

### LightingPresetDataAsset.cpp

```cpp
// LightingPresetDataAsset.cpp

#include "LightingPresetDataAsset.h"
```

---

### LightingSubsystem.h

```cpp
// LightingSubsystem.h
// World subsystem for managing lighting and time-of-day
// Uses data-driven presets from ULightingPresetDataAsset

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LightingPresetDataAsset.h"
#include "LightingSubsystem.generated.h"

class USkyAtmosphereComponent;
class USkyLightComponent;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLightingPresetChanged, ULightingPresetDataAsset*, NewPreset);

/**
 * World subsystem for lighting management
 * Handles time-of-day transitions and preset application
 */
UCLASS()
class ALIS_API ULightingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ============================================
	// PRESET MANAGEMENT
	// ============================================

	// Apply preset instantly
	UFUNCTION(BlueprintCallable, Category = "Lighting")
	void ApplyPreset(ULightingPresetDataAsset* Preset);

	// Apply preset with transition
	UFUNCTION(BlueprintCallable, Category = "Lighting")
	void TransitionToPreset(ULightingPresetDataAsset* Preset, float Duration = -1.0f);

	// Get current active preset
	UFUNCTION(BlueprintPure, Category = "Lighting")
	ULightingPresetDataAsset* GetCurrentPreset() const { return CurrentPreset; }

	// Check if transition is in progress
	UFUNCTION(BlueprintPure, Category = "Lighting")
	bool IsTransitioning() const { return bIsTransitioning; }

	// ============================================
	// COMPONENT REGISTRATION
	// ============================================

	UFUNCTION(BlueprintCallable, Category = "Lighting|Components")
	void RegisterSkyAtmosphere(USkyAtmosphereComponent* Component);

	UFUNCTION(BlueprintCallable, Category = "Lighting|Components")
	void RegisterSkyLight(USkyLightComponent* Component);

	UFUNCTION(BlueprintCallable, Category = "Lighting|Components")
	void RegisterDirectionalLight(UDirectionalLightComponent* Component);

	UFUNCTION(BlueprintCallable, Category = "Lighting|Components")
	void RegisterHeightFog(UExponentialHeightFogComponent* Component);

	UFUNCTION(BlueprintCallable, Category = "Lighting|Components")
	void RegisterPostProcess(UPostProcessComponent* Component);

	// ============================================
	// EVENTS
	// ============================================

	UPROPERTY(BlueprintAssignable, Category = "Lighting|Events")
	FOnLightingPresetChanged OnPresetChanged;

	// ============================================
	// DEFAULT PRESETS (built-in)
	// ============================================

	UFUNCTION(BlueprintPure, Category = "Lighting|Defaults")
	static FLightingPreset GetDayPreset();

	UFUNCTION(BlueprintPure, Category = "Lighting|Defaults")
	static FLightingPreset GetSunsetPreset();

	UFUNCTION(BlueprintPure, Category = "Lighting|Defaults")
	static FLightingPreset GetNightPreset();

	UFUNCTION(BlueprintPure, Category = "Lighting|Defaults")
	static FLightingPreset GetSunrisePreset();

	UFUNCTION(BlueprintCallable, Category = "Lighting")
	void ApplyBuiltInPreset(FName PresetName);

	void ApplySettingsDirectly(const FLightingPreset& Settings);

private:
	void ApplySettings(const FLightingPreset& Settings);
	void TickTransition(float DeltaTime);
	static FLightingPreset LerpPreset(const FLightingPreset& A, const FLightingPreset& B, float Alpha);
	void SetupPostProcessSettings(const FLightingPreset& Settings);

	// STATE
	UPROPERTY()
	TObjectPtr<ULightingPresetDataAsset> CurrentPreset;

	UPROPERTY()
	TObjectPtr<ULightingPresetDataAsset> TargetPreset;

	FLightingPreset CurrentSettings;
	FLightingPreset StartSettings;
	FLightingPreset TargetSettings;

	float TransitionTime = 0.0f;
	float TransitionDuration = 0.0f;
	bool bIsTransitioning = false;

	FTimerHandle TransitionTimerHandle;

	// REGISTERED COMPONENTS
	UPROPERTY()
	TWeakObjectPtr<USkyAtmosphereComponent> SkyAtmosphere;

	UPROPERTY()
	TWeakObjectPtr<USkyLightComponent> SkyLight;

	UPROPERTY()
	TWeakObjectPtr<UDirectionalLightComponent> DirectionalLight;

	UPROPERTY()
	TWeakObjectPtr<UExponentialHeightFogComponent> HeightFog;

	UPROPERTY()
	TWeakObjectPtr<UPostProcessComponent> PostProcess;
};
```

### LightingSubsystem.cpp

```cpp
// LightingSubsystem.cpp
// World subsystem for managing lighting and time-of-day

#include "LightingSubsystem.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"

bool ULightingSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		return World->IsGameWorld();
	}
	return false;
}

void ULightingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CurrentSettings = GetDayPreset();
}

void ULightingSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TransitionTimerHandle);
	}
	Super::Deinitialize();
}

void ULightingSubsystem::ApplyPreset(ULightingPresetDataAsset* Preset)
{
	if (!Preset) return;

	bIsTransitioning = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TransitionTimerHandle);
	}

	CurrentPreset = Preset;
	CurrentSettings = Preset->Settings;
	ApplySettings(CurrentSettings);
	OnPresetChanged.Broadcast(Preset);
}

void ULightingSubsystem::TransitionToPreset(ULightingPresetDataAsset* Preset, float Duration)
{
	if (!Preset) return;

	if (Duration < 0.0f)
	{
		Duration = Preset->TransitionDuration;
	}

	if (Duration <= 0.0f)
	{
		ApplyPreset(Preset);
		return;
	}

	TargetPreset = Preset;
	StartSettings = CurrentSettings;
	TargetSettings = Preset->Settings;
	TransitionTime = 0.0f;
	TransitionDuration = Duration;
	bIsTransitioning = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			TransitionTimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (UWorld* World = GetWorld())
				{
					TickTransition(World->GetDeltaSeconds());
				}
			}),
			0.016f,
			true
		);
	}
}

void ULightingSubsystem::TickTransition(float DeltaTime)
{
	if (!bIsTransitioning) return;

	TransitionTime += DeltaTime;
	const float Alpha = FMath::Clamp(TransitionTime / TransitionDuration, 0.0f, 1.0f);

	CurrentSettings = LerpPreset(StartSettings, TargetSettings, Alpha);
	ApplySettings(CurrentSettings);

	if (Alpha >= 1.0f)
	{
		bIsTransitioning = false;
		CurrentPreset = TargetPreset;
		TargetPreset = nullptr;

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(TransitionTimerHandle);
		}
		OnPresetChanged.Broadcast(CurrentPreset);
	}
}

FLightingPreset ULightingSubsystem::LerpPreset(const FLightingPreset& A, const FLightingPreset& B, float Alpha)
{
	FLightingPreset Result;

	Result.SunIntensity = FMath::Lerp(A.SunIntensity, B.SunIntensity, Alpha);
	Result.SunColor = FMath::Lerp(A.SunColor, B.SunColor, Alpha);
	Result.SunRotation = FMath::Lerp(A.SunRotation, B.SunRotation, Alpha);
	Result.SunSourceAngle = FMath::Lerp(A.SunSourceAngle, B.SunSourceAngle, Alpha);

	Result.SkyLightIntensity = FMath::Lerp(A.SkyLightIntensity, B.SkyLightIntensity, Alpha);
	Result.LowerHemisphereColor = FMath::Lerp(A.LowerHemisphereColor, B.LowerHemisphereColor, Alpha);

	Result.FogDensity = FMath::Lerp(A.FogDensity, B.FogDensity, Alpha);
	Result.FogHeightFalloff = FMath::Lerp(A.FogHeightFalloff, B.FogHeightFalloff, Alpha);
	Result.FogInscatteringColor = FMath::Lerp(A.FogInscatteringColor, B.FogInscatteringColor, Alpha);
	Result.FogMaxOpacity = FMath::Lerp(A.FogMaxOpacity, B.FogMaxOpacity, Alpha);
	Result.bEnableVolumetricFog = Alpha < 0.5f ? A.bEnableVolumetricFog : B.bEnableVolumetricFog;
	Result.VolumetricFogScatteringDistribution = FMath::Lerp(A.VolumetricFogScatteringDistribution, B.VolumetricFogScatteringDistribution, Alpha);
	Result.VolumetricFogExtinctionScale = FMath::Lerp(A.VolumetricFogExtinctionScale, B.VolumetricFogExtinctionScale, Alpha);
	Result.VolumetricFogDistance = FMath::Lerp(A.VolumetricFogDistance, B.VolumetricFogDistance, Alpha);

	Result.FinalGatherQuality = FMath::Lerp(A.FinalGatherQuality, B.FinalGatherQuality, Alpha);
	Result.FinalGatherLightingUpdateSpeed = FMath::Lerp(A.FinalGatherLightingUpdateSpeed, B.FinalGatherLightingUpdateSpeed, Alpha);
	Result.AmbientOcclusionIntensity = FMath::Lerp(A.AmbientOcclusionIntensity, B.AmbientOcclusionIntensity, Alpha);
	Result.AmbientOcclusionRadius = FMath::Lerp(A.AmbientOcclusionRadius, B.AmbientOcclusionRadius, Alpha);
	Result.BloomIntensity = FMath::Lerp(A.BloomIntensity, B.BloomIntensity, Alpha);
	Result.BloomThreshold = FMath::Lerp(A.BloomThreshold, B.BloomThreshold, Alpha);

	return Result;
}

void ULightingSubsystem::ApplySettings(const FLightingPreset& Settings)
{
	if (DirectionalLight.IsValid())
	{
		DirectionalLight->SetIntensity(Settings.SunIntensity);
		DirectionalLight->SetLightColor(Settings.SunColor.ToFColor(true));
		DirectionalLight->SetRelativeRotation(Settings.SunRotation);
		DirectionalLight->LightSourceAngle = Settings.SunSourceAngle;
	}

	if (SkyLight.IsValid())
	{
		SkyLight->Intensity = Settings.SkyLightIntensity;
		SkyLight->LowerHemisphereColor = Settings.LowerHemisphereColor;
		SkyLight->RecaptureSky();
	}

	if (HeightFog.IsValid())
	{
		HeightFog->SetFogDensity(Settings.FogDensity);
		HeightFog->SetFogHeightFalloff(Settings.FogHeightFalloff);
		HeightFog->SetFogInscatteringColor(Settings.FogInscatteringColor);
		HeightFog->SetFogMaxOpacity(Settings.FogMaxOpacity);
		HeightFog->SetVolumetricFog(Settings.bEnableVolumetricFog);
		HeightFog->VolumetricFogScatteringDistribution = Settings.VolumetricFogScatteringDistribution;
		HeightFog->VolumetricFogExtinctionScale = Settings.VolumetricFogExtinctionScale;
		HeightFog->VolumetricFogDistance = Settings.VolumetricFogDistance;
	}

	SetupPostProcessSettings(Settings);
}

void ULightingSubsystem::SetupPostProcessSettings(const FLightingPreset& Settings)
{
	if (!PostProcess.IsValid()) return;

	FPostProcessSettings& PPSettings = PostProcess->Settings;

	PPSettings.bOverride_LumenFinalGatherQuality = true;
	PPSettings.LumenFinalGatherQuality = Settings.FinalGatherQuality;

	PPSettings.bOverride_LumenFinalGatherLightingUpdateSpeed = true;
	PPSettings.LumenFinalGatherLightingUpdateSpeed = Settings.FinalGatherLightingUpdateSpeed;

	PPSettings.bOverride_AmbientOcclusionIntensity = true;
	PPSettings.AmbientOcclusionIntensity = Settings.AmbientOcclusionIntensity;

	PPSettings.bOverride_AmbientOcclusionRadius = true;
	PPSettings.AmbientOcclusionRadius = Settings.AmbientOcclusionRadius;

	PPSettings.bOverride_BloomIntensity = true;
	PPSettings.BloomIntensity = Settings.BloomIntensity;

	PPSettings.bOverride_BloomThreshold = true;
	PPSettings.BloomThreshold = Settings.BloomThreshold;

	PPSettings.bOverride_SceneFringeIntensity = true;
	PPSettings.SceneFringeIntensity = 0.0f;

	PPSettings.bOverride_VignetteIntensity = true;
	PPSettings.VignetteIntensity = 0.0f;

	PPSettings.bOverride_MotionBlurAmount = true;
	PPSettings.MotionBlurAmount = 0.0f;
}

// Component Registration
void ULightingSubsystem::RegisterSkyAtmosphere(USkyAtmosphereComponent* Component) { SkyAtmosphere = Component; }

void ULightingSubsystem::RegisterSkyLight(USkyLightComponent* Component)
{
	SkyLight = Component;
	if (SkyLight.IsValid())
	{
		SkyLight->SetMobility(EComponentMobility::Movable);
		SkyLight->SourceType = ESkyLightSourceType::SLS_CapturedScene;
		SkyLight->bRealTimeCapture = true;
		SkyLight->bLowerHemisphereIsBlack = false;
		SkyLight->CastShadows = false;
	}
}

void ULightingSubsystem::RegisterDirectionalLight(UDirectionalLightComponent* Component)
{
	DirectionalLight = Component;
	if (DirectionalLight.IsValid())
	{
		DirectionalLight->SetMobility(EComponentMobility::Movable);
		DirectionalLight->bAtmosphereSunLight = true;
		DirectionalLight->CastShadows = true;
		DirectionalLight->ContactShadowLength = 0.1f;
	}
}

void ULightingSubsystem::RegisterHeightFog(UExponentialHeightFogComponent* Component) { HeightFog = Component; }
void ULightingSubsystem::RegisterPostProcess(UPostProcessComponent* Component) { PostProcess = Component; }

// Built-in presets
void ULightingSubsystem::ApplyBuiltInPreset(FName PresetName)
{
	FLightingPreset Settings;
	if (PresetName == "Day") Settings = GetDayPreset();
	else if (PresetName == "Sunset") Settings = GetSunsetPreset();
	else if (PresetName == "Night") Settings = GetNightPreset();
	else if (PresetName == "Sunrise") Settings = GetSunrisePreset();
	else Settings = GetDayPreset();
	ApplySettingsDirectly(Settings);
}

void ULightingSubsystem::ApplySettingsDirectly(const FLightingPreset& Settings)
{
	bIsTransitioning = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TransitionTimerHandle);
	}
	CurrentPreset = nullptr;
	CurrentSettings = Settings;
	ApplySettings(CurrentSettings);
}

FLightingPreset ULightingSubsystem::GetDayPreset()
{
	FLightingPreset P;
	P.SunIntensity = 8.0f;
	P.SunColor = FLinearColor(1.0f, 0.98f, 0.94f, 1.0f);
	P.SunRotation = FRotator(-60.0f, 0.0f, 0.0f);
	P.SunSourceAngle = 2.0f;
	P.SkyLightIntensity = 1.0f;
	P.LowerHemisphereColor = FLinearColor(0.376f, 0.376f, 0.376f, 1.0f);
	P.FogDensity = 0.005f;
	P.FogInscatteringColor = FLinearColor(0.69f, 0.77f, 0.87f, 1.0f);
	return P;
}

FLightingPreset ULightingSubsystem::GetSunsetPreset()
{
	FLightingPreset P;
	P.SunIntensity = 3.0f;
	P.SunColor = FLinearColor(1.0f, 0.39f, 0.28f, 1.0f);
	P.SunRotation = FRotator(-10.0f, 0.0f, 0.0f);
	P.SunSourceAngle = 3.0f;
	P.SkyLightIntensity = 0.7f;
	P.LowerHemisphereColor = FLinearColor(0.545f, 0.27f, 0.075f, 1.0f);
	P.FogDensity = 0.015f;
	P.FogInscatteringColor = FLinearColor(1.0f, 0.6f, 0.4f, 1.0f);
	return P;
}

FLightingPreset ULightingSubsystem::GetNightPreset()
{
	FLightingPreset P;
	P.SunIntensity = 0.1f;
	P.SunColor = FLinearColor(0.255f, 0.412f, 0.882f, 1.0f);
	P.SunRotation = FRotator(30.0f, 0.0f, 0.0f);
	P.SunSourceAngle = 0.5f;
	P.SkyLightIntensity = 0.3f;
	P.LowerHemisphereColor = FLinearColor(0.102f, 0.102f, 0.18f, 1.0f);
	P.FogDensity = 0.02f;
	P.FogInscatteringColor = FLinearColor(0.1f, 0.1f, 0.2f, 1.0f);
	P.BloomIntensity = 0.5f;
	P.BloomThreshold = 1.0f;
	return P;
}

FLightingPreset ULightingSubsystem::GetSunrisePreset()
{
	FLightingPreset P;
	P.SunIntensity = 2.0f;
	P.SunColor = FLinearColor(1.0f, 0.627f, 0.478f, 1.0f);
	P.SunRotation = FRotator(-10.0f, 90.0f, 0.0f);
	P.SunSourceAngle = 3.0f;
	P.SkyLightIntensity = 0.7f;
	P.LowerHemisphereColor = FLinearColor(0.545f, 0.27f, 0.075f, 1.0f);
	P.FogDensity = 0.02f;
	P.FogInscatteringColor = FLinearColor(1.0f, 0.7f, 0.5f, 1.0f);
	return P;
}
```

---

### LightingSetupActor.h (refactored)

```cpp
// LightingSetupActor.h
// Utility actor that creates lighting components and registers them with LightingSubsystem

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LightingPresetDataAsset.h"
#include "LightingSetupActor.generated.h"

class USkyAtmosphereComponent;
class USkyLightComponent;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;
class ULightingSubsystem;

UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "Lighting Setup Actor"))
class ALIS_API ALightingSetupActor : public AActor
{
	GENERATED_BODY()

public:
	ALightingSetupActor();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting|Preset")
	TObjectPtr<ULightingPresetDataAsset> InitialPreset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting|Preset")
	bool bUseBuiltInPreset = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting|Preset", meta = (EditCondition = "bUseBuiltInPreset"))
	FName BuiltInPresetName = "Day";

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lighting|Components")
	TObjectPtr<USkyAtmosphereComponent> SkyAtmosphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lighting|Components")
	TObjectPtr<USkyLightComponent> SkyLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lighting|Components")
	TObjectPtr<UDirectionalLightComponent> DirectionalLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lighting|Components")
	TObjectPtr<UExponentialHeightFogComponent> HeightFog;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lighting|Components")
	TObjectPtr<UPostProcessComponent> PostProcess;

	UFUNCTION(BlueprintPure, Category = "Lighting")
	ULightingSubsystem* GetLightingSubsystem() const;

	UFUNCTION(BlueprintCallable, Category = "Lighting")
	void ApplyPreset(ULightingPresetDataAsset* Preset);

	UFUNCTION(BlueprintCallable, Category = "Lighting")
	void TransitionToPreset(ULightingPresetDataAsset* Preset, float Duration = 2.0f);

private:
	void RegisterWithSubsystem();
	void ApplyInitialPreset();
};
```

### LightingSetupActor.cpp (refactored)

```cpp
// LightingSetupActor.cpp

#include "LightingSetupActor.h"
#include "LightingSubsystem.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"

ALightingSetupActor::ALightingSetupActor()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	SkyAtmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
	SkyAtmosphere->SetupAttachment(Root);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->SourceType = ESkyLightSourceType::SLS_CapturedScene;
	SkyLight->bRealTimeCapture = true;
	SkyLight->bLowerHemisphereIsBlack = false;
	SkyLight->CastShadows = false;

	DirectionalLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("DirectionalLight"));
	DirectionalLight->SetupAttachment(Root);
	DirectionalLight->SetMobility(EComponentMobility::Movable);
	DirectionalLight->SetRelativeRotation(FRotator(-60.0f, 0.0f, 0.0f));
	DirectionalLight->bAtmosphereSunLight = true;
	DirectionalLight->CastShadows = true;

	HeightFog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("HeightFog"));
	HeightFog->SetupAttachment(Root);
	HeightFog->bEnableVolumetricFog = true;

	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	PostProcess->SetupAttachment(Root);
	PostProcess->bUnbound = false;
	PostProcess->Priority = -1.0f;
}

void ALightingSetupActor::BeginPlay()
{
	Super::BeginPlay();
	RegisterWithSubsystem();
	ApplyInitialPreset();
}

void ALightingSetupActor::RegisterWithSubsystem()
{
	ULightingSubsystem* Subsystem = GetLightingSubsystem();
	if (!Subsystem) return;

	Subsystem->RegisterSkyAtmosphere(SkyAtmosphere);
	Subsystem->RegisterSkyLight(SkyLight);
	Subsystem->RegisterDirectionalLight(DirectionalLight);
	Subsystem->RegisterHeightFog(HeightFog);
	Subsystem->RegisterPostProcess(PostProcess);
}

void ALightingSetupActor::ApplyInitialPreset()
{
	ULightingSubsystem* Subsystem = GetLightingSubsystem();
	if (!Subsystem) return;

	if (InitialPreset)
	{
		Subsystem->ApplyPreset(InitialPreset);
	}
	else if (bUseBuiltInPreset)
	{
		Subsystem->ApplyBuiltInPreset(BuiltInPresetName);
	}
}

ULightingSubsystem* ALightingSetupActor::GetLightingSubsystem() const
{
	if (UWorld* World = GetWorld())
	{
		return World->GetSubsystem<ULightingSubsystem>();
	}
	return nullptr;
}

void ALightingSetupActor::ApplyPreset(ULightingPresetDataAsset* Preset)
{
	if (ULightingSubsystem* Subsystem = GetLightingSubsystem())
	{
		Subsystem->ApplyPreset(Preset);
	}
}

void ALightingSetupActor::TransitionToPreset(ULightingPresetDataAsset* Preset, float Duration)
{
	if (ULightingSubsystem* Subsystem = GetLightingSubsystem())
	{
		Subsystem->TransitionToPreset(Preset, Duration);
	}
}
```
