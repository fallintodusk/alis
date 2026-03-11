#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FProjectSinglePlayClientModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static inline FProjectSinglePlayClientModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FProjectSinglePlayClientModule>("ProjectSinglePlayClient");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("ProjectSinglePlayClient");
	}
};
