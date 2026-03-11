// Copyright ALIS. All Rights Reserved.

#include "OrchestratorManifest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FPluginDependency FPluginDependency::FromJson(const TSharedPtr<FJsonObject>& JsonObject)
{
	FPluginDependency Dependency;
	if (JsonObject.IsValid())
	{
		JsonObject->TryGetStringField(TEXT("name"), Dependency.Name);
		JsonObject->TryGetStringField(TEXT("version"), Dependency.VersionConstraint);
	}
	return Dependency;
}

TSharedPtr<FJsonObject> FPluginDependency::ToJson() const
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
	JsonObject->SetStringField(TEXT("name"), Name);
	JsonObject->SetStringField(TEXT("version"), VersionConstraint);
	return JsonObject;
}

FOrchestratorPluginEntry FOrchestratorPluginEntry::FromJson(const TSharedPtr<FJsonObject>& JsonObject)
{
	FOrchestratorPluginEntry Entry;
	if (JsonObject.IsValid())
	{
		JsonObject->TryGetStringField(TEXT("name"), Entry.Name);
		JsonObject->TryGetStringField(TEXT("version"), Entry.Version);
		JsonObject->TryGetStringField(TEXT("activation_strategy"), Entry.ActivationStrategy);
		JsonObject->TryGetStringField(TEXT("module"), Entry.Module);
		JsonObject->TryGetStringField(TEXT("code_hash"), Entry.CodeHash);
		JsonObject->TryGetStringField(TEXT("content_hash"), Entry.ContentHash);
		JsonObject->TryGetStringField(TEXT("url_code"), Entry.UrlCode);
		JsonObject->TryGetStringField(TEXT("url_content"), Entry.UrlContent);
		JsonObject->TryGetStringField(TEXT("channel"), Entry.Channel);
		JsonObject->TryGetStringField(TEXT("signature_thumbprint"), Entry.SignatureThumbprint);
		JsonObject->TryGetStringField(TEXT("release_notes"), Entry.ReleaseNotes);

		int32 SizeCodeInt = 0;
		if (JsonObject->TryGetNumberField(TEXT("size_code"), SizeCodeInt))
		{
			Entry.SizeCode = SizeCodeInt;
		}

		int32 SizeContentInt = 0;
		if (JsonObject->TryGetNumberField(TEXT("size_content"), SizeContentInt))
		{
			Entry.SizeContent = SizeContentInt;
		}

		// Parse mirrors
		const TArray<TSharedPtr<FJsonValue>>* MirrorsArray;
		if (JsonObject->TryGetArrayField(TEXT("mirrors"), MirrorsArray))
		{
			for (const TSharedPtr<FJsonValue>& MirrorValue : *MirrorsArray)
			{
				Entry.Mirrors.Add(MirrorValue->AsString());
			}
		}

		// Parse requires_engine_plugins
		const TArray<TSharedPtr<FJsonValue>>* EnginePluginsArray;
		if (JsonObject->TryGetArrayField(TEXT("requires_engine_plugins"), EnginePluginsArray))
		{
			for (const TSharedPtr<FJsonValue>& PluginValue : *EnginePluginsArray)
			{
				if (PluginValue->Type == EJson::String)
				{
					Entry.RequiresEnginePlugins.Add(PluginValue->AsString());
				}
			}
		}

		// Parse dependencies
		const TArray<TSharedPtr<FJsonValue>>* DependsOnArray;
		if (JsonObject->TryGetArrayField(TEXT("depends_on"), DependsOnArray))
		{
			for (const TSharedPtr<FJsonValue>& DepValue : *DependsOnArray)
			{
				if (DepValue->Type == EJson::Object)
				{
					Entry.DependsOn.Add(FPluginDependency::FromJson(DepValue->AsObject()));
				}
			}
		}
	}
	return Entry;
}

TSharedPtr<FJsonObject> FOrchestratorPluginEntry::ToJson() const
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
	JsonObject->SetStringField(TEXT("name"), Name);
	JsonObject->SetStringField(TEXT("version"), Version);
	if (!ActivationStrategy.IsEmpty())
	{
		JsonObject->SetStringField(TEXT("activation_strategy"), ActivationStrategy);
	}
	if (!Module.IsEmpty())
	{
		JsonObject->SetStringField(TEXT("module"), Module);
	}
	JsonObject->SetStringField(TEXT("code_hash"), CodeHash);
	JsonObject->SetStringField(TEXT("content_hash"), ContentHash);
	if (!UrlCode.IsEmpty())
	{
		JsonObject->SetStringField(TEXT("url_code"), UrlCode);
	}
	if (!UrlContent.IsEmpty())
	{
		JsonObject->SetStringField(TEXT("url_content"), UrlContent);
	}
	if (!Channel.IsEmpty())
	{
		JsonObject->SetStringField(TEXT("channel"), Channel);
	}
	if (!SignatureThumbprint.IsEmpty())
	{
		JsonObject->SetStringField(TEXT("signature_thumbprint"), SignatureThumbprint);
	}
	if (SizeCode > 0)
	{
		JsonObject->SetNumberField(TEXT("size_code"), SizeCode);
	}
	if (SizeContent > 0)
	{
		JsonObject->SetNumberField(TEXT("size_content"), SizeContent);
	}
	if (!ReleaseNotes.IsEmpty())
	{
		JsonObject->SetStringField(TEXT("release_notes"), ReleaseNotes);
	}

	// Mirrors array
	if (Mirrors.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> MirrorsArray;
		for (const FString& Mirror : Mirrors)
		{
			MirrorsArray.Add(MakeShareable(new FJsonValueString(Mirror)));
		}
		JsonObject->SetArrayField(TEXT("mirrors"), MirrorsArray);
	}

	// Engine plugins array
	if (RequiresEnginePlugins.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> EnginePluginsArray;
		for (const FString& EnginePlugin : RequiresEnginePlugins)
		{
			EnginePluginsArray.Add(MakeShareable(new FJsonValueString(EnginePlugin)));
		}
		JsonObject->SetArrayField(TEXT("requires_engine_plugins"), EnginePluginsArray);
	}

	// Dependencies array
	if (DependsOn.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> DependsOnArray;
		for (const FPluginDependency& Dep : DependsOn)
		{
			DependsOnArray.Add(MakeShareable(new FJsonValueObject(Dep.ToJson())));
		}
		JsonObject->SetArrayField(TEXT("depends_on"), DependsOnArray);
	}

	return JsonObject;
}

bool FOrchestratorManifest::LoadFromString(const FString& JsonString, FOrchestratorManifest& OutManifest)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(JsonReader, JsonObject) || !JsonObject.IsValid())
	{
		return false;
	}

	// Parse header
	JsonObject->TryGetStringField(TEXT("manifest_version"), OutManifest.ManifestVersion);
	JsonObject->TryGetStringField(TEXT("engine_build_id"), OutManifest.EngineBuildId);
	JsonObject->TryGetStringField(TEXT("signed_at"), OutManifest.SignedAt);
	JsonObject->TryGetStringField(TEXT("signing_key_id"), OutManifest.SigningKeyId);
	JsonObject->TryGetStringField(TEXT("signature"), OutManifest.Signature);

	// Parse plugins array
	const TArray<TSharedPtr<FJsonValue>>* PluginsArray;
	if (JsonObject->TryGetArrayField(TEXT("plugins"), PluginsArray))
	{
		for (const TSharedPtr<FJsonValue>& PluginValue : *PluginsArray)
		{
			if (PluginValue->Type == EJson::Object)
			{
				OutManifest.Plugins.Add(FOrchestratorPluginEntry::FromJson(PluginValue->AsObject()));
			}
		}
	}

	return true;
}

const FOrchestratorPluginEntry* FOrchestratorManifest::FindPlugin(const FString& PluginName) const
{
	for (const FOrchestratorPluginEntry& Plugin : Plugins)
	{
		if (Plugin.Name == PluginName)
		{
			return &Plugin;
		}
	}
	return nullptr;
}
