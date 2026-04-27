// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IInteractableTarget.h"

class AActor;
class UActorComponent;
class UPrimitiveComponent;
class USceneComponent;

class FInteractionCapabilitySelector
{
public:
	static void GatherComponents(AActor* Target, TArray<UActorComponent*>& OutComponents);
	static UPrimitiveComponent* GetTargetMesh(UActorComponent* Capability);

	static bool ResolveFocus(AActor* Target, UPrimitiveComponent* HitComponent, FInteractionFocusInfo& OutFocus);
	static bool ResolveExecutionSpec(
		AActor* Target,
		UPrimitiveComponent* HitComponent,
		AActor* Instigator,
		FInteractionExecutionSpec& OutSpec);
	static bool ExecuteInteraction(AActor* Target, AActor* Instigator, UPrimitiveComponent* HitComponent);

private:
	static void CollectAttachmentHierarchy(USceneComponent* HitNode, TSet<UPrimitiveComponent*>& OutMeshes);
	static bool CapabilityMatchesHierarchy(
		UActorComponent* Capability,
		const TSet<UPrimitiveComponent*>& HierarchyMeshes,
		bool& OutHasMeshProperty);
	static UActorComponent* SelectBestComponent(AActor* Target, UPrimitiveComponent* HitComponent);
};
