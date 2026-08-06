#include "Modules/ModuleManager.h"
#include "Presentation/ProjectWorldPresentationGate.h"

class FProjectWorldModule : public IModuleInterface
{
	virtual void StartupModule() override
	{
		PresentationGate = MakeUnique<FProjectWorldPresentationGate>();
		PresentationGate->StartIfRequested();
	}

	virtual void ShutdownModule() override
	{
		PresentationGate.Reset();
	}

private:
	TUniquePtr<FProjectWorldPresentationGate> PresentationGate;
};

IMPLEMENT_MODULE(FProjectWorldModule, ProjectWorld)
