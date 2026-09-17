#include "ProjectSettingsModule.h"

#define LOCTEXT_NAMESPACE "FProjectSettingsModule"

void FProjectSettingsModule::StartupModule()
{
}

void FProjectSettingsModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProjectSettingsModule, ProjectSettings)
