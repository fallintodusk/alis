// Copyright ALIS. All Rights Reserved.

#include "ProjectCinematicEditor.h"  // LogProjectCinematicEditor

void FProjectCinematicEditorModule::StartupModule()
{
	UE_LOG(LogProjectCinematicEditor, Log,
		TEXT("[ProjectCinematicEditor][Module] StartupModule -- editor module loaded. Recorder/Director/Subsystem available."));
}

void FProjectCinematicEditorModule::ShutdownModule()
{
	UE_LOG(LogProjectCinematicEditor, Log,
		TEXT("[ProjectCinematicEditor][Module] ShutdownModule -- editor module unloading."));
}

IMPLEMENT_MODULE(FProjectCinematicEditorModule, ProjectCinematicEditor)
