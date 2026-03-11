// Copyright ALIS. All Rights Reserved.

#include "ProjectCore.h"
#include "ProjectLogging.h"
#include "ProjectServiceLocator.h"

#define LOCTEXT_NAMESPACE "FProjectCoreModule"

void FProjectCoreModule::StartupModule()
{
	// This code will execute after your module is loaded into memory;
	// the exact timing is specified in the .uplugin file per-module

	FProjectServiceLocator::Reset();
	UE_LOG(LogProjectCore, Log, TEXT("ProjectCore: Module startup"));
}

void FProjectCoreModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.
	// For modules that support dynamic reloading, we call this function before unloading the module.

	FProjectServiceLocator::Reset();
	UE_LOG(LogProjectCore, Log, TEXT("ProjectCore: Module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProjectCoreModule, ProjectCore)
