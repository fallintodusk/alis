// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

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
	/**
	 * Deliberately NOT a UPROPERTY, and every entry is explicitly rooted instead.
	 *
	 * This singleton is created during module startup, before the engine closes the
	 * disregard-for-GC window, so the registry object itself lives in that set. A
	 * disregard-for-GC object may not hold reflected references to ordinary objects: doing so
	 * trips VerifyGCAssumptions and fatally crashes the packaged game at the first GC.
	 *
	 * CDO descriptors are permanent objects and were always safe, but descriptors created from
	 * configured data are ordinary objects. Rooting each entry keeps lifetime guaranteed while
	 * leaving this object with no reflected outgoing references. Registration is
	 * process-lifetime by design; there is no unregister path.
	 */
	TMap<FName, UProjectExperienceDescriptorBase*> Descriptors;
};
