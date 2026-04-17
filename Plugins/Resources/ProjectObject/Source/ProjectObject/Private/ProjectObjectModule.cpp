#include "ProjectObjectModule.h"
#include "Services/ObjectSpawnServiceImpl.h"
#include "ProjectServiceLocator.h"
#include "Experience/GlobalAssetScanRegistry.h"

DEFINE_LOG_CATEGORY(LogProjectObject);

#define LOCTEXT_NAMESPACE "FProjectObjectModule"

namespace
{
	TSharedPtr<FObjectSpawnServiceImpl> GObjectSpawnService;
}

void FProjectObjectModule::StartupModule()
{
	GObjectSpawnService = MakeShared<FObjectSpawnServiceImpl>();
	FProjectServiceLocator::Register<IObjectSpawnService>(GObjectSpawnService);

	// Register global asset scan specs so cooked builds discover these types
	// via EnsureGlobalAssetScans() before any experience-specific scans run.
	// Matches DefaultGame.ini PrimaryAssetTypesToScan entries.
	{
		FExperienceAssetScanSpec ObjectDefSpec;
		ObjectDefSpec.PrimaryAssetType = TEXT("ObjectDefinition");
		ObjectDefSpec.Directories.Add(TEXT("/ProjectObject"));
		ObjectDefSpec.bForceSynchronousScan = true;
		ObjectDefSpec.bRequireNonEmpty = true;
		FGlobalAssetScanRegistry::Get().RegisterScanSpec(ObjectDefSpec);
	}
	{
		FExperienceAssetScanSpec LootProfileSpec;
		LootProfileSpec.PrimaryAssetType = TEXT("LootProfileDefinition");
		LootProfileSpec.Directories.Add(TEXT("/ProjectObject/LootProfiles"));
		LootProfileSpec.bForceSynchronousScan = true;
		LootProfileSpec.bRequireNonEmpty = true;
		FGlobalAssetScanRegistry::Get().RegisterScanSpec(LootProfileSpec);
	}
}

void FProjectObjectModule::ShutdownModule()
{
	FProjectServiceLocator::Unregister<IObjectSpawnService>();
	GObjectSpawnService.Reset();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProjectObjectModule, ProjectObject)
