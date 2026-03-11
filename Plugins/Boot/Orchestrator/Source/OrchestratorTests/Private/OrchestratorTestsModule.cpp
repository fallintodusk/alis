// Copyright ALIS. All Rights Reserved.

#include "Modules/ModuleManager.h"

class FOrchestratorTestsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// Module startup
	}

	virtual void ShutdownModule() override
	{
		// Module shutdown
	}
};

IMPLEMENT_MODULE(FOrchestratorTestsModule, OrchestratorTests)
