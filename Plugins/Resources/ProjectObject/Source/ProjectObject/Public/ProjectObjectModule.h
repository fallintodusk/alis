#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// Module-wide log category
DECLARE_LOG_CATEGORY_EXTERN(LogProjectObject, Log, All);

class FProjectObjectModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
