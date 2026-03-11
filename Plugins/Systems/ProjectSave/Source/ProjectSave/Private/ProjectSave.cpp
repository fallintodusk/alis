// Copyright ALIS. All Rights Reserved.

#include "ProjectSave.h"

#define LOCTEXT_NAMESPACE "FProjectSaveModule"

void FProjectSaveModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("ProjectSave module started"));
}

void FProjectSaveModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("ProjectSave module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProjectSaveModule, ProjectSave)
