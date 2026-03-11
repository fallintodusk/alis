#pragma once

#include "CoreMinimal.h"
#include "SinglePlayModeConfig.h"

/**
 * Static registry for single-play mode configurations.
 *
 * Pure C++ driven - no UAssets required. Agents can edit mode definitions
 * directly in SinglePlayModeDefaults.cpp.
 *
 * Optional JSON override support:
 *   Place Config/SinglePlay/ModeOverrides.json in project root to override
 *   or extend C++ mode definitions at runtime.
 *
 * Usage:
 *   // Register a mode (typically in module startup or static init)
 *   FSinglePlayModeRegistry::RegisterMode(MyConfig);
 *
 *   // Find a mode at runtime
 *   const FSinglePlayModeConfig* Config = FSinglePlayModeRegistry::FindMode(FName("Single"));
 */
class PROJECTSINGLEPLAY_API FSinglePlayModeRegistry
{
public:
	// Register a mode configuration
	// Call during module startup or via static initializer
	static void RegisterMode(const FSinglePlayModeConfig& Config);

	// Find a mode by name, returns nullptr if not found
	static const FSinglePlayModeConfig* FindMode(FName ModeName);

	// Get all registered mode names
	static TArray<FName> GetAllModeNames();

	// Check if a mode is registered
	static bool HasMode(FName ModeName);

	// Clear all registered modes (mainly for testing)
	static void ClearAll();

	// Initialize default modes (called by module startup)
	static void InitializeDefaults();

	/**
	 * Load JSON overrides from Config/SinglePlay/ModeOverrides.json
	 * Merges with existing C++ definitions (JSON values override C++ values)
	 * Invalid entries are logged and skipped (guardrail)
	 *
	 * Called automatically during InitializeDefaults() if file exists
	 */
	static void LoadJsonOverrides();

	/**
	 * Load JSON overrides from a specific file path
	 * @param JsonFilePath Absolute or project-relative path to JSON file
	 * @return Number of modes successfully loaded/merged
	 */
	static int32 LoadJsonOverridesFromFile(const FString& JsonFilePath);

private:
	static TMap<FName, FSinglePlayModeConfig>& GetRegistry();
	static bool bDefaultsInitialized;
	static bool bJsonOverridesLoaded;

	// Parse a single mode entry from JSON object
	// Returns true if parsing succeeded
	static bool ParseModeFromJson(const TSharedPtr<FJsonObject>& ModeObject, FSinglePlayModeConfig& OutConfig);
};

/**
 * Helper macro to define a mode config inline.
 *
 * Usage in SinglePlayModeDefaults.cpp:
 *
 *   DEFINE_SINGLEPLAY_MODE(Single)
 *   {
 *       Config.ModeName = FName(TEXT("Single"));
 *       Config.FeatureConfigs.Add(FName(TEXT("Difficulty")), TEXT("{\"level\":\"normal\"}"));
 *   }
 */
#define DEFINE_SINGLEPLAY_MODE(ModeName) \
	static struct FSinglePlayModeInit_##ModeName \
	{ \
		FSinglePlayModeInit_##ModeName() \
		{ \
			FSinglePlayModeConfig Config; \
			InitConfig(Config); \
			FSinglePlayModeRegistry::RegisterMode(Config); \
		} \
		static void InitConfig(FSinglePlayModeConfig& Config); \
	} GModInit_##ModeName; \
	void FSinglePlayModeInit_##ModeName::InitConfig(FSinglePlayModeConfig& Config)
