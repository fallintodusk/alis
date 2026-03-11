// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

PROJECTINVENTORY_API DECLARE_LOG_CATEGORY_EXTERN(LogProjectInventory, Log, All);

class FInventoryInteractionHandler;

/**
 * ProjectInventory module - provides inventory mechanics and pickup/loot handling.
 *
 * Responsibilities:
 * - Register "Inventory" feature with FFeatureRegistry
 * - Subscribe to IInteractionService for pickup/loot handling
 * - Provide UProjectInventoryComponent with rules (CanFitItems, AddItemsBatch, etc.)
 */
class FProjectInventoryModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** Handles pickup and loot container interactions. */
	TSharedPtr<FInventoryInteractionHandler> InteractionHandler;
};
