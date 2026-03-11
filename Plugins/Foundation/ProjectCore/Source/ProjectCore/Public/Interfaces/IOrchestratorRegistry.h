// Copyright Fall.is. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Registry API for querying loaded features
 *
 * DEPENDENCY INVERSION PRINCIPLE (DIP) - "D" in SOLID:
 * This interface exists in ProjectCore to prevent architectural violations.
 *
 * Architecture:
 *   ProjectLoading (high-level)
 *        |
 *        v depends on abstraction
 *   IOrchestratorRegistry (THIS INTERFACE - in ProjectCore)
 *        ^
 *        | implements abstraction
 *   OrchestratorCore (low-level)
 *
 * WHY: High-level game systems must NOT depend on low-level boot infrastructure.
 *      This interface decouples them.
 *
 * Roles:
 * - Defined in ProjectCore (shared abstraction layer)
 * - Implemented by OrchestratorCore (low-level boot system)
 * - Consumed by ProjectLoading, ProjectSession, etc. (high-level systems)
 *
 * Usage:
 *   IOrchestratorRegistry* Registry = GetOrchestratorRegistry();
 *   if (Registry && Registry->IsFeatureAvailable("ProjectMenuCore"))
 *   {
 *       // Feature is loaded and available
 *   }
 */
class PROJECTCORE_API IOrchestratorRegistry
{
public:
	virtual ~IOrchestratorRegistry() = default;

	/**
	 * Check if a feature plugin is loaded and available
	 *
	 * @param PluginName The plugin name (e.g., "ProjectMenuCore")
	 * @return True if plugin is registered, mounted, and module loaded
	 */
	virtual bool IsFeatureAvailable(FName PluginName) const = 0;

	/**
	 * Get list of all currently loaded feature plugins
	 *
	 * @return Array of plugin names that are active
	 */
	virtual TArray<FName> GetLoadedFeatures() const = 0;

	/**
	 * Get version of a loaded feature plugin
	 *
	 * @param PluginName The plugin name
	 * @return Version string (e.g., "1.2.3"), or empty if not loaded
	 */
	virtual FString GetFeatureVersion(FName PluginName) const = 0;
};

/**
 * Global accessor for Orchestrator registry
 * Returns nullptr if Orchestrator not initialized
 *
 * Implementation in ProjectCore, instance set by OrchestratorCore at startup
 */
PROJECTCORE_API IOrchestratorRegistry* GetOrchestratorRegistry();

/**
 * Set the global Orchestrator registry instance
 * Called by OrchestratorCore during module startup
 *
 * @param Registry The registry implementation (or nullptr to clear)
 */
PROJECTCORE_API void SetOrchestratorRegistry(IOrchestratorRegistry* Registry);
