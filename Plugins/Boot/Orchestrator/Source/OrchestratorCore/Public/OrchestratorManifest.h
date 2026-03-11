// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Dependency constraint for a plugin.
 */
struct ORCHESTRATORCORE_API FPluginDependency
{
	FString Name;
	FString VersionConstraint;  // e.g., ">=1.4.0"

	FPluginDependency() = default;
	FPluginDependency(const FString& InName, const FString& InVersionConstraint)
		: Name(InName), VersionConstraint(InVersionConstraint) {}

	static FPluginDependency FromJson(const TSharedPtr<FJsonObject>& JsonObject);
	TSharedPtr<FJsonObject> ToJson() const;
};

/**
 * Full plugin entry in manifest (Orchestrator-level, richer than BootROM's FBootPluginEntry).
 * Includes dependency information for resolution.
 */
struct ORCHESTRATORCORE_API FOrchestratorPluginEntry
{
	FString Name;
	FString Version;
	FString ActivationStrategy;  // "Boot" (load at startup) or "OnDemand" (lazy-load when requested)
	FString Module;              // Empty for content-only plugins
	FString CodeHash;
	FString ContentHash;
	FString UrlCode;
	FString UrlContent;
	FString Channel;             // stable|optional|experimental|dev
	FString SignatureThumbprint;
	int64 SizeCode = 0;
	int64 SizeContent = 0;
	FString ReleaseNotes;
	TArray<FString> Mirrors;

	// Engine plugins required by this plugin (e.g., ["CommonUI", "EnhancedInput"])
	TArray<FString> RequiresEnginePlugins;

	// Dependency constraints (authoritative)
	TArray<FPluginDependency> DependsOn;

	FOrchestratorPluginEntry() = default;

	static FOrchestratorPluginEntry FromJson(const TSharedPtr<FJsonObject>& JsonObject);
	TSharedPtr<FJsonObject> ToJson() const;
};

/**
 * Full manifest structure (Orchestrator-level).
 * Parses complete manifest including dependencies.
 */
struct ORCHESTRATORCORE_API FOrchestratorManifest
{
	FString ManifestVersion;
	FString EngineBuildId;
	FString SignedAt;
	FString SigningKeyId;
	FString Signature;

	TArray<FOrchestratorPluginEntry> Plugins;

	FOrchestratorManifest() = default;

	/** Load from JSON string (Orchestrator loads from disk, not from BootContext) */
	static bool LoadFromString(const FString& JsonString, FOrchestratorManifest& OutManifest);

	/** Find plugin by name */
	const FOrchestratorPluginEntry* FindPlugin(const FString& PluginName) const;
};
