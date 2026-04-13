// TransitionGuard correction implementation.
// Combines node setup/update (from LocalBodyAnimInstance proxy) with the
// stateful filter evaluation (from LocalBodyUpperChainFilter::EvaluateTransitionGuard).

#include "LocalBodyCorrectionTransitionGuard.h"
#include "LocalBodyAnimInstance.h"

// ---------------------------------------------------------------------------
// File-local helpers (wrapped in anonymous namespace for unity build safety)
// ---------------------------------------------------------------------------

namespace {

static float ComputeUpperChainForwardAngleDeg(const FVector& UpperChainCameraDelta)
{
	const float ForwardCm = FMath::Max(0.0f, UpperChainCameraDelta.X);
	const float DownCm = FMath::Abs(FMath::Min(0.0f, UpperChainCameraDelta.Z));
	if (ForwardCm <= KINDA_SMALL_NUMBER && DownCm <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	return FMath::RadiansToDegrees(FMath::Atan2(ForwardCm, FMath::Max(KINDA_SMALL_NUMBER, DownCm)));
}

static FVector ConstrainUpperChainCameraDelta(
	const FVector& SourceUpperChainCameraDelta,
	const float ChainLengthCm,
	const float MaxForwardAngleDeg,
	const float MinDropCm)
{
	if (ChainLengthCm <= KINDA_SMALL_NUMBER)
	{
		return SourceUpperChainCameraDelta;
	}

	const float LateralCm = SourceUpperChainCameraDelta.Y;
	const float PlanarLenSq = FMath::Max(
		0.0f,
		FMath::Square(ChainLengthCm) - FMath::Square(LateralCm));
	const float PlanarLen = FMath::Sqrt(PlanarLenSq);
	if (PlanarLen <= KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	const float SafeMinDropCm = FMath::Min(MinDropCm, PlanarLen);
	const float MaxForwardByAngleCm =
		PlanarLen * FMath::Sin(FMath::DegreesToRadians(MaxForwardAngleDeg));
	const float MaxForwardByDropCm = FMath::Sqrt(FMath::Max(
		0.0f,
		PlanarLenSq - FMath::Square(SafeMinDropCm)));
	const float MaxForwardCm = FMath::Min(MaxForwardByAngleCm, MaxForwardByDropCm);

	FVector Result = SourceUpperChainCameraDelta;
	if (Result.X > 0.0f)
	{
		Result.X = FMath::Min(Result.X, MaxForwardCm);
	}

	Result.Z = -FMath::Sqrt(FMath::Max(0.0f, PlanarLenSq - FMath::Square(Result.X)));
	return Result;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// InitializeNodes
// ---------------------------------------------------------------------------

void FLocalBodyCorrectionTransitionGuard::InitializeNodes(
	UAnimInstance* AnimInstance,
	FAnimNode_Base* InputNode)
{
	// Configure spine_03/04/05: additive translation + rotation in component space
	auto ConfigureSpineNode = [](FAnimNode_ModifyBone& Node, const FName& BoneName)
	{
		Node.BoneToModify.BoneName = BoneName;
		Node.TranslationMode = BMM_Additive;
		Node.TranslationSpace = BCS_ComponentSpace;
		Node.RotationMode = BMM_Additive;
		Node.RotationSpace = BCS_ComponentSpace;
		Node.ScaleMode = BMM_Ignore;
		Node.Alpha = 1.0f;
	};

	ConfigureSpineNode(Spine03Node, FName("spine_03"));
	ConfigureSpineNode(Spine04Node, FName("spine_04"));
	ConfigureSpineNode(Spine05Node, FName("spine_05"));

	// Configure neck_01: replace translation, additive rotation in component space
	NeckLockNode.BoneToModify.BoneName = FName("neck_01");
	NeckLockNode.TranslationMode = BMM_Replace;
	NeckLockNode.TranslationSpace = BCS_ComponentSpace;
	NeckLockNode.RotationMode = BMM_Additive;
	NeckLockNode.RotationSpace = BCS_ComponentSpace;
	NeckLockNode.ScaleMode = BMM_Ignore;
	NeckLockNode.Alpha = 1.0f;

	// Wire chain: InputNode -> Spine03 -> Spine04 -> Spine05 -> NeckLock
	Spine03Node.ComponentPose.SetLinkNode(InputNode);
	Spine04Node.ComponentPose.SetLinkNode(&Spine03Node);
	Spine05Node.ComponentPose.SetLinkNode(&Spine04Node);
	NeckLockNode.ComponentPose.SetLinkNode(&Spine05Node);
}

// ---------------------------------------------------------------------------
// GetOutputNode / GetNodes
// ---------------------------------------------------------------------------

FAnimNode_Base* FLocalBodyCorrectionTransitionGuard::GetOutputNode()
{
	return &NeckLockNode;
}

void FLocalBodyCorrectionTransitionGuard::GetNodes(TArray<FAnimNode_Base*>& OutNodes)
{
	OutNodes.Add(&Spine03Node);
	OutNodes.Add(&Spine04Node);
	OutNodes.Add(&Spine05Node);
	OutNodes.Add(&NeckLockNode);
}

// ---------------------------------------------------------------------------
// Update -- apply spine rotation/translation from game-thread data (anim thread)
// ---------------------------------------------------------------------------

void FLocalBodyCorrectionTransitionGuard::Update(const FSpineLockData& Data)
{
	Spine03Node.Rotation = FRotator(Data.UpperSpinePitchDeg * 0.20f, Data.YawDeltaDeg * 0.3f, 0.f);
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
// EvaluateGameThread -- TransitionGuard filter (from LocalBodyUpperChainFilter)
// ---------------------------------------------------------------------------

void FLocalBodyCorrectionTransitionGuard::EvaluateGameThread(
	const FLocalBodyFrameContext& Context,
	FSpineLockData& OutData)
{
	// Update temporal state: stop guard
	if (State.PreviousHorizontalSpeed > 350.0f && Context.HorizontalSpeed < 40.0f)
	{
		State.StopGuardTimeRemaining = Settings.StopGuardDurationSec;
	}
	State.PreviousHorizontalSpeed = Context.HorizontalSpeed;
	State.StopGuardTimeRemaining = FMath::Max(0.0f, State.StopGuardTimeRemaining - Context.DeltaSeconds);
	State.StopGuardAlpha =
		(Settings.StopGuardDurationSec > 0.0f)
			? FMath::Clamp(State.StopGuardTimeRemaining / Settings.StopGuardDurationSec, 0.0f, 1.0f)
			: 0.0f;

	// Update temporal state: landing guard
	if (State.bWasFallingLastUpdate && !Context.bIsFalling)
	{
		State.LandingGuardTimeRemaining = Settings.LandingGuardDurationSec;
	}
	State.bWasFallingLastUpdate = Context.bIsFalling;
	State.LandingGuardTimeRemaining = FMath::Max(0.0f, State.LandingGuardTimeRemaining - Context.DeltaSeconds);
	State.LandingGuardAlpha =
		(Settings.LandingGuardDurationSec > 0.0f)
			? FMath::Clamp(State.LandingGuardTimeRemaining / Settings.LandingGuardDurationSec, 0.0f, 1.0f)
			: 0.0f;

	// Only compute upper chain correction when looking down past the retreat threshold
	const float LookDownAlpha = FMath::GetMappedRangeValueClamped(
		FVector2D(Settings.UpperSpineRetreatStartPitchDeg, -89.f),
		FVector2D(0.f, 1.f),
		Context.ControlPitch);

	if (LookDownAlpha <= 0.f || !Context.bHasSourceNeck || !Context.bHasSourceSpine05)
	{
		return;
	}

	const FVector DesiredNeckCameraLocal =
		Context.CameraTransform.InverseTransformPosition(Context.NeckTargetWorld);

	// Source head metrics
	FVector SourceHeadCameraLocal = FVector::ZeroVector;
	float HeadForwardExcess = 0.0f;
	float HeadStretchExcess = 0.0f;
	if (Context.bHasSourceHead)
	{
		SourceHeadCameraLocal = Context.CameraTransform.InverseTransformPosition(Context.SourceHeadWorld);
		HeadForwardExcess = FMath::Max(0.0f, SourceHeadCameraLocal.X - Settings.SourceHeadForwardGuardCm);
		HeadStretchExcess = FMath::Max(0.0f, SourceHeadCameraLocal.Size() - Settings.SourceHeadStretchGuardCm);
	}

	// Source neck metrics
	const FVector SourceNeckCameraLocal =
		Context.CameraTransform.InverseTransformPosition(Context.SourceNeckWorld);
	const float SourceNeckGapDist = FVector::Dist(Context.SourceNeckWorld, Context.NeckTargetWorld);
	const float NeckForwardExcess = FMath::Max(0.0f, SourceNeckCameraLocal.X - Settings.SourceNeckForwardGuardCm);
	const FVector SourceNeckCS =
		Context.ComponentTransform.InverseTransformPosition(Context.SourceNeckWorld);

	// Source spine_05 metrics
	const FVector SourceSpine05CameraLocal =
		Context.CameraTransform.InverseTransformPosition(Context.SourceSpine05World);
	const FVector SourceSpine05CS =
		Context.ComponentTransform.InverseTransformPosition(Context.SourceSpine05World);

	// Spine05 lean excess
	float Spine05LeanExcessDeg = 0.0f;
	{
		const FVector NeckToSpineCameraLocal =
			SourceSpine05CameraLocal - DesiredNeckCameraLocal;
		const FVector2D NeckToSpineXZ(
			NeckToSpineCameraLocal.X,
			NeckToSpineCameraLocal.Z);
		const float NeckToSpineXZLen = NeckToSpineXZ.Size();
		if (NeckToSpineXZLen > KINDA_SMALL_NUMBER)
		{
			const float DownDot = FMath::Clamp(
				-NeckToSpineCameraLocal.Z / NeckToSpineXZLen,
				-1.0f,
				1.0f);
			const float CurrentLeanDeg =
				FMath::RadiansToDegrees(FMath::Acos(DownDot));
			Spine05LeanExcessDeg =
				FMath::Max(0.0f, CurrentLeanDeg - Settings.MaxUpperChainLeanDeg);
		}
	}

	// Spine05 perpendicular excess
	const float Spine05PerpDist = FVector2D(
		SourceSpine05CameraLocal.Y,
		SourceSpine05CameraLocal.Z).Size();
	const float Spine05PerpExcess =
		FMath::Max(0.0f, Settings.MinUpperTorsoPerpDistCm - Spine05PerpDist);

	// Chain stretch risk
	const FVector SourceUpperChainCameraDelta =
		SourceSpine05CameraLocal - SourceNeckCameraLocal;
	const float SourceChainDist =
		FVector::Dist(Context.SourceSpine05World, Context.SourceNeckWorld);
	const float SourceUpperChainLenCm = SourceChainDist;
	const float DesiredChainDist =
		FVector::Dist(Context.SourceSpine05World, Context.NeckTargetWorld);
	const float ChainStretchRiskCm =
		FMath::Max(0.0f, DesiredChainDist - SourceChainDist - 4.0f);

	// Guard state multiplier
	float GuardStateMultiplier = 1.0f;
	if (Context.bIsCrouching)
	{
		GuardStateMultiplier += Settings.CrouchGuardMultiplier;
	}
	if (Context.bIsFalling)
	{
		GuardStateMultiplier += Settings.AirGuardMultiplier;
	}
	GuardStateMultiplier += State.StopGuardAlpha * Settings.StopGuardMultiplier;
	GuardStateMultiplier += State.LandingGuardAlpha * Settings.LandingGuardMultiplier;

	// Guard driver
	const float NeckGapExcess = FMath::Max(0.0f, SourceNeckGapDist - Settings.SourceNeckGapToleranceCm);
	const float StretchDriverCm = FMath::Max3(
		NeckGapExcess * 1.10f,
		FMath::Max(HeadForwardExcess * 0.70f, NeckForwardExcess),
		HeadStretchExcess * 0.45f);
	const float ChainDriverCm = FMath::Max(
		ChainStretchRiskCm,
		FMath::Max(Spine05PerpExcess, Spine05LeanExcessDeg * 0.30f));
	const float GuardDriverCm = FMath::Max(StretchDriverCm, ChainDriverCm);

	// Compute guard alpha
	const float GapSolveAlpha = FMath::GetMappedRangeValueClamped(
		FVector2D(Settings.MinUpperChainSolveGapCm, Settings.SourceNeckGapToleranceCm + 4.0f),
		FVector2D(0.0f, 1.0f),
		SourceNeckGapDist);
	const float EffectiveGuardAlpha = FMath::Clamp(
		(GuardDriverCm / 26.0f) * GuardStateMultiplier,
		0.0f,
		1.0f);
	const float GuardAlpha = FMath::Max(GapSolveAlpha, EffectiveGuardAlpha);

	// Constrain and apply upper chain
	const float AllowedForwardAngleDeg = FMath::Lerp(
		Settings.MaxUpperChainForwardAngleDeg,
		Settings.TightUpperChainForwardAngleDeg,
		GuardAlpha);
	const FVector ConstrainedUpperChainCameraDelta = ConstrainUpperChainCameraDelta(
		SourceUpperChainCameraDelta,
		SourceUpperChainLenCm,
		AllowedForwardAngleDeg,
		Settings.MinUpperChainDropCm);
	const FVector DesiredUpperChainCameraDelta = FMath::Lerp(
		SourceUpperChainCameraDelta,
		ConstrainedUpperChainCameraDelta,
		GuardAlpha);
	const FVector DesiredSpine05World =
		Context.NeckTargetWorld +
		Context.CameraTransform.TransformVectorNoScale(DesiredUpperChainCameraDelta);
	const FVector DesiredSpine05CS =
		Context.ComponentTransform.InverseTransformPosition(DesiredSpine05World);
	OutData.UpperSpineFollowCS =
		(DesiredSpine05CS - SourceSpine05CS).GetClampedToMaxSize(Settings.MaxUpperSpineFollowCm);

	// Backward pitch guard
	const float SourceForwardAngleDeg =
		ComputeUpperChainForwardAngleDeg(SourceUpperChainCameraDelta);
	const float DesiredForwardAngleDeg =
		ComputeUpperChainForwardAngleDeg(DesiredUpperChainCameraDelta);
	const float BackwardPitchGuardDeg = FMath::Max(
		0.0f,
		SourceForwardAngleDeg - DesiredForwardAngleDeg);
	OutData.UpperSpinePitchDeg = -FMath::Min(Settings.MaxUpperSpinePitchDeg, BackwardPitchGuardDeg);
	OutData.UpperSpineGuardAlpha = GuardAlpha;
}
