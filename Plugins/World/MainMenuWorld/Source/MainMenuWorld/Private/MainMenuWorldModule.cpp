#include "MainMenuWorldModule.h"

#include "Experience/ProjectExperienceRegistry.h"
#include "MainMenuExperienceDescriptor.h"

void FMainMenuWorldModule::StartupModule()
{
	UProjectExperienceRegistry::Get()->RegisterDescriptor(GetMutableDefault<UMainMenuExperienceDescriptor>());
}

void FMainMenuWorldModule::ShutdownModule()
{
	// No dynamic allocations to clean up (registry owns CDO pointer).
}

IMPLEMENT_MODULE(FMainMenuWorldModule, MainMenuWorld)
