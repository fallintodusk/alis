// Copyright ALIS. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FProjectLoadingMoviePlayerModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
