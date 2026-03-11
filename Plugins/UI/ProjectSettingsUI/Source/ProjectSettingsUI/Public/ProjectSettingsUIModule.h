#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class UProjectSettingsRootWidget;

class PROJECTSETTINGSUI_API FProjectSettingsUIModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Create the root settings widget (owned by caller). Returns nullptr on failure. */
	virtual class UUserWidget* CreateSettingsRoot(UObject* Outer);
};
