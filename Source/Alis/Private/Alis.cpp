// License terms: see repository root LICENSE.

#include "Alis.h"
#include "Experience/ProjectExperienceRegistration.h"
#include "KazanTerritoryExperienceDescriptor.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogAlisModule, Log, All);

class FAlisModule final : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();
		ProjectExperience::RegisterDescriptor<UKazanTerritoryExperienceDescriptor>();
		UE_LOG(LogAlisModule, Log, TEXT("[FAlisModule::StartupModule] Registered experience - KazanTerritory"));
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FAlisModule, Alis, "Alis");
