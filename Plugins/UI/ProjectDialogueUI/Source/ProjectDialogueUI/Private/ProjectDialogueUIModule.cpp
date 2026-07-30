// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectDialogueUIModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectDialogueUI, Log, All);

void FProjectDialogueUIModule::StartupModule()
{
	UE_LOG(LogProjectDialogueUI, Log, TEXT("ProjectDialogueUI module started"));
}

void FProjectDialogueUIModule::ShutdownModule()
{
	UE_LOG(LogProjectDialogueUI, Log, TEXT("ProjectDialogueUI module shutdown"));
}

IMPLEMENT_MODULE(FProjectDialogueUIModule, ProjectDialogueUI)
