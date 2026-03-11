// Copyright ALIS. All Rights Reserved.

#include "ProjectCombat.h"
#include "FeatureRegistry.h"
#include "FeatureInitContext.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectCombat, Log, All);

void FProjectCombatModule::StartupModule()
{
	// REGISTRATION ONLY - no init happens here
	// GameMode will call this lambda later, in order, after pawn spawn
	// See: Plugins/Gameplay/ProjectFeature/README.md for design rationale
	FFeatureRegistry::RegisterFeature(TEXT("Combat"), [](const FFeatureInitContext& Context)
	{
		// NOW we init - GameMode called us with context (pawn exists)
		UE_LOG(LogProjectCombat, Log, TEXT("Combat feature initializing (Mode: %s)"),
			*Context.ModeName.ToString());

		// Feature owns what to attach to pawn:
		// APawn* Pawn = Context.Pawn;
		// UCombatComponent* Combat = NewObject<UCombatComponent>(Pawn);
		// Pawn->AddInstanceComponent(Combat);
		// Combat->RegisterComponent();

		// Feature owns how to configure:
		// if (!Context.ConfigJson.IsEmpty()) { /* parse config */ }

		// TODO: Implement combat feature init (attach components, etc.)
	});

	UE_LOG(LogProjectCombat, Log, TEXT("StartupModule() - Combat registered with FFeatureRegistry"));
}

void FProjectCombatModule::ShutdownModule()
{
	UE_LOG(LogProjectCombat, Log, TEXT("ShutdownModule()"));
}

IMPLEMENT_MODULE(FProjectCombatModule, ProjectCombat)
