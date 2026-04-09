// AnimInstance for LocalBodyMesh (Layer 2b -- owner-visible first-person body).
//
// Why this exists: Motion Matching causes the skeletal mesh to drift forward/back
// relative to the camera. Without correction, the player sees inside their own body.
// This AnimInstance copies the pose from a runtime-resolved source mesh and applies
// spine yaw tracking (camera-driven upper body rotation) plus neck anti-clip safety.
//
// Source resolution:
// - preferred path: WorldBody / WorldBodyMesh (retargeted visible layer)
// - fallback path: DriverBody only while the world visual layer is still empty
//
// Pipeline: CopyPose -> CS -> Spine01 -> Spine02 -> Spine03 -> NeckLock -> Local -> Output
//
// Lives in ProjectSkeletalCapabilities (not ProjectCharacter) because it is part of
// the modular skeletal assembly framework. Legacy AProjectCharacter also uses it.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimNodes/AnimNode_CopyPoseFromMesh.h"
#include "Animation/AnimNodeSpaceConversions.h"
#include "BoneControllers/AnimNode_ModifyBone.h"
#include "LocalBodyAnimInstance.generated.h"

// Bridges game thread -> anim thread because anim nodes run on worker threads
// and cannot safely access UWorld objects (camera, bone transforms) directly.
USTRUCT()
struct FSpineLockData
{
	GENERATED_BODY()

	// Camera yaw delta (ControlYaw - ActorYaw), clamped to [-90, 90]
	// Distributed across spine_01/02/03 for upper body camera tracking
	float YawDeltaDeg = 0.f;

	// Neck pitch correction for anti-clip when looking down
	float NeckPitchDeg = 0.f;

	// First-frame guard
	bool bValid = false;
};

// Custom proxy owns the anim node chain and runs spine/neck math on the anim thread.
//
// Node chain: CopyPose -> CS -> Spine01 -> Spine02 -> Spine03 -> NeckLock -> Local -> Output
USTRUCT()
struct FLocalBodyAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FLocalBodyAnimInstanceProxy() : FAnimInstanceProxy() {}
	FLocalBodyAnimInstanceProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual FAnimNode_Base* GetCustomRootNode() override { return &CSToLocalNode; }
	virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override;
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;

	FAnimNode_CopyPoseFromMesh CopyPoseNode;
	FAnimNode_ConvertLocalToComponentSpace LocalToCSNode;
	FAnimNode_ModifyBone Spine01Node;
	FAnimNode_ModifyBone Spine02Node;
	FAnimNode_ModifyBone Spine03Node;
	FAnimNode_ModifyBone NeckLockNode;
	FAnimNode_ConvertComponentToLocalSpace CSToLocalNode;

	FSpineLockData SpineLockData;
};

UCLASS()
class PROJECTSKELETALCAPABILITIES_API ULocalBodyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	// Which bone to measure drift against camera
	UPROPERTY(EditDefaultsOnly, Category = "FirstPerson")
	FName AnchorBoneName = TEXT("head");

	// Master toggle for root offset
	UPROPERTY(EditAnywhere, Category = "FirstPerson")
	bool bEnableSpineLock = true;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	FLocalBodyAnimInstanceProxy LocalBodyProxy;

	// One-time diagnostic log guard
	bool bLoggedOnce = false;
};
