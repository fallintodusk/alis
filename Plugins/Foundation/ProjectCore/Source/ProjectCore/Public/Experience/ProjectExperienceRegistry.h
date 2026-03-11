// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProjectExperienceRegistry.generated.h"

class UProjectExperienceDescriptorBase;

/**
 * Singleton registry for experience descriptors (CDO-based).
 * Lives in ProjectCore to keep plugins independent of ProjectLoading.
 */
UCLASS()
class PROJECTCORE_API UProjectExperienceRegistry : public UObject
{
	GENERATED_BODY()

public:
	static UProjectExperienceRegistry* Get();

	void RegisterDescriptor(UProjectExperienceDescriptorBase* Descriptor);
	UProjectExperienceDescriptorBase* FindDescriptor(FName ExperienceName) const;

private:
	UPROPERTY()
	TMap<FName, UProjectExperienceDescriptorBase*> Descriptors;
};
