// License terms: see repository root LICENSE.

#include "Alis.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogAlisModule, Log, All);

/**
 * Application module: composition and bootstrap only.
 *
 * Concrete experiences (territories, showcases) are configured records owned by
 * ProjectExperienceData and discovered by ProjectLoading. The application module holds
 * no experience identity, map path, or traversal policy, so adding a reconstructed
 * territory never edits this file.
 */
class FAlisModule final : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();
		UE_LOG(LogAlisModule, Log, TEXT("[FAlisModule::StartupModule] Application module ready"));
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FAlisModule, Alis, "Alis");
