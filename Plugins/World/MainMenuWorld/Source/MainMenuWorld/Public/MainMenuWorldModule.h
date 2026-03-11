#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FMainMenuWorldModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
