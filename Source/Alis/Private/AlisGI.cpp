// Fill out your copyright notice in the Description page of Project Settings.


#include "AlisGI.h"
#include "Experience/ProjectExperienceDescriptorBase.h"
#include "Experience/ProjectExperienceRegistry.h"
#include "Misc/ConfigCacheIni.h"
#include "ProjectServiceLocator.h"

void UAlisGI::Init()
{
	Super::Init();

	UE_LOG(LogTemp, Log, TEXT("UAlisGI::Init - starting (EntryPointExperience from config/state)"));

	// Resolve the entry experience name from config (default: MainMenu) to avoid hardcoding.
	FString EntryExperienceName = TEXT("MainMenuWorld");
	GConfig->GetString(TEXT("/Script/Alis.AlisGI"), TEXT("EntryPointExperience"), EntryExperienceName, GGameIni);
	UE_LOG(LogTemp, Log, TEXT("UAlisGI::Init - EntryPointExperience='%s' (provided to subsystems)"), *EntryExperienceName);
}
