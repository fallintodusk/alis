#include "ProjectMenuMainModule.h"
#include "ProjectMenuMainLog.h"
#include "Interfaces/IFeatureModuleRegistry.h"

DEFINE_LOG_CATEGORY(LogProjectMenuMain);

void FProjectMenuMainModule::StartupModule()
{
	// Register with feature module registry for boot-phase callbacks
	RegisterFeatureModule(TEXT("ProjectMenuMain"), this);

	UE_LOG(LogProjectMenuMain, Log, TEXT("StartupModule() - registered with feature module registry"));
}

void FProjectMenuMainModule::ShutdownModule()
{
	UnregisterFeatureModule(TEXT("ProjectMenuMain"));

	UE_LOG(LogProjectMenuMain, Log, TEXT("ShutdownModule()"));
}

void FProjectMenuMainModule::BootInitialize(const FBootContext& Context)
{
	// Called by Orchestrator during boot phase (before GameInstance/World)
	// Boot plugins receive this callback; OnDemand plugins skip it.
	UE_LOG(LogProjectMenuMain, Log, TEXT("BootInitialize() - LocalRoot: %s, EngineBuildId: %s"),
		*Context.LocalRoot, *Context.EngineBuildId);
}

void FProjectMenuMainModule::BootShutdown()
{
	// Called by Orchestrator during shutdown (NOT YET IMPLEMENTED in Orchestrator)
	UE_LOG(LogProjectMenuMain, Log, TEXT("BootShutdown()"));
}

IMPLEMENT_MODULE(FProjectMenuMainModule, ProjectMenuMain);
