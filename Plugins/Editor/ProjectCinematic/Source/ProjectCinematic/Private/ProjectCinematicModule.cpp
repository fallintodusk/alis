// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectCinematic.h"

DEFINE_LOG_CATEGORY(LogProjectCinematic);

void FProjectCinematicModule::StartupModule()
{
	UE_LOG(LogProjectCinematic, Log,
		TEXT("[ProjectCinematic][Module] StartupModule -- runtime module loaded. ACinematicGameMode available."));
}

void FProjectCinematicModule::ShutdownModule()
{
	UE_LOG(LogProjectCinematic, Log,
		TEXT("[ProjectCinematic][Module] ShutdownModule -- runtime module unloading."));
}

IMPLEMENT_MODULE(FProjectCinematicModule, ProjectCinematic)
