#pragma once

#include "CoreMinimal.h"
#include "OrchestratorVersion.h"

class ORCHESTRATORCORE_API FOrchestratorRegistry
{
public:
    static bool LoadRegistry(TArray<FOrchestratorRegistryEntry>& OutEntries);
    static bool SaveRegistry(const TArray<FOrchestratorRegistryEntry>& Entries);
    static const FOrchestratorRegistryEntry* FindEntry(const TArray<FOrchestratorRegistryEntry>& Entries, const FString& PluginName);
    static void UpdateEntry(TArray<FOrchestratorRegistryEntry>& Entries, const FOrchestratorRegistryEntry& NewEntry);
    static bool IsUpdateAvailable(const FOrchestratorRegistryEntry& Entry);
    static FString GetRegistryFilePath();
    static TArray<FOrchestratorRegistryEntry> CreateDefaultRegistry();
    static FOrchestratorMetadata ParseMetadataFromJson(const TSharedPtr<FJsonObject>& JsonObject);
    static TSharedPtr<FJsonObject> SerializeMetadataToJson(const FOrchestratorMetadata& Metadata);
    static FOrchestratorRegistryEntry ParseEntryFromJson(const TSharedPtr<FJsonObject>& JsonObject);
    static TSharedPtr<FJsonObject> SerializeEntryToJson(const FOrchestratorRegistryEntry& Entry);
};

