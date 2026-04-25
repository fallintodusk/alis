// Fill out your copyright notice in the Description page of Project Settings.


#include "AlisGI.h"
#include "Experience/ProjectExperienceDescriptorBase.h"
#include "Experience/ProjectExperienceRegistry.h"
#include "GameFramework/GameUserSettings.h"
#include "GenericPlatform/GenericApplication.h" // FDisplayMetrics
#include "Misc/ConfigCacheIni.h"
#include "ProjectServiceLocator.h"
#include "Scalability.h"

DEFINE_LOG_CATEGORY_STATIC(LogAlis, Log, All);

// First-run: detect hardware and apply sensible defaults.
// Adds a one-time startup stall (RunHardwareBenchmark) on first boot only.
// Tradeoff: correct first frame vs fastest first boot. Acceptable for a single occurrence.
static void EnsureFirstRunDefaults()
{
	UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!UserSettings)
	{
		return;
	}

	// UE's SetToDefaults() sets LastConfirmedScreenResolution to 1280x720, so it's
	// never 0x0 even on first run. Instead check LastCPUBenchmarkResult which is -1.0
	// until RunHardwareBenchmark() has been called at least once.
	if (UserSettings->GetLastCPUBenchmarkResult() >= 0.0f)
	{
		return;
	}

	UE_LOG(LogAlis, Display, TEXT("First run detected (no confirmed resolution). Auto-detecting hardware."));

	// Desktop resolution via display metrics (not GSystemResolution which may already be 1280x720)
	FDisplayMetrics DisplayMetrics;
	FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);
	const int32 DesktopW = DisplayMetrics.PrimaryDisplayWidth;
	const int32 DesktopH = DisplayMetrics.PrimaryDisplayHeight;

	if (DesktopW >= 1280 && DesktopH >= 720)
	{
		UserSettings->SetScreenResolution(FIntPoint(DesktopW, DesktopH));
		// WindowedFullscreen: safe for mixed-monitor setups, no mode switch risk
		UserSettings->SetFullscreenMode(EWindowMode::WindowedFullscreen);
		UE_LOG(LogAlis, Display, TEXT("First run: desktop %dx%d, windowed fullscreen"), DesktopW, DesktopH);
	}

	// UE hardware benchmark detects GPU capability and sets scalability levels.
	// ApplyHardwareBenchmarkResults already applies + saves internally.
	UserSettings->RunHardwareBenchmark();
	UserSettings->ApplyHardwareBenchmarkResults();

	// Clamp to High (@2) max: performant out of the box, users raise manually.
	// High = TSR history 100% (not 200%), reasonable shadow/GI/reflection cost.
	Scalability::FQualityLevels Levels = Scalability::GetQualityLevels();
	const int32 Cap = 2;
	Levels.ResolutionQuality = FMath::Min(Levels.ResolutionQuality, 100.0f);
	Levels.ShadowQuality = FMath::Min(Levels.ShadowQuality, Cap);
	Levels.GlobalIlluminationQuality = FMath::Min(Levels.GlobalIlluminationQuality, Cap);
	Levels.ReflectionQuality = FMath::Min(Levels.ReflectionQuality, Cap);
	Levels.PostProcessQuality = FMath::Min(Levels.PostProcessQuality, Cap);
	Levels.TextureQuality = FMath::Min(Levels.TextureQuality, Cap);
	Levels.EffectsQuality = FMath::Min(Levels.EffectsQuality, Cap);
	Levels.FoliageQuality = FMath::Min(Levels.FoliageQuality, Cap);
	Levels.ShadingQuality = FMath::Min(Levels.ShadingQuality, Cap);
	Levels.AntiAliasingQuality = FMath::Min(Levels.AntiAliasingQuality, Cap);
	Levels.ViewDistanceQuality = FMath::Min(Levels.ViewDistanceQuality, Cap);
	Scalability::SetQualityLevels(Levels);

	// Single save: apply clamped levels + confirm video mode
	UserSettings->ApplySettings(false);
	UserSettings->ConfirmVideoMode();

	UE_LOG(LogAlis, Display, TEXT("First run: benchmark done, clamped to High, saved to GameUserSettings.ini"));
}

void UAlisGI::Init()
{
	Super::Init();

	UE_LOG(LogAlis, Log, TEXT("UAlisGI::Init - starting (EntryPointExperience from config/state)"));

	// First-run: detect hardware and apply sensible defaults before any map load.
	// Must happen here (GameInstance::Init), not in UI code, so the first rendered
	// frame already uses correct resolution and quality settings.
	EnsureFirstRunDefaults();

	// Resolve the entry experience name from config (default: MainMenu) to avoid hardcoding.
	FString EntryExperienceName = TEXT("MainMenuWorld");
	GConfig->GetString(TEXT("/Script/Alis.AlisGI"), TEXT("EntryPointExperience"), EntryExperienceName, GGameIni);
	UE_LOG(LogAlis, Log, TEXT("UAlisGI::Init - EntryPointExperience='%s' (provided to subsystems)"), *EntryExperienceName);
}
