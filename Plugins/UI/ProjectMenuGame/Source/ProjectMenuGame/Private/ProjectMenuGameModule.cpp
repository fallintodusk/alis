#include "ProjectMenuGameModule.h"
#include "ProjectGameMenuService.h"
#include "ProjectServiceLocator.h"

#define LOCTEXT_NAMESPACE "FProjectMenuGameModule"

namespace
{
	TSharedPtr<IGameMenuService> GameMenuService;
}

void FProjectMenuGameModule::StartupModule()
{
	GameMenuService = MakeShared<FProjectGameMenuService>();
	FProjectServiceLocator::Register<IGameMenuService>(GameMenuService);
}

void FProjectMenuGameModule::ShutdownModule()
{
	FProjectServiceLocator::Unregister<IGameMenuService>();
	GameMenuService.Reset();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProjectMenuGameModule, ProjectMenuGame)
