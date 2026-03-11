// Copyright ALIS. All Rights Reserved.

#include "ProjectDefinitionGenerator.h"
#include "ToolMenus.h"

DEFINE_LOG_CATEGORY(LogProjectDefinitionGenerator);

// Forward declare menu functions
namespace DefinitionGeneratorMenu
{
	void RegisterMenus(void* Owner);
	void UnregisterMenus();
}

void FProjectDefinitionGeneratorModule::StartupModule()
{
	UE_LOG(LogProjectDefinitionGenerator, Log, TEXT("ProjectDefinitionGenerator module started"));

	// Register menus after ToolMenus is ready
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FProjectDefinitionGeneratorModule::RegisterMenus)
	);
}

void FProjectDefinitionGeneratorModule::RegisterMenus()
{
	DefinitionGeneratorMenu::RegisterMenus(this);
}

void FProjectDefinitionGeneratorModule::ShutdownModule()
{
	UE_LOG(LogProjectDefinitionGenerator, Log, TEXT("ProjectDefinitionGenerator module shutdown"));

	if (UObjectInitialized())
	{
		DefinitionGeneratorMenu::UnregisterMenus();
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
	}
}

IMPLEMENT_MODULE(FProjectDefinitionGeneratorModule, ProjectDefinitionGenerator)
