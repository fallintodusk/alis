#pragma once

// License terms: see repository root LICENSE.
// Shared types, constants, enums, and inline helpers for ClipMatrix tests.
// Extracted from FirstPersonClipMatrixTest.cpp.

#include "CoreMinimal.h"

namespace ClipMatrixHelpers
{

// -------------------------------------------------------------------------
// Constants
// -------------------------------------------------------------------------

// Bones tracked for forbidden-volume intrusion
static const FName TrackedBones[] = {
	FName("head"),
	FName("neck_01"),
	FName("spine_05"),
	FName("spine_03"),
	FName("clavicle_l"),
	FName("clavicle_r"),
	FName("upperarm_l"),
	FName("upperarm_r")
};
static constexpr int32 NumTrackedBones = UE_ARRAY_COUNT(TrackedBones);

// Forbidden volume: initial threshold, provisional until first data run
static constexpr float ForbiddenRadiusCm = 15.f;
// Consecutive bad frames required to fail a phase (filter single-frame noise)
static constexpr int32 ConsecutiveFailThreshold = 3;
// The player-visible failure is often neckline stretch, not only direct torso ray hits.
// Track the copied source pose against the fixed neck target so automation can fail
// on the same issue the live debug log shows.
static constexpr float SourceHeadStretchFailCm = 28.f;
static constexpr float SourceNeckGapFailCm = 14.f;
static constexpr float CorrectedNeckChainErrorFailCm = 4.f;
static constexpr float CorrectedUpperChainAheadSlackCm = 1.5f;
static constexpr float CorrectedUpperChainAboveSlackCm = 2.f;
// Measure first, capture second. The matrix pass stays logs-only and a
// targeted replay requests exact first-person screenshots after the edge
// timestamps are known, avoiding capture-induced timing drift in the score pass.
static constexpr bool bCapturePhaseScreenshots = true;
static constexpr float RequestedMaxDownPitchDeg = -89.f;
static constexpr float UpperTorsoProxyRadiusCm = 22.5f;
static constexpr float UpperTorsoCapsuleRadiusCm = 18.f;
static constexpr float CameraProbeLengthCm = 120.f;
static constexpr float ArtifactReplaySettleSec = 0.35f;
// Edge replay should request on or after the measured bad frame, not early.
static constexpr float ArtifactReplayCaptureSlackSec = 0.0f;
static constexpr float ArtifactDebugDrawWindowSec = 0.08f;

// -------------------------------------------------------------------------
// Structs
// -------------------------------------------------------------------------

struct FClipPhase
{
	FString Name;
	float DurationSec;
	FVector2D MoveInput;
	float TargetPitchDeg;
	float YawRateDegPerSec;
	bool bSprint;
	bool bCrouch;
	bool bJump;
};

// Per-bone per-frame intrusion data
struct FBoneIntrusion
{
	FName BoneName;
	FVector CameraLocalPos;
	float DistFromCamera;
	bool bInForbiddenVolume;
};

struct FFrameSample
{
	int32 PhaseIndex = -1;
	int64 FrameNumber = 0;
	float PhaseTime = 0.f;
	FVector CameraWorldPos = FVector::ZeroVector;
	FRotator CameraWorldRot = FRotator::ZeroRotator;
	FVector CameraForward = FVector::ForwardVector;
	FVector ActorForward = FVector::ForwardVector;
	float RequestedPitchDeg = 0.f;
	float CameraPitchDeg = 0.f;
	float CameraDownDot = 0.f;
	FRotator ControlRotation = FRotator::ZeroRotator;
	FRotator ActorRotation = FRotator::ZeroRotator;
	float Speed = 0.f;
	bool bIsFalling = false;
	bool bIsCrouched = false;
	FString CopyPoseSourceName;
	FString OwnerVisibleMeshName;
	int32 IntrusionCount = 0;
	bool bCameraRayHitsBody = false;
	float CameraRayHitDist = -1.f;
	FString CameraRayHitBone;
	bool bUpperTorsoProxyRayHit = false;
	float UpperTorsoProxyRayDist = -1.f;
	float UpperTorsoProxyPerpDist = -1.f;
	FVector UpperTorsoProxyWorld = FVector::ZeroVector;
	FVector UpperTorsoProxyCameraLocal = FVector::ZeroVector;
	bool bUpperTorsoCapsuleCameraInside = false;
	float UpperTorsoCapsuleCameraDist = -1.f;
	bool bUpperTorsoCapsuleRayHit = false;
	float UpperTorsoCapsuleRayDist = -1.f;
	float UpperTorsoCapsulePerpDist = -1.f;
	FVector UpperTorsoCapsuleStartWorld = FVector::ZeroVector;
	FVector UpperTorsoCapsuleEndWorld = FVector::ZeroVector;
	FVector UpperTorsoCapsuleStartCameraLocal = FVector::ZeroVector;
	FVector UpperTorsoCapsuleEndCameraLocal = FVector::ZeroVector;
	FVector DesiredNeckWorld = FVector::ZeroVector;
	FVector DesiredNeckCameraLocal = FVector::ZeroVector;
	float CameraActorZ = 0.f;
	float ExpectedCameraActorZ = 0.f;
	float CameraActorZError = 0.f;
	float SourceHeadActorZ = 0.f;
	float SourceNeckActorZ = 0.f;
	float CameraHeadZDelta = 0.f;
	float CameraNeckZDelta = 0.f;
	float SourceHeadCameraDist = -1.f;
	float SourceNeckTargetGapDist = -1.f;
	float CorrectedNeckTargetGapDist = -1.f;
	float SourceNeckChainDist = -1.f;
	float CorrectedNeckChainDist = -1.f;
	float CorrectedNeckChainErrorDist = -1.f;
	FVector SourceHeadCameraLocal = FVector::ZeroVector;
	FVector SourceNeckCameraLocal = FVector::ZeroVector;
	FVector SourceUpperChainCameraDelta = FVector::ZeroVector;
	FVector CorrectedNeckCameraLocal = FVector::ZeroVector;
	FVector CorrectedSpine05CameraLocal = FVector::ZeroVector;
	FVector CorrectedUpperChainCameraDelta = FVector::ZeroVector;
	FVector SourceNeckWorld = FVector::ZeroVector;
	FVector SourceSpine05World = FVector::ZeroVector;
	FVector CorrectedNeckWorld = FVector::ZeroVector;
	FVector CorrectedSpine05World = FVector::ZeroVector;
	FVector SourceNeckTargetGap = FVector::ZeroVector;
	FVector CorrectedNeckTargetGap = FVector::ZeroVector;
	float SourceUpperLeanDeg = 0.f;
	float CorrectedUpperLeanDeg = 0.f;
	float SourceUpperForwardCm = 0.f;
	float CorrectedUpperForwardCm = 0.f;
	float UpperForwardDeltaCm = 0.f;
	bool bHeadStretchExceeded = false;
	bool bNeckGapExceeded = false;
	bool bCorrectedNeckChainErrorExceeded = false;
	bool bCorrectedUpperChainFoldExceeded = false;
	// Severity: higher = worse. Sum of (ForbiddenRadius - dist) for all intruding bones + ray hit bonus.
	float Severity = 0.f;
	TArray<FBoneIntrusion> Bones;
};

struct FPhaseSummary
{
	FString Name;
	int32 SampleCount = 0;
	int32 TotalIntrusionFrames = 0;
	int32 MaxConsecutiveIntrusions = 0;
	float WorstIntrusionDistCm = FLT_MAX;
	FName WorstBone;
	int32 CameraRayHitFrames = 0;
	int32 MaxConsecutiveRayHits = 0;
	float NearestRayHitDist = FLT_MAX;
	FString NearestRayHitBone = TEXT("None");
	int32 ProxyRayHitFrames = 0;
	int32 MaxConsecutiveProxyRayHits = 0;
	float NearestProxyRayHitDist = FLT_MAX;
	float MinProxyPerpDist = FLT_MAX;
	int32 CapsuleCameraInsideFrames = 0;
	int32 MaxConsecutiveCapsuleCameraInside = 0;
	float MinCapsuleCameraDist = FLT_MAX;
	int32 CapsuleRayHitFrames = 0;
	int32 MaxConsecutiveCapsuleRayHits = 0;
	float NearestCapsuleRayHitDist = FLT_MAX;
	float MinCapsuleRayPerpDist = FLT_MAX;
	int32 HeadStretchFrames = 0;
	int32 MaxConsecutiveHeadStretch = 0;
	float MaxSourceHeadCameraDist = 0.f;
	int32 NeckGapFrames = 0;
	int32 MaxConsecutiveNeckGap = 0;
	float MaxSourceNeckTargetGapDist = 0.f;
	float MaxCorrectedNeckTargetGapDist = 0.f;
	int32 CorrectedNeckChainErrorFrames = 0;
	int32 MaxConsecutiveCorrectedNeckChainError = 0;
	float MaxCorrectedNeckChainErrorDist = 0.f;
	int32 CorrectedUpperChainFoldFrames = 0;
	int32 MaxConsecutiveCorrectedUpperChainFold = 0;
	float MaxCorrectedUpperChainAheadDist = 0.f;
	float MaxCorrectedUpperChainAboveDist = 0.f;
	float ExpectedCameraActorZ = 0.f;
	float MinCameraActorZ = FLT_MAX;
	float MaxCameraActorZ = -FLT_MAX;
	float MaxAbsCameraActorZError = 0.f;
	float MinCameraHeadZDelta = FLT_MAX;
	float MaxCameraHeadZDelta = -FLT_MAX;
	float MinCameraNeckZDelta = FLT_MAX;
	float MaxCameraNeckZDelta = -FLT_MAX;
	float MaxSourceUpperLeanDeg = 0.f;
	float MaxCorrectedUpperLeanDeg = 0.f;
	float MaxSourceUpperForwardCm = 0.f;
	float MaxCorrectedUpperForwardCm = 0.f;
	float MaxUpperForwardDeltaCm = 0.f;
	float PeakSeverity = 0.f;
	// index into AllSamples for worst frame
	int32 WorstSampleIndex = -1;
	bool bFailed = false;
};

struct FArtifactReplayTarget
{
	FString PhaseName;
	int32 PhaseIndex = INDEX_NONE;
	float PhaseTime = 0.f;
	float Severity = 0.f;
	FString Reason;
	int32 MeasurementSampleIndex = INDEX_NONE;
	bool bRequested = false;
	bool bProcessed = false;
	bool bFileExistsAfterProcess = false;
	FString ArtifactStem;
	FString ScreenshotPath;
	FString SidecarPath;
	int64 RequestedFrameNumber = 0;
	int64 ProcessedFrameNumber = 0;
	float RequestedPhaseTime = 0.f;
	float RequestedTimeErrorSec = 0.f;
	FFrameSample ReplaySample;
	bool bHasReplaySample = false;
};

// -------------------------------------------------------------------------
// Enums
// -------------------------------------------------------------------------

// Layer isolation mode: which mesh is visible during the test
enum class EClipMatrixLayerMode : uint8
{
	// Mode D: normal owner-view, SpineLock ON (default runtime)
	LocalBody_Corrected,
	// Mode C: normal owner-view, SpineLock OFF (raw copy-pose)
	LocalBody_Raw,
	// Mode B: only WorldBody visible (full head, no owner-only split)
	WorldBody_Only,
	// Mode A: only DriverBody visible (raw MM output)
	DriverBody_Only,
};

enum class EClipMatrixScenario : uint8
{
	FullMatrix,
	SprintStopLoop,
};

// -------------------------------------------------------------------------
// Inline helpers: enum to string
// -------------------------------------------------------------------------

inline const TCHAR* LayerModeToString(EClipMatrixLayerMode Mode)
{
	switch (Mode)
	{
	case EClipMatrixLayerMode::LocalBody_Corrected: return TEXT("D_LocalCorrected");
	case EClipMatrixLayerMode::LocalBody_Raw:       return TEXT("C_LocalRaw");
	case EClipMatrixLayerMode::WorldBody_Only:      return TEXT("B_WorldBody");
	case EClipMatrixLayerMode::DriverBody_Only:     return TEXT("A_DriverBody");
	default: return TEXT("Unknown");
	}
}

inline const TCHAR* ScenarioToString(EClipMatrixScenario Scenario)
{
	switch (Scenario)
	{
	case EClipMatrixScenario::SprintStopLoop: return TEXT("SprintStopOnly");
	case EClipMatrixScenario::FullMatrix:
	default: return TEXT("FullMatrix");
	}
}

// -------------------------------------------------------------------------
// Phase arrays
// -------------------------------------------------------------------------

// ALL phases look down at max angle (-80) to catch neck hole visibility.
// Movement varies to cover inertia/transition cases.
static const FClipPhase GPhases[] = {
	// 0: idle looking straight down
	{ TEXT("Idle_MaxDown"),             2.0f, {0, 0},  RequestedMaxDownPitchDeg,  0.f,   false, false, false },
	// 1-2: sprint forward looking down, then sudden stop
	{ TEXT("Sprint_MaxDown"),           2.0f, {1, 0},  RequestedMaxDownPitchDeg,  0.f,   true,  false, false },
	{ TEXT("SprintStop_MaxDown"),       2.0f, {0, 0},  RequestedMaxDownPitchDeg,  0.f,   false, false, false },
	// 3: jump while looking down
	{ TEXT("Jump_MaxDown"),             0.5f, {0, 0},  RequestedMaxDownPitchDeg,  0.f,   false, false, true  },
	{ TEXT("JumpFall_MaxDown"),         1.5f, {0, 0},  RequestedMaxDownPitchDeg,  0.f,   false, false, false },
	{ TEXT("JumpLand_MaxDown"),         1.5f, {0, 0},  RequestedMaxDownPitchDeg,  0.f,   false, false, false },
	// 6: run + jump while looking down
	{ TEXT("RunJump_MaxDown"),          0.5f, {1, 0},  RequestedMaxDownPitchDeg,  0.f,   false, false, true  },
	{ TEXT("RunJumpFall_MaxDown"),      1.5f, {1, 0},  RequestedMaxDownPitchDeg,  0.f,   false, false, false },
	{ TEXT("RunJumpLand_MaxDown"),      1.5f, {0, 0},  RequestedMaxDownPitchDeg,  0.f,   false, false, false },
	// 9: crouch while looking down
	{ TEXT("Crouch_MaxDown"),           1.5f, {0, 0},  RequestedMaxDownPitchDeg,  0.f,   false, true,  false },
	{ TEXT("CrouchRun_MaxDown"),        1.5f, {1, 0},  RequestedMaxDownPitchDeg,  0.f,   false, true,  false },
	{ TEXT("Uncrouch_MaxDown"),         1.5f, {0, 0},  RequestedMaxDownPitchDeg,  0.f,   false, false, false },
	// 12: strafe while looking down
	{ TEXT("StrafeL_MaxDown"),          1.5f, {0,-1},  RequestedMaxDownPitchDeg,  0.f,   false, false, false },
	{ TEXT("StrafeR_MaxDown"),          1.5f, {0, 1},  RequestedMaxDownPitchDeg,  0.f,   false, false, false },
	// 14: final settle
	{ TEXT("Settle"),                   1.0f, {0, 0},  0.f,    0.f,   false, false, false },
};
static constexpr int32 GNumPhases = UE_ARRAY_COUNT(GPhases);

static const FClipPhase GSprintStopLoopPhases[] = {
	{ TEXT("SprintForwardA_MaxDown"),   2.0f, {1, 0},  RequestedMaxDownPitchDeg,  0.f,   true,  false, false },
	{ TEXT("SprintStopA_MaxDown"),      2.0f, {0, 0},  RequestedMaxDownPitchDeg,  0.f,   false, false, false },
	{ TEXT("SprintForwardB_MaxDown"),   2.0f, {1, 0},  RequestedMaxDownPitchDeg,  0.f,   true,  false, false },
	{ TEXT("SprintStopB_MaxDown"),      2.0f, {0, 0},  RequestedMaxDownPitchDeg,  0.f,   false, false, false },
	{ TEXT("SprintStopFinalSettle"),    1.0f, {0, 0},  RequestedMaxDownPitchDeg,  0.f,   false, false, false },
};
static constexpr int32 GNumSprintStopLoopPhases = UE_ARRAY_COUNT(GSprintStopLoopPhases);

} // namespace ClipMatrixHelpers
