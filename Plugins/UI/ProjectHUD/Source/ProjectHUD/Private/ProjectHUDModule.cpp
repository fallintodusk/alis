// Copyright ALIS. All Rights Reserved.

#include "ProjectHUDModule.h"

IMPLEMENT_MODULE(FProjectHUDModule, ProjectHUD)

void FProjectHUDModule::StartupModule()
{
	// Module is intentionally thin - no registration here
	// ProjectUI registry/factory owns widget creation
}

void FProjectHUDModule::ShutdownModule()
{
}
