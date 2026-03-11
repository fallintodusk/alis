// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProjectUIActionDescriptor.generated.h"

/**
 * Generic UI action descriptor used by feature UIs to render command rows/menus.
 * Domain logic decides visibility/enabled state; widgets only render this model.
 */
USTRUCT(BlueprintType)
struct PROJECTUI_API FProjectUIActionDescriptor
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "ProjectUI|Action")
	FName ActionId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "ProjectUI|Action")
	FText Label;

	UPROPERTY(BlueprintReadOnly, Category = "ProjectUI|Action")
	bool bVisible = false;

	UPROPERTY(BlueprintReadOnly, Category = "ProjectUI|Action")
	bool bEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "ProjectUI|Action")
	int32 Priority = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ProjectUI|Action")
	FName IconId = NAME_None;

	bool IsVisibleAndEnabled() const
	{
		return bVisible && bEnabled;
	}
};

