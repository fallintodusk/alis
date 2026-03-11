#include "SinglePlayModeRegistry.h"
#include "ProjectSinglePlayLog.h"
#include "ProjectPaths.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

bool FSinglePlayModeRegistry::bDefaultsInitialized = false;
bool FSinglePlayModeRegistry::bJsonOverridesLoaded = false;

TMap<FName, FSinglePlayModeConfig>& FSinglePlayModeRegistry::GetRegistry()
{
	static TMap<FName, FSinglePlayModeConfig> Registry;
	return Registry;
}

void FSinglePlayModeRegistry::RegisterMode(const FSinglePlayModeConfig& Config)
{
	if (Config.ModeName.IsNone())
	{
		UE_LOG(LogProjectSinglePlay, Warning, TEXT("ModeRegistry: Cannot register mode with empty name"));
		return;
	}

	TMap<FName, FSinglePlayModeConfig>& Registry = GetRegistry();

	if (Registry.Contains(Config.ModeName))
	{
		UE_LOG(LogProjectSinglePlay, Log, TEXT("ModeRegistry: Overwriting existing mode '%s'"),
			*Config.ModeName.ToString());
	}

	Registry.Add(Config.ModeName, Config);
	UE_LOG(LogProjectSinglePlay, Log, TEXT("ModeRegistry: Registered mode '%s' (Features=%d, PawnClass=%s)"),
		*Config.ModeName.ToString(),
		Config.FeatureNames.Num(),
		Config.DefaultPawnClass.IsNull() ? TEXT("default") : TEXT("custom"));
}

const FSinglePlayModeConfig* FSinglePlayModeRegistry::FindMode(FName ModeName)
{
	// Ensure defaults are initialized on first lookup
	if (!bDefaultsInitialized)
	{
		InitializeDefaults();
	}

	const FSinglePlayModeConfig* Found = GetRegistry().Find(ModeName);
	if (Found)
	{
		UE_LOG(LogProjectSinglePlay, Verbose, TEXT("FindMode('%s'): Found with %d features"),
			*ModeName.ToString(), Found->FeatureNames.Num());
	}
	else
	{
		UE_LOG(LogProjectSinglePlay, Verbose, TEXT("FindMode('%s'): Not found. Registry has %d modes"),
			*ModeName.ToString(), GetRegistry().Num());
	}
	return Found;
}

TArray<FName> FSinglePlayModeRegistry::GetAllModeNames()
{
	if (!bDefaultsInitialized)
	{
		InitializeDefaults();
	}

	TArray<FName> Names;
	GetRegistry().GetKeys(Names);
	return Names;
}

bool FSinglePlayModeRegistry::HasMode(FName ModeName)
{
	if (!bDefaultsInitialized)
	{
		InitializeDefaults();
	}

	return GetRegistry().Contains(ModeName);
}

void FSinglePlayModeRegistry::ClearAll()
{
	GetRegistry().Empty();
	bDefaultsInitialized = false;
	bJsonOverridesLoaded = false;
}

void FSinglePlayModeRegistry::InitializeDefaults()
{
	if (bDefaultsInitialized)
	{
		return;
	}

	bDefaultsInitialized = true;

	// Note: Mode definitions are registered via static initializers
	// in SinglePlayModeDefaults.cpp using DEFINE_SINGLEPLAY_MODE macro
	// or by calling RegisterMode() directly.

	// Load JSON overrides (merges with C++ definitions)
	LoadJsonOverrides();

	// Ensure at least "Single" mode exists as ultimate fallback
	if (!GetRegistry().Contains(FName(TEXT("Single"))))
	{
		UE_LOG(LogProjectSinglePlay, Log, TEXT("ModeRegistry: No 'Single' mode registered, adding fallback"));
		FSinglePlayModeConfig DefaultConfig = FSinglePlayModeConfig::GetDefault();
		GetRegistry().Add(DefaultConfig.ModeName, DefaultConfig);
	}

	UE_LOG(LogProjectSinglePlay, Log, TEXT("ModeRegistry: Initialized with %d modes"),
		GetRegistry().Num());
}

void FSinglePlayModeRegistry::LoadJsonOverrides()
{
	if (bJsonOverridesLoaded)
	{
		return;
	}
	bJsonOverridesLoaded = true;

	// Default location: Plugins/Gameplay/ProjectSinglePlay/Data/ModeOverrides.json
	const FString DefaultPath = FProjectPaths::GetPluginDataDir(TEXT("ProjectSinglePlay")) / TEXT("ModeOverrides.json");

	if (!FPaths::FileExists(DefaultPath))
	{
		UE_LOG(LogProjectSinglePlay, Verbose, TEXT("ModeRegistry: No JSON overrides file at %s (optional)"), *DefaultPath);
		return;
	}

	const int32 LoadedCount = LoadJsonOverridesFromFile(DefaultPath);
	UE_LOG(LogProjectSinglePlay, Log, TEXT("ModeRegistry: Loaded %d mode overrides from JSON"), LoadedCount);
}

int32 FSinglePlayModeRegistry::LoadJsonOverridesFromFile(const FString& JsonFilePath)
{
	FString JsonContent;
	if (!FFileHelper::LoadFileToString(JsonContent, *JsonFilePath))
	{
		UE_LOG(LogProjectSinglePlay, Warning, TEXT("ModeRegistry: Failed to read JSON file: %s"), *JsonFilePath);
		return 0;
	}

	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);

	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogProjectSinglePlay, Warning, TEXT("ModeRegistry: Failed to parse JSON file: %s"), *JsonFilePath);
		return 0;
	}

	// Expected format:
	// {
	//   "modes": [
	//     {
	//       "modeName": "Single",
	//       "featureConfigs": { "Difficulty": "{\"level\":\"easy\"}" }
	//     }
	//   ]
	// }

	const TArray<TSharedPtr<FJsonValue>>* ModesArray = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("modes"), ModesArray) || !ModesArray)
	{
		UE_LOG(LogProjectSinglePlay, Warning, TEXT("ModeRegistry: JSON missing 'modes' array: %s"), *JsonFilePath);
		return 0;
	}

	int32 SuccessCount = 0;
	int32 SkippedCount = 0;

	for (int32 i = 0; i < ModesArray->Num(); ++i)
	{
		const TSharedPtr<FJsonValue>& ModeValue = (*ModesArray)[i];
		if (!ModeValue.IsValid() || ModeValue->Type != EJson::Object)
		{
			UE_LOG(LogProjectSinglePlay, Warning, TEXT("ModeRegistry: Invalid mode entry at index %d (not an object)"), i);
			SkippedCount++;
			continue;
		}

		FSinglePlayModeConfig ParsedConfig;
		if (!ParseModeFromJson(ModeValue->AsObject(), ParsedConfig))
		{
			UE_LOG(LogProjectSinglePlay, Warning, TEXT("ModeRegistry: Failed to parse mode at index %d"), i);
			SkippedCount++;
			continue;
		}

		// Merge with existing config if present, otherwise add new
		TMap<FName, FSinglePlayModeConfig>& Registry = GetRegistry();
		if (FSinglePlayModeConfig* ExistingConfig = Registry.Find(ParsedConfig.ModeName))
		{
			// Merge: JSON overrides C++ values (only non-empty values)
			if (!ParsedConfig.DefaultPawnClass.IsNull())
			{
				ExistingConfig->DefaultPawnClass = ParsedConfig.DefaultPawnClass;
			}
			if (!ParsedConfig.PlayerControllerClass.IsNull())
			{
				ExistingConfig->PlayerControllerClass = ParsedConfig.PlayerControllerClass;
			}
			if (ParsedConfig.FeatureNames.Num() > 0)
			{
				ExistingConfig->FeatureNames = ParsedConfig.FeatureNames;
			}
			// Merge FeatureConfigs (JSON values override C++ values per-key)
			for (const auto& Pair : ParsedConfig.FeatureConfigs)
			{
				ExistingConfig->FeatureConfigs.Add(Pair.Key, Pair.Value);
			}

			UE_LOG(LogProjectSinglePlay, Log, TEXT("ModeRegistry: Merged JSON overrides for mode '%s'"),
				*ParsedConfig.ModeName.ToString());
		}
		else
		{
			// Add new mode from JSON
			Registry.Add(ParsedConfig.ModeName, ParsedConfig);
			UE_LOG(LogProjectSinglePlay, Log, TEXT("ModeRegistry: Added new mode '%s' from JSON"),
				*ParsedConfig.ModeName.ToString());
		}

		SuccessCount++;
	}

	if (SkippedCount > 0)
	{
		UE_LOG(LogProjectSinglePlay, Warning, TEXT("ModeRegistry: Skipped %d invalid entries in JSON"), SkippedCount);
	}

	return SuccessCount;
}

bool FSinglePlayModeRegistry::ParseModeFromJson(const TSharedPtr<FJsonObject>& ModeObject, FSinglePlayModeConfig& OutConfig)
{
	if (!ModeObject.IsValid())
	{
		return false;
	}

	// Required: modeName
	FString ModeNameStr;
	if (!ModeObject->TryGetStringField(TEXT("modeName"), ModeNameStr) || ModeNameStr.IsEmpty())
	{
		UE_LOG(LogProjectSinglePlay, Warning, TEXT("ModeRegistry: JSON mode missing 'modeName' field"));
		return false;
	}
	OutConfig.ModeName = FName(*ModeNameStr);

	// Optional: defaultPawnClass (soft class path)
	FString PawnClassPath;
	if (ModeObject->TryGetStringField(TEXT("defaultPawnClass"), PawnClassPath) && !PawnClassPath.IsEmpty())
	{
		OutConfig.DefaultPawnClass = TSoftClassPtr<APawn>(FSoftObjectPath(PawnClassPath));
	}

	// Optional: playerControllerClass (soft class path)
	FString PCClassPath;
	if (ModeObject->TryGetStringField(TEXT("playerControllerClass"), PCClassPath) && !PCClassPath.IsEmpty())
	{
		OutConfig.PlayerControllerClass = TSoftClassPtr<APlayerController>(FSoftObjectPath(PCClassPath));
	}

	// Optional: featureNames (array of feature name strings)
	const TArray<TSharedPtr<FJsonValue>>* NamesArray = nullptr;
	if (ModeObject->TryGetArrayField(TEXT("featureNames"), NamesArray) && NamesArray)
	{
		for (const TSharedPtr<FJsonValue>& NameValue : *NamesArray)
		{
			if (NameValue.IsValid() && NameValue->Type == EJson::String)
			{
				OutConfig.FeatureNames.Add(FName(*NameValue->AsString()));
			}
		}
	}

	// Optional: featureConfigs (object with key-value pairs)
	const TSharedPtr<FJsonObject>* FeatureConfigsObject = nullptr;
	if (ModeObject->TryGetObjectField(TEXT("featureConfigs"), FeatureConfigsObject) && FeatureConfigsObject)
	{
		for (const auto& Pair : (*FeatureConfigsObject)->Values)
		{
			if (Pair.Value.IsValid() && Pair.Value->Type == EJson::String)
			{
				OutConfig.FeatureConfigs.Add(FName(*Pair.Key), Pair.Value->AsString());
			}
		}
	}

	return true;
}
