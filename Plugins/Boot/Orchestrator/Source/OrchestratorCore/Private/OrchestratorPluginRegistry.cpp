// Copyright ALIS. All Rights Reserved.

#include "OrchestratorPluginRegistry.h"
#include "Interfaces/IPluginManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogOrchestratorPluginRegistry, Log, All);

FPluginRegistrationResult FOrchestratorPluginRegistry::RegisterPlugin(const FString& UpluginFilePath)
{
	UE_LOG(LogOrchestratorPluginRegistry, Log, TEXT("Registering external plugin: %s"), *UpluginFilePath);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Verify .uplugin file exists
	if (!PlatformFile.FileExists(*UpluginFilePath))
	{
		const FString Error = FString::Printf(TEXT("Plugin descriptor not found: %s"), *UpluginFilePath);
		UE_LOG(LogOrchestratorPluginRegistry, Error, TEXT("%s"), *Error);
		return FPluginRegistrationResult::MakeFailure(Error);
	}

	// Parse plugin name from descriptor
	FString PluginName;
	if (!VerifyUpluginFile(UpluginFilePath, PluginName))
	{
		return FPluginRegistrationResult::MakeFailure(TEXT("Failed to parse plugin descriptor"));
	}

	UE_LOG(LogOrchestratorPluginRegistry, Log, TEXT("  Plugin name: %s"), *PluginName);

	// Check if already mounted
	if (IsPluginMounted(PluginName))
	{
		UE_LOG(LogOrchestratorPluginRegistry, Warning, TEXT("  Plugin already mounted, skipping registration"));
		const FString PluginDir = FPaths::GetPath(UpluginFilePath);
		return FPluginRegistrationResult::MakeSuccess(PluginName, PluginDir);
	}

	// Mount plugin using IPluginManager
	IPluginManager& PluginManager = IPluginManager::Get();

	const bool bMounted = PluginManager.MountExplicitlyLoadedPlugin_FromFileName(UpluginFilePath);

	if (!bMounted)
	{
		const FString Error = FString::Printf(TEXT("Failed to mount plugin: %s"), *PluginName);
		UE_LOG(LogOrchestratorPluginRegistry, Error, TEXT("%s"), *Error);
		return FPluginRegistrationResult::MakeFailure(Error);
	}

	// Verify plugin is now accessible
	TSharedPtr<IPlugin> Plugin = PluginManager.FindPlugin(PluginName);
	if (!Plugin.IsValid())
	{
		const FString Error = FString::Printf(TEXT("Plugin mounted but not found: %s"), *PluginName);
		UE_LOG(LogOrchestratorPluginRegistry, Error, TEXT("%s"), *Error);
		return FPluginRegistrationResult::MakeFailure(Error);
	}

	UE_LOG(LogOrchestratorPluginRegistry, Log, TEXT("  ✓ Plugin registered successfully"));
	UE_LOG(LogOrchestratorPluginRegistry, Log, TEXT("  ✓ Plugin directory: %s"), *Plugin->GetBaseDir());

	return FPluginRegistrationResult::MakeSuccess(PluginName, Plugin->GetBaseDir());
}

bool FOrchestratorPluginRegistry::AddPluginSearchPath(const FString& SearchPath, bool bRefresh)
{
	UE_LOG(LogOrchestratorPluginRegistry, Log, TEXT("Adding plugin search path: %s"), *SearchPath);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Verify directory exists
	if (!PlatformFile.DirectoryExists(*SearchPath))
	{
		UE_LOG(LogOrchestratorPluginRegistry, Error, TEXT("Search path does not exist: %s"), *SearchPath);
		return false;
	}

	// Add to IPluginManager
	IPluginManager& PluginManager = IPluginManager::Get();
	const bool bSuccess = PluginManager.AddPluginSearchPath(SearchPath, bRefresh);

	if (bSuccess)
	{
		UE_LOG(LogOrchestratorPluginRegistry, Log, TEXT("  ✓ Search path added"));
	}
	else
	{
		UE_LOG(LogOrchestratorPluginRegistry, Warning, TEXT("  Failed to add search path"));
	}

	return bSuccess;
}

bool FOrchestratorPluginRegistry::IsPluginMounted(const FString& PluginName)
{
	IPluginManager& PluginManager = IPluginManager::Get();
	TSharedPtr<IPlugin> Plugin = PluginManager.FindPlugin(PluginName);
	return Plugin.IsValid();
}

bool FOrchestratorPluginRegistry::GetPluginInfo(
	const FString& PluginName,
	FString& OutPluginPath,
	FString& OutModuleName)
{
	IPluginManager& PluginManager = IPluginManager::Get();
	TSharedPtr<IPlugin> Plugin = PluginManager.FindPlugin(PluginName);

	if (!Plugin.IsValid())
	{
		return false;
	}

	OutPluginPath = Plugin->GetBaseDir();

	// Get primary module name from descriptor
	const FPluginDescriptor& Descriptor = Plugin->GetDescriptor();
	if (Descriptor.Modules.Num() > 0)
	{
		OutModuleName = Descriptor.Modules[0].Name.ToString();
	}
	else
	{
		OutModuleName = TEXT(""); // Content-only plugin
	}

	return true;
}

bool FOrchestratorPluginRegistry::VerifyUpluginFile(
	const FString& UpluginFilePath,
	FString& OutPluginName)
{
	UE_LOG(LogOrchestratorPluginRegistry, Verbose, TEXT("Verifying .uplugin file: %s"), *UpluginFilePath);

	// Load .uplugin file
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *UpluginFilePath))
	{
		UE_LOG(LogOrchestratorPluginRegistry, Error, TEXT("Failed to read .uplugin file"));
		return false;
	}

	// Parse JSON
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogOrchestratorPluginRegistry, Error, TEXT("Failed to parse .uplugin JSON"));
		return false;
	}

	// Extract plugin name from file name (standard UE convention)
	OutPluginName = FPaths::GetBaseFilename(UpluginFilePath);

	// Verify required fields exist
	if (!JsonObject->HasField(TEXT("FileVersion")))
	{
		UE_LOG(LogOrchestratorPluginRegistry, Error, TEXT("Missing FileVersion in .uplugin"));
		return false;
	}

	// Optional: Verify plugin name matches FriendlyName if present
	if (JsonObject->HasField(TEXT("FriendlyName")))
	{
		const FString FriendlyName = JsonObject->GetStringField(TEXT("FriendlyName"));
		UE_LOG(LogOrchestratorPluginRegistry, Verbose, TEXT("  Plugin friendly name: %s"), *FriendlyName);
	}

	UE_LOG(LogOrchestratorPluginRegistry, Verbose, TEXT("  ✓ Valid .uplugin file"));
	return true;
}

int32 FOrchestratorPluginRegistry::GetAllPluginNames(TArray<FString>& OutPluginNames)
{
	OutPluginNames.Empty();

	IPluginManager& PluginManager = IPluginManager::Get();
	TArray<TSharedRef<IPlugin>> AllPlugins = PluginManager.GetDiscoveredPlugins();

	for (const TSharedRef<IPlugin>& Plugin : AllPlugins)
	{
		OutPluginNames.Add(Plugin->GetName());
	}

	return OutPluginNames.Num();
}
