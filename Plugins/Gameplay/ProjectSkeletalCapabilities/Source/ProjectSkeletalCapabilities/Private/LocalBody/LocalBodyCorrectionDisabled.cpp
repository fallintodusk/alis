// Disabled correction: pass-through, no modification.

#include "LocalBodyCorrectionDisabled.h"
#include "LocalBodyAnimInstance.h"

void FLocalBodyCorrectionDisabled::InitializeNodes(UAnimInstance* AnimInstance, FAnimNode_Base* InputNode)
{
	PassThroughNode = InputNode;
}

FAnimNode_Base* FLocalBodyCorrectionDisabled::GetOutputNode()
{
	return PassThroughNode;
}

void FLocalBodyCorrectionDisabled::GetNodes(TArray<FAnimNode_Base*>& OutNodes)
{
}

void FLocalBodyCorrectionDisabled::Update(const FSpineLockData& Data)
{
}

void FLocalBodyCorrectionDisabled::EvaluateGameThread(
	const FLocalBodyFrameContext& Context,
	FSpineLockData& OutData)
{
	OutData.UpperSpineFollowCS = FVector::ZeroVector;
	OutData.UpperSpinePitchDeg = 0.f;
	OutData.UpperSpineGuardAlpha = 0.f;
}
