#include "City17Module.h"

#include "City17ExperienceDescriptor.h"
#include "Experience/ProjectExperienceRegistration.h"

void FCity17Module::StartupModule()
{
	ProjectExperience::RegisterDescriptor<UCity17ExperienceDescriptor>();
}

void FCity17Module::ShutdownModule()
{
	// No dynamic allocations to clean up (registry owns CDO pointer).
}

IMPLEMENT_MODULE(FCity17Module, City17)
