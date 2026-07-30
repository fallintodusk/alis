// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"

/**
 * Log category owned by the ProjectCinematic runtime module. The editor
 * module ProjectCinematicEditor has its own LogProjectCinematicEditor so
 * the two TUs never collide under adaptive unity builds.
 *
 * Defined in ProjectCinematicModule.cpp via DEFINE_LOG_CATEGORY. Used only
 * within this module (CinematicGameMode.cpp); no cross-module API export
 * needed.
 */
DECLARE_LOG_CATEGORY_EXTERN(LogProjectCinematic, Log, All);

class FProjectCinematicModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
