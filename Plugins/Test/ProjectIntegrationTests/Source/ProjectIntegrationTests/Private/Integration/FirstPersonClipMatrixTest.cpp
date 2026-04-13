// Copyright ALIS. All Rights Reserved.
// FirstPersonClipMatrix: deterministic test for body-into-camera intrusion.
//
// Runs scripted movement phases and measures whether tracked bone points
// enter a forbidden volume around the camera. This is the real regression
// test for first-person body clipping -- not "did bones rotate" but
// "did body geometry enter the camera volume."
//
// Output: Saved/Validation/ClipMatrix/ (JSONL timeline + summary JSON)

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "UnrealClient.h"
#include "DrawDebugHelpers.h"

#include "ClipMatrix/ClipMatrixTypes.h"
#include "ClipMatrix/ClipMatrixHelpers.h"
#include "ClipMatrix/ClipMatrixSampler.h"
#include "ClipMatrix/ClipMatrixReportWriter.h"

namespace ClipMatrixHelpers
{

// Latent command: thin coordinator that delegates to extracted helpers
class FClipMatrixCommand : public IAutomationLatentCommand
{
public:
	// UpperChainModeOverride: INDEX_NONE = leave runtime default,
	// 0 = force Disabled (baseline), 1 = force FilterV1.
	explicit FClipMatrixCommand(
		FAutomationTestBase* InTest,
		EClipMatrixLayerMode InMode = EClipMatrixLayerMode::LocalBody_Corrected,
		EClipMatrixScenario InScenario = EClipMatrixScenario::FullMatrix,
		int32 InUpperChainModeOverride = INDEX_NONE)
		: Test(InTest)
		, LayerMode(InMode)
		, Scenario(InScenario)
		, UpperChainModeOverride(InUpperChainModeOverride)
		, RunId(FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")))
		, OutputDir(FPaths::ProjectSavedDir() / TEXT("Validation/ClipMatrix"))
	{
		ResolveScenarioPhases();
		FScreenshotRequest::OnScreenshotRequestProcessed().AddRaw(
			this,
			&FClipMatrixCommand::HandleScreenshotRequestProcessed);
	}

	~FClipMatrixCommand() override
	{
		FScreenshotRequest::OnScreenshotRequestProcessed().RemoveAll(this);
	}

	virtual bool Update() override
	{
		if (!Test) return true;
		const uint64 Frame = GFrameCounter;
		if (Frame == LastFrame) return false;
		LastFrame = Frame;

		switch (Stage)
		{
		case 0: return WaitForPawn();
		case 1: return DumpMeshIdentity();
		case 2: return ApplyLayerIsolation();
		case 3: return RunMeasurementPhases();
		case 4: return PrepareArtifactReplay();
		case 5: return RunArtifactReplay();
		case 6: return FinalizeArtifactReplay();
		case 7: return WriteResults();
		default: return true;
		}
	}

private:
	FAutomationTestBase* Test = nullptr;
	EClipMatrixLayerMode LayerMode;
	EClipMatrixScenario Scenario;
	int32 UpperChainModeOverride = INDEX_NONE;
	FString RunId;
	FString OutputDir;
	uint64 LastFrame = 0;
	int32 Stage = 0;
	float SettleTime = 0.f;

	ACharacter* Character = nullptr;
	APlayerController* PC = nullptr;
	USkeletalMeshComponent* OwnerMesh = nullptr;

	int32 CurrentPhase = -1;
	float PhaseElapsed = 0.f;
	int32 ConsecutiveIntrusionFrames = 0;
	bool bScreenshotTakenThisPhase = false;
	bool bDetailedIssueLoggedThisPhase = false;
	TArray<FFrameSample> AllSamples;
	TArray<FPhaseSummary> PhaseSummaries;
	TArray<FString> TimelineLines;
	bool bSprintActive = false;
	bool bUpperChainModeApplied = false;
	FVector NeckOffsetFromCamera = LoadNeckOffsetFromHeroJson();
	FVector ExpectedCameraRelativeOffset = LoadViewRelativeOffsetFromHeroJson();
	FTransform InitialCharacterTransform = FTransform::Identity;
	FRotator InitialControlRotation = FRotator::ZeroRotator;
	float InitialCapsuleHalfHeight = 0.f;
	TArray<FArtifactReplayTarget> ArtifactReplayTargets;
	int32 PendingArtifactCaptureIndex = INDEX_NONE;
	float ReplayWarmupRemaining = 0.f;
	bool bScreenshotProcessedSignal = false;
	const FClipPhase* ActivePhases = GPhases;
	int32 ActivePhaseCount = GNumPhases;

	static constexpr float MaxSettleSec = 15.f;

	void HandleScreenshotRequestProcessed()
	{
		bScreenshotProcessedSignal = true;
	}

	void ResolveScenarioPhases()
	{
		switch (Scenario)
		{
		case EClipMatrixScenario::SprintStopLoop:
			ActivePhases = GSprintStopLoopPhases;
			ActivePhaseCount = GNumSprintStopLoopPhases;
			break;
		case EClipMatrixScenario::FullMatrix:
		default:
			ActivePhases = GPhases;
			ActivePhaseCount = GNumPhases;
			break;
		}
	}

	bool ShouldCaptureArtifactsForPhase(const FString& PhaseName) const
	{
		if (Scenario == EClipMatrixScenario::SprintStopLoop)
		{
			return PhaseName.Contains(TEXT("SprintStop"));
		}

		return PhaseName == TEXT("SprintStop_MaxDown")
			|| PhaseName == TEXT("RunJumpLand_MaxDown")
			|| PhaseName == TEXT("CrouchRun_MaxDown");
	}

	float ResolveRequestedPitch(const FClipPhase& Phase) const
	{
		float ResolvedPitch = Phase.TargetPitchDeg;
		if (PC && PC->PlayerCameraManager)
		{
			const float PitchMin = FRotator::NormalizeAxis(PC->PlayerCameraManager->ViewPitchMin);
			ResolvedPitch = FMath::Max(ResolvedPitch, PitchMin);
		}
		return ResolvedPitch;
	}

	void ApplyPhaseInputs(const FClipPhase& Phase)
	{
		if (!PC || !Character) return;

		FRotator CR = PC->GetControlRotation();
		CR.Pitch = ResolveRequestedPitch(Phase);
		PC->SetControlRotation(CR);

		if (!Phase.MoveInput.IsNearlyZero())
		{
			const FRotator YawRot(0, CR.Yaw, 0);
			Character->AddMovementInput(
				FRotationMatrix(YawRot).GetUnitAxis(EAxis::X), Phase.MoveInput.X);
			Character->AddMovementInput(
				FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y), Phase.MoveInput.Y);
		}

		if (Phase.bCrouch && !Character->bIsCrouched)
		{
			Character->Crouch();
		}
		else if (!Phase.bCrouch && Character->bIsCrouched)
		{
			Character->UnCrouch();
		}

		SetSprintState(Phase.bSprint);

		if (Phase.bJump)
		{
			Character->Jump();
		}
	}

	void SetSprintState(bool bEnable)
	{
		if (!Character)
		{
			return;
		}

		if (UFunction* Func = Character->FindFunction(
			bEnable ? FName(TEXT("StartSprint")) : FName(TEXT("StopSprint"))))
		{
			if (bSprintActive != bEnable)
			{
				Character->ProcessEvent(Func, nullptr);
			}
		}
		else if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
		{
			const float DesiredSpeed = bEnable ? 700.f : (Character->bIsCrouched ? 225.f : 500.f);
			if (!FMath::IsNearlyEqual(MoveComp->MaxWalkSpeed, DesiredSpeed))
			{
				MoveComp->MaxWalkSpeed = DesiredSpeed;
			}
		}

		bSprintActive = bEnable;
	}

	void ResetCharacterForReplay()
	{
		if (!Character || !PC)
		{
			return;
		}

		SetSprintState(false);
		Character->StopJumping();
		if (Character->bIsCrouched)
		{
			Character->UnCrouch();
		}

		if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
		{
			MoveComp->StopMovementImmediately();
			MoveComp->Velocity = FVector::ZeroVector;
			MoveComp->UpdateComponentVelocity();
			MoveComp->bForceNextFloorCheck = true;
		}

		Character->SetActorLocationAndRotation(
			InitialCharacterTransform.GetLocation(),
			InitialCharacterTransform.GetRotation(),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		PC->SetControlRotation(InitialControlRotation);
	}

	void DrawArtifactReplayDebug(const FFrameSample& Sample) const
	{
		if (!Character) return;
		UWorld* World = Character->GetWorld();
		if (!World) return;

		const FVector RayOrigin = Sample.CameraWorldPos;
		const FVector RayDir = Sample.CameraWorldRot.Vector();
		DrawDebugLine(World, RayOrigin, RayOrigin + RayDir * CameraProbeLengthCm,
			FColor::Red, false, ArtifactDebugDrawWindowSec, 0, 1.5f);
		DrawDebugDirectionalArrow(World,
			Character->GetActorLocation(),
			Character->GetActorLocation() + Sample.ActorForward * 35.0f,
			8.0f, FColor::Orange, false, ArtifactDebugDrawWindowSec, 0, 1.5f);
		DrawDebugDirectionalArrow(World,
			RayOrigin, RayOrigin + FVector::UpVector * 25.0f,
			6.0f, FColor::White, false, ArtifactDebugDrawWindowSec, 0, 1.2f);

		if (!Sample.UpperTorsoProxyWorld.IsZero())
		{
			DrawDebugSphere(World, Sample.UpperTorsoProxyWorld, UpperTorsoProxyRadiusCm,
				16, FColor::Yellow, false, ArtifactDebugDrawWindowSec, 0, 1.2f);
		}
		if (!Sample.UpperTorsoCapsuleStartWorld.IsZero() || !Sample.UpperTorsoCapsuleEndWorld.IsZero())
		{
			DrawDebugSegmentCapsule(World,
				Sample.UpperTorsoCapsuleStartWorld, Sample.UpperTorsoCapsuleEndWorld,
				UpperTorsoCapsuleRadiusCm,
				(Sample.bUpperTorsoCapsuleCameraInside || Sample.bUpperTorsoCapsuleRayHit)
					? FColor::Cyan : FColor::Blue,
				ArtifactDebugDrawWindowSec);
		}
		if (!Sample.SourceNeckWorld.IsZero() || !Sample.SourceSpine05World.IsZero())
		{
			DrawDebugDirectionalArrow(World,
				Sample.SourceNeckWorld, Sample.SourceSpine05World,
				7.0f, FColor::Magenta, false, ArtifactDebugDrawWindowSec, 0, 1.2f);
		}
		if (!Sample.CorrectedNeckWorld.IsZero() || !Sample.CorrectedSpine05World.IsZero())
		{
			DrawDebugDirectionalArrow(World,
				Sample.CorrectedNeckWorld, Sample.CorrectedSpine05World,
				7.0f, FColor::Green, false, ArtifactDebugDrawWindowSec, 0, 1.4f);
			DrawDebugLine(World, Sample.CameraWorldPos, Sample.CorrectedNeckWorld,
				FColor::White, false, ArtifactDebugDrawWindowSec, 0, 1.0f);
		}
		if (!Sample.DesiredNeckWorld.IsZero())
		{
			DrawDebugSphere(World, Sample.DesiredNeckWorld, 3.5f, 10,
				FColor(80, 255, 120), false, ArtifactDebugDrawWindowSec, 0, 1.2f);
		}
		if (!Sample.SourceNeckWorld.IsZero() && !Sample.DesiredNeckWorld.IsZero())
		{
			DrawDebugLine(World, Sample.SourceNeckWorld, Sample.DesiredNeckWorld,
				FColor::Yellow, false, ArtifactDebugDrawWindowSec, 0, 1.1f);
		}
		if (!Sample.CorrectedNeckWorld.IsZero() && !Sample.DesiredNeckWorld.IsZero())
		{
			DrawDebugLine(World, Sample.CorrectedNeckWorld, Sample.DesiredNeckWorld,
				FColor::Green, false, ArtifactDebugDrawWindowSec, 0, 1.1f);
		}
	}

	void BuildArtifactReplayTargets()
	{
		ArtifactReplayTargets.Reset();
		if (!bCapturePhaseScreenshots)
		{
			return;
		}

		for (int32 PhaseIdx = 0; PhaseIdx < ActivePhaseCount; ++PhaseIdx)
		{
			const FString& PhaseName = ActivePhases[PhaseIdx].Name;
			if (!ShouldCaptureArtifactsForPhase(PhaseName))
			{
				continue;
			}

			int32 BestEdgeIndex = INDEX_NONE;
			float BestEdgeScore = -FLT_MAX;
			int32 BestFallbackIndex = INDEX_NONE;
			float BestFallbackScore = -FLT_MAX;
			for (int32 SampleIdx = 0; SampleIdx < AllSamples.Num(); ++SampleIdx)
			{
				const FFrameSample& Sample = AllSamples[SampleIdx];
				if (Sample.PhaseIndex != PhaseIdx) continue;

				const bool bIsEdge =
					Sample.bCameraRayHitsBody ||
					Sample.bUpperTorsoProxyRayHit ||
					Sample.bUpperTorsoCapsuleRayHit ||
					Sample.bUpperTorsoCapsuleCameraInside ||
					Sample.bHeadStretchExceeded ||
					Sample.bNeckGapExceeded ||
					Sample.bCorrectedNeckChainErrorExceeded ||
					Sample.bCorrectedUpperChainFoldExceeded;
				const float Score =
					Sample.Severity +
					(Sample.bUpperTorsoCapsuleRayHit ? 8.f : 0.f) +
					(Sample.bUpperTorsoCapsuleCameraInside ? 8.f : 0.f) +
					(Sample.bUpperTorsoProxyRayHit ? 4.f : 0.f) +
					(Sample.bCameraRayHitsBody ? 4.f : 0.f) +
					(Sample.bHeadStretchExceeded ? (4.f + Sample.SourceHeadCameraDist) : 0.f) +
					(Sample.bNeckGapExceeded ? (4.f + Sample.SourceNeckTargetGapDist) : 0.f);

				if (Score > BestFallbackScore)
				{
					BestFallbackScore = Score;
					BestFallbackIndex = SampleIdx;
				}
				if (bIsEdge && Score > BestEdgeScore)
				{
					BestEdgeScore = Score;
					BestEdgeIndex = SampleIdx;
				}
			}

			const int32 ChosenIndex = BestEdgeIndex != INDEX_NONE ? BestEdgeIndex : BestFallbackIndex;
			if (!AllSamples.IsValidIndex(ChosenIndex)) continue;

			const FFrameSample& Chosen = AllSamples[ChosenIndex];
			FArtifactReplayTarget Target;
			Target.PhaseName = PhaseName;
			Target.PhaseIndex = PhaseIdx;
			Target.PhaseTime = Chosen.PhaseTime;
			Target.Severity = Chosen.Severity;
			Target.Reason = BestEdgeIndex != INDEX_NONE ? TEXT("EDGE") : TEXT("CHECK");
			Target.MeasurementSampleIndex = ChosenIndex;
			Target.ArtifactStem = FString::Printf(
				TEXT("%s_%s_%s_t%.2f_p%.1f_ray%.1f_proxy%.1f_capsule%.1f"),
				*RunId, *Target.PhaseName, *Target.Reason,
				Target.PhaseTime, Chosen.CameraPitchDeg,
				Chosen.CameraRayHitDist, Chosen.UpperTorsoProxyRayDist,
				Chosen.UpperTorsoCapsuleRayDist);
			Target.ScreenshotPath = OutputDir / (Target.ArtifactStem + TEXT(".png"));
			Target.SidecarPath = OutputDir / (Target.ArtifactStem + TEXT(".json"));
			ArtifactReplayTargets.Add(Target);
		}

		ArtifactReplayTargets.Sort([](const FArtifactReplayTarget& A, const FArtifactReplayTarget& B)
		{
			return A.PhaseIndex < B.PhaseIndex;
		});
	}

	void QueueArtifactCapture(int32 TargetIndex, const FFrameSample& Sample)
	{
		if (!ArtifactReplayTargets.IsValidIndex(TargetIndex) || PendingArtifactCaptureIndex != INDEX_NONE)
			return;

		FArtifactReplayTarget& Target = ArtifactReplayTargets[TargetIndex];
		if (Target.bRequested || FScreenshotRequest::IsScreenshotRequested())
			return;

		// Debug draw removed from screenshots -- clean first-person view only.
		// Debug geometry was masking the actual defect in captured images.
		Target.ReplaySample = Sample;
		Target.bHasReplaySample = true;
		Target.RequestedFrameNumber = Sample.FrameNumber;
		Target.RequestedPhaseTime = Sample.PhaseTime;
		Target.RequestedTimeErrorSec = Sample.PhaseTime - Target.PhaseTime;
		bScreenshotProcessedSignal = false;
		FScreenshotRequest::RequestScreenshot(Target.ScreenshotPath, false, false);
		Target.bRequested = true;
		PendingArtifactCaptureIndex = TargetIndex;

		Test->AddInfo(FString::Printf(
			TEXT("ClipMatrixArtifact: queued %s at phase=%s t=%.3f target=%.3f dt=%.4f reason=%s"),
			*Target.ArtifactStem, *Target.PhaseName,
			Sample.PhaseTime, Target.PhaseTime,
			Target.RequestedTimeErrorSec, *Target.Reason));
	}

	void CompletePendingArtifactCapture()
	{
		if (!ArtifactReplayTargets.IsValidIndex(PendingArtifactCaptureIndex))
		{
			PendingArtifactCaptureIndex = INDEX_NONE;
			bScreenshotProcessedSignal = false;
			return;
		}

		FArtifactReplayTarget& Target = ArtifactReplayTargets[PendingArtifactCaptureIndex];
		Target.bProcessed = true;
		Target.ProcessedFrameNumber = static_cast<int64>(GFrameCounter);
		Target.bFileExistsAfterProcess = IFileManager::Get().FileExists(*Target.ScreenshotPath);
		WriteArtifactSidecar(Target, AllSamples, RunId, Scenario,
			InitialCapsuleHalfHeight, ActivePhases, ActivePhaseCount);
		Test->AddInfo(FString::Printf(
			TEXT("ClipMatrixArtifact: processed %s requestedDt=%.4f file=%d"),
			*Target.ArtifactStem, Target.RequestedTimeErrorSec,
			Target.bFileExistsAfterProcess ? 1 : 0));

		PendingArtifactCaptureIndex = INDEX_NONE;
		bScreenshotProcessedSignal = false;
	}

	// ---- Stage 0: Wait for character ----
	bool WaitForPawn()
	{
		UWorld* World = nullptr;
		for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		{
			if (Ctx.WorldType == EWorldType::PIE || Ctx.WorldType == EWorldType::Game)
			{
				World = Ctx.World();
				break;
			}
		}
		if (!World) { SettleTime += 0.016f; return SettleTime > MaxSettleSec; }

		PC = World->GetFirstPlayerController();
		if (!PC) { SettleTime += 0.016f; return SettleTime > MaxSettleSec; }

		Character = Cast<ACharacter>(PC->GetPawn());
		if (!Character)
		{
			SettleTime += 0.016f;
			if (SettleTime > MaxSettleSec)
			{
				Test->AddError(TEXT("ClipMatrix: No character pawn after settle timeout"));
				return true;
			}
			return false;
		}

		OwnerMesh = FindOwnerVisibleMesh(Character);
		if (!OwnerMesh || !OwnerMesh->GetSkeletalMeshAsset())
		{
			SettleTime += 0.016f;
			if (SettleTime > MaxSettleSec)
			{
				Test->AddWarning(TEXT("ClipMatrix: No owner-visible mesh with skeletal asset, skipping"));
				return true;
			}
			return false;
		}

		Test->AddInfo(FString::Printf(
			TEXT("ClipMatrix: character=%s ownerMesh=%s"),
			*Character->GetClass()->GetName(), *OwnerMesh->GetName()));

		Stage = 1;
		return false;
	}

	// ---- Stage 1: Dump mesh identity ----
	bool DumpMeshIdentity()
	{
		IFileManager::Get().MakeDirectory(*OutputDir, true);

		TSharedPtr<FJsonObject> Identity = CollectMeshIdentity(Character);
		Identity->SetStringField(TEXT("runId"), RunId);
		Identity->SetStringField(TEXT("copyPoseSource"),
			GetCopyPoseSourceName(OwnerMesh));

		FString JsonStr;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
		FJsonSerializer::Serialize(Identity.ToSharedRef(), Writer);

		const FString Path = OutputDir / FString::Printf(
			TEXT("%s_mesh_identity.json"), *RunId);
		FFileHelper::SaveStringToFile(JsonStr, *Path);

		Test->AddInfo(FString::Printf(TEXT("ClipMatrix: mesh identity -> %s"), *Path));

		Stage = 2;
		return false;
	}

	// ---- Stage 2: Apply layer isolation ----
	bool ApplyLayerIsolation()
	{
		const FString ModeStr = LayerModeToString(LayerMode);
		const FString ScenarioStr = ScenarioToString(Scenario);
		Test->AddInfo(FString::Printf(
			TEXT("ClipMatrix: Layer mode = %s, scenario = %s"), *ModeStr, *ScenarioStr));

		TArray<USkeletalMeshComponent*> AllMeshes;
		Character->GetComponents<USkeletalMeshComponent>(AllMeshes);

		USkeletalMeshComponent* DriverBody = FindMeshByRole(Character, TEXT("DriverBody"));
		USkeletalMeshComponent* WorldBody = FindMeshByRole(Character, TEXT("WorldBody"));
		USkeletalMeshComponent* LocalBody = FindMeshByRole(Character, TEXT("LocalBody"));

		switch (LayerMode)
		{
		case EClipMatrixLayerMode::DriverBody_Only:
			for (USkeletalMeshComponent* M : AllMeshes)
			{
				if (M == DriverBody)
				{
					M->SetHiddenInGame(false);
					M->SetOwnerNoSee(false);
					M->SetOnlyOwnerSee(false);
				}
				else
				{
					M->SetHiddenInGame(true);
				}
			}
			OwnerMesh = DriverBody;
			break;

		case EClipMatrixLayerMode::WorldBody_Only:
			for (USkeletalMeshComponent* M : AllMeshes)
			{
				if (M == WorldBody)
				{
					M->SetHiddenInGame(false);
					M->SetOwnerNoSee(false);
					M->SetOnlyOwnerSee(false);
				}
				else
				{
					M->SetHiddenInGame(true);
				}
			}
			OwnerMesh = WorldBody;
			break;

		case EClipMatrixLayerMode::LocalBody_Raw:
			if (OwnerMesh)
			{
				if (UAnimInstance* Anim = OwnerMesh->GetAnimInstance())
				{
					if (FBoolProperty* Prop = CastField<FBoolProperty>(
						Anim->GetClass()->FindPropertyByName(FName("bEnableSpineLock"))))
					{
						Prop->SetPropertyValue(
							Prop->ContainerPtrToValuePtr<void>(Anim), false);
					}
				}
			}
			break;

		case EClipMatrixLayerMode::LocalBody_Corrected:
			break;
		}

		// UpperChainMode override is deferred to RunMeasurementPhases because
		// the LocalBodyCustomization anim instance may not exist yet at this
		// stage (Mutable build is still in progress).
		bUpperChainModeApplied = (UpperChainModeOverride == INDEX_NONE);

		const FString ChainModeStr = UpperChainModeOverride == 0 ? TEXT("Baseline")
			: UpperChainModeOverride == 1 ? TEXT("FilterV1")
			: TEXT("Default");
		RunId += FString::Printf(TEXT("_%s_%s_%s"), *ModeStr, *ScenarioStr, *ChainModeStr);
		InitialCharacterTransform = Character ? Character->GetActorTransform() : FTransform::Identity;
		InitialControlRotation = PC ? PC->GetControlRotation() : FRotator::ZeroRotator;
		if (Character && Character->GetCapsuleComponent())
		{
			float InitialCapsuleRadius = 0.f;
			Character->GetCapsuleComponent()->GetScaledCapsuleSize(
				InitialCapsuleRadius, InitialCapsuleHalfHeight);
		}

		Stage = 3;
		CurrentPhase = 0;
		PhaseElapsed = 0.f;
		ConsecutiveIntrusionFrames = 0;
		bScreenshotTakenThisPhase = false;
		bDetailedIssueLoggedThisPhase = false;
		ApplyPhaseInputs(ActivePhases[0]);
		return false;
	}

	// Try to apply the deferred UpperChainMode override.
	// The LocalBodyCustomization anim instance may appear after Mutable rebuild.
	void TryApplyUpperChainModeOverride()
	{
		if (bUpperChainModeApplied || UpperChainModeOverride == INDEX_NONE || !Character)
		{
			return;
		}

		FString TargetMeshName;
		UAnimInstance* TargetAnim = FindLocalBodyCorrectionAnimInstance(
			Character, &TargetMeshName);
		if (!TargetAnim)
		{
			return;
		}

		const FString AnimClassName = TargetAnim->GetClass()->GetName();
		FProperty* ModeProp = TargetAnim->GetClass()->FindPropertyByName(
			FName("UpperChainMode"));
		FEnumProperty* EnumProp = CastField<FEnumProperty>(ModeProp);
		FByteProperty* ByteProp = CastField<FByteProperty>(ModeProp);
		if (EnumProp)
		{
			uint8 Value = static_cast<uint8>(UpperChainModeOverride);
			EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(
				EnumProp->ContainerPtrToValuePtr<void>(TargetAnim),
				static_cast<int64>(Value));
			Test->AddInfo(FString::Printf(
				TEXT("ClipMatrix: Forced UpperChainMode = %d on mesh=%s anim=%s"),
				Value, *TargetMeshName, *AnimClassName));
		}
		else if (ByteProp)
		{
			ByteProp->SetPropertyValue(
				ByteProp->ContainerPtrToValuePtr<void>(TargetAnim),
				static_cast<uint8>(UpperChainModeOverride));
			Test->AddInfo(FString::Printf(
				TEXT("ClipMatrix: Forced UpperChainMode = %d on mesh=%s anim=%s (byte)"),
				UpperChainModeOverride, *TargetMeshName, *AnimClassName));
		}
		bUpperChainModeApplied = true;
	}

	// ---- Stage 3: Measurement pass ----
	bool RunMeasurementPhases()
	{
		TryApplyUpperChainModeOverride();
		const float DeltaTime = FApp::GetDeltaTime();
		PhaseElapsed += DeltaTime;
		const FClipPhase& Phase = ActivePhases[CurrentPhase];

		if (PC)
		{
			FRotator CR = PC->GetControlRotation();
			CR.Pitch = ResolveRequestedPitch(Phase);
			if (!FMath::IsNearlyZero(Phase.YawRateDegPerSec))
			{
				CR.Yaw += Phase.YawRateDegPerSec * DeltaTime;
			}
			PC->SetControlRotation(CR);
		}
		if (Character && !Phase.MoveInput.IsNearlyZero())
		{
			const FRotator YawRot(0, PC->GetControlRotation().Yaw, 0);
			Character->AddMovementInput(
				FRotationMatrix(YawRot).GetUnitAxis(EAxis::X), Phase.MoveInput.X);
			Character->AddMovementInput(
				FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y), Phase.MoveInput.Y);
		}

		FFrameSample Sample = CollectSample(
			Character, OwnerMesh, PC, CurrentPhase, PhaseElapsed,
			ResolveRequestedPitch(Phase), NeckOffsetFromCamera, ExpectedCameraRelativeOffset);
		AllSamples.Add(Sample);

		if (Sample.IntrusionCount > 0)
		{
			++ConsecutiveIntrusionFrames;
		}
		else
		{
			ConsecutiveIntrusionFrames = 0;
		}

		const bool bAnyRayHit =
			Sample.bCameraRayHitsBody ||
			Sample.bUpperTorsoProxyRayHit ||
			Sample.bUpperTorsoCapsuleRayHit ||
			Sample.bUpperTorsoCapsuleCameraInside;
		const bool bAnyVisibleStretch =
			Sample.bHeadStretchExceeded ||
			Sample.bNeckGapExceeded ||
			Sample.bCorrectedNeckChainErrorExceeded ||
			Sample.bCorrectedUpperChainFoldExceeded;
		if ((bAnyRayHit || bAnyVisibleStretch) && !bDetailedIssueLoggedThisPhase)
		{
			bDetailedIssueLoggedThisPhase = true;
			const FString ProxyLocal = Sample.UpperTorsoProxyCameraLocal.ToCompactString();
			const FString Spine05Local = Sample.Bones.IsValidIndex(2)
				? Sample.Bones[2].CameraLocalPos.ToCompactString() : TEXT("none");
			const FString HeadLocal = Sample.SourceHeadCameraLocal.ToCompactString();
			const FString NeckLocal = Sample.SourceNeckCameraLocal.ToCompactString();
			const FString NeckGap = Sample.SourceNeckTargetGap.ToCompactString();
			const FString SourceUpper = Sample.SourceUpperChainCameraDelta.ToCompactString();
			const FString CorrectedUpper = Sample.CorrectedUpperChainCameraDelta.ToCompactString();
			const FString FoldExcess =
				(Sample.CorrectedUpperChainCameraDelta - Sample.SourceUpperChainCameraDelta).ToCompactString();
			const FString DesiredNeckLocal = Sample.DesiredNeckCameraLocal.ToCompactString();
			Test->AddWarning(FString::Printf(
				TEXT("ClipMatrixEdge: phase=%s t=%.3f reqPitch=%.1f camPitch=%.1f downDot=%.3f speed=%.1f falling=%d crouched=%d rayHit=%d rayBone=%s rayDist=%.1f proxyHit=%d proxyDist=%.1f proxyPerp=%.1f capsuleIn=%d capsuleDist=%.1f capsuleRay=%d capsuleRayDist=%.1f capsulePerp=%.1f camActorZ=%.1f camActorErr=%.2f headActorZ=%.1f neckActorZ=%.1f camHeadDZ=%.1f camNeckDZ=%.1f headStretch=%d headDist=%.1f neckGap=%d neckGapDist=%.1f chainError=%d chainErrorDist=%.1f fold=%d foldExcess=%s srcUpper=%s corrUpper=%s srcLean=%.1f corrLean=%.1f srcForward=%.1f corrForward=%.1f fwdDelta=%.1f targetLocal=%s proxyLocal=%s spine05Local=%s headLocal=%s neckLocal=%s neckGapVec=%s"),
				*Phase.Name, Sample.PhaseTime, Sample.RequestedPitchDeg,
				Sample.CameraPitchDeg, Sample.CameraDownDot, Sample.Speed,
				Sample.bIsFalling ? 1 : 0, Sample.bIsCrouched ? 1 : 0,
				Sample.bCameraRayHitsBody ? 1 : 0, *Sample.CameraRayHitBone,
				Sample.CameraRayHitDist,
				Sample.bUpperTorsoProxyRayHit ? 1 : 0, Sample.UpperTorsoProxyRayDist,
				Sample.UpperTorsoProxyPerpDist,
				Sample.bUpperTorsoCapsuleCameraInside ? 1 : 0, Sample.UpperTorsoCapsuleCameraDist,
				Sample.bUpperTorsoCapsuleRayHit ? 1 : 0, Sample.UpperTorsoCapsuleRayDist,
				Sample.UpperTorsoCapsulePerpDist,
				Sample.CameraActorZ, Sample.CameraActorZError,
				Sample.SourceHeadActorZ, Sample.SourceNeckActorZ,
				Sample.CameraHeadZDelta, Sample.CameraNeckZDelta,
				Sample.bHeadStretchExceeded ? 1 : 0, Sample.SourceHeadCameraDist,
				Sample.bNeckGapExceeded ? 1 : 0, Sample.SourceNeckTargetGapDist,
				Sample.bCorrectedNeckChainErrorExceeded ? 1 : 0,
				Sample.CorrectedNeckChainErrorDist,
				Sample.bCorrectedUpperChainFoldExceeded ? 1 : 0,
				*FoldExcess, *SourceUpper, *CorrectedUpper,
				Sample.SourceUpperLeanDeg, Sample.CorrectedUpperLeanDeg,
				Sample.SourceUpperForwardCm, Sample.CorrectedUpperForwardCm,
				Sample.UpperForwardDeltaCm,
				*DesiredNeckLocal, *ProxyLocal, *Spine05Local,
				*HeadLocal, *NeckLocal, *NeckGap));
		}

		TimelineLines.Add(SampleToJsonLine(Sample, ActivePhases, ActivePhaseCount));

		if (PhaseElapsed >= Phase.DurationSec)
		{
			FPhaseSummary Sum = BuildPhaseSummary(AllSamples, CurrentPhase, ActivePhases, ActivePhaseCount);
			PhaseSummaries.Add(Sum);

			if (Sum.bFailed)
			{
				Test->AddWarning(FString::Printf(
					TEXT("ClipMatrix: Phase '%s' FAILED -- boneIntrusion=%d/%d rayHits=%d/%d proxyHits=%d/%d capsuleInside=%d/%d capsuleRay=%d/%d chainError=%d/%d chainErrorDist=%.1f cm fold=%d/%d aheadExcess=%.1f cm aboveExcess=%.1f cm headStretch=%d/%d headDist=%.1f cm neckGap=%d/%d neckGapDist=%.1f cm camActorZ=%.1f..%.1f exp=%.1f err=%.2f cm camHeadDZ=%.1f..%.1f camNeckDZ=%.1f..%.1f srcLean=%.1f corrLean=%.1f srcForward=%.1f corrForward=%.1f corrDelta=%.1f worstBone=%s dist=%.1f cm rayBone=%s rayDist=%.1f cm capsuleDist=%.1f cm capsuleRayDist=%.1f cm"),
					*Sum.Name, Sum.MaxConsecutiveIntrusions, ConsecutiveFailThreshold,
					Sum.MaxConsecutiveRayHits, ConsecutiveFailThreshold,
					Sum.MaxConsecutiveProxyRayHits, ConsecutiveFailThreshold,
					Sum.MaxConsecutiveCapsuleCameraInside, ConsecutiveFailThreshold,
					Sum.MaxConsecutiveCapsuleRayHits, ConsecutiveFailThreshold,
					Sum.MaxConsecutiveCorrectedNeckChainError, ConsecutiveFailThreshold,
					Sum.MaxCorrectedNeckChainErrorDist,
					Sum.MaxConsecutiveCorrectedUpperChainFold, ConsecutiveFailThreshold,
					Sum.MaxCorrectedUpperChainAheadDist,
					Sum.MaxCorrectedUpperChainAboveDist,
					Sum.MaxConsecutiveHeadStretch, ConsecutiveFailThreshold,
					Sum.MaxSourceHeadCameraDist,
					Sum.MaxConsecutiveNeckGap, ConsecutiveFailThreshold,
					Sum.MaxSourceNeckTargetGapDist,
					Sum.MinCameraActorZ < FLT_MAX ? Sum.MinCameraActorZ : -1.f,
					Sum.MaxCameraActorZ > -FLT_MAX ? Sum.MaxCameraActorZ : -1.f,
					Sum.ExpectedCameraActorZ, Sum.MaxAbsCameraActorZError,
					Sum.MinCameraHeadZDelta < FLT_MAX ? Sum.MinCameraHeadZDelta : -1.f,
					Sum.MaxCameraHeadZDelta > -FLT_MAX ? Sum.MaxCameraHeadZDelta : -1.f,
					Sum.MinCameraNeckZDelta < FLT_MAX ? Sum.MinCameraNeckZDelta : -1.f,
					Sum.MaxCameraNeckZDelta > -FLT_MAX ? Sum.MaxCameraNeckZDelta : -1.f,
					Sum.MaxSourceUpperLeanDeg, Sum.MaxCorrectedUpperLeanDeg,
					Sum.MaxSourceUpperForwardCm, Sum.MaxCorrectedUpperForwardCm,
					Sum.MaxUpperForwardDeltaCm,
					*Sum.WorstBone.ToString(),
					Sum.WorstIntrusionDistCm < FLT_MAX ? Sum.WorstIntrusionDistCm : -1.f,
					*Sum.NearestRayHitBone,
					Sum.NearestRayHitDist < FLT_MAX ? Sum.NearestRayHitDist : -1.f,
					Sum.MinCapsuleCameraDist < FLT_MAX ? Sum.MinCapsuleCameraDist : -1.f,
					Sum.NearestCapsuleRayHitDist < FLT_MAX ? Sum.NearestCapsuleRayHitDist : -1.f));
			}

			++CurrentPhase;
			if (CurrentPhase >= ActivePhaseCount)
			{
				Stage = 4;
				return false;
			}

			PhaseElapsed = 0.f;
			ConsecutiveIntrusionFrames = 0;
			bScreenshotTakenThisPhase = false;
			bDetailedIssueLoggedThisPhase = false;
			ApplyPhaseInputs(ActivePhases[CurrentPhase]);
		}

		return false;
	}

	// ---- Stage 4: Prepare artifact replay ----
	bool PrepareArtifactReplay()
	{
		BuildArtifactReplayTargets();
		if (ArtifactReplayTargets.Num() == 0)
		{
			Stage = 7;
			return false;
		}

		ResetCharacterForReplay();
		CurrentPhase = 0;
		PhaseElapsed = 0.f;
		ReplayWarmupRemaining = ArtifactReplaySettleSec;
		PendingArtifactCaptureIndex = INDEX_NONE;
		bDetailedIssueLoggedThisPhase = false;
		Stage = 5;

		Test->AddInfo(FString::Printf(
			TEXT("ClipMatrixArtifact: replaying %d targeted phases for screenshots"),
			ArtifactReplayTargets.Num()));
		for (const FArtifactReplayTarget& Target : ArtifactReplayTargets)
		{
			Test->AddInfo(FString::Printf(
				TEXT("ClipMatrixArtifact: target phase=%s t=%.3f reason=%s"),
				*Target.PhaseName, Target.PhaseTime, *Target.Reason));
		}

		return false;
	}

	// ---- Stage 5: Run artifact replay ----
	bool RunArtifactReplay()
	{
		const float DeltaTime = FApp::GetDeltaTime();
		if (PendingArtifactCaptureIndex != INDEX_NONE && bScreenshotProcessedSignal)
		{
			CompletePendingArtifactCapture();
		}

		if (ReplayWarmupRemaining > 0.f)
		{
			ReplayWarmupRemaining = FMath::Max(0.f, ReplayWarmupRemaining - DeltaTime);
			if (ReplayWarmupRemaining <= 0.f)
			{
				ApplyPhaseInputs(ActivePhases[CurrentPhase]);
			}
			return false;
		}

		PhaseElapsed += DeltaTime;
		const FClipPhase& Phase = ActivePhases[CurrentPhase];

		if (PC)
		{
			FRotator CR = PC->GetControlRotation();
			CR.Pitch = ResolveRequestedPitch(Phase);
			if (!FMath::IsNearlyZero(Phase.YawRateDegPerSec))
			{
				CR.Yaw += Phase.YawRateDegPerSec * DeltaTime;
			}
			PC->SetControlRotation(CR);
		}
		if (Character && !Phase.MoveInput.IsNearlyZero())
		{
			const FRotator YawRot(0, PC->GetControlRotation().Yaw, 0);
			Character->AddMovementInput(
				FRotationMatrix(YawRot).GetUnitAxis(EAxis::X), Phase.MoveInput.X);
			Character->AddMovementInput(
				FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y), Phase.MoveInput.Y);
		}

		const FFrameSample ReplaySample = CollectSample(
			Character, OwnerMesh, PC, CurrentPhase, PhaseElapsed,
			ResolveRequestedPitch(Phase), NeckOffsetFromCamera, ExpectedCameraRelativeOffset);
		for (int32 TargetIndex = 0; TargetIndex < ArtifactReplayTargets.Num(); ++TargetIndex)
		{
			FArtifactReplayTarget& Target = ArtifactReplayTargets[TargetIndex];
			if (Target.bRequested || Target.PhaseIndex != CurrentPhase)
				continue;

			if (PhaseElapsed >= (Target.PhaseTime - ArtifactReplayCaptureSlackSec))
			{
				QueueArtifactCapture(TargetIndex, ReplaySample);
			}
		}

		if (PhaseElapsed >= Phase.DurationSec)
		{
			++CurrentPhase;
			if (CurrentPhase >= ActivePhaseCount)
			{
				Stage = 6;
				return false;
			}
			PhaseElapsed = 0.f;
			ApplyPhaseInputs(ActivePhases[CurrentPhase]);
		}

		return false;
	}

	// ---- Stage 6: Finalize artifact replay ----
	bool FinalizeArtifactReplay()
	{
		if (PendingArtifactCaptureIndex != INDEX_NONE && bScreenshotProcessedSignal)
		{
			CompletePendingArtifactCapture();
		}

		if (PendingArtifactCaptureIndex != INDEX_NONE)
		{
			return false;
		}

		for (const FArtifactReplayTarget& Target : ArtifactReplayTargets)
		{
			if (!Target.bRequested)
			{
				Test->AddWarning(FString::Printf(
					TEXT("ClipMatrixArtifact: no replay capture was queued for phase=%s targetT=%.3f"),
					*Target.PhaseName, Target.PhaseTime));
			}
		}

		Stage = 7;
		return false;
	}

	// ---- Stage 7: Write results ----
	bool WriteResults()
	{
		IFileManager::Get().MakeDirectory(*OutputDir, true);

		// Summary JSON
		TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("testName"), TEXT("FirstPersonClipMatrix"));
		Root->SetStringField(TEXT("runId"), RunId);
		Root->SetStringField(TEXT("scenario"), ScenarioToString(Scenario));
		Root->SetStringField(TEXT("character"),
			Character ? Character->GetClass()->GetName() : TEXT("?"));
		Root->SetStringField(TEXT("ownerMesh"),
			OwnerMesh ? OwnerMesh->GetName() : TEXT("?"));
		Root->SetStringField(TEXT("layerMode"), LayerModeToString(LayerMode));
		Root->SetNumberField(TEXT("totalSamples"), AllSamples.Num());
		Root->SetNumberField(TEXT("forbiddenRadiusCm"), ForbiddenRadiusCm);
		Root->SetNumberField(TEXT("proxyRadiusCm"), UpperTorsoProxyRadiusCm);
		Root->SetNumberField(TEXT("capsuleRadiusCm"), UpperTorsoCapsuleRadiusCm);
		Root->SetNumberField(TEXT("cameraProbeLengthCm"), CameraProbeLengthCm);
		Root->SetNumberField(TEXT("sourceHeadStretchFailCm"), SourceHeadStretchFailCm);
		Root->SetNumberField(TEXT("sourceNeckGapFailCm"), SourceNeckGapFailCm);
		Root->SetNumberField(TEXT("correctedNeckChainErrorFailCm"), CorrectedNeckChainErrorFailCm);
		Root->SetNumberField(TEXT("correctedUpperChainAheadSlackCm"), CorrectedUpperChainAheadSlackCm);
		Root->SetNumberField(TEXT("correctedUpperChainAboveSlackCm"), CorrectedUpperChainAboveSlackCm);
		Root->SetNumberField(TEXT("expectedCameraActorZ"), ExpectedCameraRelativeOffset.Z);
		Root->SetNumberField(TEXT("consecutiveFailThreshold"), ConsecutiveFailThreshold);

		int32 FailedPhases = 0;
		TArray<TSharedPtr<FJsonValue>> PhaseArray;
		for (const FPhaseSummary& Sum : PhaseSummaries)
		{
			TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
			P->SetStringField(TEXT("name"), Sum.Name);
			P->SetNumberField(TEXT("samples"), Sum.SampleCount);
			P->SetNumberField(TEXT("intrusionFrames"), Sum.TotalIntrusionFrames);
			P->SetNumberField(TEXT("maxConsecutive"), Sum.MaxConsecutiveIntrusions);
			P->SetNumberField(TEXT("worstDistCm"),
				Sum.WorstIntrusionDistCm < FLT_MAX ? Sum.WorstIntrusionDistCm : -1.f);
			P->SetStringField(TEXT("worstBone"), Sum.WorstBone.ToString());
			P->SetNumberField(TEXT("rayHitFrames"), Sum.CameraRayHitFrames);
			P->SetNumberField(TEXT("maxConsecutiveRayHits"), Sum.MaxConsecutiveRayHits);
			P->SetNumberField(TEXT("nearestRayHitDist"),
				Sum.NearestRayHitDist < FLT_MAX ? Sum.NearestRayHitDist : -1.f);
			P->SetStringField(TEXT("nearestRayHitBone"), Sum.NearestRayHitBone);
			P->SetNumberField(TEXT("proxyHitFrames"), Sum.ProxyRayHitFrames);
			P->SetNumberField(TEXT("maxConsecutiveProxyHits"), Sum.MaxConsecutiveProxyRayHits);
			P->SetNumberField(TEXT("nearestProxyRayHitDist"),
				Sum.NearestProxyRayHitDist < FLT_MAX ? Sum.NearestProxyRayHitDist : -1.f);
			P->SetNumberField(TEXT("minProxyPerpDist"),
				Sum.MinProxyPerpDist < FLT_MAX ? Sum.MinProxyPerpDist : -1.f);
			P->SetNumberField(TEXT("capsuleCameraInsideFrames"), Sum.CapsuleCameraInsideFrames);
			P->SetNumberField(TEXT("maxConsecutiveCapsuleCameraInside"), Sum.MaxConsecutiveCapsuleCameraInside);
			P->SetNumberField(TEXT("minCapsuleCameraDist"),
				Sum.MinCapsuleCameraDist < FLT_MAX ? Sum.MinCapsuleCameraDist : -1.f);
			P->SetNumberField(TEXT("capsuleRayHitFrames"), Sum.CapsuleRayHitFrames);
			P->SetNumberField(TEXT("maxConsecutiveCapsuleRayHits"), Sum.MaxConsecutiveCapsuleRayHits);
			P->SetNumberField(TEXT("nearestCapsuleRayHitDist"),
				Sum.NearestCapsuleRayHitDist < FLT_MAX ? Sum.NearestCapsuleRayHitDist : -1.f);
			P->SetNumberField(TEXT("minCapsuleRayPerpDist"),
				Sum.MinCapsuleRayPerpDist < FLT_MAX ? Sum.MinCapsuleRayPerpDist : -1.f);
			P->SetNumberField(TEXT("headStretchFrames"), Sum.HeadStretchFrames);
			P->SetNumberField(TEXT("maxConsecutiveHeadStretch"), Sum.MaxConsecutiveHeadStretch);
			P->SetNumberField(TEXT("maxSourceHeadCameraDist"), Sum.MaxSourceHeadCameraDist);
			P->SetNumberField(TEXT("neckGapFrames"), Sum.NeckGapFrames);
			P->SetNumberField(TEXT("maxConsecutiveNeckGap"), Sum.MaxConsecutiveNeckGap);
			P->SetNumberField(TEXT("maxSourceNeckTargetGapDist"), Sum.MaxSourceNeckTargetGapDist);
			P->SetNumberField(TEXT("maxCorrectedNeckTargetGapDist"), Sum.MaxCorrectedNeckTargetGapDist);
			P->SetNumberField(TEXT("correctedNeckChainErrorFrames"), Sum.CorrectedNeckChainErrorFrames);
			P->SetNumberField(TEXT("maxConsecutiveCorrectedNeckChainError"), Sum.MaxConsecutiveCorrectedNeckChainError);
			P->SetNumberField(TEXT("maxCorrectedNeckChainErrorDist"), Sum.MaxCorrectedNeckChainErrorDist);
			P->SetNumberField(TEXT("correctedUpperChainFoldFrames"), Sum.CorrectedUpperChainFoldFrames);
			P->SetNumberField(TEXT("maxConsecutiveCorrectedUpperChainFold"), Sum.MaxConsecutiveCorrectedUpperChainFold);
			P->SetNumberField(TEXT("maxCorrectedUpperChainAheadDist"), Sum.MaxCorrectedUpperChainAheadDist);
			P->SetNumberField(TEXT("maxCorrectedUpperChainAboveDist"), Sum.MaxCorrectedUpperChainAboveDist);
			P->SetNumberField(TEXT("cameraActorZExpected"), Sum.ExpectedCameraActorZ);
			P->SetNumberField(TEXT("minCameraActorZ"),
				Sum.MinCameraActorZ < FLT_MAX ? Sum.MinCameraActorZ : -1.f);
			P->SetNumberField(TEXT("maxCameraActorZ"),
				Sum.MaxCameraActorZ > -FLT_MAX ? Sum.MaxCameraActorZ : -1.f);
			P->SetNumberField(TEXT("maxAbsCameraActorZError"), Sum.MaxAbsCameraActorZError);
			P->SetNumberField(TEXT("minCameraHeadZDelta"),
				Sum.MinCameraHeadZDelta < FLT_MAX ? Sum.MinCameraHeadZDelta : -1.f);
			P->SetNumberField(TEXT("maxCameraHeadZDelta"),
				Sum.MaxCameraHeadZDelta > -FLT_MAX ? Sum.MaxCameraHeadZDelta : -1.f);
			P->SetNumberField(TEXT("minCameraNeckZDelta"),
				Sum.MinCameraNeckZDelta < FLT_MAX ? Sum.MinCameraNeckZDelta : -1.f);
			P->SetNumberField(TEXT("maxCameraNeckZDelta"),
				Sum.MaxCameraNeckZDelta > -FLT_MAX ? Sum.MaxCameraNeckZDelta : -1.f);
			P->SetNumberField(TEXT("maxSourceUpperLeanDeg"), Sum.MaxSourceUpperLeanDeg);
			P->SetNumberField(TEXT("maxCorrectedUpperLeanDeg"), Sum.MaxCorrectedUpperLeanDeg);
			P->SetNumberField(TEXT("maxSourceUpperForwardCm"), Sum.MaxSourceUpperForwardCm);
			P->SetNumberField(TEXT("maxCorrectedUpperForwardCm"), Sum.MaxCorrectedUpperForwardCm);
			P->SetNumberField(TEXT("maxUpperForwardDeltaCm"), Sum.MaxUpperForwardDeltaCm);
			P->SetNumberField(TEXT("peakSeverity"), Sum.PeakSeverity);
			P->SetNumberField(TEXT("worstSampleIndex"), Sum.WorstSampleIndex);
			P->SetBoolField(TEXT("failed"), Sum.bFailed);
			PhaseArray.Add(MakeShared<FJsonValueObject>(P));
			if (Sum.bFailed) ++FailedPhases;
		}
		Root->SetArrayField(TEXT("phases"), PhaseArray);
		Root->SetNumberField(TEXT("failedPhaseCount"), FailedPhases);

		TArray<TSharedPtr<FJsonValue>> ArtifactArray;
		for (const FArtifactReplayTarget& Target : ArtifactReplayTargets)
		{
			TSharedPtr<FJsonObject> Artifact = MakeShared<FJsonObject>();
			Artifact->SetStringField(TEXT("phase"), Target.PhaseName);
			Artifact->SetNumberField(TEXT("phaseIndex"), Target.PhaseIndex);
			Artifact->SetNumberField(TEXT("targetTime"), Target.PhaseTime);
			Artifact->SetStringField(TEXT("reason"), Target.Reason);
			Artifact->SetNumberField(TEXT("measurementSampleIndex"), Target.MeasurementSampleIndex);
			Artifact->SetBoolField(TEXT("requested"), Target.bRequested);
			Artifact->SetBoolField(TEXT("processed"), Target.bProcessed);
			Artifact->SetBoolField(TEXT("screenshotFileExists"), Target.bFileExistsAfterProcess);
			Artifact->SetNumberField(TEXT("requestedFrame"), static_cast<double>(Target.RequestedFrameNumber));
			Artifact->SetNumberField(TEXT("processedFrame"), static_cast<double>(Target.ProcessedFrameNumber));
			Artifact->SetNumberField(TEXT("requestedPhaseTime"), Target.RequestedPhaseTime);
			Artifact->SetNumberField(TEXT("requestedTimeErrorSec"), Target.RequestedTimeErrorSec);
			Artifact->SetStringField(TEXT("artifactStem"), Target.ArtifactStem);
			Artifact->SetStringField(TEXT("screenshotPath"), Target.ScreenshotPath);
			Artifact->SetStringField(TEXT("sidecarPath"), Target.SidecarPath);
			ArtifactArray.Add(MakeShared<FJsonValueObject>(Artifact));
		}
		Root->SetArrayField(TEXT("artifacts"), ArtifactArray);
		Root->SetNumberField(TEXT("artifactCount"), ArtifactReplayTargets.Num());

		FString JsonStr;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
		FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

		const FString SummaryPath = OutputDir / FString::Printf(
			TEXT("%s_clip_matrix_summary.json"), *RunId);
		FFileHelper::SaveStringToFile(JsonStr, *SummaryPath);

		// Timeline JSONL
		const FString TimelinePath = OutputDir / FString::Printf(
			TEXT("%s_clip_matrix_timeline.jsonl"), *RunId);
		FString TimelineContent;
		for (const FString& Line : TimelineLines)
		{
			TimelineContent += Line + TEXT("\n");
		}
		FFileHelper::SaveStringToFile(TimelineContent, *TimelinePath);

		// End mesh identity
		TSharedPtr<FJsonObject> EndIdentity = CollectMeshIdentity(Character);
		EndIdentity->SetStringField(TEXT("runId"), RunId);
		EndIdentity->SetStringField(TEXT("phase"), TEXT("end"));
		EndIdentity->SetStringField(TEXT("copyPoseSource"),
			GetCopyPoseSourceName(OwnerMesh));

		FString EndIdStr;
		TSharedRef<TJsonWriter<>> EndWriter = TJsonWriterFactory<>::Create(&EndIdStr);
		FJsonSerializer::Serialize(EndIdentity.ToSharedRef(), EndWriter);
		const FString EndIdPath = OutputDir / FString::Printf(
			TEXT("%s_mesh_identity_end.json"), *RunId);
		FFileHelper::SaveStringToFile(EndIdStr, *EndIdPath);

		Test->AddInfo(FString::Printf(
			TEXT("ClipMatrix: %d phases, %d samples, %d failed phases, %d artifact targets. Results -> %s"),
			PhaseSummaries.Num(), AllSamples.Num(), FailedPhases,
			ArtifactReplayTargets.Num(), *OutputDir));

		if (!bUpperChainModeApplied && UpperChainModeOverride != INDEX_NONE)
		{
			Test->AddError(TEXT("ClipMatrix: FAILED to force UpperChainMode -- LocalBody anim instance never appeared"));
		}

		if (FailedPhases > 0)
		{
			Test->AddError(FString::Printf(
				TEXT("ClipMatrix: %d/%d phases have body intrusion or neck stretch into the owner view"),
				FailedPhases, PhaseSummaries.Num()));
		}

		return true;
	}
};

} // namespace ClipMatrixHelpers

// ClipMatrix test map: flat floor with default character spawn.
// Launched via: scripts/ue/test/character/capture_parity.ps1 -TestFilter "...ClipMatrix..."
// The script passes -Map and -ProjectSkipFrontEnd to bypass the menu.
// AutomationOpenMap is a fallback for direct -Game execution.
static const TCHAR* GClipMatrixMapPath = TEXT("/Game/Project/Maps/Test/ClipMatrix_CleanMap");

// Default test: Mode D (normal runtime, SpineLock ON)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFirstPersonClipMatrixTest,
	"ProjectIntegrationTests.Character.FirstPerson.ClipMatrix.Default",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FFirstPersonClipMatrixTest::RunTest(const FString& Parameters)
{
	AutomationOpenMap(GClipMatrixMapPath);
	ADD_LATENT_AUTOMATION_COMMAND(ClipMatrixHelpers::FClipMatrixCommand(this,
		ClipMatrixHelpers::EClipMatrixLayerMode::LocalBody_Corrected));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFirstPersonClipMatrixSprintStopOnlyTest,
	"ProjectIntegrationTests.Character.FirstPerson.ClipMatrix.SprintStopOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FFirstPersonClipMatrixSprintStopOnlyTest::RunTest(const FString& Parameters)
{
	AutomationOpenMap(GClipMatrixMapPath);
	ADD_LATENT_AUTOMATION_COMMAND(ClipMatrixHelpers::FClipMatrixCommand(
		this,
		ClipMatrixHelpers::EClipMatrixLayerMode::LocalBody_Corrected,
		ClipMatrixHelpers::EClipMatrixScenario::SprintStopLoop));
	return true;
}

// Explicit A/B comparison tests: same scenario, different UpperChainMode.
// Baseline: CopyPose + spine yaw + exact neck target, NO upper-chain filter.
// FilterV1: full guard-driven upper-chain follow + pitch constraint.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFirstPersonClipMatrixBaselineTest,
	"ProjectIntegrationTests.Character.FirstPerson.ClipMatrix.Baseline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FFirstPersonClipMatrixBaselineTest::RunTest(const FString& Parameters)
{
	AutomationOpenMap(GClipMatrixMapPath);
	ADD_LATENT_AUTOMATION_COMMAND(ClipMatrixHelpers::FClipMatrixCommand(
		this,
		ClipMatrixHelpers::EClipMatrixLayerMode::LocalBody_Corrected,
		ClipMatrixHelpers::EClipMatrixScenario::SprintStopLoop,
		0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFirstPersonClipMatrixFilterV1Test,
	"ProjectIntegrationTests.Character.FirstPerson.ClipMatrix.FilterV1",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FFirstPersonClipMatrixFilterV1Test::RunTest(const FString& Parameters)
{
	AutomationOpenMap(GClipMatrixMapPath);
	ADD_LATENT_AUTOMATION_COMMAND(ClipMatrixHelpers::FClipMatrixCommand(
		this,
		ClipMatrixHelpers::EClipMatrixLayerMode::LocalBody_Corrected,
		ClipMatrixHelpers::EClipMatrixScenario::SprintStopLoop,
		1));
	return true;
}

// Layer isolation modes (DriverBody/WorldBody/LocalRaw) remain in the enum
// for diagnostic use but are not registered as automated tests.
// The focused SprintStopOnly run is for strict repro of the stop overshoot bug.
