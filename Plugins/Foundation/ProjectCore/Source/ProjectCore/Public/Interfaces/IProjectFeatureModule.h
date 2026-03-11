// Copyright Fall.is. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/**
 * Context passed to feature modules during boot phase initialization
 */
struct PROJECTCORE_API FBootContext
{
	/** Local application data root (e.g., <local-app-data>/Alis) */
	FString LocalRoot;

	/** Engine build identifier (must match manifest) */
	FString EngineBuildId;

	/** Function to check if system is shutting down (avoid async work during shutdown) */
	TFunction<bool()> IsShuttingDown;

	FBootContext()
		: IsShuttingDown([]() { return false; })
	{
	}
};

/**
 * Interface for feature modules that need boot-phase initialization
 *
 * DEPENDENCY INVERSION PRINCIPLE (DIP) - "D" in SOLID:
 * This interface exists in ProjectCore to prevent architectural violations.
 *
 * Architecture:
 *   Feature Modules (ProjectCombat, ProjectInventory, etc.)
 *        |
 *        v depends on abstraction
 *   IProjectFeatureModule (THIS INTERFACE - in ProjectCore)
 *        ^
 *        | queries implementations
 *   OrchestratorCore (boot system)
 *
 * NOTE: Most features DON'T need this! Only implement if you need:
 * - Early initialization before GameInstance/World exists
 * - Access to Orchestrator context (LocalRoot, EngineBuildId)
 * - Guaranteed dependency order during boot
 *
 * If your feature just needs normal UE lifecycle, use StartupModule() only.
 *
 * Lifecycle:
 *   1. StartupModule()    - FModuleManager loads module (UE standard)
 *   2. BootInitialize()   - Orchestrator calls after dependencies ready (BOOT PHASE)
 *   3. BootShutdown()     - Orchestrator calls during shutdown (NOT YET IMPLEMENTED)
 *   4. ShutdownModule()   - FModuleManager unloads module (UE standard)
 *
 * IMPORTANT: Register with FeatureModuleRegistry in StartupModule() to receive boot callbacks.
 *
 * Usage example:
 *   // ProjectCombat.cpp
 *   #include "Interfaces/IFeatureModuleRegistry.h"
 *
 *   void FProjectCombatModule::StartupModule()
 *   {
 *       RegisterFeatureModule(TEXT("ProjectCombat"), this);
 *   }
 *
 *   void FProjectCombatModule::BootInitialize(const FBootContext& Context)
 *   {
 *       UE_LOG(LogTemp, Log, TEXT("ProjectCombat boot init - LocalRoot: %s"), *Context.LocalRoot);
 *   }
 */
class PROJECTCORE_API IProjectFeatureModule : public IModuleInterface
{
public:
	/**
	 * Called by Orchestrator during BOOT PHASE (before GameInstance/World)
	 * Only implement if you need early initialization with Orchestrator context
	 *
	 * @param Context Boot context with LocalRoot, EngineBuildId, etc.
	 */
	virtual void BootInitialize(const FBootContext& Context) {}

	/**
	 * Called by Orchestrator during shutdown (after game, before module unload)
	 * Override if you need to clean up resources acquired in BootInitialize()
	 *
	 * NOTE: Currently NOT called by Orchestrator - stub for future use.
	 * When implemented, will be called in reverse dependency order.
	 */
	virtual void BootShutdown() {}

	/**
	 * Returns list of plugin names this feature depends on
	 * Orchestrator uses this for boot-phase dependency ordering
	 *
	 * @return Array of plugin names (e.g., {"ProjectCore", "ProjectUI"})
	 */
	virtual TArray<FName> GetDependencies() const { return {}; }
};
