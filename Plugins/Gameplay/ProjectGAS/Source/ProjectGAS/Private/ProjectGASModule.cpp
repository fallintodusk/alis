// Copyright ALIS. All Rights Reserved.

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
}

void FProjectGASModule::ShutdownModule()
{
	FProjectServiceLocator::Unregister<IAttributeEffectService>();
	GAttributeEffectService.Reset();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProjectGASModule, ProjectGAS)
