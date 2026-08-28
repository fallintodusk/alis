// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class APlayerController;
class UInventoryViewModel;

class FSinglePlayScenarioInventoryInput final
{
public:
	static bool TickUseItem(
		FName ItemName,
		UInventoryViewModel& ViewModel,
		APlayerController& Controller,
		bool& bContextOpened,
		bool& bUseRequested,
		int32& InOutPointerEventCount,
		FString& OutError);
};
