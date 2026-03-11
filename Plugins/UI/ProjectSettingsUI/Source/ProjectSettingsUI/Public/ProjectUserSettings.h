// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProjectUserSettings.generated.h"

// =============================================================================
// SINGLE SOURCE OF TRUTH: All settings defined here via X-macro
// Format: M(Type, Member, ConfigKey, Category, DefaultValue)
// - Type: C++ type
// - Member: Field name (must match UPROPERTY below - UHT limitation)
// - ConfigKey: INI key name (used in GConfig read/write)
// - Category: Display, Scalability, Audio, Game
// - DefaultValue: Initial value
//
// NOTE: UHT doesn't expand macros for UPROPERTY, so fields must be manually
// declared below. Keep them IN SYNC with this macro! The macro is authoritative
// for: comparison logic (Delta::Compute), config keys (Load/Save), categories.
// =============================================================================
#define PROJECT_SETTINGS_FIELDS(M) \
	M(FString, Resolution,     "Resolution",     Display,     TEXT("1920x1080")) \
	M(bool,    bFullscreen,    "bFullscreen",    Display,     false)             \
	M(bool,    bVSync,         "bVSync",         Display,     true)              \
	M(int32,   ShadowQuality,  "ShadowQuality",  Scalability, 2)                 \
	M(int32,   TextureQuality, "TextureQuality", Scalability, 2)                 \
	M(int32,   EffectsQuality, "EffectsQuality", Scalability, 2)                 \
	M(int32,   ViewDistance,   "ViewDistance",   Scalability, 2)                 \
	M(int32,   AntiAliasing,   "AntiAliasing",   Scalability, 2)                 \
	M(float,   MasterVolume,   "MasterVolume",   Audio,       1.0f)              \
	M(float,   MusicVolume,    "MusicVolume",    Audio,       1.0f)              \
	M(float,   EffectsVolume,  "EffectsVolume",  Audio,       1.0f)              \
	M(bool,    bInvertMouseY,  "bInvertMouseY",  Game,        false)             \
	M(bool,    bShowHints,     "bShowHints",     Game,        true)

/**
 * Setting categories for grouping and batch operations.
 */
UENUM(BlueprintType)
enum class ESettingCategory : uint8
{
	Display,
	Scalability,
	Audio,
	Game
};

/**
 * Universal user settings data structure.
 * Single source of truth for all user-configurable settings.
 * Fields generated from PROJECT_SETTINGS_FIELDS macro.
 */
USTRUCT(BlueprintType)
struct PROJECTSETTINGSUI_API FProjectUserSettings
{
	GENERATED_BODY()

	// =========================================================================
	// Fields - MUST match PROJECT_SETTINGS_FIELDS macro above!
	// UHT limitation: can't generate UPROPERTY from macros.
	// If you add/remove a setting, update BOTH the macro AND these fields.
	// =========================================================================

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	FString Resolution = TEXT("1920x1080");

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	bool bFullscreen = false;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	bool bVSync = true;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	int32 ShadowQuality = 2;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	int32 TextureQuality = 2;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	int32 EffectsQuality = 2;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	int32 ViewDistance = 2;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	int32 AntiAliasing = 2;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	float MasterVolume = 1.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	float MusicVolume = 1.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	float EffectsVolume = 1.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	bool bInvertMouseY = false;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	bool bShowHints = true;

	// =========================================================================
	// Comparison
	// =========================================================================

	bool operator==(const FProjectUserSettings& Other) const
	{
		return Resolution == Other.Resolution
			&& bFullscreen == Other.bFullscreen
			&& bVSync == Other.bVSync
			&& ShadowQuality == Other.ShadowQuality
			&& TextureQuality == Other.TextureQuality
			&& EffectsQuality == Other.EffectsQuality
			&& ViewDistance == Other.ViewDistance
			&& AntiAliasing == Other.AntiAliasing
			&& FMath::IsNearlyEqual(MasterVolume, Other.MasterVolume, 0.01f)
			&& FMath::IsNearlyEqual(MusicVolume, Other.MusicVolume, 0.01f)
			&& FMath::IsNearlyEqual(EffectsVolume, Other.EffectsVolume, 0.01f)
			&& bInvertMouseY == Other.bInvertMouseY
			&& bShowHints == Other.bShowHints;
	}

	bool operator!=(const FProjectUserSettings& Other) const
	{
		return !(*this == Other);
	}

	// Category-specific comparison helpers
	bool GraphicsEquals(const FProjectUserSettings& Other) const
	{
		return Resolution == Other.Resolution
			&& bFullscreen == Other.bFullscreen
			&& bVSync == Other.bVSync
			&& ShadowQuality == Other.ShadowQuality
			&& TextureQuality == Other.TextureQuality
			&& EffectsQuality == Other.EffectsQuality
			&& ViewDistance == Other.ViewDistance
			&& AntiAliasing == Other.AntiAliasing;
	}

	bool AudioEquals(const FProjectUserSettings& Other) const
	{
		return FMath::IsNearlyEqual(MasterVolume, Other.MasterVolume, 0.01f)
			&& FMath::IsNearlyEqual(MusicVolume, Other.MusicVolume, 0.01f)
			&& FMath::IsNearlyEqual(EffectsVolume, Other.EffectsVolume, 0.01f);
	}

	bool GameEquals(const FProjectUserSettings& Other) const
	{
		return bInvertMouseY == Other.bInvertMouseY
			&& bShowHints == Other.bShowHints;
	}

	// Scalability level to string (0-3)
	static FString ScalabilityToString(int32 Level)
	{
		switch (Level)
		{
			case 0: return TEXT("Low");
			case 1: return TEXT("Medium");
			case 2: return TEXT("High");
			case 3: return TEXT("Epic");
			default: return TEXT("High");
		}
	}

	// String to scalability level
	static int32 StringToScalability(const FString& Label)
	{
		if (Label.Equals(TEXT("Low"), ESearchCase::IgnoreCase)) return 0;
		if (Label.Equals(TEXT("Medium"), ESearchCase::IgnoreCase)) return 1;
		if (Label.Equals(TEXT("High"), ESearchCase::IgnoreCase)) return 2;
		if (Label.Equals(TEXT("Epic"), ESearchCase::IgnoreCase)) return 3;
		return 2; // Default to High
	}
};
