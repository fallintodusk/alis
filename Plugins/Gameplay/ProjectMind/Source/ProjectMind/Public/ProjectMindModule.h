#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FMindServiceImpl;
class FDefaultInventoryMindThoughtSource;
class FDefaultVitalsMindThoughtSource;
class FNullQuestMindThoughtSource;

class FProjectMindModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedPtr<FMindServiceImpl> MindService;
	TSharedPtr<FDefaultVitalsMindThoughtSource> DefaultVitalsThoughtSource;
	TSharedPtr<FDefaultInventoryMindThoughtSource> DefaultInventoryThoughtSource;
	TSharedPtr<FNullQuestMindThoughtSource> DefaultQuestThoughtSource;
};
