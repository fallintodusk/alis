// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "InteractionCapabilitySelector.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"

void FInteractionCapabilitySelector::CollectAttachmentHierarchy(
	USceneComponent* HitNode,
	TSet<UPrimitiveComponent*>& OutMeshes)
{
	if (!HitNode)
	{
		return;
	}

	AActor* Owner = HitNode->GetOwner();
	if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(HitNode))
	{
		OutMeshes.Add(Prim);
	}

	USceneComponent* Parent = HitNode->GetAttachParent();
	while (Parent && Parent->GetOwner() == Owner)
	{
		if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Parent))
		{
			OutMeshes.Add(Prim);
		}
		Parent = Parent->GetAttachParent();
	}

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

UPrimitiveComponent* FInteractionCapabilitySelector::GetTargetMesh(UActorComponent* Capability)
{
	if (!Capability || !Capability->Implements<UInteractableComponentTargetInterface>())
	{
		return nullptr;
	}

	return IInteractableComponentTargetInterface::Execute_GetInteractTargetMesh(Capability);
}

bool FInteractionCapabilitySelector::CapabilityMatchesHierarchy(
	UActorComponent* Capability,
	const TSet<UPrimitiveComponent*>& HierarchyMeshes,
	bool& OutHasMeshProperty)
{
	OutHasMeshProperty = false;
	if (!Capability)
	{
		return false;
	}

	if (UPrimitiveComponent* TargetMesh = GetTargetMesh(Capability))
	{
		OutHasMeshProperty = true;
		return HierarchyMeshes.Contains(TargetMesh);
	}

	return false;
}

void FInteractionCapabilitySelector::GatherComponents(AActor* Target, TArray<UActorComponent*>& OutComponents)
{
	OutComponents.Reset();
	if (!Target)
	{
		return;
	}

	for (UActorComponent* Comp : Target->GetComponents())
	{
		if (Comp && Comp->Implements<UInteractableComponentTargetInterface>())
		{
			OutComponents.Add(Comp);
		}
	}

	OutComponents.Sort([](const UActorComponent& A, const UActorComponent& B)
	{
		const int32 PriorityA = IInteractableComponentTargetInterface::Execute_GetInteractPriority(&A);
		const int32 PriorityB = IInteractableComponentTargetInterface::Execute_GetInteractPriority(&B);
		return PriorityA > PriorityB;
	});
}

UActorComponent* FInteractionCapabilitySelector::SelectBestComponent(AActor* Target, UPrimitiveComponent* HitComponent)
{
	if (!Target)
	{
		return nullptr;
	}

	TArray<UActorComponent*> InteractableComponents;
	GatherComponents(Target, InteractableComponents);
	if (InteractableComponents.Num() == 0)
	{
		return nullptr;
	}

	if (!HitComponent)
	{
		return InteractableComponents[0];
	}

	TSet<UPrimitiveComponent*> HierarchyMeshes;
	CollectAttachmentHierarchy(HitComponent, HierarchyMeshes);

	UActorComponent* BestMeshScoped = nullptr;
	int32 BestMeshPriority = MIN_int32;
	UActorComponent* BestActorScoped = nullptr;
	int32 BestActorPriority = MIN_int32;

	for (UActorComponent* Comp : InteractableComponents)
	{
		bool bHasMeshProperty = false;
		const bool bMatchesHierarchy = CapabilityMatchesHierarchy(Comp, HierarchyMeshes, bHasMeshProperty);
		const int32 Priority = IInteractableComponentTargetInterface::Execute_GetInteractPriority(Comp);

		if (bMatchesHierarchy)
		{
			if (!BestMeshScoped || Priority > BestMeshPriority)
			{
				BestMeshScoped = Comp;
				BestMeshPriority = Priority;
			}
		}
		else if (!bHasMeshProperty)
		{
			if (!BestActorScoped || Priority > BestActorPriority)
			{
				BestActorScoped = Comp;
				BestActorPriority = Priority;
			}
		}
	}

	return BestMeshScoped ? BestMeshScoped : BestActorScoped;
}

bool FInteractionCapabilitySelector::ResolveFocus(
	AActor* Target,
	UPrimitiveComponent* HitComponent,
	FInteractionFocusInfo& OutFocus)
{
	if (!Target || !HitComponent)
	{
		return false;
	}

	UActorComponent* Selected = SelectBestComponent(Target, HitComponent);
	if (!Selected)
	{
		return false;
	}

	OutFocus.Label = IInteractableComponentTargetInterface::Execute_GetInteractionLabel(Selected);
	if (OutFocus.Label.IsEmpty())
	{
		OutFocus.Label = NSLOCTEXT("Interaction", "Interact", "Interact");
	}
	OutFocus.HighlightMesh = GetTargetMesh(Selected);
	if (!OutFocus.HighlightMesh)
	{
		OutFocus.HighlightMesh = HitComponent;
	}

	return true;
}

bool FInteractionCapabilitySelector::ResolveExecutionSpec(
	AActor* Target,
	UPrimitiveComponent* HitComponent,
	AActor* Instigator,
	FInteractionExecutionSpec& OutSpec)
{
	OutSpec = FInteractionExecutionSpec();
	if (!Target)
	{
		return false;
	}

	UActorComponent* Selected = SelectBestComponent(Target, HitComponent);
	if (!Selected)
	{
		return false;
	}

	OutSpec = IInteractableComponentTargetInterface::Execute_GetInteractionExecutionSpec(Selected, Instigator);
	return true;
}

bool FInteractionCapabilitySelector::ExecuteInteraction(
	AActor* Target,
	AActor* Instigator,
	UPrimitiveComponent* HitComponent)
{
	if (!Target || !Instigator)
	{
		return false;
	}

	UActorComponent* Selected = SelectBestComponent(Target, HitComponent);
	if (!Selected)
	{
		return false;
	}

	return IInteractableComponentTargetInterface::Execute_OnComponentInteract(Selected, Instigator);
}
