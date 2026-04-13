// ChainIK correction: NeckLock + CCDIK spine + TwoBoneIK arms.
//
// Pipeline: Input -> NeckLock(neck_01 hard pin) -> CCDIK(spine_03..spine_05) -> ArmL -> ArmR
//
// NeckLock hard-attaches neck_01 to camera (BMM_Replace). No jitter.
// CCDIK then solves spine_03/04/05 to connect smoothly to the pinned neck.
// TwoBoneIK restores hands to source pose after spine adjustment.

#include "LocalBodyCorrectionChainIK.h"
#include "LocalBodyAnimInstance.h"

void FLocalBodyCorrectionChainIK::InitializeNodes(
	UAnimInstance* AnimInstance,
	FAnimNode_Base* InputNode)
{
	// NeckLock: hard pin neck_01 to camera target (no solver jitter)
	NeckLockNode.BoneToModify.BoneName = FName("neck_01");
	NeckLockNode.TranslationMode = BMM_Replace;
	NeckLockNode.TranslationSpace = BCS_ComponentSpace;
	NeckLockNode.RotationMode = BMM_Additive;
	NeckLockNode.RotationSpace = BCS_ComponentSpace;
	NeckLockNode.ScaleMode = BMM_Ignore;
	NeckLockNode.Alpha = 1.0f;

	// CCDIK: solve spine_03..spine_05 to bridge to the pinned neck
	CCDIKNode.RootBone.BoneName = FName("spine_03");
	CCDIKNode.TipBone.BoneName = FName("spine_05");
	CCDIKNode.Precision = Precision;
	CCDIKNode.MaxIterations = MaxIterations;
	CCDIKNode.bStartFromTail = true;
	CCDIKNode.EffectorLocationSpace = BCS_ComponentSpace;
	CCDIKNode.bEnableRotationLimit = false;
	CCDIKNode.ResizeRotationLimitPerJoints(3);
	CCDIKNode.Alpha = 0.f;

	// TwoBoneIK left arm
	ArmLNode.IKBone.BoneName = FName("hand_l");
	ArmLNode.EffectorLocationSpace = BCS_ComponentSpace;
	ArmLNode.JointTargetLocationSpace = BCS_ComponentSpace;
	ArmLNode.bAllowStretching = false;
	ArmLNode.bTakeRotationFromEffectorSpace = false;
	ArmLNode.bMaintainEffectorRelRot = true;
	ArmLNode.Alpha = 0.f;

	// TwoBoneIK right arm
	ArmRNode.IKBone.BoneName = FName("hand_r");
	ArmRNode.EffectorLocationSpace = BCS_ComponentSpace;
	ArmRNode.JointTargetLocationSpace = BCS_ComponentSpace;
	ArmRNode.bAllowStretching = false;
	ArmRNode.bTakeRotationFromEffectorSpace = false;
	ArmRNode.bMaintainEffectorRelRot = true;
	ArmRNode.Alpha = 0.f;

	// Chain: Input -> NeckLock -> CCDIK -> ArmL -> ArmR
	NeckLockNode.ComponentPose.SetLinkNode(InputNode);
	CCDIKNode.ComponentPose.SetLinkNode(&NeckLockNode);
	ArmLNode.ComponentPose.SetLinkNode(&CCDIKNode);
	ArmRNode.ComponentPose.SetLinkNode(&ArmLNode);
}

FAnimNode_Base* FLocalBodyCorrectionChainIK::GetOutputNode()
{
	return &ArmRNode;
}

void FLocalBodyCorrectionChainIK::GetNodes(TArray<FAnimNode_Base*>& OutNodes)
{
	OutNodes.Add(&NeckLockNode);
	OutNodes.Add(&CCDIKNode);
	OutNodes.Add(&ArmLNode);
	OutNodes.Add(&ArmRNode);
}

void FLocalBodyCorrectionChainIK::Update(const FSpineLockData& Data)
{
	// NeckLock: hard pin neck_01 to camera target
	if (Data.bNeckTargetValid)
	{
		NeckLockNode.Translation = Data.NeckTargetCS;
	}
	NeckLockNode.Rotation = FRotator(Data.NeckPitchDeg, 0.f, 0.f);
	NeckLockNode.Alpha = 1.0f;

	// CCDIK: solve spine_03..spine_05 toward the pinned neck position.
	// The effector is the neck target -- spine_05 should end up close to
	// where neck_01 is pinned so the chain doesn't stretch.
	CCDIKNode.EffectorLocation = Data.NeckTargetCS;
	CCDIKNode.Alpha = Data.bNeckTargetValid ? 1.0f : 0.0f;

	// TwoBoneIK: restore hands to source pose
	ArmLNode.EffectorLocation = Data.HandLTargetCS;
	ArmLNode.JointTargetLocation = Data.JointTargetLCS;
	ArmLNode.Alpha = Data.bHandTargetsValid ? 1.0f : 0.0f;

	ArmRNode.EffectorLocation = Data.HandRTargetCS;
	ArmRNode.JointTargetLocation = Data.JointTargetRCS;
	ArmRNode.Alpha = Data.bHandTargetsValid ? 1.0f : 0.0f;
}

void FLocalBodyCorrectionChainIK::EvaluateGameThread(
	const FLocalBodyFrameContext& Context,
	FSpineLockData& OutData)
{
	OutData.UpperSpineFollowCS = FVector::ZeroVector;
	OutData.UpperSpinePitchDeg = 0.f;
	OutData.UpperSpineGuardAlpha = 0.f;
}
