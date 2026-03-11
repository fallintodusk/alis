// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Plugin state entry in state files (latest.json, last_good.json).
 * Represents installed version and hashes for a single plugin.
 */
struct ORCHESTRATORCORE_API FPluginState
{
	FString Name;
	FString Version;
	FString Module;           // Empty for content-only plugins
	FString CodeHash;
	FString ContentHash;
	FString InstalledPath;
	FString Channel;

	FPluginState() = default;

	static FPluginState FromJson(const TSharedPtr<FJsonObject>& JsonObject);
	TSharedPtr<FJsonObject> ToJson() const;
};

/**
 * Latest/last_good state file structure.
 * Tracks currently installed plugin versions.
 */
struct ORCHESTRATORCORE_API FOrchestratorState
{
	FString ManifestVersion;
	FString EngineBuildId;
	FString AppliedAt;  // ISO 8601 timestamp
	TMap<FString, FPluginState> Plugins;  // Key = plugin name

	FOrchestratorState() = default;

	static bool LoadFromFile(const FString& FilePath, FOrchestratorState& OutState);
	bool SaveToFile(const FString& FilePath) const;

	static FOrchestratorState FromJson(const TSharedPtr<FJsonObject>& JsonObject);
	TSharedPtr<FJsonObject> ToJson() const;
};

/**
 * Pending update entry (pending_updates.json).
 * Represents code update staged for next restart.
 */
struct ORCHESTRATORCORE_API FPendingUpdate
{
	FString Name;
	FString FromVersion;
	FString ToVersion;
	FString Change;           // "code" or "content"
	FString StagedPath;
	bool bRequiresRestart = false;

	FPendingUpdate() = default;

	static FPendingUpdate FromJson(const TSharedPtr<FJsonObject>& JsonObject);
	TSharedPtr<FJsonObject> ToJson() const;
};

/**
 * Pending updates file structure.
 */
struct ORCHESTRATORCORE_API FPendingUpdates
{
	FString ManifestVersion;
	FString EngineBuildId;
	FString CreatedAt;
	TArray<FPendingUpdate> Updates;

	FPendingUpdates() = default;

	static bool LoadFromFile(const FString& FilePath, FPendingUpdates& OutUpdates);
	bool SaveToFile(const FString& FilePath) const;

	static FPendingUpdates FromJson(const TSharedPtr<FJsonObject>& JsonObject);
	TSharedPtr<FJsonObject> ToJson() const;
};
