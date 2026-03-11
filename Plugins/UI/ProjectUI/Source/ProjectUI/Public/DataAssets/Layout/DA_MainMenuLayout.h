#pragma once

#include "CoreMinimal.h"
#include "Layout/ProjectUIScreenLayoutData.h"
#include "DA_MainMenuLayout.generated.h"

/**
 * Default main menu layout wiring (C++ base asset).
 */
UCLASS(BlueprintType)
class PROJECTUI_API UDA_MainMenuLayout : public UProjectUIScreenLayoutData
{
	GENERATED_BODY()

public:
	UDA_MainMenuLayout();
};
