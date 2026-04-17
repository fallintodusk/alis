// Copyright ALIS. All Rights Reserved.

#include "ProjectGASModule.h"
#include "Services/AttributeEffectServiceImpl.h"
#include "ProjectServiceLocator.h"
#include "Experience/GlobalAssetScanRegistry.h"

#define LOCTEXT_NAMESPACE "FProjectGASModule"

namespace
{
	TSharedPtr<FAttributeEffectServiceImpl> GAttributeEffectService;
}

void FProjectGASModule::StartupModule()
{
	GAttributeEffectService = MakeShared<FAttributeEffectServiceImpl>();
	FProjectServiceLocator::Register<IAttributeEffectService>(GAttributeEffectService);

	// Register global asset scan spec so cooked builds discover ability sets
	// via EnsureGlobalAssetScans(). Matches DefaultGame.ini entry.
	{
		FExperienceAssetScanSpec AbilitySetSpec;
		AbilitySetSpec.PrimaryAssetType = TEXT("ProjectAbilitySet");
		AbilitySetSpec.Directories.Add(TEXT("/ProjectGAS/AbilitySets"));
		AbilitySetSpec.bForceSynchronousScan = true;
		AbilitySetSpec.bRequireNonEmpty = true;
		FGlobalAssetScanRegistry::Get().RegisterScanSpec(AbilitySetSpec);
	}
}

void FProjectGASModule::ShutdownModule()
{
	FProjectServiceLocator::Unregister<IAttributeEffectService>();
	GAttributeEffectService.Reset();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProjectGASModule, ProjectGAS)
