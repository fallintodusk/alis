// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectSkeletalCapabilitiesModule.h"
#include "Registry/RegisteredClassProviderRegistry.h"

DEFINE_LOG_CATEGORY(LogProjectSkeletalCapabilities);

#define LOCTEXT_NAMESPACE "FProjectSkeletalCapabilitiesModule"

void FProjectSkeletalCapabilitiesModule::StartupModule()
{
	FRegisteredClassProviderRegistry::RegisterProviderModule(
		FPrimaryAssetType(TEXT("CapabilityComponent")),
		TEXT("ProjectSkeletalCapabilities"));
}

void FProjectSkeletalCapabilitiesModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProjectSkeletalCapabilitiesModule, ProjectSkeletalCapabilities)
