#include "ProjectSinglePlayModule.h"
#include "ProjectSinglePlayLog.h"

#define LOCTEXT_NAMESPACE "FProjectSinglePlayModule"

void FProjectSinglePlayModule::StartupModule()
{
	UE_LOG(LogProjectSinglePlay, Log, TEXT("ProjectSinglePlay module started"));
}

void FProjectSinglePlayModule::ShutdownModule()
{
	UE_LOG(LogProjectSinglePlay, Log, TEXT("ProjectSinglePlay module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProjectSinglePlayModule, ProjectSinglePlay)
