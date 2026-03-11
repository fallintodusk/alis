#pragma once

#include "CoreMinimal.h"
#include "Experience/ProjectExperienceDescriptorBase.h"
#include "InventoryExperienceDescriptor.generated.h"

/**
 * Descriptor for the inventory feature (OnDemand content).
 * Keeps the load recipe with the feature plugin.
 */
UCLASS(BlueprintType)
class PROJECTINVENTORY_API UInventoryExperienceDescriptor : public UProjectExperienceDescriptorBase
{
	GENERATED_BODY()

public:
	UInventoryExperienceDescriptor();

	virtual void BuildLoadRequest(FLoadRequest& OutRequest) const override; // Reserved for per-feature flags
};
