// Copyright ALIS. All Rights Reserved.

#include "ProjectDialogue.h"
#include "FeatureRegistry.h"
#include "FeatureInitContext.h"
#include "ProjectServiceLocator.h"
#include "Interfaces/IDialogueService.h"
#include "Services/DialogueServiceImpl.h"
#include "Interaction/DialogueInteractionHandler.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectDialogue, Log, All);

#define LOCTEXT_NAMESPACE "FProjectDialogueModule"

void FProjectDialogueModule::StartupModule()
{
	// Create and register dialogue service
	ServiceImpl = MakeShared<FDialogueServiceImpl>();
	FProjectServiceLocator::Register<IDialogueService>(ServiceImpl);

	// Create interaction handler
	InteractionHandler = MakeShared<FDialogueInteractionHandler>();
	InteractionHandler->SetService(ServiceImpl);

	// Try subscribing immediately; retry via ticker if IInteractionService not yet available
	// (OnDemand plugins may load after OnPostEngineInit has already fired)
	if (TrySubscribeInteraction(0.f))
	{
		RetryHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateRaw(this, &FProjectDialogueModule::TrySubscribeInteraction), 0.5f);
	}

	// Feature registry (for orchestrator-based loading)
	FFeatureRegistry::RegisterFeature(TEXT("Dialogue"), [](const FFeatureInitContext& Context)
	{
		UE_LOG(LogProjectDialogue, Log, TEXT("Dialogue feature initialized (Mode: %s)"),
			*Context.ModeName.ToString());
	});

	UE_LOG(LogProjectDialogue, Log, TEXT("StartupModule() - Dialogue service registered"));
}

bool FProjectDialogueModule::TrySubscribeInteraction(float /*DeltaTime*/)
{
	if (!InteractionHandler.IsValid())
	{
		return false;
	}

	InteractionHandler->Subscribe();

	// Subscribe returns silently if service not available - check if it actually bound
	if (!InteractionHandler->IsSubscribed())
	{
		return true; // keep ticking
	}

	UE_LOG(LogProjectDialogue, Log, TEXT("Interaction handler subscribed"));
	RetryHandle.Reset();
	return false; // stop ticker
}

void FProjectDialogueModule::ShutdownModule()
{
	if (RetryHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(RetryHandle);
		RetryHandle.Reset();
	}

	if (InteractionHandler.IsValid())
	{
		InteractionHandler->Unsubscribe();
		InteractionHandler.Reset();
	}

	FProjectServiceLocator::Unregister<IDialogueService>();
	ServiceImpl.Reset();

	UE_LOG(LogProjectDialogue, Log, TEXT("ShutdownModule()"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProjectDialogueModule, ProjectDialogue)
