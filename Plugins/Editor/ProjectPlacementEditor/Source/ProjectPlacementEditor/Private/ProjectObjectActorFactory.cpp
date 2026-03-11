// Copyright ALIS. All Rights Reserved.

#include "ProjectObjectActorFactory.h"
#include "Data/ObjectDefinition.h"
#include "ObjectDefinitionHostHelpers.h"
#include "Template/Interactable/InteractableActor.h"
#include "Spawning/ObjectSpawnUtility.h"
#include "ProjectPlacementEditor.h"
#include "Engine/AssetManager.h"
#include "ActorEditorUtils.h"

UProjectObjectActorFactory::UProjectObjectActorFactory()
{
	DisplayName = NSLOCTEXT("ProjectPlacement", "ObjectActorFactoryName", "Project Object");
	NewActorClass = AInteractableActor::StaticClass();
	bUseSurfaceOrientation = true;
}

bool UProjectObjectActorFactory::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	if (AssetData.IsValid() && AssetData.GetClass()->IsChildOf(UObjectDefinition::StaticClass()))
	{
		return true;
	}

	OutErrorMsg = NSLOCTEXT("ProjectPlacement", "CannotCreateObject", "Asset must be an ObjectDefinition");
	return false;
}

AActor* UProjectObjectActorFactory::SpawnActor(UObject* InAsset, ULevel* InLevel, const FTransform& InTransform, const FActorSpawnParameters& InSpawnParams)
{
	UObjectDefinition* Def = Cast<UObjectDefinition>(InAsset);
	if (!Def)
	{
		return nullptr;
	}

	// Set up spawn params with editor-specific level override
	FActorSpawnParameters SpawnParams = InSpawnParams;
	SpawnParams.OverrideLevel = InLevel;

	// Use shared spawn utility (runtime + editor)
	FText SpawnError;
	AActor* SpawnedActor = ProjectObjectSpawn::SpawnFromDefinition(
		InLevel->GetWorld(),
		Def,
		InTransform,
		SpawnParams,
		&SpawnError);

	if (!SpawnedActor)
	{
		UE_LOG(LogProjectPlacementEditor, Error, TEXT("SpawnActor failed: %s"), *SpawnError.ToString());
		return nullptr;
	}

	// Apply editor-specific setup (undo/redo support)
	SpawnedActor->Modify();

	// Apply RF_Transactional to all instance components for undo/redo
	for (UActorComponent* Comp : SpawnedActor->GetInstanceComponents())
	{
		if (Comp)
		{
			Comp->SetFlags(RF_Transactional);
		}
	}

	// Set actor label from definition ID with unique suffix (editor-only, for outliner display)
	// Uses FActorLabelUtilities to auto-add _1, _2 suffixes when label already exists
	FActorLabelUtilities::SetActorLabelUnique(SpawnedActor, Def->ObjectId.ToString());

	return SpawnedActor;
}

UObject* UProjectObjectActorFactory::GetAssetFromActorInstance(AActor* ActorInstance)
{
	// Return the ObjectDefinition that was used to spawn this actor
	// Enables: Content Browser "Find in Content Browser" for placed objects
	if (ActorInstance)
	{
		const FPrimaryAssetId DefinitionId = ProjectWorldDefinitionHost::GetDefinitionId(ActorInstance);
		if (DefinitionId.IsValid())
		{
			// Load the definition asset synchronously (editor-only)
			if (UAssetManager* AssetManager = UAssetManager::GetIfInitialized())
			{
				FSoftObjectPath AssetPath = AssetManager->GetPrimaryAssetPath(DefinitionId);
				return AssetPath.TryLoad();
			}
		}
	}
	return nullptr;
}
