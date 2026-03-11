// Copyright Fall.is. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Forward declaration
class IProjectFeatureModule;

/**
 * Feature Module Registry - Global registration API
 *
 * DEPENDENCY INVERSION PRINCIPLE (DIP) - "D" in SOLID:
 * This registry exists in ProjectCore to prevent circular dependencies.
 *
 * Architecture:
 *   Feature Modules (ProjectCombat, ProjectInventory, etc.)
 *        |
 *        v registers via abstraction
 *   IFeatureModuleRegistry (THIS FILE - in ProjectCore)
 *        ^
 *        | queries registered modules
 *   OrchestratorCore (boot system)
 *
 * WHY: Features and Orchestrator both depend on ProjectCore (stable abstraction).
 *      Neither depends on the other directly. This breaks the circular dependency.
 *
 * Storage: Static TMap<FName, IProjectFeatureModule*> owned by ProjectCore.
 * Thread Safety: Registration assumed to happen during module startup (single-threaded).
 *
 * Usage in Feature Module:
 *   #include "Interfaces/IFeatureModuleRegistry.h"
 *   #include "Interfaces/IProjectFeatureModule.h"
 *
 *   void FProjectCombatModule::StartupModule()
 *   {
 *       RegisterFeatureModule(TEXT("ProjectCombat"), this);
 *   }
 *
 *   void FProjectCombatModule::ShutdownModule()
 *   {
 *       UnregisterFeatureModule(TEXT("ProjectCombat"));
 *   }
 *
 * Usage in Orchestrator:
 *   const TMap<FName, IProjectFeatureModule*>& Modules = GetFeatureModuleRegistry();
 *   for (const auto& Pair : Modules)
 *   {
 *       Pair.Value->BootInitialize(Context);
 *   }
 */

/**
 * Register a feature module for lifecycle callbacks
 * Feature modules implementing IProjectFeatureModule should call this during StartupModule()
 *
 * @param ModuleName The name of the module (e.g., "ProjectCombat")
 * @param FeatureModule Pointer to the feature module interface
 */
PROJECTCORE_API void RegisterFeatureModule(FName ModuleName, IProjectFeatureModule* FeatureModule);

/**
 * Unregister a feature module (called during ShutdownModule())
 *
 * @param ModuleName The name of the module to unregister
 */
PROJECTCORE_API void UnregisterFeatureModule(FName ModuleName);

/**
 * Get the global feature module registry
 *
 * @return Reference to the registry map (module name -> interface pointer)
 */
PROJECTCORE_API const TMap<FName, IProjectFeatureModule*>& GetFeatureModuleRegistry();

/**
 * Check if a feature module is registered
 *
 * @param ModuleName The name of the module to check
 * @return True if module is registered
 */
PROJECTCORE_API bool IsFeatureModuleRegistered(FName ModuleName);

/**
 * Get a specific feature module by name
 *
 * @param ModuleName The name of the module
 * @return Pointer to the feature module, or nullptr if not registered
 */
PROJECTCORE_API IProjectFeatureModule* GetFeatureModule(FName ModuleName);
