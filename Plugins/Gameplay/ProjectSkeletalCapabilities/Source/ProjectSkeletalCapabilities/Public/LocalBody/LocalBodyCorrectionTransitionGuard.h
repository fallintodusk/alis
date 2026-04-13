// TransitionGuard correction: stateful, transition-aware spine correction.
// Driven by source-mesh drift, movement state (stop/landing/crouch/air), and neck-gap excess.
// Uses ModifyBone nodes on spine_03/04/05 + neck_01 BMM_Replace.

#pragma once

#include "ILocalBodyCorrection.h"
#include "BoneControllers/AnimNode_ModifyBone.h"
#include "LocalBodyCorrectionTransitionGuard.generated.h"

USTRUCT()
struct FTransitionGuardSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere) float SourceHeadStretchGuardCm = 26.f;
	UPROPERTY(EditAnywhere) float SourceNeckGapToleranceCm = 12.f;
	UPROPERTY(EditAnywhere) float SourceHeadForwardGuardCm = 10.f;
	UPROPERTY(EditAnywhere) float SourceNeckForwardGuardCm = 8.f;
	UPROPERTY(EditAnywhere) float UpperSpineRetreatStartPitchDeg = -30.f;
	UPROPERTY(EditAnywhere) float StopGuardDurationSec = 0.45f;
	UPROPERTY(EditAnywhere) float StopGuardMultiplier = 0.45f;
	UPROPERTY(EditAnywhere) float MaxUpperSpinePitchDeg = 20.f;
	UPROPERTY(EditAnywhere) float MaxUpperSpineFollowCm = 45.f;
	UPROPERTY(EditAnywhere) float MaxUpperChainForwardAngleDeg = 42.f;
	UPROPERTY(EditAnywhere) float TightUpperChainForwardAngleDeg = 20.f;
	UPROPERTY(EditAnywhere) float MinUpperChainDropCm = 9.f;
	UPROPERTY(EditAnywhere) float MinUpperChainSolveGapCm = 2.f;
	UPROPERTY(EditAnywhere) float MaxUpperChainLeanDeg = 52.f;
	UPROPERTY(EditAnywhere) float MinUpperTorsoPerpDistCm = 24.f;
	UPROPERTY(EditAnywhere) float CrouchGuardMultiplier = 0.20f;
	UPROPERTY(EditAnywhere) float AirGuardMultiplier = 0.35f;
	UPROPERTY(EditAnywhere) float LandingGuardDurationSec = 0.35f;
	UPROPERTY(EditAnywhere) float LandingGuardMultiplier = 0.55f;
};

struct FTransitionGuardState
{
	float PreviousHorizontalSpeed = 0.f;
	float StopGuardTimeRemaining = 0.f;
	float LandingGuardTimeRemaining = 0.f;
	bool bWasFallingLastUpdate = false;
	float StopGuardAlpha = 0.f;
	float LandingGuardAlpha = 0.f;
};

class FLocalBodyCorrectionTransitionGuard : public ILocalBodyCorrection
{
public:
	FTransitionGuardSettings Settings;
	FTransitionGuardState State;

	virtual void InitializeNodes(UAnimInstance* AnimInstance, FAnimNode_Base* InputNode) override;
	virtual FAnimNode_Base* GetOutputNode() override;
	virtual void GetNodes(TArray<FAnimNode_Base*>& OutNodes) override;
	virtual void Update(const FSpineLockData& Data) override;
	virtual void EvaluateGameThread(const FLocalBodyFrameContext& Context, FSpineLockData& OutData) override;

private:
	FAnimNode_ModifyBone Spine03Node;
	FAnimNode_ModifyBone Spine04Node;
	FAnimNode_ModifyBone Spine05Node;
	FAnimNode_ModifyBone NeckLockNode;
};
