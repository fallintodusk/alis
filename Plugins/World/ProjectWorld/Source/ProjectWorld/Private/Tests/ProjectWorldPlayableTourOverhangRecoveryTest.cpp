// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Presentation/ProjectWorldPlayableTourDriver.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldPlayableTourOverhangRecoveryTest,
	"Project.World.PlayableTour.OverhangRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectWorldPlayableTourOverhangRecoveryTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Initialization = UWorld::InitializationValues().AllowAudioPlayback(false)
		.CreatePhysicsScene(true).RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
		ERHIFeatureLevel::Num, &Initialization);
	if (World == nullptr)
	{
		AddError(TEXT("Unable to create the overhang physics fixture."));
		return false;
	}
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	ACharacter* Character = World->SpawnActor<ACharacter>();
	AActor* Roof = World->SpawnActor<AActor>();
	if (Controller == nullptr || Character == nullptr || Roof == nullptr)
	{
		AddError(TEXT("Unable to spawn the overhang fixture actors."));
		World->DestroyWorld(false);
		return false;
	}
	UBoxComponent* Box = NewObject<UBoxComponent>(Roof);
	Roof->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(1000, 1000, 10));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Box->SetCollisionResponseToAllChannels(ECR_Block);
	Box->RegisterComponent();
	Roof->SetActorLocation(FVector(0, 0, 120));
	Controller->Possess(Character);
	Character->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	{
		FProjectWorldPlayableTourDriver Driver;
		FString Error;
		if (!Driver.Initialize(*Controller, *Character,
			{FVector::ZeroVector, FVector(100000, 0, 0), FVector::ZeroVector}, Error))
		{
			AddError(FString::Printf(TEXT("The real-input driver could not initialize: %s"), *Error));
			World->DestroyWorld(false);
			return false;
		}
		Driver.Phase = FProjectWorldPlayableTourDriver::EPhase::Traversing;
		Driver.WaypointIndex = 1;
		Driver.bClearingObstacle = true;
		Driver.ObstacleClearanceStartedZ = 0;
		Driver.CurrentLegTimeoutSeconds = 60;
		Driver.PhaseStartedSeconds = FPlatformTime::Seconds();
		Driver.NextDiagnosticSeconds = FPlatformTime::Seconds() + 5;
		Driver.PriorTargetDistanceCentimeters = 100000;
		FHitResult Hit;
		UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
		TestTrue(TEXT("The fixture actually blocks the production capsule upward."),
			World->SweepSingleByChannel(Hit, FVector::ZeroVector, FVector(0, 0, 200),
				Capsule->GetComponentQuat(), Capsule->GetCollisionObjectType(),
				FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()),
				FCollisionQueryParams(SCENE_QUERY_STAT(ProjectWorldOverhangFixture), false, Character),
				FCollisionResponseParams(Capsule->GetCollisionResponseToChannels())));
		TestTrue(TEXT("The blocker is an underside."), Hit.ImpactNormal.Z < -0.5);
		Driver.TickTraversing(0.1f, Error);
		TestTrue(TEXT("A real blocked underside selects backward simulated input."), Driver.HeldKeys.Contains(EKeys::S));
		TestFalse(TEXT("Recovery releases forward input instead of pressing into the corner."), Driver.HeldKeys.Contains(EKeys::W));
		TestTrue(TEXT("Recovery retains upward input."), Driver.HeldKeys.Contains(EKeys::SpaceBar));
		AActor* RearWall = World->SpawnActor<AActor>();
		UBoxComponent* WallBox = NewObject<UBoxComponent>(RearWall);
		RearWall->SetRootComponent(WallBox);
		WallBox->SetBoxExtent(FVector(10, 1000, 1000));
		WallBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		WallBox->SetCollisionResponseToAllChannels(ECR_Block);
		WallBox->RegisterComponent();
		RearWall->SetActorLocation(FVector(-100, 0, 0));
		TestTrue(TEXT("The rear wall blocks the production capsule retreat."),
			World->SweepSingleByChannel(Hit, FVector::ZeroVector, FVector(-200, 0, 0),
				Capsule->GetComponentQuat(), Capsule->GetCollisionObjectType(),
				FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()),
				FCollisionQueryParams(SCENE_QUERY_STAT(ProjectWorldRearWallFixture), false, Character),
				FCollisionResponseParams(Capsule->GetCollisionResponseToChannels())));
		TestFalse(TEXT("The side remains open for the production capsule."),
			World->SweepSingleByChannel(Hit, FVector::ZeroVector, FVector(0, 200, 0),
				Capsule->GetComponentQuat(), Capsule->GetCollisionObjectType(),
				FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()),
				FCollisionQueryParams(SCENE_QUERY_STAT(ProjectWorldOpenSideFixture), false, Character),
				FCollisionResponseParams(Capsule->GetCollisionResponseToChannels())));
		Driver.TickTraversing(0.1f, Error);
		TestFalse(TEXT("Blocked retreat releases backward input."), Driver.HeldKeys.Contains(EKeys::S));
		TestTrue(TEXT("Blocked retreat selects exactly one open lateral input."),
			Driver.HeldKeys.Contains(EKeys::A) != Driver.HeldKeys.Contains(EKeys::D));
		TestTrue(TEXT("Lateral recovery retains upward input."), Driver.HeldKeys.Contains(EKeys::SpaceBar));
		WallBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		const bool bWasMovingRight = Driver.HeldKeys.Contains(EKeys::D);
		Driver.TickTraversing(0.1f, Error);
		TestTrue(TEXT("An open lateral escape remains selected when backward opens again."),
			Driver.HeldKeys.Contains(bWasMovingRight ? EKeys::D : EKeys::A));
		Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Character->SetActorLocation(FVector(0, 0, 1000));
		Driver.TickTraversing(0.1f, Error);
		TestFalse(TEXT("Clearance releases backward input."), Driver.HeldKeys.Contains(EKeys::S));
		TestFalse(TEXT("Clearance releases left input."), Driver.HeldKeys.Contains(EKeys::A));
		TestFalse(TEXT("Clearance releases right input."), Driver.HeldKeys.Contains(EKeys::D));
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Character->SetActorLocation(FVector::ZeroVector);
		WallBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		for (double Side : { -1.0, 1.0 })
		{
			AActor* SideWall = World->SpawnActor<AActor>();
			UBoxComponent* SideBox = NewObject<UBoxComponent>(SideWall);
			SideWall->SetRootComponent(SideBox);
			SideBox->SetBoxExtent(FVector(1000, 10, 1000));
			SideBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			SideBox->SetCollisionResponseToAllChannels(ECR_Block);
			SideBox->RegisterComponent();
			SideWall->SetActorLocation(FVector(0, Side * 100, 0));
		}
		TestFalse(TEXT("The three-wall fixture has an open forward exit for the production capsule."),
			World->SweepSingleByChannel(Hit, FVector::ZeroVector, FVector(200, 0, 0),
				Capsule->GetComponentQuat(), Capsule->GetCollisionObjectType(),
				FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()),
				FCollisionQueryParams(SCENE_QUERY_STAT(ProjectWorldForwardExitFixture), false, Character),
				FCollisionResponseParams(Capsule->GetCollisionResponseToChannels())));
		Driver.TickTraversing(0.1f, Error);
		TestTrue(TEXT("When backward and both sides are blocked, the open forward exit is selected."),
			Driver.HeldKeys.Contains(EKeys::W));
		TestFalse(TEXT("Forward escape releases backward input."), Driver.HeldKeys.Contains(EKeys::S));
		TestFalse(TEXT("Forward escape releases left input."), Driver.HeldKeys.Contains(EKeys::A));
		TestFalse(TEXT("Forward escape releases right input."), Driver.HeldKeys.Contains(EKeys::D));
		WallBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Driver.TickTraversing(0.1f, Error);
		TestTrue(TEXT("An open forward escape remains selected when backward opens again."),
			Driver.HeldKeys.Contains(EKeys::W));
		WallBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		AActor* FrontWall = World->SpawnActor<AActor>();
		UBoxComponent* FrontBox = NewObject<UBoxComponent>(FrontWall);
		FrontWall->SetRootComponent(FrontBox);
		FrontBox->SetBoxExtent(FVector(10, 1000, 1000));
		FrontBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		FrontBox->SetCollisionResponseToAllChannels(ECR_Block);
		FrontBox->RegisterComponent();
		FrontWall->SetActorLocation(FVector(100, 0, 0));
		TestTrue(TEXT("The fourth wall actually closes the forward capsule exit."),
			World->SweepSingleByChannel(Hit, FVector::ZeroVector, FVector(200, 0, 0),
				Capsule->GetComponentQuat(), Capsule->GetCollisionObjectType(),
				FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()),
				FCollisionQueryParams(SCENE_QUERY_STAT(ProjectWorldEnclosedFixture), false, Character),
				FCollisionResponseParams(Capsule->GetCollisionResponseToChannels())));
		Driver.TickTraversing(0.1f, Error);
		TestFalse(TEXT("A fully trapped capsule does not keep forward input."), Driver.HeldKeys.Contains(EKeys::W));
		TestFalse(TEXT("A fully trapped capsule does not keep backward input."), Driver.HeldKeys.Contains(EKeys::S));
		TestFalse(TEXT("A fully trapped capsule does not keep left input."), Driver.HeldKeys.Contains(EKeys::A));
		TestFalse(TEXT("A fully trapped capsule does not keep right input."), Driver.HeldKeys.Contains(EKeys::D));
		Driver.OverhangRecoveryStartedSeconds = FPlatformTime::Seconds() - 11.0;
		AddExpectedError(TEXT("Real backward/upward input could not clear an overhang within the bounded retreat."),
			EAutomationExpectedErrorFlags::Contains, 1);
		TestTrue(TEXT("A trapped retreat rejects at its time bound."),
			Driver.TickTraversing(0.1f, Error) == EProjectWorldPlayableTourResult::Rejected);
		TestTrue(TEXT("Refusal releases every simulated key."), Driver.HeldKeys.IsEmpty());
		Driver.ReleaseInputs();
		TestTrue(TEXT("Terminal cleanup releases every simulated key."), Driver.HeldKeys.IsEmpty());
	}
	World->DestroyWorld(false);
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldPlayableTourOverhangRecoveryTest,
	"Project.World.PlayableTour.OverhangRecovery", "[Fast][Unit][World]")

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldPlayableTourCenterArrivalTest,
	"Project.World.PlayableTour.CenterArrival",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectWorldPlayableTourCenterArrivalTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Initialization = UWorld::InitializationValues().AllowAudioPlayback(false)
		.CreatePhysicsScene(true).RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
		ERHIFeatureLevel::Num, &Initialization);
	if (World == nullptr)
	{
		AddError(TEXT("Unable to create the center-arrival fixture."));
		return false;
	}
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	ACharacter* Character = World->SpawnActor<ACharacter>();
	if (Controller == nullptr || Character == nullptr)
	{
		AddError(TEXT("Unable to spawn the center-arrival fixture actors."));
		World->DestroyWorld(false);
		return false;
	}
	Controller->Possess(Character);
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	Movement->SetMovementMode(MOVE_Flying);
	Movement->BrakingDecelerationFlying = 6000.0f;
	{
		FProjectWorldPlayableTourDriver Driver;
		FString Error;
		TestTrue(TEXT("The production-input driver initializes."), Driver.Initialize(*Controller, *Character,
			{FVector::ZeroVector, FVector(100000, 0, 0), FVector::ZeroVector}, Error, true));
		Driver.Phase = FProjectWorldPlayableTourDriver::EPhase::Traversing;
		Driver.WaypointIndex = 2;
		Character->SetActorLocation(FVector(-10000, 0, 0));
		Movement->Velocity = FVector(12000, 0, 0);
		Driver.TickTraversing(0.016f, Error);
		TestFalse(TEXT("The final approach releases W within the native stopping distance."),
			Driver.HeldKeys.Contains(EKeys::W));
		Character->SetActorLocation(FVector(-400, 0, 0));
		Driver.TickTraversing(0.016f, Error);
		TestEqual(TEXT("Passing through the center at flight speed is not arrival."), Driver.Evidence.WaypointsReached, 0);
		TestFalse(TEXT("A moving pass does not authorize center-return streaming evidence."), Driver.HasReturnedToCenter());
		TestFalse(TEXT("The approach does not start descending before horizontal flight settles."),
			Driver.HeldKeys.Contains(EKeys::LeftControl));
		Movement->Velocity = FVector::ZeroVector;
		Driver.TickTraversing(0.016f, Error);
		TestEqual(TEXT("A stopped character within the precise center radius arrives."), Driver.Evidence.WaypointsReached, 1);
		TestTrue(TEXT("Settled arrival authorizes the center-return evidence."), Driver.HasReturnedToCenter());
		FProjectWorldPlayableTourDriver StandardDriver;
		TestTrue(TEXT("The standard traversal initializes."), StandardDriver.Initialize(*Controller, *Character,
			{FVector::ZeroVector, FVector(100000, 0, 0), FVector::ZeroVector}, Error));
		StandardDriver.Phase = FProjectWorldPlayableTourDriver::EPhase::Traversing;
		StandardDriver.WaypointIndex = 2;
		Movement->Velocity = FVector(12000, 0, 0);
		StandardDriver.TickTraversing(0.016f, Error);
		TestEqual(TEXT("Standard traversal preserves its arrival without precise settlement."),
			StandardDriver.Evidence.WaypointsReached, 1);
	}
	World->DestroyWorld(false);
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldPlayableTourCenterArrivalTest,
	"Project.World.PlayableTour.CenterArrival", "[Fast][Unit][World]")

#endif
