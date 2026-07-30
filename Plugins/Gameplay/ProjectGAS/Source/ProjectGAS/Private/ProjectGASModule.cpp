// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectGASModule.h"
#include "Services/AttributeEffectServiceImpl.h"
#include "ProjectServiceLocator.h"

#define LOCTEXT_NAMESPACE "FProjectGASModule"

namespace
{
	TSharedPtr<FAttributeEffectServiceImpl> GAttributeEffectService;
}

void FProjectGASModule::StartupModule()
{
	GAttributeEffectService = MakeShared<FAttributeEffectServiceImpl>();
	FProjectServiceLocator::Register<IAttributeEffectService>(GAttributeEffectService);

	// No AbilitySet asset scan here. All current gameplay effects (GE_ThresholdDebuff_*,
	// GE_ConditionRegen, GE_GenericInstant, etc.) are C++ classes applied directly via
	// UProjectGASLibrary::ApplyMagnitudes and UProjectVitalsComponent, not BP data assets.
	// When DefinitionGenerator starts creating AbilitySet .uassets from item definitions
	// (GrantedAbilities/GrantedEffects), add the scan spec back with bRequireNonEmpty=true.
	// DefaultGame.ini still has the PrimaryAssetTypesToScan entry (CookRule=AlwaysCook)
	// so any authored assets will cook correctly without code changes.
}

void FProjectGASModule::ShutdownModule()
{
	FProjectServiceLocator::Unregister<IAttributeEffectService>();
	GAttributeEffectService.Reset();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProjectGASModule, ProjectGAS)
