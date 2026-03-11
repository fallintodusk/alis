// Copyright ALIS. All Rights Reserved.

#include "Services/ObjectSpawnServiceImpl.h"
#include "Spawning/ObjectSpawnUtility.h"

AActor* FObjectSpawnServiceImpl::SpawnFromDefinition(
	UWorld* World,
	FPrimaryAssetId ObjectId,
	const FTransform& Transform,
	FText* OutError)
{
	return ProjectObjectSpawn::SpawnFromDefinition(World, ObjectId, Transform, FActorSpawnParameters(), OutError);
}
