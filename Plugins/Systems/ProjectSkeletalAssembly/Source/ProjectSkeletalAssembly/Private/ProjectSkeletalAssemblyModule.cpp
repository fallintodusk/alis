// Copyright ALIS. All Rights Reserved.

#include "ProjectSkeletalAssemblyModule.h"

DEFINE_LOG_CATEGORY(LogSkeletalAssembly);

#define LOCTEXT_NAMESPACE "FProjectSkeletalAssemblyModule"

void FProjectSkeletalAssemblyModule::StartupModule()
{
	UE_LOG(LogSkeletalAssembly, Log, TEXT("ProjectSkeletalAssembly module started."));
}

void FProjectSkeletalAssemblyModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProjectSkeletalAssemblyModule, ProjectSkeletalAssembly)
