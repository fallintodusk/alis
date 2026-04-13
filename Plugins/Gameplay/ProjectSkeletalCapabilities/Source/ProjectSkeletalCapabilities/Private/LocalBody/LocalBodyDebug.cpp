// Debug draw and logging for LocalBody spine tracking.
// Extracted from LocalBodyAnimInstance.cpp -- identical output.

#include "LocalBodyDebug.h"
#include "ILocalBodyCorrection.h"
#include "ProjectSkeletalCapabilitiesModule.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarLocalBodyDebugDraw(
	TEXT("alis.LocalBody.DebugDraw"),
	0,
	TEXT("Enable LocalBody debug draw in PIE/runtime."));

static TAutoConsoleVariable<int32> CVarLocalBodyDebugLog(
	TEXT("alis.LocalBody.DebugLog"),
	0,
	TEXT("Enable LocalBody debug logging in PIE/runtime."));

bool LocalBodyDebug::IsDebugDrawEnabled()
{
	return CVarLocalBodyDebugDraw.GetValueOnGameThread() != 0;
}

bool LocalBodyDebug::IsDebugLogEnabled()
{
	return CVarLocalBodyDebugLog.GetValueOnGameThread() != 0;
}

void LocalBodyDebug::DrawSpineTrackingDebug(
	UWorld* World,
	const APawn* PawnOwner,
	const USkeletalMeshComponent* SourceMesh,
	const USkeletalMeshComponent* LocalBodyComp,
	const UCameraComponent* Camera,
	const FVector& CameraWorldPos,
	const FVector& NeckTargetWorld,
	const FVector& UpperSpineFollowCS,
	float UpperSpinePitchDeg,
	float UpperSpineGuardAlpha,
	const FLocalBodyFilterState& FilterState)
{
#if ENABLE_DRAW_DEBUG
	if (!World || !PawnOwner || !LocalBodyComp)
	{
		return;
	}

	const FVector CameraPos = CameraWorldPos;

	// Blue = camera position + control direction
	DrawDebugDirectionalArrow(World,
		CameraPos, CameraPos + PawnOwner->GetControlRotation().Vector() * 30.f,
		5.f, FColor::Blue, false, -1.f, 0, 2.f);

	// Red = head bone on source mesh (where MM put the head)
	if (SourceMesh)
	{
		const int32 HeadBoneIdx = SourceMesh->GetBoneIndex(FName("head"));
		if (HeadBoneIdx != INDEX_NONE)
		{
			const FVector HeadPos = SourceMesh->GetBoneTransform(HeadBoneIdx).GetLocation();
			DrawDebugDirectionalArrow(World,
				HeadPos, HeadPos + FVector::UpVector * 15.f,
				5.f, FColor::Red, false, -1.f, 0, 2.f);
		}
	}

	// Yellow = uncorrected neck from source mesh (stable reference)
	if (SourceMesh)
	{
		const int32 NeckIdx = SourceMesh->GetBoneIndex(FName("neck_01"));
		if (NeckIdx != INDEX_NONE)
		{
			const FVector NeckSrc = SourceMesh->GetBoneTransform(NeckIdx).GetLocation();
			DrawDebugDirectionalArrow(World,
				NeckSrc, NeckSrc + FVector::UpVector * 20.f,
				5.f, FColor::Yellow, false, -1.f, 0, 2.f);

			// Green = corrected neck on local body + its forward direction
			const int32 LocalNeckIdx = LocalBodyComp->GetBoneIndex(FName("neck_01"));
			FVector NeckCorrected = NeckSrc;
			if (LocalNeckIdx != INDEX_NONE)
			{
				const FTransform NeckT = LocalBodyComp->GetBoneTransform(LocalNeckIdx);
				NeckCorrected = NeckT.GetLocation();
				// Green up = corrected neck position
				DrawDebugDirectionalArrow(World,
					NeckCorrected, NeckCorrected + FVector::UpVector * 20.f,
					5.f, FColor::Green, false, -1.f, 0, 2.f);
				// Cyan = neck forward direction
				const FVector NeckFwd = NeckT.GetRotation().GetForwardVector();
				DrawDebugDirectionalArrow(World,
					NeckCorrected, NeckCorrected + NeckFwd * 25.f,
					4.f, FColor::Cyan, false, -1.f, 0, 1.5f);
			}

			// White line = camera to corrected neck (should be short when locked)
			DrawDebugLine(World,
				CameraPos, NeckCorrected,
				FColor::White, false, -1.f, 0, 1.f);

			// Detailed log
			static double LastLogTime = 0.0;
			const double Now = FPlatformTime::Seconds();
			const ACharacter* CharLog = Cast<ACharacter>(PawnOwner);
			const bool bLogEveryTick = CharLog && CharLog->GetCharacterMovement() &&
				(CharLog->GetCharacterMovement()->IsFalling() ||
				 CharLog->GetCharacterMovement()->IsCrouching() ||
				 FilterState.StopGuardTimeRemaining > 0.0f ||
				 FilterState.LandingGuardTimeRemaining > 0.0f);

			if (IsDebugLogEnabled() && (bLogEveryTick || (Now - LastLogTime > 1.0)))
			{
				LastLogTime = Now;
				const FTransform CameraTransform = Camera->GetComponentTransform();
				FVector HeadSrc = FVector::ZeroVector;
				if (SourceMesh)
				{
					const int32 HIdx = SourceMesh->GetBoneIndex(FName("head"));
					if (HIdx != INDEX_NONE)
					{
						HeadSrc = SourceMesh->GetBoneTransform(HIdx).GetLocation();
					}
				}
				const FVector HeadDrift = HeadSrc - CameraPos;
				const FVector CorrDrift = NeckCorrected - CameraPos;
				const FVector NeckGap = NeckTargetWorld - NeckSrc;
				const FVector CorrGap = NeckTargetWorld - NeckCorrected;
				const FVector SourceNeckCameraLocalLog =
					CameraTransform.InverseTransformPosition(NeckSrc);
				const FVector CorrectedNeckCameraLocalLog =
					CameraTransform.InverseTransformPosition(NeckCorrected);
				float SourceChainDist = 0.0f;
				float CorrectedChainDist = 0.0f;
				float ChainStretch = 0.0f;
				FVector SourceSpine05CameraLocal = FVector::ZeroVector;
				FVector CorrectedSpine05CameraLocal = FVector::ZeroVector;
				FVector SourceSpineToNeckCameraLocal = FVector::ZeroVector;
				FVector CorrectedSpineToNeckCameraLocal = FVector::ZeroVector;
				float SourceChainLeanDeg = 0.0f;
				float CorrectedChainLeanDeg = 0.0f;
				if (SourceMesh)
				{
					const int32 SourceSpine05Idx = SourceMesh->GetBoneIndex(FName("spine_05"));
					const int32 LocalSpine05Idx = LocalBodyComp->GetBoneIndex(FName("spine_05"));
					if (SourceSpine05Idx != INDEX_NONE && LocalSpine05Idx != INDEX_NONE)
					{
						const FVector SourceSpine05 =
							SourceMesh->GetBoneTransform(SourceSpine05Idx).GetLocation();
						const FVector CorrectedSpine05 =
							LocalBodyComp->GetBoneTransform(LocalSpine05Idx).GetLocation();
						SourceSpine05CameraLocal =
							CameraTransform.InverseTransformPosition(SourceSpine05);
						CorrectedSpine05CameraLocal =
							CameraTransform.InverseTransformPosition(CorrectedSpine05);
						SourceSpineToNeckCameraLocal =
							SourceSpine05CameraLocal - SourceNeckCameraLocalLog;
						CorrectedSpineToNeckCameraLocal =
							CorrectedSpine05CameraLocal - CorrectedNeckCameraLocalLog;
						SourceChainDist = FVector::Dist(SourceSpine05, NeckSrc);
						CorrectedChainDist = FVector::Dist(CorrectedSpine05, NeckCorrected);
						ChainStretch = CorrectedChainDist - SourceChainDist;
						const auto ComputeLeanDeg = [](const FVector& SpineCam, const FVector& NeckCam)
						{
							const FVector Delta = SpineCam - NeckCam;
							const FVector2D DeltaXZ(Delta.X, Delta.Z);
							const float DeltaXZLen = DeltaXZ.Size();
							if (DeltaXZLen <= KINDA_SMALL_NUMBER)
							{
								return 0.0f;
							}

							const float DownDot = FMath::Clamp(-Delta.Z / DeltaXZLen, -1.0f, 1.0f);
							return FMath::RadiansToDegrees(FMath::Acos(DownDot));
						};
						SourceChainLeanDeg =
							ComputeLeanDeg(SourceSpine05CameraLocal, SourceNeckCameraLocalLog);
						CorrectedChainLeanDeg =
							ComputeLeanDeg(CorrectedSpine05CameraLocal, CorrectedNeckCameraLocalLog);
					}
				}
				FString MoveState = TEXT("?");
				const ACharacter* CharOwnerDbg = Cast<ACharacter>(PawnOwner);
				if (CharOwnerDbg && CharOwnerDbg->GetCharacterMovement())
				{
					const UCharacterMovementComponent* CMCDbg = CharOwnerDbg->GetCharacterMovement();
					if (CMCDbg->IsFalling()) MoveState = TEXT("AIR");
					else if (CMCDbg->IsCrouching()) MoveState = TEXT("CROUCH");
					else MoveState = TEXT("GROUND");
				}
				const float ActorZ = PawnOwner->GetActorLocation().Z;
				UE_LOG(LogProjectSkeletalCapabilities, Log,
					TEXT("[LocalBody] [%s] ActorZ=%.0f CamZ=%.0f HeadZ=%.0f NeckZ=%.0f | Head-Actor=%.1f Cam-Actor=%.1f HeadDelta=(%.1f,%.1f,%.1f)|%.1f| NeckGap=(%.1f,%.1f,%.1f)|%.1f| S05SrcCam=(%.1f,%.1f,%.1f) S05CorrCam=(%.1f,%.1f,%.1f) S05vsNeck(src=(%.1f,%.1f,%.1f) corr=(%.1f,%.1f,%.1f)) Lean(src=%.1f corr=%.1f) Chain(src=%.1f corr=%.1f delta=%.1f) Follow=%.1f GuardPitch=%.1f GuardAlpha=%.2f Stop=%.2f Land=%.2f CorrGap=%.1f CorrDelta=%.1f"),
					*MoveState, ActorZ, CameraPos.Z, HeadSrc.Z, NeckSrc.Z,
					HeadSrc.Z - ActorZ, CameraPos.Z - ActorZ,
					HeadDrift.X, HeadDrift.Y, HeadDrift.Z, HeadDrift.Size(),
					NeckGap.X, NeckGap.Y, NeckGap.Z, NeckGap.Size(),
					SourceSpine05CameraLocal.X,
					SourceSpine05CameraLocal.Y,
					SourceSpine05CameraLocal.Z,
					CorrectedSpine05CameraLocal.X,
					CorrectedSpine05CameraLocal.Y,
					CorrectedSpine05CameraLocal.Z,
					SourceSpineToNeckCameraLocal.X,
					SourceSpineToNeckCameraLocal.Y,
					SourceSpineToNeckCameraLocal.Z,
					CorrectedSpineToNeckCameraLocal.X,
					CorrectedSpineToNeckCameraLocal.Y,
					CorrectedSpineToNeckCameraLocal.Z,
					SourceChainLeanDeg,
					CorrectedChainLeanDeg,
					SourceChainDist,
					CorrectedChainDist,
					ChainStretch,
					UpperSpineFollowCS.Size(),
					UpperSpinePitchDeg,
					UpperSpineGuardAlpha,
					FilterState.StopGuardAlpha,
					FilterState.LandingGuardAlpha,
					CorrGap.Size(),
					CorrDrift.Size());
			}
		}
	}
#endif
}

void LocalBodyDebug::LogIdleNeckOffset(
	const USkeletalMeshComponent* SourceMesh,
	const UCameraComponent* Camera,
	const FRotator& ActorRotation,
	const FVector& NeckOffsetFromCamera)
{
	if (!SourceMesh || !Camera)
	{
		return;
	}

	const int32 NeckBoneIdx = SourceMesh->GetBoneIndex(FName("neck_01"));
	if (NeckBoneIdx != INDEX_NONE)
	{
		const FVector NeckWorld = SourceMesh->GetBoneTransform(NeckBoneIdx).GetLocation();
		const FVector CamWorld = Camera->GetComponentLocation();
		const FVector WorldOffset = NeckWorld - CamWorld;
		const FVector LocalOffset = ActorRotation.UnrotateVector(WorldOffset);
		UE_LOG(LogProjectSkeletalCapabilities, Warning,
			TEXT("[LocalBody] IDLE NECK OFFSET: world=(%s) local=(%s) -- paste local into Hero.json neckOffset"),
			*WorldOffset.ToCompactString(), *LocalOffset.ToCompactString());
	}
	UE_LOG(LogProjectSkeletalCapabilities, Log,
		TEXT("[LocalBody] Spine tracking active: SourceMesh=%s NeckOffset=%s"),
		*GetNameSafe(SourceMesh), *NeckOffsetFromCamera.ToString());
}
