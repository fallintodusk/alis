#pragma once

#include "CoreMinimal.h"
#include "Animation/ProjectUITransitionPreset.h"
#include "DA_SlideTransition.generated.h"

/**
 * Default slide-up transition preset.
 */
UCLASS(BlueprintType)
class PROJECTUI_API UDA_SlideTransition : public UProjectUITransitionPreset
{
	GENERATED_BODY()

public:
	UDA_SlideTransition();
};
