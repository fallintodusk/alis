// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Presentation/ProjectWorldProductRouteGate.h"

#include "Presentation/ProjectWorldPresentationSampling.h"
#include "Presentation/ProjectWorldProductRouteCollision.h"
#include "Presentation/ProjectWorldRuntimeScreenshotCapture.h"
#include "Presentation/ProjectWorldScreenshotValidation.h"
#include "Interfaces/IInteractionService.h"
#include "ProjectServiceLocator.h"

#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GenericPlatform/GenericPlatformDriver.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RHI.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"
#include "WorldPartition/WorldPartition.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectWorldProductRouteGate, Log, All);

namespace
{
	constexpr double ProductRouteGateTimeoutSeconds = 300.0;
	constexpr double WorldTimeoutSeconds = 180.0;
	constexpr double StreamingTimeoutSeconds = 90.0;
	constexpr double MovementTimeoutSeconds = 8.0;
	constexpr double InteractionTimeoutSeconds = 8.0;
	constexpr double ProductRouteScreenshotTimeoutSeconds = 30.0;
	constexpr float RequiredMovementCentimeters = 100.0f;
	const FString CellPrefix(TEXT("ProjectWorld.Cell="));
	const FString GameplayObjectPrefix(TEXT("ProjectWorld.GameplayObject="));
	const FName GeneratedTag(TEXT("ProjectWorld.Generated.v1"));
	const FName LandscapeTag(TEXT("ProjectWorld.Landscape.v1"));
	const FName RoadTag(TEXT("ProjectWorld.Road.v1"));
	// The generic product route serves every /ProjectWorldData/Generated/ map, so it accepts both
	// massing generations. Realization, the partition audit, and territory acceptance do the same.
	const FName BuildingTagV1(TEXT("ProjectWorld.BuildingMassing.v1"));
	const FName BuildingTagV2(TEXT("ProjectWorld.BuildingMassing.v2"));

	bool ParseProductRouteValue(const TCHAR* Name, FString& OutValue, bool bShouldStopOnSeparator = true)
	{
		return FParse::Value(FCommandLine::Get(), Name, OutValue, bShouldStopOnSeparator) && !OutValue.IsEmpty();
	}

	bool IsProductRouteToken(const FString& Value)
	{
		if (Value.IsEmpty())
		{
			return false;
		}
		for (const TCHAR Character : Value)
		{
			if (!FChar::IsAlnum(Character) && Character != TEXT('_') && Character != TEXT('-'))
			{
				return false;
			}
		}
		return true;
	}

	bool IsProductRouteSha256(const FString& Value)
	{
		if (Value.Len() != 64)
		{
			return false;
		}
		for (const TCHAR Character : Value)
		{
			if (!FChar::IsHexDigit(Character))
			{
				return false;
			}
		}
		return true;
	}

	FString ProductRouteTagValue(const AActor& Actor, const FString& Prefix)
	{
		for (const FName& Tag : Actor.Tags)
		{
			const FString Value = Tag.ToString();
			if (Value.StartsWith(Prefix))
			{
				return Value.RightChop(Prefix.Len());
			}
		}
		return FString();
	}
}

FProjectWorldProductRouteGate::~FProjectWorldProductRouteGate()
{
	ReleaseInteractionSubscription();
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	}
}

void FProjectWorldProductRouteGate::StartIfRequested()
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("ProjectWorldProductRouteGate")))
	{
		return;
	}

	FString Error;
	if (!ParseConfig(Error))
	{
		FinishRejected(TEXT("product_route_config_invalid"), Error);
		return;
	}

	GateStartedSeconds = FPlatformTime::Seconds();
	SetPhase(EPhase::WaitingForWorld);
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &FProjectWorldProductRouteGate::Tick));
	UE_LOG(LogProjectWorldProductRouteGate, Display,
		TEXT("[FProjectWorldProductRouteGate::StartIfRequested] Started - operation=%s map=%s"),
		*Config.OperationId,
		*Config.MapPackage);
}

bool FProjectWorldProductRouteGate::ParseConfig(FString& OutError)
{
	Config.bRestorePreviewFlight = FParse::Param(
		FCommandLine::Get(),
		TEXT("ProjectWorldProductRouteRestorePreviewFlight"));
	Config.bRequireGameplayInteraction = !FParse::Param(
		FCommandLine::Get(),
		TEXT("ProjectWorldProductRouteSkipInteraction"));
	FString EdgeText;
	if (!ParseProductRouteValue(TEXT("ProjectWorldProductOperation="), Config.OperationId) ||
		!ParseProductRouteValue(TEXT("ProjectWorldProductResult="), Config.ResultPath) ||
		!ParseProductRouteValue(TEXT("ProjectWorldProductMap="), Config.MapPackage) ||
		!ParseProductRouteValue(TEXT("ProjectWorldProductRuntime="), Config.RuntimeProfileId) ||
		!ParseProductRouteValue(TEXT("ProjectWorldProductRuntimeHash="), Config.RuntimeProfileHash) ||
		!ParseProductRouteValue(TEXT("ProjectWorldProductMachine="), Config.MachineProfileId) ||
		!ParseProductRouteValue(TEXT("ProjectWorldProductEdge="), EdgeText, false))
	{
		OutError = TEXT("A required product-route argument is missing.");
		return false;
	}

	TArray<FString> Coordinates;
	EdgeText.ParseIntoArray(Coordinates, TEXT(","), true);
	if (Coordinates.Num() != 3 ||
		!LexTryParseString(Config.EdgeLocation.X, *Coordinates[0]) ||
		!LexTryParseString(Config.EdgeLocation.Y, *Coordinates[1]) ||
		!LexTryParseString(Config.EdgeLocation.Z, *Coordinates[2]))
	{
		OutError = TEXT("The product-route edge must be X,Y,Z numeric coordinates.");
		return false;
	}
	const bool bIdentityValid = IsProductRouteToken(Config.OperationId) &&
		IsProductRouteToken(Config.RuntimeProfileId) &&
		IsProductRouteToken(Config.MachineProfileId) && IsProductRouteSha256(Config.RuntimeProfileHash);
	const bool bEdgeValid = !Config.EdgeLocation.ContainsNaN() &&
		!FVector2D(Config.EdgeLocation.X, Config.EdgeLocation.Y).IsNearlyZero();
	if (!bIdentityValid || !bEdgeValid || FPaths::IsRelative(Config.ResultPath) ||
		!Config.MapPackage.StartsWith(TEXT("/ProjectWorldData/Generated/")))
	{
		OutError = TEXT("The product-route identity, result path, map, or edge is outside the supported contract.");
		return false;
	}
	FPaths::NormalizeFilename(Config.ResultPath);
	return true;
}

bool FProjectWorldProductRouteGate::Tick(float DeltaSeconds)
{
	if (Phase == EPhase::Finished)
	{
		return false;
	}
	++PhaseFrameCount;
	const double Now = FPlatformTime::Seconds();
	if (Now - GateStartedSeconds > ProductRouteGateTimeoutSeconds)
	{
		FinishRejected(TEXT("product_route_timeout"), TEXT("The complete product route exceeded its bounded timeout."));
		return false;
	}

	if (Phase == EPhase::WaitingForWorld)
	{
		if (!TryAcquireProductWorld() && Now - PhaseStartedSeconds > WorldTimeoutSeconds)
		{
			FinishRejected(TEXT("product_route_world_timeout"), LastReadinessError);
		}
		return Phase != EPhase::Finished;
	}

	if (!ProductWorld.IsValid() || !PlayerCharacter.IsValid() || !PlayerController.IsValid())
	{
		FinishRejected(TEXT("product_route_ownership_lost"), TEXT("The accepted product world, controller, or pawn became unavailable."));
		return false;
	}
	FString OwnershipError;
	if (!InspectRuntimeOwnership(OwnershipError))
	{
		FinishRejected(TEXT("product_route_runtime_ownership_invalid"), OwnershipError);
		return false;
	}

	switch (Phase)
	{
	case EPhase::SettlingAtCenter:
		TickCenterSettlement();
		break;
	case EPhase::MovingNormally:
		TickNormalMovement();
		break;
	case EPhase::ProbingCenter:
		ProbeCenterContracts();
		break;
	case EPhase::WaitingForInteraction:
		if (Progress.bGameplayInteraction)
		{
			BeginEdgeTraversal();
		}
		else if (Now - PhaseStartedSeconds > InteractionTimeoutSeconds)
		{
			FinishRejected(TEXT("product_route_interaction_timeout"),
				TEXT("The production interaction service did not broadcast a handled gameplay-object interaction."));
		}
		break;
	case EPhase::WaitingAtEdge:
		TickEdgeSettlement();
		break;
	case EPhase::WaitingAtCenter:
		TickCenterReturn();
		break;
	case EPhase::SettlingForScreenshot:
		if (PhaseFrameCount >= 30)
		{
			RequestScreenshot();
		}
		break;
	case EPhase::WaitingForScreenshot:
		if (IFileManager::Get().FileSize(*ScreenshotPath) > 0)
		{
			FString ScreenshotError;
			if (ProjectWorldScreenshotValidation::ValidateFile(ScreenshotPath, ScreenshotError))
			{
				FinishAccepted();
			}
			else
			{
				FinishRejected(TEXT("product_route_screenshot_invalid"), ScreenshotError);
			}
		}
		else if (Now - PhaseStartedSeconds > ProductRouteScreenshotTimeoutSeconds)
		{
			FinishRejected(TEXT("product_route_screenshot_timeout"), TEXT("The product-route screenshot was not written."));
		}
		break;
	default:
		break;
	}
	return Phase != EPhase::Finished;
}

bool FProjectWorldProductRouteGate::TryAcquireProductWorld()
{
	if (GEngine == nullptr || GEngine->GameViewport == nullptr)
	{
		LastReadinessError = TEXT("The normal game viewport is not ready.");
		return false;
	}
	UWorld* World = GEngine->GameViewport->GetWorld();
	if (World == nullptr || !World->IsGameWorld())
	{
		LastReadinessError = TEXT("The normal game world is not ready.");
		return false;
	}
	if (World->GetPackage()->GetName() != Config.MapPackage)
	{
		LastReadinessError = FString::Printf(
			TEXT("Waiting for the menu/loading route to enter '%s'; current world is '%s'."),
			*Config.MapPackage,
			*World->GetPackage()->GetName());
		return false;
	}
	Progress.bMapIdentity = true;
	if (FCString::Strcmp(World->URL.GetOption(TEXT("ProjectLoadingRoute="), TEXT("")), TEXT("1")) != 0)
	{
		FinishRejected(TEXT("product_route_loading_provenance_missing"),
			TEXT("The generated world was not entered through the authoritative ProjectLoading travel executor."));
		return false;
	}
	AGameModeBase* GameMode = World->GetAuthGameMode();
	if (GameMode == nullptr ||
		GameMode->GetClass()->GetPathName() != TEXT("/Script/ProjectSinglePlay.SinglePlayerGameMode"))
	{
		FinishRejected(TEXT("product_route_game_mode_invalid"), TEXT("The generated world did not start with SinglePlayerGameMode."));
		return false;
	}
	Progress.bGameModeIdentity = true;
	APlayerController* Controller = World->GetFirstPlayerController();
	ACharacter* Character = Controller == nullptr ? nullptr : Cast<ACharacter>(Controller->GetPawn());
	UCharacterMovementComponent* Movement = Character == nullptr ? nullptr : Character->GetCharacterMovement();
	if (Controller == nullptr || Character == nullptr || Movement == nullptr || Character->GetController() != Controller)
	{
		LastReadinessError = TEXT("Waiting for the normal possessed character.");
		return false;
	}
	Progress.bPossessedPlayer = true;
	if (Config.bRestorePreviewFlight)
	{
		if (World->URL.GetOption(TEXT("Traversal="), TEXT("")) != FString(TEXT("PreviewFlight")))
		{
			FinishRejected(TEXT("product_route_preview_flight_missing"),
				TEXT("The product route did not receive the selected PreviewFlight experience option."));
			return false;
		}
		Movement->Velocity = FVector::ZeroVector;
		Movement->SetMovementMode(MOVE_Walking);
	}
	ProductWorld = World;
	PlayerController = Controller;
	PlayerCharacter = Character;
	CharacterMovement = Movement;
	CenterLocation = Character->GetActorLocation();
	SetPhase(EPhase::SettlingAtCenter);
	return true;
}

bool FProjectWorldProductRouteGate::TickCenterSettlement()
{
	UCharacterMovementComponent* Movement = CharacterMovement.Get();
	const bool bStreamingComplete = IsStreamingCompleted();
	if (Movement != nullptr && bStreamingComplete && PhaseFrameCount >= 3)
	{
		if (Movement->IsMovingOnGround())
		{
			Progress.bGroundedPlayer = true;
			MovementStart = PlayerCharacter->GetActorLocation();
			SetPhase(EPhase::MovingNormally);
			return true;
		}
		if (Config.bRestorePreviewFlight && !bCenterGroundPlacementRequested &&
			MovePlayerTo(CenterLocation, true))
		{
			bCenterGroundPlacementRequested = true;
			Movement->SetMovementMode(MOVE_Walking);
			return false;
		}
	}
	if (FPlatformTime::Seconds() - PhaseStartedSeconds > StreamingTimeoutSeconds)
	{
		FinishRejected(TEXT("product_route_center_not_ready"),
			TEXT("The possessed player never became grounded in a streaming-complete center state."));
	}
	return false;
}

bool FProjectWorldProductRouteGate::TickNormalMovement()
{
	ACharacter* Character = PlayerCharacter.Get();
	UCharacterMovementComponent* Movement = CharacterMovement.Get();
	if (Character == nullptr || Movement == nullptr)
	{
		return false;
	}
	Character->AddMovementInput(Character->GetActorForwardVector(), 1.0f, false);
	MovementDistanceCentimeters = FVector::Dist2D(MovementStart, Character->GetActorLocation());
	if (MovementDistanceCentimeters >= RequiredMovementCentimeters && Movement->IsMovingOnGround())
	{
		Progress.bNormalMovement = true;
		SetPhase(EPhase::ProbingCenter);
		return true;
	}
	if (FPlatformTime::Seconds() - PhaseStartedSeconds > MovementTimeoutSeconds)
	{
		FinishRejected(TEXT("product_route_normal_movement_failed"),
			FString::Printf(TEXT("Normal movement reached only %.1f cm while grounded."), MovementDistanceCentimeters));
	}
	return false;
}

bool FProjectWorldProductRouteGate::ProbeCenterContracts()
{
	Progress.bTerrainCollision = ProbeTerrainCollision(
		TerrainCollisionActor, TerrainCandidateCount, TerrainBlockingPrimitiveCount);
	Progress.bRoadCollision = ProbeTaggedCollision(
		{RoadTag}, RoadCollisionActor, RoadCandidateCount, RoadBlockingPrimitiveCount);
	Progress.bBuildingCollision = ProbeTaggedCollision(
		{BuildingTagV1, BuildingTagV2},
		BuildingCollisionActor,
		BuildingCandidateCount,
		BuildingBlockingPrimitiveCount);
	if (!Progress.bTerrainCollision || !Progress.bRoadCollision || !Progress.bBuildingCollision)
	{
		LastReadinessError = FString::Printf(
			TEXT("Waiting for actor-specific collision: terrain=%d (%d actors/%d blocking primitives), "
				"road=%d (%d/%d), building=%d (%d/%d), player=%s."),
			Progress.bTerrainCollision ? 1 : 0,
			TerrainCandidateCount,
			TerrainBlockingPrimitiveCount,
			Progress.bRoadCollision ? 1 : 0,
			RoadCandidateCount,
			RoadBlockingPrimitiveCount,
			Progress.bBuildingCollision ? 1 : 0,
			BuildingCandidateCount,
			BuildingBlockingPrimitiveCount,
			*PlayerCharacter->GetActorLocation().ToCompactString());
		if (FPlatformTime::Seconds() - PhaseStartedSeconds > StreamingTimeoutSeconds)
		{
			FinishRejected(TEXT("product_route_collision_failed"), LastReadinessError);
		}
		return false;
	}
	CenterCellMarker = FindNearestCellMarker(CenterLocation);
	if (CenterCellMarker.IsEmpty())
	{
		LastReadinessError = TEXT("Waiting for a spatial generated cell marker near the center.");
		if (FPlatformTime::Seconds() - PhaseStartedSeconds > StreamingTimeoutSeconds)
		{
			FinishRejected(TEXT("product_route_center_marker_missing"), LastReadinessError);
		}
		return false;
	}
	if (!Config.bRequireGameplayInteraction)
	{
		BeginEdgeTraversal();
		return true;
	}
	if (!BeginGameplayInteraction() && FPlatformTime::Seconds() - PhaseStartedSeconds > StreamingTimeoutSeconds)
	{
		FinishRejected(TEXT("product_route_interaction_contract_missing"), LastReadinessError);
	}
	return Phase == EPhase::WaitingForInteraction;
}

bool FProjectWorldProductRouteGate::BeginGameplayInteraction()
{
	AActor* Target = FindNearestGameplayObject();
	UActorComponent* InteractionComponent = FindInteractionComponent();
	InteractionService = FProjectServiceLocator::Resolve<IInteractionService>();
	if (Target == nullptr || InteractionComponent == nullptr || !InteractionService.IsValid())
	{
		LastReadinessError = FString::Printf(
			TEXT("Waiting for interaction contract: target=%d component=%d service=%d."),
			Target != nullptr ? 1 : 0,
			InteractionComponent != nullptr ? 1 : 0,
			InteractionService.IsValid() ? 1 : 0);
		InteractionService.Reset();
		return false;
	}
	InteractionObjectId = ProductRouteTagValue(*Target, GameplayObjectPrefix);
	FVector Direction = PlayerCharacter->GetActorLocation() - Target->GetActorLocation();
	Direction.Z = 0.0f;
	if (!Direction.Normalize())
	{
		Direction = FVector::ForwardVector;
	}
	if (!MovePlayerTo(Target->GetActorLocation() + Direction * 100.0f, true))
	{
		LastReadinessError = TEXT("Waiting for grounded placement near the generated gameplay object.");
		InteractionService.Reset();
		return false;
	}
	InteractionHandle = InteractionService->OnInteraction().AddRaw(
		this, &FProjectWorldProductRouteGate::HandleInteraction);
	CharacterMovement->SetMovementMode(MOVE_Walking);
	SetPhase(EPhase::WaitingForInteraction);
	bInteractionDispatchAccepted = IInteractionComponentInterface::Execute_TryInteractWithActor(
		InteractionComponent, Target);
	if (!bInteractionDispatchAccepted)
	{
		FinishRejected(TEXT("product_route_interaction_dispatch_rejected"),
			TEXT("The production pawn interaction component rejected the generated gameplay object."));
		return false;
	}
	return true;
}

void FProjectWorldProductRouteGate::HandleInteraction(AActor* Target, AActor* Instigator)
{
	if (Target != nullptr && Instigator == PlayerCharacter.Get() &&
		ProductRouteTagValue(*Target, GameplayObjectPrefix) == InteractionObjectId)
	{
		Progress.bGameplayInteraction = true;
	}
}

void FProjectWorldProductRouteGate::BeginEdgeTraversal()
{
	ReleaseInteractionSubscription();
	MovePlayerTo(Config.EdgeLocation, false);
	SetPhase(EPhase::WaitingAtEdge);
}

bool FProjectWorldProductRouteGate::TickEdgeSettlement()
{
	if (PhaseFrameCount >= 3 && IsStreamingCompleted())
	{
		EdgeCellMarker = FindNearestCellMarker(Config.EdgeLocation, CenterCellMarker);
		const bool bCenterAbsent = !HasCellMarker(CenterCellMarker);
		if (!EdgeCellMarker.IsEmpty() && bCenterAbsent && MovePlayerTo(Config.EdgeLocation, true))
		{
			Progress.bCenterUnloadedAtEdge = true;
			Progress.bEdgeLoaded = true;
			BeginCenterReturn();
			return true;
		}
	}
	if (FPlatformTime::Seconds() - PhaseStartedSeconds > StreamingTimeoutSeconds)
	{
		FinishRejected(TEXT("product_route_edge_streaming_failed"),
			TEXT("The edge did not load while the center cell marker unloaded."));
	}
	return false;
}

void FProjectWorldProductRouteGate::BeginCenterReturn()
{
	MovePlayerTo(CenterLocation + FVector(0.0f, 0.0f, 10000.0f), false);
	SetPhase(EPhase::WaitingAtCenter);
}

bool FProjectWorldProductRouteGate::TickCenterReturn()
{
	if (PhaseFrameCount >= 3 && IsStreamingCompleted() && HasCellMarker(CenterCellMarker) &&
		MovePlayerTo(CenterLocation, true))
	{
		Progress.bCenterReloaded = true;
		CharacterMovement->SetMovementMode(
			Config.bRestorePreviewFlight ? MOVE_Flying : MOVE_Walking);
		bPreviewFlightRestored = !Config.bRestorePreviewFlight || CharacterMovement->IsFlying();
		SetPhase(EPhase::SettlingForScreenshot);
		return true;
	}
	if (FPlatformTime::Seconds() - PhaseStartedSeconds > StreamingTimeoutSeconds)
	{
		FinishRejected(TEXT("product_route_center_reload_failed"),
			TEXT("The original center cell marker did not return after center traversal."));
	}
	return false;
}

bool FProjectWorldProductRouteGate::IsStreamingCompleted() const
{
	UWorld* World = ProductWorld.Get();
	UWorldPartition* Partition = World == nullptr ? nullptr : World->GetWorldPartition();
	if (Partition == nullptr)
	{
		return false;
	}
	const TArray<FWorldPartitionStreamingSource>& Sources = Partition->GetStreamingSources();
	return !Sources.IsEmpty() && Partition->IsStreamingCompleted(&Sources);
}

bool FProjectWorldProductRouteGate::InspectRuntimeOwnership(FString& OutError)
{
	UWorld* World = ProductWorld.Get();
	if (World == nullptr)
	{
		OutError = TEXT("The product world is unavailable.");
		return false;
	}
	const ProjectWorldPresentation::FRuntimeRoleScan Scan = ProjectWorldPresentation::ScanRuntimeRoles(
		*World, Config.RuntimeProfileId, Config.RuntimeProfileHash);
	if (!Scan.bValid)
	{
		OutError = Scan.Error;
		return false;
	}
	ObservedRuntimeRoles.Append(Scan.LoadedRoles);
	return true;
}

bool FProjectWorldProductRouteGate::ProbeTaggedCollision(
	TConstArrayView<FName> AcceptedTags,
	FString& OutActorName,
	int32& OutCandidateCount,
	int32& OutBlockingPrimitiveCount) const
{
	OutCandidateCount = 0;
	OutBlockingPrimitiveCount = 0;
	UWorld* World = ProductWorld.Get();
	if (World == nullptr)
	{
		return false;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const bool bTagged = AcceptedTags.ContainsByPredicate(
			[&It](const FName& Tag) { return It->Tags.Contains(Tag); });
		if (!bTagged)
		{
			continue;
		}
		++OutCandidateCount;
		OutBlockingPrimitiveCount += ProjectWorldProductRouteCollision::CountBlockingPrimitives(**It);
		if (ProbeActorCollision(**It))
		{
			OutActorName = It->GetPathName();
			return true;
		}
	}
	return false;
}

bool FProjectWorldProductRouteGate::ProbeTerrainCollision(
	FString& OutActorName,
	int32& OutCandidateCount,
	int32& OutBlockingPrimitiveCount) const
{
	OutCandidateCount = 0;
	OutBlockingPrimitiveCount = 0;
	UWorld* World = ProductWorld.Get();
	ACharacter* Character = PlayerCharacter.Get();
	if (World == nullptr || Character == nullptr)
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ProjectWorldProductRouteTerrainCollision), true);
	Params.AddIgnoredActor(Character);
	const FVector PlayerLocation = Character->GetActorLocation();
	const FVector Start(PlayerLocation.X, PlayerLocation.Y, PlayerLocation.Z + 10000.0f);
	const FVector End(PlayerLocation.X, PlayerLocation.Y, PlayerLocation.Z - 100000.0f);
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (!It->Tags.Contains(LandscapeTag))
		{
			continue;
		}
		++OutCandidateCount;
		OutBlockingPrimitiveCount += ProjectWorldProductRouteCollision::CountBlockingPrimitives(**It);
		FHitResult Hit;
		if ((It->ActorLineTraceSingle(Hit, Start, End, ECC_Pawn, Params) && Hit.bBlockingHit) ||
			ProbeActorCollision(**It))
		{
			OutActorName = It->GetPathName();
			return true;
		}
	}
	return false;
}

bool FProjectWorldProductRouteGate::ProbeActorCollision(const AActor& Actor) const
{
	FVector Origin;
	FVector Extent;
	Actor.GetActorBounds(false, Origin, Extent, true);
	if (Origin.ContainsNaN() || Extent.ContainsNaN() || Extent.IsNearlyZero())
	{
		return false;
	}
	TInlineComponentArray<UPrimitiveComponent*> Components;
	Actor.GetComponents(Components);
	for (UPrimitiveComponent* Primitive : Components)
	{
		if (Primitive == nullptr || !Primitive->IsRegistered() || !Primitive->IsCollisionEnabled() ||
			Primitive->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block)
		{
			continue;
		}
		const FBoxSphereBounds Bounds = Primitive->Bounds;
		const float Radius = FMath::Max(100.0, Bounds.SphereRadius + 100.0);
		const FCollisionShape ProbeSphere = FCollisionShape::MakeSphere(Radius);
		if (Primitive->OverlapComponent(Bounds.Origin, FQuat::Identity, ProbeSphere))
		{
			return true;
		}
		const FVector Start = Bounds.Origin + FVector(0.0f, 0.0f, Bounds.BoxExtent.Z + Radius + 100.0f);
		const FVector End = Bounds.Origin - FVector(0.0f, 0.0f, Bounds.BoxExtent.Z + Radius + 100.0f);
		FHitResult Hit;
		if (Primitive->SweepComponent(
			Hit, Start, End, FQuat::Identity, ProbeSphere, false) && Hit.bBlockingHit)
		{
			return true;
		}
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ProjectWorldProductRouteCollision), true);
	Params.AddIgnoredActor(PlayerCharacter.Get());
	const FVector QueryOffsets[] =
	{
		FVector(0.0f, 0.0f, Extent.Z + 5000.0f),
		FVector(Extent.X + 5000.0f, 0.0f, 0.0f),
		FVector(-(Extent.X + 5000.0f), 0.0f, 0.0f),
		FVector(0.0f, Extent.Y + 5000.0f, 0.0f),
		FVector(0.0f, -(Extent.Y + 5000.0f), 0.0f)
	};
	for (const FVector& Offset : QueryOffsets)
	{
		FVector ClosestPoint;
		UPrimitiveComponent* Primitive = nullptr;
		const float Distance = Actor.ActorGetDistanceToCollision(
			Origin + Offset, ECC_Pawn, ClosestPoint, &Primitive);
		if (Distance >= 0.0f && Primitive != nullptr)
		{
			return true;
		}
	}
	constexpr int32 SamplesPerAxis = 25;
	for (int32 XIndex = 0; XIndex < SamplesPerAxis; ++XIndex)
	{
		for (int32 YIndex = 0; YIndex < SamplesPerAxis; ++YIndex)
		{
			const float XAlpha = SamplesPerAxis == 1 ? 0.0f : -1.0f + 2.0f * XIndex / (SamplesPerAxis - 1);
			const float YAlpha = SamplesPerAxis == 1 ? 0.0f : -1.0f + 2.0f * YIndex / (SamplesPerAxis - 1);
			const FVector Start(Origin.X + Extent.X * XAlpha, Origin.Y + Extent.Y * YAlpha, Origin.Z + Extent.Z + 5000.0f);
			const FVector End(Start.X, Start.Y, Origin.Z - Extent.Z - 5000.0f);
			FHitResult Hit;
			if (Actor.ActorLineTraceSingle(Hit, Start, End, ECC_Pawn, Params) && Hit.bBlockingHit)
			{
				return true;
			}
		}
	}
	return false;
}

AActor* FProjectWorldProductRouteGate::FindNearestGameplayObject() const
{
	UWorld* World = ProductWorld.Get();
	ACharacter* Character = PlayerCharacter.Get();
	AActor* Nearest = nullptr;
	double NearestDistanceSquared = TNumericLimits<double>::Max();
	if (World == nullptr || Character == nullptr)
	{
		return nullptr;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (ProductRouteTagValue(**It, GameplayObjectPrefix).IsEmpty())
		{
			continue;
		}
		const double DistanceSquared = FVector::DistSquared(Character->GetActorLocation(), It->GetActorLocation());
		if (DistanceSquared < NearestDistanceSquared)
		{
			Nearest = *It;
			NearestDistanceSquared = DistanceSquared;
		}
	}
	return Nearest;
}

UActorComponent* FProjectWorldProductRouteGate::FindInteractionComponent() const
{
	ACharacter* Character = PlayerCharacter.Get();
	if (Character == nullptr)
	{
		return nullptr;
	}
	TArray<UActorComponent*> Components;
	Character->GetComponents(Components);
	for (UActorComponent* Component : Components)
	{
		if (Component != nullptr && Component->GetClass()->ImplementsInterface(UInteractionComponentInterface::StaticClass()))
		{
			return Component;
		}
	}
	return nullptr;
}

FString FProjectWorldProductRouteGate::FindNearestCellMarker(
	const FVector& Location,
	const FString& ExcludedMarker) const
{
	UWorld* World = ProductWorld.Get();
	FString NearestMarker;
	double NearestDistanceSquared = TNumericLimits<double>::Max();
	if (World == nullptr)
	{
		return NearestMarker;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const FString Marker = ProductRouteTagValue(**It, CellPrefix);
		if (Marker.IsEmpty() || Marker == ExcludedMarker || !It->Tags.Contains(GeneratedTag))
		{
			continue;
		}
		FVector Origin;
		FVector Extent;
		It->GetActorBounds(false, Origin, Extent, true);
		const double DistanceSquared = FVector::DistSquared2D(Location, Origin);
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestMarker = Marker;
			NearestDistanceSquared = DistanceSquared;
		}
	}
	return NearestDistanceSquared <= FMath::Square(200000.0) ? NearestMarker : FString();
}

bool FProjectWorldProductRouteGate::HasCellMarker(const FString& Marker) const
{
	UWorld* World = ProductWorld.Get();
	if (World == nullptr || Marker.IsEmpty())
	{
		return false;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (ProductRouteTagValue(**It, CellPrefix) == Marker)
		{
			return true;
		}
	}
	return false;
}

bool FProjectWorldProductRouteGate::MovePlayerTo(const FVector& Location, bool bPlaceOnGround)
{
	ACharacter* Character = PlayerCharacter.Get();
	UCharacterMovementComponent* Movement = CharacterMovement.Get();
	if (Character == nullptr || Movement == nullptr)
	{
		return false;
	}
	FVector Destination = Location;
	if (bPlaceOnGround && !FindGroundLocation(Location, Destination))
	{
		return false;
	}
	Movement->SetMovementMode(MOVE_Flying);
	Movement->Velocity = FVector::ZeroVector;
	return Character->SetActorLocation(Destination, false, nullptr, ETeleportType::TeleportPhysics);
}

bool FProjectWorldProductRouteGate::FindGroundLocation(
	const FVector& Location,
	FVector& OutGroundLocation) const
{
	UWorld* World = ProductWorld.Get();
	ACharacter* Character = PlayerCharacter.Get();
	if (World == nullptr || Character == nullptr)
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ProjectWorldProductRouteGround), false);
	Params.AddIgnoredActor(Character);
	const FVector Start(Location.X, Location.Y, Location.Z + 100000.0f);
	const FVector End(Location.X, Location.Y, Location.Z - 100000.0f);
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Pawn, Params) && Hit.bBlockingHit)
	{
		const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		OutGroundLocation = Hit.ImpactPoint + FVector(0.0f, 0.0f, HalfHeight + 10.0f);
		return true;
	}
	return false;
}

void FProjectWorldProductRouteGate::RequestScreenshot()
{
	ScreenshotPath = FPaths::Combine(FPaths::GetPath(Config.ResultPath), TEXT("product-route.png"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ScreenshotPath), true);
	IFileManager::Get().Delete(*ScreenshotPath, false, true, true);
	FString CaptureError;
	if (!ProjectWorldRuntimeScreenshotCapture::CapturePlayerContext(
		*ProductWorld.Get(), *PlayerController.Get(), ScreenshotPath, CaptureError))
	{
		FinishRejected(TEXT("product_route_screenshot_invalid"), CaptureError);
		return;
	}
	SetPhase(EPhase::WaitingForScreenshot);
}

void FProjectWorldProductRouteGate::FinishAccepted()
{
	if (!Progress.IsAccepted(Config.bRequireGameplayInteraction))
	{
		FinishRejected(TEXT("product_route_evidence_incomplete"),
			FString::Printf(TEXT("The first missing product gate is '%s'."),
				*Progress.FirstMissingGate(Config.bRequireGameplayInteraction)));
		return;
	}
	WriteResult(TEXT("accepted"), FString(), FString());
	SetPhase(EPhase::Finished);
	if (FParse::Param(FCommandLine::Get(), TEXT("ProjectWorldProductPerformanceGate")))
	{
		return;
	}
	FPlatformMisc::RequestExitWithStatus(false, 0, TEXT("ProjectWorldProductRouteGate.Accepted"));
}

void FProjectWorldProductRouteGate::FinishRejected(const FString& Code, const FString& Message)
{
	WriteResult(TEXT("rejected"), Code, Message);
	SetPhase(EPhase::Finished);
	UE_LOG(LogProjectWorldProductRouteGate, Error,
		TEXT("[FProjectWorldProductRouteGate::FinishRejected] Rejected - code=%s message=%s"),
		*Code,
		*Message);
	FPlatformMisc::RequestExitWithStatus(false, 9, TEXT("ProjectWorldProductRouteGate.Rejected"));
}

void FProjectWorldProductRouteGate::WriteResult(
	const FString& Status,
	const FString& ErrorCode,
	const FString& ErrorMessage)
{
	if (Config.ResultPath.IsEmpty())
	{
		return;
	}
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("$schema"), TEXT("https://alis.world/schemas/world-product-route/product-route-result-v1.json"));
	Root->SetNumberField(TEXT("schema_version"), 1);
	Root->SetStringField(TEXT("operation_id"), Config.OperationId);
	Root->SetStringField(TEXT("status"), Status);
	Root->SetStringField(TEXT("map_package"), Config.MapPackage);
	Root->SetStringField(TEXT("runtime_profile"), Config.RuntimeProfileId);
	Root->SetStringField(TEXT("runtime_profile_sha256"), Config.RuntimeProfileHash);
	Root->SetStringField(TEXT("machine_profile_id"), Config.MachineProfileId);
	Root->SetStringField(TEXT("executable"), FPlatformProcess::ExecutablePath());
	Root->SetStringField(TEXT("build_configuration"), LexToString(FApp::GetBuildConfiguration()));
	Root->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString());
	Root->SetStringField(TEXT("gpu_adapter"), GRHIAdapterName);
	const FGPUDriverInfo Driver = FPlatformMisc::GetGPUDriverInfo(GRHIAdapterName);
	Root->SetStringField(TEXT("gpu_driver"), Driver.UserDriverVersion);
	Root->SetStringField(TEXT("rhi"), GDynamicRHI == nullptr ? TEXT("unavailable") : GDynamicRHI->GetName());
	Root->SetStringField(TEXT("game_mode"), TEXT("/Script/ProjectSinglePlay.SinglePlayerGameMode"));
	Root->SetStringField(TEXT("pawn_class"), PlayerCharacter.IsValid() ? PlayerCharacter->GetClass()->GetPathName() : FString());
	Root->SetBoolField(TEXT("project_loading_provenance"), Progress.bMapIdentity && Progress.bGameModeIdentity);
	Root->SetBoolField(TEXT("possessed_player"), Progress.bPossessedPlayer);
	Root->SetBoolField(TEXT("grounded_player"), Progress.bGroundedPlayer);
	Root->SetBoolField(TEXT("normal_movement"), Progress.bNormalMovement);
	Root->SetNumberField(TEXT("movement_distance_cm"), MovementDistanceCentimeters);
	Root->SetBoolField(TEXT("terrain_collision"), Progress.bTerrainCollision);
	Root->SetStringField(TEXT("terrain_collision_actor"), TerrainCollisionActor);
	Root->SetNumberField(TEXT("terrain_candidate_actors"), TerrainCandidateCount);
	Root->SetNumberField(TEXT("terrain_blocking_primitives"), TerrainBlockingPrimitiveCount);
	Root->SetBoolField(TEXT("road_collision"), Progress.bRoadCollision);
	Root->SetStringField(TEXT("road_collision_actor"), RoadCollisionActor);
	Root->SetNumberField(TEXT("road_candidate_actors"), RoadCandidateCount);
	Root->SetNumberField(TEXT("road_blocking_primitives"), RoadBlockingPrimitiveCount);
	Root->SetBoolField(TEXT("building_collision"), Progress.bBuildingCollision);
	Root->SetStringField(TEXT("building_collision_actor"), BuildingCollisionActor);
	Root->SetNumberField(TEXT("building_candidate_actors"), BuildingCandidateCount);
	Root->SetNumberField(TEXT("building_blocking_primitives"), BuildingBlockingPrimitiveCount);
	Root->SetBoolField(TEXT("gameplay_interaction"), Progress.bGameplayInteraction);
	Root->SetBoolField(TEXT("gameplay_interaction_required"), Config.bRequireGameplayInteraction);
	Root->SetStringField(TEXT("gameplay_object_id"), InteractionObjectId);
	Root->SetBoolField(TEXT("interaction_dispatch_accepted"), bInteractionDispatchAccepted);
	Root->SetStringField(TEXT("center_cell_marker"), CenterCellMarker);
	Root->SetStringField(TEXT("edge_cell_marker"), EdgeCellMarker);
	Root->SetBoolField(TEXT("center_unloaded_at_edge"), Progress.bCenterUnloadedAtEdge);
	Root->SetBoolField(TEXT("edge_loaded"), Progress.bEdgeLoaded);
	Root->SetBoolField(TEXT("center_reloaded"), Progress.bCenterReloaded);
	Root->SetBoolField(TEXT("preview_flight_restored"), bPreviewFlightRestored);
	Root->SetStringField(TEXT("screenshot"), ScreenshotPath);
	TArray<TSharedPtr<FJsonValue>> Roles;
	for (const FString& Role : ObservedRuntimeRoles)
	{
		Roles.Add(MakeShared<FJsonValueString>(Role));
	}
	Root->SetArrayField(TEXT("observed_runtime_roles"), Roles);
	TArray<TSharedPtr<FJsonValue>> Errors;
	if (!ErrorCode.IsEmpty())
	{
		TSharedRef<FJsonObject> Error = MakeShared<FJsonObject>();
		Error->SetStringField(TEXT("code"), ErrorCode);
		Error->SetStringField(TEXT("message"), ErrorMessage);
		Errors.Add(MakeShared<FJsonValueObject>(Error));
	}
	Root->SetArrayField(TEXT("errors"), Errors);

	FString Payload;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		return;
	}
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Config.ResultPath), true);
	const FString Staging = Config.ResultPath + TEXT(".tmp");
	if (FFileHelper::SaveStringToFile(Payload + TEXT("\n"), *Staging, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		IFileManager::Get().Move(*Config.ResultPath, *Staging, true, true);
	}
}

void FProjectWorldProductRouteGate::SetPhase(EPhase NewPhase)
{
	Phase = NewPhase;
	PhaseStartedSeconds = FPlatformTime::Seconds();
	PhaseFrameCount = 0;
}

void FProjectWorldProductRouteGate::ReleaseInteractionSubscription()
{
	if (InteractionService.IsValid() && InteractionHandle.IsValid())
	{
		InteractionService->OnInteraction().Remove(InteractionHandle);
	}
	InteractionHandle.Reset();
	InteractionService.Reset();
}
