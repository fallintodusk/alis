// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UStaticMeshComponent;
class UPrimitiveComponent;
class UBoxComponent;

/**
 * Static helpers for physics constraint setup (hinges, sliders).
 * Extracted to avoid duplication between SpringRotatorComponent and SpringSliderComponent.
 */
class PROJECTMOTIONSYSTEM_API FMotionConstraintHelpers
{
public:
	/**
	 * Find or create an anchor component for a physics constraint.
	 * Tries: parent primitive -> actor root primitive -> create invisible UBoxComponent
	 *
	 * @param TargetMesh The mesh being constrained
	 * @param OutCreatedAnchor If a new anchor was created, it's stored here (caller should keep a pointer)
	 * @return Valid primitive component to use as anchor, or nullptr on failure
	 */
	static UPrimitiveComponent* FindOrCreateAnchor(
		UStaticMeshComponent* TargetMesh,
		UBoxComponent*& OutCreatedAnchor);

	/**
	 * Configure mesh for physics constraint mode.
	 * Sets: Movable mobility, SimulatePhysics(true), PhysicsActor profile, Gravity(false)
	 *
	 * @param Mesh Mesh component to configure
	 */
	static void EnablePhysicsOnMesh(UStaticMeshComponent* Mesh);
};
