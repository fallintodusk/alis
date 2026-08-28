#include "ProjectSinglePlayClientModule.h"
#include "ProjectSinglePlayLog.h"
#include "Scenario/SinglePlayScenarioPackagedGate.h"

namespace
{
	TUniquePtr<FSinglePlayScenarioPackagedGate> ScenarioPackagedGate;
}

#define LOCTEXT_NAMESPACE "FProjectSinglePlayClientModule"

void FProjectSinglePlayClientModule::StartupModule()
{
	UE_LOG(LogProjectSinglePlay, Log, TEXT("ProjectSinglePlayClient module started"));
	ScenarioPackagedGate = MakeUnique<FSinglePlayScenarioPackagedGate>();
	ScenarioPackagedGate->StartIfRequested();
}

void FProjectSinglePlayClientModule::ShutdownModule()
{
	UE_LOG(LogProjectSinglePlay, Log, TEXT("ProjectSinglePlayClient module shutdown"));
	ScenarioPackagedGate.Reset();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProjectSinglePlayClientModule, ProjectSinglePlayClient)
