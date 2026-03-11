#include "ProjectMenuPlayModule.h"

#define LOCTEXT_NAMESPACE "FProjectMenuPlayModule"

void FProjectMenuPlayModule::StartupModule()
{
	// Module is intentionally lean; GameMode lives in this plugin and is referenced from menu maps.
}

void FProjectMenuPlayModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProjectMenuPlayModule, ProjectMenuPlay)
