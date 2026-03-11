#pragma once

#include "CoreMinimal.h"
#include "Layout/ProjectUIScreenLayoutData.h"
#include "DA_LoadingScreenLayout.generated.h"

/**
 * Default loading screen layout wiring (C++ base asset).
 */
UCLASS(BlueprintType)
class PROJECTUI_API UDA_LoadingScreenLayout : public UProjectUIScreenLayoutData
{
	GENERATED_BODY()

public:
	UDA_LoadingScreenLayout();
};
