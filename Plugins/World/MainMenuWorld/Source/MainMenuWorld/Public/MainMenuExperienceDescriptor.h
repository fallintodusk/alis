#pragma once

#include "CoreMinimal.h"
#include "Experience/ProjectExperienceDescriptorBase.h"
#include "MainMenuExperienceDescriptor.generated.h"

/**
 * Descriptor for the main menu experience.
 * - Lives with the world content so the load recipe (map + UI) stays alongside the assets.
 * - ProjectLoading uses this descriptor to build FLoadRequest and drive the 6-phase pipeline.
 * - ProjectMenuPlay (GameMode) runs after the load completes; it does not own the descriptor.
 */
UCLASS(BlueprintType)
class MAINMENUWORLD_API UMainMenuExperienceDescriptor : public UProjectExperienceDescriptorBase
{
	GENERATED_BODY()

public:
	UMainMenuExperienceDescriptor();

	virtual void BuildLoadRequest(FLoadRequest& OutRequest) const override; // Reserved for extra per-experience flags if needed
	virtual void GetAssetScanSpecs(TArray<FExperienceAssetScanSpec>& OutSpecs) const override;
};
