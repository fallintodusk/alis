// Copyright ALIS. All Rights Reserved.

#include "Components/ObjectDefinitionHostComponent.h"
#include "Net/UnrealNetwork.h"

UObjectDefinitionHostComponent::UObjectDefinitionHostComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

FPrimaryAssetId UObjectDefinitionHostComponent::GetHostedObjectDefinitionId_Implementation() const
{
	return ObjectDefinitionId;
}

void UObjectDefinitionHostComponent::SetHostedObjectDefinitionId_Implementation(const FPrimaryAssetId& DefinitionId)
{
	ObjectDefinitionId = DefinitionId;
}

FString UObjectDefinitionHostComponent::GetHostedAppliedStructureHash_Implementation() const
{
	return AppliedStructureHash;
}

void UObjectDefinitionHostComponent::SetHostedAppliedStructureHash_Implementation(const FString& StructureHash)
{
	AppliedStructureHash = StructureHash;
}

FString UObjectDefinitionHostComponent::GetHostedAppliedContentHash_Implementation() const
{
	return AppliedContentHash;
}

void UObjectDefinitionHostComponent::SetHostedAppliedContentHash_Implementation(const FString& ContentHash)
{
	AppliedContentHash = ContentHash;
}

FPrimaryAssetId UObjectDefinitionHostComponent::GetObjectDefinitionId_Implementation() const
{
	return ObjectDefinitionId;
}

void UObjectDefinitionHostComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UObjectDefinitionHostComponent, ObjectDefinitionId);
	DOREPLIFETIME(UObjectDefinitionHostComponent, AppliedStructureHash);
	DOREPLIFETIME(UObjectDefinitionHostComponent, AppliedContentHash);
}
