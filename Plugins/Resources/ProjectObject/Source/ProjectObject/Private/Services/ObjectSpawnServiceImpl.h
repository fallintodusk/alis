// Copyright ALIS. All Rights Reserved.

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
};
