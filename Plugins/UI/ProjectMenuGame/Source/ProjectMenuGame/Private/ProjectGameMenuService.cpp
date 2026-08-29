// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectGameMenuService.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Subsystems/ProjectUILayerHostSubsystem.h"

namespace
{
	const FName GameMenuDefinitionId(TEXT("ProjectMenuGame.PauseMenu"));

	UProjectUILayerHostSubsystem* ResolveLayerHost(const APlayerController& PlayerController)
	{
		const UGameInstance* GameInstance = PlayerController.GetGameInstance();
		return GameInstance != nullptr
			? GameInstance->GetSubsystem<UProjectUILayerHostSubsystem>()
			: nullptr;
	}
}

bool FProjectGameMenuService::Toggle(APlayerController& PlayerController)
{
	return SetVisible(PlayerController, !IsVisible(PlayerController));
}

bool FProjectGameMenuService::SetVisible(APlayerController& PlayerController, bool bVisible)
{
	if (!PlayerController.IsLocalController())
	{
		return false;
	}

	UProjectUILayerHostSubsystem* LayerHost = ResolveLayerHost(PlayerController);
	if (LayerHost == nullptr)
	{
		return false;
	}

	if (bVisible)
	{
		if (IsVisible(PlayerController))
		{
			return true;
		}
		LayerHost->InitializeForPlayer(&PlayerController);
		UUserWidget* Menu = LayerHost->ShowDefinition(GameMenuDefinitionId);
		if (Menu == nullptr || !PlayerController.SetPause(true))
		{
			LayerHost->HideDefinition(GameMenuDefinitionId);
			return false;
		}
		Menu->SetKeyboardFocus();
		return true;
	}

	const bool bUnpaused = PlayerController.SetPause(false);
	LayerHost->HideDefinition(GameMenuDefinitionId);
	return bUnpaused && !IsVisible(PlayerController);
}

bool FProjectGameMenuService::IsVisible(const APlayerController& PlayerController) const
{
	const UProjectUILayerHostSubsystem* LayerHost = ResolveLayerHost(PlayerController);
	return LayerHost != nullptr && LayerHost->IsDefinitionVisible(GameMenuDefinitionId);
}
