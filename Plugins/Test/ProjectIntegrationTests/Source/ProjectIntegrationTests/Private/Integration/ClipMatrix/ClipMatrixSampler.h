// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
// Extracted from FirstPersonClipMatrixTest: pure measurement functions.
#pragma once
#include "ClipMatrixTypes.h"

class ACharacter;
class APlayerController;
class UCameraComponent;

namespace ClipMatrixHelpers
{
	// Collect a single frame sample. Pure measurement, no pass/fail logic.
	FFrameSample CollectSample(
		ACharacter* Character,
		USkeletalMeshComponent* OwnerMesh,
		APlayerController* PC,
		int32 PhaseIdx,
		float Time,
		float RequestedPitchDeg,
		const FVector& NeckOffsetFromCamera,
		const FVector& ExpectedCameraRelativeOffset);

	// Aggregate all samples for a phase into a summary with consecutive-frame tracking.
	FPhaseSummary BuildPhaseSummary(
		const TArray<FFrameSample>& AllSamples,
		int32 PhaseIdx,
		const FClipPhase* ActivePhases,
		int32 ActivePhaseCount);
}
