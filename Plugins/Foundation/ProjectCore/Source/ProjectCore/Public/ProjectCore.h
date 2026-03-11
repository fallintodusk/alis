// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * ProjectCore Module - ABSTRACTION LAYER (SOLID Compliance)
 *
 * Foundation module providing:
 * - Interface definitions (Interfaces/*.h) for cross-module communication
 * - Shared data types and structures
 * - Service locator / dependency injection framework
 * - Logging and configuration utilities
 * - Validation helpers
 *
 * DEPENDENCY INVERSION PRINCIPLE (DIP):
 * This module serves as the stable abstraction layer between high-level and low-level modules.
 *
 * Architecture Rule:
 *   High-Level Modules (ProjectLoading, ProjectSession, etc.)
 *        |
 *        v depends on abstraction
 *   ProjectCore (THIS MODULE - interfaces only)
 *        ^
 *        | implements abstraction
 *   Low-Level Modules (OrchestratorCore, BootROM, etc.)
 *
 * WHY: High-level game systems must NOT depend on low-level boot infrastructure.
 *      Both depend on abstractions defined here (e.g., IOrchestratorRegistry).
 *
 * This module contains NO asset references and NO gameplay logic.
 * All other Project modules depend on this core foundation.
 */
class PROJECTCORE_API FProjectCoreModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	 * Get the ProjectCore module instance
	 * @return Reference to the ProjectCore module
	 */
	static inline FProjectCoreModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FProjectCoreModule>("ProjectCore");
	}

	/**
	 * Check if the ProjectCore module is loaded
	 * @return True if the module is currently loaded
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("ProjectCore");
	}
};
