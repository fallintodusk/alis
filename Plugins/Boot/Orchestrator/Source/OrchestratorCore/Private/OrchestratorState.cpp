// Copyright ALIS. All Rights Reserved.

#include "OrchestratorState.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/PlatformFileManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogOrchestratorState, Log, All);

// ===== FPluginState =====

FPluginState FPluginState::FromJson(const TSharedPtr<FJsonObject>& JsonObject)
{
	FPluginState State;
	if (!JsonObject.IsValid()) return State;

	JsonObject->TryGetStringField(TEXT("name"), State.Name);
	JsonObject->TryGetStringField(TEXT("version"), State.Version);
	JsonObject->TryGetStringField(TEXT("module"), State.Module);
	JsonObject->TryGetStringField(TEXT("code_hash"), State.CodeHash);
	JsonObject->TryGetStringField(TEXT("content_hash"), State.ContentHash);
	JsonObject->TryGetStringField(TEXT("installed_path"), State.InstalledPath);
	JsonObject->TryGetStringField(TEXT("channel"), State.Channel);

	return State;
}

TSharedPtr<FJsonObject> FPluginState::ToJson() const
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());

	JsonObject->SetStringField(TEXT("name"), Name);
	JsonObject->SetStringField(TEXT("version"), Version);
	JsonObject->SetStringField(TEXT("module"), Module);
	JsonObject->SetStringField(TEXT("code_hash"), CodeHash);
	JsonObject->SetStringField(TEXT("content_hash"), ContentHash);
	JsonObject->SetStringField(TEXT("installed_path"), InstalledPath);
	JsonObject->SetStringField(TEXT("channel"), Channel);

	return JsonObject;
}

// ===== FOrchestratorState =====

bool FOrchestratorState::LoadFromFile(const FString& FilePath, FOrchestratorState& OutState)
{
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		UE_LOG(LogOrchestratorState, Warning, TEXT("Failed to read state file: %s"), *FilePath);
		return false;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogOrchestratorState, Error, TEXT("Failed to parse state JSON: %s"), *FilePath);
		return false;
	}

	OutState = FromJson(JsonObject);
	return true;
}

bool FOrchestratorState::SaveToFile(const FString& FilePath) const
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const FString Directory = FPaths::GetPath(FilePath);

	if (!PlatformFile.DirectoryExists(*Directory))
	{
		if (!PlatformFile.CreateDirectoryTree(*Directory))
		{
			UE_LOG(LogOrchestratorState, Error, TEXT("Failed to create directory: %s"), *Directory);
			return false;
		}
	}

	TSharedPtr<FJsonObject> JsonObject = ToJson();

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	if (!FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer))
	{
		UE_LOG(LogOrchestratorState, Error, TEXT("Failed to serialize state"));
		return false;
	}

	// Atomic write: temp + rename
	const FString TempPath = FilePath + TEXT(".tmp");

	if (!FFileHelper::SaveStringToFile(JsonString, *TempPath))
	{
		UE_LOG(LogOrchestratorState, Error, TEXT("Failed to write temp file: %s"), *TempPath);
		return false;
	}

	if (PlatformFile.FileExists(*FilePath))
	{
		PlatformFile.DeleteFile(*FilePath);
	}

	if (!PlatformFile.MoveFile(*FilePath, *TempPath))
	{
		UE_LOG(LogOrchestratorState, Error, TEXT("Failed to rename temp file"));
		return false;
	}

	UE_LOG(LogOrchestratorState, Log, TEXT("State saved: %s"), *FilePath);
	return true;
}

FOrchestratorState FOrchestratorState::FromJson(const TSharedPtr<FJsonObject>& JsonObject)
{
	FOrchestratorState State;
	if (!JsonObject.IsValid()) return State;

	JsonObject->TryGetStringField(TEXT("manifest_version"), State.ManifestVersion);
	JsonObject->TryGetStringField(TEXT("engine_build_id"), State.EngineBuildId);
	JsonObject->TryGetStringField(TEXT("applied_at"), State.AppliedAt);

	const TSharedPtr<FJsonObject>* PluginsObject = nullptr;
	if (JsonObject->TryGetObjectField(TEXT("plugins"), PluginsObject))
	{
		for (const auto& Pair : (*PluginsObject)->Values)
		{
			if (Pair.Value->Type == EJson::Object)
			{
				FPluginState PluginState = FPluginState::FromJson(Pair.Value->AsObject());
				State.Plugins.Add(Pair.Key, PluginState);
			}
		}
	}

	return State;
}

TSharedPtr<FJsonObject> FOrchestratorState::ToJson() const
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());

	JsonObject->SetStringField(TEXT("manifest_version"), ManifestVersion);
	JsonObject->SetStringField(TEXT("engine_build_id"), EngineBuildId);
	JsonObject->SetStringField(TEXT("applied_at"), AppliedAt);

	TSharedPtr<FJsonObject> PluginsObject = MakeShareable(new FJsonObject());
	for (const auto& Pair : Plugins)
	{
		PluginsObject->SetObjectField(Pair.Key, Pair.Value.ToJson());
	}
	JsonObject->SetObjectField(TEXT("plugins"), PluginsObject);

	return JsonObject;
}

// ===== FPendingUpdate =====

FPendingUpdate FPendingUpdate::FromJson(const TSharedPtr<FJsonObject>& JsonObject)
{
	FPendingUpdate Update;
	if (!JsonObject.IsValid()) return Update;

	JsonObject->TryGetStringField(TEXT("name"), Update.Name);
	JsonObject->TryGetStringField(TEXT("from_version"), Update.FromVersion);
	JsonObject->TryGetStringField(TEXT("to_version"), Update.ToVersion);
	JsonObject->TryGetStringField(TEXT("change"), Update.Change);
	JsonObject->TryGetStringField(TEXT("staged_path"), Update.StagedPath);
	JsonObject->TryGetBoolField(TEXT("requires_restart"), Update.bRequiresRestart);

	return Update;
}

TSharedPtr<FJsonObject> FPendingUpdate::ToJson() const
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());

	JsonObject->SetStringField(TEXT("name"), Name);
	JsonObject->SetStringField(TEXT("from_version"), FromVersion);
	JsonObject->SetStringField(TEXT("to_version"), ToVersion);
	JsonObject->SetStringField(TEXT("change"), Change);
	JsonObject->SetStringField(TEXT("staged_path"), StagedPath);
	JsonObject->SetBoolField(TEXT("requires_restart"), bRequiresRestart);

	return JsonObject;
}

// ===== FPendingUpdates =====

bool FPendingUpdates::LoadFromFile(const FString& FilePath, FPendingUpdates& OutUpdates)
{
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		UE_LOG(LogOrchestratorState, Log, TEXT("No pending updates file: %s"), *FilePath);
		return false;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogOrchestratorState, Error, TEXT("Failed to parse pending updates JSON"));
		return false;
	}

	OutUpdates = FromJson(JsonObject);
	return true;
}

bool FPendingUpdates::SaveToFile(const FString& FilePath) const
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const FString Directory = FPaths::GetPath(FilePath);

	if (!PlatformFile.DirectoryExists(*Directory))
	{
		if (!PlatformFile.CreateDirectoryTree(*Directory))
		{
			UE_LOG(LogOrchestratorState, Error, TEXT("Failed to create directory: %s"), *Directory);
			return false;
		}
	}

	TSharedPtr<FJsonObject> JsonObject = ToJson();

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	if (!FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer))
	{
		UE_LOG(LogOrchestratorState, Error, TEXT("Failed to serialize pending updates"));
		return false;
	}

	// Atomic write
	const FString TempPath = FilePath + TEXT(".tmp");

	if (!FFileHelper::SaveStringToFile(JsonString, *TempPath))
	{
		UE_LOG(LogOrchestratorState, Error, TEXT("Failed to write temp file: %s"), *TempPath);
		return false;
	}

	if (PlatformFile.FileExists(*FilePath))
	{
		PlatformFile.DeleteFile(*FilePath);
	}

	if (!PlatformFile.MoveFile(*FilePath, *TempPath))
	{
		UE_LOG(LogOrchestratorState, Error, TEXT("Failed to rename temp file"));
		return false;
	}

	UE_LOG(LogOrchestratorState, Log, TEXT("Pending updates saved: %s"), *FilePath);
	return true;
}

FPendingUpdates FPendingUpdates::FromJson(const TSharedPtr<FJsonObject>& JsonObject)
{
	FPendingUpdates Updates;
	if (!JsonObject.IsValid()) return Updates;

	JsonObject->TryGetStringField(TEXT("manifest_version"), Updates.ManifestVersion);
	JsonObject->TryGetStringField(TEXT("engine_build_id"), Updates.EngineBuildId);
	JsonObject->TryGetStringField(TEXT("created_at"), Updates.CreatedAt);

	const TArray<TSharedPtr<FJsonValue>>* UpdatesArray = nullptr;
	if (JsonObject->TryGetArrayField(TEXT("updates"), UpdatesArray))
	{
		for (const TSharedPtr<FJsonValue>& UpdateValue : *UpdatesArray)
		{
			if (UpdateValue->Type == EJson::Object)
			{
				Updates.Updates.Add(FPendingUpdate::FromJson(UpdateValue->AsObject()));
			}
		}
	}

	return Updates;
}

TSharedPtr<FJsonObject> FPendingUpdates::ToJson() const
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());

	JsonObject->SetStringField(TEXT("manifest_version"), ManifestVersion);
	JsonObject->SetStringField(TEXT("engine_build_id"), EngineBuildId);
	JsonObject->SetStringField(TEXT("created_at"), CreatedAt);

	TArray<TSharedPtr<FJsonValue>> UpdatesArray;
	for (const FPendingUpdate& Update : Updates)
	{
		UpdatesArray.Add(MakeShareable(new FJsonValueObject(Update.ToJson())));
	}
	JsonObject->SetArrayField(TEXT("updates"), UpdatesArray);

	return JsonObject;
}
