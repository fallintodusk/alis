// Copyright ALIS. All Rights Reserved.

#include "CapabilityHierarchyHelpers.h"
#include "Interfaces/IInteractableTarget.h"

void CapabilityHierarchyHelpers::CollectAttachmentHierarchy(USceneComponent* HitNode, TSet<UPrimitiveComponent*>& OutMeshes)
{
	if (!HitNode)
	{
		return;
	}

	AActor* Owner = HitNode->GetOwner();

	// Add hit mesh if primitive
	if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(HitNode))
	{
		OutMeshes.Add(Prim);
	}

	// Traverse UP: add all parent primitives (within same actor)
	USceneComponent* Parent = HitNode->GetAttachParent();
	while (Parent && Parent->GetOwner() == Owner)
	{
		if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Parent))
		{
			OutMeshes.Add(Prim);
		}
		Parent = Parent->GetAttachParent();
	}

	// Traverse DOWN: add all descendants of hit mesh (NOT siblings)
	TArray<USceneComponent*> Descendants;
	HitNode->GetChildrenComponents(true, Descendants);
	for (USceneComponent* Child : Descendants)
	{
		if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Child))
		{
			OutMeshes.Add(Prim);
		}
	}
}

UPrimitiveComponent* CapabilityHierarchyHelpers::GetCapabilityTargetMesh(UActorComponent* Capability)
{
	if (!Capability || !Capability->Implements<UInteractableComponentTargetInterface>())
	{
		return nullptr;
	}

	return IInteractableComponentTargetInterface::Execute_GetInteractTargetMesh(Capability);
}

bool CapabilityHierarchyHelpers::CapabilityMatchesHierarchy(
	UActorComponent* Capability,
	const TSet<UPrimitiveComponent*>& HierarchyMeshes,
	bool& OutHasMeshProperty)
{
	OutHasMeshProperty = false;

	if (!Capability)
	{
		return false;
	}

	if (UPrimitiveComponent* TargetMesh = GetCapabilityTargetMesh(Capability))
	{
		OutHasMeshProperty = true;
		return HierarchyMeshes.Contains(TargetMesh);
	}
	return false;
}
