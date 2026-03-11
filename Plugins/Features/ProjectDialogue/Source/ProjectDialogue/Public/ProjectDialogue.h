// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Containers/Ticker.h"

class FDialogueServiceImpl;
class FDialogueInteractionHandler;

/**
 * Feature plugin providing universal dialogue mechanics.
 * Data-driven: dialogue trees defined in JSON, auto-generated to UAssets.
 * Works on any actor (NPCs, objects, terminals).
 */
class FProjectDialogueModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	bool TrySubscribeInteraction(float DeltaTime);

	TSharedPtr<FDialogueServiceImpl> ServiceImpl;
	TSharedPtr<FDialogueInteractionHandler> InteractionHandler;
	FTSTicker::FDelegateHandle RetryHandle;
};
