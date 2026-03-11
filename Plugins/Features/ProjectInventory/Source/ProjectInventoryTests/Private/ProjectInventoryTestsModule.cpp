// Copyright ALIS. All Rights Reserved.

#include "Modules/ModuleManager.h"

/**
 * Test module for ProjectInventory plugin.
 *
 * Tests inventory component functionality including item management,
 * weight/slot constraints, and event handling.
 */
class FProjectInventoryTestsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// Test module startup
	}

	virtual void ShutdownModule() override
	{
		// Test module shutdown
	}
};

IMPLEMENT_MODULE(FProjectInventoryTestsModule, ProjectInventoryTests)
