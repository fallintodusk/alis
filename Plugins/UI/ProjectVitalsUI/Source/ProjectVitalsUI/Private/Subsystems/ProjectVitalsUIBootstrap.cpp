// Copyright ALIS. All Rights Reserved.

#include "Subsystems/ProjectVitalsUIBootstrap.h"
#include "ProjectGameplayTags.h"
#include "Subsystems/ProjectTagTextSubsystem.h"

#define LOCTEXT_NAMESPACE "VitalsUI"

DEFINE_LOG_CATEGORY_STATIC(LogVitalsUIBootstrap, Log, All);

void UProjectVitalsUIBootstrap::Initialize(FSubsystemCollectionBase& Collection)
{
	// CRITICAL: Ensure dependencies are initialized first
	Collection.InitializeDependency<UProjectTagTextSubsystem>();
	Super::Initialize(Collection);

	UE_LOG(LogVitalsUIBootstrap, Log, TEXT("Initialize: Registering VitalsUI tag texts"));

	// Register tag texts for vitals states
	RegisterTagTexts();
}

void UProjectVitalsUIBootstrap::RegisterTagTexts()
{
	UProjectTagTextSubsystem* TagTextSub = GetGameInstance()->GetSubsystem<UProjectTagTextSubsystem>();
	if (!TagTextSub)
	{
		UE_LOG(LogVitalsUIBootstrap, Warning, TEXT("RegisterTagTexts: ProjectTagTextSubsystem not found"));
		return;
	}

	// Condition states
	TagTextSub->RegisterOrReplace(ProjectTags::State_Condition_Low,
		LOCTEXT("State_Condition_Low", "Injured"));
	TagTextSub->RegisterOrReplace(ProjectTags::State_Condition_Critical,
		LOCTEXT("State_Condition_Critical", "Severely injured"));
	TagTextSub->RegisterOrReplace(ProjectTags::State_Condition_Empty,
		LOCTEXT("State_Condition_Empty", "Near death"));

	// Stamina states
	TagTextSub->RegisterOrReplace(ProjectTags::State_Stamina_Low,
		LOCTEXT("State_Stamina_Low", "Winded"));
	TagTextSub->RegisterOrReplace(ProjectTags::State_Stamina_Critical,
		LOCTEXT("State_Stamina_Critical", "Exhausted"));
	TagTextSub->RegisterOrReplace(ProjectTags::State_Stamina_Empty,
		LOCTEXT("State_Stamina_Empty", "Out of breath"));

	// Calories states (hunger)
	TagTextSub->RegisterOrReplace(ProjectTags::State_Calories_Low,
		LOCTEXT("State_Calories_Low", "Hungry - stamina regen reduced"));
	TagTextSub->RegisterOrReplace(ProjectTags::State_Calories_Critical,
		LOCTEXT("State_Calories_Critical", "Starving - no health regen"));
	TagTextSub->RegisterOrReplace(ProjectTags::State_Calories_Empty,
		LOCTEXT("State_Calories_Empty", "Starving - health draining"));

	// Hydration states (thirst)
	TagTextSub->RegisterOrReplace(ProjectTags::State_Hydration_Low,
		LOCTEXT("State_Hydration_Low", "Thirsty - max stamina reduced"));
	TagTextSub->RegisterOrReplace(ProjectTags::State_Hydration_Critical,
		LOCTEXT("State_Hydration_Critical", "Dehydrated - no health regen"));
	TagTextSub->RegisterOrReplace(ProjectTags::State_Hydration_Empty,
		LOCTEXT("State_Hydration_Empty", "Dehydrated - health draining"));

	// Fatigue states (inverted: higher = worse)
	TagTextSub->RegisterOrReplace(ProjectTags::State_Fatigue_Tired,
		LOCTEXT("State_Fatigue_Tired", "Tired"));
	TagTextSub->RegisterOrReplace(ProjectTags::State_Fatigue_Exhausted,
		LOCTEXT("State_Fatigue_Exhausted", "Exhausted - stamina regen reduced"));
	TagTextSub->RegisterOrReplace(ProjectTags::State_Fatigue_Critical,
		LOCTEXT("State_Fatigue_Critical", "Collapse imminent"));

	UE_LOG(LogVitalsUIBootstrap, Log, TEXT("RegisterTagTexts: Registered vitals state texts"));
}

void UProjectVitalsUIBootstrap::Deinitialize()
{
	UE_LOG(LogVitalsUIBootstrap, Log, TEXT("Deinitialize: VitalsUI bootstrap shutdown"));

	Super::Deinitialize();
}

#undef LOCTEXT_NAMESPACE
