// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Services/IObjectSpawnService.h"

/**
 * Implementation of IObjectSpawnService.
 * Wraps ProjectObjectSpawn::SpawnFromDefinition for service-based access.
 */
class FObjectSpawnServiceImpl : public IObjectSpawnService
{
public:
	virtual AActor* SpawnFromDefinition(
		UWorld* World,
		FPrimaryAssetId ObjectId,
		const FTransform& Transform,
		FText* OutError = nullptr) override;

	#if WITH_EDITOR
	virtual AActor* SpawnFromDefinitionWithIdentity(
		UWorld* World,
		FPrimaryAssetId ObjectId,
		const FTransform& Transform,
		FName ActorName,
		const FGuid& ActorGuid,
		FText* OutError = nullptr) override;
	#endif

	virtual bool GetDefinitionIdentity(
		FPrimaryAssetId ObjectId,
		FString& OutIdentity,
		FText* OutError = nullptr) override;
};
