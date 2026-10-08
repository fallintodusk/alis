// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Presentation/ProjectWorldPlayableTourDriver.h"

#include "Components/CapsuleComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "InputCoreTypes.h"
#include "Interfaces/IGameMenuService.h"
#include "ProjectServiceLocator.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectWorldPlayableTour, Log, All);

namespace
{
	constexpr double AscendHeightCentimeters = 7500.0;
	constexpr double MenuTransitionTimeoutSeconds = 3.0;
	constexpr double AscendTimeoutSeconds = 20.0;
	constexpr double ArrivalRadiusCentimeters = 25000.0;
	constexpr double StandardCenterArrivalRadiusCentimeters = 2500.0;
	constexpr double PreciseCenterArrivalRadiusCentimeters = 500.0;
	constexpr double SettledCenterSpeedCentimetersPerSecond = 10.0;
	constexpr double MinimumTurnBeforeTravelDegrees = 12.0;
	constexpr double DescentTimeoutSeconds = 45.0;
	constexpr double MinimumDescentBeforeCollisionCentimeters = 3000.0;
	constexpr double StableCollisionRequiredSeconds = 0.5;
	constexpr double SlideDurationSeconds = 1.5;
	constexpr double MinimumSlideCentimeters = 100.0;
	constexpr double ObstacleStallSeconds = 1.0;
	constexpr double MaximumObstacleAscentCentimeters = 75000.0;
	constexpr double OverhangRecoveryTimeoutSeconds = 10.0;
	constexpr double OverhangProbeCentimeters = 200.0;
	constexpr double OverallTimeoutSeconds = 360.0;
}

FProjectWorldPlayableTourDriver::~FProjectWorldPlayableTourDriver()
{
	ReleaseInputs();
}

bool FProjectWorldPlayableTourDriver::Initialize(
	APlayerController& InController,
	ACharacter& InCharacter,
	const TArray<FVector>& InWaypoints,
	FString& OutError,
	bool bPreciseCenterReturn)
{
	UCharacterMovementComponent* Movement = InCharacter.GetCharacterMovement();
	if (InWaypoints.Num() < 3 || Movement == nullptr || !Movement->IsFlying())
	{
		OutError = TEXT("Playable tour requires preview flight and at least three territory waypoints.");
		return false;
	}
	if (InCharacter.GetCapsuleComponent()->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
	{
		OutError = TEXT("Playable tour requires the production character capsule collision.");
		return false;
	}

	Controller = &InController;
	Character = &InCharacter;
	Waypoints = InWaypoints;
	FinalCenterArrivalRadiusCentimeters = bPreciseCenterReturn
		? PreciseCenterArrivalRadiusCentimeters : StandardCenterArrivalRadiusCentimeters;
	Evidence.InputMethod = TEXT("APlayerController::InputKey/FInputKeyEventArgs::CreateSimulated");
	Evidence.StartLocation = InCharacter.GetActorLocation();
	double FarthestDistanceSquared = 0.0;
	for (int32 Index = 1; Index < Waypoints.Num(); ++Index)
	{
		const double DistanceSquared = FVector2D::DistSquared(
			FVector2D(Evidence.StartLocation), FVector2D(Waypoints[Index]));
		if (DistanceSquared > FarthestDistanceSquared)
		{
			FarthestDistanceSquared = DistanceSquared;
			EdgeWaypointIndex = Index;
		}
	}
	PriorLocation = Evidence.StartLocation;
	PriorYawDegrees = InController.GetControlRotation().Yaw;
	StartedSeconds = FPlatformTime::Seconds();
	WaypointIndex = 1;
	SetPhase(EPhase::OpeningMenu);
	return true;
}

EProjectWorldPlayableTourResult FProjectWorldPlayableTourDriver::Tick(
	float DeltaSeconds,
	FString& OutError)
{
	if (!Controller.IsValid() || !Character.IsValid())
	{
		return Reject(TEXT("The possessed production character was lost during playable traversal."), OutError);
	}
	if (FPlatformTime::Seconds() - StartedSeconds > OverallTimeoutSeconds)
	{
		return Reject(TEXT("The bounded real-input playable tour timed out."), OutError);
	}

	const FVector Location = Character->GetActorLocation();
	Evidence.HorizontalDisplacementCentimeters += FVector2D::Distance(
		FVector2D(PriorLocation),
		FVector2D(Location));
	PriorLocation = Location;

	switch (Phase)
	{
	case EPhase::OpeningMenu:
		return TickOpeningMenu(OutError);
	case EPhase::ClosingMenu:
		return TickClosingMenu(OutError);
	case EPhase::Ascending:
		return TickAscending(OutError);
	case EPhase::Traversing:
		return TickTraversing(DeltaSeconds, OutError);
	case EPhase::Descending:
		if (Evidence.HighLocation.Z - Location.Z >= MinimumDescentBeforeCollisionCentimeters &&
			FMath::Abs(Character->GetVelocity().Z) < 5.0)
		{
			StableCollisionSeconds += DeltaSeconds;
		}
		else
		{
			StableCollisionSeconds = 0.0;
		}
		return TickDescending(OutError);
	case EPhase::Sliding:
		return TickSliding(OutError);
	case EPhase::Finished:
		return EProjectWorldPlayableTourResult::Accepted;
	default:
		return Reject(TEXT("Playable tour entered an unknown phase."), OutError);
	}
}

void FProjectWorldPlayableTourDriver::ReleaseInputs()
{
	const TArray<FKey> Keys = HeldKeys.Array();
	for (const FKey& Key : Keys)
	{
		Release(Key);
	}
	bBackingFromOverhang = false;
}

void FProjectWorldPlayableTourDriver::AppendReceiptFields(FJsonObject& Receipt) const
{
	Receipt.SetStringField(TEXT("input_method"), Evidence.InputMethod);
	Receipt.SetNumberField(TEXT("input_event_count"), Evidence.InputEventCount);
	Receipt.SetNumberField(TEXT("waypoints_reached"), Evidence.WaypointsReached);
	Receipt.SetNumberField(TEXT("obstacle_clearance_count"), Evidence.ObstacleClearanceCount);
	Receipt.SetNumberField(TEXT("ascent_cm"), Evidence.AscentCentimeters);
	Receipt.SetNumberField(TEXT("descent_cm"), Evidence.DescentCentimeters);
	Receipt.SetNumberField(TEXT("horizontal_displacement_cm"),
		Evidence.HorizontalDisplacementCentimeters);
	Receipt.SetNumberField(TEXT("slide_displacement_cm"), Evidence.SlideDisplacementCentimeters);
	Receipt.SetNumberField(TEXT("maximum_yaw_change_degrees"), Evidence.MaximumYawChangeDegrees);
	Receipt.SetNumberField(TEXT("maximum_consecutive_stall_seconds"),
		Evidence.MaximumConsecutiveStallSeconds);
	Receipt.SetNumberField(TEXT("playable_tour_duration_seconds"), Evidence.DurationSeconds);
	Receipt.SetBoolField(TEXT("collision_blocked_descent"), Evidence.bCollisionBlockedDescent);
	Receipt.SetBoolField(TEXT("collision_slide"), Evidence.bCollisionSlide);
	Receipt.SetBoolField(TEXT("pause_menu_opened"), Evidence.bPauseMenuOpened);
	Receipt.SetBoolField(TEXT("pause_menu_closed"), Evidence.bPauseMenuClosed);
	TArray<TSharedPtr<FJsonValue>> PhaseValues;
	for (const FString& PhaseName : Evidence.CompletedPhases)
	{
		PhaseValues.Add(MakeShared<FJsonValueString>(PhaseName));
	}
	Receipt.SetArrayField(TEXT("input_phases"), PhaseValues);
}

void FProjectWorldPlayableTourDriver::SetPhase(EPhase NewPhase, const TCHAR* CompletedPhase)
{
	if (CompletedPhase != nullptr)
	{
		Evidence.CompletedPhases.Add(CompletedPhase);
		UE_LOG(LogProjectWorldPlayableTour, Display,
			TEXT("[FProjectWorldPlayableTourDriver::SetPhase] Completed - phase=%s"),
			CompletedPhase);
	}
	Phase = NewPhase;
	PhaseStartedSeconds = FPlatformTime::Seconds();
}

void FProjectWorldPlayableTourDriver::Hold(const FKey& Key)
{
	if (!Controller.IsValid())
	{
		return;
	}
	const EInputEvent Event = HeldKeys.Contains(Key) ? IE_Repeat : IE_Pressed;
	Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Key, Event, 1.0f));
	HeldKeys.Add(Key);
	++Evidence.InputEventCount;
}

void FProjectWorldPlayableTourDriver::Release(const FKey& Key)
{
	if (!HeldKeys.Remove(Key))
	{
		return;
	}
	if (Controller.IsValid())
	{
		Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Released, 0.0f));
		++Evidence.InputEventCount;
	}
}

void FProjectWorldPlayableTourDriver::Tap(const FKey& Key)
{
	if (!Controller.IsValid())
	{
		return;
	}
	Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Pressed, 1.0f));
	Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Released, 0.0f));
	Evidence.InputEventCount += 2;
}

EProjectWorldPlayableTourResult FProjectWorldPlayableTourDriver::TickOpeningMenu(FString& OutError)
{
	const TSharedPtr<IGameMenuService> MenuService = FProjectServiceLocator::Resolve<IGameMenuService>();
	if (!MenuService.IsValid())
	{
		return Reject(TEXT("The packaged route cannot resolve the game-menu service."), OutError);
	}
	if (!bMenuInputSent)
	{
		Tap(EKeys::Escape);
		bMenuInputSent = true;
		return EProjectWorldPlayableTourResult::Running;
	}
	if (MenuService->IsVisible(*Controller) && Controller->IsPaused())
	{
		Evidence.bPauseMenuOpened = true;
		bMenuInputSent = false;
		SetPhase(EPhase::ClosingMenu, TEXT("pause_menu_open"));
		return EProjectWorldPlayableTourResult::Running;
	}
	if (FPlatformTime::Seconds() - PhaseStartedSeconds > MenuTransitionTimeoutSeconds)
	{
		return Reject(TEXT("Escape did not open and pause the packaged game menu."), OutError);
	}
	return EProjectWorldPlayableTourResult::Running;
}

EProjectWorldPlayableTourResult FProjectWorldPlayableTourDriver::TickClosingMenu(FString& OutError)
{
	const TSharedPtr<IGameMenuService> MenuService = FProjectServiceLocator::Resolve<IGameMenuService>();
	if (!MenuService.IsValid())
	{
		return Reject(TEXT("The packaged route lost the game-menu service."), OutError);
	}
	if (!bMenuInputSent)
	{
		Tap(EKeys::Escape);
		bMenuInputSent = true;
		return EProjectWorldPlayableTourResult::Running;
	}
	if (!MenuService->IsVisible(*Controller) && !Controller->IsPaused())
	{
		Evidence.bPauseMenuClosed = true;
		bMenuInputSent = false;
		SetPhase(EPhase::Ascending, TEXT("pause_menu_close"));
		return EProjectWorldPlayableTourResult::Running;
	}
	if (FPlatformTime::Seconds() - PhaseStartedSeconds > MenuTransitionTimeoutSeconds)
	{
		return Reject(TEXT("Escape did not close and unpause the packaged game menu."), OutError);
	}
	return EProjectWorldPlayableTourResult::Running;
}

void FProjectWorldPlayableTourDriver::SendLook(double YawErrorDegrees)
{
	if (!Controller.IsValid())
	{
		return;
	}
	const double CurrentYaw = Controller->GetControlRotation().Yaw;
	Evidence.MaximumYawChangeDegrees = FMath::Max(
		Evidence.MaximumYawChangeDegrees,
		FMath::Abs(FMath::FindDeltaAngleDegrees(PriorYawDegrees, CurrentYaw)));
	PriorYawDegrees = CurrentYaw;

	const FVector2D LookInput(FMath::Clamp(YawErrorDegrees * 0.2, -10.0, 10.0), 0.0);
	Controller->InputKey(FInputKeyEventArgs::CreateSimulated(
		EKeys::MouseX, IE_Axis, LookInput.X));
	++Evidence.InputEventCount;
}

EProjectWorldPlayableTourResult FProjectWorldPlayableTourDriver::TickAscending(FString& OutError)
{
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (Movement == nullptr || !Movement->IsFlying())
	{
		return Reject(FString::Printf(
			TEXT("PreviewFlight left flying movement mode before ascent (mode=%d)."),
			Movement == nullptr ? INDEX_NONE : static_cast<int32>(Movement->MovementMode)),
			OutError);
	}
	Hold(EKeys::SpaceBar);
	const FVector Location = Character->GetActorLocation();
	const double Ascent = Location.Z - Evidence.StartLocation.Z;
	const double PhaseSeconds = FPlatformTime::Seconds() - PhaseStartedSeconds;
	if (Location.Z >= Evidence.StartLocation.Z + AscendHeightCentimeters)
	{
		Release(EKeys::SpaceBar);
		Evidence.HighLocation = Location;
		Evidence.AscentCentimeters = Ascent;
		SetPhase(EPhase::Traversing, TEXT("ascend"));
		return EProjectWorldPlayableTourResult::Running;
	}
	if (PhaseSeconds > AscendTimeoutSeconds)
	{
		return Reject(FString::Printf(
			TEXT("Held Space did not produce the required preview-flight ascent: delta_z=%.1f, ")
			TEXT("velocity=%s, last_input=%s, move_ignored=%d."),
			Location.Z - Evidence.StartLocation.Z,
			*Character->GetVelocity().ToCompactString(),
			*Character->GetLastMovementInputVector().ToCompactString(),
			Controller->IsMoveInputIgnored() ? 1 : 0),
			OutError);
	}
	return EProjectWorldPlayableTourResult::Running;
}

EProjectWorldPlayableTourResult FProjectWorldPlayableTourDriver::TickTraversing(
	float DeltaSeconds,
	FString& OutError)
{
	if (!Waypoints.IsValidIndex(WaypointIndex))
	{
		ReleaseInputs();
		StableCollisionSeconds = 0.0;
		SetPhase(EPhase::Descending, TEXT("centre_dense_edge_centre"));
		return EProjectWorldPlayableTourResult::Running;
	}

	const FVector Location = Character->GetActorLocation();
	const FVector Target = Waypoints[WaypointIndex];
	const FVector2D Delta(FVector2D(Target) - FVector2D(Location));
	const double Distance = Delta.Size();
	if (CurrentLegTimeoutSeconds <= 0.0)
	{
		CurrentLegTimeoutSeconds = FMath::Max(45.0, Distance / 6000.0 + 30.0);
		PhaseStartedSeconds = FPlatformTime::Seconds();
		PriorTargetDistanceCentimeters = Distance;
		ConsecutiveStallSeconds = 0.0;
		NextDiagnosticSeconds = PhaseStartedSeconds + 5.0;
		UE_LOG(LogProjectWorldPlayableTour, Display,
			TEXT("[FProjectWorldPlayableTourDriver::TickTraversing] Leg started - index=%d distance_cm=%.1f timeout_s=%.1f"),
			WaypointIndex, Distance, CurrentLegTimeoutSeconds);
	}
	const bool bFinalLeg = WaypointIndex == Waypoints.Num() - 1;
	const bool bPreciseFinalLeg = bFinalLeg &&
		FinalCenterArrivalRadiusCentimeters == PreciseCenterArrivalRadiusCentimeters;
	const double ArrivalRadius = bFinalLeg
		? FinalCenterArrivalRadiusCentimeters
		: ArrivalRadiusCentimeters;
	const FVector2D HorizontalVelocity(Character->GetVelocity());
	double StoppingDistance = 0.0;
	if (bPreciseFinalLeg)
	{
		const double Braking = Character->GetCharacterMovement()->GetMaxBrakingDeceleration();
		if (!FMath::IsFinite(Braking) || Braking <= 0.0)
		{
			return Reject(TEXT("The final flight approach requires positive native braking deceleration."), OutError);
		}
		const double ApproachSpeed = FMath::Max(0.0,
			FVector2D::DotProduct(HorizontalVelocity, Delta.GetSafeNormal()));
		StoppingDistance = FMath::Square(ApproachSpeed) / (2.0 * Braking);
	}
	if (Distance <= ArrivalRadius && (!bPreciseFinalLeg ||
		HorizontalVelocity.Size() <= SettledCenterSpeedCentimetersPerSecond))
	{
		Release(EKeys::W);
		Release(EKeys::S);
		Release(EKeys::A);
		Release(EKeys::D);
		Release(EKeys::SpaceBar);
		bClearingObstacle = false;
		bBackingFromOverhang = false;
		++Evidence.WaypointsReached;
		if (WaypointIndex == EdgeWaypointIndex)
		{
			Evidence.EdgeLocation = Location;
			bReachedEdge = true;
		}
		if (bFinalLeg)
		{
			bReturnedToCenter = true;
			UE_LOG(LogProjectWorldPlayableTour, Display,
				TEXT("[FProjectWorldPlayableTourDriver::TickTraversing] Center return - distance_cm=%.1f speed_cm_s=%.1f location=%s"),
				Distance, HorizontalVelocity.Size(), *Location.ToCompactString());
		}
		++WaypointIndex;
		CurrentLegTimeoutSeconds = 0.0;
		PriorTargetDistanceCentimeters = 0.0;
		ConsecutiveStallSeconds = 0.0;
		PhaseStartedSeconds = FPlatformTime::Seconds();
		return EProjectWorldPlayableTourResult::Running;
	}

	const double DesiredYaw = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
	const double YawError = FMath::FindDeltaAngleDegrees(
		Controller->GetControlRotation().Yaw,
		DesiredYaw);
	SendLook(YawError);
	// Vertical or lateral acceleration also suppresses native flight braking.
	const bool bBrakingForCenter = bPreciseFinalLeg &&
		(Distance <= ArrivalRadius || Distance <= StoppingDistance + ArrivalRadius * 0.5);
	if (bBrakingForCenter)
	{
		ReleaseInputs();
		bClearingObstacle = false;
	}
	else if (!bBackingFromOverhang && FMath::Abs(YawError) <= MinimumTurnBeforeTravelDegrees)
	{
		Hold(EKeys::W);
	}
	else if (!bBackingFromOverhang)
	{
		Release(EKeys::W);
	}

	const bool bMadeForwardProgress = Distance < PriorTargetDistanceCentimeters - 10.0;
	if (HeldKeys.Contains(EKeys::W) && !bMadeForwardProgress)
	{
		ConsecutiveStallSeconds += DeltaSeconds;
		Evidence.MaximumConsecutiveStallSeconds = FMath::Max(
			Evidence.MaximumConsecutiveStallSeconds,
			ConsecutiveStallSeconds);
	}
	else
	{
		ConsecutiveStallSeconds = 0.0;
	}
	const double NowSeconds = FPlatformTime::Seconds();
	if (bClearingObstacle)
	{
		if (TickObstacleClearance(bMadeForwardProgress, OutError) == EProjectWorldPlayableTourResult::Rejected)
		{
			return EProjectWorldPlayableTourResult::Rejected;
		}
	}
	else if (HeldKeys.Contains(EKeys::W) &&
		ConsecutiveStallSeconds >= ObstacleStallSeconds)
	{
		bClearingObstacle = true;
		ObstacleClearanceStartedZ = Location.Z;
		++Evidence.ObstacleClearanceCount;
		Hold(EKeys::SpaceBar);
		UE_LOG(LogProjectWorldPlayableTour, Display,
			TEXT("[FProjectWorldPlayableTourDriver::TickTraversing] Obstacle clearance started - index=%d direction=up location=%s"),
			WaypointIndex, *Location.ToCompactString());
	}
	PriorTargetDistanceCentimeters = Distance;
	if (NowSeconds >= NextDiagnosticSeconds)
	{
		UE_LOG(LogProjectWorldPlayableTour, Display,
			TEXT("[FProjectWorldPlayableTourDriver::TickTraversing] Progress - index=%d distance_cm=%.1f location=%s velocity=%s yaw=%.1f desired_yaw=%.1f yaw_error=%.1f w_held=%s stall_s=%.1f"),
			WaypointIndex,
			Distance,
			*Location.ToCompactString(),
			*Character->GetVelocity().ToCompactString(),
			Controller->GetControlRotation().Yaw,
			DesiredYaw,
			YawError,
			HeldKeys.Contains(EKeys::W) ? TEXT("true") : TEXT("false"),
			ConsecutiveStallSeconds);
		NextDiagnosticSeconds = NowSeconds + 5.0;
	}

	if (FPlatformTime::Seconds() - PhaseStartedSeconds > CurrentLegTimeoutSeconds)
	{
		return Reject(TEXT("Real look/WASD input did not reach a territory waypoint."), OutError);
	}
	return EProjectWorldPlayableTourResult::Running;
}

EProjectWorldPlayableTourResult FProjectWorldPlayableTourDriver::TickObstacleClearance(
	bool bMadeForwardProgress, FString& OutError)
{
	const FVector Location = Character->GetActorLocation();
	if (Location.Z - ObstacleClearanceStartedZ >= MaximumObstacleAscentCentimeters)
	{
		return Reject(TEXT("Real flight input could not clear a blocking volume within the bounded ascent."), OutError);
	}
	UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	UWorld* World = Character->GetWorld();
	if (Capsule == nullptr || World == nullptr)
	{
		return Reject(TEXT("Obstacle clearance lost the production collision owner."), OutError);
	}
	FHitResult Hit;
	const bool bBlockedUp = World->SweepSingleByChannel(Hit, Location,
		Location + FVector(0, 0, OverhangProbeCentimeters), Capsule->GetComponentQuat(),
		Capsule->GetCollisionObjectType(),
		FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()),
		FCollisionQueryParams(SCENE_QUERY_STAT(ProjectWorldOverhangClearance), false, Character.Get()),
		FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()));
	const double Now = FPlatformTime::Seconds();
	if (!bBackingFromOverhang && bBlockedUp && Hit.ImpactNormal.Z < -0.5)
	{
		Release(EKeys::W);
		bBackingFromOverhang = true;
		OverhangRecoveryStartedSeconds = Now;
		OverhangClearanceZ = Hit.ImpactPoint.Z + 2.0 * Capsule->GetScaledCapsuleHalfHeight();
		UE_LOG(LogProjectWorldPlayableTour, Display,
			TEXT("[FProjectWorldPlayableTourDriver::TickObstacleClearance] Overhang retreat started - index=%d blocker=%s component=%s normal=%s clearance_z=%.1f"),
			WaypointIndex, *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()),
			*Hit.ImpactNormal.ToCompactString(), OverhangClearanceZ);
	}
	Hold(EKeys::SpaceBar);
	if (bBackingFromOverhang)
	{
		const FRotationMatrix InputRotation(FRotator(0, Controller->GetControlRotation().Yaw, 0));
		const FKey EscapeKeys[] = { EKeys::S, EKeys::D, EKeys::A, EKeys::W };
		const FVector EscapeDirections[] = { -InputRotation.GetUnitAxis(EAxis::X),
			InputRotation.GetUnitAxis(EAxis::Y), -InputRotation.GetUnitAxis(EAxis::Y),
			InputRotation.GetUnitAxis(EAxis::X) };
		bool OpenDirections[UE_ARRAY_COUNT(EscapeKeys)];
		int32 SelectedDirection = INDEX_NONE;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(EscapeKeys); ++Index)
		{
			FHitResult EscapeHit;
			OpenDirections[Index] = !World->SweepSingleByChannel(EscapeHit, Location,
				Location + EscapeDirections[Index] * OverhangProbeCentimeters, Capsule->GetComponentQuat(),
				Capsule->GetCollisionObjectType(),
				FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()),
				FCollisionQueryParams(SCENE_QUERY_STAT(ProjectWorldOverhangEscape), false, Character.Get()),
				FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()));
			if (OpenDirections[Index] && HeldKeys.Contains(EscapeKeys[Index]))
			{
				SelectedDirection = Index;
			}
		}
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(EscapeKeys); ++Index)
		{
			if (SelectedDirection == INDEX_NONE && OpenDirections[Index])
			{
				SelectedDirection = Index;
			}
		}
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(EscapeKeys); ++Index)
		{
			if (Index == SelectedDirection)
			{
				if (!HeldKeys.Contains(EscapeKeys[Index]))
				{
					UE_LOG(LogProjectWorldPlayableTour, Display,
						TEXT("[FProjectWorldPlayableTourDriver::TickObstacleClearance] Overhang escape input - index=%d key=%s location=%s"),
						WaypointIndex, *EscapeKeys[Index].ToString(), *Location.ToCompactString());
				}
				Hold(EscapeKeys[Index]);
			}
			else
			{
				Release(EscapeKeys[Index]);
			}
		}
		if (!bBlockedUp && Location.Z >= OverhangClearanceZ)
		{
			Release(EKeys::W);
			Release(EKeys::S);
			Release(EKeys::A);
			Release(EKeys::D);
			bBackingFromOverhang = false;
			UE_LOG(LogProjectWorldPlayableTour, Display,
				TEXT("[FProjectWorldPlayableTourDriver::TickObstacleClearance] Overhang retreat completed - index=%d location=%s duration_s=%.1f"),
				WaypointIndex, *Location.ToCompactString(), Now - OverhangRecoveryStartedSeconds);
		}
		else if (Now - OverhangRecoveryStartedSeconds > OverhangRecoveryTimeoutSeconds)
		{
			return Reject(TEXT("Real backward/upward input could not clear an overhang within the bounded retreat."), OutError);
		}
		return EProjectWorldPlayableTourResult::Running;
	}
	if (bMadeForwardProgress)
	{
		Release(EKeys::SpaceBar);
		bClearingObstacle = false;
		ConsecutiveStallSeconds = 0.0;
		UE_LOG(LogProjectWorldPlayableTour, Display,
			TEXT("[FProjectWorldPlayableTourDriver::TickObstacleClearance] Obstacle clearance completed - index=%d ascent_cm=%.1f"),
			WaypointIndex, Location.Z - ObstacleClearanceStartedZ);
	}
	return EProjectWorldPlayableTourResult::Running;
}

EProjectWorldPlayableTourResult FProjectWorldPlayableTourDriver::TickDescending(FString& OutError)
{
	Hold(EKeys::LeftControl);
	if (StableCollisionSeconds >= StableCollisionRequiredSeconds)
	{
		Evidence.bCollisionBlockedDescent = true;
		Evidence.DescentCentimeters = Evidence.HighLocation.Z - Character->GetActorLocation().Z;
		SlideStartLocation = Character->GetActorLocation();
		UE_LOG(LogProjectWorldPlayableTour, Display,
			TEXT("[FProjectWorldPlayableTourDriver::TickDescending] Collision - location=%s yaw=%.1f velocity=%s"),
			*SlideStartLocation.ToCompactString(), Controller->GetControlRotation().Yaw,
			*Character->GetVelocity().ToCompactString());
		SetPhase(EPhase::Sliding, TEXT("descend_to_collision"));
		return EProjectWorldPlayableTourResult::Running;
	}
	if (FPlatformTime::Seconds() - PhaseStartedSeconds > DescentTimeoutSeconds)
	{
		return Reject(TEXT("Held Ctrl did not reach a swept blocking surface."), OutError);
	}
	return EProjectWorldPlayableTourResult::Running;
}

EProjectWorldPlayableTourResult FProjectWorldPlayableTourDriver::TickSliding(FString& OutError)
{
	Hold(EKeys::LeftControl);
	Hold(EKeys::W);
	if (FPlatformTime::Seconds() - PhaseStartedSeconds < SlideDurationSeconds)
	{
		return EProjectWorldPlayableTourResult::Running;
	}

	ReleaseInputs();
	Evidence.EndLocation = Character->GetActorLocation();
	Evidence.SlideDisplacementCentimeters = FVector2D::Distance(
		FVector2D(SlideStartLocation),
		FVector2D(Evidence.EndLocation));
	Evidence.bCollisionSlide = Evidence.SlideDisplacementCentimeters >= MinimumSlideCentimeters;
	Evidence.DurationSeconds = FPlatformTime::Seconds() - StartedSeconds;
	UE_LOG(LogProjectWorldPlayableTour, Display,
		TEXT("[FProjectWorldPlayableTourDriver::TickSliding] Forward slide - start=%s end=%s displacement_cm=%.1f yaw=%.1f"),
		*SlideStartLocation.ToCompactString(), *Evidence.EndLocation.ToCompactString(),
		Evidence.SlideDisplacementCentimeters, Controller->GetControlRotation().Yaw);
	if (!Evidence.bCollisionSlide)
	{
		return Reject(TEXT("W input did not slide along the blocking surface."), OutError);
	}
	if (Evidence.MaximumYawChangeDegrees < 1.0)
	{
		return Reject(TEXT("Simulated look input did not rotate the player controller."), OutError);
	}
	SetPhase(EPhase::Finished, TEXT("collision_slide"));
	return EProjectWorldPlayableTourResult::Accepted;
}

EProjectWorldPlayableTourResult FProjectWorldPlayableTourDriver::Reject(
	const FString& Error,
	FString& OutError)
{
	ReleaseInputs();
	Evidence.EndLocation = Character.IsValid() ? Character->GetActorLocation() : PriorLocation;
	Evidence.DurationSeconds = FPlatformTime::Seconds() - StartedSeconds;
	OutError = Error;
	UE_LOG(LogProjectWorldPlayableTour, Error,
		TEXT("[FProjectWorldPlayableTourDriver::Reject] Rejected - reason=%s"), *Error);
	Phase = EPhase::Finished;
	return EProjectWorldPlayableTourResult::Rejected;
}
