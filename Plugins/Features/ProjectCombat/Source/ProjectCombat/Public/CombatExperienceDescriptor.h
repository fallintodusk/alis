#pragma once

#include "CoreMinimal.h"
#include "Experience/ProjectExperienceDescriptorBase.h"
#include "CombatExperienceDescriptor.generated.h"

/**
 * Descriptor for the combat feature (OnDemand content).
 * - Lives with the feature plugin so the load recipe stays alongside assets.
 * - ProjectLoading builds the load request from this when combat is requested.
 */
UCLASS(BlueprintType)
class PROJECTCOMBAT_API UCombatExperienceDescriptor : public UProjectExperienceDescriptorBase
{
	GENERATED_BODY()

public:
	UCombatExperienceDescriptor();

	virtual void BuildLoadRequest(FLoadRequest& OutRequest) const override; // Reserved for future per-feature flags
};
