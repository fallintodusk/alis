// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "MotionMatchingBridgeAnimInstance.generated.h"

class UCharacterMovementComponent;

/**
 * PostProcess AnimInstance that bridges CMC data to the MM AnimBP.
 *
 * The primary AnimBP (SandboxCharacter_CMC_ABP) reads CharacterProperties
 * via BPI_SandboxCharacter_Pawn interface. ADefinitionCharacter does not
 * implement this BP interface, so the struct stays zeroed.
 *
 * This PostProcess instance runs AFTER the primary's BlueprintUpdateAnimation
 * (which zeros CharacterProperties) and BEFORE the primary's
 * BlueprintThreadSafeUpdateAnimation (which reads it via Update_Logic).
 *
 * Timing (UE 5.7 SkeletalMeshComponent::TickAnimInstances):
 *   1. Primary->UpdateAnimation()       // zeros CharacterProperties via BPI
 *   2. PostProcess->UpdateAnimation()   // we write correct values here
 *   3. Primary->ParallelUpdateAnimation // Update_Logic reads our values
 *
 * IMPORTANT: This class must be used via a BP AnimBP derivative that has a
 * pass-through AnimGraph (LinkedAnimGraphInput -> OutputPose). Without a graph,
 * PostProcess evaluation produces ref pose and overwrites the primary's pose.
 * See: ABP_MotionMatchingBridge in ProjectSkeletalCapabilities Content.
 *
 * All struct field access uses UE reflection (FProperty), safe in Shipping.
 */
UCLASS()
class PROJECTSKELETALCAPABILITIES_API UMotionMatchingBridgeAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	bool CachePropertyPointers(UAnimInstance* PrimaryInstance);

	void WriteCharacterProperties(UAnimInstance* PrimaryInstance, ACharacter* Character,
		UCharacterMovementComponent* CMC);

	void ForceThreadSafeFlag(UAnimInstance* PrimaryInstance);

	// Cached property on primary AnimInstance: "CharacterProperties" struct
	FStructProperty* CachedCharPropsProp = nullptr;

	struct FCachedFields
	{
		FProperty* MovementMode = nullptr;
		FProperty* Stance = nullptr;
		FProperty* RotationMode = nullptr;
		FProperty* Gait = nullptr;
		FProperty* ActorTransform = nullptr;
		FProperty* Velocity = nullptr;
		FProperty* InputAcceleration = nullptr;
		FProperty* CurrentMaxAcceleration = nullptr;
		FProperty* CurrentMaxDeceleration = nullptr;
		FProperty* OrientationIntent = nullptr;
		FProperty* AimingRotation = nullptr;
		FProperty* JustLanded = nullptr;
		FProperty* LandVelocity = nullptr;
		FProperty* GroundNormal = nullptr;
		FProperty* InputState = nullptr;
		FProperty* WantsToSprint = nullptr;
		FProperty* WantsToWalk = nullptr;
		FProperty* WantsToStrafe = nullptr;
		FProperty* WantsToAim = nullptr;
		FProperty* WantsToCrouch = nullptr;

		bool IsValid() const
		{
			return MovementMode && Stance && RotationMode && Gait
				&& ActorTransform && Velocity && InputAcceleration
				&& AimingRotation && OrientationIntent;
		}
	};

	FCachedFields Fields;

	FBoolProperty* CachedThreadSafeProp = nullptr;
	TWeakObjectPtr<UAnimInstance> CachedPrimaryInstance;
	bool bCacheResolved = false;
};
