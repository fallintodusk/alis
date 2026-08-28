// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Presentation/ProjectWorldProductRouteProgress.h"

class AActor;
class ACharacter;
class APlayerController;
class IInteractionService;
class UActorComponent;
class UCharacterMovementComponent;
class UWorld;

struct FProjectWorldProductRouteGateConfig
{
	FString OperationId;
	FString ResultPath;
	FString MapPackage;
	FString RuntimeProfileId;
	FString RuntimeProfileHash;
	FString MachineProfileId;
	FVector EdgeLocation = FVector::ZeroVector;
	bool bRestorePreviewFlight = false;
};

class FProjectWorldProductRouteGate
{
public:
	~FProjectWorldProductRouteGate();

	void StartIfRequested();

private:
	enum class EPhase : uint8
	{
		WaitingForWorld,
		SettlingAtCenter,
		MovingNormally,
		ProbingCenter,
		WaitingForInteraction,
		WaitingAtEdge,
		WaitingAtCenter,
		SettlingForScreenshot,
		WaitingForScreenshot,
		Finished
	};

	bool ParseConfig(FString& OutError);
	bool Tick(float DeltaSeconds);
	bool TryAcquireProductWorld();
	bool TickCenterSettlement();
	bool TickNormalMovement();
	bool ProbeCenterContracts();
	bool BeginGameplayInteraction();
	void HandleInteraction(AActor* Target, AActor* Instigator);
	void BeginEdgeTraversal();
	bool TickEdgeSettlement();
	void BeginCenterReturn();
	bool TickCenterReturn();
	bool IsStreamingCompleted() const;
	bool InspectRuntimeOwnership(FString& OutError);
	bool ProbeTaggedCollision(
		FName RequiredTag,
		FString& OutActorName,
		int32& OutCandidateCount,
		int32& OutBlockingPrimitiveCount) const;
	bool ProbeTerrainCollision(
		FString& OutActorName,
		int32& OutCandidateCount,
		int32& OutBlockingPrimitiveCount) const;
	bool ProbeActorCollision(const AActor& Actor) const;
	AActor* FindNearestGameplayObject() const;
	UActorComponent* FindInteractionComponent() const;
	FString FindNearestCellMarker(const FVector& Location, const FString& ExcludedMarker = FString()) const;
	bool HasCellMarker(const FString& Marker) const;
	bool MovePlayerTo(const FVector& Location, bool bPlaceOnGround);
	bool FindGroundLocation(const FVector& Location, FVector& OutGroundLocation) const;
	void RequestScreenshot();
	void FinishAccepted();
	void FinishRejected(const FString& Code, const FString& Message);
	void WriteResult(const FString& Status, const FString& ErrorCode, const FString& ErrorMessage);
	void SetPhase(EPhase NewPhase);
	void ReleaseInteractionSubscription();

	FProjectWorldProductRouteGateConfig Config;
	FProjectWorldProductRouteProgress Progress;
	TSet<FString> ObservedRuntimeRoles;
	TWeakObjectPtr<UWorld> ProductWorld;
	TWeakObjectPtr<APlayerController> PlayerController;
	TWeakObjectPtr<ACharacter> PlayerCharacter;
	TWeakObjectPtr<UCharacterMovementComponent> CharacterMovement;
	TSharedPtr<IInteractionService> InteractionService;
	FDelegateHandle InteractionHandle;
	FTSTicker::FDelegateHandle TickerHandle;
	FVector CenterLocation = FVector::ZeroVector;
	FVector MovementStart = FVector::ZeroVector;
	FString CenterCellMarker;
	FString EdgeCellMarker;
	FString InteractionObjectId;
	FString TerrainCollisionActor;
	FString RoadCollisionActor;
	FString BuildingCollisionActor;
	FString ScreenshotPath;
	FString LastReadinessError;
	EPhase Phase = EPhase::Finished;
	double GateStartedSeconds = 0.0;
	double PhaseStartedSeconds = 0.0;
	int32 PhaseFrameCount = 0;
	float MovementDistanceCentimeters = 0.0f;
	int32 TerrainCandidateCount = 0;
	int32 TerrainBlockingPrimitiveCount = 0;
	int32 RoadCandidateCount = 0;
	int32 RoadBlockingPrimitiveCount = 0;
	int32 BuildingCandidateCount = 0;
	int32 BuildingBlockingPrimitiveCount = 0;
	bool bInteractionDispatchAccepted = false;
	bool bPreviewFlightRestored = false;
};
