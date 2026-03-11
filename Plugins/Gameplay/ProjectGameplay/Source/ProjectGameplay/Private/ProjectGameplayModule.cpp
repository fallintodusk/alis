#include "ProjectGameplayModule.h"

#define LOCTEXT_NAMESPACE "FProjectGameplayModule"

void FProjectGameplayModule::StartupModule()
{
	// Stub: will contain base gameplay classes
}

void FProjectGameplayModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProjectGameplayModule, ProjectGameplay)
