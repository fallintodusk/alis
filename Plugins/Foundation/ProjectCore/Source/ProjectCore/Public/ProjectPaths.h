// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Resolves paths to plugin data in Plugins/<Category>/<PluginName>/Data/.
 * Mirrors the Plugins/ folder structure for consistency.
 *
 * Structure:
 *   Plugins/<Category>/<PluginName>/Data/<Filename>
 *
 * Example:
 *   Plugins/UI/ProjectUI/           -> Plugins/UI/ProjectUI/Data/
 *   Plugins/Resources/ProjectObject/ -> Plugins/Resources/ProjectObject/Data/
 */
class PROJECTCORE_API FProjectPaths final
{
public:
	/**
	 * Get the base data directory for a plugin.
	 * @param PluginName - Plugin name (e.g., "ProjectUI")
	 * @return Absolute path to Apps/Alis/Plugins/<Plugin>/Data/
	 */
	static FString GetPluginDataDir(const FString& PluginName);

	// Helper for checking file existence in plugin data
	static bool PluginDataFileExists(const FString& PluginName, const FString& RelativePath);
};
