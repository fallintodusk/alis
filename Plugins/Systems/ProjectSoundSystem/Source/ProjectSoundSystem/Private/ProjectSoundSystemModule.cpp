// Copyright ALIS. All Rights Reserved.

#include "ProjectSoundSystemModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectSoundSystem, Log, All);

IMPLEMENT_MODULE(FProjectSoundSystemModule, ProjectSoundSystem)

void FProjectSoundSystemModule::StartupModule()
{
	UE_LOG(LogProjectSoundSystem, Log, TEXT("ProjectSoundSystem module startup"));
}

void FProjectSoundSystemModule::ShutdownModule()
{
	UE_LOG(LogProjectSoundSystem, Log, TEXT("ProjectSoundSystem module shutdown"));
}
