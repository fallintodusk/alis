// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

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
