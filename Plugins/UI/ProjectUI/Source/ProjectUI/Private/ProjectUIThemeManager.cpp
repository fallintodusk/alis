// Copyright ALIS. All Rights Reserved.

#include "Theme/ProjectUIThemeManager.h"
#include "Theme/ProjectUIThemeData.h"
#include "Settings/ProjectUISettings.h"
#include "Engine/AssetManager.h"

void UProjectUIThemeManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogTemp, Display, TEXT("ProjectUI Theme Manager initializing"));

	// Create embedded default theme as fallback
	CreateDefaultTheme();

	// Load configured default theme from project settings
	LoadDefaultTheme();

	UE_LOG(LogTemp, Display, TEXT("ProjectUI Theme Manager initialized with theme: %s"),
		ActiveTheme ? *ActiveTheme->ThemeName.ToString() : TEXT("Default"));
}

void UProjectUIThemeManager::Deinitialize()
{
	UE_LOG(LogTemp, Display, TEXT("ProjectUI Theme Manager shutting down"));

	// Clear delegate bindings
	OnThemeChanged.Clear();

	ActiveTheme = nullptr;
	DefaultTheme = nullptr;

	Super::Deinitialize();
}

UProjectUIThemeData* UProjectUIThemeManager::GetActiveTheme() const
{
	// Always return valid theme (fallback to default if needed)
	return ActiveTheme ? ActiveTheme : DefaultTheme;
}

bool UProjectUIThemeManager::SetActiveTheme(UProjectUIThemeData* NewTheme)
{
	if (!NewTheme)
	{
		UE_LOG(LogTemp, Error, TEXT("SetActiveTheme called with null theme"));
		return false;
	}

	if (ActiveTheme == NewTheme)
	{
		UE_LOG(LogTemp, Verbose, TEXT("Theme already active: %s"), *NewTheme->ThemeName.ToString());
		return true;
	}

	UE_LOG(LogTemp, Display, TEXT("Changing theme to: %s"), *NewTheme->ThemeName.ToString());

	ActiveTheme = NewTheme;

	// Notify all UI widgets to refresh their styling
	OnThemeChanged.Broadcast(ActiveTheme);

	return true;
}

bool UProjectUIThemeManager::LoadAndSetTheme(const FSoftObjectPath& ThemePath)
{
	if (!ThemePath.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("LoadAndSetTheme called with invalid path"));
		return false;
	}

	UE_LOG(LogTemp, Display, TEXT("Loading theme from path: %s"), *ThemePath.ToString());

	// Synchronously load the theme asset
	// For async loading, use LoadAndSetThemeAsync instead
	UProjectUIThemeData* LoadedTheme = Cast<UProjectUIThemeData>(ThemePath.TryLoad());

	if (!LoadedTheme)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load theme from path: %s"), *ThemePath.ToString());
		return false;
	}

	return SetActiveTheme(LoadedTheme);
}

void UProjectUIThemeManager::LoadAndSetThemeAsync(const FSoftObjectPath& ThemePath, FOnThemeLoadComplete OnComplete)
{
	if (!ThemePath.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("LoadAndSetThemeAsync called with invalid path"));
		if (OnComplete.IsBound())
		{
			OnComplete.Execute(false, nullptr);
		}
		return;
	}

	UE_LOG(LogTemp, Display, TEXT("Loading theme asynchronously from path: %s"), *ThemePath.ToString());

	// Get streamable manager for async loading
	UAssetManager& AssetManager = UAssetManager::Get();
	FStreamableManager& StreamableManager = AssetManager.GetStreamableManager();

	// Request async load with callback
	TSharedPtr<FStreamableHandle> Handle = StreamableManager.RequestAsyncLoad(
		ThemePath,
		[this, ThemePath, OnComplete]()
		{
			// Callback executes on game thread when loading completes
			UProjectUIThemeData* LoadedTheme = Cast<UProjectUIThemeData>(ThemePath.ResolveObject());

			if (!LoadedTheme)
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to load theme asynchronously from path: %s"), *ThemePath.ToString());
				if (OnComplete.IsBound())
				{
					OnComplete.Execute(false, nullptr);
				}
				return;
			}

			UE_LOG(LogTemp, Display, TEXT("Successfully loaded theme asynchronously: %s"), *LoadedTheme->ThemeName.ToString());

			// Set the loaded theme as active
			const bool bSuccess = SetActiveTheme(LoadedTheme);

			// Execute user callback
			if (OnComplete.IsBound())
			{
				OnComplete.Execute(bSuccess, LoadedTheme);
			}
		},
		FStreamableManager::AsyncLoadHighPriority,
		false // Don't manage active handle (callback will execute immediately if already loaded)
	);

	if (!Handle.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to start async load for theme: %s"), *ThemePath.ToString());
		if (OnComplete.IsBound())
		{
			OnComplete.Execute(false, nullptr);
		}
	}
}

void UProjectUIThemeManager::GetAvailableThemes(TArray<UProjectUIThemeData*>& OutThemes)
{
	OutThemes.Empty();

	UAssetManager& AssetManager = UAssetManager::Get();

	// Query Asset Manager for all theme assets
	TArray<FAssetData> ThemeAssets;
	AssetManager.GetPrimaryAssetDataList(FPrimaryAssetType("ProjectUITheme"), ThemeAssets);

	UE_LOG(LogTemp, Verbose, TEXT("Found %d available themes"), ThemeAssets.Num());

	// Load all theme assets
	for (const FAssetData& AssetData : ThemeAssets)
	{
		UProjectUIThemeData* Theme = Cast<UProjectUIThemeData>(AssetData.GetAsset());
		if (Theme)
		{
			OutThemes.Add(Theme);
			UE_LOG(LogTemp, VeryVerbose, TEXT("Available theme: %s"), *Theme->ThemeName.ToString());
		}
	}
}

FProjectUIColorPalette UProjectUIThemeManager::GetColors() const
{
	UProjectUIThemeData* Theme = GetActiveTheme();
	return Theme ? Theme->Colors : FProjectUIColorPalette();
}

FProjectUITypography UProjectUIThemeManager::GetTypography() const
{
	UProjectUIThemeData* Theme = GetActiveTheme();
	return Theme ? Theme->Typography : FProjectUITypography();
}

FProjectUISpacing UProjectUIThemeManager::GetSpacing() const
{
	UProjectUIThemeData* Theme = GetActiveTheme();
	return Theme ? Theme->Spacing : FProjectUISpacing();
}

FProjectUIAnimationSettings UProjectUIThemeManager::GetAnimationSettings() const
{
	UProjectUIThemeData* Theme = GetActiveTheme();
	return Theme ? Theme->Animation : FProjectUIAnimationSettings();
}

void UProjectUIThemeManager::LoadDefaultTheme()
{
	// Read default theme path from project settings
	const UProjectUISettings* Settings = GetDefault<UProjectUISettings>();
	if (!Settings)
	{
		UE_LOG(LogTemp, Warning, TEXT("ProjectUISettings not available, using embedded default theme"));
		if (DefaultTheme)
		{
			ActiveTheme = DefaultTheme;
		}
		return;
	}

	// Try loading configured theme if path is set
	if (Settings->DefaultThemePath.IsValid())
	{
		UE_LOG(LogTemp, Display, TEXT("Loading default theme from settings: %s"), *Settings->DefaultThemePath.ToString());

		if (LoadAndSetTheme(Settings->DefaultThemePath))
		{
			UE_LOG(LogTemp, Display, TEXT("Successfully loaded theme from settings"));
			return;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to load theme from settings path, falling back to embedded theme"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Verbose, TEXT("No default theme path configured, using embedded default theme"));
	}

	// Fallback to embedded default theme
	if (DefaultTheme)
	{
		ActiveTheme = DefaultTheme;
		UE_LOG(LogTemp, Verbose, TEXT("Using embedded default theme"));
	}
}

void UProjectUIThemeManager::CreateDefaultTheme()
{
	// Create an embedded default theme with reasonable fallback values
	// This ensures the system always has a valid theme available
	DefaultTheme = NewObject<UProjectUIThemeData>(this, TEXT("DefaultEmbeddedTheme"));

	DefaultTheme->ThemeName = FText::FromString(TEXT("Default Dark Theme"));
	DefaultTheme->ThemeDescription = FText::FromString(TEXT("Built-in fallback theme with dark color scheme"));

	// Default color palette (dark theme)
	DefaultTheme->Colors.Primary = FLinearColor(0.0f, 0.47f, 0.84f, 1.0f);
	DefaultTheme->Colors.Secondary = FLinearColor(0.29f, 0.56f, 0.89f, 1.0f);
	DefaultTheme->Colors.Success = FLinearColor(0.3f, 0.69f, 0.31f, 1.0f);
	DefaultTheme->Colors.Warning = FLinearColor(1.0f, 0.76f, 0.03f, 1.0f);
	DefaultTheme->Colors.Error = FLinearColor(0.96f, 0.26f, 0.21f, 1.0f);
	DefaultTheme->Colors.Background = FLinearColor(0.02f, 0.02f, 0.02f, 0.95f);
	DefaultTheme->Colors.Surface = FLinearColor(0.12f, 0.12f, 0.12f, 0.9f);
	DefaultTheme->Colors.TextPrimary = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
	DefaultTheme->Colors.TextSecondary = FLinearColor(0.7f, 0.7f, 0.7f, 1.0f);
	DefaultTheme->Colors.TextDisabled = FLinearColor(0.4f, 0.4f, 0.4f, 1.0f);
	DefaultTheme->Colors.Border = FLinearColor(0.3f, 0.3f, 0.3f, 1.0f);

	// Default typography (sizes set in FProjectUITypography constructor)
	// Font families will use engine defaults

	// Default spacing (values set in FProjectUISpacing constructor)

	// Default animation settings (values set in FProjectUIAnimationSettings constructor)

	UE_LOG(LogTemp, Verbose, TEXT("Created embedded default theme"));
}
