// Copyright ALIS. All Rights Reserved.

#include "ProjectAssetSyncModule.h"

#define LOCTEXT_NAMESPACE "FProjectAssetSyncModule"

void FProjectAssetSyncModule::StartupModule()
{
	// UJsonRefSyncSubsystem is auto-initialized by UEditorSubsystem framework
}

void FProjectAssetSyncModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProjectAssetSyncModule, ProjectAssetSync)
