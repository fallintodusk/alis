// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"

/**
 * Log category owned by the ProjectCinematicEditor module. Define lives in
 * ProjectCinematicSubsystem.cpp. Forward-declared here in the module's public
 * header so every TU in this module sees a single complete struct definition,
 * which avoids the unity-build "use of undefined type" pattern that hits when
 * DECLARE_LOG_CATEGORY_EXTERN is repeated in multiple .cpp files.
 */
DECLARE_LOG_CATEGORY_EXTERN(LogProjectCinematicEditor, Log, All);

class FProjectCinematicEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
