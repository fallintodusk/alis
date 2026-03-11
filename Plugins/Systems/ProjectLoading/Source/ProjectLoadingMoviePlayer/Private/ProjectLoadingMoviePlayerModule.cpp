// Copyright ALIS. All Rights Reserved.

#include "ProjectLoadingMoviePlayer.h"

#include "Modules/ModuleManager.h"
#include "ProjectLoadingLog.h"

IMPLEMENT_MODULE(FProjectLoadingMoviePlayerModule, ProjectLoadingMoviePlayer);

void FProjectLoadingMoviePlayerModule::StartupModule()
{
	UE_LOG(LogProjectLoading, Display, TEXT("ProjectLoadingMoviePlayer module startup"));
}

void FProjectLoadingMoviePlayerModule::ShutdownModule()
{
}
