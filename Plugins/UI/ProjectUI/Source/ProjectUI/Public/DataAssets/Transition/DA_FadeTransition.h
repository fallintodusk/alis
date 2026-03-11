#pragma once

#include "CoreMinimal.h"
#include "Animation/ProjectUITransitionPreset.h"
#include "DA_FadeTransition.generated.h"

/**
 * Default fade transition preset.
 */
UCLASS(BlueprintType)
class PROJECTUI_API UDA_FadeTransition : public UProjectUITransitionPreset
{
	GENERATED_BODY()

public:
	UDA_FadeTransition();
};
