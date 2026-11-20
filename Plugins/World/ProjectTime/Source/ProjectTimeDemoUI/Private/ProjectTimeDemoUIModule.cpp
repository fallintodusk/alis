#include "Modules/ModuleManager.h"
#include "ProjectTimeRuntimeProof.h"

class FProjectTimeDemoUIModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override { ProjectTimeRuntimeProof::Start(); }
	virtual void ShutdownModule() override { ProjectTimeRuntimeProof::Stop(); }
};
IMPLEMENT_MODULE(FProjectTimeDemoUIModule, ProjectTimeDemoUI)
