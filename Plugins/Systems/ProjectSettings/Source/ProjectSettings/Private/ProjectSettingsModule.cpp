#include "ProjectSettingsModule.h"

#define LOCTEXT_NAMESPACE "FProjectSettingsModule"

void FProjectSettingsModule::StartupModule()
{
	// Stub: will contain settings storage and API
}

void FProjectSettingsModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProjectSettingsModule, ProjectSettings)
