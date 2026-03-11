// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ActorFactories/ActorFactory.h"
#include "ProjectObjectActorFactory.generated.h"

class UObjectDefinition;

/**
 * Actor factory for placing world objects from UObjectDefinition assets.
 *
 * Thin wrapper around ProjectObjectSpawn::SpawnFromDefinition().
 * Adds editor-specific setup (undo/redo via Modify + RF_Transactional).
 *
 * Core spawn logic is in ObjectSpawnUtility (shared with runtime drops).
 */
UCLASS()
class PROJECTPLACEMENTEDITOR_API UProjectObjectActorFactory : public UActorFactory
{
	GENERATED_BODY()

public:
	UProjectObjectActorFactory();

	// UActorFactory interface
	virtual bool CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg) override;
	virtual AActor* SpawnActor(UObject* InAsset, ULevel* InLevel, const FTransform& InTransform, const FActorSpawnParameters& InSpawnParams) override;
	virtual UObject* GetAssetFromActorInstance(AActor* ActorInstance) override;
};
