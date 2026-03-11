#pragma once

#include "CoreMinimal.h"
#include "Theme/ProjectUIThemeData.h"
#include "DA_DefaultTheme.generated.h"

/**
 * Default development theme (blue/grey palette).
 */
UCLASS(BlueprintType)
class PROJECTUI_API UDA_DefaultTheme : public UProjectUIThemeData
{
	GENERATED_BODY()

public:
	UDA_DefaultTheme();
};
