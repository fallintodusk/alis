// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectObjectCapabilitiesModule.h"

DEFINE_LOG_CATEGORY(LogProjectObjectCapabilities);

#define LOCTEXT_NAMESPACE "FProjectObjectCapabilitiesModule"

void FProjectObjectCapabilitiesModule::StartupModule()
{
}

void FProjectObjectCapabilitiesModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProjectObjectCapabilitiesModule, ProjectObjectCapabilities)
