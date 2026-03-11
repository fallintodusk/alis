#pragma once

#include "CoreMinimal.h"
#include "Theme/ProjectUIThemeData.h"
#include "DA_DarkTheme.generated.h"

/**
 * High contrast dark theme for cinematic sessions.
 */
UCLASS(BlueprintType)
class PROJECTUI_API UDA_DarkTheme : public UProjectUIThemeData
{
	GENERATED_BODY()

public:
	UDA_DarkTheme();
};
