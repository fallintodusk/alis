#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FProjectSinglePlayModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static inline FProjectSinglePlayModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FProjectSinglePlayModule>("ProjectSinglePlay");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("ProjectSinglePlay");
	}
};
