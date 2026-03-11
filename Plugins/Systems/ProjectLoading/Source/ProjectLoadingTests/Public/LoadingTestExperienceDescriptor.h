// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Experience/ProjectExperienceDescriptorBase.h"
#include "Engine/World.h"
#include "LoadingTestExperienceDescriptor.generated.h"

/**
 * Concrete test descriptor for ProjectLoading unit tests.
 * Lives in ProjectLoadingTests module (only compiled when tests enabled).
 */
UCLASS()
class ULoadingTestExperienceDescriptor : public UProjectExperienceDescriptorBase
{
	GENERATED_BODY()

public:
	ULoadingTestExperienceDescriptor()
	{
		ExperienceName = TEXT("TestLoading");
		LoadAssets.Map = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/PluginTest/Maps/TestMap.TestMap")));
	}
};
