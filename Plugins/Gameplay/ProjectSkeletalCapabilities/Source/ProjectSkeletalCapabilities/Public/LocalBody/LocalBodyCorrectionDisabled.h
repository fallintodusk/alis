// Pass-through correction: no upper-chain modification.
// Used as the baseline etalon for A/B testing.

#pragma once

#include "ILocalBodyCorrection.h"

class FLocalBodyCorrectionDisabled : public ILocalBodyCorrection
{
public:
	virtual void InitializeNodes(UAnimInstance* AnimInstance, FAnimNode_Base* InputNode) override;
	virtual FAnimNode_Base* GetOutputNode() override;
	virtual void GetNodes(TArray<FAnimNode_Base*>& OutNodes) override;
	virtual void Update(const FSpineLockData& Data) override;
	virtual void EvaluateGameThread(const FLocalBodyFrameContext& Context, FSpineLockData& OutData) override;

private:
	FAnimNode_Base* PassThroughNode = nullptr;
};
