// AngleClamp correction: reactive angle-based clamp.
// Fires only when the upper chain forward angle exceeds a threshold.
// Same ModifyBone node setup as TransitionGuard.

#include "LocalBodyCorrectionAngleClamp.h"
#include "LocalBodyAnimInstance.h"
#include "LocalBodyCorrectionMath.h"

using LocalBodyCorrectionMath::ComputeUpperChainForwardAngleDeg;
using LocalBodyCorrectionMath::ConstrainUpperChainCameraDelta;

// ---------------------------------------------------------------------------
// File-local helpers
// ---------------------------------------------------------------------------

namespace {

static void ConfigureSpineNode(
	FAnimNode_ModifyBone& Node,
	const FName& BoneName)
{
	Node.BoneToModify.BoneName = BoneName;
	Node.TranslationMode = BMM_Ignore;
	Node.RotationMode = BMM_Additive;
	Node.RotationSpace = BCS_ComponentSpace;
	Node.ScaleMode = BMM_Ignore;
	Node.Alpha = 1.0f;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// InitializeNodes
// ---------------------------------------------------------------------------

void FLocalBodyCorrectionAngleClamp::InitializeNodes(
	UAnimInstance* AnimInstance,
	FAnimNode_Base* InputNode)
{
	// Spine03/04/05: additive rotation + translation in component space
	ConfigureSpineNode(Spine03Node, FName("spine_03"));
	ConfigureSpineNode(Spine04Node, FName("spine_04"));
	ConfigureSpineNode(Spine05Node, FName("spine_05"));

	Spine03Node.TranslationMode = BMM_Additive;
	Spine03Node.TranslationSpace = BCS_ComponentSpace;
	Spine04Node.TranslationMode = BMM_Additive;
	Spine04Node.TranslationSpace = BCS_ComponentSpace;
	Spine05Node.TranslationMode = BMM_Additive;
	Spine05Node.TranslationSpace = BCS_ComponentSpace;

	// NeckLock: replace translation to pin neck_01 at camera target
	ConfigureSpineNode(NeckLockNode, FName("neck_01"));
	NeckLockNode.TranslationMode = BMM_Replace;
	NeckLockNode.TranslationSpace = BCS_ComponentSpace;

	// Chain: InputNode -> Spine03 -> Spine04 -> Spine05 -> NeckLock
	Spine03Node.ComponentPose.SetLinkNode(InputNode);
	Spine04Node.ComponentPose.SetLinkNode(&Spine03Node);
	Spine05Node.ComponentPose.SetLinkNode(&Spine04Node);
	NeckLockNode.ComponentPose.SetLinkNode(&Spine05Node);
}

// ---------------------------------------------------------------------------
// GetOutputNode
// ---------------------------------------------------------------------------

FAnimNode_Base* FLocalBodyCorrectionAngleClamp::GetOutputNode()
{
	return &NeckLockNode;
}

// ---------------------------------------------------------------------------
// GetNodes
// ---------------------------------------------------------------------------

void FLocalBodyCorrectionAngleClamp::GetNodes(TArray<FAnimNode_Base*>& OutNodes)
{
	OutNodes.Add(&Spine03Node);
	OutNodes.Add(&Spine04Node);
	OutNodes.Add(&Spine05Node);
	OutNodes.Add(&NeckLockNode);
}

// ---------------------------------------------------------------------------
// Update -- apply rotation/translation from SpineLockData (anim thread)
// ---------------------------------------------------------------------------

void FLocalBodyCorrectionAngleClamp::Update(const FSpineLockData& Data)
{
	const float YawDelta = Data.YawDeltaDeg;

	Spine03Node.Rotation = FRotator(Data.UpperSpinePitchDeg * 0.20f, YawDelta * 0.3f, 0.f);
	Spine04Node.Rotation = FRotator(Data.UpperSpinePitchDeg * 0.35f, 0.f, 0.f);
	Spine05Node.Rotation = FRotator(Data.UpperSpinePitchDeg * 0.45f, 0.f, 0.f);

	Spine03Node.Translation = Data.UpperSpineFollowCS * 0.25f;
	Spine04Node.Translation = Data.UpperSpineFollowCS * 0.30f;
	Spine05Node.Translation = Data.UpperSpineFollowCS * 0.45f;

	Spine03Node.Alpha = Data.bNeckTargetValid ? 1.0f : 0.0f;
	Spine04Node.Alpha = Data.bNeckTargetValid ? 1.0f : 0.0f;
	Spine05Node.Alpha = Data.bNeckTargetValid ? 1.0f : 0.0f;

	if (Data.bNeckTargetValid)
	{
		NeckLockNode.Translation = Data.NeckTargetCS;
	}
	NeckLockNode.Rotation = FRotator(Data.NeckPitchDeg, 0.f, 0.f);
	NeckLockNode.Alpha = 1.0f;
}

// ---------------------------------------------------------------------------
// EvaluateGameThread -- reactive angle clamp filter
// ---------------------------------------------------------------------------

void FLocalBodyCorrectionAngleClamp::EvaluateGameThread(
	const FLocalBodyFrameContext& Context,
	FSpineLockData& OutData)
{
	OutData.UpperSpineFollowCS = FVector::ZeroVector;
	OutData.UpperSpinePitchDeg = 0.f;
	OutData.UpperSpineGuardAlpha = 0.f;

	if (!Context.bHasSourceNeck || !Context.bHasSourceSpine05)
	{
		State.bWasCorrectingLastFrame = false;
		return;
	}

	// Measure current upper chain forward angle in camera-local space
	const FVector SourceNeckCameraLocal =
		Context.CameraTransform.InverseTransformPosition(Context.SourceNeckWorld);
	const FVector SourceSpine05CameraLocal =
		Context.CameraTransform.InverseTransformPosition(Context.SourceSpine05World);
	const FVector SourceUpperChainCameraDelta =
		SourceSpine05CameraLocal - SourceNeckCameraLocal;
	const float CurrentForwardAngleDeg =
		ComputeUpperChainForwardAngleDeg(SourceUpperChainCameraDelta);

	// Hysteresis: lower threshold to deactivate, higher to activate
	const float ActivationAngle = State.bWasCorrectingLastFrame
		? (Settings.MaxUpperChainForwardAngleDeg - Settings.AngleHysteresisDeg)
		: Settings.MaxUpperChainForwardAngleDeg;

	// Within bounds -- animation is fine, no correction needed
	if (CurrentForwardAngleDeg <= ActivationAngle)
	{
		State.bWasCorrectingLastFrame = false;
		return;
	}

	// Exceeding max angle -- clamp
	State.bWasCorrectingLastFrame = true;
	const float ExcessDeg = CurrentForwardAngleDeg - Settings.MaxUpperChainForwardAngleDeg;
	OutData.UpperSpineGuardAlpha = FMath::Clamp(
		ExcessDeg / FMath::Max(1.0f, Settings.MaxUpperChainForwardAngleDeg),
		0.0f, 1.0f);

	// Constrain the delta to the max allowed angle
	const float SourceChainDist =
		FVector::Dist(Context.SourceSpine05World, Context.SourceNeckWorld);
	const FVector ConstrainedDelta = ConstrainUpperChainCameraDelta(
		SourceUpperChainCameraDelta,
		SourceChainDist,
		Settings.MaxUpperChainForwardAngleDeg,
		Settings.MinUpperChainDropCm);

	// Follow translation: move spine_05 from source to constrained position
	const FVector SourceSpine05CS =
		Context.ComponentTransform.InverseTransformPosition(Context.SourceSpine05World);
	const FVector DesiredSpine05World =
		Context.NeckTargetWorld +
		Context.CameraTransform.TransformVectorNoScale(ConstrainedDelta);
	const FVector DesiredSpine05CS =
		Context.ComponentTransform.InverseTransformPosition(DesiredSpine05World);
	OutData.UpperSpineFollowCS =
		(DesiredSpine05CS - SourceSpine05CS).GetClampedToMaxSize(Settings.MaxUpperSpineFollowCm);

	// Pitch correction: tilt spine backward by the excess angle
	OutData.UpperSpinePitchDeg =
		-FMath::Min(Settings.MaxUpperSpinePitchDeg, ExcessDeg);
}
