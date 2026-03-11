// Copyright ALIS. All Rights Reserved.

#include "Modules/ModuleManager.h"

/**
 * ProjectSaveTests module - Unit tests for ProjectSave plugin
 */
class FProjectSaveTestsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE(FProjectSaveTestsModule, ProjectSaveTests)
