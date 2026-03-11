#include "OrchestratorRegistry.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

FString FOrchestratorRegistry::GetRegistryFilePath()
{
    return FPaths::ProjectSavedDir() / TEXT("Config") / TEXT("OrchestratorRegistry.json");
}

TArray<FOrchestratorRegistryEntry> FOrchestratorRegistry::CreateDefaultRegistry()
{
    TArray<FOrchestratorRegistryEntry> DefaultEntries;

    FOrchestratorRegistryEntry ShippedEntry;
    ShippedEntry.PluginName = TEXT("ProjectCoreFeature");
    ShippedEntry.InstalledVersion = FOrchestratorVersion(0, 1, 0);
    ShippedEntry.LatestVersion = FOrchestratorVersion(0, 1, 0);
    ShippedEntry.LastUpdateCheckTimestamp = 0;
    ShippedEntry.bUpdateAvailable = false;
    DefaultEntries.Add(ShippedEntry);

    FOrchestratorRegistryEntry FallbackEntry;
    FallbackEntry.PluginName = TEXT("ProjectMenuExperience");
    FallbackEntry.InstalledVersion = FOrchestratorVersion(0, 1, 0);
    FallbackEntry.LatestVersion = FOrchestratorVersion(0, 1, 0);
    FallbackEntry.LastUpdateCheckTimestamp = 0;
    FallbackEntry.bUpdateAvailable = false;
    DefaultEntries.Add(FallbackEntry);

    return DefaultEntries;
}

bool FOrchestratorRegistry::LoadRegistry(TArray<FOrchestratorRegistryEntry>& OutEntries)
{
    const FString RegistryPath = GetRegistryFilePath();
    if (!FPaths::FileExists(RegistryPath))
    {
        OutEntries = CreateDefaultRegistry();
        SaveRegistry(OutEntries);
        return true;
    }

    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *RegistryPath))
    {
        return false;
    }

    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* EntriesArray = nullptr;
    if (!RootObject->TryGetArrayField(TEXT("Entries"), EntriesArray))
    {
        return false;
    }

    OutEntries.Empty();
    for (const TSharedPtr<FJsonValue>& EntryValue : *EntriesArray)
    {
        if (const TSharedPtr<FJsonObject>* EntryObject = nullptr; EntryValue->TryGetObject(EntryObject))
        {
            OutEntries.Add(ParseEntryFromJson(*EntryObject));
        }
    }
    return true;
}

bool FOrchestratorRegistry::SaveRegistry(const TArray<FOrchestratorRegistryEntry>& Entries)
{
    const FString RegistryPath = GetRegistryFilePath();
    const FString RegistryDir = FPaths::GetPath(RegistryPath);
    if (!FPaths::DirectoryExists(RegistryDir))
    {
        IFileManager::Get().MakeDirectory(*RegistryDir, true);
    }

    TSharedPtr<FJsonObject> RootObject = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> EntriesArray;
    for (const FOrchestratorRegistryEntry& Entry : Entries)
    {
        TSharedPtr<FJsonObject> EntryObject = SerializeEntryToJson(Entry);
        EntriesArray.Add(MakeShared<FJsonValueObject>(EntryObject));
    }
    RootObject->SetArrayField(TEXT("Entries"), EntriesArray);

    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
    if (!FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer))
    {
        return false;
    }

    return FFileHelper::SaveStringToFile(JsonString, *RegistryPath);
}

const FOrchestratorRegistryEntry* FOrchestratorRegistry::FindEntry(const TArray<FOrchestratorRegistryEntry>& Entries, const FString& PluginName)
{
    return Entries.FindByPredicate([&PluginName](const FOrchestratorRegistryEntry& Entry)
    {
        return Entry.PluginName.Equals(PluginName, ESearchCase::IgnoreCase);
    });
}

void FOrchestratorRegistry::UpdateEntry(TArray<FOrchestratorRegistryEntry>& Entries, const FOrchestratorRegistryEntry& NewEntry)
{
    if (FOrchestratorRegistryEntry* ExistingEntry = Entries.FindByPredicate([&NewEntry](const FOrchestratorRegistryEntry& Entry)
    {
        return Entry.PluginName.Equals(NewEntry.PluginName, ESearchCase::IgnoreCase);
    }))
    {
        *ExistingEntry = NewEntry;
    }
    else
    {
        Entries.Add(NewEntry);
    }
}

bool FOrchestratorRegistry::IsUpdateAvailable(const FOrchestratorRegistryEntry& Entry)
{
    return Entry.LatestVersion > Entry.InstalledVersion;
}

FOrchestratorMetadata FOrchestratorRegistry::ParseMetadataFromJson(const TSharedPtr<FJsonObject>& JsonObject)
{
    FOrchestratorMetadata Metadata;
    if (!JsonObject.IsValid())
    {
        return Metadata;
    }
    Metadata.PluginName = JsonObject->GetStringField(TEXT("PluginName"));
    Metadata.PluginURL = JsonObject->GetStringField(TEXT("PluginURL"));
    Metadata.MinEngineVersion = JsonObject->GetStringField(TEXT("MinEngineVersion"));
    Metadata.Changelog = JsonObject->GetStringField(TEXT("Changelog"));
    if (const TSharedPtr<FJsonObject>* VersionObject = nullptr; JsonObject->TryGetObjectField(TEXT("Version"), VersionObject))
    {
        Metadata.Version.Major = (*VersionObject)->GetIntegerField(TEXT("Major"));
        Metadata.Version.Minor = (*VersionObject)->GetIntegerField(TEXT("Minor"));
        Metadata.Version.Patch = (*VersionObject)->GetIntegerField(TEXT("Patch"));
    }
    const TArray<TSharedPtr<FJsonValue>>* DepsArray = nullptr;
    if (JsonObject->TryGetArrayField(TEXT("Dependencies"), DepsArray))
    {
        for (const TSharedPtr<FJsonValue>& DepValue : *DepsArray)
        {
            Metadata.Dependencies.Add(DepValue->AsString());
        }
    }
    return Metadata;
}

TSharedPtr<FJsonObject> FOrchestratorRegistry::SerializeMetadataToJson(const FOrchestratorMetadata& Metadata)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("PluginName"), Metadata.PluginName);
    Obj->SetStringField(TEXT("PluginURL"), Metadata.PluginURL);
    Obj->SetStringField(TEXT("MinEngineVersion"), Metadata.MinEngineVersion);
    Obj->SetStringField(TEXT("Changelog"), Metadata.Changelog);
    TSharedPtr<FJsonObject> Ver = MakeShared<FJsonObject>();
    Ver->SetNumberField(TEXT("Major"), Metadata.Version.Major);
    Ver->SetNumberField(TEXT("Minor"), Metadata.Version.Minor);
    Ver->SetNumberField(TEXT("Patch"), Metadata.Version.Patch);
    Obj->SetObjectField(TEXT("Version"), Ver);
    TArray<TSharedPtr<FJsonValue>> Deps;
    for (const FString& D : Metadata.Dependencies)
    {
        Deps.Add(MakeShared<FJsonValueString>(D));
    }
    Obj->SetArrayField(TEXT("Dependencies"), Deps);
    return Obj;
}

FOrchestratorRegistryEntry FOrchestratorRegistry::ParseEntryFromJson(const TSharedPtr<FJsonObject>& JsonObject)
{
    FOrchestratorRegistryEntry Entry;
    Entry.PluginName = JsonObject->GetStringField(TEXT("PluginName"));
    const TSharedPtr<FJsonObject>* InstalledObj = nullptr;
    if (JsonObject->TryGetObjectField(TEXT("InstalledVersion"), InstalledObj))
    {
        Entry.InstalledVersion.Major = (*InstalledObj)->GetIntegerField(TEXT("Major"));
        Entry.InstalledVersion.Minor = (*InstalledObj)->GetIntegerField(TEXT("Minor"));
        Entry.InstalledVersion.Patch = (*InstalledObj)->GetIntegerField(TEXT("Patch"));
    }
    const TSharedPtr<FJsonObject>* LatestObj = nullptr;
    if (JsonObject->TryGetObjectField(TEXT("LatestVersion"), LatestObj))
    {
        Entry.LatestVersion.Major = (*LatestObj)->GetIntegerField(TEXT("Major"));
        Entry.LatestVersion.Minor = (*LatestObj)->GetIntegerField(TEXT("Minor"));
        Entry.LatestVersion.Patch = (*LatestObj)->GetIntegerField(TEXT("Patch"));
    }
    Entry.LastUpdateCheckTimestamp = (int64)JsonObject->GetNumberField(TEXT("LastUpdateCheckTimestamp"));
    Entry.bUpdateAvailable = JsonObject->GetBoolField(TEXT("bUpdateAvailable"));
    return Entry;
}

TSharedPtr<FJsonObject> FOrchestratorRegistry::SerializeEntryToJson(const FOrchestratorRegistryEntry& Entry)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("PluginName"), Entry.PluginName);
    TSharedPtr<FJsonObject> Installed = MakeShared<FJsonObject>();
    Installed->SetNumberField(TEXT("Major"), Entry.InstalledVersion.Major);
    Installed->SetNumberField(TEXT("Minor"), Entry.InstalledVersion.Minor);
    Installed->SetNumberField(TEXT("Patch"), Entry.InstalledVersion.Patch);
    Obj->SetObjectField(TEXT("InstalledVersion"), Installed);
    TSharedPtr<FJsonObject> Latest = MakeShared<FJsonObject>();
    Latest->SetNumberField(TEXT("Major"), Entry.LatestVersion.Major);
    Latest->SetNumberField(TEXT("Minor"), Entry.LatestVersion.Minor);
    Latest->SetNumberField(TEXT("Patch"), Entry.LatestVersion.Patch);
    Obj->SetObjectField(TEXT("LatestVersion"), Latest);
    Obj->SetNumberField(TEXT("LastUpdateCheckTimestamp"), (double)Entry.LastUpdateCheckTimestamp);
    Obj->SetBoolField(TEXT("bUpdateAvailable"), Entry.bUpdateAvailable);
    return Obj;
}

