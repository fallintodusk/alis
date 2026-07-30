// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Settings/InventoryUISettings.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Layout/ProjectWidgetLayoutLoader.h"
#include "Logging/LogMacros.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogInventoryUISettings, Log, All);

namespace
{
	FInventoryUISettings LoadFromJson()
	{
		FInventoryUISettings Settings; // in-code defaults are the fallback

		const FString ConfigPath = UProjectWidgetLayoutLoader::GetPluginUIConfigPath(
			TEXT("ProjectInventoryUI"),
			TEXT("InventoryUISettings.json"));
		if (ConfigPath.IsEmpty() || !IFileManager::Get().FileExists(*ConfigPath))
		{
			UE_LOG(LogInventoryUISettings, Warning,
				TEXT("InventoryUISettings.json not found (path='%s'). Using in-code defaults."),
				*ConfigPath);
			return Settings;
		}

		FString JsonString;
		if (!FFileHelper::LoadFileToString(JsonString, *ConfigPath))
		{
			UE_LOG(LogInventoryUISettings, Warning,
				TEXT("Failed to read InventoryUISettings.json at '%s'. Using in-code defaults."),
				*ConfigPath);
			return Settings;
		}

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			UE_LOG(LogInventoryUISettings, Warning,
				TEXT("Failed to parse InventoryUISettings.json. Using in-code defaults."));
			return Settings;
		}

		auto ReadFloat = [&Root](const TCHAR* Field, float& OutValue)
		{
			double Value = 0.0;
			if (Root->TryGetNumberField(Field, Value) && Value > 0.0)
			{
				OutValue = static_cast<float>(Value);
			}
		};

		ReadFloat(TEXT("cellSize"), Settings.CellSize);
		ReadFloat(TEXT("gridSlotLineWidth"), Settings.GridSlotLineWidth);
		ReadFloat(TEXT("cellInnerPadding"), Settings.CellInnerPadding);
		ReadFloat(TEXT("hostOuterPadding"), Settings.HostOuterPadding);

		return Settings;
	}
}

const FInventoryUISettings& FInventoryUISettings::Get()
{
	static const FInventoryUISettings Instance = LoadFromJson();
	return Instance;
}
