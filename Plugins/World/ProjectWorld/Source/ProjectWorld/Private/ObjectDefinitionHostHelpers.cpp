// Copyright ALIS. All Rights Reserved.

#include "ObjectDefinitionHostHelpers.h"
#include "Interfaces/IObjectDefinitionHost.h"
#include "Components/ObjectDefinitionHostComponent.h"
#include "ProjectWorldActor.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	UObject* FindHostObjectImpl(AActor* Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}

		if (Actor->Implements<UObjectDefinitionHostInterface>())
		{
			return Actor;
		}

		for (UActorComponent* Comp : Actor->GetComponents())
		{
			if (Comp && Comp->Implements<UObjectDefinitionHostInterface>())
			{
				return Comp;
			}
		}

		return nullptr;
	}
}

namespace ProjectWorldDefinitionHost
{
	UObject* FindHostObject(AActor* Actor)
	{
		return FindHostObjectImpl(Actor);
	}

	const UObject* FindHostObject(const AActor* Actor)
	{
		return FindHostObjectImpl(const_cast<AActor*>(Actor));
	}

	UObject* EnsureHostObject(AActor* Actor, bool bAllowInjectComponent)
	{
		if (!Actor)
		{
			return nullptr;
		}

		if (UObject* ExistingHost = FindHostObject(Actor))
		{
			return ExistingHost;
		}

		if (!bAllowInjectComponent)
		{
			return nullptr;
		}

		const FName UniqueComponentName = MakeUniqueObjectName(
			Actor,
			UObjectDefinitionHostComponent::StaticClass(),
			FName(TEXT("ObjectDefinitionHost")));

		UObjectDefinitionHostComponent* HostComponent = NewObject<UObjectDefinitionHostComponent>(
			Actor,
			UniqueComponentName);
		if (!HostComponent)
		{
			return nullptr;
		}

		Actor->AddInstanceComponent(HostComponent);
		HostComponent->RegisterComponent();
		return HostComponent;
	}

	bool ReadHostState(
		const AActor* Actor,
		FPrimaryAssetId& OutDefinitionId,
		FString& OutStructureHash,
		FString& OutContentHash)
	{
		// LEGACY_OBJECT_PARENT_GENERALIZATION(L004): AProjectWorldActor still owns definition metadata fields directly.
		// Read those fields first so placement/readback stays deterministic for AInteractableActor lineage.
		// Remove when L004 migration fully moves metadata ownership to host component/interface only.
		if (const AProjectWorldActor* ProjectWorldActor = Cast<AProjectWorldActor>(Actor))
		{
			OutDefinitionId = ProjectWorldActor->ObjectDefinitionId;
			OutStructureHash = ProjectWorldActor->AppliedStructureHash;
			OutContentHash = ProjectWorldActor->AppliedContentHash;
			return true;
		}

		const UObject* HostObject = FindHostObject(Actor);
		if (!HostObject || !HostObject->Implements<UObjectDefinitionHostInterface>())
		{
			return false;
		}

		OutDefinitionId = IObjectDefinitionHostInterface::Execute_GetHostedObjectDefinitionId(HostObject);
		OutStructureHash = IObjectDefinitionHostInterface::Execute_GetHostedAppliedStructureHash(HostObject);
		OutContentHash = IObjectDefinitionHostInterface::Execute_GetHostedAppliedContentHash(HostObject);
		return true;
	}

	bool WriteHostState(
		AActor* Actor,
		const FPrimaryAssetId& DefinitionId,
		const FString& StructureHash,
		const FString& ContentHash,
		bool bAllowInjectComponent)
	{
		// LEGACY_OBJECT_PARENT_GENERALIZATION(L004): AProjectWorldActor still owns definition metadata fields directly.
		// Write those fields first so spawn-path host write/read roundtrip cannot diverge during placement.
		// Remove when L004 migration fully moves metadata ownership to host component/interface only.
		if (AProjectWorldActor* ProjectWorldActor = Cast<AProjectWorldActor>(Actor))
		{
			ProjectWorldActor->ObjectDefinitionId = DefinitionId;
			ProjectWorldActor->AppliedStructureHash = StructureHash;
			ProjectWorldActor->AppliedContentHash = ContentHash;
			return true;
		}

		UObject* HostObject = EnsureHostObject(Actor, bAllowInjectComponent);
		if (!HostObject || !HostObject->Implements<UObjectDefinitionHostInterface>())
		{
			return false;
		}

		IObjectDefinitionHostInterface::Execute_SetHostedObjectDefinitionId(HostObject, DefinitionId);
		IObjectDefinitionHostInterface::Execute_SetHostedAppliedStructureHash(HostObject, StructureHash);
		IObjectDefinitionHostInterface::Execute_SetHostedAppliedContentHash(HostObject, ContentHash);
		return true;
	}

	FPrimaryAssetId GetDefinitionId(const AActor* Actor)
	{
		FPrimaryAssetId DefinitionId;
		FString StructureHash;
		FString ContentHash;
		if (ReadHostState(Actor, DefinitionId, StructureHash, ContentHash))
		{
			return DefinitionId;
		}

		return FPrimaryAssetId();
	}
}
