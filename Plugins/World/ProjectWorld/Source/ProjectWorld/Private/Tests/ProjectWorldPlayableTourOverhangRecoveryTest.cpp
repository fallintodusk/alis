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
		Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Character->SetActorLocation(FVector(0, 0, 1000));
		Driver.TickTraversing(0.1f, Error);
		TestFalse(TEXT("Clearance releases backward input."), Driver.HeldKeys.Contains(EKeys::S));
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Character->SetActorLocation(FVector::ZeroVector);
		Driver.TickTraversing(0.1f, Error);
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

#endif
