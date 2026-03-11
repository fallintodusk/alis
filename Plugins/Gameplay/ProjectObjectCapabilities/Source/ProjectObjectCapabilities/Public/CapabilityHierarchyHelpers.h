// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"

/**
 * Utilities for capability hierarchy resolution.
 *
 * Mental model: Combined objects (via attachment) are single logical entities.
 * Capabilities on any mesh in hierarchy apply to the whole combined object.
 */
namespace CapabilityHierarchyHelpers
{
	/**
	 * Collect all primitive components in the attachment hierarchy.
	 * Traverses both up (parents) and down (children) recursively.
	 */
	PROJECTOBJECTCAPABILITIES_API void CollectAttachmentHierarchy(USceneComponent* Root, TSet<UPrimitiveComponent*>& OutMeshes);

	/**
	 * Extract target mesh from a capability component.
	 * Uses explicit interaction interface contract only.
	 */
	PROJECTOBJECTCAPABILITIES_API UPrimitiveComponent* GetCapabilityTargetMesh(UActorComponent* Capability);

	/**
	 * Check if capability targets any mesh in the given set.
	 */
	PROJECTOBJECTCAPABILITIES_API bool CapabilityMatchesHierarchy(
		UActorComponent* Capability,
		const TSet<UPrimitiveComponent*>& HierarchyMeshes,
		bool& OutHasMeshProperty);
}
