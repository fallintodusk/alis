// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
// Extracted from FirstPersonClipMatrixTest: CollectSample and BuildPhaseSummary.
#include "ClipMatrixSampler.h"
#include "ClipMatrixHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"

namespace ClipMatrixHelpers
{

FFrameSample CollectSample(
	ACharacter* Character,
	USkeletalMeshComponent* OwnerMesh,
	APlayerController* PC,
	int32 PhaseIdx,
	float Time,
	float RequestedPitchDeg,
	const FVector& NeckOffsetFromCamera,
	const FVector& ExpectedCameraRelativeOffset)
{
	FFrameSample S;
	S.PhaseIndex = PhaseIdx;
	S.FrameNumber = static_cast<int64>(GFrameCounter);
	S.PhaseTime = Time;
	S.RequestedPitchDeg = RequestedPitchDeg;
	S.OwnerVisibleMeshName = OwnerMesh ? OwnerMesh->GetName() : TEXT("none");

	UCameraComponent* Camera = Character->FindComponentByClass<UCameraComponent>();
	if (Camera)
	{
		S.CameraWorldPos = Camera->GetComponentLocation();
		S.CameraWorldRot = Camera->GetComponentRotation();
		S.CameraForward = Camera->GetForwardVector();
		S.CameraPitchDeg = FRotator::NormalizeAxis(S.CameraWorldRot.Pitch);
		S.CameraDownDot = FVector::DotProduct(Camera->GetForwardVector(), -FVector::UpVector);
	}

	if (PC)
	{
		S.ControlRotation = PC->GetControlRotation();
	}
	S.ActorRotation = Character->GetActorRotation();
	S.ActorForward = S.ActorRotation.Vector();
	S.CameraActorZ = S.CameraWorldPos.Z - Character->GetActorLocation().Z;
	S.ExpectedCameraActorZ = ExpectedCameraRelativeOffset.Z;
	S.CameraActorZError = S.CameraActorZ - S.ExpectedCameraActorZ;
	S.Speed = Character->GetVelocity().Size();
	S.bIsFalling = Character->GetCharacterMovement()->IsFalling();
	S.bIsCrouched = Character->bIsCrouched;
	S.CopyPoseSourceName = GetCopyPoseSourceName(OwnerMesh);
	USkeletalMeshComponent* SourceMesh = FindMeshByName(Character, S.CopyPoseSourceName);

	// Bone intrusion check
	if (OwnerMesh && OwnerMesh->GetSkeletalMeshAsset() && Camera)
	{
		const FTransform CameraTransform = Camera->GetComponentTransform();
		const bool bEvaluateDownlookMetrics =
			(RequestedPitchDeg <= -60.0f) || (S.CameraDownDot >= 0.85f);

		bool bHasSpine05 = false;
		bool bHasSpine03 = false;
		bool bHasClavicleL = false;
		bool bHasClavicleR = false;
		FVector Spine05World = FVector::ZeroVector;
		FVector Spine03World = FVector::ZeroVector;
		FVector ClavicleLWorld = FVector::ZeroVector;
		FVector ClavicleRWorld = FVector::ZeroVector;

		for (int32 i = 0; i < NumTrackedBones; ++i)
		{
			const int32 BoneIdx = OwnerMesh->GetBoneIndex(TrackedBones[i]);
			if (BoneIdx == INDEX_NONE) continue;
			// Skip hidden bones -- invisible to player
			if (OwnerMesh->IsBoneHiddenByName(TrackedBones[i])) continue;

			const FVector BoneWorldPos = OwnerMesh->GetBoneTransform(BoneIdx).GetLocation();
			if (TrackedBones[i] == FName(TEXT("spine_05")))
			{
				bHasSpine05 = true;
				Spine05World = BoneWorldPos;
			}
			else if (TrackedBones[i] == FName(TEXT("spine_03")))
			{
				bHasSpine03 = true;
				Spine03World = BoneWorldPos;
			}
			else if (TrackedBones[i] == FName(TEXT("clavicle_l")))
			{
				bHasClavicleL = true;
				ClavicleLWorld = BoneWorldPos;
			}
			else if (TrackedBones[i] == FName(TEXT("clavicle_r")))
			{
				bHasClavicleR = true;
				ClavicleRWorld = BoneWorldPos;
			}

			const FVector CameraLocalPos = CameraTransform.InverseTransformPosition(BoneWorldPos);
			const float Dist = FVector::Dist(S.CameraWorldPos, BoneWorldPos);

			FBoneIntrusion BI;
			BI.BoneName = TrackedBones[i];
			BI.CameraLocalPos = CameraLocalPos;
			BI.DistFromCamera = Dist;
			BI.bInForbiddenVolume = (Dist < ForbiddenRadiusCm);
			S.Bones.Add(BI);

			if (BI.bInForbiddenVolume)
			{
				++S.IntrusionCount;
			}
		}

		if (bHasSpine05)
		{
			FVector ProxyWorld = Spine05World;
			if (bHasClavicleL && bHasClavicleR)
			{
				ProxyWorld =
					(Spine05World * 0.55f) +
					(ClavicleLWorld * 0.225f) +
					(ClavicleRWorld * 0.225f);
			}

			S.UpperTorsoProxyWorld = ProxyWorld;
			const FVector RayOrigin = S.CameraWorldPos;
			const FVector RayDir = Camera->GetForwardVector();
			const FVector CamToProxy = ProxyWorld - RayOrigin;
			const float ProxyProj = FVector::DotProduct(CamToProxy, RayDir);
			S.UpperTorsoProxyCameraLocal = CameraTransform.InverseTransformPosition(ProxyWorld);
			if (ProxyProj > 0.f)
			{
				const FVector ClosestOnRay = RayOrigin + RayDir * ProxyProj;
				S.UpperTorsoProxyPerpDist = FVector::Dist(ClosestOnRay, ProxyWorld);
				if (S.UpperTorsoProxyPerpDist < UpperTorsoProxyRadiusCm)
				{
					S.bUpperTorsoProxyRayHit = true;
					S.UpperTorsoProxyRayDist = ProxyProj;
				}
			}

			if (bHasSpine03)
			{
				FVector CapsuleStart = Spine03World;
				FVector CapsuleEnd = Spine05World;
				if (bHasClavicleL && bHasClavicleR)
				{
					CapsuleEnd = (ClavicleLWorld + ClavicleRWorld) * 0.5f;
				}

				S.UpperTorsoCapsuleStartWorld = CapsuleStart;
				S.UpperTorsoCapsuleEndWorld = CapsuleEnd;
				S.UpperTorsoCapsuleStartCameraLocal =
					CameraTransform.InverseTransformPosition(CapsuleStart);
				S.UpperTorsoCapsuleEndCameraLocal =
					CameraTransform.InverseTransformPosition(CapsuleEnd);
				S.UpperTorsoCapsuleCameraDist = ComputePointToSegmentDistance(
					S.CameraWorldPos,
					CapsuleStart,
					CapsuleEnd);
				S.bUpperTorsoCapsuleCameraInside =
					S.UpperTorsoCapsuleCameraDist < UpperTorsoCapsuleRadiusCm;
				EvaluateRaySegmentCapsule(
					RayOrigin,
					RayDir,
					CameraProbeLengthCm,
					CapsuleStart,
					CapsuleEnd,
					UpperTorsoCapsuleRadiusCm,
					S.bUpperTorsoCapsuleRayHit,
					S.UpperTorsoCapsuleRayDist,
					S.UpperTorsoCapsulePerpDist);
			}
		}

		if (SourceMesh && SourceMesh->GetSkeletalMeshAsset())
		{
			const FVector DesiredNeckWorld =
				S.CameraWorldPos +
				Character->GetActorRotation().RotateVector(NeckOffsetFromCamera);
			S.DesiredNeckWorld = DesiredNeckWorld;
			S.DesiredNeckCameraLocal = CameraTransform.InverseTransformPosition(DesiredNeckWorld);
			const int32 SourceHeadIdx = SourceMesh->GetBoneIndex(FName(TEXT("head")));
			const int32 SourceNeckIdx = SourceMesh->GetBoneIndex(FName(TEXT("neck_01")));
			const int32 SourceSpine05Idx = SourceMesh->GetBoneIndex(FName(TEXT("spine_05")));
			const int32 CorrectedNeckIdx = OwnerMesh->GetBoneIndex(FName(TEXT("neck_01")));
			const int32 CorrectedSpine05Idx = OwnerMesh->GetBoneIndex(FName(TEXT("spine_05")));

			if (SourceHeadIdx != INDEX_NONE)
			{
				const FVector SourceHeadWorld =
					SourceMesh->GetBoneTransform(SourceHeadIdx).GetLocation();
				S.SourceHeadCameraLocal =
					CameraTransform.InverseTransformPosition(SourceHeadWorld);
				S.SourceHeadCameraDist = FVector::Dist(SourceHeadWorld, S.CameraWorldPos);
				S.SourceHeadActorZ = SourceHeadWorld.Z - Character->GetActorLocation().Z;
				S.CameraHeadZDelta = S.CameraWorldPos.Z - SourceHeadWorld.Z;
				S.bHeadStretchExceeded =
					bEvaluateDownlookMetrics &&
					(S.SourceHeadCameraDist > SourceHeadStretchFailCm);
			}

			if (SourceNeckIdx != INDEX_NONE)
			{
				const FVector SourceNeckWorld =
					SourceMesh->GetBoneTransform(SourceNeckIdx).GetLocation();
				S.SourceNeckWorld = SourceNeckWorld;
				S.SourceNeckCameraLocal =
					CameraTransform.InverseTransformPosition(SourceNeckWorld);
				S.SourceNeckActorZ = SourceNeckWorld.Z - Character->GetActorLocation().Z;
				S.CameraNeckZDelta = S.CameraWorldPos.Z - SourceNeckWorld.Z;
				S.SourceNeckTargetGap = DesiredNeckWorld - SourceNeckWorld;
				S.SourceNeckTargetGapDist = S.SourceNeckTargetGap.Size();
				S.bNeckGapExceeded =
					bEvaluateDownlookMetrics &&
					(S.SourceNeckTargetGapDist > SourceNeckGapFailCm);
			}

			if (CorrectedNeckIdx != INDEX_NONE)
			{
				const FVector CorrectedNeckWorld =
					OwnerMesh->GetBoneTransform(CorrectedNeckIdx).GetLocation();
				S.CorrectedNeckWorld = CorrectedNeckWorld;
				S.CorrectedNeckCameraLocal =
					CameraTransform.InverseTransformPosition(CorrectedNeckWorld);
				S.CorrectedNeckTargetGap = DesiredNeckWorld - CorrectedNeckWorld;
				S.CorrectedNeckTargetGapDist = S.CorrectedNeckTargetGap.Size();

				if (SourceNeckIdx != INDEX_NONE && SourceSpine05Idx != INDEX_NONE &&
					CorrectedSpine05Idx != INDEX_NONE)
				{
					const FVector SourceNeckWorld =
						SourceMesh->GetBoneTransform(SourceNeckIdx).GetLocation();
					const FVector SourceSpine05World =
						SourceMesh->GetBoneTransform(SourceSpine05Idx).GetLocation();
					const FVector CorrectedSpine05World =
						OwnerMesh->GetBoneTransform(CorrectedSpine05Idx).GetLocation();
					S.SourceSpine05World = SourceSpine05World;
					S.CorrectedSpine05World = CorrectedSpine05World;
					S.SourceUpperChainCameraDelta =
						CameraTransform.InverseTransformPosition(SourceSpine05World) -
						S.SourceNeckCameraLocal;
					S.CorrectedSpine05CameraLocal =
						CameraTransform.InverseTransformPosition(CorrectedSpine05World);
					S.CorrectedUpperChainCameraDelta =
						S.CorrectedSpine05CameraLocal - S.CorrectedNeckCameraLocal;
					S.SourceUpperLeanDeg =
						ComputeUpperChainLeanDeg(S.SourceUpperChainCameraDelta);
					S.CorrectedUpperLeanDeg =
						ComputeUpperChainLeanDeg(S.CorrectedUpperChainCameraDelta);
					S.SourceUpperForwardCm = S.SourceUpperChainCameraDelta.X;
					S.CorrectedUpperForwardCm = S.CorrectedUpperChainCameraDelta.X;
					S.UpperForwardDeltaCm =
						S.CorrectedUpperForwardCm - S.SourceUpperForwardCm;
					S.SourceNeckChainDist = FVector::Dist(SourceSpine05World, SourceNeckWorld);
					S.CorrectedNeckChainDist = FVector::Dist(CorrectedSpine05World, CorrectedNeckWorld);
					S.CorrectedNeckChainErrorDist = FMath::Abs(
						S.CorrectedNeckChainDist - S.SourceNeckChainDist);
					S.bCorrectedNeckChainErrorExceeded =
						bEvaluateDownlookMetrics &&
						(S.CorrectedNeckChainErrorDist > CorrectedNeckChainErrorFailCm);
					const float AheadExcess =
						S.CorrectedUpperChainCameraDelta.X - S.SourceUpperChainCameraDelta.X;
					const float AboveExcess =
						S.CorrectedUpperChainCameraDelta.Z - S.SourceUpperChainCameraDelta.Z;
					S.bCorrectedUpperChainFoldExceeded =
						bEvaluateDownlookMetrics &&
						((AheadExcess > CorrectedUpperChainAheadSlackCm) ||
						 (AboveExcess > CorrectedUpperChainAboveSlackCm));
				}
			}
		}

		// Line trace from camera forward toward the body.
		// Instead of tracing against the mesh (which has no collision),
		// check if any tracked bone's SPHERE (neck hole area) intersects
		// with the camera forward ray. This is equivalent to: "does the
		// camera see the neck hole area?"
		//
		// For each tracked bone, test if the camera forward ray passes
		// within BoneSphereRadius of the bone center. That means the
		// player is looking directly at (or through) that bone area.
		// spine_05 sphere must be large enough to cover the entire neck hole
		// opening, not just the bone center. The hole extends ~20cm beyond
		// the bone in all directions (shoulder width / 2).
		static constexpr float NeckStumpSphereRadius = 25.f;
		static constexpr float BodySphereRadius = 8.f;

		const FVector RayOrigin = S.CameraWorldPos;
		const FVector RayDir = Camera->GetForwardVector();

		// Ray-sphere check against the OWNER-VISIBLE mesh (LocalBody),
		// not the source mesh. This is what the player actually sees.
		// Hidden bones (head, neck_01) are scaled to zero so their
		// positions collapse -- the ray won't hit them. spine_05 is
		// the last visible bone and the one our fix rotates.
		if (OwnerMesh && OwnerMesh->GetSkeletalMeshAsset())
		{
			for (int32 i = 0; i < NumTrackedBones; ++i)
			{
				const int32 BoneIdx = OwnerMesh->GetBoneIndex(TrackedBones[i]);
				if (BoneIdx == INDEX_NONE) continue;

				// Skip hidden bones -- they're scaled to zero for rendering,
				// invisible to the player, so ray hits on them are false positives.
				if (OwnerMesh->IsBoneHiddenByName(TrackedBones[i])) continue;

				const FVector BoneWorld = OwnerMesh->GetBoneTransform(BoneIdx).GetLocation();
				const float Radius = (TrackedBones[i] == FName("spine_05"))
					? NeckStumpSphereRadius : BodySphereRadius;

				// Point-to-ray distance: does the camera ray pass within Radius of this bone?
				const FVector CamToBone = BoneWorld - RayOrigin;
				const float Proj = FVector::DotProduct(CamToBone, RayDir);

				// Only count if bone is in front of camera (Proj > 0)
				if (Proj > 0.f)
				{
					const FVector ClosestOnRay = RayOrigin + RayDir * Proj;
					const float RayBoneDist = FVector::Dist(ClosestOnRay, BoneWorld);

					if (RayBoneDist < Radius)
					{
						S.bCameraRayHitsBody = true;
						if (Proj < S.CameraRayHitDist || S.CameraRayHitDist < 0.f)
						{
							S.CameraRayHitDist = Proj;
							S.CameraRayHitBone = TrackedBones[i].ToString();
						}
					}
				}
			}
		}
	}

	// Compute severity: sum of intrusion depths + ray hit bonus
	for (const FBoneIntrusion& BI : S.Bones)
	{
		if (BI.bInForbiddenVolume)
		{
			S.Severity += ForbiddenRadiusCm - BI.DistFromCamera;
		}
	}
	if (S.bCameraRayHitsBody && S.CameraRayHitDist >= 0.f)
	{
		S.Severity += 20.f; // large bonus for ray hit
	}
	if (S.bUpperTorsoProxyRayHit && S.UpperTorsoProxyRayDist >= 0.f)
	{
		S.Severity += 20.f;
	}
	if (S.bUpperTorsoCapsuleCameraInside && S.UpperTorsoCapsuleCameraDist >= 0.f)
	{
		S.Severity += 20.f + (UpperTorsoCapsuleRadiusCm - S.UpperTorsoCapsuleCameraDist);
	}
	if (S.bUpperTorsoCapsuleRayHit && S.UpperTorsoCapsuleRayDist >= 0.f)
	{
		S.Severity += 20.f;
	}
	if (S.bHeadStretchExceeded)
	{
		S.Severity += 6.f + S.SourceHeadCameraDist;
	}
	if (S.bNeckGapExceeded)
	{
		S.Severity += 6.f + S.SourceNeckTargetGapDist;
	}
	if (S.bCorrectedNeckChainErrorExceeded)
	{
		S.Severity += 10.f + S.CorrectedNeckChainErrorDist;
	}
	if (S.bCorrectedUpperChainFoldExceeded)
	{
		const float AheadExcess =
			FMath::Max(0.0f, S.CorrectedUpperChainCameraDelta.X - S.SourceUpperChainCameraDelta.X);
		const float AboveExcess =
			FMath::Max(0.0f, S.CorrectedUpperChainCameraDelta.Z - S.SourceUpperChainCameraDelta.Z);
		S.Severity +=
			10.f +
			AheadExcess +
			AboveExcess;
	}

	return S;
}

FPhaseSummary BuildPhaseSummary(
	const TArray<FFrameSample>& AllSamples,
	int32 PhaseIdx,
	const FClipPhase* ActivePhases,
	int32 ActivePhaseCount)
{
	const FClipPhase& Phase = ActivePhases[PhaseIdx];
	FPhaseSummary Sum;
	Sum.Name = Phase.Name;

	int32 ConsecutiveRun = 0;
	int32 MaxRun = 0;

	for (const FFrameSample& S : AllSamples)
	{
		if (S.PhaseIndex != PhaseIdx) continue;
		++Sum.SampleCount;
		Sum.ExpectedCameraActorZ = S.ExpectedCameraActorZ;
		Sum.MinCameraActorZ = FMath::Min(Sum.MinCameraActorZ, S.CameraActorZ);
		Sum.MaxCameraActorZ = FMath::Max(Sum.MaxCameraActorZ, S.CameraActorZ);
		Sum.MaxAbsCameraActorZError = FMath::Max(
			Sum.MaxAbsCameraActorZError,
			FMath::Abs(S.CameraActorZError));
		Sum.MinCameraHeadZDelta = FMath::Min(Sum.MinCameraHeadZDelta, S.CameraHeadZDelta);
		Sum.MaxCameraHeadZDelta = FMath::Max(Sum.MaxCameraHeadZDelta, S.CameraHeadZDelta);
		Sum.MinCameraNeckZDelta = FMath::Min(Sum.MinCameraNeckZDelta, S.CameraNeckZDelta);
		Sum.MaxCameraNeckZDelta = FMath::Max(Sum.MaxCameraNeckZDelta, S.CameraNeckZDelta);
		Sum.MaxSourceUpperLeanDeg = FMath::Max(Sum.MaxSourceUpperLeanDeg, S.SourceUpperLeanDeg);
		Sum.MaxCorrectedUpperLeanDeg = FMath::Max(Sum.MaxCorrectedUpperLeanDeg, S.CorrectedUpperLeanDeg);
		Sum.MaxSourceUpperForwardCm = FMath::Max(Sum.MaxSourceUpperForwardCm, S.SourceUpperForwardCm);
		Sum.MaxCorrectedUpperForwardCm = FMath::Max(
			Sum.MaxCorrectedUpperForwardCm,
			S.CorrectedUpperForwardCm);
		Sum.MaxUpperForwardDeltaCm = FMath::Max(
			Sum.MaxUpperForwardDeltaCm,
			S.UpperForwardDeltaCm);

		if (S.IntrusionCount > 0)
		{
			++Sum.TotalIntrusionFrames;
			++ConsecutiveRun;
			if (ConsecutiveRun > MaxRun)
			{
				MaxRun = ConsecutiveRun;
			}
		}
		else
		{
			ConsecutiveRun = 0;
		}

		for (const FBoneIntrusion& BI : S.Bones)
		{
			if (BI.bInForbiddenVolume && BI.DistFromCamera < Sum.WorstIntrusionDistCm)
			{
				Sum.WorstIntrusionDistCm = BI.DistFromCamera;
				Sum.WorstBone = BI.BoneName;
			}
		}
	}

	Sum.MaxConsecutiveIntrusions = MaxRun;

	// Camera ray hit tracking
	int32 RayConsecutiveRun = 0;
	int32 RayMaxRun = 0;
	int32 ProxyConsecutiveRun = 0;
	int32 ProxyMaxRun = 0;
	int32 CapsuleInsideConsecutiveRun = 0;
	int32 CapsuleInsideMaxRun = 0;
	int32 CapsuleRayConsecutiveRun = 0;
	int32 CapsuleRayMaxRun = 0;
	int32 HeadStretchConsecutiveRun = 0;
	int32 HeadStretchMaxRun = 0;
	int32 NeckGapConsecutiveRun = 0;
	int32 NeckGapMaxRun = 0;
	int32 CorrectedChainConsecutiveRun = 0;
	int32 CorrectedChainMaxRun = 0;
	int32 CorrectedFoldConsecutiveRun = 0;
	int32 CorrectedFoldMaxRun = 0;
	for (const FFrameSample& S : AllSamples)
	{
		if (S.PhaseIndex != PhaseIdx) continue;
		if (S.bCameraRayHitsBody)
		{
			++Sum.CameraRayHitFrames;
			++RayConsecutiveRun;
			if (RayConsecutiveRun > RayMaxRun) RayMaxRun = RayConsecutiveRun;
			if (S.CameraRayHitDist < Sum.NearestRayHitDist)
			{
				Sum.NearestRayHitDist = S.CameraRayHitDist;
				Sum.NearestRayHitBone = S.CameraRayHitBone;
			}
		}
		else
		{
			RayConsecutiveRun = 0;
		}

		if (S.bUpperTorsoProxyRayHit)
		{
			++Sum.ProxyRayHitFrames;
			++ProxyConsecutiveRun;
			if (ProxyConsecutiveRun > ProxyMaxRun) ProxyMaxRun = ProxyConsecutiveRun;
			if (S.UpperTorsoProxyRayDist < Sum.NearestProxyRayHitDist)
			{
				Sum.NearestProxyRayHitDist = S.UpperTorsoProxyRayDist;
			}
		}
		else
		{
			ProxyConsecutiveRun = 0;
		}

		if (S.UpperTorsoProxyPerpDist >= 0.f && S.UpperTorsoProxyPerpDist < Sum.MinProxyPerpDist)
		{
			Sum.MinProxyPerpDist = S.UpperTorsoProxyPerpDist;
		}

		if (S.bUpperTorsoCapsuleCameraInside)
		{
			++Sum.CapsuleCameraInsideFrames;
			++CapsuleInsideConsecutiveRun;
			if (CapsuleInsideConsecutiveRun > CapsuleInsideMaxRun)
			{
				CapsuleInsideMaxRun = CapsuleInsideConsecutiveRun;
			}
		}
		else
		{
			CapsuleInsideConsecutiveRun = 0;
		}

		if (S.UpperTorsoCapsuleCameraDist >= 0.f && S.UpperTorsoCapsuleCameraDist < Sum.MinCapsuleCameraDist)
		{
			Sum.MinCapsuleCameraDist = S.UpperTorsoCapsuleCameraDist;
		}

		if (S.bUpperTorsoCapsuleRayHit)
		{
			++Sum.CapsuleRayHitFrames;
			++CapsuleRayConsecutiveRun;
			if (CapsuleRayConsecutiveRun > CapsuleRayMaxRun)
			{
				CapsuleRayMaxRun = CapsuleRayConsecutiveRun;
			}
			if (S.UpperTorsoCapsuleRayDist < Sum.NearestCapsuleRayHitDist)
			{
				Sum.NearestCapsuleRayHitDist = S.UpperTorsoCapsuleRayDist;
			}
		}
		else
		{
			CapsuleRayConsecutiveRun = 0;
		}

		if (S.UpperTorsoCapsulePerpDist >= 0.f && S.UpperTorsoCapsulePerpDist < Sum.MinCapsuleRayPerpDist)
		{
			Sum.MinCapsuleRayPerpDist = S.UpperTorsoCapsulePerpDist;
		}

		if (S.SourceHeadCameraDist > Sum.MaxSourceHeadCameraDist)
		{
			Sum.MaxSourceHeadCameraDist = S.SourceHeadCameraDist;
		}
		if (S.SourceNeckTargetGapDist > Sum.MaxSourceNeckTargetGapDist)
		{
			Sum.MaxSourceNeckTargetGapDist = S.SourceNeckTargetGapDist;
		}
		if (S.CorrectedNeckTargetGapDist > Sum.MaxCorrectedNeckTargetGapDist)
		{
			Sum.MaxCorrectedNeckTargetGapDist = S.CorrectedNeckTargetGapDist;
		}
		if (S.CorrectedNeckChainErrorDist > Sum.MaxCorrectedNeckChainErrorDist)
		{
			Sum.MaxCorrectedNeckChainErrorDist = S.CorrectedNeckChainErrorDist;
		}
		const float AheadExcess = FMath::Max(
			0.0f,
			S.CorrectedUpperChainCameraDelta.X - S.SourceUpperChainCameraDelta.X);
		const float AboveExcess = FMath::Max(
			0.0f,
			S.CorrectedUpperChainCameraDelta.Z - S.SourceUpperChainCameraDelta.Z);
		if (AheadExcess > Sum.MaxCorrectedUpperChainAheadDist)
		{
			Sum.MaxCorrectedUpperChainAheadDist = AheadExcess;
		}
		if (AboveExcess > Sum.MaxCorrectedUpperChainAboveDist)
		{
			Sum.MaxCorrectedUpperChainAboveDist = AboveExcess;
		}

		if (S.bHeadStretchExceeded)
		{
			++Sum.HeadStretchFrames;
			++HeadStretchConsecutiveRun;
			if (HeadStretchConsecutiveRun > HeadStretchMaxRun)
			{
				HeadStretchMaxRun = HeadStretchConsecutiveRun;
			}
		}
		else
		{
			HeadStretchConsecutiveRun = 0;
		}

		if (S.bNeckGapExceeded)
		{
			++Sum.NeckGapFrames;
			++NeckGapConsecutiveRun;
			if (NeckGapConsecutiveRun > NeckGapMaxRun)
			{
				NeckGapMaxRun = NeckGapConsecutiveRun;
			}
		}
		else
		{
			NeckGapConsecutiveRun = 0;
		}

		if (S.bCorrectedNeckChainErrorExceeded)
		{
			++Sum.CorrectedNeckChainErrorFrames;
			++CorrectedChainConsecutiveRun;
			if (CorrectedChainConsecutiveRun > CorrectedChainMaxRun)
			{
				CorrectedChainMaxRun = CorrectedChainConsecutiveRun;
			}
		}
		else
		{
			CorrectedChainConsecutiveRun = 0;
		}

		if (S.bCorrectedUpperChainFoldExceeded)
		{
			++Sum.CorrectedUpperChainFoldFrames;
			++CorrectedFoldConsecutiveRun;
			if (CorrectedFoldConsecutiveRun > CorrectedFoldMaxRun)
			{
				CorrectedFoldMaxRun = CorrectedFoldConsecutiveRun;
			}
		}
		else
		{
			CorrectedFoldConsecutiveRun = 0;
		}
	}
	Sum.MaxConsecutiveRayHits = RayMaxRun;
	Sum.MaxConsecutiveProxyRayHits = ProxyMaxRun;
	Sum.MaxConsecutiveCapsuleCameraInside = CapsuleInsideMaxRun;
	Sum.MaxConsecutiveCapsuleRayHits = CapsuleRayMaxRun;
	Sum.MaxConsecutiveHeadStretch = HeadStretchMaxRun;
	Sum.MaxConsecutiveNeckGap = NeckGapMaxRun;
	Sum.MaxConsecutiveCorrectedNeckChainError = CorrectedChainMaxRun;
	Sum.MaxConsecutiveCorrectedUpperChainFold = CorrectedFoldMaxRun;

	// Track worst severity frame for screenshot burst
	for (int32 i = 0; i < AllSamples.Num(); ++i)
	{
		if (AllSamples[i].PhaseIndex == PhaseIdx && AllSamples[i].Severity > Sum.PeakSeverity)
		{
			Sum.PeakSeverity = AllSamples[i].Severity;
			Sum.WorstSampleIndex = i;
		}
	}

	// Fail if EITHER bone intrusion OR camera ray hits body
	Sum.bFailed = (MaxRun >= ConsecutiveFailThreshold)
		|| (RayMaxRun >= ConsecutiveFailThreshold)
		|| (ProxyMaxRun >= ConsecutiveFailThreshold)
		|| (CapsuleInsideMaxRun >= ConsecutiveFailThreshold)
		|| (CapsuleRayMaxRun >= ConsecutiveFailThreshold)
		|| (HeadStretchMaxRun >= ConsecutiveFailThreshold)
		|| (NeckGapMaxRun >= ConsecutiveFailThreshold)
		|| (CorrectedChainMaxRun >= ConsecutiveFailThreshold)
		|| (CorrectedFoldMaxRun >= ConsecutiveFailThreshold);

	return Sum;
}

} // namespace ClipMatrixHelpers
