// AngleClamp correction: reactive angle-based clamp.
// Fires only when the upper chain forward angle exceeds a threshold.
// Uses same ModifyBone nodes as TransitionGuard.

#pragma once

#include "ILocalBodyCorrection.h"
#include "BoneControllers/AnimNode_ModifyBone.h"
#include "LocalBodyCorrectionAngleClamp.generated.h"

USTRUCT()
struct FAngleClampSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere) float MaxUpperChainForwardAngleDeg = 42.f;
	UPROPERTY(EditAnywhere) float AngleHysteresisDeg = 5.f;
	UPROPERTY(EditAnywhere) float MinUpperChainDropCm = 9.f;
	UPROPERTY(EditAnywhere) float MaxUpperSpinePitchDeg = 20.f;
	UPROPERTY(EditAnywhere) float MaxUpperSpineFollowCm = 45.f;
};

struct FAngleClampState
{
	bool bWasCorrectingLastFrame = false;
};

class FLocalBodyCorrectionAngleClamp : public ILocalBodyCorrection
{
public:
	FAngleClampSettings Settings;
	FAngleClampState State;

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
