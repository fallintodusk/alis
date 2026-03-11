#include "ProjectMenuGameModule.h"

#define LOCTEXT_NAMESPACE "FProjectMenuGameModule"

void FProjectMenuGameModule::StartupModule()
{
	// Stub: will contain in-game pause menu
}

void FProjectMenuGameModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProjectMenuGameModule, ProjectMenuGame)
