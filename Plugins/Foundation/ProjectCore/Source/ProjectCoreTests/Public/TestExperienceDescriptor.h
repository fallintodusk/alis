// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Experience/ProjectExperienceDescriptorBase.h"
#include "Engine/World.h"
#include "TestExperienceDescriptor.generated.h"

/**
 * Concrete test descriptor for unit tests.
 * Lives in ProjectCoreTests module (only compiled when tests enabled).
 */
UCLASS()
class UTestExperienceDescriptor : public UProjectExperienceDescriptorBase
{
	GENERATED_BODY()

public:
	UTestExperienceDescriptor()
	{
		ExperienceName = TEXT("TestExperience");
		LoadAssets.Map = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/PluginTest/Maps/TestMap.TestMap")));
	}

	virtual void GetAssetScanSpecs(TArray<FExperienceAssetScanSpec>& OutSpecs) const override
	{
		FExperienceAssetScanSpec Spec;
		Spec.PrimaryAssetType = TEXT("Map");
		Spec.BaseClass = UWorld::StaticClass();
		Spec.Directories.Add(TEXT("/PluginTest/Maps"));
		OutSpecs.Add(Spec);
	}
};
