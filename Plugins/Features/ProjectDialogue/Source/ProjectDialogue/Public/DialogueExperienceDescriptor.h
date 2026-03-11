#pragma once

#include "CoreMinimal.h"
#include "Experience/ProjectExperienceDescriptorBase.h"
#include "DialogueExperienceDescriptor.generated.h"

/**
 * Descriptor for the dialogue feature (OnDemand content).
 * Keeps the load recipe (maps/UI/data) with the feature plugin.
 */
UCLASS(BlueprintType)
class PROJECTDIALOGUE_API UDialogueExperienceDescriptor : public UProjectExperienceDescriptorBase
{
	GENERATED_BODY()

public:
	UDialogueExperienceDescriptor();

	virtual void BuildLoadRequest(FLoadRequest& OutRequest) const override; // Reserved for per-feature flags
};
