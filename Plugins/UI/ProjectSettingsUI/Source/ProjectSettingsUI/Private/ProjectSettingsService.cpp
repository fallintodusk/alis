// Copyright ALIS. All Rights Reserved.

#include "ProjectSettingsService.h"

#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"
#include "Scalability.h"

DEFINE_LOG_CATEGORY_STATIC(LogSettingsService, Log, All);

// Static members
const TCHAR* UProjectSettingsService::AudioSection = TEXT("AlisAudio");
const TCHAR* UProjectSettingsService::GameSection = TEXT("AlisGame");
TWeakObjectPtr<UProjectSettingsService> UProjectSettingsService::Instance;

UProjectSettingsService* UProjectSettingsService::Get()
{
	if (!Instance.IsValid())
	{
		Instance = NewObject<UProjectSettingsService>(GetTransientPackage(), NAME_None, RF_MarkAsRootSet);
	}
	return Instance.Get();
}

// =============================================================================
// Public API
// =============================================================================

FProjectUserSettings UProjectSettingsService::LoadSettings()
{
	FProjectUserSettings Settings;
	LoadGraphicsSettings(Settings);
	LoadAudioSettings(Settings);
	LoadGameSettings(Settings);
	UE_LOG(LogSettingsService, Display, TEXT("Settings loaded"));
	return Settings;
}

void UProjectSettingsService::Apply(const FProjectUserSettings& Settings, const FProjectSettingsDelta& Delta)
{
	if (Delta.IsEmpty())
	{
		UE_LOG(LogSettingsService, Verbose, TEXT("Apply: No changes"));
		return;
	}

	Delta.Log(TEXT("Apply"));

	ApplyGraphics(Settings, Delta);
	ApplyAudio(Settings, Delta);
	ApplyGame(Settings, Delta);

	if (GConfig)
	{
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	UE_LOG(LogSettingsService, Display, TEXT("Settings applied successfully"));
}

FProjectUserSettings UProjectSettingsService::ResetToDefaults()
{
	if (GEngine)
	{
		if (UGameUserSettings* UserSettings = GEngine->GetGameUserSettings())
		{
			UserSettings->SetToDefaults();
			UserSettings->ApplySettings(false);
			UserSettings->SaveSettings();
		}
	}

	Scalability::FQualityLevels Defaults;
	Defaults.SetFromSingleQualityLevel(2);
	Scalability::SetQualityLevels(Defaults);
	Scalability::SaveState(GGameUserSettingsIni);

	if (GConfig)
	{
		GConfig->SetFloat(AudioSection, TEXT("MasterVolume"), 1.0f, GGameUserSettingsIni);
		GConfig->SetFloat(AudioSection, TEXT("MusicVolume"), 1.0f, GGameUserSettingsIni);
		GConfig->SetFloat(AudioSection, TEXT("EffectsVolume"), 1.0f, GGameUserSettingsIni);
		GConfig->SetBool(GameSection, TEXT("bInvertMouseY"), false, GGameUserSettingsIni);
		GConfig->SetBool(GameSection, TEXT("bShowHints"), true, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	UE_LOG(LogSettingsService, Display, TEXT("Settings reset to defaults"));
	return LoadSettings();
}

TArray<FString> UProjectSettingsService::GetSupportedResolutions()
{
	TArray<FString> Resolutions;
	TArray<FIntPoint> Supported;
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Supported);

	TSet<FString> Added;
	for (const FIntPoint& Res : Supported)
	{
		if (Res.X < 1280 || Res.Y < 720) continue;

		const float Aspect = static_cast<float>(Res.X) / static_cast<float>(Res.Y);
		if (Aspect < 1.5f || Aspect > 2.5f) continue;

		const FString ResStr = FString::Printf(TEXT("%dx%d"), Res.X, Res.Y);
		if (!Added.Contains(ResStr))
		{
			Resolutions.Add(ResStr);
			Added.Add(ResStr);
		}
	}

	if (Resolutions.Num() == 0)
	{
		Resolutions.Add(TEXT("1280x720"));
		Resolutions.Add(TEXT("1920x1080"));
		Resolutions.Add(TEXT("2560x1440"));
		Resolutions.Add(TEXT("3840x2160"));
	}

	return Resolutions;
}

FString UProjectSettingsService::GetCurrentResolution()
{
	if (GEngine)
	{
		if (UGameUserSettings* UserSettings = GEngine->GetGameUserSettings())
		{
			const FIntPoint Res = UserSettings->GetScreenResolution();
			return FString::Printf(TEXT("%dx%d"), Res.X, Res.Y);
		}
	}
	return TEXT("1920x1080");
}

// =============================================================================
// Load Helpers
// =============================================================================

void UProjectSettingsService::LoadGraphicsSettings(FProjectUserSettings& Out)
{
	if (!GEngine) return;

	UGameUserSettings* UserSettings = GEngine->GetGameUserSettings();
	if (!UserSettings) return;

	const FIntPoint Res = UserSettings->GetScreenResolution();
	Out.Resolution = FString::Printf(TEXT("%dx%d"), Res.X, Res.Y);

	const EWindowMode::Type Mode = UserSettings->GetFullscreenMode();
	Out.bFullscreen = (Mode == EWindowMode::Fullscreen || Mode == EWindowMode::WindowedFullscreen);
	Out.bVSync = UserSettings->IsVSyncEnabled();

	UE_LOG(LogSettingsService, Verbose, TEXT("LoadGraphics: Res=%s Fullscreen=%d (Mode=%d) VSync=%d"),
		*Out.Resolution, Out.bFullscreen, static_cast<int32>(Mode), Out.bVSync);

	Scalability::FQualityLevels Levels = Scalability::GetQualityLevels();
	Out.ShadowQuality = Levels.ShadowQuality;
	Out.TextureQuality = Levels.TextureQuality;
	Out.EffectsQuality = Levels.EffectsQuality;
	Out.ViewDistance = Levels.ViewDistanceQuality;
	Out.AntiAliasing = Levels.AntiAliasingQuality;
}

void UProjectSettingsService::LoadAudioSettings(FProjectUserSettings& Out)
{
	if (!GConfig) return;

	GConfig->GetFloat(AudioSection, TEXT("MasterVolume"), Out.MasterVolume, GGameUserSettingsIni);
	GConfig->GetFloat(AudioSection, TEXT("MusicVolume"), Out.MusicVolume, GGameUserSettingsIni);
	GConfig->GetFloat(AudioSection, TEXT("EffectsVolume"), Out.EffectsVolume, GGameUserSettingsIni);

	Out.MasterVolume = FMath::Clamp(Out.MasterVolume, 0.0f, 1.0f);
	Out.MusicVolume = FMath::Clamp(Out.MusicVolume, 0.0f, 1.0f);
	Out.EffectsVolume = FMath::Clamp(Out.EffectsVolume, 0.0f, 1.0f);
}

void UProjectSettingsService::LoadGameSettings(FProjectUserSettings& Out)
{
	if (!GConfig) return;

	GConfig->GetBool(GameSection, TEXT("bInvertMouseY"), Out.bInvertMouseY, GGameUserSettingsIni);
	GConfig->GetBool(GameSection, TEXT("bShowHints"), Out.bShowHints, GGameUserSettingsIni);
}

// =============================================================================
// Apply Helpers - use GET_MEMBER_NAME_CHECKED for compile-time safety
// =============================================================================

void UProjectSettingsService::ApplyGraphics(const FProjectUserSettings& S, const FProjectSettingsDelta& D)
{
	if (!GEngine) return;

	UGameUserSettings* UserSettings = GEngine->GetGameUserSettings();
	if (!UserSettings) return;

	// Display settings
	if (D.HasDisplay())
	{
		bool bNeedApplyResolution = false;

		// Resolution - compile-time validated property name
		if (D.IsChanged(GET_MEMBER_NAME_CHECKED(FProjectUserSettings, Resolution)))
		{
			FString XStr, YStr;
			if (S.Resolution.Split(TEXT("x"), &XStr, &YStr))
			{
				const int32 X = FCString::Atoi(*XStr);
				const int32 Y = FCString::Atoi(*YStr);
				if (X > 0 && Y > 0)
				{
					UE_LOG(LogSettingsService, Display, TEXT("ApplyGraphics: Setting resolution to %dx%d"), X, Y);
					UserSettings->SetScreenResolution(FIntPoint(X, Y));
					bNeedApplyResolution = true;
				}
			}
		}

		// Fullscreen - use true Fullscreen (not WindowedFullscreen) to support resolution changes
		if (D.IsChanged(GET_MEMBER_NAME_CHECKED(FProjectUserSettings, bFullscreen)))
		{
			// Fullscreen = exclusive fullscreen (supports custom resolution)
			// WindowedFullscreen = borderless window at desktop res (ignores resolution setting)
			const EWindowMode::Type Mode = S.bFullscreen ? EWindowMode::Fullscreen : EWindowMode::Windowed;
			UE_LOG(LogSettingsService, Display, TEXT("ApplyGraphics: Setting fullscreen=%d (Mode=%d)"), S.bFullscreen, static_cast<int32>(Mode));
			UserSettings->SetFullscreenMode(Mode);
			bNeedApplyResolution = true;
		}

		// VSync
		if (D.IsChanged(GET_MEMBER_NAME_CHECKED(FProjectUserSettings, bVSync)))
		{
			UE_LOG(LogSettingsService, Display, TEXT("ApplyGraphics: Setting VSync=%d"), S.bVSync);
			UserSettings->SetVSyncEnabled(S.bVSync);
		}

		// Apply settings at runtime
		if (bNeedApplyResolution)
		{
			// Apply + confirm immediately (no timeout dialog)
			UserSettings->ApplySettings(false);
			UserSettings->ConfirmVideoMode();

			UE_LOG(LogSettingsService, Display, TEXT("ApplyGraphics: Applied Res=%dx%d Mode=%d"),
				UserSettings->GetScreenResolution().X, UserSettings->GetScreenResolution().Y,
				static_cast<int32>(UserSettings->GetFullscreenMode()));
		}
		else
		{
			// VSync-only change - still need to save
			UserSettings->SaveSettings();
		}
	}

	// Scalability settings - only if any scalability flag set
	if (D.HasScalability())
	{
		Scalability::FQualityLevels Levels = Scalability::GetQualityLevels();

		if (D.IsChanged(GET_MEMBER_NAME_CHECKED(FProjectUserSettings, ShadowQuality)))
			Levels.ShadowQuality = FMath::Clamp(S.ShadowQuality, 0, 3);
		if (D.IsChanged(GET_MEMBER_NAME_CHECKED(FProjectUserSettings, TextureQuality)))
			Levels.TextureQuality = FMath::Clamp(S.TextureQuality, 0, 3);
		if (D.IsChanged(GET_MEMBER_NAME_CHECKED(FProjectUserSettings, EffectsQuality)))
			Levels.EffectsQuality = FMath::Clamp(S.EffectsQuality, 0, 3);
		if (D.IsChanged(GET_MEMBER_NAME_CHECKED(FProjectUserSettings, ViewDistance)))
			Levels.ViewDistanceQuality = FMath::Clamp(S.ViewDistance, 0, 3);
		if (D.IsChanged(GET_MEMBER_NAME_CHECKED(FProjectUserSettings, AntiAliasing)))
			Levels.AntiAliasingQuality = FMath::Clamp(S.AntiAliasing, 0, 3);

		Scalability::SetQualityLevels(Levels);
		Scalability::SaveState(GGameUserSettingsIni);
	}
}

void UProjectSettingsService::ApplyAudio(const FProjectUserSettings& S, const FProjectSettingsDelta& D)
{
	if (!GConfig || !D.HasAudio()) return;

	if (D.IsChanged(GET_MEMBER_NAME_CHECKED(FProjectUserSettings, MasterVolume)))
	{
		UE_LOG(LogSettingsService, Display, TEXT("ApplyAudio: MasterVolume=%.2f"), S.MasterVolume);
		GConfig->SetFloat(AudioSection, TEXT("MasterVolume"), S.MasterVolume, GGameUserSettingsIni);
	}
	if (D.IsChanged(GET_MEMBER_NAME_CHECKED(FProjectUserSettings, MusicVolume)))
	{
		UE_LOG(LogSettingsService, Display, TEXT("ApplyAudio: MusicVolume=%.2f"), S.MusicVolume);
		GConfig->SetFloat(AudioSection, TEXT("MusicVolume"), S.MusicVolume, GGameUserSettingsIni);
	}
	if (D.IsChanged(GET_MEMBER_NAME_CHECKED(FProjectUserSettings, EffectsVolume)))
	{
		UE_LOG(LogSettingsService, Display, TEXT("ApplyAudio: EffectsVolume=%.2f"), S.EffectsVolume);
		GConfig->SetFloat(AudioSection, TEXT("EffectsVolume"), S.EffectsVolume, GGameUserSettingsIni);
	}
}

void UProjectSettingsService::ApplyGame(const FProjectUserSettings& S, const FProjectSettingsDelta& D)
{
	if (!GConfig || !D.HasGame()) return;

	if (D.IsChanged(GET_MEMBER_NAME_CHECKED(FProjectUserSettings, bInvertMouseY)))
	{
		GConfig->SetBool(GameSection, TEXT("bInvertMouseY"), S.bInvertMouseY, GGameUserSettingsIni);
	}
	if (D.IsChanged(GET_MEMBER_NAME_CHECKED(FProjectUserSettings, bShowHints)))
	{
		GConfig->SetBool(GameSection, TEXT("bShowHints"), S.bShowHints, GGameUserSettingsIni);
	}
}
