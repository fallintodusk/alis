// Copyright ALIS. All Rights Reserved.

#include "ProjectSkeletalCapabilitiesModule.h"
#include "CapabilityRegistry.h"

DEFINE_LOG_CATEGORY(LogProjectSkeletalCapabilities);

#define LOCTEXT_NAMESPACE "FProjectSkeletalCapabilitiesModule"

void FProjectSkeletalCapabilitiesModule::StartupModule()
{
	// Self-register so FCapabilityRegistry discovers capability classes in this module.
	// External plugins use RegisterCapabilityModule() -- core always-enabled plugins
	// are listed in the registry scan config, but this plugin ships third-party deps
	// (Mutable, PoseSearch) and is therefore external to the generic registry.
	FCapabilityRegistry::RegisterCapabilityModule(TEXT("ProjectSkeletalCapabilities"));
}

void FProjectSkeletalCapabilitiesModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProjectSkeletalCapabilitiesModule, ProjectSkeletalCapabilities)
