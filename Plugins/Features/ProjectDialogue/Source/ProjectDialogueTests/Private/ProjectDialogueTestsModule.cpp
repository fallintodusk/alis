// Copyright ALIS. All Rights Reserved.

#include "Modules/ModuleManager.h"

/**
 * Test module for ProjectDialogue plugin.
 *
 * Tests dialogue component functionality including conversation state management,
 * dialogue navigation, and event handling.
 */
class FProjectDialogueTestsModule : public IModuleInterface
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

IMPLEMENT_MODULE(FProjectDialogueTestsModule, ProjectDialogueTests)
