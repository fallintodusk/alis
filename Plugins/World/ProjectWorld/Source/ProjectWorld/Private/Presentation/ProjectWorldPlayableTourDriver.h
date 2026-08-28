// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class ACharacter;
class APlayerController;
class FJsonObject;

enum class EProjectWorldPlayableTourResult : uint8
{
	Running,
	Accepted,
	Rejected
};

struct FProjectWorldPlayableTourEvidence
{
	FString InputMethod;
	TArray<FString> CompletedPhases;
	FVector StartLocation = FVector::ZeroVector;
	FVector HighLocation = FVector::ZeroVector;
	FVector EdgeLocation = FVector::ZeroVector;
	FVector EndLocation = FVector::ZeroVector;
	double AscentCentimeters = 0.0;
	double DescentCentimeters = 0.0;
	double HorizontalDisplacementCentimeters = 0.0;
	double SlideDisplacementCentimeters = 0.0;
	double MaximumYawChangeDegrees = 0.0;
	double MaximumConsecutiveStallSeconds = 0.0;
	double DurationSeconds = 0.0;
	int32 InputEventCount = 0;
	int32 WaypointsReached = 0;
	int32 ObstacleClearanceCount = 0;
	bool bCollisionBlockedDescent = false;
	bool bCollisionSlide = false;
};

class FProjectWorldPlayableTourDriver
{
public:
	~FProjectWorldPlayableTourDriver();

	bool Initialize(
		APlayerController& InController,
		ACharacter& InCharacter,
		const TArray<FVector>& InWaypoints,
		FString& OutError);
	EProjectWorldPlayableTourResult Tick(float DeltaSeconds, FString& OutError);
	void ReleaseInputs();
	void AppendReceiptFields(FJsonObject& Receipt) const;

	const FProjectWorldPlayableTourEvidence& GetEvidence() const { return Evidence; }
	bool HasReachedEdge() const { return bReachedEdge; }
	bool HasReturnedToCenter() const { return bReturnedToCenter; }

private:
	enum class EPhase : uint8
	{
		Ascending,
		Traversing,
		Descending,
		Sliding,
		Finished
	};

	void SetPhase(EPhase NewPhase, const TCHAR* CompletedPhase = nullptr);
	void Hold(const FKey& Key);
	void Release(const FKey& Key);
	void SendLook(double YawErrorDegrees);
	EProjectWorldPlayableTourResult TickAscending(FString& OutError);
	EProjectWorldPlayableTourResult TickTraversing(float DeltaSeconds, FString& OutError);
	EProjectWorldPlayableTourResult TickDescending(FString& OutError);
	EProjectWorldPlayableTourResult TickSliding(FString& OutError);
	EProjectWorldPlayableTourResult Reject(const FString& Error, FString& OutError);

	TWeakObjectPtr<APlayerController> Controller;
	TWeakObjectPtr<ACharacter> Character;
	TArray<FVector> Waypoints;
	TSet<FKey> HeldKeys;
	FProjectWorldPlayableTourEvidence Evidence;
	FVector PriorLocation = FVector::ZeroVector;
	FVector SlideStartLocation = FVector::ZeroVector;
	double StartedSeconds = 0.0;
	double PhaseStartedSeconds = 0.0;
	double CurrentLegTimeoutSeconds = 0.0;
	double PriorTargetDistanceCentimeters = 0.0;
	double ConsecutiveStallSeconds = 0.0;
	double NextDiagnosticSeconds = 0.0;
	double ObstacleClearanceStartedZ = 0.0;
	double StableCollisionSeconds = 0.0;
	double PriorYawDegrees = 0.0;
	int32 WaypointIndex = 0;
	int32 EdgeWaypointIndex = INDEX_NONE;
	bool bClearingObstacle = false;
	bool bReachedEdge = false;
	bool bReturnedToCenter = false;
	EPhase Phase = EPhase::Finished;
};
