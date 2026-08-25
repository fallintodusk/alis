// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Services/ObjectSpawnServiceImpl.h"
#include "Data/ObjectDefinition.h"
#include "Spawning/ObjectSpawnUtility.h"

#include "Engine/AssetManager.h"

AActor* FObjectSpawnServiceImpl::SpawnFromDefinition(
	UWorld* World,
	FPrimaryAssetId ObjectId,
	const FTransform& Transform,
	FText* OutError)
{
	return ProjectObjectSpawn::SpawnFromDefinition(World, ObjectId, Transform, FActorSpawnParameters(), OutError);
}

#if WITH_EDITOR
AActor* FObjectSpawnServiceImpl::SpawnFromDefinitionWithIdentity(
	UWorld* World,
	FPrimaryAssetId ObjectId,
	const FTransform& Transform,
	FName ActorName,
	const FGuid& ActorGuid,
	FText* OutError)
{
	FActorSpawnParameters Parameters;
	Parameters.Name = ActorName;
	Parameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Required_ErrorAndReturnNull;
	Parameters.OverrideActorGuid = ActorGuid;
	return ProjectObjectSpawn::SpawnFromDefinition(World, ObjectId, Transform, Parameters, OutError);
}
#endif

bool FObjectSpawnServiceImpl::GetDefinitionIdentity(
	FPrimaryAssetId ObjectId,
	FString& OutIdentity,
	FText* OutError)
{
	UAssetManager& AssetManager = UAssetManager::Get();
	UObjectDefinition* Definition = AssetManager.GetPrimaryAssetObject<UObjectDefinition>(ObjectId);
	if (Definition == nullptr)
	{
		Definition = Cast<UObjectDefinition>(
			AssetManager.GetStreamableManager().LoadSynchronous(AssetManager.GetPrimaryAssetPath(ObjectId), false));
	}
	if (Definition == nullptr || Definition->DefinitionStructureHash.IsEmpty() || Definition->DefinitionContentHash.IsEmpty())
	{
		if (OutError != nullptr)
		{
			*OutError = FText::FromString(TEXT("ObjectDefinition identity is unavailable: ") + ObjectId.ToString());
		}
		return false;
	}
	OutIdentity = Definition->DefinitionStructureHash + TEXT("|") + Definition->DefinitionContentHash;
	return true;
}
