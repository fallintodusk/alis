#include "ProjectInteractionModule.h"
#include "InteractionService.h"
#include "InteractionComponent.h"
#include "ProjectServiceLocator.h"
#include "FeatureRegistry.h"
#include "GameFramework/Pawn.h"

#define LOCTEXT_NAMESPACE "FProjectInteractionModule"

DEFINE_LOG_CATEGORY_STATIC(LogProjectInteraction, Log, All);
DEFINE_LOG_CATEGORY(LogInteractionService);

void FProjectInteractionModule::StartupModule()
{
	UE_LOG(LogProjectInteraction, Log, TEXT("StartupModule: Initializing ProjectInteraction..."));

	// Create and register interaction service
	TSharedRef<IInteractionService> Service = MakeShared<FInteractionService>();
	FProjectServiceLocator::Register<IInteractionService>(Service);

	UE_LOG(LogProjectInteraction, Log, TEXT("StartupModule: Registered IInteractionService in ServiceLocator"));

	// Register with FFeatureRegistry for GameMode-driven initialization
	FFeatureRegistry::RegisterFeature(FName(TEXT("Interaction")), [](const FFeatureInitContext& Context)
	{
		if (!Context.Pawn)
		{
			UE_LOG(LogProjectInteraction, Warning, TEXT("Feature init: No pawn provided, skipping InteractionComponent"));
			return;
		}

		// Check if already has component
		if (Context.Pawn->FindComponentByClass<UInteractionComponent>())
		{
			UE_LOG(LogProjectInteraction, Verbose, TEXT("Feature init: Pawn '%s' already has InteractionComponent"),
				*Context.Pawn->GetName());
			return;
		}

		// Create and attach component
		UInteractionComponent* Comp = NewObject<UInteractionComponent>(Context.Pawn, TEXT("InteractionComponent"));
		if (Comp)
		{
			Comp->RegisterComponent();
			UE_LOG(LogProjectInteraction, Log, TEXT("Feature init: Added InteractionComponent to pawn '%s'"),
				*Context.Pawn->GetName());
		}
	});

	UE_LOG(LogProjectInteraction, Log, TEXT("StartupModule: Registered 'Interaction' feature with FFeatureRegistry"));
}

void FProjectInteractionModule::ShutdownModule()
{
	UE_LOG(LogProjectInteraction, Log, TEXT("ShutdownModule: Shutting down ProjectInteraction..."));

	// Check if registered before unregister
	const bool bWasRegistered = FProjectServiceLocator::IsRegistered<IInteractionService>();

	FProjectServiceLocator::Unregister<IInteractionService>();

	UE_LOG(LogProjectInteraction, Log, TEXT("ShutdownModule: Unregistered IInteractionService (WasRegistered=%s)"),
		bWasRegistered ? TEXT("true") : TEXT("false"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProjectInteractionModule, ProjectInteraction)
