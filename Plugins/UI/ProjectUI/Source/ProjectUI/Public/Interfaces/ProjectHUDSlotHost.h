// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "ProjectHUDSlotHost.generated.h"

/**
 * Interface for HUD layout widgets that host named slots.
 * Implemented by ProjectHUD's W_HUDLayout.
 */
UINTERFACE(MinimalAPI)
class UProjectHUDSlotHost : public UInterface
{
	GENERATED_BODY()
};

class PROJECTUI_API IProjectHUDSlotHost
{
	GENERATED_BODY()

public:
	/** Rebuild the given HUD slot. */
	virtual void RefreshSlot(FGameplayTag SlotTag) = 0;
};
