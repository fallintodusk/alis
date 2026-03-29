// AnimInstance for LocalBodyMesh (Layer 2b — owner-visible first-person body).
//
// Why this exists: Motion Matching causes the skeletal mesh to drift forward/back
// relative to the camera. Without correction, the player sees inside their own body.
// This AnimInstance copies the pose from WorldBodyMesh and offsets the root bone
// to keep the torso centered under the camera.
//
// Pipeline: CopyPoseFromMesh -> LocalToCS -> RootFreezeNode(root) -> CSToLocal -> Output

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
// CopyPose: replicates WorldBodyMesh pose so LocalBody has identical animation
#include "AnimNodes/AnimNode_CopyPoseFromMesh.h"
// Space conversions: ModifyBone works in Component Space, output must be Local Space
#include "Animation/AnimNodeSpaceConversions.h"
// ModifyBone: applies root XY offset to cancel mesh drift from Motion Matching
#include "BoneControllers/AnimNode_ModifyBone.h"
#include "LocalBodyAnimInstance.generated.h"

// Bridges game thread -> anim thread because anim nodes run on worker threads
// and cannot safely access UWorld objects (camera, bone transforms) directly.
USTRUCT()
struct FSpineLockData
{
	GENERATED_BODY()

	// Read from WorldBodyMesh (not LocalBody) to break the feedback loop —
	// reading LocalBody's own root would chase its own correction endlessly
	UPROPERTY()
	FVector WorldRootPos = FVector::ZeroVector;

	// neck_02 position from WorldBodyMesh — drift is measured as (neck - camera) in XY
	UPROPERTY()
	FVector GoalLocation = FVector::ZeroVector;

	// First-frame guard: ModifyBone with ZeroVector would snap skeleton to origin
	bool bValid = false;
};

// Custom proxy owns the anim node chain and runs root offset math on the anim thread.
// Why a proxy: UE evaluates anim graphs on worker threads for performance —
// all per-frame bone math must live here, not in UAnimInstance.
//
// Node chain: CopyPose -> LocalToCS -> RootFreezeNode(root) -> CSToLocal -> Output
// SpineLockNode and NeckLockNode are kept at Alpha=0 for future per-bone experiments.
USTRUCT()
struct FLocalBodyAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FLocalBodyAnimInstanceProxy() : FAnimInstanceProxy() {}
	FLocalBodyAnimInstanceProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	// Wires the node chain and sets CopyPose source to WorldBodyMesh
	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	// Entry point for anim evaluation — returns the last node in the chain
	virtual FAnimNode_Base* GetCustomRootNode() override { return &CSToLocalNode; }
	// Registers all nodes so UE can initialize/cache them
	virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override;
	// Runs every frame before evaluation — calculates root XY offset from SpineLockData
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;

	// --- Anim node chain ---
	// Replicates WorldBodyMesh pose so LocalBody animates identically
	FAnimNode_CopyPoseFromMesh CopyPoseNode;
	// ModifyBone requires Component Space input
	FAnimNode_ConvertLocalToComponentSpace LocalToCSNode;
	// Active: offsets root XY to cancel Motion Matching drift from camera
	FAnimNode_ModifyBone RootFreezeNode;
	// Reserved: future per-bone spine correction if root-only proves insufficient (Alpha=0)
	FAnimNode_ModifyBone SpineLockNode;
	// Reserved: future per-bone neck correction (Alpha=0)
	FAnimNode_ModifyBone NeckLockNode;
	// Converts back to Local Space for final output pose
	FAnimNode_ConvertComponentToLocalSpace CSToLocalNode;

	// Game thread writes, anim thread reads — the only safe data channel between them
	FSpineLockData SpineLockData;
};

// Game-thread side: reads WorldBodyMesh bone positions each frame
// and passes them to the proxy via SpineLockData for root offset calculation.
UCLASS()
class PROJECTCHARACTER_API ULocalBodyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	// Which bone to measure drift against camera — head sits directly above spine,
	// giving the most accurate read of how far the body drifted from camera center
	UPROPERTY(EditDefaultsOnly, Category = "FirstPerson")
	FName AnchorBoneName = TEXT("head");

	// Master toggle: allows disabling root offset at runtime for A/B testing or debugging
	UPROPERTY(EditAnywhere, Category = "FirstPerson")
	bool bEnableSpineLock = true;

protected:
	// Returns member proxy instead of heap-allocating — avoids per-instance new/delete
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	// No-op: proxy is a member field, engine must not free it
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
	// Game thread: reads WorldBodyMesh bones and writes SpineLockData for the anim thread
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	// Member-owned proxy — lifetime tied to this AnimInstance, no separate allocation
	FLocalBodyAnimInstanceProxy LocalBodyProxy;
};
