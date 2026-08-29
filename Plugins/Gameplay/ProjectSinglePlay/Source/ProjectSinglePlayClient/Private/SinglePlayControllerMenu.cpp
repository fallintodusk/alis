// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "SinglePlayController.h"

#include "InputActionValue.h"
#include "Interfaces/IGameMenuService.h"
#include "ProjectServiceLocator.h"
#include "ProjectSinglePlayLog.h"

void ASinglePlayController::HandleToggleGameMenuAction(const FInputActionValue& Value)
{
	if (!Value.Get<bool>() || !IsLocalController())
	{
		return;
	}

	const TSharedPtr<IGameMenuService> MenuService = FProjectServiceLocator::Resolve<IGameMenuService>();
	if (!MenuService.IsValid())
	{
		UE_LOG(LogProjectSinglePlay, Error, TEXT("Escape cannot resolve IGameMenuService."));
		return;
	}

	if (!MenuService->Toggle(*this))
	{
		UE_LOG(LogProjectSinglePlay, Error, TEXT("Escape failed to toggle the game menu."));
	}
}
