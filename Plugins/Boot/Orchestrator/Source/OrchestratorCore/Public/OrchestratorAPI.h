// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/**
 * Stable ABI: Boot context passed from BootROM to Orchestrator.
 *
 * CRITICAL: This structure is ABI-stable and must not change without major version bump.
 * Any modification requires coordinated BootROM + Orchestrator update.
 */
struct FBootContext
{
	/** Local installation root (e.g., <local-app-data>/Alis) */
	FString LocalRoot;

	/** Engine build identifier - must match manifest.engine_build_id */
	FString EngineBuildId;

	/**
	 * Reserved for manifest JSON (not used in current immutable design).
	 * Orchestrator loads manifest from LocalRoot or project config dir.
	 * Kept for ABI compatibility - always empty string in current implementation.
	 */
	FString VerifiedManifest;

	/** Shutdown check callback */
	TFunction<bool()> IsShuttingDown;

	FBootContext()
		: IsShuttingDown([]() { return IsEngineExitRequested(); })
	{
	}
};

/**
 * Stable ABI: Orchestrator module interface.
 * BootROM loads Orchestrator by name and calls Start() with validated context.
 *
 * CRITICAL: This interface is ABI-stable. Method signatures must not change.
 */
class IOrchestratorModule : public IModuleInterface
{
public:
	/**
	 * Entry point called by BootROM after loading Orchestrator module.
	 *
	 * Orchestrator responsibilities:
	 * - Load manifest from disk (LocalRoot/Manifests/latest.json or Config/Manifest/dev_manifest.json)
	 * - Verify manifest signature (Shipping) and engine_build_id
	 * - Read local state (latest.json, last_good.json, pending_updates.json)
	 * - Resolve dependencies with cycle detection
	 * - Apply decision rule (code vs content hash changes)
	 * - Execute hot path (content-only) or cold path (code change)
	 * - Update state files atomically
	 * - Load feature modules in dependency order
	 * - Manage current_orchestrator_version.txt for self-updates
	 *
	 * @param Context Boot context from BootROM (LocalRoot + EngineBuildId only)
	 */
	virtual void Start(const FBootContext& Context) = 0;

	/**
	 * Shutdown hook for graceful cleanup.
	 */
	virtual void Stop() = 0;
};
