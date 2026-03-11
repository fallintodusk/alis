#include "Modules/ModuleManager.h"

class FProjectIntegrationTestsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FProjectIntegrationTestsModule, ProjectIntegrationTests)
