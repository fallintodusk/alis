// AnimInstance for LocalBodyMesh (Layer 2b -- owner-visible first-person body).
//
// Thin orchestrator: owns CopyPose, space conversion, spine yaw tracking.
// Delegates upper-chain correction to the active ILocalBodyCorrection strategy.
//
// Pipeline: CopyPose -> CS -> Spine01(yaw) -> Spine02(yaw) -> [Correction] -> Local -> Output

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimNodes/AnimNode_CopyPoseFromMesh.h"
#include "Animation/AnimNodeSpaceConversions.h"
#include "BoneControllers/AnimNode_ModifyBone.h"
#include "ILocalBodyCorrection.h"
#include "LocalBodyCorrectionDisabled.h"
#include "LocalBodyCorrectionTransitionGuard.h"
#include "LocalBodyCorrectionAngleClamp.h"
#include "LocalBodyCorrectionChainIK.h"
#include "LocalBodyAnimInstance.generated.h"

// Bridges game thread -> anim thread.
USTRUCT()
struct FSpineLockData
{
	GENERATED_BODY()

	float YawDeltaDeg = 0.f;
	float NeckPitchDeg = 0.f;
	FVector NeckTargetCS = FVector::ZeroVector;
	bool bNeckTargetValid = false;
	FVector UpperSpineFollowCS = FVector::ZeroVector;
	float UpperSpinePitchDeg = 0.f;
	float UpperSpineGuardAlpha = 0.f;
	bool bValid = false;

	// Hand restore targets (saved from source pose before correction)
	FVector HandLTargetCS = FVector::ZeroVector;
	FVector HandRTargetCS = FVector::ZeroVector;
	FVector JointTargetLCS = FVector::ZeroVector;
	FVector JointTargetRCS = FVector::ZeroVector;
	bool bHandTargetsValid = false;
};

// Custom proxy: owns base anim nodes + delegates to correction strategy.
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

	// Base nodes (always active)
	FAnimNode_CopyPoseFromMesh CopyPoseNode;
	FAnimNode_ConvertLocalToComponentSpace LocalToCSNode;
	FAnimNode_ModifyBone Spine01Node;
	FAnimNode_ModifyBone Spine02Node;
	FAnimNode_ConvertComponentToLocalSpace CSToLocalNode;

	// Active correction strategy (set during Initialize)
	ILocalBodyCorrection* ActiveCorrection = nullptr;

	FSpineLockData SpineLockData;
};

UCLASS()
class PROJECTSKELETALCAPABILITIES_API ULocalBodyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UFUNCTION()
	FName GetCurrentSourceName() const;

	UPROPERTY(EditDefaultsOnly, Category = "FirstPerson")
	FName AnchorBoneName = TEXT("head");

	UPROPERTY(EditAnywhere, Category = "FirstPerson")
	bool bEnableSpineLock = true;

	UPROPERTY(EditAnywhere, Category = "FirstPerson")
	ELocalBodyUpperChainMode UpperChainMode = ELocalBodyUpperChainMode::ChainIK;

	FVector NeckOffsetFromCamera = FVector(-8.f, 0.f, -10.f);

	// Per-mode settings (only the active mode's settings are used)
	UPROPERTY(EditAnywhere, Category = "FirstPerson|TransitionGuard")
	FTransitionGuardSettings TransitionGuardSettings;

	UPROPERTY(EditAnywhere, Category = "FirstPerson|AngleClamp")
	FAngleClampSettings AngleClampSettings;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	FLocalBodyAnimInstanceProxy LocalBodyProxy;
	bool bLoggedOnce = false;
	bool bModeReinitPending = false;

	// Track which mode was wired at init so we can detect runtime changes.
	ELocalBodyUpperChainMode InitializedMode = ELocalBodyUpperChainMode::ChainIK;

	// Correction strategy instances (one per mode, created at init)
	FLocalBodyCorrectionDisabled CorrectionDisabled;
	FLocalBodyCorrectionTransitionGuard CorrectionTransitionGuard;
	FLocalBodyCorrectionAngleClamp CorrectionAngleClamp;
	FLocalBodyCorrectionChainIK CorrectionChainIK;

public:
	// Resolve the active correction based on UpperChainMode.
	ILocalBodyCorrection* ResolveCorrection();

private:
	void RequestDeferredModeReinitialize();
};
