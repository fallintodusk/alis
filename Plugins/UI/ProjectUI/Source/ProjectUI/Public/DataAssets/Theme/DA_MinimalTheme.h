#pragma once

#include "CoreMinimal.h"
#include "Theme/ProjectUIThemeData.h"
#include "DA_MinimalTheme.generated.h"

/**
 * Neutral theme for prototyping and accessibility testing.
 */
UCLASS(BlueprintType)
class PROJECTUI_API UDA_MinimalTheme : public UProjectUIThemeData
{
	GENERATED_BODY()

public:
	UDA_MinimalTheme();
};
