// ChainIK correction: hard neck attach + CCDIK spine solve + TwoBoneIK arm restore.
// NeckLock pins neck_01 to camera (hard, no jitter).
// CCDIK solves spine_03..spine_05 to bridge smoothly to the pinned neck.
// TwoBoneIK restores hand positions from source pose after spine correction.

#pragma once

#include "ILocalBodyCorrection.h"
#include "BoneControllers/AnimNode_ModifyBone.h"
#include "BoneControllers/AnimNode_CCDIK.h"
#include "BoneControllers/AnimNode_TwoBoneIK.h"

class FLocalBodyCorrectionChainIK : public ILocalBodyCorrection
{
public:
	float Precision = 0.5f;
	int32 MaxIterations = 10;

	virtual void InitializeNodes(UAnimInstance* AnimInstance, FAnimNode_Base* InputNode) override;
	virtual FAnimNode_Base* GetOutputNode() override;
	virtual void GetNodes(TArray<FAnimNode_Base*>& OutNodes) override;
	virtual void Update(const FSpineLockData& Data) override;
	virtual void EvaluateGameThread(const FLocalBodyFrameContext& Context, FSpineLockData& OutData) override;

private:
	FAnimNode_ModifyBone NeckLockNode;
	FAnimNode_CCDIK CCDIKNode;
	FAnimNode_TwoBoneIK ArmLNode;
	FAnimNode_TwoBoneIK ArmRNode;
};
