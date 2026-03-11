#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IProjectFeatureModule.h"

/**
 * Main menu module - Boot plugin with Orchestrator lifecycle integration.
 *
 * Demonstrates IProjectFeatureModule boot-phase callbacks for Boot plugins.
 * OnDemand plugins (like ProjectCombat) don't use this interface.
 */
class PROJECTMENUMAIN_API FProjectMenuMainModule : public IProjectFeatureModule
{
public:
	// IModuleInterface (UE standard)
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// IProjectFeatureModule (Orchestrator boot phase)
	virtual void BootInitialize(const FBootContext& Context) override;
	virtual void BootShutdown() override;
};

