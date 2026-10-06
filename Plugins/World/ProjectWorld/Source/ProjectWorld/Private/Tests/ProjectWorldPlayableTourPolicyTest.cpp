// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Presentation/ProjectWorldPlayableTourDriver.h"
#include "Presentation/ProjectWorldProductPerformanceGate.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldPlayableTourPolicyTest,
	"Project.World.PlayableTour.CenterReturnPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectWorldPlayableTourPolicyTest::RunTest(const FString& Parameters)
{
	const FString OriginalCommandLine = FCommandLine::Get();
	FCommandLine::Set(TEXT("-ProjectWorldPlayableTourPreciseCenterReturn"));
	FProjectWorldProductPerformanceGate Gate;
	FString Error;
	TestFalse(TEXT("Precise center return cannot be configured without playable traversal."), Gate.ParseConfig(Error));
	TestEqual(TEXT("The refusal identifies the invalid mode combination."), Error,
		FString(TEXT("Precise center return requires playable traversal.")));
	FCommandLine::Set(*OriginalCommandLine);

	const UWorld::InitializationValues Initialization = UWorld::InitializationValues().AllowAudioPlayback(false)
		.CreatePhysicsScene(false).RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
		ERHIFeatureLevel::Num, &Initialization);
	if (World == nullptr)
	{
		AddError(TEXT("Unable to create the transient policy fixture."));
		return false;
	}
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	ACharacter* Character = World->SpawnActor<ACharacter>();
	if (Controller != nullptr && Character != nullptr)
	{
		Character->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		const TArray<FVector> Points = {FVector::ZeroVector, FVector(100000, 0, 0), FVector::ZeroVector};
		{
			FProjectWorldPlayableTourDriver Standard;
			TestTrue(TEXT("Ordinary traversal initializes."), Standard.Initialize(*Controller, *Character, Points, Error));
			TestEqual(TEXT("Ordinary traversal uses the native 25 m policy."),
				Standard.GetFinalCenterArrivalRadiusCentimeters(), 2500.0);
			FProjectWorldPlayableTourDriver Precise;
			TestTrue(TEXT("Precise traversal initializes."), Precise.Initialize(*Controller, *Character, Points, Error, true));
			TestEqual(TEXT("Precise traversal uses the native 5 m policy."),
				Precise.GetFinalCenterArrivalRadiusCentimeters(), 500.0);
		}
	}
	else
	{
		AddError(TEXT("Unable to create the controller and character policy fixture."));
	}
	World->DestroyWorld(false);
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldPlayableTourPolicyTest,
	"Project.World.PlayableTour.CenterReturnPolicy", "[Fast][Unit][World]")

#endif
