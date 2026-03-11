#include "ProjectObjectModule.h"
#include "Services/ObjectSpawnServiceImpl.h"
#include "ProjectServiceLocator.h"

DEFINE_LOG_CATEGORY(LogProjectObject);

#define LOCTEXT_NAMESPACE "FProjectObjectModule"

namespace
{
	TSharedPtr<FObjectSpawnServiceImpl> GObjectSpawnService;
}

void FProjectObjectModule::StartupModule()
{
	GObjectSpawnService = MakeShared<FObjectSpawnServiceImpl>();
	FProjectServiceLocator::Register<IObjectSpawnService>(GObjectSpawnService);
}

void FProjectObjectModule::ShutdownModule()
{
	FProjectServiceLocator::Unregister<IObjectSpawnService>();
	GObjectSpawnService.Reset();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProjectObjectModule, ProjectObject)
