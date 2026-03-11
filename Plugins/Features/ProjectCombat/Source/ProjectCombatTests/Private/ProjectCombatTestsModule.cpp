// Copyright ALIS. All Rights Reserved.

#include "Modules/ModuleManager.h"

/**
 * Test module for ProjectCombat plugin.
 *
 * Tests combat component functionality including health management,
 * damage application, healing, and death state.
 */
class FProjectCombatTestsModule : public IModuleInterface
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

IMPLEMENT_MODULE(FProjectCombatTestsModule, ProjectCombatTests)
