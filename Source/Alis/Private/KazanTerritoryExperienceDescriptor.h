// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Experience/ProjectExperienceDescriptorBase.h"
#include "KazanTerritoryExperienceDescriptor.generated.h"

UCLASS()
class UKazanTerritoryExperienceDescriptor final : public UProjectExperienceDescriptorBase
{
	GENERATED_BODY()

public:
	UKazanTerritoryExperienceDescriptor();

	virtual void GetAssetScanSpecs(TArray<FExperienceAssetScanSpec>& OutSpecs) const override;
};
