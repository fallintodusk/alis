#include "ProjectCoreEditor.h"

void FProjectCoreEditorModule::StartupModule()
{
	// Filter extension auto-registers via CDO
}

void FProjectCoreEditorModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FProjectCoreEditorModule, ProjectCoreEditor)
