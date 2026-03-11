#pragma once

#include "Modules/ModuleManager.h"

class PROJECTLOADING_API FProjectLoadingModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
