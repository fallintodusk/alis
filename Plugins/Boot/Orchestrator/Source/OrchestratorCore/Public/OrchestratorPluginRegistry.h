// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Plugin registration result.
 */
struct ORCHESTRATORCORE_API FPluginRegistrationResult
{
	bool bSuccess = false;
	FString ErrorMessage;
	FString PluginName;
	FString PluginPath;

	FPluginRegistrationResult() = default;

	static FPluginRegistrationResult MakeSuccess(const FString& Name, const FString& Path)
	{
		FPluginRegistrationResult Result;
		Result.bSuccess = true;
		Result.PluginName = Name;
		Result.PluginPath = Path;
		return Result;
	}

	static FPluginRegistrationResult MakeFailure(const FString& Error)
	{
		FPluginRegistrationResult Result;
		Result.bSuccess = false;
		Result.ErrorMessage = Error;
		return Result;
	}
};

/**
 * Plugin registration utilities for Orchestrator.
 * Handles registering and mounting externally downloaded plugins with IPluginManager.
 */
class ORCHESTRATORCORE_API FOrchestratorPluginRegistry
{
public:
	/**
	 * Register and mount an external plugin from .uplugin file.
	 * Makes the plugin available to the engine's module system.
	 *
	 * @param UpluginFilePath Absolute path to .uplugin file
	 * @return Registration result
	 */
	static FPluginRegistrationResult RegisterPlugin(const FString& UpluginFilePath);

	/**
	 * Register a plugin search path with IPluginManager.
	 * Allows automatic discovery of plugins in the specified directory.
	 *
	 * @param SearchPath Absolute path to directory containing plugins
	 * @param bRefresh Whether to refresh plugin list immediately
	 * @return true if search path was added successfully
	 */
	static bool AddPluginSearchPath(const FString& SearchPath, bool bRefresh = true);

	/**
	 * Check if a plugin is currently mounted.
	 *
	 * @param PluginName Name of plugin to check
	 * @return true if plugin is mounted
	 */
	static bool IsPluginMounted(const FString& PluginName);

	/**
	 * Get information about a mounted plugin.
	 *
	 * @param PluginName Name of plugin
	 * @param OutPluginPath Receives absolute path to plugin directory
	 * @param OutModuleName Receives primary module name (empty for content-only)
	 * @return true if plugin exists
	 */
	static bool GetPluginInfo(
		const FString& PluginName,
		FString& OutPluginPath,
		FString& OutModuleName);

	/**
	 * Verify .uplugin file exists and is valid.
	 *
	 * @param UpluginFilePath Path to .uplugin file
	 * @param OutPluginName Receives plugin name from descriptor
	 * @return true if valid
	 */
	static bool VerifyUpluginFile(
		const FString& UpluginFilePath,
		FString& OutPluginName);

	/**
	 * Get list of all mounted plugins from IPluginManager.
	 *
	 * @param OutPluginNames Array to fill with plugin names
	 * @return Number of plugins
	 */
	static int32 GetAllPluginNames(TArray<FString>& OutPluginNames);
};
