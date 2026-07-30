// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

PROJECTCOMBAT_API DECLARE_LOG_CATEGORY_EXTERN(LogProjectCombat, Log, All);

/**
 * Feature plugin providing combat mechanics.
 *
 * OnDemand plugin - uses standard IModuleInterface (no boot callbacks).
 * Boot callbacks (IProjectFeatureModule) are only for Boot plugins like ProjectMenuMain.
 */
class FProjectCombatModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
