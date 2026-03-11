// Copyright ALIS. All Rights Reserved.

#include "Modules/ModuleManager.h"

/**
 * ProjectCoreTests module - Unit tests for ProjectCore plugin
 */
class FProjectCoreTestsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE(FProjectCoreTestsModule, ProjectCoreTests)
