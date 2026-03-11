#pragma once

#include "Modules/ModuleManager.h"

class FProjectCoreEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
