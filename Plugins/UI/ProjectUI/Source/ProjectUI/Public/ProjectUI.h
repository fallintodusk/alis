// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * ProjectUI Module
 *
 * Core UI foundation providing:
 * - MVVM architecture (ViewModel base classes, property binding)
 * - Theme system (colors, fonts, spacing)
 * - Reusable widget library (buttons, panels, progress bars)
 * - Animation system (transitions, effects)
 *
 * This module is UI framework-agnostic at the C++ level.
 * UMG integration is optional and handled in widget implementations.
 */
class FProjectUIModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	 * Get absolute path to plugin fonts directory.
	 * Fonts are stored in: Plugins/UI/ProjectUI/Content/Slate/Fonts/
	 */
	static const FString& GetFontsDir();

private:
	static FString FontsDir;
};
