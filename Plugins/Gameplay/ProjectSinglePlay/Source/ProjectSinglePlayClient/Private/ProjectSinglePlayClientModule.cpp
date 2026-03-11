#include "ProjectSinglePlayClientModule.h"
#include "ProjectSinglePlayLog.h"

#define LOCTEXT_NAMESPACE "FProjectSinglePlayClientModule"

void FProjectSinglePlayClientModule::StartupModule()
{
	UE_LOG(LogProjectSinglePlay, Log, TEXT("ProjectSinglePlayClient module started"));
}

void FProjectSinglePlayClientModule::ShutdownModule()
{
	UE_LOG(LogProjectSinglePlay, Log, TEXT("ProjectSinglePlayClient module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProjectSinglePlayClientModule, ProjectSinglePlayClient)
