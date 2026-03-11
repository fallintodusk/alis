#include "ProjectOnlinePlayModule.h"

#define LOCTEXT_NAMESPACE "FProjectOnlinePlayModule"

void FProjectOnlinePlayModule::StartupModule()
{
	// Stub: will contain GameMode for online multiplayer
}

void FProjectOnlinePlayModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProjectOnlinePlayModule, ProjectOnlinePlay)
