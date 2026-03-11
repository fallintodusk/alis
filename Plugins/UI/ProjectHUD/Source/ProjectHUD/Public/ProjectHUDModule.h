// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * ProjectHUD Module
 *
 * Provides the HUD composition root (W_HUDLayout) that hosts
 * slot-based widgets from ProjectUI definitions.
 *
 * This module is intentionally thin - it just provides the layout widget.
 * ProjectUI registry/factory owns creation and injection.
 */
class FProjectHUDModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
