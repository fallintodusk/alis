#pragma once

#include "CoreMinimal.h"
#include "Experience/ProjectExperienceDescriptorBase.h"
#include "City17ExperienceDescriptor.generated.h"

/**
 * Descriptor for the City17 world (OnDemand content).
 * Keeps the load recipe with the world plugin.
 */
UCLASS(BlueprintType)
class CITY17_API UCity17ExperienceDescriptor : public UProjectExperienceDescriptorBase
{
	GENERATED_BODY()

public:
	UCity17ExperienceDescriptor();

	virtual void BuildLoadRequest(FLoadRequest& OutRequest) const override;
	virtual void GetAssetScanSpecs(TArray<FExperienceAssetScanSpec>& OutSpecs) const override;
};
